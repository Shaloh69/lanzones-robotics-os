// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON project-specific pins.
// ======================= !! PLACEHOLDERS — NOT FINAL !! ====================
// Everything in this file is a compile-time placeholder so the firmware
// builds and the UI is testable BEFORE the custom PCB wiring is finalized.
// Several placeholders intentionally OVERLAP each other (there are only 9
// free GPIOs on this package after the shared OS layer + reserved pins:
// PA2 PA3 PA8 PA15 PB1 PB3 PB4 PB5 PB10, of which only PA2/PA3/PB1 are ADC).
// The full sensor set (5x ToF XSHUT + 2 edge + motors + encoders) does NOT
// fit directly — the PCB will need an I2C GPIO expander or ToF address
// strategy. UPDATE EVERY PIN BELOW once the PCB is final.
// ===========================================================================
#pragma once
#include <Arduino.h>

// Motor driver (spec 2.3: ESC single-PWM or H-bridge PWM+DIR, selectable)
static const uint8_t PIN_MOTOR_L_PWM = PA8;   // PLACEHOLDER — TIM1_CH1
static const uint8_t PIN_MOTOR_L_DIR = PB4;   // PLACEHOLDER (H-bridge mode)
static const uint8_t PIN_MOTOR_R_PWM = PA15;  // PLACEHOLDER — TIM2_CH1
static const uint8_t PIN_MOTOR_R_DIR = PB5;   // PLACEHOLDER (H-bridge mode)

// 5x VL53L1X ToF — XSHUT lines for address assignment at boot
// PLACEHOLDER — overlaps motor/encoder pins; PCB will resolve (expander?)
static const uint8_t PIN_TOF_XSHUT[5] = {PB3, PB10, PB4, PB5, PA2};

// 2x edge reflectance sensors (analog)
static const uint8_t PIN_EDGE_LEFT = PA2;   // PLACEHOLDER — ADC1_IN2
static const uint8_t PIN_EDGE_RIGHT = PA3;  // PLACEHOLDER — ADC1_IN3

// Optional quadrature encoders (spec 2.3) — interrupt-capable pins
// PLACEHOLDER — overlaps ToF XSHUT placeholders; PCB will resolve
static const uint8_t PIN_ENC_L_A = PB3;
static const uint8_t PIN_ENC_L_B = PB10;
static const uint8_t PIN_ENC_R_A = PA2;
static const uint8_t PIN_ENC_R_B = PA3;

// IMU (MPU6050-class accelerometer) shares I2C1 with the OLED — no pin cost.
static const uint8_t IMU_I2C_ADDR = 0x68;
