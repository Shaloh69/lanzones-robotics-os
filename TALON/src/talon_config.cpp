// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON CONFIGURE tree (spec 2.1): PID Tuning (with test-drive), Speed
// Limits, Motor Config (spec 2.3), Strategy Builder (talon_strategy.cpp),
// Profiles (Save As / Load / Delete), Save-all.
#include <LzOS.h>
#include <LzUi.h>

#include "talon_hw.h"
#include "talon_model.h"

// Strategy Builder entry point — implemented in talon_strategy.cpp (Step 3b).
void openStrategyBuilder() __attribute__((weak));
void openStrategyBuilder() { Message.show("Strategy Builder", "Coming in Step 3b"); }

// ---------------- PID tuning (test-drive briefly spins motors) ----------
static void pidTestDrive() { talonMotorsTestPulse(50, 50, 400); }

static void editKp() { NumEditor.openF("Kp", &G.cur.kp, 0.0f, 20.0f, 0.1f, 2, nullptr, pidTestDrive); }
static void editKi() { NumEditor.openF("Ki", &G.cur.ki, 0.0f, 5.0f, 0.01f, 2, nullptr, pidTestDrive); }
static void editKd() { NumEditor.openF("Kd", &G.cur.kd, 0.0f, 5.0f, 0.01f, 2, nullptr, pidTestDrive); }

static void vKp(char *o, size_t n) { snprintf(o, n, "%.2f", (double)G.cur.kp); }
static void vKi(char *o, size_t n) { snprintf(o, n, "%.2f", (double)G.cur.ki); }
static void vKd(char *o, size_t n) { snprintf(o, n, "%.2f", (double)G.cur.kd); }

static const LzMenuItem PID_ITEMS[] = {
    {"Kp", editKp, vKp},
    {"Ki", editKi, vKi},
    {"Kd", editKd, vKd},
};
static LzMenuScreen pidMenu("PID", PID_ITEMS, 3, "SEL:Edit ST:Test BK:Back");
static void openPid() { OS.push(&pidMenu); }

// ---------------- Speed limits ----------------
static void editFwd() { NumEditor.openI("Max forward %", &G.cur.maxFwdPct, 10, 100, 5, "%"); }
static void editRev() { NumEditor.openI("Max reverse %", &G.cur.maxRevPct, 10, 100, 5, "%"); }
static void vFwd(char *o, size_t n) { snprintf(o, n, "%d%%", G.cur.maxFwdPct); }
static void vRev(char *o, size_t n) { snprintf(o, n, "%d%%", G.cur.maxRevPct); }

static const LzMenuItem SPEED_ITEMS[] = {
    {"Max forward", editFwd, vFwd},
    {"Max reverse", editRev, vRev},
};
static LzMenuScreen speedMenu("SPEED", SPEED_ITEMS, 2, "SEL:Edit BK:Back");
static void openSpeed() { OS.push(&speedMenu); }

// ---------------- Motor config (spec 2.3) ----------------
static const char *const MOTOR_MODES[] = {"ESC (1xPWM)", "H-bridge"};
static const char *const LOOP_MODES[] = {"Open-loop", "Encoders"};

static void motorModeDone(bool saved) {
  if (saved) talonMotorsReinit();
}
static void editMotorMode() {
  EnumEditor.open("Motor driver", MOTOR_MODES, 2, &G.cur.motorMode, motorModeDone);
}
static void editLoopMode() {
  EnumEditor.open("Speed control", LOOP_MODES, 2, &G.cur.closedLoop);
}
static void vMotorMode(char *o, size_t n) { snprintf(o, n, "%s", G.cur.motorMode == 0 ? "ESC" : "H-brdg"); }
static void vLoopMode(char *o, size_t n) { snprintf(o, n, "%s", G.cur.closedLoop ? "Enc" : "Open"); }

static const LzMenuItem MOTORCFG_ITEMS[] = {
    {"Driver type", editMotorMode, vMotorMode},
    {"Speed control", editLoopMode, vLoopMode},
};
static LzMenuScreen motorCfgMenu("MOTORS", MOTORCFG_ITEMS, 2, "SEL:Edit BK:Back");
static void openMotorCfg() { OS.push(&motorCfgMenu); }

// ---------------- Traction Control (addendum 1.1) ----------------
static const char *const TRAC_MODES[] = {"Disabled", "Enabled"};
static void editTracEnable() {
  EnumEditor.open("Traction control", TRAC_MODES, 2, &G.cur.tractionEnable);
}
static void editSlipSense() {
  NumEditor.openI("Slip sensitivity", &G.cur.slipSense, 1, 20, 1);
}
static void editSlipResp() {
  EnumEditor.open("Response on slip", SLIP_RESP_NAMES, 3, &G.cur.slipResponse);
}
static void vTracEnable(char *o, size_t n) {
  snprintf(o, n, "%s", G.cur.tractionEnable ? "ON" : "OFF");
}
static void vSlipSense(char *o, size_t n) { snprintf(o, n, "%d", G.cur.slipSense); }
static void vSlipResp(char *o, size_t n) {
  snprintf(o, n, "%s", SLIP_RESP_NAMES[G.cur.slipResponse < 3 ? G.cur.slipResponse : 1]);
}

static const LzMenuItem TRAC_ITEMS[] = {
    {"Enable", editTracEnable, vTracEnable},
    {"Slip sensitivity", editSlipSense, vSlipSense},
    {"Response on slip", editSlipResp, vSlipResp},
};
static LzMenuScreen tracMenu("TRACTION", TRAC_ITEMS, 3, "SEL:Edit BK:Back");
static void openTraction() { OS.push(&tracMenu); }

// ---------------- Profiles (Save As / Load / Delete) ----------------
static uint8_t profCount() {
  uint8_t n = 0;
  for (auto &p : G.profiles)
    if (p.used) n++;
  return n;
}
// map visible index -> storage slot
static int profSlot(uint8_t vis) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < TALON_MAX_PROFILES; i++)
    if (G.profiles[i].used && n++ == vis) return i;
  return -1;
}
static void profName(uint8_t i, char *out, size_t n) {
  int s = profSlot(i);
  snprintf(out, n, "%s", s >= 0 ? G.profiles[s].name : "?");
}
static void profOpen(uint8_t i) {
  int s = profSlot(i);
  if (s < 0) return;
  if (!OS.editAllowed()) return;  // loading changes config
  G.cur = G.profiles[s].t;
  talonMotorsReinit();
  Battery.setWarnVoltage(G.cur.warnVoltage);
  Buzzer.play(SND_CONFIRM);
  Message.show("Profile loaded:", G.profiles[s].name);
}
static char newProfName[LZ_NAME_LEN];
static void profNameDone(bool ok) {
  if (!ok) return;
  for (uint8_t i = 0; i < TALON_MAX_PROFILES; i++) {
    if (!G.profiles[i].used) {
      G.profiles[i].used = 1;
      memcpy(G.profiles[i].name, newProfName, LZ_NAME_LEN);
      G.profiles[i].t = G.cur;  // snapshot of current full config
      talonSaveWithFeedback();  // explicit Save As -> batched flash write
      return;
    }
  }
}
static bool profCreate() {
  if (profCount() >= TALON_MAX_PROFILES) return false;  // bounds (spec 6.2)
  newProfName[0] = 0;
  NameEditor.open("Profile name:", newProfName, profNameDone);
  return true;
}
static void profDel(uint8_t i) {
  int s = profSlot(i);
  if (s < 0) return;
  G.profiles[s].used = 0;
  G.profiles[s].name[0] = 0;
  talonSaveWithFeedback();
}
static const LzSlotOps PROF_OPS = {
    profCount, profName, profOpen, profCreate, profDel,
    "+ Save As (new)", "Profile slots full"};
static LzSlotListScreen profScreen("PROFILES", &PROF_OPS);
static void openProfiles() { OS.push(&profScreen); }

// ---------------- CONFIGURE menu ----------------
static void saveAll() { talonSaveWithFeedback(); }

static const LzMenuItem CFG_ITEMS[] = {
    {"PID Tuning", openPid, nullptr},
    {"Speed Limits", openSpeed, nullptr},
    {"Traction Control", openTraction, nullptr},
    {"Motor Config", openMotorCfg, nullptr},
    {"Strategy Builder", openStrategyBuilder, nullptr},
    {"Profiles", openProfiles, nullptr},
    {"Save All To Flash", saveAll, nullptr},
};
static LzMenuScreen cfgMenu("CONFIG", CFG_ITEMS,
                            sizeof(CFG_ITEMS) / sizeof(CFG_ITEMS[0]));
void openConfigure() { OS.push(&cfgMenu); }
