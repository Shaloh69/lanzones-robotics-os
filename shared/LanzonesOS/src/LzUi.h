// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// UI toolkit built on the screen stack: menus, numeric/enum/name editors,
// hold-to-confirm, message overlay, and generic named-slot lists
// (Create New / Open / Delete — spec 1.1).
#pragma once
#include <Arduino.h>

#include "LzOS.h"

// ---------------- Menu ----------------
struct LzMenuItem {
  const char *label;
  void (*onSelect)();                       // required
  void (*valueText)(char *out, size_t n);   // optional right-aligned value
};

class LzMenuScreen : public LzScreen {
 public:
  LzMenuScreen(const char *name, const LzMenuItem *items, uint8_t count,
               const char *hintStr = nullptr)
      : LzScreen(name), items_(items), count_(count), hint_(hintStr) {}
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return hint_ ? hint_ : "UD:Nav SEL:Open BK:Back"; }
  uint8_t sel() const { return sel_; }

 private:
  const LzMenuItem *items_;
  uint8_t count_, sel_ = 0, top_ = 0;
  const char *hint_;
};

// ---------------- Numeric editor (float/int) ----------------
// UP/DOWN steps (auto fast-scroll on hold); SELECT saves; BACK cancels;
// optional START action ("test-drive" per spec 2.1).
class LzNumEditor : public LzScreen {
 public:
  LzNumEditor() : LzScreen("EDIT") {}
  void openF(const char *label, float *v, float mn, float mx, float step,
             uint8_t decimals, const char *unit = nullptr,
             void (*onTest)() = nullptr, void (*onDone)(bool saved) = nullptr);
  void openI(const char *label, int16_t *v, int16_t mn, int16_t mx,
             int16_t step, const char *unit = nullptr,
             void (*onTest)() = nullptr, void (*onDone)(bool saved) = nullptr);
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override;

 private:
  void adjust(int dir);
  const char *label_ = "";
  const char *unit_ = nullptr;
  void (*onTest_)() = nullptr;
  void (*onDone_)(bool) = nullptr;
  bool isFloat_ = true;
  float *fp_ = nullptr, fMin_ = 0, fMax_ = 0, fStep_ = 0, fVal_ = 0;
  int16_t *ip_ = nullptr, iMin_ = 0, iMax_ = 0, iStep_ = 0, iVal_ = 0;
  uint8_t dec_ = 1;
};

// ---------------- Enum editor ----------------
class LzEnumEditor : public LzScreen {
 public:
  LzEnumEditor() : LzScreen("EDIT") {}
  void open(const char *label, const char *const *labels, uint8_t count,
            uint8_t *target, void (*onDone)(bool saved) = nullptr);
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "UD:Change SEL:Save BK:Cancel"; }

 private:
  const char *label_ = "";
  const char *const *labels_ = nullptr;
  uint8_t count_ = 0, val_ = 0;
  uint8_t *target_ = nullptr;
  void (*onDone_)(bool) = nullptr;
};

// ---------------- Name editor (profile/strategy names) ----------------
class LzNameEditor : public LzScreen {
 public:
  LzNameEditor() : LzScreen("NAME") {}
  void open(const char *title, char *target, void (*onDone)(bool saved));
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "UD:Char SEL:Next Hold:OK"; }

 private:
  const char *title_ = "";
  char *target_ = nullptr;
  char work_[LZ_NAME_LEN];
  uint8_t pos_ = 0;
  void (*onDone_)(bool) = nullptr;
};

// ---------------- Hold-to-confirm (destructive actions, spec 1.0) --------
class LzConfirmScreen : public LzScreen {
 public:
  LzConfirmScreen() : LzScreen("CONFIRM") {}
  void open(const char *what, void (*onConfirm)());
  bool onEvent(const LzEvent &ev) override;
  void onTick(uint32_t now) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "Hold SEL:Confirm BK:Cancel"; }

 private:
  char what_[26] = "";
  void (*onConfirm_)() = nullptr;
};

// ---------------- Message overlay ----------------
class LzMessageScreen : public LzScreen {
 public:
  LzMessageScreen() : LzScreen("MSG") {}
  void show(const char *l1, const char *l2 = nullptr);
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "Any key: OK"; }

 private:
  char l1_[26] = "", l2_[26] = "";
};

// ---------------- Named-slot list (Profiles/Strategies/paths) ------------
struct LzSlotOps {
  uint8_t (*count)();
  void (*nameOf)(uint8_t i, char *out, size_t n);
  void (*open)(uint8_t i);
  bool (*create)();        // return false when list is full
  void (*del)(uint8_t i);
  const char *newLabel;    // e.g. "+ New Strategy"
  const char *fullMsg;     // e.g. "Strategy list full"
};

class LzSlotListScreen : public LzScreen {
 public:
  LzSlotListScreen(const char *name, const LzSlotOps *ops)
      : LzScreen(name), ops_(ops) {}
  void onEnter() override;
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "SEL:Open Hold SEL:Del"; }

 private:
  static void doDelete();
  static LzSlotListScreen *active_;
  static uint8_t pendingDel_;
  const LzSlotOps *ops_;
  uint8_t sel_ = 0, top_ = 0;
};

// ---------------- Help (scrollable reference, spec 4) ----------------
struct LzHelpEntry {
  const char *title;
  const char *body;  // pre-wrapped with '\n', <=21 chars per line
};

class LzHelpViewScreen : public LzScreen {
 public:
  LzHelpViewScreen() : LzScreen("VIEW") {}
  void open(const LzHelpEntry *e);
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "UD:Scroll BK:Index"; }

 private:
  const LzHelpEntry *e_ = nullptr;
  uint8_t scroll_ = 0, lines_ = 0;
};

class LzHelpIndexScreen : public LzScreen {
 public:
  LzHelpIndexScreen(const LzHelpEntry *entries, uint8_t count)
      : LzScreen("HELP"), entries_(entries), count_(count) {}
  bool onEvent(const LzEvent &ev) override;
  void draw(U8G2 &g) override;
  const char *hint() override { return "UD:Nav SEL:Read BK:Back"; }

 private:
  const LzHelpEntry *entries_;
  uint8_t count_, sel_ = 0, top_ = 0;
};

// Shared singletons (single-level modals; never nested with themselves)
extern LzNumEditor NumEditor;
extern LzEnumEditor EnumEditor;
extern LzNameEditor NameEditor;
extern LzConfirmScreen Confirm;
extern LzMessageScreen Message;
extern LzHelpViewScreen HelpView;
