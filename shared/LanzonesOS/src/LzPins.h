// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Shared "OS layer" pin assignments — spec section 1.2.
// Deviation, agreed with the team 2026-07-26: the spec listed a single RGB
// status LED on PC0, but that pin does not exist on the Black Pill's 48-pin
// (UFQFPN48) package — only PC13/14/15 of port C are bonded out, all
// reserved. Moved to PB8 (was previously "green LED" before the RGB
// consolidation below).
//
// Hardware consolidation (spec 1.2, this pass): the two discrete status
// LEDs collapsed into one WS2812-style addressable RGB LED (PB8, was
// PB8/PB9 for green/red — PB9 is now free), and the voltage-divider battery
// sense moved to an INA219 power monitor on the shared I2C bus, freeing
// PB0. Net: this pass frees PB9 and PB0 for project-specific use (2 pins).
//
// Reserved pins (never route): PA9/PA10 (USART1 debug), PA11/PA12 (USB),
// PA13/PA14 (SWD — also broken out to a debug-fallback header per spec 1),
// PB2 (BOOT1), PC13 (onboard LED), PC14/PC15 (oscillator),
// PA4-PA7 (WeAct SPI1 flash footprint).
#pragma once
#include <Arduino.h>

// OLED — hardware I2C1, default mapping. Also carries: IMU (TALON), INA219
// power monitor, 24LC256 EEPROM, PCF8574 GPIO expander(s) (TALON) — all
// I2C peripherals share this one bus, no extra pins per device.
static const uint8_t LZ_PIN_OLED_SCL = PB6;
static const uint8_t LZ_PIN_OLED_SDA = PB7;

// Buttons — input w/ internal pull-up (active low)
static const uint8_t LZ_PIN_BTN_UP     = PB12;
static const uint8_t LZ_PIN_BTN_DOWN   = PB13;
static const uint8_t LZ_PIN_BTN_SELECT = PB14;
static const uint8_t LZ_PIN_BTN_BACK   = PB15;
static const uint8_t LZ_PIN_BTN_START  = PA0;  // also the WeAct onboard KEY button

// Feedback
static const uint8_t LZ_PIN_BUZZER  = PA1;  // TIM2_CH2-capable
static const uint8_t LZ_PIN_LED_RGB = PB8;  // single WS2812-style data pin
// PB9 and PB0 are now free (see consolidation note above) — available for
// project-specific sensors/motors in each project's pin_config.h.
