// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Virtual buttons over USB-CDC serial — for running the menus with no
// physical buttons wired (see platformio.ini's PIO_FRAMEWORK_ARDUINO_
// ENABLE_CDC note). Single-key commands feed the exact same debounce/
// hold state machine a real GPIO press would (LzButtons::virtualTap/
// virtualHold), so nothing downstream of input treats it differently.
// Harmless and always active — a wired physical button still works
// exactly as before; this is just a second, optional input source.
#pragma once
#include <Arduino.h>

namespace LzVirtualInput {
void begin();            // prints the key-mapping banner once
void tick(uint32_t now); // call every loop iteration, bounded serial read
}  // namespace LzVirtualInput
