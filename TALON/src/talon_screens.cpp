// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON screens: RUN MODE (countdown + live status + match timer +
// post-match Quick Rematch), SENSOR HEALTH, CALIBRATE (edge check + ToF
// zero), MOTOR TEST, ORIENTATION. Live screens redraw change-driven.
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
    if (runState() == RS_COUNTDOWN || runState() == RS_RUNNING)
      runAbort("EXIT");
  }
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn == BTN_START && ev.type == EV_PRESS) {
      if (runState() == RS_IDLE || runState() == RS_POSTMATCH)
        runStart();  // arm — from post-match this IS Quick Rematch (spec 2.1)
      else
        runAbort("STOP BTN");
      return true;
    }
    if (ev.btn == BTN_BACK && ev.type == EV_PRESS &&
        (runState() == RS_COUNTDOWN || runState() == RS_RUNNING)) {
      runAbort("STOP BTN");
      return true;
    }
    return false;
  }
  void onTick(uint32_t now) override {
    (void)now;
    int d = opponentDistMm();
    uint32_t s = ((uint32_t)runState() << 28) ^ ((uint32_t)runPhaseIdx() << 24) ^
                 ((uint32_t)(d < 0 ? 0xFFF : d / 10) << 8) ^
                 ((edgeL ? 2 : 0) | (edgeR ? 1 : 0)) ^
                 ((runCountdownMs() / 100) << 16) ^
                 ((runMatchRemainingMs() / 1000) << 4) ^
                 ((uint32_t)tractionSlipActive() << 27);
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
      uint32_t rem = runMatchRemainingMs() / 1000;
      int d = opponentDistMm();
      if (d >= 0)
        snprintf(b, sizeof(b), "Opp:%4dmm  %lu:%02lu", d,
                 (unsigned long)(rem / 60), (unsigned long)(rem % 60));
      else
        snprintf(b, sizeof(b), "Opp: ---    %lu:%02lu",
                 (unsigned long)(rem / 60), (unsigned long)(rem % 60));
      Display.bodyRow(0, b, false);
      snprintf(b, sizeof(b), "Edge:%s %s %s%s", edgeL ? "L!" : "L-",
               edgeR ? "R!" : "R-", runBoosted() ? "BOOST " : "",
               tractionSlipActive() ? "SLIP!" : "");
      Display.bodyRow(1, b, false);
      snprintf(b, sizeof(b), "Act: %s", runActionName());
      Display.bodyRow(2, b, false);
      snprintf(b, sizeof(b), "Phase %d/%d %s", runPhaseIdx() + 1,
               st.phaseCount, st.name);
      Display.bodyRow(3, b, false);
      return;
    }
    if (runState() == RS_POSTMATCH) {  // post-match: Quick Rematch (spec 2.1)
      Display.bodyRow(0, "== MATCH OVER ==", false);
      snprintf(b, sizeof(b), "Result: %s", runStopReason());
      Display.bodyRow(1, b, false);
      Display.bodyRow(2, "START: Quick", false);
      Display.bodyRow(3, "Rematch  BK: exit", false);
      return;
    }
    // idle
    Strategy &st = G.strategies[G.cur.activeStrategy];
    snprintf(b, sizeof(b), "Strategy: %s", st.used ? st.name : "NONE!");
    Display.bodyRow(0, b, false);
    snprintf(b, sizeof(b), "Match len: %d:%02d", st.matchDurS / 60,
             st.matchDurS % 60);
    Display.bodyRow(1, b, false);
    Display.bodyRow(2, "START: arm (5s)", false);
    if (runStopReason()[0]) {
      snprintf(b, sizeof(b), "Last: %s", runStopReason());
      Display.bodyRow(3, b, false);
    }
  }
  const char *hint() override {
    switch (runState()) {
      case RS_IDLE: return "ST:Start BK:Back";
      case RS_POSTMATCH: return "ST:Rematch BK:Exit";
      default: return "ST/BK:ABORT";
    }
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
    else if (ev.btn == BTN_DOWN && top_ < 4) top_++;  // 8 rows, window of 4
    else return false;
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
    static const char *const TOF_NAMES[5] = {"WdL", "AnL", "Frt", "AnR", "WdR"};
    char b[26];
    for (uint8_t r = 0; r < 4; r++) {
      uint8_t i = (uint8_t)(top_ + r);
      if (i < 5) {
        if (tofState[i].ok)
          snprintf(b, sizeof(b), "ToF %s %4dmm PASS", TOF_NAMES[i],
                   tofState[i].mm);
        else
          snprintf(b, sizeof(b), "ToF %s  ---  FAIL", TOF_NAMES[i]);
      } else if (i == 5) {
        snprintf(b, sizeof(b), "EdgeL %s %s", edgeL ? "TRIP " : "clear",
                 expanderOk ? "PASS" : "FAIL");
      } else if (i == 6) {
        snprintf(b, sizeof(b), "EdgeR %s %s", edgeR ? "TRIP " : "clear",
                 expanderOk ? "PASS" : "FAIL");
      } else {
        snprintf(b, sizeof(b), "Bump  %s %s", bumpContact ? "TRIP " : "clear",
                 expanderOk ? "PASS" : "FAIL");
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

// ---------------- CALIBRATE: Edge sensor check (digital modules) ---------
// Edge sensors are digital (expander P5/P6) per updated spec 1.2 — the
// threshold lives in each module's onboard pot, so the old analog capture
// wizard becomes a guided VERIFICATION: white must TRIP, dark must CLEAR.
class EdgeCheckScreen : public LzScreen {
 public:
  EdgeCheckScreen() : LzScreen("EDGE-CAL") {}
  void onEnter() override { step_ = 0; okWhite_ = okDark_ = false; }
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn != BTN_SELECT || ev.type != EV_PRESS) return false;
    if (step_ == 0) {
      okWhite_ = edgeL && edgeR;  // both must see the white boundary
      step_ = 1;
    } else if (step_ == 1) {
      okDark_ = !edgeL && !edgeR;  // both must clear on dark clay
      step_ = 2;
      Buzzer.play((okWhite_ && okDark_) ? SND_CONFIRM : SND_ERROR);
    } else {
      step_ = 0;
    }
    return true;
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
    snprintf(b, sizeof(b), "Live: L=%s R=%s", edgeL ? "TRIP" : "clr",
             edgeR ? "TRIP" : "clr");
    if (step_ == 0) {
      Display.bodyRow(0, "1/3 Sensors on", false);
      Display.bodyRow(1, "WHITE line...", false);
      Display.bodyRow(2, b, false);
      Display.bodyRow(3, "SELECT: check", false);
    } else if (step_ == 1) {
      Display.bodyRow(0, "2/3 Sensors on", false);
      Display.bodyRow(1, "DARK surface...", false);
      Display.bodyRow(2, b, false);
      Display.bodyRow(3, "SELECT: check", false);
    } else {
      snprintf(b, sizeof(b), "White:%s Dark:%s", okWhite_ ? "PASS" : "FAIL",
               okDark_ ? "PASS" : "FAIL");
      Display.bodyRow(0, "3/3 Result", false);
      Display.bodyRow(1, b, false);
      Display.bodyRow(2, okWhite_ && okDark_ ? "Edge sensors OK"
                                             : "Adjust module pot!",
                      false);
      Display.bodyRow(3, "SELECT: redo", false);
    }
  }
  const char *hint() override { return "SEL:Step BK:Back"; }

 private:
  uint8_t step_ = 0;
  bool okWhite_ = false, okDark_ = false;
  uint32_t last_ = 0;
};
static EdgeCheckScreen edgeCheck;

// ---------------- CALIBRATE: ToF zero reference ----------------
class TofZeroScreen : public LzScreen {
 public:
  TofZeroScreen() : LzScreen("TOF-CAL") {}
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
      if (tofState[2].ok) {  // front sensor against a 100 mm target
        int16_t raw = (int16_t)(tofState[2].mm - G.cur.tofZeroOffsetMm);
        G.cur.tofZeroOffsetMm = (int16_t)(100 - raw);
        talonSaveWithFeedback();
      } else {
        Buzzer.play(SND_ERROR);
        Message.show("Front ToF FAIL", "Check wiring first");
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
      snprintf(b, sizeof(b), "Front: %dmm", tofState[2].mm);
    else
      snprintf(b, sizeof(b), "Front: FAIL");
    Display.bodyRow(2, b, false);
    snprintf(b, sizeof(b), "Offset: %+dmm", G.cur.tofZeroOffsetMm);
    Display.bodyRow(3, b, false);
  }
  const char *hint() override { return "SEL:Capture BK:Back"; }

 private:
  uint32_t last_ = 0;
};
static TofZeroScreen tofZero;

static void openEdgeCheck() { OS.push(&edgeCheck); }
static void openTofZero() {
  if (OS.editAllowed()) OS.push(&tofZero);
}
// Edge Polarity (spec Edge Calibration): matches the module's output sense.
static const char *const EDGE_POL_NAMES[2] = {"Active-High", "Active-Low"};
static void editEdgePolarity() {
  EnumEditor.open("Edge polarity", EDGE_POL_NAMES, 2, &G.cur.edgePolarity);
}
static void vEdgePol(char *o, size_t n) {
  snprintf(o, n, "%s", G.cur.edgePolarity ? "Act-LO" : "Act-HI");
}
static const LzMenuItem CAL_ITEMS[] = {
    {"Edge Sensor Check", openEdgeCheck, nullptr},
    {"Edge Polarity", editEdgePolarity, vEdgePol},
    {"ToF Zero Reference", openTofZero, nullptr},
};
static LzMenuScreen calMenu("CALIB", CAL_ITEMS, 3);
void openCalibrate() { OS.push(&calMenu); }

// ---------------- MOTOR TEST ----------------
class MotorTestScreen : public LzScreen {
 public:
  MotorTestScreen() : LzScreen("MTEST") {}
  void onLeave() override { talonMotorsStop(); }
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
    if (ev.btn == BTN_START) {
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

// ---------------- ORIENTATION (IMU) ----------------
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
          G.cur.autoStopFlip ^= 1;
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
