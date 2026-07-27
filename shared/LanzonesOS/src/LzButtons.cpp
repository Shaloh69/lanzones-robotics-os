// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzButtons.h"
#include "LzPins.h"

static const uint8_t PIN_OF[BTN_COUNT] = {
    LZ_PIN_BTN_UP, LZ_PIN_BTN_DOWN, LZ_PIN_BTN_SELECT,
    LZ_PIN_BTN_BACK, LZ_PIN_BTN_START};

void LzButtons::begin() {
  for (uint8_t i = 0; i < BTN_COUNT; i++) pinMode(PIN_OF[i], INPUT_PULLUP);
}

void LzButtons::virtualTap(LzBtn b) {
  if (b >= BTN_COUNT) return;  // bounds check
  virtualUntil_[b] = millis() + 80;  // clears debounce, registers one press
}

void LzButtons::virtualHold(LzBtn b) {
  if (b >= BTN_COUNT) return;
  virtualUntil_[b] = millis() + (LZ_HOLD_MS + 200);  // outlasts the hold gesture
}

void LzButtons::push(LzBtn b, LzEvType t) {
  uint8_t nextTail = (uint8_t)((qTail_ + 1) % QLEN);
  if (nextTail == qHead_) return;  // queue full: drop (bounds-safe)
  q_[qTail_] = {b, t};
  qTail_ = nextTail;
}

bool LzButtons::next(LzEvent &ev) {
  if (qHead_ == qTail_) return false;
  ev = q_[qHead_];
  qHead_ = (uint8_t)((qHead_ + 1) % QLEN);
  return true;
}

void LzButtons::poll(uint32_t now) {
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    bool virtualHeld = virtualUntil_[i] != 0 && (int32_t)(now - virtualUntil_[i]) < 0;
    if (virtualUntil_[i] != 0 && !virtualHeld) virtualUntil_[i] = 0;  // expired
    bool r = (digitalRead(PIN_OF[i]) == LOW) || virtualHeld;  // active low, pull-up
    if (r != raw_[i]) {
      raw_[i] = r;
      lastEdge_[i] = now;
    }
    // timed debounce — accept the raw state once stable for LZ_DEBOUNCE_MS
    if (raw_[i] != stable_[i] && (now - lastEdge_[i]) >= LZ_DEBOUNCE_MS) {
      stable_[i] = raw_[i];
      if (stable_[i]) {
        pressedAt_[i] = now;
        lastRepeat_[i] = now;
        holdFired_[i] = false;
        push((LzBtn)i, EV_PRESS);
      } else {
        push((LzBtn)i, EV_RELEASE);
      }
    }
    if (stable_[i]) {
      uint32_t held = now - pressedAt_[i];
      // UP/DOWN fast-scroll on numeric fields after ~1 s held (spec 1.0)
      if ((i == BTN_UP || i == BTN_DOWN) && held >= LZ_FASTSCROLL_AFTER &&
          (now - lastRepeat_[i]) >= LZ_FASTSCROLL_EVERY) {
        lastRepeat_[i] = now;
        push((LzBtn)i, EV_REPEAT);
      }
      // 1 s hold gesture (hold-to-confirm)
      if (!holdFired_[i] && held >= LZ_HOLD_MS) {
        holdFired_[i] = true;
        push((LzBtn)i, EV_HOLD);
      }
    }
  }
}
