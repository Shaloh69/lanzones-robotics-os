// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzMotor.h"

void LzMotor::begin(Mode m, uint8_t pwmPin, uint8_t dirPin) {
  mode_ = m;
  pwmPin_ = pwmPin;
  dirPin_ = dirPin;
  pinMode(pwmPin_, OUTPUT);
  if (mode_ == HBRIDGE) {
    pinMode(dirPin_, OUTPUT);
    digitalWrite(dirPin_, LOW);
    analogWriteFrequency(20000);  // above audible for H-bridge
  } else {
    analogWriteFrequency(50);     // RC ESC frame rate
  }
  begun_ = true;
  setPercent(0);
}

void LzMotor::setPercent(int16_t pct) {
  if (!begun_) return;
  if (pct > 100) pct = 100;
  if (pct < -100) pct = -100;
  pct_ = pct;
  if (mode_ == HBRIDGE) {
    digitalWrite(dirPin_, pct >= 0 ? HIGH : LOW);
    int duty = (int)((pct >= 0 ? pct : -pct) * 255L / 100L);
    analogWrite(pwmPin_, duty);
  } else {
    // ESC: 1500 us neutral, 1000/2000 us full reverse/forward.
    // At 50 Hz, period = 20000 us; 8-bit duty maps linearly.
    int us = 1500 + (int)pct * 5;
    int duty = (int)((int32_t)us * 255L / 20000L);
    analogWrite(pwmPin_, duty);
  }
}

// ---------------- encoder ----------------
LzEncoder *LzEncoder::inst_[2] = {nullptr, nullptr};
uint8_t LzEncoder::nInst_ = 0;

void LzEncoder::isr0() { if (inst_[0]) inst_[0]->handle(); }
void LzEncoder::isr1() { if (inst_[1]) inst_[1]->handle(); }

void LzEncoder::handle() {
  // simple x2 decode: direction from B at A's edge
  bool a = digitalRead(pinA_), b = digitalRead(pinB_);
  count_ += (a == b) ? 1 : -1;
  sawEdge_ = true;
}

void LzEncoder::begin(uint8_t pinA, uint8_t pinB) {
  pinA_ = pinA;
  pinB_ = pinB;
  pinMode(pinA_, INPUT_PULLUP);
  pinMode(pinB_, INPUT_PULLUP);
  if (nInst_ < 2) {
    inst_[nInst_] = this;
    attachInterrupt(digitalPinToInterrupt(pinA_),
                    nInst_ == 0 ? isr0 : isr1, CHANGE);
    nInst_++;
  }
}

int32_t LzEncoder::read() {
  noInterrupts();  // brief critical section (spec 6.2)
  int32_t c = count_;
  interrupts();
  return c;
}

int32_t LzEncoder::takeDelta() {
  int32_t c = read();
  int32_t d = c - lastRead_;
  lastRead_ = c;
  return d;
}
