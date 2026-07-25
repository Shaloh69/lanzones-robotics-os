// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR — Line Follower OS entry point.
// Shared OS-layer pins (spec 1.2): see shared/LanzonesOS/src/LzPins.h
// Project pins (PLACEHOLDERS until PCB final): see include/pin_config.h
#include <Arduino.h>

#include <LzBatteryScreen.h>
#include <LzOS.h>
#include <LzUi.h>

#include "build_id.h"
#include "pin_config.h"

// ---- placeholder settings (moved into the profile store in Step 3) ----
static float gWarnVoltage = 7.0f;
static void warnChanged() {}

// ---------------- placeholder for Step 3 features ----------------
static void openTodo() { Message.show("Coming in Step 3", "Feature stub"); }

// ---------------- Battery ----------------
static LzBatteryScreen batteryScreen(&gWarnVoltage, warnChanged);
static void openBattery() { OS.push(&batteryScreen); }

// ---------------- Help ----------------
static const LzHelpEntry HELP_ENTRIES[] = {
    {"About / Branding",
     "VECTOR Line\nFollower OS\n" LZ_FW_VERSION "\n"
     "Owned by\n Team Lanzones\nPartnered by\n Koogs Robotics\n"
     "Build ID:\n " VECTOR_BUILD_ID},
    {"Navigation Basics",
     "UP/DOWN moves\nbetween items.\nSELECT opens or\nconfirms. BACK\n"
     "exits without\nsaving. Hold\nSELECT or BACK\nfor 1 second to\n"
     "confirm a delete."},
};
static LzHelpIndexScreen helpScreen(HELP_ENTRIES,
                                    sizeof(HELP_ENTRIES) / sizeof(HELP_ENTRIES[0]));
static void openHelp() { OS.push(&helpScreen); }

// ---------------- Lock Config (spec 1.0) ----------------
static void doUnlock() { OS.setLocked(false); }
static void toggleLock() {
  if (!OS.locked()) {
    OS.setLocked(true);
    Buzzer.play(SND_CONFIRM);
  } else {
    Confirm.open("Unlock config", doUnlock);  // explicit hold-gesture unlock
  }
  OS.requestRedraw();
}
static void lockValue(char *out, size_t n) {
  snprintf(out, n, "%s", OS.locked() ? "ON" : "OFF");
}

// ---------------- Main menu (spec 3.1) ----------------
static const LzMenuItem MAIN_ITEMS[] = {
    {"RUN MODE", openTodo, nullptr},
    {"CONFIGURE", openTodo, nullptr},
    {"SENSOR HEALTH", openTodo, nullptr},
    {"CALIBRATE", openTodo, nullptr},
    {"MOTOR TEST", openTodo, nullptr},
    {"BATTERY STATUS", openBattery, nullptr},
    {"HELP", openHelp, nullptr},
    {"LOCK CONFIG", toggleLock, lockValue},
};
static LzMenuScreen mainMenu("MAIN", MAIN_ITEMS,
                             sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]));

void setup() {
  OS.begin("VECTOR", VECTOR_BUILD_ID);
  OS.showSplash();
  OS.push(&mainMenu);
  Leds.green(LZLED_ON);  // ready/healthy at a glance
  Battery.setWarnVoltage(gWarnVoltage);
}

void loop() { OS.tick(); }  // non-blocking state machine — no delay() anywhere
