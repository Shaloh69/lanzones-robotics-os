// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Motors + timed test pulses (Step 3a). Sensors/IMU/run engine arrive in 3c.
#include "talon_hw.h"

#include "pin_config.h"
#include "talon_model.h"

LzMotor motorL, motorR;

static uint32_t pulseEnd = 0;
static bool pulsing = false;

void talonHwBegin() {
  LzMotor::Mode m = G.cur.motorMode == 0 ? LzMotor::ESC : LzMotor::HBRIDGE;
  motorL.begin(m, PIN_MOTOR_L_PWM, PIN_MOTOR_L_DIR);
  motorR.begin(m, PIN_MOTOR_R_PWM, PIN_MOTOR_R_DIR);
}

void talonMotorsReinit() { talonHwBegin(); }

void talonMotorsStop() {
  pulsing = false;
  motorL.stop();
  motorR.stop();
}

void talonMotorsTestPulse(int16_t pctL, int16_t pctR, uint16_t ms) {
  motorL.setPercent(pctL);
  motorR.setPercent(pctR);
  pulseEnd = millis() + ms;
  pulsing = true;
}

void talonHwTick(uint32_t now) {
  if (pulsing && (int32_t)(now - pulseEnd) >= 0) talonMotorsStop();
}
