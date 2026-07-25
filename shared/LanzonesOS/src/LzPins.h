// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Shared "OS layer" pin assignments — spec section 1.2.
// One deviation, agreed with the team 2026-07-26: the spec listed the status
// LEDs on PC0/PC1, but those pins do not exist on the Black Pill's 48-pin
// (UFQFPN48) package — only PC13/14/15 of port C are bonded out, all reserved.
// LEDs moved to PB8 (green) / PB9 (red).
//
// Reserved pins (never route): PA9/PA10 (USART1 debug), PA11/PA12 (USB),
// PA13/PA14 (SWD), PB2 (BOOT1), PC13 (onboard LED), PC14/PC15 (oscillator),
// PA4-PA7 (WeAct SPI1 flash footprint).
#pragma once
#include <Arduino.h>

// OLED — hardware I2C1, default mapping
static const uint8_t LZ_PIN_OLED_SCL = PB6;
static const uint8_t LZ_PIN_OLED_SDA = PB7;

// Buttons — input w/ internal pull-up (active low)
static const uint8_t LZ_PIN_BTN_UP     = PB12;
static const uint8_t LZ_PIN_BTN_DOWN   = PB13;
static const uint8_t LZ_PIN_BTN_SELECT = PB14;
static const uint8_t LZ_PIN_BTN_BACK   = PB15;
static const uint8_t LZ_PIN_BTN_START  = PA0;  // also the WeAct onboard KEY button

// Feedback
static const uint8_t LZ_PIN_BUZZER    = PA1;   // TIM2_CH2-capable
static const uint8_t LZ_PIN_LED_GREEN = PB8;   // was PC0 in spec — see header note
static const uint8_t LZ_PIN_LED_RED   = PB9;   // was PC1 in spec — see header note

// Battery voltage divider
static const uint8_t LZ_PIN_VBAT = PB0;        // ADC1_IN8
