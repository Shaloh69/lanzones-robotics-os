// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Flash persistence for profiles/strategies/path arrays (spec 1.0 "Storage").
// One versioned, CRC-protected image in the last 128 KB flash sector
// (sector 7 @ 0x08060000 on STM32F401CE — far above the firmware).
//
// Wear/reliability rules (spec 6.1/6.2):
//  - save() is only called from explicit Save/confirm actions in CONFIGURE —
//    callers batch edits in RAM; nothing writes flash per keypress.
//  - save() REFUSES to run while RUN MODE is active (lzStoreRunGuard).
//  - Erase+program is blocking by nature; the 8 s watchdog outlasts the
//    worst-case sector erase and is fed immediately before/after.
#pragma once
#include <Arduino.h>

// Set true while RUN MODE is active; LzStore::save() fails fast when set.
extern volatile bool lzStoreRunGuard;

namespace LzStore {
// Returns true if a valid image with matching version/size was loaded.
bool load(void *payload, uint16_t size, uint16_t version);
// Erase sector + write image + verify. False on guard/HAL/verify failure.
bool save(const void *payload, uint16_t size, uint16_t version);
uint32_t crc32(const void *data, uint32_t len);
}  // namespace LzStore
