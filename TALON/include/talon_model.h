// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON data model: tuning, strategies (spec 2.2), profiles, persistence.
// All edits happen in RAM; flash is written only on explicit Save actions
// (spec 6.1/6.2 — batched writes, never during RUN MODE, never per keypress).
#pragma once
#include <Arduino.h>
#include <LzConfig.h>

// ---------------- Strategy Builder model (spec 2.2) ----------------
enum PhaseType : uint8_t {
  // SEARCH
  PH_SEARCH_SPIN, PH_SEARCH_SWEEP, PH_SEARCH_CHARGE,
  // ATTACK
  PH_ATT_RAM, PH_ATT_CURVE, PH_ATT_SIDE,
  // RETREAT
  PH_RET_BACKTURN, PH_RET_BACKONLY, PH_RET_CENTER,
  PH_TYPE_COUNT
};
enum PhaseTrigger : uint8_t { TR_TIME, TR_OPPONENT, TR_EDGE, TR_COUNT };

static inline bool phaseIsAttack(uint8_t t) {
  return t >= PH_ATT_RAM && t <= PH_ATT_SIDE;
}
static inline bool phaseIsRetreat(uint8_t t) {
  return t >= PH_RET_BACKTURN && t <= PH_RET_CENTER;
}

extern const char *const PHASE_TYPE_NAMES[PH_TYPE_COUNT];
extern const char *const PHASE_TRIG_NAMES[TR_COUNT];
extern const char *const RETREAT_NAMES[3];  // for give-up retreat selection

struct Phase {
  uint8_t type = PH_SEARCH_SPIN;      // PhaseType
  uint8_t trigger = TR_TIME;          // PhaseTrigger
  uint16_t durDs = 20;                // duration, deciseconds (2.0 s)
};

#define TALON_MAX_PHASES 8
struct Strategy {
  char name[LZ_NAME_LEN] = "";
  uint8_t used = 0;
  uint8_t phaseCount = 0;
  Phase phases[TALON_MAX_PHASES];
  uint16_t giveUpDs = 40;             // give-up safety timer (4.0 s)
  uint8_t giveUpRetreat = 0;          // index into RETREAT_NAMES
};

// ---------------- Tuning / configuration ----------------
struct TalonTuning {
  float kp = 2.0f, ki = 0.0f, kd = 0.1f;
  int16_t maxFwdPct = 80, maxRevPct = 60;
  int16_t edgeThreshold = 500;        // reflectance cutoff (wizard-set)
  int16_t tofZeroOffsetMm = 0;
  uint8_t motorMode = 1;              // 0=ESC, 1=H-bridge (spec 2.3)
  uint8_t closedLoop = 0;             // encoder speed loop when available
  uint8_t autoStopFlip = 1;           // IMU auto-stop (spec 2.1)
  int16_t tiltLimitDeg = 60;
  int16_t pushSense = 5;              // impact threshold, 0.1 g units
  float warnVoltage = 7.0f;
  uint8_t activeStrategy = 0;         // index into strategies
};

#define TALON_MAX_PROFILES 5
struct TalonProfile {
  char name[LZ_NAME_LEN] = "";
  uint8_t used = 0;
  TalonTuning t;
};

#define TALON_MAX_STRATEGIES 6
struct TalonStore {
  TalonTuning cur;
  TalonProfile profiles[TALON_MAX_PROFILES];
  Strategy strategies[TALON_MAX_STRATEGIES];
};
#define TALON_STORE_VERSION 1

extern TalonStore G;

void talonDefaults();
bool talonLoad();      // boot: load from flash or fall back to defaults
bool talonSaveAll();   // explicit-save only; false in RUN MODE or on error
// Draws an immediate "Saving..." panel (flash erase blocks for ~1-2 s),
// runs talonSaveAll(), then reports the result.
void talonSaveWithFeedback();
