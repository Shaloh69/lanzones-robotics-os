// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR data model: tuning, IR calibration, path array (spec 3.2),
// profiles. RAM-edited; flash written only on explicit Save (spec 6.1/6.2).
#pragma once
#include <Arduino.h>
#include <LzConfig.h>

// ---------------- Path array (spec 3.2) ----------------
enum PathStep : uint8_t { STEP_F, STEP_L, STEP_R, STEP_U, STEP_COUNT };
extern const char *const STEP_NAMES[STEP_COUNT];  // "F","L","R","U"
extern const char *const TURN_STYLE_NAMES[3];     // Default/Smooth-Arc/Point

// Per-junction speed/turn configuration (spec 3.2). Zero fields fall back
// to the global Speed Profile defaults.
struct VecJcfg {
  uint8_t approachPct = 0;  // cruise % on the segment INTO this junction
  uint8_t brakeDs = 0;      // decel time x0.1s once the junction is seen
  uint8_t turnStyle = 0;    // 0=default(point), 1=smooth-arc, 2=point-turn
  uint8_t postPct = 0;      // speed % right after the turn (ramps back up)
  uint16_t reacqMs = 0;     // line-reacquisition timeout; 0 = default
};

#define VEC_MAX_PATH 48
struct VecPath {
  uint8_t len = 0;
  uint8_t steps[VEC_MAX_PATH] = {};
  VecJcfg cfg[VEC_MAX_PATH];
};

// ---------------- IR calibration ----------------
struct VecCal {
  int16_t mn[8];
  int16_t mx[8];
};

// ---------------- Tuning ----------------
struct VectorTuning {
  float kp = 0.8f, ki = 0.0f, kd = 0.3f;
  int16_t baseSpeed = 45;     // %
  int16_t maxSpeed = 70;      // % on straights
  int16_t cornerCut = 30;     // % speed reduction in corners
  uint8_t lineMode = 0;       // 0 = Black-on-white, 1 = White-on-black
  uint8_t closedLoop = 0;
  float warnVoltage = 7.0f;
};

#define VEC_MAX_PROFILES 4
struct VecProfile {
  char name[LZ_NAME_LEN] = "";
  uint8_t used = 0;
  VectorTuning t;
  VecPath path;  // path array is part of the profile (spec 3.2)
};

struct VectorStore {
  VectorTuning cur;
  uint8_t lockedFlag = 0;  // Lock Config persists across power cycles (spec 1)
  VecCal cal;
  VecPath path;  // working path (Learn Mode writes here)
  VecProfile profiles[VEC_MAX_PROFILES];
};
#define VECTOR_STORE_VERSION 2  // v2: per-junction cfg + lock persistence

extern VectorStore G;

void vectorDefaults();
bool vectorLoad();
bool vectorSaveAll();
void vectorSaveWithFeedback();
