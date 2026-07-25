// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Screen base class. Every UI state is a screen on the OS screen stack; the
// stack itself produces the breadcrumb ("TALON > CONFIG > STRATEGY").
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "LzButtons.h"

class LzScreen {
 public:
  explicit LzScreen(const char *n) : name(n) {}
  virtual ~LzScreen() {}
  const char *name;

  virtual void onEnter() {}
  virtual void onLeave() {}
  // Return true if the event was consumed. Unconsumed BACK presses pop the
  // stack (handled centrally by LzOS).
  virtual bool onEvent(const LzEvent &ev) { (void)ev; return false; }
  virtual void onTick(uint32_t now) { (void)now; }
  virtual void draw(U8G2 &g) = 0;
  // Context-sensitive hint bar (spec 1.1) — every screen labels its buttons.
  virtual const char *hint() { return "UD:Nav SEL:Open BK:Back"; }
};
