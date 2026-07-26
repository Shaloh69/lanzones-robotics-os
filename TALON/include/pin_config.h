// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON project-specific pins.
// ======================= !! PLACEHOLDERS — NOT FINAL !! ====================
// Updated per spec 1.2 (2026-07-26): the pin crunch is solved with an I2C
// GPIO expander (PCF8574-class) on the shared I2C1 bus — it carries the
// 5 ToF XSHUT lines (P0-P4), the 2 DIGITAL edge sensor outputs (P5/P6),
// and the front bumper contact microswitch (P7) — exactly 8 bits, one
// full PCF8574. Only the motor + optional encoder pins remain direct GPIO.
// UPDATE pins/address below once the PCB is final.
// ===========================================================================
#pragma once
#include <Arduino.h>

// Motor driver (spec 2.3: ESC single-PWM or H-bridge PWM+DIR, selectable)
static const uint8_t PIN_MOTOR_L_PWM = PA8;   // PLACEHOLDER — TIM1_CH1
static const uint8_t PIN_MOTOR_L_DIR = PB4;   // PLACEHOLDER (H-bridge mode)
static const uint8_t PIN_MOTOR_R_PWM = PA15;  // PLACEHOLDER — TIM2_CH1
static const uint8_t PIN_MOTOR_R_DIR = PB5;   // PLACEHOLDER (H-bridge mode)

// I2C GPIO expander #1 (spec 1.2): XSHUT x5 on P0-P4, edge digital on
// P5/P6, bump/contact microswitch on P7 — a full 8 bits, no room left on
// this chip for anything else.
#define TALON_EXPANDER_ADDR 0x20  // PLACEHOLDER — PCF8574 A2A1A0 = 000
// PCF8574 /INT output -> dedicated GPIO (spec 1.2, REQUIRED): edge changes
// wake an EXTI handler instead of 5 ms polling.
static const uint8_t PIN_EXPANDER_INT = PB1;  // PLACEHOLDER — EXTI1

// I2C GPIO expander #2 (this pass): physical strategy-select switch (DIP
// or rotary). Expander #1's 8 bits are fully committed above, so this is a
// second PCF8574 at a different address — still zero direct-GPIO cost,
// same shared I2C bus. Reads P0-P2 as a 3-bit position (0-7), mapped to a
// saved strategy slot (TALON_MAX_STRATEGIES=6 fits in 3 bits). Polled at a
// slow rate and only sampled while RS_IDLE/RS_POSTMATCH — never mid-match.
#define TALON_EXPANDER2_ADDR 0x21  // PLACEHOLDER — PCF8574 A2A1A0 = 001

// Optional quadrature encoders (spec 2.3) — interrupt-capable pins
static const uint8_t PIN_ENC_L_A = PB3;   // PLACEHOLDER
static const uint8_t PIN_ENC_L_B = PB10;  // PLACEHOLDER
static const uint8_t PIN_ENC_R_A = PA2;   // PLACEHOLDER
static const uint8_t PIN_ENC_R_B = PA3;   // PLACEHOLDER

// IMU (MPU6050-class accelerometer) shares I2C1 — no pin cost.
static const uint8_t IMU_I2C_ADDR = 0x68;

// ---------------------------------------------------------------------------
// Zero-cost PCB additions (spec 1, this pass) — no firmware code needed,
// physical header pins only:
//  - SWD debug-fallback header: break out PA13 (SWDIO) / PA14 (SWCLK) to a
//    small header. These are already reserved regardless of this project
//    (see LzPins.h), so this costs nothing but 2 broken-out header pins —
//    a recovery path via STM32CubeProgrammer's debug mode if a board ever
//    gets stuck in a state DFU can't recover from.
//  - Expansion header: break out any pin left over after the assignments
//    above to a small header, even unpopulated, to avoid a PCB re-spin if
//    a future addition is needed. Exact pin depends on final layout.
// ---------------------------------------------------------------------------
