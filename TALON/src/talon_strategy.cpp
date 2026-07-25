// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Strategy Builder (spec 2.2).
// Strategy List -> Strategy Editor (phases + give-up timer + set-active)
//               -> Phase Editor (type / trigger / duration / reorder /
//                  insert / delete, all bounds-checked with on-screen errors)
// Edits work on a RAM working copy; "Save Strategy" batches ONE flash write
// (spec 6.2). BACK with unsaved changes warns once, then discards.
#include <LzOS.h>
#include <LzUi.h>

#include "talon_model.h"

static int8_t editSlot = -1;      // storage slot being edited
static Strategy work;             // RAM working copy
static bool dirty = false, backWarned = false;
static uint8_t phaseIdx = 0;      // phase being edited in the phase editor
static float tmpSec = 0;          // seconds<->deciseconds editing shim

// ---------------- Phase editor ----------------
class PhaseEditScreen : public LzScreen {
 public:
  PhaseEditScreen() : LzScreen("PHASE") {}
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "SEL:Edit Hold:Del BK:Back"; }
  uint8_t sel = 0, top = 0;
  static const uint8_t ROWS = 6;  // type,trigger,duration,up,down,insert
};
static PhaseEditScreen phaseEdit;

static void durDone(bool saved) {
  if (!saved) return;
  work.phases[phaseIdx].durDs = (uint16_t)(tmpSec * 10.0f + 0.5f);
  dirty = true;
}
static void markDirty(bool saved) { if (saved) dirty = true; }

static void phaseDeleteConfirmed() {
  if (work.phaseCount <= 1) return;
  for (uint8_t i = phaseIdx; i + 1 < work.phaseCount; i++)
    work.phases[i] = work.phases[i + 1];
  work.phaseCount--;
  dirty = true;
  OS.pop();  // leave the phase editor — this phase no longer exists
}

bool PhaseEditScreen::onEvent(const LzEvent &ev) {
  Phase &p = work.phases[phaseIdx];
  if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
      (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
    if (ev.btn == BTN_UP)
      sel = (sel == 0) ? (uint8_t)(ROWS - 1) : (uint8_t)(sel - 1);
    else
      sel = (uint8_t)((sel + 1) % ROWS);
    if (sel < top) top = sel;
    if (sel >= (uint8_t)(top + 4)) top = (uint8_t)(sel - 3);
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_HOLD) {  // delete this phase
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
  if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
    switch (sel) {
      case 0:
        EnumEditor.open("Phase type", PHASE_TYPE_NAMES, PH_TYPE_COUNT, &p.type,
                        markDirty);
        break;
      case 1:
        EnumEditor.open("Transition trigger", PHASE_TRIG_NAMES, TR_COUNT,
                        &p.trigger, markDirty);
        break;
      case 2:
        tmpSec = p.durDs / 10.0f;
        NumEditor.openF("Duration", &tmpSec, 0.1f, 30.0f, 0.1f, 1, "s",
                        nullptr, durDone);
        break;
      case 3:  // move earlier
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
      case 4:  // move later
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
      case 5:  // insert a new phase after this one
        if (work.phaseCount >= TALON_MAX_PHASES) {
          Buzzer.play(SND_ERROR);  // explicit bounds message (spec 6.2)
          Message.show("Phase list full", "Max 8 phases");
        } else {
          for (uint8_t i = work.phaseCount; i > phaseIdx + 1; i--)
            work.phases[i] = work.phases[i - 1];
          work.phases[phaseIdx + 1] = Phase();
          work.phaseCount++;
          phaseIdx++;
          dirty = true;
          Buzzer.play(SND_CONFIRM);
        }
        break;
    }
    return true;
  }
  return false;
}

void PhaseEditScreen::draw(U8G2 &g) {
  (void)g;
  Phase &p = work.phases[phaseIdx];
  char rows[ROWS][26];
  snprintf(rows[0], 26, "Type: %s", PHASE_TYPE_NAMES[p.type]);
  snprintf(rows[1], 26, "Trigger: %s", PHASE_TRIG_NAMES[p.trigger]);
  snprintf(rows[2], 26, "Duration: %d.%ds", p.durDs / 10, p.durDs % 10);
  snprintf(rows[3], 26, "Move earlier");
  snprintf(rows[4], 26, "Move later");
  snprintf(rows[5], 26, "+ Insert phase after");
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t i = (uint8_t)(top + r);
    if (i >= ROWS) break;
    Display.bodyRow(r, rows[i], i == sel);
  }
}

// ---------------- Strategy editor ----------------
class StrategyEditScreen : public LzScreen {
 public:
  StrategyEditScreen() : LzScreen("EDIT") {}
  void onEnter() override { backWarned = false; }
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "SEL:Edit BK:Back"; }
  uint8_t sel = 0, top = 0;
  // rows: phases[0..n-1], +Add, GiveUp, GU-Retreat, SetActive, Save
  uint8_t rowCount() const { return (uint8_t)(work.phaseCount + 5); }
};
static StrategyEditScreen stratEdit;

static void giveUpDone(bool saved) {
  if (!saved) return;
  work.giveUpDs = (uint16_t)(tmpSec * 10.0f + 0.5f);
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
  if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
    if (sel < work.phaseCount) {  // open phase editor
      phaseIdx = sel;
      phaseEdit.sel = phaseEdit.top = 0;
      OS.push(&phaseEdit);
    } else if (sel == work.phaseCount) {  // + Add Phase (append)
      if (work.phaseCount >= TALON_MAX_PHASES) {
        Buzzer.play(SND_ERROR);
        Message.show("Phase list full", "Max 8 phases");
      } else {
        work.phases[work.phaseCount++] = Phase();
        dirty = true;
        Buzzer.play(SND_CONFIRM);
      }
    } else if (sel == work.phaseCount + 1) {  // give-up timer
      tmpSec = work.giveUpDs / 10.0f;
      NumEditor.openF("Give-up timer", &tmpSec, 0.5f, 60.0f, 0.5f, 1, "s",
                      nullptr, giveUpDone);
    } else if (sel == work.phaseCount + 2) {  // give-up retreat type
      EnumEditor.open("Give-up retreat", RETREAT_NAMES, 3, &work.giveUpRetreat,
                      markDirty);
    } else if (sel == work.phaseCount + 3) {  // set active
      G.cur.activeStrategy = (uint8_t)editSlot;
      Buzzer.play(SND_CONFIRM);
      Message.show("Active strategy:", work.name);
    } else {  // save strategy (single batched flash write)
      if (editSlot >= 0) {
        G.strategies[editSlot] = work;
        talonSaveWithFeedback();
        dirty = false;
      }
    }
    return true;
  }
  return false;
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
      snprintf(row, sizeof(row), "Give-up: %d.%ds%s", work.giveUpDs / 10,
               work.giveUpDs % 10, dirty ? " *" : "");
    } else if (i == work.phaseCount + 2) {
      snprintf(row, sizeof(row), "GU Retreat: %s",
               RETREAT_NAMES[work.giveUpRetreat]);
    } else if (i == work.phaseCount + 3) {
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
      G.strategies[i].phaseCount = 1;  // starts with one default phase
      editSlot = (int8_t)i;
      work = G.strategies[i];
      dirty = true;  // not yet in flash
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

void openStrategyBuilder();  // strong definition overrides the weak stub
void openStrategyBuilder() { OS.push(&stratList); }
