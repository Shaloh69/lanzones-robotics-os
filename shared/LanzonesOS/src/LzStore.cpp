// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzStore.h"

#include <IWatchdog.h>
#include "stm32f4xx_hal.h"

#include "LzConfig.h"
#include "LzEeprom.h"

volatile bool lzStoreRunGuard = false;

static const uint32_t STORE_ADDR = 0x08060000UL;  // F411CE sector 7 (128 KB)
static const uint32_t STORE_SECTOR = FLASH_SECTOR_7;

struct StoreHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
};
static const uint32_t STORE_MAGIC = 0x4C4E5A53UL;  // "SZNL"

// External EEPROM mirror (spec 1): best-effort secondary copy on the shared
// I2C bus. Internal flash above is authoritative; an absent/failed EEPROM
// never fails a save or blocks a load — it's pure extra resilience. Layout
// is independent of the internal-flash one (header, then payload, then a
// CRC over the payload alone — no need for a contiguous header+payload
// buffer like the in-flash format uses).
static bool eepromChecked = false, eepromPresent = false;
static void ensureEepromChecked() {
  if (eepromChecked) return;
  eepromChecked = true;
  eepromPresent = LzEeprom::probe(LZ_EEPROM_ADDR);
}

uint32_t LzStore::crc32(const void *data, uint32_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFUL;
  while (len--) {
    crc ^= *p++;
    for (uint8_t k = 0; k < 8; k++)
      crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}

static bool loadFromEeprom(void *payload, uint16_t size, uint16_t version) {
  ensureEepromChecked();
  if (!eepromPresent) return false;
  StoreHeader eh;
  if (!LzEeprom::read(LZ_EEPROM_ADDR, 0, (uint8_t *)&eh, sizeof(eh))) return false;
  if (eh.magic != STORE_MAGIC || eh.version != version || eh.size != size)
    return false;
  if (!LzEeprom::read(LZ_EEPROM_ADDR, sizeof(eh), (uint8_t *)payload, size))
    return false;
  uint32_t storedCrc = 0;
  if (!LzEeprom::read(LZ_EEPROM_ADDR, sizeof(eh) + size, (uint8_t *)&storedCrc, 4))
    return false;
  return storedCrc == LzStore::crc32(payload, size);
}

bool LzStore::load(void *payload, uint16_t size, uint16_t version) {
  const StoreHeader *h = (const StoreHeader *)STORE_ADDR;
  bool flashValid = h->magic == STORE_MAGIC && h->version == version &&
                    h->size == size;
  if (flashValid) {
    const uint8_t *body = (const uint8_t *)(STORE_ADDR + sizeof(StoreHeader));
    uint32_t storedCrc = *(const uint32_t *)(body + size);
    uint32_t calc = crc32((const void *)STORE_ADDR, sizeof(StoreHeader) + size);
    flashValid = (storedCrc == calc);
  }
  if (flashValid) {
    const uint8_t *body = (const uint8_t *)(STORE_ADDR + sizeof(StoreHeader));
    memcpy(payload, body, size);
    return true;
  }
  // Internal flash blank/corrupt (e.g. a fresh chip, or a version bump) —
  // fall back to the EEPROM mirror, if one is present.
  return loadFromEeprom(payload, size, version);
}

static bool programWords(uint32_t addr, const uint8_t *data, uint32_t len) {
  for (uint32_t off = 0; off < len; off += 4) {
    uint32_t word = 0xFFFFFFFFUL;
    uint32_t n = (len - off) < 4 ? (len - off) : 4;
    memcpy(&word, data + off, n);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + off, word) != HAL_OK)
      return false;
  }
  return true;
}

bool LzStore::save(const void *payload, uint16_t size, uint16_t version) {
  if (lzStoreRunGuard) return false;  // spec 6.1: never write flash in RUN MODE

  StoreHeader h{STORE_MAGIC, version, size};

  IWatchdog.reload();  // sector erase can take seconds — feed first
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR |
                         FLASH_FLAG_PGSERR);
  FLASH_EraseInitTypeDef er = {};
  er.TypeErase = FLASH_TYPEERASE_SECTORS;
  er.Sector = STORE_SECTOR;
  er.NbSectors = 1;
  er.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  uint32_t badSector = 0;
  bool ok = (HAL_FLASHEx_Erase(&er, &badSector) == HAL_OK);
  IWatchdog.reload();

  if (ok) ok = programWords(STORE_ADDR, (const uint8_t *)&h, sizeof(h));
  if (ok)
    ok = programWords(STORE_ADDR + sizeof(h), (const uint8_t *)payload, size);
  if (ok) {
    uint32_t crc = crc32((const void *)STORE_ADDR, sizeof(h) + size);
    ok = programWords(STORE_ADDR + sizeof(h) + size, (const uint8_t *)&crc, 4);
  }
  HAL_FLASH_Lock();
  IWatchdog.reload();

  // verify readback
  if (ok)
    ok = (memcmp((const void *)(STORE_ADDR + sizeof(h)), payload, size) == 0);

  // Best-effort EEPROM mirror (spec 1) — only after internal flash (the
  // authoritative copy) has already succeeded and verified. EEPROM absence
  // or a mid-write failure here does NOT change the return value: the save
  // already succeeded where it matters.
  if (ok) {
    ensureEepromChecked();
    if (eepromPresent) {
      StoreHeader eh{STORE_MAGIC, version, size};
      uint32_t pcrc = crc32(payload, size);
      bool eepOk = LzEeprom::write(LZ_EEPROM_ADDR, 0, (const uint8_t *)&eh,
                                   sizeof(eh));
      if (eepOk)
        eepOk = LzEeprom::write(LZ_EEPROM_ADDR, sizeof(eh),
                                (const uint8_t *)payload, size);
      if (eepOk)
        eepOk = LzEeprom::write(LZ_EEPROM_ADDR, sizeof(eh) + size,
                                (const uint8_t *)&pcrc, 4);
      (void)eepOk;  // mirror only; internal flash is what "save succeeded" means
      IWatchdog.reload();
    }
  }

  return ok;
}
