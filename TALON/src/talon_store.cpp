// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include <LzOS.h>
#include <LzStore.h>
#include <LzUi.h>

#include "talon_model.h"

TalonStore G;

const char *const PHASE_TYPE_NAMES[PH_TYPE_COUNT] = {
    "S:Spin", "S:Sweep", "S:Charge",
    "A:Ram", "A:Curve", "A:SideSlam",
    "R:Bk+Turn", "R:Backup", "R:Center",
};
const char *const PHASE_TRIG_NAMES[TR_COUNT] = {"Time", "Opponent", "Edge"};
const char *const RETREAT_NAMES[3] = {"Bk+Turn", "Backup", "Center"};

void talonDefaults() {
  G = TalonStore();
  // ship one example strategy so RUN MODE works out of the box
  Strategy &s = G.strategies[0];
  snprintf(s.name, sizeof(s.name), "DEFAULT");
  s.used = 1;
  s.phaseCount = 3;
  s.phases[0] = {PH_SEARCH_SWEEP, TR_OPPONENT, 30};
  s.phases[1] = {PH_ATT_CURVE, TR_TIME, 20};
  s.phases[2] = {PH_ATT_RAM, TR_EDGE, 50};
  s.giveUpDs = 40;
  s.giveUpRetreat = 0;
}

bool talonLoad() {
  if (LzStore::load(&G, sizeof(G), TALON_STORE_VERSION)) return true;
  talonDefaults();
  return false;
}

bool talonSaveAll() { return LzStore::save(&G, sizeof(G), TALON_STORE_VERSION); }

void talonSaveWithFeedback() {
  // immediate feedback before the blocking erase (drawn directly, since the
  // normal render pass won't run until after the save returns)
  U8G2 &g = Display.gfx();
  g.clearBuffer();
  g.setFont(u8g2_font_7x13B_tr);
  g.drawStr((128 - g.getStrWidth("Saving...")) / 2, 36, "Saving...");
  g.sendBuffer();

  if (talonSaveAll()) {
    Buzzer.play(SND_CONFIRM);
    Message.show("Saved to flash");
  } else {
    Buzzer.play(SND_ERROR);
    Message.show("Save FAILED", OS.runActive() ? "Blocked in RUN MODE" : "Flash error");
  }
}
