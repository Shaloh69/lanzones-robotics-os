// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzVirtualInput.h"

#include "LzOS.h"

static void printHelp() {
  Serial.println();
  Serial.println(F("=== Virtual buttons (no physical buttons wired) ==="));
  Serial.println(F("  u/d/s/b/g = tap  UP/DOWN/SELECT/BACK/START"));
  Serial.println(F("  U/D/S/B/G = hold (~1.2s, for hold-to-confirm)"));
  Serial.println(F("  ?         = show this again"));
  Serial.println(F("====================================================="));
}

void LzVirtualInput::begin() { printHelp(); }

void LzVirtualInput::tick(uint32_t now) {
  (void)now;
  uint8_t budget = 8;  // bounded work per tick, never blocks the loop
  while (Serial.available() && budget--) {
    switch ((char)Serial.read()) {
      case 'u': Buttons.virtualTap(BTN_UP); break;
      case 'U': Buttons.virtualHold(BTN_UP); break;
      case 'd': Buttons.virtualTap(BTN_DOWN); break;
      case 'D': Buttons.virtualHold(BTN_DOWN); break;
      case 's': Buttons.virtualTap(BTN_SELECT); break;
      case 'S': Buttons.virtualHold(BTN_SELECT); break;
      case 'b': Buttons.virtualTap(BTN_BACK); break;
      case 'B': Buttons.virtualHold(BTN_BACK); break;
      case 'g': Buttons.virtualTap(BTN_START); break;
      case 'G': Buttons.virtualHold(BTN_START); break;
      case '?': printHelp(); break;
      default: break;  // ignore line endings / stray bytes quietly
    }
  }
}
