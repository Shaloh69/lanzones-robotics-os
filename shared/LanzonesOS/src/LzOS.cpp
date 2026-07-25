// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzOS.h"

#include <IWatchdog.h>

#include "LzStore.h"
#include "LzUi.h"

LzOS OS;
LzButtons Buttons;
LzDisplay Display;
LzBuzzer Buzzer;
LzLeds Leds;
LzBattery Battery;

void LzOS::begin(const char *name, const char *bid) {
  osName = name;
  buildId = bid;
  Serial.begin(115200);  // USART1 PA9/PA10: debug + config export/import
  Buttons.begin();
  Display.begin();
  Buzzer.begin();
  Leds.begin();
  Battery.begin();
  // Watchdog (spec 6.2) — resets the board if the main loop ever hangs.
  IWatchdog.begin(LZ_WATCHDOG_US);
  Buzzer.play(SND_BOOT);
}

// Boot-time bounded wait — keeps the watchdog fed and the boot jingle
// playing. Only ever used inside the splash, before the state machine runs.
static void splashWait(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    IWatchdog.reload();
    Buzzer.tick(millis());
  }
}

// SSD1306 "fade" = contrast ramp (the panel has no grayscale).
static void fadeTo(U8G2 &g, int from, int to) {
  int step = (to > from) ? 15 : -15;
  for (int c = from; (step > 0) ? c < to : c > to; c += step) {
    g.setContrast((uint8_t)c);
    splashWait(15);
  }
  g.setContrast((uint8_t)to);
}

void LzOS::showSplash(const uint8_t *logo64) {
  U8G2 &g = Display.gfx();

  // Frame 1: OS logo alone — 64x64, full display height, centered.
  // logos.h is Adafruit-style horizontal MSB-first => drawBitmap, not XBM.
  g.setContrast(0);
  g.clearBuffer();
  if (logo64) g.drawBitmap(32, 0, 8, 64, logo64);
  g.sendBuffer();
  fadeTo(g, 0, 255);   // fade in
  splashWait(600);     // hold
  fadeTo(g, 255, 0);   // fade out toward the text frame

  // Frame 2: OS name (large) + firmware version + "LANZONES x KOOGS"
  g.clearBuffer();
  g.setFont(u8g2_font_ncenB14_tr);
  g.drawStr((128 - g.getStrWidth(osName)) / 2, 26, osName);
  g.setFont(u8g2_font_5x7_tr);
  g.drawStr((128 - g.getStrWidth(LZ_FW_VERSION)) / 2, 42, LZ_FW_VERSION);
  g.drawStr((128 - g.getStrWidth(LZ_BRAND_SHORT)) / 2, 58, LZ_BRAND_SHORT);
  g.sendBuffer();
  fadeTo(g, 0, 255);
  splashWait(900);
}

void LzOS::rebuildCrumb() {
  crumb_[0] = 0;
  size_t used = 0;
  for (uint8_t i = 0; i < depth_; i++) {
    const char *n = (i == 0) ? osName : stack_[i]->name;
    size_t left = sizeof(crumb_) - used - 1;
    int w = snprintf(crumb_ + used, left + 1, (i == 0) ? "%s" : ">%s", n);
    if (w < 0 || (size_t)w > left) break;
    used += (size_t)w;
  }
  Display.markDirty();
}

void LzOS::push(LzScreen *s) {
  if (depth_ >= LZ_MAX_STACK || !s) return;  // bounds check
  stack_[depth_++] = s;
  s->onEnter();
  rebuildCrumb();
}

void LzOS::pop() {
  if (depth_ <= 1) return;  // never pop the root
  stack_[--depth_]->onLeave();
  rebuildCrumb();
}

void LzOS::popToRoot() {
  while (depth_ > 1) stack_[--depth_]->onLeave();
  rebuildCrumb();
}

void LzOS::requestRedraw() { Display.markDirty(); }

bool LzOS::editAllowed() {
  if (!locked_) return true;
  Buzzer.play(SND_ERROR);
  Message.show("CONFIG LOCKED", "Unlock from Main Menu");
  return false;
}

void LzOS::setRunActive(bool a) {
  lzStoreRunGuard = a;  // spec 6.1: no flash writes during RUN MODE
}

bool LzOS::runActive() const { return lzStoreRunGuard; }

void LzOS::tick() {
  uint32_t now = millis();
  IWatchdog.reload();  // fed every loop — watchdog trips only on a real hang
  Buttons.poll(now);

  LzEvent ev;
  while (Buttons.next(ev)) {
    LzScreen *t = top();
    if (!t) break;
    if (ev.type == EV_PRESS) Buzzer.play(SND_CLICK);  // menu click feedback
    bool consumed = t->onEvent(ev);
    if (!consumed && ev.btn == BTN_BACK && ev.type == EV_PRESS && depth_ > 1)
      pop();
    Display.markDirty();  // input always may change the UI
  }

  if (top()) top()->onTick(now);

  Battery.tick(now);
  if (Battery.isLow()) {
    Leds.red(LZLED_BLINK);
    if (now - lastLowBeep_ > 8000) {  // low-battery alarm, rate-limited
      lastLowBeep_ = now;
      Buzzer.play(SND_LOWBATT);
    }
  }

  Buzzer.tick(now);
  Leds.tick(now);
  Display.render(now, top(), crumb_, Battery.percent());
}
