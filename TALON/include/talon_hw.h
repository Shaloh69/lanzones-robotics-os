// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON hardware layer: motors, 5x VL53L1X ToF, 2x edge sensors, IMU
// (MPU6050-class on the shared I2C bus), and the RUN MODE engine.
// All sensor reads are non-blocking with timeouts (spec 6.1) — a dead
// sensor shows FAIL in Sensor Health and never stalls the loop.
#pragma once
#include <Arduino.h>
#include <LzMotor.h>

extern LzMotor motorL, motorR;

// ---------------- sensors ----------------
struct TofState {
  uint16_t mm = 0;
  bool present = false;  // initialized OK at boot
  bool ok = false;       // fresh valid reading this cycle (PASS/FAIL)
};
extern TofState tofState[5];
extern int16_t edgeValL, edgeValR;  // raw reflectance
extern bool edgeL, edgeR;           // over-threshold (white boundary)

struct ImuState {
  bool present = false;
  float tiltDeg = 0;
  bool flipped = false;
  bool impact = false;  // latched push/impact spike; cleared by engine
};
extern ImuState imu;

int opponentDistMm();  // min valid ToF distance, -1 if none
int opponentDirIdx();  // 0..4 = sensor index of nearest, 2 = straight ahead

// ---------------- lifecycle ----------------
void talonHwBegin();             // motors + sensors + IMU + control timer
void talonMotorsReinit();
void talonHwTick(uint32_t now);  // non-blocking; call every loop
void talonMotorsTestPulse(int16_t pctL, int16_t pctR, uint16_t ms);
void talonMotorsStop();

// ---------------- RUN MODE engine (spec 2.1) ----------------
enum RunState : uint8_t { RS_IDLE, RS_COUNTDOWN, RS_RUNNING };
RunState runState();
uint8_t runPhaseIdx();
const char *runActionName();   // current action label for the live screen
uint16_t runCountdownMs();     // remaining, while RS_COUNTDOWN
uint32_t runElapsedMs();
const char *runStopReason();   // why the last run ended ("" if none)
void runStart();               // begins the 5 s competition countdown
void runAbort(const char *reason);  // instant stop (START/STOP button)
