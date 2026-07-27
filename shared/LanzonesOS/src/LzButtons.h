// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// 5-button non-blocking input: timed debounce (no delay()), auto-repeat
// fast-scroll for held UP/DOWN, 1 s HOLD gesture for hold-to-confirm.
#pragma once
#include <Arduino.h>
#include "LzConfig.h"

enum LzBtn : uint8_t { BTN_UP, BTN_DOWN, BTN_SELECT, BTN_BACK, BTN_START, BTN_COUNT };
enum LzEvType : uint8_t { EV_NONE, EV_PRESS, EV_REPEAT, EV_HOLD, EV_RELEASE };

struct LzEvent {
  LzBtn btn;
  LzEvType type;
};

class LzButtons {
 public:
  void begin();
  void poll(uint32_t now);          // call every loop iteration
  bool next(LzEvent &ev);           // pop next queued event
  bool isDown(LzBtn b) const { return stable_[b]; }
  uint32_t heldFor(LzBtn b, uint32_t now) const {
    return stable_[b] ? now - pressedAt_[b] : 0;
  }

  // Virtual input (no physical buttons wired yet, spec: USB-CDC serial
  // control): holds a button "down" for a fixed synthetic duration, fed
  // into the SAME debounce/hold/repeat state machine as a real GPIO press
  // — not a separate code path. A tap is long enough to clear debounce and
  // register a normal press; a hold is long enough to also fire EV_HOLD
  // (hold-to-confirm, name-editor finish), matching a real ~1s hold.
  void virtualTap(LzBtn b);
  void virtualHold(LzBtn b);

 private:
  void push(LzBtn b, LzEvType t);
  bool raw_[BTN_COUNT] = {};
  bool stable_[BTN_COUNT] = {};
  uint32_t lastEdge_[BTN_COUNT] = {};
  uint32_t pressedAt_[BTN_COUNT] = {};
  uint32_t lastRepeat_[BTN_COUNT] = {};
  bool holdFired_[BTN_COUNT] = {};
  uint32_t virtualUntil_[BTN_COUNT] = {};  // 0 = no active virtual press
  static const uint8_t QLEN = 8;
  LzEvent q_[QLEN];
  uint8_t qHead_ = 0, qTail_ = 0;
};
