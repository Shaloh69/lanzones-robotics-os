// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON hardware + RUN MODE engine (spec 2.x + advanced motion addendum).
//
// Real-time structure (spec 6.1):
//  - Sensor reads happen in talonHwTick() (main loop), non-blocking with
//    short timeouts (VL53L1X timeout 15 ms per updated spec 6.1).
//  - A 200 Hz TIM5 ISR computes steering PID from a volatile snapshot and
//    writes motor PWM; multi-field updates use brief critical sections.
//  - Edge sensors are DIGITAL inputs on the PCF8574 expander (updated spec
//    1.2), polled at 200 Hz on the shared I2C bus.
//
// Engine semantics decided with the team (2026-07-26):
//  - Edge Escape is the strategy-level edge interrupt; a phase whose OWN
//    trigger is Until-edge-detected keeps its trigger (advances normally)
//    and escape covers every other phase.
//  - Sensor Ignore Window filters TRIGGER evaluation only (opponent/edge
//    detection, give-up progress) — motion/aiming and Sensor Health always
//    use raw readings; time triggers unaffected (addendum 1.6).
#include "talon_hw.h"

#include <HardwareTimer.h>
#include <IWatchdog.h>
#include <LzOS.h>
#include <LzUi.h>
#include <VL53L1X.h>
#include <Wire.h>

#include "pin_config.h"
#include "talon_model.h"

LzMotor motorL, motorR;
LzEncoder encL, encR;
TofState tofState[5];
bool edgeL = false, edgeR = false;
bool expanderOk = false;
bool bumpContact = false;
bool expander2Ok = false;
ImuState imu;

static VL53L1X tof[5];
static uint32_t tofLastOk[5] = {};
static uint32_t lastImuMs = 0;
static uint32_t lastExpanderMs = 0;
static float accMag = 1.0f;
// PCF8574 /INT (spec 1.2): the ISR only raises a flag — the actual I2C
// read happens in the main loop (never I2C inside an interrupt).
static volatile bool expanderIntFlag = false;
static void expanderIntIsr() { expanderIntFlag = true; }

// ---- motor command shared with the 200 Hz control ISR ----
struct MotorCmd {
  volatile int16_t l = 0, r = 0;
  volatile bool steerPid = false;
  volatile int16_t base = 0;
  volatile int8_t dirIdx = 2;
  volatile bool valid = false;
  volatile uint8_t slipScalePct = 100;  // traction "Reduce Power" response
};
static MotorCmd cmd;
static HardwareTimer ctlTimer(TIM5);

static uint32_t pulseEnd = 0;
static bool pulsing = false;

// ---------------- PCF8574 expander (XSHUT P0-P4, edges P5/P6) ------------
static uint8_t expanderShadow = 0xFF;  // quasi-bidirectional: 1 = input/high

static bool expanderWrite(uint8_t val) {
  Wire.beginTransmission(TALON_EXPANDER_ADDR);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool expanderRead(uint8_t &val) {
  if (Wire.requestFrom((int)TALON_EXPANDER_ADDR, 1) != 1) return false;
  val = (uint8_t)Wire.read();
  return true;
}

// ---------------- PCF8574 #2: physical strategy-select switch -----------
static bool expander2Write(uint8_t val) {
  Wire.beginTransmission(TALON_EXPANDER2_ADDR);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool expander2Read(uint8_t &val) {
  if (Wire.requestFrom((int)TALON_EXPANDER2_ADDR, 1) != 1) return false;
  val = (uint8_t)Wire.read();
  return true;
}

// Polls a DIP/rotary switch on expander #2's P0-P2 (3-bit position, 0-7)
// and maps it directly to the Nth USED strategy slot. Debounced for
// mechanical switch bounce; only sampled while idle/post-match — a
// physical flick can never change the active strategy mid-match. Raw bits
// are read as the position value as-is; adjust the decode once the actual
// switch part (DIP vs. rotary, active-high vs. active-low) is chosen.
static void stratSwitchTick(uint32_t now) {
  if (!expander2Ok) return;
  RunState rs = runState();
  if (rs != RS_IDLE && rs != RS_POSTMATCH) return;  // never mid-match
  static uint32_t lastPoll = 0, candT0 = 0;
  static uint8_t candidate = 0xFF, stable = 0xFF;
  if (now - lastPoll < 100) return;  // 10 Hz — plenty for a mechanical switch
  lastPoll = now;
  uint8_t v;
  if (!expander2Read(v)) return;
  uint8_t pos = v & 0x07;
  if (pos != candidate) {
    candidate = pos;
    candT0 = now;
  } else if (now - candT0 >= 50 && pos != stable) {  // 50 ms debounce
    stable = pos;
    uint8_t n = 0;
    for (uint8_t i = 0; i < TALON_MAX_STRATEGIES; i++) {
      if (!G.strategies[i].used) continue;
      if (n == stable) {
        if (G.cur.activeStrategy != i) {
          G.cur.activeStrategy = i;  // RAM only — persists on next Save
          Buzzer.play(SND_CLICK);
          OS.requestRedraw();
        }
        break;
      }
      n++;
    }
  }
}

static void expanderSetXshut(uint8_t idx, bool high) {
  if (high)
    expanderShadow |= (uint8_t)(1 << idx);
  else
    expanderShadow &= (uint8_t)~(1 << idx);
  // keep P5-P7 high (inputs stay readable on a PCF8574 when written 1)
  expanderWrite((uint8_t)(expanderShadow | 0xE0));
}

// ---------------- sensors ----------------
static void tofInitAll() {
  uint8_t dummy;
  expanderOk = expanderWrite(0xE0) && expanderRead(dummy);  // all XSHUT low
  if (!expanderOk) {
    for (uint8_t i = 0; i < 5; i++) tofState[i].present = false;
    return;  // no expander: ToF + edge FAIL in Sensor Health, OS still runs
  }
  delay(10);  // boot-time only
  for (uint8_t i = 0; i < 5; i++) {
    IWatchdog.reload();
    expanderSetXshut(i, true);
    delay(10);
    tof[i].setTimeout(15);  // short timeout per updated spec 6.1
    if (tof[i].init()) {
      tof[i].setAddress(0x2A + i);
      tof[i].setDistanceMode(VL53L1X::Short);
      tof[i].setMeasurementTimingBudget(20000);
      tof[i].startContinuous(33);
      tofState[i].present = true;
    } else {
      tofState[i].present = false;
    }
  }
}

static bool imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(IMU_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static void imuInit() { imu.present = imuWrite(0x6B, 0x00); }

static void imuUpdate(uint32_t now) {
  if (!imu.present || now - lastImuMs < 20) return;  // 50 Hz
  lastImuMs = now;
  Wire.beginTransmission(IMU_I2C_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return;
  if (Wire.requestFrom((int)IMU_I2C_ADDR, 6) != 6) return;
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  float fx = ax / 16384.0f, fy = ay / 16384.0f, fz = az / 16384.0f;
  float horiz = sqrtf(fx * fx + fy * fy);
  imu.tiltDeg = atan2f(horiz, fz) * 57.2958f;
  imu.flipped = (fz < 0);
  float mag = sqrtf(fx * fx + fy * fy + fz * fz);
  accMag = accMag * 0.7f + mag * 0.3f;
  if (fabsf(mag - accMag) > G.cur.pushSense * 0.1f) imu.impact = true;
}

static void sensorsUpdate(uint32_t now) {
  for (uint8_t i = 0; i < 5; i++) {
    if (!tofState[i].present) {
      tofState[i].ok = false;
      continue;
    }
    if (tof[i].dataReady()) {
      uint16_t mm = tof[i].read(false);
      if (mm > 0 && !tof[i].timeoutOccurred()) {
        int32_t adj = (int32_t)mm + G.cur.tofZeroOffsetMm;
        tofState[i].mm = (uint16_t)((adj < 0) ? 0 : adj);
        tofLastOk[i] = now;
      }
    }
    tofState[i].ok = (now - tofLastOk[i]) < 300 && tofState[i].mm > 0;
  }
  // digital edge sensors via expander P5/P6: event-driven off /INT
  // (reading the port also clears the PCF8574's interrupt), plus a slow
  // 100 ms fallback read in case an INT edge was ever missed.
  if (expanderOk && (expanderIntFlag || now - lastExpanderMs >= 100)) {
    expanderIntFlag = false;
    lastExpanderMs = now;
    uint8_t v;
    if (expanderRead(v)) {
      bool rawL = (v & (1 << 5)) != 0;
      bool rawR = (v & (1 << 6)) != 0;
      // Edge Polarity setting (spec Edge Calibration): 0=Active-High
      edgeL = G.cur.edgePolarity ? !rawL : rawL;
      edgeR = G.cur.edgePolarity ? !rawR : rawR;
      // Bump/contact microswitch (this pass, P7): assumed a normally-open
      // switch shorting to GND on contact (active-low) — no separate
      // polarity setting exists for this yet; flip here if the physical
      // switch is wired the other way.
      bumpContact = (v & (1 << 7)) == 0;
    }
  }
  imuUpdate(now);
}

int opponentDistMm() {
  int best = -1;
  for (uint8_t i = 0; i < 5; i++)
    if (tofState[i].ok && (best < 0 || tofState[i].mm < best))
      best = tofState[i].mm;
  return best;
}

int opponentDirIdx() {
  int best = -1, idx = 2;
  for (uint8_t i = 0; i < 5; i++)
    if (tofState[i].ok && (best < 0 || tofState[i].mm < best)) {
      best = tofState[i].mm;
      idx = i;
    }
  return idx;
}

// masked variants — used ONLY for transition-trigger / give-up evaluation
static int opponentDistMasked(uint8_t mask) {
  int best = -1;
  for (uint8_t i = 0; i < 5; i++)
    if (tofState[i].ok && !(mask & TOF_IGN_BIT[i]) &&
        (best < 0 || tofState[i].mm < best))
      best = tofState[i].mm;
  return best;
}

// ---------------- 200 Hz control ISR ----------------
static void controlIsr() {
  static float integ = 0, prevErr = 0;
  int16_t l, r;
  if (cmd.steerPid && cmd.valid) {
    float err = (float)(cmd.dirIdx - 2);
    integ += err * 0.005f;
    if (integ > 50) integ = 50;
    if (integ < -50) integ = -50;
    float d = (err - prevErr) * 200.0f;
    prevErr = err;
    float steer = G.cur.kp * err * 10.0f + G.cur.ki * integ + G.cur.kd * d;
    if (steer > 60) steer = 60;
    if (steer < -60) steer = -60;
    l = (int16_t)(cmd.base + steer);
    r = (int16_t)(cmd.base - steer);
  } else {
    integ = 0;
    prevErr = 0;
    l = cmd.l;
    r = cmd.r;
  }
  uint8_t s = cmd.slipScalePct;  // traction "Reduce Power" (addendum 1.1)
  motorL.setPercent((int16_t)((int32_t)l * s / 100));
  motorR.setPercent((int16_t)((int32_t)r * s / 100));
}

// ---------------- motors ----------------
void talonHwBegin() {
  LzMotor::Mode m = G.cur.motorMode == 0 ? LzMotor::ESC : LzMotor::HBRIDGE;
  motorL.begin(m, PIN_MOTOR_L_PWM, PIN_MOTOR_L_DIR);
  motorR.begin(m, PIN_MOTOR_R_PWM, PIN_MOTOR_R_DIR);
  encL.begin(PIN_ENC_L_A, PIN_ENC_L_B);
  encR.begin(PIN_ENC_R_A, PIN_ENC_R_B);
  tofInitAll();
  expander2Ok = expander2Write(0xFF);  // all-input mode (strategy switch)
  // /INT is open-drain, active-low; a change on any expander input
  // asserts it until we read the port back.
  pinMode(PIN_EXPANDER_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_EXPANDER_INT), expanderIntIsr,
                  FALLING);
  imuInit();
  ctlTimer.setOverflow(200, HERTZ_FORMAT);
  ctlTimer.attachInterrupt(controlIsr);
  ctlTimer.resume();
}

void talonMotorsReinit() {
  LzMotor::Mode m = G.cur.motorMode == 0 ? LzMotor::ESC : LzMotor::HBRIDGE;
  motorL.begin(m, PIN_MOTOR_L_PWM, PIN_MOTOR_L_DIR);
  motorR.begin(m, PIN_MOTOR_R_PWM, PIN_MOTOR_R_DIR);
}

static void cmdOpenLoop(int16_t l, int16_t r) {
  noInterrupts();
  cmd.steerPid = false;
  cmd.l = l;
  cmd.r = r;
  interrupts();
}

void talonMotorsStop() {
  pulsing = false;
  cmdOpenLoop(0, 0);
}

void talonMotorsTestPulse(int16_t pctL, int16_t pctR, uint16_t ms) {
  if (runState() != RS_IDLE) return;
  cmdOpenLoop(pctL, pctR);
  pulseEnd = millis() + ms;
  pulsing = true;
}

// ---------------- RUN MODE engine ----------------
static RunState rs = RS_IDLE;
static uint32_t rsT0 = 0;
static uint32_t matchT0 = 0;
static uint8_t phase = 0;
static uint32_t phaseT0 = 0;
static uint32_t giveUpT0 = 0;
static int bestDist = -1;
static uint8_t cdBeeps = 0;
static char stopReason[20] = "";
static const char *actionName = "IDLE";
static bool boosted = false;
static bool angleDone = false;
// Edge Escape sub-state
static bool escaping = false;
static uint32_t escT0 = 0;
// give-up retreat sub-state
static bool givingUp = false;
static uint32_t giveT0v = 0;
// traction
static bool slipFlag = false;
static uint32_t slipHoldUntil = 0, lastTracMs = 0;
static uint8_t slipCount = 0;

static const uint16_t SPIN_MS_PER_DEG = 4;  // timed-turn rate: TUNE ON ROBOT

RunState runState() { return rs; }
uint8_t runPhaseIdx() { return phase; }
const char *runActionName() { return actionName; }
uint32_t runElapsedMs() { return rs == RS_RUNNING ? millis() - matchT0 : 0; }
const char *runStopReason() { return stopReason; }
bool runBoosted() { return boosted; }
bool tractionSlipActive() { return slipFlag; }
uint16_t runCountdownMs() {
  if (rs != RS_COUNTDOWN) return 0;
  uint32_t e = millis() - rsT0;
  return e >= 5000 ? 0 : (uint16_t)(5000 - e);
}

static Strategy &activeStrategy() {
  uint8_t i = G.cur.activeStrategy;
  if (i >= TALON_MAX_STRATEGIES || !G.strategies[i].used) i = 0;
  return G.strategies[i];
}

uint32_t runMatchRemainingMs() {
  if (rs != RS_RUNNING) return (uint32_t)activeStrategy().matchDurS * 1000;
  uint32_t dur = (uint32_t)activeStrategy().matchDurS * 1000;
  uint32_t e = millis() - matchT0;
  return e >= dur ? 0 : dur - e;
}

void runStart() {  // initial arm AND Quick Rematch: full state reset
  Strategy &s = activeStrategy();
  if (!s.used || s.phaseCount == 0) {
    Buzzer.play(SND_ERROR);
    return;
  }
  // Safety interlock (spec 2.1): never arm blind to the boundary — if the
  // edge sensors show FAIL in Sensor Health, refuse with an explanation.
  if (!expanderOk) {
    Buzzer.play(SND_ERROR);
    Leds.setState(LZLED_FAULT);
    Message.show("Cannot arm: edge", "sensors FAIL. Fix!");
    return;
  }
  rs = RS_COUNTDOWN;
  rsT0 = millis();
  cdBeeps = 0;
  stopReason[0] = 0;
  boosted = false;
  escaping = false;
  givingUp = false;
  slipFlag = false;
  cmd.slipScalePct = 100;
  OS.setRunActive(true);
  Leds.setState(LZLED_ARMED);
  actionName = "COUNTDOWN";
}

void runAbort(const char *reason) {
  rs = RS_IDLE;
  talonMotorsStop();
  OS.setRunActive(false);
  Leds.setState(LZLED_READY);
  snprintf(stopReason, sizeof(stopReason), "%s", reason);
  actionName = "IDLE";
}

static void postMatch(const char *reason) {
  rs = RS_POSTMATCH;
  talonMotorsStop();
  OS.setRunActive(false);
  Leds.setState(LZLED_POSTMATCH);
  snprintf(stopReason, sizeof(stopReason), "%s", reason);
  actionName = "POST-MATCH";
  Buzzer.play(SND_MATCH_START);
}

static void enterPhase(uint8_t idx, uint32_t now) {
  Strategy &s = activeStrategy();
  phase = (idx < s.phaseCount) ? idx : 0;
  phaseT0 = now;
  angleDone = false;
  imu.impact = false;
}

// active phase's ignore mask, valid only inside its window (addendum 1.6)
static uint8_t curIgnoreMask(uint32_t now) {
  Strategy &s = activeStrategy();
  Phase &p = s.phases[phase];
  return (p.ignoreMs && (now - phaseT0) < p.ignoreMs) ? p.ignoreMask : 0;
}

// shared retreat-style motion patterns (RETREAT phases, Edge Escape,
// give-up retreat). kind: 0=Backup+Turn 1=Backup-only 2=Center
static bool driveRetreatPattern(uint8_t kind, uint32_t el, float ramp) {
  int16_t fwd = (int16_t)(G.cur.maxFwdPct * ramp);
  int16_t rev = (int16_t)(-G.cur.maxRevPct * ramp);
  switch (kind) {
    case 0:  // Backup+Turn
      if (el < 600) cmdOpenLoop(rev, rev);
      else if (el < 1100) cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(-fwd / 2));
      else return true;
      break;
    case 1:  // Backup-only
      if (el < 800) cmdOpenLoop(rev, rev);
      else return true;
      break;
    default:  // Reposition-to-center
      if (el < 500) cmdOpenLoop(rev, rev);
      else if (el < 900) cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(-fwd / 2));
      else if (el < 1500) cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(fwd / 2));
      else return true;
      break;
  }
  return false;
}

static void drivePhase(uint32_t now) {
  Strategy &s = activeStrategy();
  Phase &p = s.phases[phase];
  uint32_t el = now - phaseT0;
  // ramp-up/ramp-down factor (addendum 1.2): 0 = instant
  float ramp = 1.0f;
  if (p.rampMs > 0 && (phaseIsAttack(p.type) || phaseIsRetreat(p.type))) {
    ramp = (float)el / p.rampMs;
    if (ramp > 1.0f) ramp = 1.0f;
  }
  int16_t fwd = (int16_t)(G.cur.maxFwdPct * ramp);
  int16_t rev = (int16_t)(-G.cur.maxRevPct * ramp);
  actionName = PHASE_TYPE_NAMES[p.type];

  switch (p.type) {
    case PH_SEARCH_SPIN:
    case PH_SEARCH_SWEEP: {
      // differential-speed search radius (addendum 1.3):
      // outer = s, inner = s*(2r-100)/100 -> r=0 pure spin, r=100 straight
      int16_t s0 = (int16_t)(G.cur.maxFwdPct / 2);
      int16_t inner = (int16_t)((int32_t)s0 * (2 * p.searchRadius - 100) / 100);
      bool swap = (p.type == PH_SEARCH_SWEEP) && ((el / 1000) & 1);
      cmdOpenLoop(swap ? inner : s0, swap ? s0 : inner);
      break;
    }
    case PH_SEARCH_CRAWL:  // addendum 1.4: own low speed
      cmdOpenLoop((int16_t)p.crawlPct, (int16_t)p.crawlPct);
      break;
    case PH_ANGLED_TURN: {  // addendum 1.5: timed pivot to target angle
      uint32_t target = (uint32_t)abs(p.angleDeg) * SPIN_MS_PER_DEG;
      if (!angleDone && el < target) {
        int16_t s0 = (int16_t)(G.cur.maxFwdPct / 2);
        if (p.angleCW) cmdOpenLoop(s0, (int16_t)-s0);
        else cmdOpenLoop((int16_t)-s0, s0);
      } else {
        angleDone = true;
        cmdOpenLoop(0, 0);  // hold; phase's trigger decides what's next
      }
      break;
    }
    case PH_SEARCH_CHARGE:
    case PH_ATT_RAM: {
      noInterrupts();
      cmd.steerPid = true;
      cmd.base = (p.type == PH_ATT_RAM) ? fwd : (int16_t)(fwd * 3 / 4);
      cmd.dirIdx = (int8_t)opponentDirIdx();  // motion uses raw sensors
      cmd.valid = opponentDistMm() > 0;
      interrupts();
      break;
    }
    case PH_ATT_CURVE: {
      int dir = opponentDirIdx();
      int16_t inner = (int16_t)(fwd / 2), outer = fwd;
      if (dir <= 1) cmdOpenLoop(inner, outer);
      else if (dir >= 3) cmdOpenLoop(outer, inner);
      else cmdOpenLoop(outer, (int16_t)(outer * 4 / 5));
      break;
    }
    case PH_ATT_SIDE: {
      if (el < (uint32_t)(p.durDs * 40))
        cmdOpenLoop(fwd, (int16_t)(fwd / 4));
      else
        cmdOpenLoop((int16_t)(fwd / 4), fwd);
      break;
    }
    case PH_RET_BACKTURN:
    case PH_RET_BACKONLY:
    case PH_RET_CENTER:
    default:
      (void)rev;
      driveRetreatPattern((uint8_t)(p.type - PH_RET_BACKTURN),
                          el % 1600, ramp);  // loop the pattern for duration
      break;
  }
}

// traction / wheel-slip detection (addendum 1.1). Inert without encoders.
static void tractionTick(uint32_t now) {
  if (!G.cur.tractionEnable || !imu.present ||
      (!encL.present() && !encR.present())) {
    slipFlag = false;
    return;
  }
  if (now - lastTracMs < 50) return;
  lastTracMs = now;
  int32_t wheel = labs(encL.takeDelta()) + labs(encR.takeDelta());
  int16_t cmdMag = (int16_t)(abs(cmd.l) + abs(cmd.r));
  // wheels turning fast + commanded hard + IMU sees no acceleration change
  bool slipNow = cmdMag > 80 && wheel > (int32_t)(10 * G.cur.slipSense) &&
                 fabsf(accMag - 1.0f) < 0.02f * G.cur.slipSense;
  slipCount = slipNow ? (uint8_t)(slipCount + 1) : 0;
  if (slipCount >= 3) {  // ~150 ms sustained
    slipFlag = true;
    switch (G.cur.slipResponse) {
      case 0:  // Reduce Power briefly, then reapply
        cmd.slipScalePct = 50;
        slipHoldUntil = now + 300;
        break;
      case 2:  // Trigger Retreat: trip the give-up timer immediately
        giveUpT0 = now - (uint32_t)activeStrategy().giveUpDs * 100 - 1;
        break;
      default:
        break;  // Alert Only: flag shows on Run screen / red LED
    }
    slipCount = 0;
  }
  if (slipHoldUntil && now > slipHoldUntil) {
    cmd.slipScalePct = 100;
    slipHoldUntil = 0;
  }
  if (slipFlag && !slipNow && slipCount == 0 && cmd.slipScalePct == 100)
    slipFlag = false;
}

static void engineTick(uint32_t now) {
  if (rs == RS_COUNTDOWN) {
    uint32_t e = now - rsT0;
    uint8_t sec = (uint8_t)(e / 1000);
    if (sec + 1 > cdBeeps && sec < 5) {
      cdBeeps = (uint8_t)(sec + 1);
      Buzzer.play(SND_CLICK);
      OS.requestRedraw();
    }
    if (e >= 5000) {
      Buzzer.play(SND_MATCH_START);
      rs = RS_RUNNING;
      matchT0 = now;
      giveUpT0 = now;
      bestDist = -1;
      enterPhase(0, now);
    }
    return;
  }
  if (rs != RS_RUNNING) return;

  Strategy &s = activeStrategy();
  Phase &p = s.phases[phase];
  uint8_t mask = curIgnoreMask(now);

  // --- IMU safety
  if (G.cur.autoStopFlip && imu.present &&
      (imu.flipped || imu.tiltDeg > G.cur.tiltLimitDeg)) {
    Buzzer.play(SND_ERROR);
    runAbort("FLIP AUTO-STOP");
    return;
  }

  // --- Match Timer (addendum 1.8): expiry -> post-match screen
  uint32_t matchDur = (uint32_t)s.matchDurS * 1000;
  uint32_t matchEl = now - matchT0;
  if (matchEl >= matchDur) {
    postMatch("TIME UP");
    return;
  }
  // Aggression Boost: no clear win by threshold -> jump to boost phase
  if (!boosted && s.boostThreshS > 0 &&
      (matchDur - matchEl) <= (uint32_t)s.boostThreshS * 1000) {
    boosted = true;
    if (s.boostPhase < s.phaseCount) {
      enterPhase(s.boostPhase, now);
      giveUpT0 = now;  // boost is a fresh commitment
      bestDist = -1;
      Buzzer.play(SND_LOWBATT);  // audible escalation cue
    }
  }

  tractionTick(now);

  // --- edge handling: masked edges are invisible to triggers AND escape
  bool eL = edgeL && !(mask & IGN_EDGE_L);
  bool eR = edgeR && !(mask & IGN_EDGE_R);
  if ((eL || eR) && !escaping && !givingUp) {
    if (p.trigger == TR_EDGE) {
      // phase explicitly listening for edge keeps its own trigger
      enterPhase((uint8_t)(phase + 1 >= s.phaseCount ? 0 : phase + 1), now);
    } else {
      escaping = true;  // Edge Escape interrupt (addendum 1.7)
      escT0 = now;
      Buzzer.play(SND_CLICK);
    }
  }
  if (escaping) {
    actionName = "EDGE-ESCAPE";
    if (driveRetreatPattern(s.escapeManeuver, now - escT0, 1.0f)) {
      escaping = false;
      switch (s.escapeResume) {  // resume behavior (addendum 1.7)
        case 1:  // resume next phase in sequence
          enterPhase((uint8_t)(phase + 1 >= s.phaseCount ? 0 : phase + 1), now);
          break;
        case 2: {  // fall back to first SEARCH phase
          uint8_t tgt = 0;
          for (uint8_t i = 0; i < s.phaseCount; i++)
            if (phaseIsSearch(s.phases[i].type)) {
              tgt = i;
              break;
            }
          enterPhase(tgt, now);
          break;
        }
        default:  // restart strategy
          enterPhase(0, now);
          break;
      }
      giveUpT0 = now;
    }
    return;  // escape overrides phase motion
  }

  // --- give-up safety timer (uses masked distance: trigger-class logic)
  int dist = opponentDistMasked(mask);
  bool bumpTrig = bumpContact && !(mask & IGN_CONTACT);
  if (dist > 0 && (bestDist < 0 || dist < bestDist - 50)) {
    bestDist = dist;
    giveUpT0 = now;
  }
  if (imu.impact && phaseIsAttack(p.type)) giveUpT0 = now;
  // physical contact is unambiguous progress — resets give-up same as impact
  if (bumpTrig && phaseIsAttack(p.type)) giveUpT0 = now;
  if (!givingUp && phaseIsAttack(p.type) &&
      (now - giveUpT0) > (uint32_t)s.giveUpDs * 100) {
    givingUp = true;
    giveT0v = now;
  }
  if (givingUp) {
    actionName = "GIVE-UP RETREAT";
    if (driveRetreatPattern(s.giveUpRetreat, now - giveT0v, 1.0f)) {
      givingUp = false;
      enterPhase(0, now);
      giveUpT0 = now;
      bestDist = -1;
    }
    return;
  }

  // --- normal phase transitions (masked sensors; time unaffected)
  bool advance = false;
  switch (p.trigger) {
    case TR_TIME:
      advance = (now - phaseT0) >= (uint32_t)p.durDs * 100;
      break;
    case TR_OPPONENT:
      advance = (dist > 0 && dist < 800);
      break;
    case TR_EDGE:
      break;  // handled above
    case TR_CONTACT:
      advance = bumpTrig;
      break;
  }
  // Angled Turn with a Time trigger: duration counts AFTER the turn finishes
  if (p.type == PH_ANGLED_TURN && p.trigger == TR_TIME && !angleDone)
    advance = false;
  if (advance)
    enterPhase((uint8_t)(phase + 1 >= s.phaseCount ? 0 : phase + 1), now);

  drivePhase(now);
}

void talonHwTick(uint32_t now) {
  sensorsUpdate(now);
  stratSwitchTick(now);
  engineTick(now);
  if (pulsing && (int32_t)(now - pulseEnd) >= 0) talonMotorsStop();
}
