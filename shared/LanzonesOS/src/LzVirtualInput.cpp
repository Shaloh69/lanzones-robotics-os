// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzVirtualInput.h"

#include "LzOS.h"

// WASD-style, not first-letter mnemonics: W/S give UP/DOWN real vertical
// keyboard correspondence (matches near-universal gaming muscle memory),
// which matters far more than mnemonics since UP/DOWN dominate menu
// navigation. A/D/X cover SELECT/BACK/START, keeping the left hand on
// WASD; X (common confirm/action key) lands on START, the least-pressed
// of the three.
static void printHelp() {
  Serial.println();
  Serial.println(F("=== Virtual buttons (no physical buttons wired) ==="));
  Serial.println(F("  w/s/a/d/x = tap  UP/DOWN/SELECT/BACK/START"));
  Serial.println(F("  W/S/A/D/X = hold (~1.2s, for hold-to-confirm)"));
  Serial.println(F("  ?         = show this again"));
  Serial.println(F("====================================================="));
}

void LzVirtualInput::begin() { printHelp(); }

void LzVirtualInput::tick(uint32_t now) {
  (void)now;
  uint8_t budget = 8;  // bounded work per tick, never blocks the loop
  while (Serial.available() && budget--) {
    switch ((char)Serial.read()) {
      case 'w': Buttons.virtualTap(BTN_UP); break;
      case 'W': Buttons.virtualHold(BTN_UP); break;
      case 's': Buttons.virtualTap(BTN_DOWN); break;
      case 'S': Buttons.virtualHold(BTN_DOWN); break;
      case 'a': Buttons.virtualTap(BTN_SELECT); break;
      case 'A': Buttons.virtualHold(BTN_SELECT); break;
      case 'd': Buttons.virtualTap(BTN_BACK); break;
      case 'D': Buttons.virtualHold(BTN_BACK); break;
      case 'x': Buttons.virtualTap(BTN_START); break;
      case 'X': Buttons.virtualHold(BTN_START); break;
      case '?': printHelp(); break;
      default: break;  // ignore line endings / stray bytes quietly
    }
  }
}
