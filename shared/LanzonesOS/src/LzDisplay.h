// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// 128x64 SSD1306 layout engine implementing the spec 1.1 template:
//   row 1: breadcrumb (left) + battery % (right)   | divider
//   rows 2-5: body (4 lines, selected row inverted) | divider
//   row 6: context-sensitive hint bar
// Redraw is change-driven (markDirty) and capped at 20 Hz — never per-loop.
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "LzConfig.h"

class LzScreen;

class LzDisplay {
 public:
  void begin();
  void markDirty() { dirty_ = true; }
  // Renders iff dirty and the 20 Hz cap allows. Returns true if drawn.
  bool render(uint32_t now, LzScreen *top, const char *breadcrumb, int battPct);
  U8G2 &gfx() { return u8g2_; }

  // ---- body drawing helpers (4 rows, y-layout owned here) ----
  static const uint8_t BODY_ROWS = 4;
  void bodyRow(uint8_t row, const char *text, bool selected);
  void bodyRowValue(uint8_t row, const char *label, const char *value, bool selected);
  int rowTop(uint8_t row) const { return 12 + row * 10; }

  void fullScreenText(const char *l1, const char *l2, const char *l3, const char *l4);

 private:
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_{U8G2_R0, U8X8_PIN_NONE};
  bool dirty_ = true;
  uint32_t lastDraw_ = 0;
};
