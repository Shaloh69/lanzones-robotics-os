// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzFeedback.h"
#include "LzPins.h"

// ---------------- Buzzer ----------------
// freq,ms pairs. freq 0 = rest.
static const uint16_t SEQ_CLICK[]  = {3500, 10};
static const uint16_t SEQ_CONF[]   = {2200, 40, 3000, 70};
static const uint16_t SEQ_ERR[]    = {500, 100, 350, 160};
static const uint16_t SEQ_MATCH[]  = {1800, 90, 0, 120, 1800, 90, 0, 120,
                                      1800, 90, 0, 120, 2800, 350};
static const uint16_t SEQ_LOW[]    = {900, 70, 700, 70, 500, 110};
static const uint16_t SEQ_BOOT[]   = {1400, 60, 1900, 60, 2600, 90};

struct SeqRef { const uint16_t *p; uint8_t pairs; };
static const SeqRef SEQS[] = {
    {SEQ_CLICK, 1}, {SEQ_CONF, 2}, {SEQ_ERR, 2},
    {SEQ_MATCH, 7}, {SEQ_LOW, 3},  {SEQ_BOOT, 3},
};

void LzBuzzer::begin() { pinMode(LZ_PIN_BUZZER, OUTPUT); }

void LzBuzzer::play(LzSound s) {
  if (s >= (uint8_t)(sizeof(SEQS) / sizeof(SEQS[0]))) return;
  seq_ = SEQS[s].p;
  len_ = SEQS[s].pairs;
  idx_ = 0;
  active_ = true;
  stepEnd_ = 0;  // start immediately on next tick
}

void LzBuzzer::stop() {
  active_ = false;
  noTone(LZ_PIN_BUZZER);
}

void LzBuzzer::tick(uint32_t now) {
  if (!active_) return;
  if (stepEnd_ != 0 && now < stepEnd_) return;
  if (stepEnd_ != 0) idx_++;
  if (idx_ >= len_) {
    stop();
    return;
  }
  uint16_t f = seq_[idx_ * 2], ms = seq_[idx_ * 2 + 1];
  if (f)
    tone(LZ_PIN_BUZZER, f);
  else
    noTone(LZ_PIN_BUZZER);
  stepEnd_ = now + ms;
}

// ---------------- LEDs ----------------
void LzLeds::begin() {
  pinMode(LZ_PIN_LED_GREEN, OUTPUT);
  pinMode(LZ_PIN_LED_RED, OUTPUT);
}

bool LzLeds::phase(LzLedMode m, uint32_t now) {
  switch (m) {
    case LZLED_ON: return true;
    case LZLED_BLINK: return (now / 400) & 1;
    case LZLED_BLINK_FAST: return (now / 120) & 1;
    default: return false;
  }
}

void LzLeds::tick(uint32_t now) {
  digitalWrite(LZ_PIN_LED_GREEN, phase(g_, now) ? HIGH : LOW);
  digitalWrite(LZ_PIN_LED_RED, phase(r_, now) ? HIGH : LOW);
}

// ---------------- Battery ----------------
void LzBattery::begin() {
  pinMode(LZ_PIN_VBAT, INPUT_ANALOG);
  analogReadResolution(12);
}

void LzBattery::tick(uint32_t now) {
  if (now - lastSample_ < 100) return;  // 10 Hz sampling is plenty
  lastSample_ = now;
  float v = analogRead(LZ_PIN_VBAT) * (LZ_VBAT_ADC_REF / 4095.0f) * LZ_VBAT_DIVIDER;
  volts_ = (volts_ < 0) ? v : (volts_ * 0.8f + v * 0.2f);  // EMA smoothing
  // hysteresis so the alarm doesn't chatter at the threshold
  if (!low_ && volts_ < warnV_) low_ = true;
  else if (low_ && volts_ > warnV_ + 0.15f) low_ = false;
}

int LzBattery::percent() const {
  if (volts_ < 0) return -1;
  float p = (volts_ - LZ_VBAT_DEFAULT_MIN) /
            (LZ_VBAT_DEFAULT_MAX - LZ_VBAT_DEFAULT_MIN) * 100.0f;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (int)p;
}
