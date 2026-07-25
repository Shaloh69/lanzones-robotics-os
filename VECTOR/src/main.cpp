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
#include "vector_hw.h"
#include "vector_model.h"

// screens implemented across vector_*.cpp
void openConfigure();
void openRunMode();
void openSensorHealth();
void openCalibrate();
void openMotorTest();
void openHelpScreen();

// ---------------- Battery ----------------
static void warnChanged() {}
static LzBatteryScreen batteryScreen(&G.cur.warnVoltage, warnChanged);
static void openBattery() { OS.push(&batteryScreen); }

// ---------------- Lock Config (spec 1: persists across power cycles) ----
static void doUnlock() {
  OS.setLocked(false);
  G.lockedFlag = 0;
  vectorSaveWithFeedback();  // explicit action; persists the unlocked state
}
static void toggleLock() {
  if (!OS.locked()) {
    OS.setLocked(true);
    G.lockedFlag = 1;
    Buzzer.play(SND_CONFIRM);
    vectorSaveWithFeedback();  // survives a bench power-cycle (spec 1)
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
    {"RUN MODE", openRunMode, nullptr},
    {"CONFIGURE", openConfigure, nullptr},
    {"SENSOR HEALTH", openSensorHealth, nullptr},
    {"CALIBRATE", openCalibrate, nullptr},
    {"MOTOR TEST", openMotorTest, nullptr},
    {"BATTERY STATUS", openBattery, nullptr},
    {"HELP", openHelpScreen, nullptr},
    {"LOCK CONFIG", toggleLock, lockValue},
};
static LzMenuScreen mainMenu("MAIN", MAIN_ITEMS,
                             sizeof(MAIN_ITEMS) / sizeof(MAIN_ITEMS[0]));

void setup() {
  vectorLoad();  // flash image or defaults (before anything reads G)
  OS.begin("VECTOR", VECTOR_BUILD_ID);
  OS.showSplash();
  vectorHwBegin();
  OS.push(&mainMenu);
  Leds.green(LZLED_ON);  // ready/healthy at a glance
  Battery.setWarnVoltage(G.cur.warnVoltage);
  OS.setLocked(G.lockedFlag != 0);  // Lock Config persists (spec 1)
}

void loop() {  // non-blocking state machine — no delay() anywhere
  OS.tick();
  vectorHwTick(millis());
}
