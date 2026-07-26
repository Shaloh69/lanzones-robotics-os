// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON data model v2: tuning, strategies (spec 2.2 + advanced motion
// control addendum), profiles, persistence.
// All edits happen in RAM; flash is written only on explicit Save actions
// (spec 6.1/6.2 — batched writes, never during RUN MODE, never per keypress).
#pragma once
#include <Arduino.h>
#include <LzConfig.h>

// ---------------- Phase types ----------------
// New v2 types appended so v1 indices keep their meaning in code.
enum PhaseType : uint8_t {
  // SEARCH
  PH_SEARCH_SPIN, PH_SEARCH_SWEEP, PH_SEARCH_CHARGE,
  // ATTACK
  PH_ATT_RAM, PH_ATT_CURVE, PH_ATT_SIDE,
  // RETREAT
  PH_RET_BACKTURN, PH_RET_BACKONLY, PH_RET_CENTER,
  // addendum
  PH_SEARCH_CRAWL,   // slow probing creep, own speed field (addendum 1.4)
  PH_ANGLED_TURN,    // precise angle + direction (addendum 1.5)
  PH_TYPE_COUNT
};
// TR_CONTACT (spec, this pass): the front bumper microswitch — a genuine
// "contact made" confirmation, distinct from ToF proximity.
enum PhaseTrigger : uint8_t { TR_TIME, TR_OPPONENT, TR_EDGE, TR_CONTACT, TR_COUNT };

static inline bool phaseIsAttack(uint8_t t) {
  return t >= PH_ATT_RAM && t <= PH_ATT_SIDE;
}
static inline bool phaseIsRetreat(uint8_t t) {
  return t >= PH_RET_BACKTURN && t <= PH_RET_CENTER;
}
static inline bool phaseIsSearch(uint8_t t) {
  return t <= PH_SEARCH_CHARGE || t == PH_SEARCH_CRAWL;
}
static inline bool phaseIsSpinSweep(uint8_t t) {
  return t == PH_SEARCH_SPIN || t == PH_SEARCH_SWEEP;
}

extern const char *const PHASE_TYPE_NAMES[PH_TYPE_COUNT];
extern const char *const PHASE_TRIG_NAMES[TR_COUNT];
extern const char *const RETREAT_NAMES[3];
extern const char *const RESUME_NAMES[3];     // Edge Escape resume behavior
extern const char *const SLIP_RESP_NAMES[3];  // traction response
extern const char *const IGN_SENSOR_NAMES[8]; // ignore-window multi-select

// ---------------- Sensor Ignore Window mask (addendum 1.6) ----------------
enum : uint8_t {
  IGN_TOF_FRONT = 1 << 0,
  IGN_TOF_AL    = 1 << 1,  // Angled-Left ToF
  IGN_TOF_AR    = 1 << 2,  // Angled-Right ToF
  IGN_TOF_WL    = 1 << 3,  // Wide-Left ToF
  IGN_TOF_WR    = 1 << 4,  // Wide-Right ToF
  IGN_EDGE_L    = 1 << 5,
  IGN_EDGE_R    = 1 << 6,
  IGN_CONTACT   = 1 << 7,  // bump/contact microswitch (this pass)
};
// ToF array index (0=Wide-L,1=Angled-L,2=Front,3=Angled-R,4=Wide-R) -> bit
extern const uint8_t TOF_IGN_BIT[5];

// ---------------- Phase ----------------
struct Phase {
  uint8_t type = PH_SEARCH_SPIN;
  uint8_t trigger = TR_TIME;
  uint16_t durDs = 20;        // deciseconds
  uint16_t rampMs = 0;        // ATTACK ramp-up / RETREAT ramp-down; 0=instant
  uint8_t searchRadius = 0;   // Spin/Sweep: 0=in-place .. 100=straight arc
  uint8_t crawlPct = 25;      // Crawl speed (own field, addendum 1.4)
  int16_t angleDeg = 45;      // Angled Turn target angle (15 deg steps)
  uint8_t angleCW = 1;        // 1=clockwise
  uint16_t ignoreMs = 0;      // Sensor Ignore Window at phase start
  uint8_t ignoreMask = 0;     // IGN_* bits muted during the window
};

#define TALON_MAX_PHASES 8
struct Strategy {
  char name[LZ_NAME_LEN] = "";
  uint8_t used = 0;
  uint8_t phaseCount = 0;
  Phase phases[TALON_MAX_PHASES];
  uint16_t giveUpDs = 40;       // give-up safety timer (spec 2.2)
  uint8_t giveUpRetreat = 0;    // RETREAT_NAMES index
  // Edge Escape (addendum 1.7) — strategy-level interrupt, always on;
  // suppressed only via a phase's ignore window that includes Edge-L/R.
  uint8_t escapeManeuver = 0;   // RETREAT_NAMES index
  uint8_t escapeResume = 0;     // RESUME_NAMES index
  // Match Timer + Aggression Boost (addendum 1.8)
  uint16_t matchDurS = 180;     // match duration, seconds (3:00)
  uint16_t boostThreshS = 20;   // remaining-seconds threshold; 0 = disabled
  uint8_t boostPhase = 0;       // phase index to jump to on boost
};

// ---------------- Tuning / configuration ----------------
struct TalonTuning {
  float kp = 2.0f, ki = 0.0f, kd = 0.1f;
  int16_t maxFwdPct = 80, maxRevPct = 60;
  int16_t edgeThreshold = 500;   // retained for digital-edge polarity docs
  int16_t tofZeroOffsetMm = 0;
  uint8_t motorMode = 1;         // 0=ESC, 1=H-bridge (spec 2.3)
  uint8_t closedLoop = 0;
  uint8_t autoStopFlip = 1;
  int16_t tiltLimitDeg = 60;
  int16_t pushSense = 5;         // impact threshold, 0.1 g units
  float warnVoltage = 7.0f;
  uint8_t activeStrategy = 0;
  // Traction Control (addendum 1.1) — needs encoders + IMU
  uint8_t tractionEnable = 0;
  int16_t slipSense = 5;         // mismatch threshold (higher = less touchy)
  uint8_t slipResponse = 1;      // 0=Reduce Power, 1=Alert Only, 2=Retreat
  // Edge module polarity (spec: Edge Calibration): 0 = Active-High
  // (module output HIGH on the white boundary), 1 = Active-Low.
  uint8_t edgePolarity = 0;
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
  uint8_t lockedFlag = 0;  // Lock Config persists across power cycles (spec 1)
  TalonProfile profiles[TALON_MAX_PROFILES];
  Strategy strategies[TALON_MAX_STRATEGIES];
};
#define TALON_STORE_VERSION 3  // v3: edge polarity; older images load as defaults

extern TalonStore G;

void talonDefaults();
bool talonLoad();
bool talonSaveAll();
void talonSaveWithFeedback();
