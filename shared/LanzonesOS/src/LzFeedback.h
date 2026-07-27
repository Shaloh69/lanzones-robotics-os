// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Buzzer (non-blocking pattern player), status LED (single RGB), battery/
// power monitor.
#pragma once
#include <Adafruit_INA219.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "LzConfig.h"
#include "LzPins.h"

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

// ---------------- Status LED (single addressable RGB, spec 1) ------------
// Replaces the old discrete green/red LEDs with one WS2812-style pixel and
// a small priority-ordered state machine, since only one color shows at a
// time: low-battery > fault > locked > base state > off. NeoPixel .show()
// bit-bangs ~30us per pixel with interrupts briefly disabled — only called
// when the resolved color actually changes (change-driven, like the OLED),
// never per loop iteration.
enum LzLedState : uint8_t {
  LZLED_READY,      // solid green — idle/healthy
  LZLED_ARMED,      // fast-blink green — countdown/running
  LZLED_POSTMATCH,  // slow-blink amber — match ended, awaiting Quick Rematch
  LZLED_FAULT,      // fast-blink red — refused to arm / hard error
  LZLED_OFF,
};

class LzLeds {
 public:
  void begin();
  void setState(LzLedState s) { state_ = s; }
  void setLocked(bool l) { locked_ = l; }        // overlay: solid blue
  void setLowBattery(bool low) { lowBatt_ = low; }  // overlay: pulsing red, top priority
  // Freeze all color transitions (spec, this pass): while frozen, tick()
  // never calls .show() — the WS2812 bit-bang briefly disables interrupts,
  // and that must never happen during the 200Hz control ISR's window
  // (RUN MODE, tied 1:1 to OS::setRunActive). One deterministic, non-
  // blinking write happens at the moment freezing STARTS so the LED
  // doesn't land on an arbitrary blink phase for the whole match; nothing
  // else is written until unfrozen. Outside RUN MODE, unrestricted.
  void setFrozen(bool f);
  void tick(uint32_t now);

 private:
  Adafruit_NeoPixel pixel_{1, LZ_PIN_LED_RGB, NEO_GRB + NEO_KHZ800};
  LzLedState state_ = LZLED_READY;
  bool locked_ = false;
  bool lowBatt_ = false;
  bool frozen_ = false;
  uint32_t lastColor_ = 0xFFFFFFFF;  // change-driven: only push() when it differs
};

// ---------------- Battery / power monitor (INA219, spec 1) ----------------
// Replaces the old voltage-divider ADC read — shares the I2C bus, no pin
// cost, and adds real current draw (used by Traction Control as a stronger
// slip signal than the accelerometer-mismatch proxy alone).
class LzBattery {
 public:
  void begin();
  void tick(uint32_t now);
  bool present() const { return present_; }
  float voltage() const { return volts_; }
  float currentMa() const { return currentMa_; }
  float powerMw() const { return powerMw_; }
  int percent() const;              // -1 until first reading
  void setWarnVoltage(float v) { warnV_ = v; }
  float warnVoltage() const { return warnV_; }
  bool isLow() const { return low_; }

 private:
  Adafruit_INA219 ina_{LZ_INA219_ADDR};
  bool present_ = false;
  float volts_ = -1.0f;
  float currentMa_ = 0, powerMw_ = 0;
  float warnV_ = 7.0f;
  bool low_ = false;
  uint32_t lastSample_ = 0;
};
