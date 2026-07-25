// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Drive motor abstraction (spec 2.3 / 3.3):
//  - ESC mode: single 50 Hz RC-PWM signal (1000-2000 us) per motor
//  - HBRIDGE mode: PWM (20 kHz) + direction pin per motor
// Optional quadrature encoder feedback for closed-loop speed control; falls
// back to open-loop percentage when encoders are absent (no counts seen).
#pragma once
#include <Arduino.h>

class LzMotor {
 public:
  enum Mode : uint8_t { ESC = 0, HBRIDGE = 1 };

  void begin(Mode m, uint8_t pwmPin, uint8_t dirPin);
  // pct: -100..100 (negative = reverse). Clamped internally.
  void setPercent(int16_t pct);
  void stop() { setPercent(0); }
  int16_t current() const { return pct_; }

 private:
  Mode mode_ = HBRIDGE;
  uint8_t pwmPin_ = 0, dirPin_ = 0;
  int16_t pct_ = 0;
  bool begun_ = false;
};

// ---- quadrature encoder (interrupt-driven; spec 6.2 race-condition rules:
// count is volatile and read under a brief critical section) ----
class LzEncoder {
 public:
  void begin(uint8_t pinA, uint8_t pinB);
  int32_t read();          // total count (atomic snapshot)
  int32_t takeDelta();     // counts since last call (atomic)
  bool present() const { return sawEdge_; }  // any pulses ever seen?

 private:
  static void isr0();
  static void isr1();
  void handle();
  static LzEncoder *inst_[2];
  static uint8_t nInst_;
  uint8_t pinA_ = 0, pinB_ = 0;
  volatile int32_t count_ = 0;
  volatile bool sawEdge_ = false;
  int32_t lastRead_ = 0;
};
