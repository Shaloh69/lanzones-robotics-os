// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR CONFIGURE tree (spec 3.1): PID Tuning, Speed Profile, Line color
// mode, Path Array Editor (vector_path.cpp), Profiles, Save-all.
#include <LzOS.h>
#include <LzUi.h>

#include "vector_model.h"

void vectorMotorsReinit();  // vector_hw.cpp

// Path Array Editor entry — strong definition in vector_path.cpp (Step 3e).
void openPathEditor() __attribute__((weak));
void openPathEditor() { Message.show("Path Editor", "Coming in Step 3e"); }

// ---------------- PID tuning ----------------
static void editKp() { NumEditor.openF("Kp", &G.cur.kp, 0.0f, 10.0f, 0.05f, 2); }
static void editKi() { NumEditor.openF("Ki", &G.cur.ki, 0.0f, 2.0f, 0.01f, 2); }
static void editKd() { NumEditor.openF("Kd", &G.cur.kd, 0.0f, 5.0f, 0.05f, 2); }
static void vKp(char *o, size_t n) { snprintf(o, n, "%.2f", (double)G.cur.kp); }
static void vKi(char *o, size_t n) { snprintf(o, n, "%.2f", (double)G.cur.ki); }
static void vKd(char *o, size_t n) { snprintf(o, n, "%.2f", (double)G.cur.kd); }

static const LzMenuItem PID_ITEMS[] = {
    {"Kp", editKp, vKp},
    {"Ki", editKi, vKi},
    {"Kd", editKd, vKd},
};
static LzMenuScreen pidMenu("PID", PID_ITEMS, 3, "SEL:Edit BK:Back");
static void openPid() { OS.push(&pidMenu); }

// ---------------- Speed profile (spec 3.1) ----------------
static void editBase() { NumEditor.openI("Base speed %", &G.cur.baseSpeed, 10, 100, 5, "%"); }
static void editMax() { NumEditor.openI("Max straight %", &G.cur.maxSpeed, 10, 100, 5, "%"); }
static void editCorner() { NumEditor.openI("Corner cut %", &G.cur.cornerCut, 0, 80, 5, "%"); }
static void vBase(char *o, size_t n) { snprintf(o, n, "%d%%", G.cur.baseSpeed); }
static void vMax(char *o, size_t n) { snprintf(o, n, "%d%%", G.cur.maxSpeed); }
static void vCorner(char *o, size_t n) { snprintf(o, n, "-%d%%", G.cur.cornerCut); }

static const LzMenuItem SPEED_ITEMS[] = {
    {"Base speed", editBase, vBase},
    {"Max on straights", editMax, vMax},
    {"Corner reduction", editCorner, vCorner},
};
static LzMenuScreen speedMenu("SPEED", SPEED_ITEMS, 3, "SEL:Edit BK:Back");
static void openSpeed() { OS.push(&speedMenu); }

// ---------------- Line color mode ----------------
static const char *const LINE_MODES[] = {"Black-on-white", "White-on-black"};
static void editLineMode() {
  EnumEditor.open("Line color mode", LINE_MODES, 2, &G.cur.lineMode);
}
static void vLineMode(char *o, size_t n) {
  snprintf(o, n, "%s", G.cur.lineMode ? "W/B" : "B/W");
}

// ---------------- Closed loop ----------------
static const char *const LOOP_MODES[] = {"Open-loop", "Encoders"};
static void editLoopMode() {
  EnumEditor.open("Speed control", LOOP_MODES, 2, &G.cur.closedLoop);
}
static void vLoopMode(char *o, size_t n) {
  snprintf(o, n, "%s", G.cur.closedLoop ? "Enc" : "Open");
}

// ---------------- Profiles (Save As / Load / Delete) ----------------
static uint8_t profCount() {
  uint8_t n = 0;
  for (auto &p : G.profiles)
    if (p.used) n++;
  return n;
}
static int profSlot(uint8_t vis) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < VEC_MAX_PROFILES; i++)
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
  if (!OS.editAllowed()) return;
  G.cur = G.profiles[s].t;
  G.path = G.profiles[s].path;  // profile carries its path array (spec 3.2)
  vectorMotorsReinit();
  Battery.setWarnVoltage(G.cur.warnVoltage);
  Buzzer.play(SND_CONFIRM);
  Message.show("Profile loaded:", G.profiles[s].name);
}
static char newProfName[LZ_NAME_LEN];
static void profNameDone(bool ok) {
  if (!ok) return;
  for (uint8_t i = 0; i < VEC_MAX_PROFILES; i++) {
    if (!G.profiles[i].used) {
      G.profiles[i].used = 1;
      memcpy(G.profiles[i].name, newProfName, LZ_NAME_LEN);
      G.profiles[i].t = G.cur;
      G.profiles[i].path = G.path;  // snapshot tuning + path together
      vectorSaveWithFeedback();
      return;
    }
  }
}
static bool profCreate() {
  if (profCount() >= VEC_MAX_PROFILES) return false;  // bounds (spec 6.2)
  newProfName[0] = 0;
  NameEditor.open("Profile name:", newProfName, profNameDone);
  return true;
}
static void profDel(uint8_t i) {
  int s = profSlot(i);
  if (s < 0) return;
  G.profiles[s].used = 0;
  G.profiles[s].name[0] = 0;
  vectorSaveWithFeedback();
}
static const LzSlotOps PROF_OPS = {
    profCount, profName, profOpen, profCreate, profDel,
    "+ Save As (new)", "Profile slots full"};
static LzSlotListScreen profScreen("PROFILES", &PROF_OPS);
static void openProfiles() { OS.push(&profScreen); }

// ---------------- CONFIGURE menu ----------------
static void saveAll() { vectorSaveWithFeedback(); }

static const LzMenuItem CFG_ITEMS[] = {
    {"PID Tuning", openPid, nullptr},
    {"Speed Profile", openSpeed, nullptr},
    {"Line color", editLineMode, vLineMode},
    {"Speed control", editLoopMode, vLoopMode},
    {"Path Array Editor", openPathEditor, nullptr},
    {"Profiles", openProfiles, nullptr},
    {"Save All To Flash", saveAll, nullptr},
};
static LzMenuScreen cfgMenu("CONFIG", CFG_ITEMS,
                            sizeof(CFG_ITEMS) / sizeof(CFG_ITEMS[0]));
void openConfigure() { OS.push(&cfgMenu); }
