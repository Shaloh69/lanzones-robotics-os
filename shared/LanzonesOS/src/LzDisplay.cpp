// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzDisplay.h"
#include "LzScreen.h"

void LzDisplay::begin() {
  u8g2_.begin();
  u8g2_.setFontPosBaseline();
}

void LzDisplay::bodyRow(uint8_t row, const char *text, bool selected) {
  if (row >= BODY_ROWS) return;  // bounds check
  int top = rowTop(row);
  if (selected) {
    u8g2_.setDrawColor(1);
    u8g2_.drawBox(0, top, 128, 10);
    u8g2_.setDrawColor(0);
  } else {
    u8g2_.setDrawColor(1);
  }
  u8g2_.setFont(u8g2_font_6x10_tr);
  u8g2_.drawStr(2, top + 8, text);
  u8g2_.setDrawColor(1);
}

void LzDisplay::bodyRowValue(uint8_t row, const char *label, const char *value,
                             bool selected) {
  if (row >= BODY_ROWS) return;
  int top = rowTop(row);
  if (selected) {
    u8g2_.setDrawColor(1);
    u8g2_.drawBox(0, top, 128, 10);
    u8g2_.setDrawColor(0);
  } else {
    u8g2_.setDrawColor(1);
  }
  u8g2_.setFont(u8g2_font_6x10_tr);
  u8g2_.drawStr(2, top + 8, label);
  int w = u8g2_.getStrWidth(value);
  u8g2_.drawStr(126 - w, top + 8, value);
  u8g2_.setDrawColor(1);
}

void LzDisplay::fullScreenText(const char *l1, const char *l2, const char *l3,
                               const char *l4) {
  const char *ls[4] = {l1, l2, l3, l4};
  for (uint8_t i = 0; i < 4; i++)
    if (ls[i]) bodyRow(i, ls[i], false);
}

bool LzDisplay::render(uint32_t now, LzScreen *top, const char *breadcrumb,
                       int battPct) {
  if (!dirty_ || (now - lastDraw_) < LZ_UI_REDRAW_MS || !top) return false;
  dirty_ = false;
  lastDraw_ = now;

  u8g2_.clearBuffer();
  u8g2_.setDrawColor(1);

  // Row 1: breadcrumb left, battery % right (spec 1.1)
  u8g2_.setFont(u8g2_font_5x7_tr);
  char batt[6];
  if (battPct >= 0)
    snprintf(batt, sizeof(batt), "%d%%", battPct);
  else
    snprintf(batt, sizeof(batt), "--%%");
  int bw = u8g2_.getStrWidth(batt);
  u8g2_.drawStr(128 - bw, 7, batt);

  // Truncate breadcrumb from the LEFT so the current screen stays visible.
  int maxW = 128 - bw - 4;
  const char *bc = breadcrumb;
  while (*bc && u8g2_.getStrWidth(bc) > maxW) bc++;
  u8g2_.drawStr(0, 7, bc);

  u8g2_.drawHLine(0, 9, 128);   // top divider
  top->draw(u8g2_);             // body rows 2-5
  u8g2_.drawHLine(0, 53, 128);  // bottom divider

  // Row 6: hint bar
  u8g2_.setFont(u8g2_font_5x7_tr);
  u8g2_.drawStr(0, 63, top->hint());

  u8g2_.sendBuffer();
  return true;
}
