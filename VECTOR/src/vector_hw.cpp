// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Motors + timed test pulses (Step 3d). Sensors/PID/engine arrive in 3e.
#include "vector_hw.h"

#include "pin_config.h"
#include "vector_model.h"

LzMotor motorL, motorR;

int16_t irRaw[8] = {};
int16_t irNorm[8] = {};
bool irStuck[8] = {};

static uint32_t pulseEnd = 0;
static bool pulsing = false;

void vectorHwBegin() {
  motorL.begin(LzMotor::HBRIDGE, PIN_MOTOR_L_PWM, PIN_MOTOR_L_DIR);
  motorR.begin(LzMotor::HBRIDGE, PIN_MOTOR_R_PWM, PIN_MOTOR_R_DIR);
}

void vectorMotorsReinit() { vectorHwBegin(); }

void vectorMotorsStop() {
  pulsing = false;
  motorL.stop();
  motorR.stop();
}

void vectorMotorsTestPulse(int16_t pctL, int16_t pctR, uint16_t ms) {
  motorL.setPercent(pctL);
  motorR.setPercent(pctR);
  pulseEnd = millis() + ms;
  pulsing = true;
}

// engine stubs until Step 3e
VecMode vecMode() { return VM_IDLE; }
void vecStart(VecMode m) { (void)m; }
void vecStop(const char *reason) { (void)reason; }
const char *vecStopReason() { return ""; }
uint8_t vecJunctionCount() { return 0; }
bool vecPathFull() { return false; }
uint8_t vecSpeedIdx() { return 0; }
int linePosition() { return -1; }
float lineError() { return 0; }

void vectorHwTick(uint32_t now) {
  if (pulsing && (int32_t)(now - pulseEnd) >= 0) vectorMotorsStop();
}
