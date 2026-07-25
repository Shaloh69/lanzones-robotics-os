// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Shared OS-layer constants and tuning knobs.
#pragma once

#define LZ_FW_VERSION        "v1.0.0"
#define LZ_BRAND_LINE_1      "Owned by Team Lanzones"
#define LZ_BRAND_LINE_2      "Partnered by Koogs Robotics"
#define LZ_BRAND_SHORT       "LANZONES x KOOGS"  // fits 128px (spec 1 frame 2)

#define LZ_NAME_LEN          10    // named-slot length, incl. terminator
#define LZ_MAX_STACK         8     // max screen-stack depth

// UI timing
#define LZ_UI_REDRAW_MS      50    // change-driven redraw, capped at 20 Hz
#define LZ_DEBOUNCE_MS       20
#define LZ_FASTSCROLL_AFTER  800   // ms held before UP/DOWN fast-scroll kicks in
#define LZ_FASTSCROLL_EVERY  60    // ms between fast-scroll repeats
#define LZ_HOLD_MS           1000  // hold-to-confirm gesture length

// Battery: INA219 power monitor on the shared I2C bus (spec 1) — replaces
// the old voltage-divider ADC read. No pin cost; shunt resistor value is a
// PCB-level concern (sized for expected current draw), not a firmware one.
#define LZ_INA219_ADDR       0x40  // PLACEHOLDER — INA219 A1A0 = 00
#define LZ_VBAT_DEFAULT_MIN  6.6f  // 2S LiPo empty (assumption — adjust per pack)
#define LZ_VBAT_DEFAULT_MAX  8.4f  // 2S LiPo full

// External EEPROM (24LC256-class) on the shared I2C bus (spec 1) — mirrors
// the internal-flash profile store for extra non-volatile headroom / less
// flash wear from frequent saves. Internal flash remains authoritative;
// EEPROM is a best-effort secondary copy, never required for correct
// operation (an absent EEPROM just means no mirror, nothing else changes).
#define LZ_EEPROM_ADDR       0x50  // PLACEHOLDER — 24LC256 A2A1A0 = 000
#define LZ_EEPROM_PAGE_SIZE  64    // 24LC256 page size
#define LZ_EEPROM_SIZE       32768 // 24LC256 = 256 kbit = 32 KB

#define LZ_WATCHDOG_US       8000000  // 8 s IWDG — longer than worst-case flash erase
