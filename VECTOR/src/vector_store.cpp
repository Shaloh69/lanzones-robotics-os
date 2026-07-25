// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include <LzOS.h>
#include <LzStore.h>
#include <LzUi.h>

#include "vector_model.h"

VectorStore G;

const char *const STEP_NAMES[STEP_COUNT] = {"F", "L", "R", "U"};
const char *const TURN_STYLE_NAMES[3] = {"Default", "SmoothArc", "PointTurn"};

void vectorDefaults() {
  G = VectorStore();
  for (uint8_t i = 0; i < 8; i++) {  // sane pre-calibration span
    G.cal.mn[i] = 200;
    G.cal.mx[i] = 3800;
  }
}

bool vectorLoad() {
  if (LzStore::load(&G, sizeof(G), VECTOR_STORE_VERSION)) return true;
  vectorDefaults();
  return false;
}

bool vectorSaveAll() {
  return LzStore::save(&G, sizeof(G), VECTOR_STORE_VERSION);
}

void vectorSaveWithFeedback() {
  U8G2 &g = Display.gfx();
  g.clearBuffer();
  g.setFont(u8g2_font_7x13B_tr);
  g.drawStr((128 - g.getStrWidth("Saving...")) / 2, 36, "Saving...");
  g.sendBuffer();

  if (vectorSaveAll()) {
    Buzzer.play(SND_CONFIRM);
    Message.show("Saved to flash");
  } else {
    Buzzer.play(SND_ERROR);
    Message.show("Save FAILED",
                 OS.runActive() ? "Blocked in RUN MODE" : "Flash error");
  }
}
