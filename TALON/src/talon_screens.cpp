// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON screens: RUN MODE (countdown + live status), SENSOR HEALTH,
// CALIBRATE (edge wizard + ToF zero), MOTOR TEST, ORIENTATION (IMU).
// All live screens redraw change-driven: values are quantized and compared,
// and the display is only marked dirty when something actually changed.
#include <LzOS.h>
#include <LzUi.h>

#include "talon_hw.h"
#include "talon_model.h"

// ---------------- RUN MODE ----------------
class RunScreen : public LzScreen {
 public:
  RunScreen() : LzScreen("RUN") {}
  void onEnter() override { snap_ = 0xFFFFFFFF; }
  void onLeave() override {
    if (runState() != RS_IDLE) runAbort("EXIT");
  }
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn == BTN_START && ev.type == EV_PRESS) {
      if (runState() == RS_IDLE)
        runStart();
      else
        runAbort("STOP BTN");  // aborts back to menu instantly (spec 2.1)
      return true;
    }
    if (ev.btn == BTN_BACK && ev.type == EV_PRESS && runState() != RS_IDLE) {
      runAbort("STOP BTN");
      return true;  // first BACK stops; second BACK leaves
    }
    return false;
  }
  void onTick(uint32_t now) override {
    (void)now;
    // quantized live values -> change-driven redraw (spec 6.1)
    int d = opponentDistMm();
    uint32_t s = ((uint32_t)runState() << 28) ^ ((uint32_t)runPhaseIdx() << 24) ^
                 ((uint32_t)(d < 0 ? 0xFFF : d / 10) << 8) ^
                 ((edgeL ? 2 : 0) | (edgeR ? 1 : 0)) ^
                 ((runCountdownMs() / 100) << 16) ^
                 ((runElapsedMs() / 200) << 4);
    if (s != snap_) {
      snap_ = s;
      OS.requestRedraw();
    }
  }
  void draw(U8G2 &g) override {
    char b[26];
    if (runState() == RS_COUNTDOWN) {
      snprintf(b, sizeof(b), "%d", (runCountdownMs() + 999) / 1000);
      g.setFont(u8g2_font_ncenB14_tr);
      g.drawStr((128 - g.getStrWidth(b)) / 2, Display.rowTop(2) + 8, b);
      Display.bodyRow(0, "Match start in...", false);
      Display.bodyRow(3, "START/STOP: abort", false);
      return;
    }
    if (runState() == RS_RUNNING) {
      Strategy &st = G.strategies[G.cur.activeStrategy];
      int d = opponentDistMm();
      if (d >= 0)
        snprintf(b, sizeof(b), "Opp: %dmm  T:%lus", d,
                 (unsigned long)(runElapsedMs() / 1000));
      else
        snprintf(b, sizeof(b), "Opp: ---   T:%lus",
                 (unsigned long)(runElapsedMs() / 1000));
      Display.bodyRow(0, b, false);
      snprintf(b, sizeof(b), "Edge: %s %s", edgeL ? "L!" : "L-",
               edgeR ? "R!" : "R-");
      Display.bodyRow(1, b, false);
      snprintf(b, sizeof(b), "Act: %s", runActionName());
      Display.bodyRow(2, b, false);
      snprintf(b, sizeof(b), "Phase %d/%d %s", runPhaseIdx() + 1,
               st.phaseCount, st.name);
      Display.bodyRow(3, b, false);
      return;
    }
    // idle
    Strategy &st = G.strategies[G.cur.activeStrategy];
    snprintf(b, sizeof(b), "Strategy: %s", st.used ? st.name : "NONE!");
    Display.bodyRow(0, b, false);
    Display.bodyRow(1, "Press START/STOP", false);
    Display.bodyRow(2, "for 5s countdown", false);
    if (runStopReason()[0]) {
      snprintf(b, sizeof(b), "Last: %s", runStopReason());
      Display.bodyRow(3, b, false);
    }
  }
  const char *hint() override {
    return runState() == RS_IDLE ? "ST:Start BK:Back" : "ST/BK:ABORT";
  }

 private:
  uint32_t snap_ = 0;
};
static RunScreen runScreen;
void openRunMode() { OS.push(&runScreen); }

// ---------------- SENSOR HEALTH ----------------
class SensorHealthScreen : public LzScreen {
 public:
  SensorHealthScreen() : LzScreen("SENSORS") {}
  void onEnter() override { top_ = 0; }
  bool onEvent(const LzEvent &ev) override {
    if (ev.type != EV_PRESS && ev.type != EV_REPEAT) return false;
    if (ev.btn == BTN_UP && top_ > 0) top_--;
    else if (ev.btn == BTN_DOWN && top_ < 3) top_++;  // 7 rows, window 4
    else return false;
    return true;
  }
  void onTick(uint32_t now) override {
    if (now - last_ >= 200) {  // 5 Hz live refresh, change-driven enough
      last_ = now;
      OS.requestRedraw();
    }
  }
  void draw(U8G2 &g) override {
    (void)g;
    char b[26];
    for (uint8_t r = 0; r < 4; r++) {
      uint8_t i = (uint8_t)(top_ + r);
      if (i < 5) {
        if (tofState[i].ok)
          snprintf(b, sizeof(b), "ToF%d %4dmm PASS", i + 1, tofState[i].mm);
        else
          snprintf(b, sizeof(b), "ToF%d  ---   FAIL", i + 1);
      } else if (i == 5) {
        snprintf(b, sizeof(b), "EdgeL %4d %s", edgeValL,
                 edgeValL > 0 ? "PASS" : "FAIL");
      } else {
        snprintf(b, sizeof(b), "EdgeR %4d %s", edgeValR,
                 edgeValR > 0 ? "PASS" : "FAIL");
      }
      Display.bodyRow(r, b, false);
    }
  }
  const char *hint() override { return "UD:Scroll BK:Back"; }

 private:
  uint8_t top_ = 0;
  uint32_t last_ = 0;
};
static SensorHealthScreen sensorHealth;
void openSensorHealth() { OS.push(&sensorHealth); }

// ---------------- CALIBRATE: Edge Threshold Wizard (3 steps) ------------
class EdgeWizardScreen : public LzScreen {
 public:
  EdgeWizardScreen() : LzScreen("EDGE-CAL") {}
  void onEnter() override { step_ = 0; }
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn != BTN_SELECT || ev.type != EV_PRESS) return false;
    int16_t avg = (int16_t)(((int32_t)edgeValL + edgeValR) / 2);
    if (step_ == 0) {
      white_ = avg;
      step_ = 1;
    } else if (step_ == 1) {
      dark_ = avg;
      computed_ = (int16_t)(((int32_t)white_ + dark_) / 2);
      step_ = 2;
    } else {
      G.cur.edgeThreshold = computed_;  // apply + persist (explicit save)
      talonSaveWithFeedback();
      step_ = 0;
    }
    return true;
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
    snprintf(b, sizeof(b), "Live: L%4d R%4d", edgeValL, edgeValR);
    if (step_ == 0) {
      Display.bodyRow(0, "1/3 Place sensors", false);
      Display.bodyRow(1, "on WHITE line", false);
      Display.bodyRow(2, b, false);
      Display.bodyRow(3, "SELECT: capture", false);
    } else if (step_ == 1) {
      Display.bodyRow(0, "2/3 Place sensors", false);
      Display.bodyRow(1, "on DARK surface", false);
      Display.bodyRow(2, b, false);
      Display.bodyRow(3, "SELECT: capture", false);
    } else {
      char c[26];
      snprintf(c, sizeof(c), "W:%d D:%d", white_, dark_);
      Display.bodyRow(0, "3/3 Auto-computed", false);
      Display.bodyRow(1, c, false);
      snprintf(c, sizeof(c), "Threshold = %d", computed_);
      Display.bodyRow(2, c, false);
      Display.bodyRow(3, "SELECT: save", false);
    }
  }
  const char *hint() override { return "SEL:Capture BK:Cancel"; }

 private:
  uint8_t step_ = 0;
  int16_t white_ = 0, dark_ = 0, computed_ = 0;
  uint32_t last_ = 0;
};
static EdgeWizardScreen edgeWizard;

// ---------------- CALIBRATE: ToF zero reference ----------------
class TofZeroScreen : public LzScreen {
 public:
  TofZeroScreen() : LzScreen("TOF-CAL") {}
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
      if (tofState[2].ok) {  // center sensor against a 100 mm target
        int16_t raw = (int16_t)(tofState[2].mm - G.cur.tofZeroOffsetMm);
        G.cur.tofZeroOffsetMm = (int16_t)(100 - raw);
        talonSaveWithFeedback();
      } else {
        Buzzer.play(SND_ERROR);
        Message.show("Center ToF FAIL", "Check wiring first");
      }
      return true;
    }
    return false;
  }
  void onTick(uint32_t now) override {
    if (now - last_ >= 150) {
      last_ = now;
      OS.requestRedraw();
    }
  }
  void draw(U8G2 &g) override {
    (void)g;
    char b[26];
    Display.bodyRow(0, "Target at 100mm,", false);
    Display.bodyRow(1, "then SELECT.", false);
    if (tofState[2].ok)
      snprintf(b, sizeof(b), "Center: %dmm", tofState[2].mm);
    else
      snprintf(b, sizeof(b), "Center: FAIL");
    Display.bodyRow(2, b, false);
    snprintf(b, sizeof(b), "Offset: %+dmm", G.cur.tofZeroOffsetMm);
    Display.bodyRow(3, b, false);
  }
  const char *hint() override { return "SEL:Capture BK:Back"; }

 private:
  uint32_t last_ = 0;
};
static TofZeroScreen tofZero;

static void openEdgeWizard() {
  if (OS.editAllowed()) OS.push(&edgeWizard);
}
static void openTofZero() {
  if (OS.editAllowed()) OS.push(&tofZero);
}
static const LzMenuItem CAL_ITEMS[] = {
    {"Edge Threshold Wiz", openEdgeWizard, nullptr},
    {"ToF Zero Reference", openTofZero, nullptr},
};
static LzMenuScreen calMenu("CALIB", CAL_ITEMS, 2);
void openCalibrate() { OS.push(&calMenu); }

// ---------------- MOTOR TEST (jog, spec 2.1) ----------------
class MotorTestScreen : public LzScreen {
 public:
  MotorTestScreen() : LzScreen("MTEST") {}
  void onLeave() override { talonMotorsStop(); }
  bool onEvent(const LzEvent &ev) override {
    if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) && ev.type == EV_PRESS) {
      sel_ = (uint8_t)((sel_ + (ev.btn == BTN_DOWN ? 1 : 2)) % 3);
      return true;
    }
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {  // cycle value
      if (sel_ == 0) motor_ ^= 1;
      else if (sel_ == 1) rev_ ^= 1;
      else speed_ = (uint8_t)(speed_ % 4 + 1);  // 25/50/75/100
      return true;
    }
    if (ev.btn == BTN_START) {  // press = jog, release = stop
      if (ev.type == EV_PRESS) {
        int16_t p = (int16_t)(speed_ * 25) * (rev_ ? -1 : 1);
        talonMotorsTestPulse(motor_ == 0 ? p : 0, motor_ == 1 ? p : 0, 30000);
      } else if (ev.type == EV_RELEASE) {
        talonMotorsStop();
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
static MotorTestScreen motorTest;
void openMotorTest() { OS.push(&motorTest); }

// ---------------- ORIENTATION (IMU, spec 2.1) ----------------
static void editTilt() {
  NumEditor.openI("Tilt limit", &G.cur.tiltLimitDeg, 20, 90, 5, " deg");
}
static void editPush() {
  NumEditor.openI("Push sense x0.1g", &G.cur.pushSense, 1, 20, 1);
}

class OrientationScreen : public LzScreen {
 public:
  OrientationScreen() : LzScreen("IMU") {}
  bool onEvent(const LzEvent &ev) override {
    if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) && ev.type == EV_PRESS) {
      sel_ = (uint8_t)((sel_ + (ev.btn == BTN_DOWN ? 1 : 2)) % 3);
      return true;
    }
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
      if (sel_ == 0) {
        if (OS.editAllowed()) {
          G.cur.autoStopFlip ^= 1;  // toggle
          Buzzer.play(SND_CONFIRM);
        }
      } else if (sel_ == 1) {
        editTilt();
      } else {
        editPush();
      }
      return true;
    }
    return false;
  }
  void onTick(uint32_t now) override {
    if (now - last_ >= 150) {
      last_ = now;
      OS.requestRedraw();
    }
  }
  void draw(U8G2 &g) override {
    (void)g;
    char b[26];
    const char *st = !imu.present ? "IMU FAIL"
                     : imu.flipped ? "FLIPPED!"
                     : imu.tiltDeg > G.cur.tiltLimitDeg ? "TILTED!"
                                                        : "FLAT ok";
    snprintf(b, sizeof(b), "[%s] %d deg %s", st, (int)imu.tiltDeg,
             imu.present ? "PASS" : "FAIL");
    Display.bodyRow(0, b, false);
    snprintf(b, sizeof(b), "AutoStop flip: %s", G.cur.autoStopFlip ? "ON" : "OFF");
    Display.bodyRow(1, b, sel_ == 0);
    snprintf(b, sizeof(b), "Tilt limit: %d deg", G.cur.tiltLimitDeg);
    Display.bodyRow(2, b, sel_ == 1);
    snprintf(b, sizeof(b), "Push sense: %d", G.cur.pushSense);
    Display.bodyRow(3, b, sel_ == 2);
  }
  const char *hint() override { return "UD SEL:Edit BK:Back"; }

 private:
  uint8_t sel_ = 0;
  uint32_t last_ = 0;
};
static OrientationScreen orientation;
void openOrientation() { OS.push(&orientation); }
