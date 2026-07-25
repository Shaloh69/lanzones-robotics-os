// Copyright (c) 2026 Team Lanzones. Partnered by Koogs Robotics. All rights reserved.
//
// TALON hardware + RUN MODE engine.
//
// Real-time structure (spec 6.1):
//  - Sensor reads happen in talonHwTick() (main loop), non-blocking with
//    freshness timeouts; results land in a volatile snapshot.
//  - A 200 Hz hardware-timer ISR (TIM5) computes steering PID from the
//    latest snapshot and writes the motor PWMs — menu activity can never
//    starve motor control.
//  - Shared ISR/main data uses volatile + brief critical sections (6.2).
#include "talon_hw.h"

#include <HardwareTimer.h>
#include <VL53L1X.h>
#include <Wire.h>
#include <IWatchdog.h>
#include <LzOS.h>

#include "pin_config.h"
#include "talon_model.h"

LzMotor motorL, motorR;
TofState tofState[5];
int16_t edgeValL = 0, edgeValR = 0;
bool edgeL = false, edgeR = false;
ImuState imu;

static VL53L1X tof[5];
static uint32_t tofLastOk[5] = {};
static uint32_t lastImuMs = 0;
static float accMag = 1.0f;

// ---- motor command shared with the 200 Hz control ISR ----
struct MotorCmd {
  volatile int16_t l = 0, r = 0;      // open-loop percentages
  volatile bool steerPid = false;     // steer toward opponent using PID
  volatile int16_t base = 0;          // base speed for PID steering
  volatile int8_t dirIdx = 2;         // latest opponent direction (0..4)
  volatile bool valid = false;        // opponent currently seen
};
static MotorCmd cmd;
static HardwareTimer ctlTimer(TIM5);

// pulse (motor test / PID test-drive)
static uint32_t pulseEnd = 0;
static bool pulsing = false;

// ---------------- sensors ----------------
static void tofInitAll() {
  for (uint8_t i = 0; i < 5; i++) {
    pinMode(PIN_TOF_XSHUT[i], OUTPUT);
    digitalWrite(PIN_TOF_XSHUT[i], LOW);  // hold all in reset
  }
  delay(10);  // boot-time only
  for (uint8_t i = 0; i < 5; i++) {
    IWatchdog.reload();
    digitalWrite(PIN_TOF_XSHUT[i], HIGH);
    delay(10);
    tof[i].setTimeout(30);  // short I2C timeout — never stalls the loop
    if (tof[i].init()) {
      tof[i].setAddress(0x2A + i);  // unique address per sensor
      tof[i].setDistanceMode(VL53L1X::Short);
      tof[i].setMeasurementTimingBudget(20000);
      tof[i].startContinuous(33);
      tofState[i].present = true;
    } else {
      tofState[i].present = false;  // FAIL in Sensor Health, keep going
    }
  }
}

static bool imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(IMU_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static void imuInit() {
  imu.present = imuWrite(0x6B, 0x00);  // wake MPU6050
}

static void imuUpdate(uint32_t now) {
  if (!imu.present || now - lastImuMs < 20) return;  // 50 Hz
  lastImuMs = now;
  Wire.beginTransmission(IMU_I2C_ADDR);
  Wire.write(0x3B);  // ACCEL_XOUT_H
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
  // impact = sudden deviation from 1 g beyond the push-sense threshold
  if (fabsf(mag - accMag) > G.cur.pushSense * 0.1f) imu.impact = true;
}

static void sensorsUpdate(uint32_t now) {
  for (uint8_t i = 0; i < 5; i++) {
    if (!tofState[i].present) {
      tofState[i].ok = false;
      continue;
    }
    if (tof[i].dataReady()) {           // non-blocking check
      uint16_t mm = tof[i].read(false); // reads already-available data
      if (mm > 0 && !tof[i].timeoutOccurred()) {
        int32_t adj = (int32_t)mm + G.cur.tofZeroOffsetMm;
        tofState[i].mm = (uint16_t)((adj < 0) ? 0 : adj);
        tofLastOk[i] = now;
      }
    }
    // PASS = valid reading within 300 ms; auto-flags 0/timeout (spec 2.1)
    tofState[i].ok = (now - tofLastOk[i]) < 300 && tofState[i].mm > 0;
  }
  edgeValL = (int16_t)analogRead(PIN_EDGE_LEFT);
  edgeValR = (int16_t)analogRead(PIN_EDGE_RIGHT);
  edgeL = edgeValL > G.cur.edgeThreshold;  // white boundary reflects more
  edgeR = edgeValR > G.cur.edgeThreshold;
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

// ---------------- 200 Hz control ISR ----------------
// Steering PID lives entirely inside the ISR (its own state); it reads the
// volatile command block and writes motor PWM. Kept deliberately short.
static void controlIsr() {
  static float integ = 0, prevErr = 0;
  int16_t l, r;
  if (cmd.steerPid && cmd.valid) {
    float err = (float)(cmd.dirIdx - 2);  // -2..+2, 0 = dead ahead
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
  motorL.setPercent(l);
  motorR.setPercent(r);
}

// ---------------- motors ----------------
void talonHwBegin() {
  LzMotor::Mode m = G.cur.motorMode == 0 ? LzMotor::ESC : LzMotor::HBRIDGE;
  motorL.begin(m, PIN_MOTOR_L_PWM, PIN_MOTOR_L_DIR);
  motorR.begin(m, PIN_MOTOR_R_PWM, PIN_MOTOR_R_DIR);
  pinMode(PIN_EDGE_LEFT, INPUT_ANALOG);
  pinMode(PIN_EDGE_RIGHT, INPUT_ANALOG);
  tofInitAll();
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
  noInterrupts();  // brief critical section — multi-field update (spec 6.2)
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
  if (runState() != RS_IDLE) return;  // never during a match
  cmdOpenLoop(pctL, pctR);
  pulseEnd = millis() + ms;
  pulsing = true;
}

// ---------------- RUN MODE engine ----------------
static RunState rs = RS_IDLE;
static uint32_t rsT0 = 0;          // countdown start / run start
static uint8_t phase = 0;
static uint32_t phaseT0 = 0;
static uint32_t giveUpT0 = 0;
static int bestDist = -1;
static uint8_t cdBeeps = 0;
static char stopReason[20] = "";
static bool evading = false;
static uint32_t evadeT0 = 0;
static const char *actionName = "IDLE";

RunState runState() { return rs; }
uint8_t runPhaseIdx() { return phase; }
const char *runActionName() { return actionName; }
uint32_t runElapsedMs() { return rs == RS_RUNNING ? millis() - rsT0 : 0; }
const char *runStopReason() { return stopReason; }
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

void runStart() {
  Strategy &s = activeStrategy();
  if (!s.used || s.phaseCount == 0) {
    Buzzer.play(SND_ERROR);
    return;
  }
  rs = RS_COUNTDOWN;
  rsT0 = millis();
  cdBeeps = 0;
  stopReason[0] = 0;
  OS.setRunActive(true);  // blocks flash writes (spec 6.1)
  Leds.green(LZLED_BLINK_FAST);
  actionName = "COUNTDOWN";
}

void runAbort(const char *reason) {
  rs = RS_IDLE;
  talonMotorsStop();
  OS.setRunActive(false);
  Leds.green(LZLED_ON);
  snprintf(stopReason, sizeof(stopReason), "%s", reason);
  actionName = "IDLE";
}

static void enterPhase(uint8_t idx, uint32_t now) {
  Strategy &s = activeStrategy();
  phase = (idx < s.phaseCount) ? idx : 0;
  phaseT0 = now;
  imu.impact = false;
}

// translate the current phase into motor commands
static void drivePhase(uint32_t now) {
  Strategy &s = activeStrategy();
  Phase &p = s.phases[phase];
  int16_t fwd = G.cur.maxFwdPct, rev = (int16_t)-G.cur.maxRevPct;
  uint32_t el = now - phaseT0;
  actionName = PHASE_TYPE_NAMES[p.type];

  switch (p.type) {
    case PH_SEARCH_SPIN:
      cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(-fwd / 2));
      break;
    case PH_SEARCH_SWEEP: {
      // arc left/right alternating each second
      bool left = (el / 1000) & 1;
      cmdOpenLoop(left ? (int16_t)(fwd / 3) : (int16_t)(fwd * 2 / 3),
                  left ? (int16_t)(fwd * 2 / 3) : (int16_t)(fwd / 3));
      break;
    }
    case PH_SEARCH_CHARGE:
    case PH_ATT_RAM: {
      // straight-ahead with PID aim when the opponent is visible
      noInterrupts();
      cmd.steerPid = true;
      cmd.base = (p.type == PH_ATT_RAM) ? fwd : (int16_t)(fwd * 3 / 4);
      cmd.dirIdx = (int8_t)opponentDirIdx();
      cmd.valid = opponentDistMm() > 0;
      interrupts();
      break;
    }
    case PH_ATT_CURVE: {
      // arcing approach biased toward the opponent's side
      int dir = opponentDirIdx();
      int16_t inner = (int16_t)(fwd / 2), outer = fwd;
      if (dir <= 1)
        cmdOpenLoop(inner, outer);       // opponent left -> arc left
      else if (dir >= 3)
        cmdOpenLoop(outer, inner);       // opponent right -> arc right
      else
        cmdOpenLoop(outer, (int16_t)(outer * 4 / 5));  // gentle default arc
      break;
    }
    case PH_ATT_SIDE: {
      // wide flank for ~40% of the phase, then cut in hard
      if (el < (uint32_t)(p.durDs * 40))
        cmdOpenLoop(fwd, (int16_t)(fwd / 4));
      else
        cmdOpenLoop((int16_t)(fwd / 4), fwd);
      break;
    }
    case PH_RET_BACKTURN:
      if (el < 600)
        cmdOpenLoop(rev, rev);
      else
        cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(-fwd / 2));
      break;
    case PH_RET_BACKONLY:
      cmdOpenLoop(rev, rev);
      break;
    case PH_RET_CENTER:
    default:
      // back off, quarter-turn, push toward center
      if (el < 500)
        cmdOpenLoop(rev, rev);
      else if (el < 900)
        cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(-fwd / 2));
      else
        cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(fwd / 2));
      break;
  }
}

static void engineTick(uint32_t now) {
  if (rs == RS_COUNTDOWN) {
    uint32_t e = now - rsT0;
    uint8_t sec = (uint8_t)(e / 1000);
    if (sec + 1 > cdBeeps && sec < 5) {  // beep each countdown second
      cdBeeps = (uint8_t)(sec + 1);
      Buzzer.play(SND_CLICK);
      OS.requestRedraw();
    }
    if (e >= 5000) {  // competition-rule 5 s delay done — fight!
      Buzzer.play(SND_MATCH_START);
      rs = RS_RUNNING;
      rsT0 = now;
      giveUpT0 = now;
      bestDist = -1;
      evading = false;
      enterPhase(0, now);
    }
    return;
  }
  if (rs != RS_RUNNING) return;

  Strategy &s = activeStrategy();
  Phase &p = s.phases[phase];

  // --- IMU safety (spec 2.1 Orientation): cut motors when flipped/tilted
  if (G.cur.autoStopFlip && imu.present &&
      (imu.flipped || imu.tiltDeg > G.cur.tiltLimitDeg)) {
    Buzzer.play(SND_ERROR);
    runAbort("FLIP AUTO-STOP");
    return;
  }

  // --- edge handling: trigger transition if the phase asks for it,
  //     otherwise reflex-evade so we never drive off the dohyo
  if (edgeL || edgeR) {
    if (p.trigger == TR_EDGE) {
      enterPhase((uint8_t)(phase + 1 >= s.phaseCount ? 0 : phase + 1), now);
    } else if (!evading) {
      evading = true;
      evadeT0 = now;
    }
  }
  if (evading) {
    actionName = "EDGE-EVADE";
    int16_t fwd = G.cur.maxFwdPct;
    if (now - evadeT0 < 450)
      cmdOpenLoop((int16_t)-G.cur.maxRevPct, (int16_t)-G.cur.maxRevPct);
    else if (now - evadeT0 < 800)
      cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(-fwd / 2));
    else
      evading = false;
    return;  // reflex overrides the phase while active
  }

  // --- give-up safety timer (spec 2.2): in ATTACK with no progress
  int dist = opponentDistMm();
  if (dist > 0 && (bestDist < 0 || dist < bestDist - 50)) {
    bestDist = dist;   // gained ≥5 cm on the opponent — progress
    giveUpT0 = now;
  }
  if (imu.impact && phaseIsAttack(p.type)) giveUpT0 = now;  // push detected
  if (phaseIsAttack(p.type) &&
      (now - giveUpT0) > (uint32_t)s.giveUpDs * 100) {
    // force the designated retreat, then restart the playbook
    static Phase retreatPhase;
    retreatPhase.type = (uint8_t)(PH_RET_BACKTURN + s.giveUpRetreat);
    retreatPhase.trigger = TR_TIME;
    retreatPhase.durDs = 15;
    // run retreat inline: reuse phase slot semantics by direct drive
    actionName = "GIVE-UP RETREAT";
    uint32_t el = now - (giveUpT0 + (uint32_t)s.giveUpDs * 100);
    int16_t fwd = G.cur.maxFwdPct, rev = (int16_t)-G.cur.maxRevPct;
    if (el < 700)
      cmdOpenLoop(rev, rev);
    else if (el < 1200)
      cmdOpenLoop((int16_t)(fwd / 2), (int16_t)(-fwd / 2));
    else {
      enterPhase(0, now);  // playbook restarts at phase 1
      giveUpT0 = now;
      bestDist = -1;
    }
    return;
  }

  // --- normal phase transitions
  bool advance = false;
  switch (p.trigger) {
    case TR_TIME:
      advance = (now - phaseT0) >= (uint32_t)p.durDs * 100;
      break;
    case TR_OPPONENT:
      advance = (dist > 0 && dist < 800);  // opponent detected in range
      break;
    case TR_EDGE:
      break;  // handled above
  }
  if (advance)
    enterPhase((uint8_t)(phase + 1 >= s.phaseCount ? 0 : phase + 1), now);

  drivePhase(now);
}

void talonHwTick(uint32_t now) {
  sensorsUpdate(now);
  engineTick(now);
  if (pulsing && (int32_t)(now - pulseEnd) >= 0) talonMotorsStop();
}
