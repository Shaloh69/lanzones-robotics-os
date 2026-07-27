// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// OS core: screen stack + breadcrumb, event dispatch, watchdog feeding,
// Lock Config enforcement, low-battery alarm, splash screen.
#pragma once
#include <Arduino.h>

#include "LzButtons.h"
#include "LzConfig.h"
#include "LzDisplay.h"
#include "LzFeedback.h"
#include "LzScreen.h"

class LzOS {
 public:
  void begin(const char *osName, const char *buildId);
  // Two-frame boot splash (spec 1): 64x64 logo fades in (contrast ramp),
  // holds ~3s, fades to OS name + version + "LANZONES x KOOGS".
  void showSplash(const uint8_t *logo64);

  void push(LzScreen *s);
  void pop();
  void popToRoot();
  LzScreen *top() { return depth_ ? stack_[depth_ - 1] : nullptr; }
  uint8_t depth() const { return depth_; }

  void tick();  // call from loop() every iteration

  // Lock Config (spec 1.0): read-only CONFIGURE while locked.
  bool editAllowed();  // false (with error tone + message) when locked
  void setLocked(bool l);  // also drives the status LED's blue overlay
  bool locked() const { return locked_; }

  // RUN MODE guard — also blocks flash writes via lzStoreRunGuard.
  void setRunActive(bool a);
  bool runActive() const;

  void requestRedraw();

  const char *osName = "OS";
  const char *buildId = "";

 private:
  void rebuildCrumb();
  LzScreen *stack_[LZ_MAX_STACK] = {};
  uint8_t depth_ = 0;
  char crumb_[64] = "";
  bool locked_ = false;
  uint32_t lastLowBeep_ = 0;
};

extern LzOS OS;
extern LzButtons Buttons;
extern LzDisplay Display;
extern LzBuzzer Buzzer;
extern LzLeds Leds;
extern LzBattery Battery;
