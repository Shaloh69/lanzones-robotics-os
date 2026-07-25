// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// Path Array Editor (spec 3.2): Review/Edit junctions (inline F/L/R/U
// cycling exactly as specified), Insert/Delete junction with bounds checks,
// Learn Mode (dry run) screen. Speed Run lives on the RUN MODE screen.
#include <LzOS.h>
#include <LzUi.h>

#include "vector_hw.h"
#include "vector_model.h"

// ---------------- Per-junction speed/turn config (spec 3.2) ----------------
// Zero-valued fields fall back to the global Speed Profile defaults.
static uint8_t cfgIdx = 0;
static int16_t jTmp = 0;

static void jApproachDone(bool ok) {
  if (ok) G.path.cfg[cfgIdx].approachPct = (uint8_t)jTmp;
}
static void jBrakeDone(bool ok) {
  if (ok) G.path.cfg[cfgIdx].brakeDs = (uint8_t)jTmp;
}
static void jPostDone(bool ok) {
  if (ok) G.path.cfg[cfgIdx].postPct = (uint8_t)jTmp;
}
static void jReacqDone(bool ok) {
  if (ok) G.path.cfg[cfgIdx].reacqMs = (uint16_t)jTmp;
}

class JunctionCfgScreen : public LzScreen {
 public:
  JunctionCfgScreen() : LzScreen("JCFG") {}
  bool onEvent(const LzEvent &ev) override {
    if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
        (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
      if (ev.btn == BTN_UP)
        sel_ = (sel_ == 0) ? 4 : (uint8_t)(sel_ - 1);
      else
        sel_ = (uint8_t)((sel_ + 1) % 5);
      if (sel_ < top_) top_ = sel_;
      if (sel_ >= (uint8_t)(top_ + 4)) top_ = (uint8_t)(sel_ - 3);
      return true;
    }
    if (ev.btn != BTN_SELECT || ev.type != EV_PRESS) return false;
    VecJcfg &c = G.path.cfg[cfgIdx];
    switch (sel_) {
      case 0:
        jTmp = c.approachPct;
        NumEditor.openI("Approach (0=def)", &jTmp, 0, 100, 5, "%", nullptr,
                        jApproachDone);
        break;
      case 1:
        jTmp = c.brakeDs;
        NumEditor.openI("Brake x0.1s (0=off)", &jTmp, 0, 20, 1, nullptr,
                        nullptr, jBrakeDone);
        break;
      case 2:
        EnumEditor.open("Turn style", TURN_STYLE_NAMES, 3, &c.turnStyle);
        break;
      case 3:
        jTmp = c.postPct;
        NumEditor.openI("Post-turn (0=def)", &jTmp, 0, 100, 5, "%", nullptr,
                        jPostDone);
        break;
      default:
        jTmp = (int16_t)c.reacqMs;
        NumEditor.openI("Reacq timeout", &jTmp, 0, 3000, 100, "ms", nullptr,
                        jReacqDone);
        break;
    }
    return true;
  }
  void draw(U8G2 &g) override {
    (void)g;
    VecJcfg &c = G.path.cfg[cfgIdx];
    char rows[5][26];
    if (c.approachPct) snprintf(rows[0], 26, "Approach: %d%%", c.approachPct);
    else snprintf(rows[0], 26, "Approach: default");
    if (c.brakeDs) snprintf(rows[1], 26, "Brake: %d.%ds", c.brakeDs / 10, c.brakeDs % 10);
    else snprintf(rows[1], 26, "Brake: off");
    snprintf(rows[2], 26, "Turn: %s", TURN_STYLE_NAMES[c.turnStyle < 3 ? c.turnStyle : 0]);
    if (c.postPct) snprintf(rows[3], 26, "Post-turn: %d%%", c.postPct);
    else snprintf(rows[3], 26, "Post-turn: default");
    if (c.reacqMs) snprintf(rows[4], 26, "Reacq: %dms", c.reacqMs);
    else snprintf(rows[4], 26, "Reacq: default");
    for (uint8_t r = 0; r < 4; r++) {
      uint8_t i = (uint8_t)(top_ + r);
      if (i >= 5) break;
      Display.bodyRow(r, rows[i], i == sel_);
    }
  }
  const char *hint() override { return "SEL:Edit BK:Done"; }

 private:
  uint8_t sel_ = 0, top_ = 0;
};
static JunctionCfgScreen jcfgScreen;

// ---------------- Review / Edit Array ----------------
class PathEditScreen : public LzScreen {
 public:
  PathEditScreen() : LzScreen("ARRAY") {}
  void onEnter() override {
    if (sel_ > G.path.len) sel_ = G.path.len;
    editing_ = false;
  }
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override {
    return editing_ ? "UD:FLRU SEL:OK ST:Cfg"
                    : "SEL:Edit ST:Ins Hold:Del";
  }

 private:
  static void doDelete();
  static uint8_t pendingDel_;
  static PathEditScreen *active_;
  uint8_t sel_ = 0, top_ = 0;
  bool editing_ = false;
  uint8_t editVal_ = STEP_F;
};
static PathEditScreen pathEdit;
uint8_t PathEditScreen::pendingDel_ = 0;
PathEditScreen *PathEditScreen::active_ = nullptr;

void PathEditScreen::doDelete() {
  if (pendingDel_ >= G.path.len) return;  // bounds
  for (uint8_t i = pendingDel_; i + 1 < G.path.len; i++)
    G.path.steps[i] = G.path.steps[i + 1];
  G.path.len--;
  if (active_) active_->onEnter();
}

bool PathEditScreen::onEvent(const LzEvent &ev) {
  uint8_t rows = (uint8_t)(G.path.len + 1);  // + "Append" row
  if (editing_) {
    if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
        (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
      editVal_ = (uint8_t)((editVal_ + (ev.btn == BTN_UP ? 1 : STEP_COUNT - 1)) %
                           STEP_COUNT);  // UP/DOWN cycles F->L->R->U
      return true;
    }
    if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {  // confirm
      G.path.steps[sel_] = editVal_;
      editing_ = false;
      Buzzer.play(SND_CONFIRM);
      return true;
    }
    if (ev.btn == BTN_BACK && ev.type == EV_PRESS) {  // cancel edit
      editing_ = false;
      return true;
    }
    if (ev.btn == BTN_START && ev.type == EV_PRESS) {  // per-junction config
      cfgIdx = sel_;
      OS.push(&jcfgScreen);
      return true;
    }
    return true;  // swallow everything else while editing
  }

  if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
      (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
    if (ev.btn == BTN_UP)
      sel_ = (sel_ == 0) ? (uint8_t)(rows - 1) : (uint8_t)(sel_ - 1);
    else
      sel_ = (uint8_t)((sel_ + 1) % rows);
    if (sel_ < top_) top_ = sel_;
    if (sel_ >= (uint8_t)(top_ + 4)) top_ = (uint8_t)(sel_ - 3);
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
    if (!OS.editAllowed()) return true;
    if (sel_ < G.path.len) {  // enter inline edit on this junction
      editing_ = true;
      editVal_ = G.path.steps[sel_];
    } else {  // append
      if (G.path.len >= VEC_MAX_PATH) {
        Buzzer.play(SND_ERROR);
        Message.show("Path full", "Max 48 junctions");
      } else {
        G.path.steps[G.path.len++] = STEP_F;
        sel_ = (uint8_t)(G.path.len - 1);
        editing_ = true;
        editVal_ = STEP_F;
      }
    }
    return true;
  }
  if (ev.btn == BTN_START && ev.type == EV_PRESS && sel_ < G.path.len) {
    if (!OS.editAllowed()) return true;
    if (G.path.len >= VEC_MAX_PATH) {  // insert before selected (bounds)
      Buzzer.play(SND_ERROR);
      Message.show("Path full", "Max 48 junctions");
    } else {
      for (uint8_t i = G.path.len; i > sel_; i--)
        G.path.steps[i] = G.path.steps[i - 1];
      G.path.steps[sel_] = STEP_F;
      G.path.len++;
      editing_ = true;
      editVal_ = STEP_F;
      Buzzer.play(SND_CONFIRM);
    }
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_HOLD && sel_ < G.path.len) {
    if (!OS.editAllowed()) return true;
    pendingDel_ = sel_;
    active_ = this;
    char m[26];
    snprintf(m, sizeof(m), "Delete junction %d", sel_ + 1);
    Confirm.open(m, doDelete);
    return true;
  }
  return false;
}

void PathEditScreen::draw(U8G2 &g) {
  (void)g;
  if (G.path.len == 0 && sel_ == 0) {
    Display.bodyRow(0, "Path is empty.", false);
    Display.bodyRow(1, "Run Learn Mode or", false);
    Display.bodyRow(2, "SELECT to append.", false);
    Display.bodyRow(3, "+ Append junction", true);
    return;
  }
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t i = (uint8_t)(top_ + r);
    if (i > G.path.len) break;
    char b[26];
    if (i == G.path.len) {
      snprintf(b, sizeof(b), "+ Append junction");
    } else if (editing_ && i == sel_) {
      snprintf(b, sizeof(b), "Junction %d: <%s>", i + 1,
               STEP_NAMES[editVal_]);
    } else {
      // '+' marks junctions carrying speed/turn overrides (spec 3.2)
      VecJcfg &c = G.path.cfg[i];
      bool tuned = c.approachPct || c.brakeDs || c.turnStyle || c.postPct ||
                   c.reacqMs;
      snprintf(b, sizeof(b), "Junction %d: %s%s", i + 1,
               STEP_NAMES[G.path.steps[i]], tuned ? " +" : "");
    }
    Display.bodyRow(r, b, i == sel_);
  }
}

// ---------------- Learn Mode (dry run) ----------------
class LearnScreen : public LzScreen {
 public:
  LearnScreen() : LzScreen("LEARN") {}
  void onLeave() override {
    if (vecMode() == VM_LEARN) vecStop("EXIT");
  }
  bool onEvent(const LzEvent &ev) override {
    if (ev.btn == BTN_START && ev.type == EV_PRESS) {
      if (vecMode() == VM_IDLE) {
        if (OS.editAllowed()) vecStart(VM_LEARN);
      } else {
        vecStop("LEARN DONE");  // keep the recorded array
        Buzzer.play(SND_CONFIRM);
      }
      return true;
    }
    return false;
  }
  void onTick(uint32_t now) override {
    uint32_t s = ((uint32_t)vecMode() << 16) ^ ((uint32_t)G.path.len << 8) ^
                 vecJunctionCount() ^ ((now / 500) << 24);
    if (s != snap_) {
      snap_ = s;
      OS.requestRedraw();
    }
  }
  void draw(U8G2 &g) override {
    (void)g;
    char b[26];
    if (vecMode() == VM_LEARN) {
      Display.bodyRow(0, "LEARNING... START", false);
      Display.bodyRow(1, "stops + keeps path", false);
    } else {
      Display.bodyRow(0, "START: begin dry", false);
      Display.bodyRow(1, "run (records path)", false);
    }
    snprintf(b, sizeof(b), "Junctions: %d/%d", G.path.len, VEC_MAX_PATH);
    Display.bodyRow(2, b, false);
    // last few recorded steps as a quick preview
    char p[26] = "";
    uint8_t start = G.path.len > 7 ? (uint8_t)(G.path.len - 7) : 0;
    size_t used = 0;
    for (uint8_t i = start; i < G.path.len && used < sizeof(p) - 3; i++) {
      used += (size_t)snprintf(p + used, sizeof(p) - used, "%s,",
                               STEP_NAMES[G.path.steps[i]]);
    }
    snprintf(b, sizeof(b), "...%s", p);
    Display.bodyRow(3, G.path.len ? b : "(empty)", false);
  }
  const char *hint() override { return "ST:Start/Stop BK:Back"; }

 private:
  uint32_t snap_ = 0;
};
static LearnScreen learnScreen;

// ---------------- Path menu ----------------
static void openReview() { OS.push(&pathEdit); }
static void openLearn() { OS.push(&learnScreen); }
static void clearConfirmed() {
  G.path.len = 0;
  Buzzer.play(SND_CONFIRM);
}
static void clearArray() {
  if (OS.editAllowed()) Confirm.open("Clear whole path", clearConfirmed);
}
static void vLen(char *o, size_t n) { snprintf(o, n, "%d", G.path.len); }

static const LzMenuItem PATH_ITEMS[] = {
    {"Review/Edit Array", openReview, vLen},
    {"Learn Mode (dry)", openLearn, nullptr},
    {"Clear Array", clearArray, nullptr},
};
static LzMenuScreen pathMenu("PATH", PATH_ITEMS, 3);

void openPathEditor();  // strong definition overrides weak stub
void openPathEditor() { OS.push(&pathMenu); }
