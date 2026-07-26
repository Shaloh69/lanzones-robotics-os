// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON hardware layer: motors, 5x VL53L1X ToF (XSHUT via PCF8574 I2C
// expander, spec 1.2), 2x digital edge sensors (expander inputs), IMU,
// and the RUN MODE engine (spec 2.1 + advanced motion control addendum).
#pragma once
#include <Arduino.h>
#include <LzMotor.h>

extern LzMotor motorL, motorR;
extern LzEncoder encL, encR;

// ---------------- sensors ----------------
struct TofState {
  uint16_t mm = 0;
  bool present = false;
  bool ok = false;
};
// index: 0=Wide-L, 1=Angled-L, 2=Front, 3=Angled-R, 4=Wide-R
extern TofState tofState[5];
extern bool edgeL, edgeR;        // digital edge state (expander #1 P5/P6)
extern bool expanderOk;          // PCF8574 #1 responded at boot
extern bool bumpContact;         // front bumper microswitch (expander #1 P7)
extern bool expander2Ok;         // PCF8574 #2 (strategy switch) responded

struct ImuState {
  bool present = false;
  float tiltDeg = 0;
  bool flipped = false;
  bool impact = false;
};
extern ImuState imu;

int opponentDistMm();            // unmasked (motion/aiming)
int opponentDirIdx();

// ---------------- lifecycle ----------------
void talonHwBegin();
void talonMotorsReinit();
void talonHwTick(uint32_t now);
void talonMotorsTestPulse(int16_t pctL, int16_t pctR, uint16_t ms);
void talonMotorsStop();

// ---------------- RUN MODE engine ----------------
enum RunState : uint8_t { RS_IDLE, RS_COUNTDOWN, RS_RUNNING, RS_POSTMATCH };
RunState runState();
uint8_t runPhaseIdx();
const char *runActionName();
uint16_t runCountdownMs();
uint32_t runElapsedMs();
uint32_t runMatchRemainingMs();  // match timer countdown (addendum 1.8)
bool runBoosted();               // aggression boost fired
bool tractionSlipActive();       // traction control slip flag (addendum 1.1)
const char *runStopReason();
void runStart();                 // also serves Quick Rematch from post-match
void runAbort(const char *reason);
