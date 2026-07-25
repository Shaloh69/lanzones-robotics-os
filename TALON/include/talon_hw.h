// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON hardware layer interface: motors (+ test pulses), sensors, run engine.
#pragma once
#include <Arduino.h>
#include <LzMotor.h>

extern LzMotor motorL, motorR;

void talonHwBegin();                 // init motors per G.cur.motorMode
void talonMotorsReinit();            // after motor-mode config change
void talonHwTick(uint32_t now);      // non-blocking; call every loop
// timed open-loop pulse (PID test-drive / motor test) — ends automatically
void talonMotorsTestPulse(int16_t pctL, int16_t pctR, uint16_t ms);
void talonMotorsStop();
