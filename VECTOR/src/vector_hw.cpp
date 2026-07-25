// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// VECTOR hardware + engine.
//
// Real-time structure (spec 6.1):
//  - IR reads + junction logic run in vectorHwTick() (main loop),
//    non-blocking; results land in a volatile control block.
//  - A 200 Hz hardware-timer ISR (TIM5) runs the line PID from that block
//    and writes motor PWM — menus can never starve the control loop.
//  - Turn maneuvers are timed sub-states; junction decisions use the
//    left-hand rule in LEARN and the stored array in SPEED RUN.
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
  volatile bool pid = false;        // false -> open-loop l/r
  volatile int16_t l = 0, r = 0;
  volatile int16_t pos = 3500;      // 0..7000
  volatile bool posValid = false;
} ctl;
static volatile float pidErr = 0;   // exposed for the live screen
static HardwareTimer ctlTimer(TIM5);

static uint32_t pulseEnd = 0;
static bool pulsing = false;

// ---- engine state ----
static VecMode mode = VM_IDLE;
enum Maneuver : uint8_t { MV_NONE, MV_TURN_L, MV_TURN_R, MV_TURN_U };
static Maneuver mv = MV_NONE;
static uint32_t mvT0 = 0;
static uint32_t lostSince = 0;
static uint32_t junctionArmedAt = 0;   // re-arm timer between junctions
static uint32_t junctionSeenAt = 0;    // debounce timer
static bool junctionPending = false;
static uint8_t junctions = 0;
static uint8_t speedIdx = 0;
static bool pathFull = false;
static char stopReason[20] = "";
static uint32_t calT0 = 0;
static int lastGoodPos = 3500;

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
  float err = ctl.posValid ? (ctl.pos - 3500) / 100.0f  // -35..+35
                           : prevErr;                   // hold last on blip
  integ += err * 0.005f;
  if (integ > 40) integ = 40;
  if (integ < -40) integ = -40;
  float d = (err - prevErr) * 200.0f;
  prevErr = err;
  float steer = G.cur.kp * err + G.cur.ki * integ + G.cur.kd * d * 0.01f;

  // speed profile (spec 3.1): ramp to max on straights, cut in corners
  float target = (fabsf(err) < 4.0f)
                     ? (float)G.cur.maxSpeed
                     : (float)G.cur.baseSpeed * (100 - G.cur.cornerCut) / 100.0f;
  speedRamp += (target - speedRamp) * 0.02f;

  int16_t l = (int16_t)(speedRamp + steer);
  int16_t r = (int16_t)(speedRamp - steer);
  motorL.setPercent(l);
  motorR.setPercent(r);
  pidErr = err;
}

// ---------------- sensors ----------------
static void irUpdate() {
  for (uint8_t i = 0; i < 8; i++) {
    irRaw[i] = (int16_t)analogRead(PIN_IR[i]);
    int32_t span = G.cal.mx[i] - G.cal.mn[i];
    irStuck[i] = (span < 100) || irRaw[i] < 5 || irRaw[i] > 4090;
    int32_t v = span > 0 ? ((int32_t)(irRaw[i] - G.cal.mn[i]) * 1000) / span : 0;
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    // normalize so 1000 == strongly on the line, per line color mode
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
  noInterrupts();  // brief critical section (spec 6.2)
  ctl.pid = false;
  ctl.l = l;
  ctl.r = r;
  interrupts();
}

static void drivePid(int pos) {
  noInterrupts();
  ctl.pid = true;
  ctl.posValid = pos >= 0;
  if (pos >= 0) ctl.pos = (int16_t)pos;
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
  Leds.green(LZLED_ON);
  snprintf(stopReason, sizeof(stopReason), "%s", reason);
}

void vecStart(VecMode m) {
  if (m == VM_IDLE) return;
  if (m == VM_SPEED && G.path.len == 0) {
    Buzzer.play(SND_ERROR);
    return;  // no array to execute
  }
  mode = m;
  mv = MV_NONE;
  junctions = 0;
  speedIdx = 0;
  pathFull = false;
  lostSince = 0;
  junctionArmedAt = millis();
  junctionPending = false;
  stopReason[0] = 0;
  if (m == VM_LEARN) G.path.len = 0;  // dry run records a fresh array
  if (m == VM_AUTOCAL) {
    calT0 = millis();
    for (uint8_t i = 0; i < 8; i++) {  // start span collection fresh
      G.cal.mn[i] = 4095;
      G.cal.mx[i] = 0;
    }
  }
  OS.setRunActive(true);  // spec 6.1: no flash writes while driving
  Leds.green(LZLED_BLINK_FAST);
  Buzzer.play(SND_MATCH_START);
}

// ---------------- motors public ----------------
void vectorHwBegin() {
  motorL.begin(LzMotor::HBRIDGE, PIN_MOTOR_L_PWM, PIN_MOTOR_L_DIR);
  motorR.begin(LzMotor::HBRIDGE, PIN_MOTOR_R_PWM, PIN_MOTOR_R_DIR);
  for (uint8_t i = 0; i < 8; i++) pinMode(PIN_IR[i], INPUT_ANALOG);
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

// ---------------- junction detection & maneuvers ----------------
static bool armLeft() { return irNorm[0] > 500 || irNorm[1] > 500; }
static bool armRight() { return irNorm[6] > 500 || irNorm[7] > 500; }
static bool armStraight() { return irNorm[3] > 400 || irNorm[4] > 400; }

static void beginManeuver(Maneuver m, uint32_t now) {
  mv = m;
  mvT0 = now;
}

static bool recordStep(uint8_t s) {
  if (G.path.len >= VEC_MAX_PATH) {  // explicit bounds stop (spec 6.2)
    pathFull = true;
    vecStop("PATH FULL (48)");
    Buzzer.play(SND_ERROR);
    return false;
  }
  G.path.steps[G.path.len++] = s;
  return true;
}

static void handleJunction(uint32_t now) {
  junctions++;
  Buzzer.play(SND_CLICK);
  if (mode == VM_LEARN) {
    // left-hand rule: prefer Left, else Forward, else Right
    uint8_t s;
    if (armLeft()) s = STEP_L;
    else if (armStraight()) s = STEP_F;
    else s = STEP_R;
    if (!recordStep(s)) return;
    if (s == STEP_L) beginManeuver(MV_TURN_L, now);
    else if (s == STEP_R) beginManeuver(MV_TURN_R, now);
    // STEP_F: keep following straight through
  } else if (mode == VM_SPEED) {
    if (speedIdx >= G.path.len) {  // bounds check before array access
      vecStop("PATH DONE");
      Buzzer.play(SND_CONFIRM);
      return;
    }
    uint8_t s = G.path.steps[speedIdx++];
    if (s == STEP_L) beginManeuver(MV_TURN_L, now);
    else if (s == STEP_R) beginManeuver(MV_TURN_R, now);
    else if (s == STEP_U) beginManeuver(MV_TURN_U, now);
    if (speedIdx >= G.path.len) {
      // last instruction executed -> finish once the turn completes
    }
  }
  junctionArmedAt = now + 250;  // ignore re-triggers while crossing
}

static void engineTick(uint32_t now) {
  if (mode == VM_IDLE) return;

  if (mode == VM_AUTOCAL) {
    // sweep the array across the line for ~3 s, collecting min/max
    for (uint8_t i = 0; i < 8; i++) {
      if (irRaw[i] < G.cal.mn[i]) G.cal.mn[i] = irRaw[i];
      if (irRaw[i] > G.cal.mx[i]) G.cal.mx[i] = irRaw[i];
    }
    uint32_t e = now - calT0;
    bool left = ((e / 750) & 1) == 0;  // pivot back and forth
    driveOpen(left ? -25 : 25, left ? 25 : -25);
    if (e >= 3000) {
      vecStop("CAL DONE");
      Buzzer.play(SND_CONFIRM);
    }
    return;
  }

  int pos = linePosition();
  if (pos >= 0) lastGoodPos = pos;

  // --- active turn maneuver (timed pivot until the line is reacquired)
  if (mv != MV_NONE) {
    uint32_t el = now - mvT0;
    int16_t s = (int16_t)(G.cur.baseSpeed);
    if (mv == MV_TURN_L) driveOpen((int16_t)-s, s);
    else if (mv == MV_TURN_R) driveOpen(s, (int16_t)-s);
    else driveOpen((int16_t)-s, s);  // U-turn: pivot until line found
    uint16_t minMs = (mv == MV_TURN_U) ? 400 : 200;
    uint16_t maxMs = (mv == MV_TURN_U) ? 1500 : 900;
    if ((el > minMs && pos >= 0 && abs(pos - 3500) < 1200) || el > maxMs) {
      mv = MV_NONE;
      if (mode == VM_SPEED && speedIdx >= G.path.len) {
        vecStop("PATH DONE");
        Buzzer.play(SND_CONFIRM);
        return;
      }
    }
    return;
  }

  // --- line lost handling
  if (pos < 0) {
    if (lostSince == 0) lostSince = now;
    if (now - lostSince > 120) {
      if (mode == VM_LEARN) {  // dead end -> record U and turn around
        if (!recordStep(STEP_U)) return;
        junctions++;
        beginManeuver(MV_TURN_U, now);
        lostSince = 0;
        return;
      }
      if (now - lostSince > 600) {  // FOLLOW/SPEED: stop, don't wander
        vecStop("LINE LOST");
        Buzzer.play(SND_ERROR);
        return;
      }
    }
    drivePid(-1);  // hold last error while briefly lost
    return;
  }
  lostSince = 0;

  // --- junction detection with debounce + re-arm
  bool junctionNow =
      (armLeft() || armRight()) && (int32_t)(now - junctionArmedAt) >= 0;
  if (junctionNow && !junctionPending) {
    junctionPending = true;
    junctionSeenAt = now;
  } else if (!junctionNow) {
    junctionPending = false;
  }
  if (junctionPending && now - junctionSeenAt >= 30) {  // 30 ms debounce
    junctionPending = false;
    handleJunction(now);
    if (mode == VM_IDLE || mv != MV_NONE) return;
  }

  drivePid(pos);  // normal following
}

void vectorHwTick(uint32_t now) {
  irUpdate();  // fast: 8 analogRead ~ tens of microseconds
  engineTick(now);
  if (pulsing && (int32_t)(now - pulseEnd) >= 0) vectorMotorsStop();
}
