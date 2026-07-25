// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR hardware + engine.
//
// Real-time structure (spec 6.1):
//  - The 8-channel IR array is read through a CD4051 mux (updated spec
//    1.2): 3 select lines + 1 ADC read per channel in the main loop.
//  - A 200 Hz TIM5 ISR runs the line PID from a volatile control block.
//  - Per-junction speed/turn config (spec 3.2) drives approach/brake/
//    turn-style/post-turn/reacquisition behavior during SPEED RUN.
//  - Finish Marker Detection (spec 3.1): a perpendicular DOUBLE line
//    (two full-width bars in quick succession) auto-stops the robot;
//    a single full-width bar falls through to normal junction handling.
#include "vector_hw.h"

#include <HardwareTimer.h>
#include <LzOS.h>

#include "pin_config.h"
#include "vector_model.h"

LzMotor motorL, motorR;

int16_t irRaw[8] = {};
int16_t irNorm[8] = {};
bool irStuck[8] = {};

// ---- control block shared with the ISR ----
static struct {
  volatile bool pid = false;
  volatile int16_t l = 0, r = 0;
  volatile int16_t pos = 3500;
  volatile bool posValid = false;
  volatile uint8_t ovrPct = 0;  // per-junction approach/post-turn speed (0=off)
} ctl;
static volatile float pidErr = 0;
static HardwareTimer ctlTimer(TIM5);

static uint32_t pulseEnd = 0;
static bool pulsing = false;

// ---- engine state ----
static VecMode mode = VM_IDLE;
enum Maneuver : uint8_t { MV_NONE, MV_BRAKE, MV_TURN_L, MV_TURN_R, MV_TURN_U };
static Maneuver mv = MV_NONE;
static Maneuver pendingTurn = MV_NONE;  // after MV_BRAKE completes
static uint32_t mvT0 = 0;
static VecJcfg curJcfg;                 // config of the junction being taken
static uint32_t postUntil = 0;          // post-turn speed window
static uint32_t lostSince = 0;
static uint32_t junctionArmedAt = 0;
static uint32_t junctionSeenAt = 0;
static bool junctionPending = false;
static uint8_t junctions = 0;
static uint8_t speedIdx = 0;
static bool pathFull = false;
static char stopReason[20] = "";
static uint32_t calT0 = 0;
static int lastGoodPos = 3500;
// finish marker detector
static uint8_t barPulses = 0;
static uint32_t barFirstAt = 0;
static bool inBar = false;
static bool finishArmed = true;

// ---------------- 200 Hz PID ISR ----------------
static void controlIsr() {
  static float integ = 0, prevErr = 0, speedRamp = 0;
  if (!ctl.pid) {
    integ = 0;
    prevErr = 0;
    speedRamp = G.cur.baseSpeed;
    motorL.setPercent(ctl.l);
    motorR.setPercent(ctl.r);
    return;
  }
  float err = ctl.posValid ? (ctl.pos - 3500) / 100.0f : prevErr;
  integ += err * 0.005f;
  if (integ > 40) integ = 40;
  if (integ < -40) integ = -40;
  float d = (err - prevErr) * 200.0f;
  prevErr = err;
  float steer = G.cur.kp * err + G.cur.ki * integ + G.cur.kd * d * 0.01f;

  float target;
  if (ctl.ovrPct) {  // per-junction approach / post-turn override (spec 3.2)
    target = (float)ctl.ovrPct;
  } else {
    target = (fabsf(err) < 4.0f)
                 ? (float)G.cur.maxSpeed
                 : (float)G.cur.baseSpeed * (100 - G.cur.cornerCut) / 100.0f;
  }
  speedRamp += (target - speedRamp) * 0.02f;

  motorL.setPercent((int16_t)(speedRamp + steer));
  motorR.setPercent((int16_t)(speedRamp - steer));
  pidErr = err;
}

// ---------------- sensors: CD4051 mux (spec 1.2) ----------------
static void irUpdate() {
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(PIN_MUX_S0, (i & 1) ? HIGH : LOW);
    digitalWrite(PIN_MUX_S1, (i & 2) ? HIGH : LOW);
    digitalWrite(PIN_MUX_S2, (i & 4) ? HIGH : LOW);
    delayMicroseconds(3);  // mux settle: microseconds, not delay() ms
    irRaw[i] = (int16_t)analogRead(PIN_MUX_COM);
    int32_t span = G.cal.mx[i] - G.cal.mn[i];
    irStuck[i] = (span < 100) || irRaw[i] < 5 || irRaw[i] > 4090;
    int32_t v = span > 0 ? ((int32_t)(irRaw[i] - G.cal.mn[i]) * 1000) / span : 0;
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    irNorm[i] = (int16_t)(G.cur.lineMode == 0 ? 1000 - v : v);
  }
}

int linePosition() {
  int32_t wsum = 0, sum = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (irNorm[i] > 200) {
      wsum += (int32_t)irNorm[i] * i * 1000;
      sum += irNorm[i];
    }
  }
  if (sum == 0) return -1;
  return (int)(wsum / sum);
}

float lineError() { return pidErr; }

// ---------------- helpers ----------------
static void driveOpen(int16_t l, int16_t r) {
  noInterrupts();
  ctl.pid = false;
  ctl.l = l;
  ctl.r = r;
  interrupts();
}

static void drivePid(int pos, uint8_t ovr) {
  noInterrupts();
  ctl.pid = true;
  ctl.posValid = pos >= 0;
  if (pos >= 0) ctl.pos = (int16_t)pos;
  ctl.ovrPct = ovr;
  interrupts();
}

VecMode vecMode() { return mode; }
const char *vecStopReason() { return stopReason; }
uint8_t vecJunctionCount() { return junctions; }
bool vecPathFull() { return pathFull; }
uint8_t vecSpeedIdx() { return speedIdx; }

void vecStop(const char *reason) {
  mode = VM_IDLE;
  mv = MV_NONE;
  driveOpen(0, 0);
  OS.setRunActive(false);
  Leds.setState(LZLED_READY);
  snprintf(stopReason, sizeof(stopReason), "%s", reason);
}

void vecStart(VecMode m) {
  if (m == VM_IDLE) return;
  if (m == VM_SPEED && G.path.len == 0) {
    Buzzer.play(SND_ERROR);
    return;
  }
  mode = m;
  mv = MV_NONE;
  pendingTurn = MV_NONE;
  junctions = 0;
  speedIdx = 0;
  pathFull = false;
  lostSince = 0;
  postUntil = 0;
  junctionArmedAt = millis();
  junctionPending = false;
  barPulses = 0;
  inBar = false;
  finishArmed = true;
  stopReason[0] = 0;
  if (m == VM_LEARN) G.path = VecPath();  // fresh array incl. cfg defaults
  if (m == VM_AUTOCAL) {
    calT0 = millis();
    for (uint8_t i = 0; i < 8; i++) {
      G.cal.mn[i] = 4095;
      G.cal.mx[i] = 0;
    }
  }
  OS.setRunActive(true);
  Leds.setState(LZLED_ARMED);
  Buzzer.play(SND_MATCH_START);
}

// ---------------- motors public ----------------
void vectorHwBegin() {
  motorL.begin(LzMotor::HBRIDGE, PIN_MOTOR_L_PWM, PIN_MOTOR_L_DIR);
  motorR.begin(LzMotor::HBRIDGE, PIN_MOTOR_R_PWM, PIN_MOTOR_R_DIR);
  pinMode(PIN_MUX_S0, OUTPUT);
  pinMode(PIN_MUX_S1, OUTPUT);
  pinMode(PIN_MUX_S2, OUTPUT);
  pinMode(PIN_MUX_COM, INPUT_ANALOG);
  ctlTimer.setOverflow(200, HERTZ_FORMAT);
  ctlTimer.attachInterrupt(controlIsr);
  ctlTimer.resume();
}

void vectorMotorsReinit() {
  motorL.begin(LzMotor::HBRIDGE, PIN_MOTOR_L_PWM, PIN_MOTOR_L_DIR);
  motorR.begin(LzMotor::HBRIDGE, PIN_MOTOR_R_PWM, PIN_MOTOR_R_DIR);
}

void vectorMotorsStop() {
  pulsing = false;
  driveOpen(0, 0);
}

void vectorMotorsTestPulse(int16_t pctL, int16_t pctR, uint16_t ms) {
  if (mode != VM_IDLE) return;
  driveOpen(pctL, pctR);
  pulseEnd = millis() + ms;
  pulsing = true;
}

// ---------------- junction / finish detection ----------------
static bool armLeft() { return irNorm[0] > 500 || irNorm[1] > 500; }
static bool armRight() { return irNorm[6] > 500 || irNorm[7] > 500; }
static bool armStraight() { return irNorm[3] > 400 || irNorm[4] > 400; }
static bool fullBar() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < 8; i++)
    if (irNorm[i] > 600) n++;
  return n >= 7;
}

static void beginTurn(Maneuver m, uint32_t now, const VecJcfg *jc) {
  curJcfg = jc ? *jc : VecJcfg();
  if (curJcfg.brakeDs > 0 && m != MV_TURN_U) {  // decelerate first (spec 3.2)
    pendingTurn = m;
    mv = MV_BRAKE;
  } else {
    mv = m;
  }
  mvT0 = now;
}

static bool recordStep(uint8_t s) {
  if (G.path.len >= VEC_MAX_PATH) {
    pathFull = true;
    vecStop("PATH FULL (48)");
    Buzzer.play(SND_ERROR);
    return false;
  }
  G.path.steps[G.path.len] = s;
  G.path.cfg[G.path.len] = VecJcfg();  // defaults; user tunes later
  G.path.len++;
  return true;
}

static void handleJunction(uint32_t now) {
  junctions++;
  Buzzer.play(SND_CLICK);
  if (mode == VM_LEARN) {
    uint8_t s;
    if (armLeft()) s = STEP_L;
    else if (armStraight()) s = STEP_F;
    else s = STEP_R;
    if (!recordStep(s)) return;
    if (s == STEP_L) beginTurn(MV_TURN_L, now, nullptr);
    else if (s == STEP_R) beginTurn(MV_TURN_R, now, nullptr);
  } else if (mode == VM_SPEED) {
    if (speedIdx >= G.path.len) {  // bounds before array access
      vecStop("PATH DONE");
      Buzzer.play(SND_CONFIRM);
      return;
    }
    uint8_t idx = speedIdx++;
    uint8_t s = G.path.steps[idx];
    const VecJcfg *jc = &G.path.cfg[idx];
    if (s == STEP_L) beginTurn(MV_TURN_L, now, jc);
    else if (s == STEP_R) beginTurn(MV_TURN_R, now, jc);
    else if (s == STEP_U) beginTurn(MV_TURN_U, now, jc);
    else {  // STEP_F: straight through; still honor post-turn speed field
      curJcfg = *jc;
      if (curJcfg.postPct) postUntil = now + 600;
    }
  }
  junctionArmedAt = now + 250;
}

// current speed override for the PID (approach of next junction, or
// post-turn window) — 0 means "use global speed profile"
static uint8_t currentOverride(uint32_t now) {
  if (postUntil && now < postUntil && curJcfg.postPct) return curJcfg.postPct;
  if (postUntil && now >= postUntil) postUntil = 0;
  if (mode == VM_SPEED && speedIdx < G.path.len)
    return G.path.cfg[speedIdx].approachPct;  // segment INTO next junction
  return 0;
}

static void engineTick(uint32_t now) {
  if (mode == VM_IDLE) return;

  if (mode == VM_AUTOCAL) {
    for (uint8_t i = 0; i < 8; i++) {
      if (irRaw[i] < G.cal.mn[i]) G.cal.mn[i] = irRaw[i];
      if (irRaw[i] > G.cal.mx[i]) G.cal.mx[i] = irRaw[i];
    }
    uint32_t e = now - calT0;
    bool left = ((e / 750) & 1) == 0;
    driveOpen(left ? -25 : 25, left ? 25 : -25);
    if (e >= 3000) {
      vecStop("CAL DONE");
      Buzzer.play(SND_CONFIRM);
    }
    return;
  }

  int pos = linePosition();
  if (pos >= 0) lastGoodPos = pos;

  // --- Finish Marker Detection (spec 3.1): double full-width bar.
  // Configurable per-profile (Double-Line / Disabled) and applies to
  // LEARN as well — a finish during the dry run ends it cleanly with the
  // recorded path kept, instead of logging junk junctions at the line.
  if (G.cur.finishMode == 0 &&
      (mode == VM_FOLLOW || mode == VM_SPEED || mode == VM_LEARN) &&
      mv == MV_NONE && finishArmed) {
    bool bar = fullBar();
    if (bar && !inBar) {
      inBar = true;
      if (barPulses == 0) {
        barPulses = 1;
        barFirstAt = now;
      } else if (now - barFirstAt <= 800) {  // second bar in time = finish
        vecStop(mode == VM_LEARN ? "LEARN FINISH" : "FINISH!");
        Buzzer.play(SND_CONFIRM);
        return;
      }
    } else if (!bar) {
      inBar = false;
    }
    if (barPulses && now - barFirstAt > 800) barPulses = 0;  // single bar
  }

  // --- brake sub-state (per-junction Brake Distance/Time, spec 3.2)
  if (mv == MV_BRAKE) {
    driveOpen((int16_t)(G.cur.baseSpeed / 2), (int16_t)(G.cur.baseSpeed / 2));
    if (now - mvT0 >= (uint32_t)curJcfg.brakeDs * 100) {
      mv = pendingTurn;
      pendingTurn = MV_NONE;
      mvT0 = now;
    }
    return;
  }

  // --- active turn maneuver
  if (mv != MV_NONE) {
    uint32_t el = now - mvT0;
    int16_t s = (int16_t)(G.cur.baseSpeed);
    bool arc = (curJcfg.turnStyle == 1) && mv != MV_TURN_U;  // Smooth-Arc
    if (mv == MV_TURN_L)
      arc ? driveOpen((int16_t)(s / 4), s) : driveOpen((int16_t)-s, s);
    else if (mv == MV_TURN_R)
      arc ? driveOpen(s, (int16_t)(s / 4)) : driveOpen(s, (int16_t)-s);
    else
      driveOpen((int16_t)-s, s);  // U-turn pivots
    uint16_t minMs = (mv == MV_TURN_U) ? 400 : 200;
    uint16_t maxMs = curJcfg.reacqMs
                         ? curJcfg.reacqMs
                         : ((mv == MV_TURN_U) ? 1500 : 900);
    bool reacquired = el > minMs && pos >= 0 && abs(pos - 3500) < 1200;
    if (reacquired) {
      mv = MV_NONE;
      if (curJcfg.postPct) postUntil = now + 600;  // post-turn speed window
      if (mode == VM_SPEED && speedIdx >= G.path.len) {
        vecStop("PATH DONE");
        Buzzer.play(SND_CONFIRM);
      }
    } else if (el > maxMs) {
      mv = MV_NONE;
      if (mode == VM_SPEED) {  // reacq timeout -> Lost Line error (spec 3.2)
        vecStop("LOST LINE");
        Buzzer.play(SND_ERROR);
      }
    }
    return;
  }

  // --- line lost handling
  if (pos < 0) {
    if (lostSince == 0) lostSince = now;
    if (now - lostSince > 120) {
      if (mode == VM_LEARN) {
        if (!recordStep(STEP_U)) return;
        junctions++;
        beginTurn(MV_TURN_U, now, nullptr);
        lostSince = 0;
        return;
      }
      if (now - lostSince > 600) {
        vecStop("LINE LOST");
        Buzzer.play(SND_ERROR);
        return;
      }
    }
    drivePid(-1, currentOverride(now));
    return;
  }
  lostSince = 0;

  // --- junction detection (suppressed while a finish candidate is pending)
  bool junctionNow = (armLeft() || armRight()) &&
                     (int32_t)(now - junctionArmedAt) >= 0 && barPulses == 0;
  if (junctionNow && !junctionPending) {
    junctionPending = true;
    junctionSeenAt = now;
  } else if (!junctionNow) {
    junctionPending = false;
  }
  if (junctionPending && now - junctionSeenAt >= 30) {
    junctionPending = false;
    handleJunction(now);
    if (mode == VM_IDLE || mv != MV_NONE) return;
  }

  drivePid(pos, currentOverride(now));
}

void vectorHwTick(uint32_t now) {
  irUpdate();
  engineTick(now);
  if (pulsing && (int32_t)(now - pulseEnd) >= 0) vectorMotorsStop();
}
