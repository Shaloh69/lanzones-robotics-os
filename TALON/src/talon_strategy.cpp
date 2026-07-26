// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Strategy Builder (spec 2.2 + advanced motion control addendum).
// Strategy List -> Strategy Editor (phases + give-up + Edge Escape +
// Match Timer/Boost + set-active) -> Phase Editor (type-dependent fields:
// radius/crawl/angle/ramp/sensor-ignore-window) -> Sensor multi-select.
// Edits work on a RAM working copy; "Save Strategy" batches ONE flash write.
#include <LzOS.h>
#include <LzUi.h>

#include "talon_model.h"

static int8_t editSlot = -1;
static Strategy work;
static bool dirty = false, backWarned = false;
static uint8_t phaseIdx = 0;
static float tmpSec = 0;
static int16_t tmpInt = 0;

static void markDirty(bool saved) { if (saved) dirty = true; }

// ---------------- Sensor Ignore multi-select (addendum 1.6) ----------------
class IgnoreSensorsScreen : public LzScreen {
 public:
  IgnoreSensorsScreen() : LzScreen("SENSORS") {}
  bool onEvent(const LzEvent &ev) override {
    if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
        (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
      if (ev.btn == BTN_UP)
        sel_ = (sel_ == 0) ? 7 : (uint8_t)(sel_ - 1);
      else
        sel_ = (uint8_t)((sel_ + 1) % 8);
      if (sel_ < top_) top_ = sel_;
      if (sel_ >= (uint8_t)(top_ + 4)) top_ = (uint8_t)(sel_ - 3);
      return true;
    }
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
      work.phases[phaseIdx].ignoreMask ^= (uint8_t)(1 << sel_);
      dirty = true;
      return true;
    }
    return false;
  }
  void draw(U8G2 &g) override {
    (void)g;
    for (uint8_t r = 0; r < 4; r++) {
      uint8_t i = (uint8_t)(top_ + r);
      if (i >= 8) break;
      char b[26];
      snprintf(b, sizeof(b), "[%c] %s",
               (work.phases[phaseIdx].ignoreMask & (1 << i)) ? 'x' : ' ',
               IGN_SENSOR_NAMES[i]);
      Display.bodyRow(r, b, i == sel_);
    }
  }
  const char *hint() override { return "SEL:Toggle BK:Done"; }

 private:
  uint8_t sel_ = 0, top_ = 0;
};
static IgnoreSensorsScreen ignoreSensors;

// ---------------- Phase editor ----------------
// Dynamic row list: which fields exist depends on the phase type.
enum PhaseRow : uint8_t {
  R_TYPE, R_TRIG, R_DUR, R_RADIUS, R_CRAWL, R_ANGLE, R_DIR,
  R_RAMP, R_IGNMS, R_IGNSENS, R_UP, R_DOWN, R_INS, R_ROW_MAX
};

class PhaseEditScreen : public LzScreen {
 public:
  PhaseEditScreen() : LzScreen("PHASE") {}
  void onEnter() override { rebuild(); }
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "SEL:Edit Hold:Del BK:Back"; }
  void rebuild();

 private:
  uint8_t rows_[R_ROW_MAX];
  uint8_t nRows_ = 0;
  uint8_t sel_ = 0, top_ = 0;
};
static PhaseEditScreen phaseEdit;

void PhaseEditScreen::rebuild() {
  Phase &p = work.phases[phaseIdx];
  nRows_ = 0;
  rows_[nRows_++] = R_TYPE;
  rows_[nRows_++] = R_TRIG;
  rows_[nRows_++] = R_DUR;
  if (phaseIsSpinSweep(p.type)) rows_[nRows_++] = R_RADIUS;
  if (p.type == PH_SEARCH_CRAWL) rows_[nRows_++] = R_CRAWL;
  if (p.type == PH_ANGLED_TURN) {
    rows_[nRows_++] = R_ANGLE;
    rows_[nRows_++] = R_DIR;
  }
  if (phaseIsAttack(p.type) || phaseIsRetreat(p.type)) rows_[nRows_++] = R_RAMP;
  rows_[nRows_++] = R_IGNMS;
  rows_[nRows_++] = R_IGNSENS;
  rows_[nRows_++] = R_UP;
  rows_[nRows_++] = R_DOWN;
  rows_[nRows_++] = R_INS;
  if (sel_ >= nRows_) sel_ = (uint8_t)(nRows_ - 1);
  if (top_ > sel_) top_ = sel_;
}

static void durDone(bool saved) {
  if (!saved) return;
  work.phases[phaseIdx].durDs = (uint16_t)(tmpSec * 10.0f + 0.5f);
  dirty = true;
}
static void rampDone(bool saved) {
  if (!saved) return;
  work.phases[phaseIdx].rampMs = (uint16_t)tmpInt;
  dirty = true;
}
static void ignMsDone(bool saved) {
  if (!saved) return;
  work.phases[phaseIdx].ignoreMs = (uint16_t)tmpInt;
  dirty = true;
}
static void radiusDone(bool saved) {
  if (!saved) return;
  work.phases[phaseIdx].searchRadius = (uint8_t)tmpInt;
  dirty = true;
}
static void crawlDone(bool saved) {
  if (!saved) return;
  work.phases[phaseIdx].crawlPct = (uint8_t)tmpInt;
  dirty = true;
}
static void typeDone(bool saved) {
  if (!saved) return;
  Phase &p = work.phases[phaseIdx];
  // presets (addendum 1.3): Spin = radius 0, Sweep = wide preset
  if (p.type == PH_SEARCH_SPIN) p.searchRadius = 0;
  if (p.type == PH_SEARCH_SWEEP && p.searchRadius == 0) p.searchRadius = 60;
  dirty = true;
  phaseEdit.rebuild();  // field rows depend on the type
  OS.requestRedraw();
}
static void dirDone(bool saved) { markDirty(saved); }

static void phaseDeleteConfirmed() {
  if (work.phaseCount <= 1) return;
  for (uint8_t i = phaseIdx; i + 1 < work.phaseCount; i++)
    work.phases[i] = work.phases[i + 1];
  work.phaseCount--;
  dirty = true;
  OS.pop();
}

static const char *const DIR_NAMES[2] = {"CCW", "CW"};

bool PhaseEditScreen::onEvent(const LzEvent &ev) {
  Phase &p = work.phases[phaseIdx];
  if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
      (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
    if (ev.btn == BTN_UP)
      sel_ = (sel_ == 0) ? (uint8_t)(nRows_ - 1) : (uint8_t)(sel_ - 1);
    else
      sel_ = (uint8_t)((sel_ + 1) % nRows_);
    if (sel_ < top_) top_ = sel_;
    if (sel_ >= (uint8_t)(top_ + 4)) top_ = (uint8_t)(sel_ - 3);
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_HOLD) {
    if (work.phaseCount <= 1) {
      Buzzer.play(SND_ERROR);
      Message.show("Cannot delete", "Strategy needs 1+ phase");
    } else {
      char m[26];
      snprintf(m, sizeof(m), "Delete phase %d", phaseIdx + 1);
      Confirm.open(m, phaseDeleteConfirmed);
    }
    return true;
  }
  if (ev.btn != BTN_SELECT || ev.type != EV_PRESS) return false;

  switch (rows_[sel_]) {
    case R_TYPE:
      EnumEditor.open("Phase type", PHASE_TYPE_NAMES, PH_TYPE_COUNT, &p.type,
                      typeDone);
      break;
    case R_TRIG:
      EnumEditor.open("Transition trigger", PHASE_TRIG_NAMES, TR_COUNT,
                      &p.trigger, markDirty);
      break;
    case R_DUR:
      tmpSec = p.durDs / 10.0f;
      NumEditor.openF("Duration", &tmpSec, 0.1f, 30.0f, 0.1f, 1, "s", nullptr,
                      durDone);
      break;
    case R_RADIUS:
      tmpInt = p.searchRadius;
      NumEditor.openI("Search radius", &tmpInt, 0, 100, 5, nullptr, nullptr,
                      radiusDone);
      break;
    case R_CRAWL:
      tmpInt = p.crawlPct;
      NumEditor.openI("Crawl speed", &tmpInt, 5, 60, 5, "%", nullptr,
                      crawlDone);
      break;
    case R_ANGLE:
      NumEditor.openI("Turn angle", &p.angleDeg, 15, 180, 15, " deg", nullptr,
                      markDirty);
      break;
    case R_DIR:
      EnumEditor.open("Turn direction", DIR_NAMES, 2, &p.angleCW, dirDone);
      break;
    case R_RAMP:
      tmpInt = (int16_t)p.rampMs;
      NumEditor.openI(phaseIsAttack(p.type) ? "Ramp-up time" : "Ramp-down time",
                      &tmpInt, 0, 2000, 50, "ms", nullptr, rampDone);
      break;
    case R_IGNMS:
      tmpInt = (int16_t)p.ignoreMs;
      NumEditor.openI("Ignore window", &tmpInt, 0, 2000, 50, "ms", nullptr,
                      ignMsDone);
      break;
    case R_IGNSENS:
      OS.push(&ignoreSensors);
      break;
    case R_UP:
      if (phaseIdx > 0) {
        Phase t = work.phases[phaseIdx - 1];
        work.phases[phaseIdx - 1] = work.phases[phaseIdx];
        work.phases[phaseIdx] = t;
        phaseIdx--;
        dirty = true;
        Buzzer.play(SND_CONFIRM);
      } else {
        Buzzer.play(SND_ERROR);
      }
      break;
    case R_DOWN:
      if ((uint8_t)(phaseIdx + 1) < work.phaseCount) {
        Phase t = work.phases[phaseIdx + 1];
        work.phases[phaseIdx + 1] = work.phases[phaseIdx];
        work.phases[phaseIdx] = t;
        phaseIdx++;
        dirty = true;
        Buzzer.play(SND_CONFIRM);
      } else {
        Buzzer.play(SND_ERROR);
      }
      break;
    case R_INS:
      if (work.phaseCount >= TALON_MAX_PHASES) {
        Buzzer.play(SND_ERROR);  // explicit bounds message (spec 6.2)
        Message.show("Phase list full", "Max 8 phases");
      } else {
        for (uint8_t i = work.phaseCount; i > phaseIdx + 1; i--)
          work.phases[i] = work.phases[i - 1];
        work.phases[phaseIdx + 1] = Phase();
        work.phaseCount++;
        phaseIdx++;
        rebuild();
        dirty = true;
        Buzzer.play(SND_CONFIRM);
      }
      break;
  }
  return true;
}

void PhaseEditScreen::draw(U8G2 &g) {
  (void)g;
  Phase &p = work.phases[phaseIdx];
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t i = (uint8_t)(top_ + r);
    if (i >= nRows_) break;
    char b[26];
    switch (rows_[i]) {
      case R_TYPE: snprintf(b, sizeof(b), "Type: %s", PHASE_TYPE_NAMES[p.type]); break;
      case R_TRIG: snprintf(b, sizeof(b), "Trigger: %s", PHASE_TRIG_NAMES[p.trigger]); break;
      case R_DUR:  snprintf(b, sizeof(b), "Duration: %d.%ds", p.durDs / 10, p.durDs % 10); break;
      case R_RADIUS: snprintf(b, sizeof(b), "Radius: %d", p.searchRadius); break;
      case R_CRAWL:  snprintf(b, sizeof(b), "Crawl spd: %d%%", p.crawlPct); break;
      case R_ANGLE:  snprintf(b, sizeof(b), "Angle: %d deg", p.angleDeg); break;
      case R_DIR:    snprintf(b, sizeof(b), "Direction: %s", DIR_NAMES[p.angleCW & 1]); break;
      case R_RAMP:
        snprintf(b, sizeof(b), "%s: %dms",
                 phaseIsAttack(p.type) ? "Ramp-up" : "Ramp-down", p.rampMs);
        break;
      case R_IGNMS: snprintf(b, sizeof(b), "Ignore win: %dms", p.ignoreMs); break;
      case R_IGNSENS: {
        uint8_t n = 0;
        for (uint8_t k = 0; k < 7; k++)
          if (p.ignoreMask & (1 << k)) n++;
        snprintf(b, sizeof(b), "Ignore sensors: %d", n);
        break;
      }
      case R_UP:   snprintf(b, sizeof(b), "Move earlier"); break;
      case R_DOWN: snprintf(b, sizeof(b), "Move later"); break;
      default:     snprintf(b, sizeof(b), "+ Insert phase after"); break;
    }
    Display.bodyRow(r, b, i == sel_);
  }
}

// ---------------- Strategy editor ----------------
// rows: phases[0..n-1], +Add, GiveUp, GURetreat, EdgeEscape, EscResume,
//       MatchDur, BoostThresh, BoostPhase, SetActive, Save
class StrategyEditScreen : public LzScreen {
 public:
  StrategyEditScreen() : LzScreen("EDIT") {}
  void onEnter() override { backWarned = false; }
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "SEL:Edit BK:Back"; }
  uint8_t sel = 0, top = 0;
  uint8_t rowCount() const { return (uint8_t)(work.phaseCount + 10); }
};
static StrategyEditScreen stratEdit;

static void giveUpDone(bool saved) {
  if (!saved) return;
  work.giveUpDs = (uint16_t)(tmpSec * 10.0f + 0.5f);
  dirty = true;
}
static void matchDurDone(bool saved) {
  if (!saved) return;
  work.matchDurS = (uint16_t)tmpInt;
  dirty = true;
}
static void boostThreshDone(bool saved) {
  if (!saved) return;
  work.boostThreshS = (uint16_t)tmpInt;
  dirty = true;
}
static void boostPhaseDone(bool saved) {
  if (!saved) return;
  work.boostPhase = (uint8_t)(tmpInt - 1);  // 1-based UI
  dirty = true;
}

bool StrategyEditScreen::onEvent(const LzEvent &ev) {
  uint8_t n = rowCount();
  if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
      (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
    if (ev.btn == BTN_UP)
      sel = (sel == 0) ? (uint8_t)(n - 1) : (uint8_t)(sel - 1);
    else
      sel = (uint8_t)((sel + 1) % n);
    if (sel < top) top = sel;
    if (sel >= (uint8_t)(top + 4)) top = (uint8_t)(sel - 3);
    return true;
  }
  if (ev.btn == BTN_BACK && ev.type == EV_PRESS) {
    if (dirty && !backWarned) {
      backWarned = true;
      Buzzer.play(SND_ERROR);
      Message.show("Unsaved changes!", "BACK again discards");
      return true;
    }
    dirty = false;
    return false;  // OS pops
  }
  if (ev.btn != BTN_SELECT || ev.type != EV_PRESS) return false;

  uint8_t k = sel;
  if (k < work.phaseCount) {
    phaseIdx = k;
    OS.push(&phaseEdit);
  } else if (k == work.phaseCount) {  // + Add Phase
    if (work.phaseCount >= TALON_MAX_PHASES) {
      Buzzer.play(SND_ERROR);
      Message.show("Phase list full", "Max 8 phases");
    } else {
      work.phases[work.phaseCount++] = Phase();
      dirty = true;
      Buzzer.play(SND_CONFIRM);
    }
  } else if (k == work.phaseCount + 1) {  // give-up timer
    tmpSec = work.giveUpDs / 10.0f;
    NumEditor.openF("Give-up timer", &tmpSec, 0.5f, 60.0f, 0.5f, 1, "s",
                    nullptr, giveUpDone);
  } else if (k == work.phaseCount + 2) {
    EnumEditor.open("Give-up retreat", RETREAT_NAMES, 3, &work.giveUpRetreat,
                    markDirty);
  } else if (k == work.phaseCount + 3) {  // Edge Escape maneuver
    EnumEditor.open("Escape maneuver", RETREAT_NAMES, 3, &work.escapeManeuver,
                    markDirty);
  } else if (k == work.phaseCount + 4) {  // resume behavior
    EnumEditor.open("Escape resume", RESUME_NAMES, 3, &work.escapeResume,
                    markDirty);
  } else if (k == work.phaseCount + 5) {  // match duration
    tmpInt = (int16_t)work.matchDurS;
    NumEditor.openI("Match duration", &tmpInt, 30, 600, 10, "s", nullptr,
                    matchDurDone);
  } else if (k == work.phaseCount + 6) {  // boost threshold
    tmpInt = (int16_t)work.boostThreshS;
    NumEditor.openI("Boost @ last", &tmpInt, 0, 120, 5, "s", nullptr,
                    boostThreshDone);
  } else if (k == work.phaseCount + 7) {  // boost target phase (1-based)
    tmpInt = (int16_t)(work.boostPhase + 1);
    NumEditor.openI("Boost phase #", &tmpInt, 1, work.phaseCount, 1, nullptr,
                    nullptr, boostPhaseDone);
  } else if (k == work.phaseCount + 8) {  // set active
    G.cur.activeStrategy = (uint8_t)editSlot;
    Buzzer.play(SND_CONFIRM);
    Message.show("Active strategy:", work.name);
  } else {  // save (single batched flash write)
    if (editSlot >= 0) {
      if (work.boostPhase >= work.phaseCount) work.boostPhase = 0;  // clamp
      G.strategies[editSlot] = work;
      talonSaveWithFeedback();
      dirty = false;
    }
  }
  return true;
}

void StrategyEditScreen::draw(U8G2 &g) {
  (void)g;
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t i = (uint8_t)(top + r);
    if (i >= rowCount()) break;
    char row[26];
    if (i < work.phaseCount) {
      Phase &p = work.phases[i];
      snprintf(row, sizeof(row), "P%d %s %d.%ds", i + 1,
               PHASE_TYPE_NAMES[p.type], p.durDs / 10, p.durDs % 10);
    } else if (i == work.phaseCount) {
      snprintf(row, sizeof(row), "+ Add Phase");
    } else if (i == work.phaseCount + 1) {
      snprintf(row, sizeof(row), "Give-up: %d.%ds", work.giveUpDs / 10,
               work.giveUpDs % 10);
    } else if (i == work.phaseCount + 2) {
      snprintf(row, sizeof(row), "GU Retreat: %s",
               RETREAT_NAMES[work.giveUpRetreat]);
    } else if (i == work.phaseCount + 3) {
      snprintf(row, sizeof(row), "EdgeEsc: %s",
               RETREAT_NAMES[work.escapeManeuver]);
    } else if (i == work.phaseCount + 4) {
      snprintf(row, sizeof(row), "EscResume: %s",
               RESUME_NAMES[work.escapeResume]);
    } else if (i == work.phaseCount + 5) {
      snprintf(row, sizeof(row), "Match: %d:%02d", work.matchDurS / 60,
               work.matchDurS % 60);
    } else if (i == work.phaseCount + 6) {
      if (work.boostThreshS)
        snprintf(row, sizeof(row), "Boost @ last %ds", work.boostThreshS);
      else
        snprintf(row, sizeof(row), "Boost: OFF");
    } else if (i == work.phaseCount + 7) {
      snprintf(row, sizeof(row), "Boost phase: P%d", work.boostPhase + 1);
    } else if (i == work.phaseCount + 8) {
      snprintf(row, sizeof(row), "Set As Active%s",
               (editSlot >= 0 && G.cur.activeStrategy == editSlot) ? " (now)"
                                                                   : "");
    } else {
      snprintf(row, sizeof(row), "SAVE STRATEGY%s", dirty ? " *" : "");
    }
    Display.bodyRow(r, row, i == sel);
  }
}

// ---------------- Strategy list (named slots) ----------------
static uint8_t stratCount() {
  uint8_t n = 0;
  for (auto &s : G.strategies)
    if (s.used) n++;
  return n;
}
static int stratSlot(uint8_t vis) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < TALON_MAX_STRATEGIES; i++)
    if (G.strategies[i].used && n++ == vis) return i;
  return -1;
}
static void stratName(uint8_t i, char *out, size_t n) {
  int s = stratSlot(i);
  if (s < 0) {
    snprintf(out, n, "?");
    return;
  }
  snprintf(out, n, "%s%s", G.strategies[s].name,
           (G.cur.activeStrategy == s) ? " *" : "");
}
static void stratOpen(uint8_t i) {
  int s = stratSlot(i);
  if (s < 0) return;
  if (!OS.editAllowed()) return;
  editSlot = (int8_t)s;
  work = G.strategies[s];
  dirty = false;
  stratEdit.sel = stratEdit.top = 0;
  OS.push(&stratEdit);
}
static char newStratName[LZ_NAME_LEN];
static void stratNameDone(bool ok) {
  if (!ok) return;
  for (uint8_t i = 0; i < TALON_MAX_STRATEGIES; i++) {
    if (!G.strategies[i].used) {
      G.strategies[i] = Strategy();
      G.strategies[i].used = 1;
      memcpy(G.strategies[i].name, newStratName, LZ_NAME_LEN);
      G.strategies[i].phaseCount = 1;
      editSlot = (int8_t)i;
      work = G.strategies[i];
      dirty = true;
      stratEdit.sel = stratEdit.top = 0;
      OS.push(&stratEdit);
      return;
    }
  }
}
static bool stratCreate() {
  if (stratCount() >= TALON_MAX_STRATEGIES) return false;
  newStratName[0] = 0;
  NameEditor.open("Strategy name:", newStratName, stratNameDone);
  return true;
}
static void stratDel(uint8_t i) {
  int s = stratSlot(i);
  if (s < 0) return;
  G.strategies[s].used = 0;
  if (G.cur.activeStrategy == s) G.cur.activeStrategy = 0;
  talonSaveWithFeedback();
}
static const LzSlotOps STRAT_OPS = {
    stratCount, stratName, stratOpen, stratCreate, stratDel,
    "+ New Strategy", "Strategy slots full"};
static LzSlotListScreen stratList("STRATEGY", &STRAT_OPS);

void openStrategyBuilder();
void openStrategyBuilder() { OS.push(&stratList); }
