// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Buzzer (non-blocking pattern player), status LEDs, battery monitor.
#pragma once
#include <Arduino.h>
#include "LzConfig.h"

// ---------------- Buzzer ----------------
enum LzSound : uint8_t {
  SND_CLICK, SND_CONFIRM, SND_ERROR, SND_MATCH_START, SND_LOWBATT, SND_BOOT
};

class LzBuzzer {
 public:
  void begin();
  void play(LzSound s);
  void tick(uint32_t now);
  void stop();

 private:
  const uint16_t *seq_ = nullptr;  // freq,ms pairs; freq 0 = rest
  uint8_t len_ = 0, idx_ = 0;
  uint32_t stepEnd_ = 0;
  bool active_ = false;
};

// ---------------- Status LEDs ----------------
enum LzLedMode : uint8_t { LZLED_OFF, LZLED_ON, LZLED_BLINK, LZLED_BLINK_FAST };

class LzLeds {
 public:
  void begin();
  void green(LzLedMode m) { g_ = m; }
  void red(LzLedMode m) { r_ = m; }
  void tick(uint32_t now);

 private:
  LzLedMode g_ = LZLED_OFF, r_ = LZLED_OFF;
  static bool phase(LzLedMode m, uint32_t now);
};

// ---------------- Battery ----------------
class LzBattery {
 public:
  void begin();
  void tick(uint32_t now);
  float voltage() const { return volts_; }
  int percent() const;              // -1 until first reading
  void setWarnVoltage(float v) { warnV_ = v; }
  float warnVoltage() const { return warnV_; }
  bool isLow() const { return low_; }

 private:
  float volts_ = -1.0f;
  float warnV_ = 7.0f;
  bool low_ = false;
  uint32_t lastSample_ = 0;
};
