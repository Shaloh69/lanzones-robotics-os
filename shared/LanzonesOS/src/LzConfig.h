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

// Battery. TODO (PCB): set the real divider ratio once the board is final.
#define LZ_VBAT_DIVIDER      4.03f // PLACEHOLDER — voltage divider ratio on PB0
#define LZ_VBAT_ADC_REF      3.3f
#define LZ_VBAT_DEFAULT_MIN  6.6f  // 2S LiPo empty (assumption — adjust per pack)
#define LZ_VBAT_DEFAULT_MAX  8.4f  // 2S LiPo full

#define LZ_WATCHDOG_US       8000000  // 8 s IWDG — longer than worst-case flash erase
