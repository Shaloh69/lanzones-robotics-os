// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR project-specific pins.
// ======================= !! PLACEHOLDERS — NOT FINAL !! ====================
// Updated per spec 1.2 (2026-07-26): the 8-channel IR array is read through
// a CD4051 8:1 analog multiplexer — 3 digital select lines + 1 ADC input
// (4 pins instead of 8). UPDATE pins below once the PCB is final.
// ===========================================================================
#pragma once
#include <Arduino.h>

// Motor driver (spec 3.3: brushed H-bridge, PWM + DIR per motor)
static const uint8_t PIN_MOTOR_L_PWM = PA8;   // PLACEHOLDER — TIM1_CH1
static const uint8_t PIN_MOTOR_L_DIR = PB4;   // PLACEHOLDER
static const uint8_t PIN_MOTOR_R_PWM = PA15;  // PLACEHOLDER — TIM2_CH1
static const uint8_t PIN_MOTOR_R_DIR = PB5;   // PLACEHOLDER

// CD4051 analog mux for the 8x IR array (spec 1.2)
static const uint8_t PIN_MUX_S0 = PB3;   // PLACEHOLDER
static const uint8_t PIN_MUX_S1 = PB10;  // PLACEHOLDER
static const uint8_t PIN_MUX_S2 = PA2;   // PLACEHOLDER
static const uint8_t PIN_MUX_COM = PB1;  // PLACEHOLDER — ADC1_IN9

// Optional quadrature encoders (spec 3.3)
// PLACEHOLDER — only PA3 remains free; second encoder pins overlap the mux
// selects until the PCB decides (encoders are optional hardware)
static const uint8_t PIN_ENC_L_A = PA3;
static const uint8_t PIN_ENC_L_B = PA3;
static const uint8_t PIN_ENC_R_A = PA3;
static const uint8_t PIN_ENC_R_B = PA3;
