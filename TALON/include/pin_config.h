// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON project-specific pins.
// ======================= !! PLACEHOLDERS — NOT FINAL !! ====================
// Updated per spec 1.2 (2026-07-26): the pin crunch is solved with an I2C
// GPIO expander (PCF8574-class) on the shared I2C1 bus — it carries the
// 5 ToF XSHUT lines (P0-P4) and the 2 DIGITAL edge sensor outputs (P5/P6).
// Only the motor + optional encoder pins remain direct GPIO.
// UPDATE pins/address below once the PCB is final.
// ===========================================================================
#pragma once
#include <Arduino.h>

// Motor driver (spec 2.3: ESC single-PWM or H-bridge PWM+DIR, selectable)
static const uint8_t PIN_MOTOR_L_PWM = PA8;   // PLACEHOLDER — TIM1_CH1
static const uint8_t PIN_MOTOR_L_DIR = PB4;   // PLACEHOLDER (H-bridge mode)
static const uint8_t PIN_MOTOR_R_PWM = PA15;  // PLACEHOLDER — TIM2_CH1
static const uint8_t PIN_MOTOR_R_DIR = PB5;   // PLACEHOLDER (H-bridge mode)

// I2C GPIO expander (spec 1.2): XSHUT x5 on P0-P4, edge digital on P5/P6
#define TALON_EXPANDER_ADDR 0x20  // PLACEHOLDER — PCF8574 A2A1A0 = 000

// Optional quadrature encoders (spec 2.3) — interrupt-capable pins
static const uint8_t PIN_ENC_L_A = PB3;   // PLACEHOLDER
static const uint8_t PIN_ENC_L_B = PB10;  // PLACEHOLDER
static const uint8_t PIN_ENC_R_A = PA2;   // PLACEHOLDER
static const uint8_t PIN_ENC_R_B = PA3;   // PLACEHOLDER

// IMU (MPU6050-class accelerometer) shares I2C1 — no pin cost.
static const uint8_t IMU_I2C_ADDR = 0x68;
