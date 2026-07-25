// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR screens: RUN MODE (bar-graph + speed + PID error, Follow or
// Speed-Run), SENSOR HEALTH (live bar-graph + PASS/FAIL), CALIBRATE
// (Auto-Cal Wizard), MOTOR TEST. Live screens redraw change-driven.
#include <LzOS.h>
#include <LzUi.h>

#include "vector_hw.h"
#include "vector_model.h"

// draw the 8-sensor bar graph into body rows r0..r1 area (20 px tall)
static void drawBars(U8G2 &g, int yTop, int height) {
  for (uint8_t i = 0; i < 8; i++) {
    int h = (int)((int32_t)irNorm[i] * height / 1000);
    int x = 4 + i * 15;
    g.drawFrame(x, yTop, 12, height);
    if (h > 0) g.drawBox(x, yTop + height - h, 12, h);
  }
}

// ---------------- RUN MODE ----------------
class VRunScreen : public LzScreen {
 public:
  VRunScreen() : LzScreen("RUN") {}
  void onEnter() override { snap_ = 0xFFFFFFFF; }
  void onLeave() override {
    if (vecMode() != VM_IDLE) vecStop("EXIT");
  }
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS && vecMode() == VM_IDLE) {
      speedRun_ = !speedRun_;  // toggle Follow / Speed Run (spec 3.2)
      return true;
    }
    if (ev.btn == BTN_START && ev.type == EV_PRESS) {
      if (vecMode() == VM_IDLE)
        vecStart(speedRun_ ? VM_SPEED : VM_FOLLOW);
      else
        vecStop("STOP BTN");  // aborts instantly (spec 3.1)
      return true;
    }
    if (ev.btn == BTN_BACK && ev.type == EV_PRESS && vecMode() != VM_IDLE) {
      vecStop("STOP BTN");
      return true;
    }
    return false;
  }
  void onTick(uint32_t now) override {
    (void)now;
    int pos = linePosition();
    uint32_t s = ((uint32_t)vecMode() << 28) ^ ((uint32_t)(pos / 100) << 16) ^
                 ((uint32_t)(int)(lineError() * 4) << 8) ^ vecSpeedIdx() ^
                 ((uint32_t)speedRun_ << 30);
    if (s != snap_) {
      snap_ = s;
      OS.requestRedraw();
    }
  }
  void draw(U8G2 &g) override {
    char b[26];
    if (vecMode() == VM_IDLE) {
      snprintf(b, sizeof(b), "Mode: %s", speedRun_ ? "SPEED RUN" : "FOLLOW");
      Display.bodyRow(0, b, false);
      snprintf(b, sizeof(b), "Path: %d junctions", G.path.len);
      Display.bodyRow(1, b, false);
      Display.bodyRow(2, "START: go", false);
      if (vecStopReason()[0]) {
        snprintf(b, sizeof(b), "Last: %s", vecStopReason());
        Display.bodyRow(3, b, false);
      }
      return;
    }
    drawBars(g, Display.rowTop(0), 20);  // live sensor visualization
    int spd = (motorL.current() + motorR.current()) / 2;
    snprintf(b, sizeof(b), "Spd:%3d%% Err:%+5.1f", spd, (double)lineError());
    Display.bodyRow(2, b, false);
    if (vecMode() == VM_SPEED)
      snprintf(b, sizeof(b), "SPEED %d/%d %s", vecSpeedIdx(), G.path.len,
               vecSpeedIdx() < G.path.len
                   ? STEP_NAMES[G.path.steps[vecSpeedIdx()]]
                   : "-");
    else
      snprintf(b, sizeof(b), "FOLLOW  J:%d", vecJunctionCount());
    Display.bodyRow(3, b, false);
  }
  const char *hint() override {
    return vecMode() == VM_IDLE ? "SEL:Mode ST:Go BK:Back" : "ST/BK:STOP";
  }

 private:
  uint32_t snap_ = 0;
  bool speedRun_ = false;
};
static VRunScreen runScreen;
void openRunMode() { OS.push(&runScreen); }

// ---------------- SENSOR HEALTH ----------------
class VSensorHealth : public LzScreen {
 public:
  VSensorHealth() : LzScreen("SENSORS") {}
  void onTick(uint32_t now) override {
    if (now - last_ >= 200) {
      last_ = now;
      OS.requestRedraw();
    }
  }
  void draw(U8G2 &g) override {
    drawBars(g, Display.rowTop(0), 26);
    char b[26];
    char flags[9];
    for (uint8_t i = 0; i < 8; i++) flags[i] = irStuck[i] ? 'X' : '.';
    flags[8] = 0;
    // a sensor stuck at rail or with no calibration span = FAIL (spec 3.1)
    snprintf(b, sizeof(b), "12345678 X=FAIL");
    Display.bodyRow(2, b, false);
    snprintf(b, sizeof(b), "%s", flags);
    Display.bodyRow(3, b, false);
  }
  const char *hint() override { return "BK:Back"; }

 private:
  uint32_t last_ = 0;
};
static VSensorHealth sensorHealth;
void openSensorHealth() { OS.push(&sensorHealth); }

// ---------------- CALIBRATE: Auto-Cal Wizard ----------------
class AutoCalScreen : public LzScreen {
 public:
  AutoCalScreen() : LzScreen("AUTOCAL") {}
  void onLeave() override {
    if (vecMode() == VM_AUTOCAL) vecStop("EXIT");
  }
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn == BTN_START && ev.type == EV_PRESS) {
      if (vecMode() == VM_IDLE && OS.editAllowed()) vecStart(VM_AUTOCAL);
      return true;
    }
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS &&
        vecMode() == VM_IDLE) {  // save captured calibration
      if (OS.editAllowed()) vectorSaveWithFeedback();
      return true;
    }
    return false;
  }
  void onTick(uint32_t now) override {
    if (now - last_ >= 200) {
      last_ = now;
      OS.requestRedraw();
    }
  }
  void draw(U8G2 &g) override {
    (void)g;
    char b[26];
    if (vecMode() == VM_AUTOCAL) {
      Display.bodyRow(0, "Sweeping ~3s...", false);
      Display.bodyRow(1, "Keep array over", false);
      Display.bodyRow(2, "the line!", false);
      return;
    }
    Display.bodyRow(0, "START: 3s sweep,", false);
    Display.bodyRow(1, "auto-computes", false);
    snprintf(b, sizeof(b), "s1 mn%4d mx%4d", G.cal.mn[0], G.cal.mx[0]);
    Display.bodyRow(2, b, false);
    Display.bodyRow(3, "SEL: save to flash", false);
  }
  const char *hint() override { return "ST:Sweep SEL:Save BK:Back"; }

 private:
  uint32_t last_ = 0;
};
static AutoCalScreen autoCal;
void openCalibrate() { OS.push(&autoCal); }

// ---------------- MOTOR TEST ----------------
class VMotorTest : public LzScreen {
 public:
  VMotorTest() : LzScreen("MTEST") {}
  void onLeave() override { vectorMotorsStop(); }
  bool onEvent(const LzEvent &ev) override {
    if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) && ev.type == EV_PRESS) {
      sel_ = (uint8_t)((sel_ + (ev.btn == BTN_DOWN ? 1 : 2)) % 3);
      return true;
    }
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
      if (sel_ == 0) motor_ ^= 1;
      else if (sel_ == 1) rev_ ^= 1;
      else speed_ = (uint8_t)(speed_ % 4 + 1);
      return true;
    }
    if (ev.btn == BTN_START) {  // press = jog, release = stop
      if (ev.type == EV_PRESS) {
        int16_t p = (int16_t)(speed_ * 25) * (rev_ ? -1 : 1);
        vectorMotorsTestPulse(motor_ == 0 ? p : 0, motor_ == 1 ? p : 0,
                              30000);
      } else if (ev.type == EV_RELEASE) {
        vectorMotorsStop();
      }
      return true;
    }
    return false;
  }
  void draw(U8G2 &g) override {
    (void)g;
    char b[26];
    snprintf(b, sizeof(b), "Motor: %s", motor_ == 0 ? "LEFT" : "RIGHT");
    Display.bodyRow(0, b, sel_ == 0);
    snprintf(b, sizeof(b), "Dir:   %s", rev_ ? "REVERSE" : "FORWARD");
    Display.bodyRow(1, b, sel_ == 1);
    snprintf(b, sizeof(b), "Speed: %d%%", speed_ * 25);
    Display.bodyRow(2, b, sel_ == 2);
    Display.bodyRow(3, "Hold START to jog", false);
  }
  const char *hint() override { return "SEL:Cycle ST:Jog BK:Back"; }

 private:
  uint8_t sel_ = 0, motor_ = 0, speed_ = 1;
  bool rev_ = false;
};
static VMotorTest motorTest;
void openMotorTest() { OS.push(&motorTest); }
