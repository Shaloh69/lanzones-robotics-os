// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// BATTERY STATUS screen (both OSes): live voltage, rough %, and an
// adjustable low-battery warning threshold (persisted by the app's Save).
#pragma once
#include "LzUi.h"

class LzBatteryScreen : public LzScreen {
 public:
  // warn: points at the app's persisted settings field.
  LzBatteryScreen(float *warn, void (*onWarnChanged)())
      : LzScreen("BATTERY"), warn_(warn), onChanged_(onWarnChanged) {}

  void onEnter() override { last_ = -1000; }

  bool onEvent(const LzEvent &ev) override {
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
      NumEditor.openF("Low-batt warn (V)", warn_, 5.0f, 12.0f, 0.1f, 1, "V");
      return true;
    }
    return false;
  }

  void onTick(uint32_t now) override {
    (void)now;
    // change-driven redraw: only when the displayed value moves
    int v10 = (int)(Battery.voltage() * 10.0f);
    if (v10 != last_) {
      last_ = v10;
      OS.requestRedraw();
    }
    if (onChanged_ && *warn_ != shown_) {
      shown_ = *warn_;
      Battery.setWarnVoltage(shown_);
      onChanged_();
    }
  }

  void draw(U8G2 &g) override {
    char b[26];
    snprintf(b, sizeof(b), "%.2f V", (double)Battery.voltage());
    g.setFont(u8g2_font_9x15B_tr);
    g.drawStr((128 - g.getStrWidth(b)) / 2, Display.rowTop(1) + 6, b);
    snprintf(b, sizeof(b), "Charge: ~%d%%", Battery.percent());
    Display.bodyRow(2, b, false);
    snprintf(b, sizeof(b), "Warn below: %.1f V", (double)*warn_);
    Display.bodyRow(3, b, false);
  }

  const char *hint() override { return "SEL:Edit warn BK:Back"; }

 private:
  float *warn_;
  void (*onChanged_)();
  int last_ = -1000;
  float shown_ = -1;
};
