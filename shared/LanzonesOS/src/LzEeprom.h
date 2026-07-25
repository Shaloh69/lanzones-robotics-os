// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Minimal 24LC256-class I2C EEPROM driver (spec 1: extra non-volatile
// storage for the profile store, shares the existing I2C bus, no pin cost).
// Used only by LzStore as a best-effort secondary mirror — internal flash
// remains authoritative; an absent/failed EEPROM never blocks a save.
//
// Page-aware writes (64-byte pages) with an ACK-poll for the ~5ms internal
// write cycle. Like the flash erase this blocks briefly — acceptable only
// because callers (LzStore::save) are explicit CONFIGURE actions, never
// reachable from RUN MODE, with the watchdog fed around the whole call.
#pragma once
#include <Arduino.h>

namespace LzEeprom {
bool probe(uint8_t addr);                                   // boot-time check
bool write(uint8_t addr, uint16_t memAddr, const uint8_t *data, uint16_t len);
bool read(uint8_t addr, uint16_t memAddr, uint8_t *data, uint16_t len);
}  // namespace LzEeprom
