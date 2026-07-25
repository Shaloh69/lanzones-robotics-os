// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR project-specific pins.
// ======================= !! PLACEHOLDERS — NOT FINAL !! ====================
// Everything in this file is a compile-time placeholder so the firmware
// builds and the UI is testable BEFORE the custom PCB wiring is finalized.
// Only 9 GPIOs are free after the shared OS layer + reserved pins
// (PA2 PA3 PA8 PA15 PB1 PB3 PB4 PB5 PB10) and only PA2/PA3/PB1 are ADC —
// so 8 direct analog IR channels DO NOT fit. The PCB will need an analog
// mux (e.g. CD4051: 3 select pins + 1 ADC input). The IR pin list below
// intentionally repeats the 3 real ADC pins as placeholders.
// UPDATE EVERY PIN BELOW once the PCB is final.
// ===========================================================================
#pragma once
#include <Arduino.h>

// Motor driver (spec 3.3: brushed H-bridge, PWM + DIR per motor)
static const uint8_t PIN_MOTOR_L_PWM = PA8;   // PLACEHOLDER — TIM1_CH1
static const uint8_t PIN_MOTOR_L_DIR = PB4;   // PLACEHOLDER
static const uint8_t PIN_MOTOR_R_PWM = PA15;  // PLACEHOLDER — TIM2_CH1
static const uint8_t PIN_MOTOR_R_DIR = PB5;   // PLACEHOLDER

// 8x analog IR line sensors
// PLACEHOLDER — repeats the only 3 ADC-capable free pins; needs mux on PCB
static const uint8_t PIN_IR[8] = {PA2, PA3, PB1, PA2, PA3, PB1, PA2, PA3};

// Optional quadrature encoders (spec 3.3) — interrupt-capable pins
// PLACEHOLDER — overlaps other placeholders; PCB will resolve
static const uint8_t PIN_ENC_L_A = PB3;
static const uint8_t PIN_ENC_L_B = PB10;
static const uint8_t PIN_ENC_R_A = PA2;
static const uint8_t PIN_ENC_R_B = PA3;
