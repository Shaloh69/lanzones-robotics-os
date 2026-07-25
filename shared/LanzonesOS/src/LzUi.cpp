// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
#include "LzUi.h"

LzNumEditor NumEditor;
LzEnumEditor EnumEditor;
LzNameEditor NameEditor;
LzConfirmScreen Confirm;
LzMessageScreen Message;
LzHelpViewScreen HelpView;

// ---------------- Menu ----------------
bool LzMenuScreen::onEvent(const LzEvent &ev) {
  if (ev.type != EV_PRESS && ev.type != EV_REPEAT) return false;
  if (ev.btn == BTN_UP) {
    sel_ = (sel_ == 0) ? (uint8_t)(count_ - 1) : (uint8_t)(sel_ - 1);
  } else if (ev.btn == BTN_DOWN) {
    sel_ = (uint8_t)((sel_ + 1) % count_);
  } else if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
    if (sel_ < count_ && items_[sel_].onSelect) items_[sel_].onSelect();
    return true;
  } else {
    return false;
  }
  // keep selection inside the 4-row window
  if (sel_ < top_) top_ = sel_;
  if (sel_ >= (uint8_t)(top_ + LzDisplay::BODY_ROWS))
    top_ = (uint8_t)(sel_ - LzDisplay::BODY_ROWS + 1);
  return true;
}

void LzMenuScreen::draw(U8G2 &g) {
  (void)g;
  for (uint8_t r = 0; r < LzDisplay::BODY_ROWS; r++) {
    uint8_t i = (uint8_t)(top_ + r);
    if (i >= count_) break;
    if (items_[i].valueText) {
      char v[16];
      items_[i].valueText(v, sizeof(v));
      Display.bodyRowValue(r, items_[i].label, v, i == sel_);
    } else {
      Display.bodyRow(r, items_[i].label, i == sel_);
    }
  }
}

// ---------------- Numeric editor ----------------
void LzNumEditor::openF(const char *label, float *v, float mn, float mx,
                        float step, uint8_t decimals, const char *unit,
                        void (*onTest)()) {
  if (!OS.editAllowed()) return;  // Lock Config enforcement
  label_ = label; unit_ = unit; onTest_ = onTest;
  isFloat_ = true; fp_ = v; fMin_ = mn; fMax_ = mx; fStep_ = step;
  fVal_ = *v; dec_ = decimals;
  OS.push(this);
}

void LzNumEditor::openI(const char *label, int16_t *v, int16_t mn, int16_t mx,
                        int16_t step, const char *unit, void (*onTest)()) {
  if (!OS.editAllowed()) return;
  label_ = label; unit_ = unit; onTest_ = onTest;
  isFloat_ = false; ip_ = v; iMin_ = mn; iMax_ = mx; iStep_ = step;
  iVal_ = *v; dec_ = 0;
  OS.push(this);
}

void LzNumEditor::adjust(int dir) {
  if (isFloat_) {
    fVal_ += fStep_ * dir;
    if (fVal_ < fMin_) fVal_ = fMin_;
    if (fVal_ > fMax_) fVal_ = fMax_;
  } else {
    int32_t nv = (int32_t)iVal_ + iStep_ * dir;
    if (nv < iMin_) nv = iMin_;
    if (nv > iMax_) nv = iMax_;
    iVal_ = (int16_t)nv;
  }
}

bool LzNumEditor::onEvent(const LzEvent &ev) {
  if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
      (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
    adjust(ev.btn == BTN_UP ? +1 : -1);
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {  // save
    if (isFloat_) *fp_ = fVal_; else *ip_ = iVal_;
    Buzzer.play(SND_CONFIRM);
    OS.pop();
    return true;
  }
  if (ev.btn == BTN_BACK && ev.type == EV_PRESS) {  // cancel
    OS.pop();
    return true;
  }
  if (ev.btn == BTN_START && ev.type == EV_PRESS && onTest_) {
    // "test-drive" — apply the WORKING value so the test uses it
    if (isFloat_) *fp_ = fVal_; else *ip_ = iVal_;
    onTest_();
    return true;
  }
  return false;
}

void LzNumEditor::draw(U8G2 &g) {
  Display.bodyRow(0, label_, false);
  char v[20];
  if (isFloat_)
    snprintf(v, sizeof(v), "%.*f%s", dec_, (double)fVal_, unit_ ? unit_ : "");
  else
    snprintf(v, sizeof(v), "%d%s", iVal_, unit_ ? unit_ : "");
  g.setFont(u8g2_font_9x15B_tr);
  g.drawStr((128 - g.getStrWidth(v)) / 2, Display.rowTop(2) + 6, v);
  char rng[26];
  if (isFloat_)
    snprintf(rng, sizeof(rng), "min %.*f  max %.*f", dec_, (double)fMin_, dec_,
             (double)fMax_);
  else
    snprintf(rng, sizeof(rng), "min %d  max %d", iMin_, iMax_);
  Display.bodyRow(3, rng, false);
}

const char *LzNumEditor::hint() {
  return onTest_ ? "UD:+- SEL:Save ST:Test" : "UD:+- SEL:Save BK:Cancel";
}

// ---------------- Enum editor ----------------
void LzEnumEditor::open(const char *label, const char *const *labels,
                        uint8_t count, uint8_t *target,
                        void (*onDone)(bool)) {
  if (!OS.editAllowed()) return;
  label_ = label; labels_ = labels; count_ = count; target_ = target;
  val_ = (*target < count) ? *target : 0;
  onDone_ = onDone;
  OS.push(this);
}

bool LzEnumEditor::onEvent(const LzEvent &ev) {
  if (ev.type != EV_PRESS && ev.type != EV_REPEAT) return false;
  if (ev.btn == BTN_UP) {
    val_ = (val_ == 0) ? (uint8_t)(count_ - 1) : (uint8_t)(val_ - 1);
    return true;
  }
  if (ev.btn == BTN_DOWN) {
    val_ = (uint8_t)((val_ + 1) % count_);
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
    *target_ = val_;
    Buzzer.play(SND_CONFIRM);
    OS.pop();
    if (onDone_) onDone_(true);
    return true;
  }
  if (ev.btn == BTN_BACK && ev.type == EV_PRESS) {
    OS.pop();
    if (onDone_) onDone_(false);
    return true;
  }
  return false;
}

void LzEnumEditor::draw(U8G2 &g) {
  Display.bodyRow(0, label_, false);
  char v[24];
  snprintf(v, sizeof(v), "< %s >", labels_[val_]);
  g.setFont(u8g2_font_7x13B_tr);
  g.drawStr((128 - g.getStrWidth(v)) / 2, Display.rowTop(2) + 6, v);
}

// ---------------- Name editor ----------------
static const char NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";

void LzNameEditor::open(const char *title, char *target,
                        void (*onDone)(bool)) {
  title_ = title; target_ = target; onDone_ = onDone; pos_ = 0;
  memset(work_, ' ', LZ_NAME_LEN - 1);
  work_[LZ_NAME_LEN - 1] = 0;
  size_t n = strlen(target);
  if (n > LZ_NAME_LEN - 1) n = LZ_NAME_LEN - 1;
  memcpy(work_, target, n);
  OS.push(this);
}

bool LzNameEditor::onEvent(const LzEvent &ev) {
  if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
      (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
    const char *found = strchr(NAME_CHARS, work_[pos_]);
    int idx = found ? (int)(found - NAME_CHARS) : 0;
    int n = (int)sizeof(NAME_CHARS) - 1;
    idx = (ev.btn == BTN_UP) ? (idx + 1) % n : (idx + n - 1) % n;
    work_[pos_] = NAME_CHARS[idx];
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
    if (pos_ < LZ_NAME_LEN - 2) pos_++;
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_HOLD) {  // finish
    // trim trailing spaces
    char out[LZ_NAME_LEN];
    memcpy(out, work_, LZ_NAME_LEN);
    for (int i = LZ_NAME_LEN - 2; i >= 0 && out[i] == ' '; i--) out[i] = 0;
    if (out[0] == 0) {
      Buzzer.play(SND_ERROR);  // empty name rejected
      return true;
    }
    memcpy(target_, out, LZ_NAME_LEN);
    Buzzer.play(SND_CONFIRM);
    OS.pop();
    if (onDone_) onDone_(true);
    return true;
  }
  if (ev.btn == BTN_BACK && ev.type == EV_PRESS) {
    if (pos_ > 0) {
      pos_--;
      return true;
    }
    OS.pop();
    if (onDone_) onDone_(false);
    return true;
  }
  return false;
}

void LzNameEditor::draw(U8G2 &g) {
  Display.bodyRow(0, title_, false);
  g.setFont(u8g2_font_9x15B_tr);
  int x0 = (128 - 9 * (LZ_NAME_LEN - 1)) / 2;
  for (uint8_t i = 0; i < LZ_NAME_LEN - 1; i++) {
    char c[2] = {work_[i], 0};
    g.drawStr(x0 + i * 9, Display.rowTop(2) + 6, c);
    if (i == pos_) g.drawHLine(x0 + i * 9, Display.rowTop(2) + 8, 8);
  }
}

// ---------------- Hold-to-confirm ----------------
void LzConfirmScreen::open(const char *what, void (*onConfirm)()) {
  snprintf(what_, sizeof(what_), "%s", what);
  onConfirm_ = onConfirm;
  OS.push(this);
}

bool LzConfirmScreen::onEvent(const LzEvent &ev) {
  if (ev.btn == BTN_SELECT && ev.type == EV_HOLD) {
    void (*cb)() = onConfirm_;
    Buzzer.play(SND_CONFIRM);
    OS.pop();
    if (cb) cb();
    return true;
  }
  if (ev.btn == BTN_BACK && ev.type == EV_PRESS) {
    OS.pop();
    return true;
  }
  return ev.btn == BTN_SELECT;  // swallow SELECT press/release
}

void LzConfirmScreen::onTick(uint32_t now) {
  (void)now;
  if (Buttons.isDown(BTN_SELECT)) OS.requestRedraw();  // animate progress
}

void LzConfirmScreen::draw(U8G2 &g) {
  Display.bodyRow(0, "Hold to confirm:", false);
  Display.bodyRow(1, what_, false);
  uint32_t held = Buttons.heldFor(BTN_SELECT, millis());
  if (held > LZ_HOLD_MS) held = LZ_HOLD_MS;
  g.drawFrame(4, Display.rowTop(3), 120, 8);
  g.drawBox(4, Display.rowTop(3), (int)(120 * held / LZ_HOLD_MS), 8);
}

// ---------------- Message ----------------
void LzMessageScreen::show(const char *l1, const char *l2) {
  snprintf(l1_, sizeof(l1_), "%s", l1 ? l1 : "");
  snprintf(l2_, sizeof(l2_), "%s", l2 ? l2 : "");
  if (OS.top() != this) OS.push(this);
  OS.requestRedraw();
}

bool LzMessageScreen::onEvent(const LzEvent &ev) {
  if (ev.type == EV_PRESS) {
    OS.pop();
    return true;
  }
  return false;
}

void LzMessageScreen::draw(U8G2 &g) {
  (void)g;
  Display.bodyRow(1, l1_, false);
  if (l2_[0]) Display.bodyRow(2, l2_, false);
}

// ---------------- Named-slot list ----------------
LzSlotListScreen *LzSlotListScreen::active_ = nullptr;
uint8_t LzSlotListScreen::pendingDel_ = 0;

void LzSlotListScreen::onEnter() {
  uint8_t n = ops_->count();
  if (sel_ > n) sel_ = n;  // n == index of "+ New" row
  if (top_ > sel_) top_ = sel_;
}

void LzSlotListScreen::doDelete() {
  if (active_ && active_->ops_->del) active_->ops_->del(pendingDel_);
  if (active_) active_->onEnter();  // re-clamp selection
}

bool LzSlotListScreen::onEvent(const LzEvent &ev) {
  uint8_t n = ops_->count();          // entries
  uint8_t rows = (uint8_t)(n + 1);    // + "New" row
  if ((ev.btn == BTN_UP || ev.btn == BTN_DOWN) &&
      (ev.type == EV_PRESS || ev.type == EV_REPEAT)) {
    if (ev.btn == BTN_UP)
      sel_ = (sel_ == 0) ? (uint8_t)(rows - 1) : (uint8_t)(sel_ - 1);
    else
      sel_ = (uint8_t)((sel_ + 1) % rows);
    if (sel_ < top_) top_ = sel_;
    if (sel_ >= (uint8_t)(top_ + LzDisplay::BODY_ROWS))
      top_ = (uint8_t)(sel_ - LzDisplay::BODY_ROWS + 1);
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
    if (sel_ < n) {
      ops_->open(sel_);
    } else {
      if (!OS.editAllowed()) return true;
      if (!ops_->create()) {
        Buzzer.play(SND_ERROR);
        Message.show(ops_->fullMsg, "Delete one first");
      }
    }
    return true;
  }
  if (ev.btn == BTN_SELECT && ev.type == EV_HOLD && sel_ < n) {
    if (!OS.editAllowed()) return true;
    active_ = this;
    pendingDel_ = sel_;
    char nm[LZ_NAME_LEN], msg[26];
    ops_->nameOf(sel_, nm, sizeof(nm));
    snprintf(msg, sizeof(msg), "Delete %s", nm);
    Confirm.open(msg, &LzSlotListScreen::doDelete);
    return true;
  }
  return false;
}

void LzSlotListScreen::draw(U8G2 &g) {
  (void)g;
  uint8_t n = ops_->count();
  for (uint8_t r = 0; r < LzDisplay::BODY_ROWS; r++) {
    uint8_t i = (uint8_t)(top_ + r);
    if (i > n) break;
    if (i == n) {
      Display.bodyRow(r, ops_->newLabel, i == sel_);
    } else {
      char nm[LZ_NAME_LEN];
      ops_->nameOf(i, nm, sizeof(nm));
      Display.bodyRow(r, nm, i == sel_);
    }
  }
}

// ---------------- Help ----------------
void LzHelpViewScreen::open(const LzHelpEntry *e) {
  e_ = e;
  scroll_ = 0;
  lines_ = 1;
  for (const char *p = e->body; *p; p++)
    if (*p == '\n') lines_++;
  OS.push(this);
}

bool LzHelpViewScreen::onEvent(const LzEvent &ev) {
  if (ev.type != EV_PRESS && ev.type != EV_REPEAT) return false;
  if (ev.btn == BTN_UP && scroll_ > 0) {
    scroll_--;
    return true;
  }
  if (ev.btn == BTN_DOWN &&
      (uint8_t)(scroll_ + LzDisplay::BODY_ROWS) < lines_) {
    scroll_++;
    return true;
  }
  return false;
}

void LzHelpViewScreen::draw(U8G2 &g) {
  (void)g;
  const char *p = e_->body;
  for (uint8_t skip = 0; skip < scroll_ && *p; ) {
    if (*p++ == '\n') skip++;
  }
  for (uint8_t r = 0; r < LzDisplay::BODY_ROWS && *p; r++) {
    char line[26];
    uint8_t n = 0;
    while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++;
    line[n] = 0;
    if (*p == '\n') p++;
    Display.bodyRow(r, line, false);
  }
}

bool LzHelpIndexScreen::onEvent(const LzEvent &ev) {
  if (ev.type != EV_PRESS && ev.type != EV_REPEAT) return false;
  if (ev.btn == BTN_UP) {
    sel_ = (sel_ == 0) ? (uint8_t)(count_ - 1) : (uint8_t)(sel_ - 1);
  } else if (ev.btn == BTN_DOWN) {
    sel_ = (uint8_t)((sel_ + 1) % count_);
  } else if (ev.btn == BTN_SELECT && ev.type == EV_PRESS) {
    HelpView.open(&entries_[sel_]);
    return true;
  } else {
    return false;
  }
  if (sel_ < top_) top_ = sel_;
  if (sel_ >= (uint8_t)(top_ + LzDisplay::BODY_ROWS))
    top_ = (uint8_t)(sel_ - LzDisplay::BODY_ROWS + 1);
  return true;
}

void LzHelpIndexScreen::draw(U8G2 &g) {
  (void)g;
  // Entry 0 is the permanent branding entry (spec 8) — defined by each app
  // as the first LzHelpEntry, so it is always at the top of the index.
  for (uint8_t r = 0; r < LzDisplay::BODY_ROWS; r++) {
    uint8_t i = (uint8_t)(top_ + r);
    if (i >= count_) break;
    Display.bodyRow(r, entries_[i].title, i == sel_);
  }
}
