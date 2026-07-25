// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzEeprom.h"

#include <Wire.h>

#include "LzConfig.h"

bool LzEeprom::probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool ackPollWrite(uint8_t addr) {
  // 24LC256 NAKs the bus during its internal write cycle (~5ms typical);
  // poll with a bounded timeout instead of a fixed delay.
  uint32_t t0 = millis();
  while (millis() - t0 < 20) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) return true;
  }
  return false;
}

bool LzEeprom::write(uint8_t addr, uint16_t memAddr, const uint8_t *data,
                     uint16_t len) {
  uint16_t off = 0;
  while (off < len) {
    uint16_t pageOff = (memAddr + off) % LZ_EEPROM_PAGE_SIZE;
    uint16_t chunk = LZ_EEPROM_PAGE_SIZE - pageOff;
    if (chunk > len - off) chunk = len - off;
    if (chunk > 30) chunk = 30;  // Wire TX buffer headroom (2 addr + data)

    Wire.beginTransmission(addr);
    Wire.write((uint8_t)((memAddr + off) >> 8));
    Wire.write((uint8_t)((memAddr + off) & 0xFF));
    for (uint16_t i = 0; i < chunk; i++) Wire.write(data[off + i]);
    if (Wire.endTransmission() != 0) return false;
    if (!ackPollWrite(addr)) return false;
    off += chunk;
  }
  return true;
}

bool LzEeprom::read(uint8_t addr, uint16_t memAddr, uint8_t *data,
                    uint16_t len) {
  uint16_t off = 0;
  while (off < len) {
    uint16_t chunk = (len - off > 32) ? 32 : (len - off);
    Wire.beginTransmission(addr);
    Wire.write((uint8_t)((memAddr + off) >> 8));
    Wire.write((uint8_t)((memAddr + off) & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;  // repeated start
    if (Wire.requestFrom((int)addr, (int)chunk) != chunk) return false;
    for (uint16_t i = 0; i < chunk; i++) data[off + i] = (uint8_t)Wire.read();
    off += chunk;
  }
  return true;
}
