// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR hardware layer: motors, 8x IR array, line PID, run engine
// (follow / learn / speed-run), auto-calibration.
#pragma once
#include <Arduino.h>
#include <LzMotor.h>

extern LzMotor motorL, motorR;

void vectorHwBegin();
void vectorMotorsReinit();
void vectorHwTick(uint32_t now);
void vectorMotorsTestPulse(int16_t pctL, int16_t pctR, uint16_t ms);
void vectorMotorsStop();

// ---------------- sensors ----------------
extern int16_t irRaw[8];    // raw ADC
extern int16_t irNorm[8];   // calibrated 0..1000 (1000 = on line)
extern bool irStuck[8];     // PASS/FAIL: dead/stuck channel detection
int linePosition();         // 0..7000 centroid, 3500 = centered; -1 = lost
float lineError();          // last PID error (for the live screen)

// ---------------- engine ----------------
enum VecMode : uint8_t {
  VM_IDLE, VM_FOLLOW,     // RUN MODE live following
  VM_LEARN,               // dry run: record path array
  VM_SPEED,               // execute the (edited) path array
  VM_AUTOCAL              // ~3 s sweep capturing min/max
};
VecMode vecMode();
void vecStart(VecMode m);   // VM_FOLLOW / VM_LEARN / VM_SPEED / VM_AUTOCAL
void vecStop(const char *reason);
const char *vecStopReason();
uint8_t vecJunctionCount(); // junctions handled this run
bool vecPathFull();         // learn hit VEC_MAX_PATH (bounds, spec 6.2)
uint8_t vecSpeedIdx();      // next instruction index during VM_SPEED
