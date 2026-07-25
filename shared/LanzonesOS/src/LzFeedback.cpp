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

// ---------------- Status LED (single RGB) ----------------
void LzLeds::begin() {
  pixel_.begin();
  pixel_.setBrightness(80);  // full 255 is eye-searing at close range
  pixel_.show();             // off
}

// resolves the priority-ordered state into an RGB color for `now`
static uint32_t resolveColor(Adafruit_NeoPixel &px, LzLedState state,
                            bool locked, bool lowBatt, uint32_t now) {
  bool fastBlink = (now / 120) & 1;
  bool slowBlink = (now / 400) & 1;
  if (lowBatt) return fastBlink ? px.Color(255, 0, 0) : 0;       // top priority
  if (state == LZLED_FAULT) return fastBlink ? px.Color(255, 0, 0) : 0;
  if (locked) return px.Color(0, 0, 255);                        // solid blue
  switch (state) {
    case LZLED_READY:     return px.Color(0, 255, 0);            // solid green
    case LZLED_ARMED:     return fastBlink ? px.Color(0, 255, 0) : 0;
    case LZLED_POSTMATCH: return slowBlink ? px.Color(255, 140, 0) : 0;
    default:              return 0;                               // OFF
  }
}

void LzLeds::tick(uint32_t now) {
  uint32_t c = resolveColor(pixel_, state_, locked_, lowBatt_, now);
  if (c == lastColor_) return;  // change-driven: skip the bit-bang write
  lastColor_ = c;
  pixel_.setPixelColor(0, c);
  pixel_.show();
}

// ---------------- Battery / power monitor (INA219) ----------------
void LzBattery::begin() {
  present_ = ina_.begin();
  if (present_) ina_.setCalibration_32V_2A();  // PLACEHOLDER: match shunt/PCB
}

void LzBattery::tick(uint32_t now) {
  if (!present_ || now - lastSample_ < 100) return;  // 10 Hz sampling
  lastSample_ = now;
  float v = ina_.getBusVoltage_V() + ina_.getShuntVoltage_mV() * 0.001f;
  currentMa_ = ina_.getCurrent_mA();
  powerMw_ = ina_.getPower_mW();
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
