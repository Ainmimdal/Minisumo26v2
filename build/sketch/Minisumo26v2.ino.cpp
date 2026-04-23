#line 1 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Servo.h>
#include "CombatProfile.h"

// ==========================================
// 1. PIN DEFINITIONS
// ==========================================
const int FWD_M1_PIN = 6;
const int REV_M1_PIN = 5;
const int FWD_M2_PIN = 8;
const int REV_M2_PIN = 7;

const int S1_PIN = 12; // Left
const int S2_PIN = 13; // Front-Left
const int S3_PIN = 14; // Front-Middle
const int S4_PIN = 15; // Front-Right
const int S5_PIN = 26; // Right

const int E2_PIN = 27; // Left Line (Digital)
const int E1_PIN = 29; // Right Line (Digital)

const int DIP1_PIN = 4;
const int DIP2_PIN = 3;
const int DIP3_PIN = 2;

const int BUTTON_PIN = 0;
const int START_BTN_PIN = 11; // IR Start Module
const int SERVO_PIN = 1;
const int LED1_PIN = 10; // blue
const int LED2_PIN = 9;  // red
const int BATT_SENSE_PIN = 28;

const int RGB_PIN = 16;
Adafruit_NeoPixel pixel(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

Servo flagServo;

// ==========================================
// 2. UI COLORS
// ==========================================
struct NamedColor { uint8_t r, g, b; };

const NamedColor COLOR_OFF     = {0, 0, 0};
const NamedColor COLOR_CYAN    = {0, 255, 255};
const NamedColor COLOR_YELLOW  = {255, 255, 0};
const NamedColor COLOR_RED     = {255, 0, 0};
const NamedColor COLOR_BLUE    = {0, 0, 255};
const NamedColor COLOR_MAGENTA = {255, 0, 255};
const NamedColor COLOR_ORANGE  = {255, 100, 0};
const NamedColor COLOR_GREEN   = {0, 255, 0};
const NamedColor COLOR_WHITE   = {255, 255, 255};

uint32_t lastShownColor = 0;
bool hasShownColor = false;

#line 57 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void showColor(const NamedColor &c);
#line 210 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void processServoDetach(unsigned long now);
#line 217 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void updateVariantLeds();
#line 222 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void readLineSensors(bool &leftLine, bool &rightLine);
#line 227 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void setMotors(int leftSpeed, int rightSpeed);
#line 273 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void startLineEvade(bool leftLine, bool rightLine, const CombatProfile &p, unsigned long now);
#line 292 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void updateLineEvade(unsigned long now);
#line 312 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
bool handleLineEvade(bool leftLine, bool rightLine, const CombatProfile &p, unsigned long now);
#line 326 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void updatePushingMatchRamp(unsigned long now);
#line 339 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void runCombatCore(bool leftLine, bool rightLine, int s1, int s2, int s3, int s4, int s5, const CombatProfile &p, bool hasTarget, unsigned long now);
#line 384 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void runPidTrackTest(bool leftLine, bool rightLine, int s1, int s2, int s3, int s4, int s5, const CombatProfile &p, bool hasTarget, unsigned long now);
#line 409 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void setup();
#line 446 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void loop();
#line 57 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\Minisumo26v2.ino"
void showColor(const NamedColor &c) {
  uint32_t packed = pixel.Color(c.r, c.g, c.b);
  if (hasShownColor && packed == lastShownColor) return;
  lastShownColor = packed;
  hasShownColor = true;
  pixel.setPixelColor(0, packed);
  pixel.show();
}


// ==========================================
// 3. TUNING KNOBS
// ==========================================
const unsigned long INPUT_LOCK_MS = 220;
const unsigned long BUTTON_LONG_PRESS_MS = 5000;
const unsigned long UI_DISPLAY_MS = 2500;

// STRATEGY 1: EDGE AMBUSH
// Preview Color: CYAN
// DIP (1,2,3): ON, OFF, OFF
// Overall: make a small opening sidestep, wait, then attack only after detection.
// Steps: use EDGE_AMBUSH_SIDESTEP_MS for the opener, then hold until target and run PID.
const unsigned long EDGE_AMBUSH_SIDESTEP_MS = 200;
//                                          base evd srch   kp    kd  bkup trnS trnL
const CombatProfile PROFILE_EDGE_AMBUSH   = { 30, 50,  30, 20.0, 70.0, 200, 150, 225};

// STRATEGY 2: WAIT THEN INCH FORWARD
// Preview Color: YELLOW
// DIP (1,2,3): OFF, ON, OFF
// Overall: delay engagement, then probe forward in pulses.
// Steps: hold for WAIT_INCH_HOLD_MS; drive for WAIT_INCH_FORWARD_MS; pause for WAIT_INCH_PAUSE_MS; repeat; on target run PID.
const unsigned long WAIT_INCH_HOLD_MS = 1700;
const unsigned long WAIT_INCH_FORWARD_MS = 180;
const unsigned long WAIT_INCH_PAUSE_MS = 720;
//                                          base evd srch   kp    kd  bkup trnS trnL
const CombatProfile PROFILE_WAIT_INCH     = { 60, 50,  30, 20.0,70.0, 250, 150, 225};

// STRATEGY 3: STATIC GUARD THEN ATTACK
// Preview Color: RED
// DIP (1,2,3): ON, ON, OFF
// Overall: pure defensive hold until enemy appears.
// Steps: stay at 0 while no target; line safety still active; on target run PID.
//                                          base evd srch   kp    kd  bkup trnS trnL
const CombatProfile PROFILE_STATIC_GUARD  = { 70, 55,  25, 20.0, 70.0, 250, 150, 225};

// STRATEGY 4: AGGRESSIVE RUSH + SEARCH
// Preview Color: BLUE
// DIP (1,2,3): OFF, OFF, ON
// Overall: aggressive opening to force early contact.
// Steps: rush straight for AGGRO_RUSH_MS; if still no target keep driving straight; on target run PID.
const unsigned long AGGRO_RUSH_MS = 200;
//                                          base evd srch   kp    kd  bkup trnS trnL
const CombatProfile PROFILE_AGGRO_RUSH    = { 50, 50,  20, 20.0, 70.0, 250, 150, 225};

// STRATEGY 5: BALANCED PID COMBAT
// Preview Color: MAGENTA
// DIP (1,2,3): ON, OFF, ON
// Overall: standard all-rounder combat behavior.
// Steps: no opener delay; immediately run PID with normal line safety.
//                                          base evd srch   kp    kd  bkup trnS trnL
const CombatProfile PROFILE_BALANCED_PID  = { 50, 50,  30, 20.0,70.0, 200, 185, 300};

// STRATEGY 6: FLAG-AWARE PID
// Preview Color: ORANGE
// DIP (1,2,3): OFF, ON, ON
// Overall: experimental flag-aware PID behavior.
// Handles two situations when front + side sensors co-activate:
//   Situation 1 (transient clip): front+side clears within grace period.
//     -> PID runs with side sensors zeroed so no 90deg yank.
//   Situation 2 (stuck at flank): front+side persists beyond grace period.
//     -> Reverse and turn toward the active side sensor.
const unsigned long S6_FLAG_GRACE_MS = 250;
const int S6_FLAG_REVERSE_SPEED = 30;
const int S6_FLAG_TURN_SPEED = 30;
//                                          base evd srch   kp    kd  bkup trnS trnL
const CombatProfile PROFILE_FLAG_EXPERIMENT = { 50, 50, 30, 20.0,70.0, 200, 185, 300};

// STRATEGY 7: SIDE FLANK OPENING
// Preview Color: GREEN
// DIP (1,2,3): ON, ON, ON
// Overall: flank first, then engage.
// Steps: turn for SIDE_FLANK_TURN_MS; drive for SIDE_FLANK_DRIVE_MS; counter-turn opposite for SIDE_FLANK_COUNTER_TURN_MS; then run PID.
const unsigned long SIDE_FLANK_TURN_MS = 50;
const unsigned long SIDE_FLANK_DRIVE_MS = 200;
const unsigned long SIDE_FLANK_COUNTER_TURN_MS = 100;
//                                          base evd srch   kp    kd  bkup trnS trnL
const CombatProfile PROFILE_SIDE_FLANK   = { 75, 55,  30, 20.0,70.0, 200, 185, 300};

// Lost-target: spin toward last seen direction briefly, then drive straight.
const unsigned long LOST_TARGET_SPIN_MS = 100;  // how long to spin toward lastSeen
const unsigned long LOST_TARGET_TIMEOUT_MS = 1000;

// Pushing match: ramp speed when locked onto target continuously.
const unsigned long PUSHING_MATCH_THRESHOLD_MS = 500;
const unsigned long PUSHING_MATCH_RAMP_INTERVAL_MS = 150;
const int PUSHING_MATCH_RAMP_STEP = 5;
const int PUSHING_MATCH_MAX_BOOST = 40;

// Braking: active brake when speed is 0 during match.
const bool ENABLE_ACTIVE_BRAKE = true;
const int ACTIVE_BRAKE_PWM = 255;

// PWM trim for straight-line compensation.
const int LEFT_PWM_TRIM = 2;
const int RIGHT_PWM_TRIM = 0;

// Servo settle time before detaching.
const unsigned long SERVO_SETTLE_MS = 150;

// ==========================================
// 4. RUNTIME STATE
// ==========================================
bool matchStarted = false;
bool lastRemoteState = false;
int lastSeen = 3;
unsigned long matchStartMs = 0;
bool preferRightOpen = true;
unsigned long inputLockUntilMs = 0;
unsigned long lastTargetSeenMs = 0;
bool buttonActive = false;
bool longPressActive = false;
unsigned long buttonTimer = 0;
unsigned long lastUiChangeMs = 0;
int lastStrategy = -1;

bool inPushingMatch = false;
unsigned long continuousTargetStartMs = 0;
unsigned long lastRampUpdateMs = 0;
int pushingMatchRampLevel = 0;

bool strategy1Engaged = false;

bool servoDetachPending = false;
unsigned long servoDetachAtMs = 0;

float lastError = 0;
unsigned long flagCoActiveStartMs = 0;

struct LineEvadeState {
  bool active;
  uint8_t phase;          // 0=backup, 1=turn
  int speed;
  int turnDir;            // +1 right, -1 left
  unsigned long phaseStartMs;
  unsigned long backupMs;
  unsigned long turnDurationMs;
};

LineEvadeState lineEvade = {false, 0, 0, 1, 0, 0, 0};

// ==========================================
// 5. HELPER FUNCTIONS
// ==========================================
void processServoDetach(unsigned long now) {
  if (servoDetachPending && now >= servoDetachAtMs) {
    flagServo.detach();
    servoDetachPending = false;
  }
}

void updateVariantLeds() {
  digitalWrite(LED2_PIN, preferRightOpen ? HIGH : LOW);
  digitalWrite(LED1_PIN, preferRightOpen ? LOW : HIGH);
}

void readLineSensors(bool &leftLine, bool &rightLine) {
  leftLine  = (digitalRead(E2_PIN) == LOW);
  rightLine = (digitalRead(E1_PIN) == LOW);
}

void setMotors(int leftSpeed, int rightSpeed) {
  bool useActiveBrakeNow = ENABLE_ACTIVE_BRAKE && matchStarted;

  leftSpeed = constrain(leftSpeed, -100, 100);
  rightSpeed = constrain(rightSpeed, -100, 100);

  int leftPWM = map(abs(leftSpeed), 0, 100, 0, 255);
  int rightPWM = map(abs(rightSpeed), 0, 100, 0, 255);

  leftPWM = constrain(leftPWM, 0, 230);
  rightPWM = constrain(rightPWM, 0, 230);

  if (leftSpeed != 0) leftPWM = constrain(leftPWM + LEFT_PWM_TRIM, 0, 230);
  if (rightSpeed != 0) rightPWM = constrain(rightPWM + RIGHT_PWM_TRIM, 0, 230);

  if (leftSpeed == 0) {
    if (useActiveBrakeNow) {
      analogWrite(FWD_M1_PIN, ACTIVE_BRAKE_PWM);
      analogWrite(REV_M1_PIN, ACTIVE_BRAKE_PWM);
    } else {
      analogWrite(FWD_M1_PIN, 0);
      analogWrite(REV_M1_PIN, 0);
    }
  } else if (leftSpeed > 0) {
    digitalWrite(REV_M1_PIN, LOW); analogWrite(FWD_M1_PIN, leftPWM);
  } else {
    digitalWrite(FWD_M1_PIN, LOW); analogWrite(REV_M1_PIN, leftPWM);
  }

  // Right Motor (Inverted Logic)
  if (rightSpeed == 0) {
    if (useActiveBrakeNow) {
      analogWrite(FWD_M2_PIN, ACTIVE_BRAKE_PWM);
      analogWrite(REV_M2_PIN, ACTIVE_BRAKE_PWM);
    } else {
      analogWrite(FWD_M2_PIN, 0);
      analogWrite(REV_M2_PIN, 0);
    }
  } else if (rightSpeed > 0) {
    digitalWrite(FWD_M2_PIN, LOW); analogWrite(REV_M2_PIN, rightPWM);
  } else {
    digitalWrite(REV_M2_PIN, LOW); analogWrite(FWD_M2_PIN, rightPWM);
  }
}

// --- LINE EVADE ---
void startLineEvade(bool leftLine, bool rightLine, const CombatProfile &p, unsigned long now) {
  lineEvade.active = true;
  lineEvade.phase = 0;
  lineEvade.speed = p.evadeSpeed;
  lineEvade.phaseStartMs = now;
  lineEvade.backupMs = p.evadeBackupMs;

  if (rightLine && !leftLine) {
    lineEvade.turnDir = -1;
    lineEvade.turnDurationMs = p.evadeTurnShortMs;
  } else if (leftLine && !rightLine) {
    lineEvade.turnDir = 1;
    lineEvade.turnDurationMs = p.evadeTurnShortMs;
  } else {
    lineEvade.turnDir = 1;
    lineEvade.turnDurationMs = p.evadeTurnLongMs;
  }
}

void updateLineEvade(unsigned long now) {
  if (!lineEvade.active) return;
  unsigned long elapsed = now - lineEvade.phaseStartMs;

  if (lineEvade.phase == 0) {
    setMotors(-lineEvade.speed, -lineEvade.speed);
    if (elapsed >= lineEvade.backupMs) {
      lineEvade.phase = 1;
      lineEvade.phaseStartMs = now;
    }
    return;
  }

  setMotors(lineEvade.turnDir * lineEvade.speed, -lineEvade.turnDir * lineEvade.speed);
  if (elapsed >= lineEvade.turnDurationMs) {
    lineEvade.active = false;
    lastSeen = 3;
  }
}

bool handleLineEvade(bool leftLine, bool rightLine, const CombatProfile &p, unsigned long now) {
  if (lineEvade.active) {
    updateLineEvade(now);
    return true;
  }
  if (leftLine || rightLine) {
    startLineEvade(leftLine, rightLine, p, now);
    updateLineEvade(now);
    return true;
  }
  return false;
}

// --- PUSHING MATCH RAMP ---
void updatePushingMatchRamp(unsigned long now) {
  if (!inPushingMatch) return;
  if (now - lastRampUpdateMs >= PUSHING_MATCH_RAMP_INTERVAL_MS) {
    lastRampUpdateMs = now;
    if (pushingMatchRampLevel < PUSHING_MATCH_MAX_BOOST) {
      pushingMatchRampLevel += PUSHING_MATCH_RAMP_STEP;
      if (pushingMatchRampLevel > PUSHING_MATCH_MAX_BOOST)
        pushingMatchRampLevel = PUSHING_MATCH_MAX_BOOST;
    }
  }
}

// --- COMBAT CORE ---
void runCombatCore(bool leftLine, bool rightLine, int s1, int s2, int s3, int s4, int s5,
                   const CombatProfile &p, bool hasTarget, unsigned long now) {

  if (handleLineEvade(leftLine, rightLine, p, now)) return;

  if (hasTarget) {
    lastTargetSeenMs = now;

    if (continuousTargetStartMs == 0) continuousTargetStartMs = now;
    unsigned long continuousMs = now - continuousTargetStartMs;

    if (!inPushingMatch && continuousMs >= PUSHING_MATCH_THRESHOLD_MS) {
      inPushingMatch = true;
      pushingMatchRampLevel = 0;
      lastRampUpdateMs = now;
    }
    if (inPushingMatch) updatePushingMatchRamp(now);

    int effectiveBaseSpeed = p.baseSpeed + pushingMatchRampLevel;
    float error = (s1 * -5.0) + (s2 * -2.0) + (s3 * 0.0) + (s4 * 2.0) + (s5 * 5.0);
    float pidOutput = (p.kp * error) + (p.kd * (error - lastError));
    lastError = error;

    setMotors(effectiveBaseSpeed + pidOutput, effectiveBaseSpeed - pidOutput);

    if (error <= -2.0) lastSeen = 1;
    else if (error >= 2.0) lastSeen = 5;
    else lastSeen = 3;
  } else {
    continuousTargetStartMs = 0;
    inPushingMatch = false;
    pushingMatchRampLevel = 0;

    unsigned long lostForMs = now - lastTargetSeenMs;
    if (lostForMs < LOST_TARGET_SPIN_MS && lastSeen != 3) {
      // Spin toward last known direction
      if (lastSeen == 1) setMotors(-p.searchSpeed, p.searchSpeed);
      else               setMotors(p.searchSpeed, -p.searchSpeed);
    } else {
      setMotors(p.searchSpeed, p.searchSpeed);
    }
  }
}

// --- TRACKING TEST CORE ---
void runPidTrackTest(bool leftLine, bool rightLine, int s1, int s2, int s3, int s4, int s5,
                     const CombatProfile &p, bool hasTarget, unsigned long now) {

  if (handleLineEvade(leftLine, rightLine, p, now)) return;

  if (hasTarget) {
    lastTargetSeenMs = now;

    float error = (s1 * -5.0) + (s2 * -2.0) + (s3 * 0.0) + (s4 * 2.0) + (s5 * 5.0);
    float pidOutput = (p.kp * error) + (p.kd * (error - lastError));
    lastError = error;

    setMotors(pidOutput, -pidOutput);

    if (error <= -2.0) lastSeen = 1;
    else if (error >= 2.0) lastSeen = 5;
    else lastSeen = 3;
  } else {
    setMotors(0, 0);
  }
}

// ==========================================
// 6. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  pixel.begin();
  pixel.setBrightness(40);

  flagServo.attach(SERVO_PIN, 500, 2500);
  flagServo.write(0);
  delay(100);
  flagServo.detach();

  pinMode(FWD_M1_PIN, OUTPUT); pinMode(REV_M1_PIN, OUTPUT);
  pinMode(FWD_M2_PIN, OUTPUT); pinMode(REV_M2_PIN, OUTPUT);
  setMotors(0, 0);

  pinMode(S1_PIN, INPUT); pinMode(S2_PIN, INPUT);
  pinMode(S3_PIN, INPUT); pinMode(S4_PIN, INPUT);
  pinMode(S5_PIN, INPUT);

  pinMode(E1_PIN, INPUT);
  pinMode(E2_PIN, INPUT);

  pinMode(DIP1_PIN, INPUT_PULLUP);
  pinMode(DIP2_PIN, INPUT_PULLUP);
  pinMode(DIP3_PIN, INPUT_PULLUP);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(START_BTN_PIN, INPUT_PULLDOWN);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  updateVariantLeds();
}

// ==========================================
// 7. MAIN LOOP
// ==========================================
void loop() {
  unsigned long now = millis();

  processServoDetach(now);

  bool dip1 = !digitalRead(DIP1_PIN);
  bool dip2 = !digitalRead(DIP2_PIN);
  bool dip3 = !digitalRead(DIP3_PIN);

  bool physicalBtnPressed = (digitalRead(BUTTON_PIN) == LOW);
  bool currentRemoteState = (digitalRead(START_BTN_PIN) == HIGH);

  bool remoteJustStarted = (currentRemoteState && !lastRemoteState);
  bool remoteJustStopped = (!currentRemoteState && lastRemoteState);
  lastRemoteState = currentRemoteState;

  bool controlsLocked = (now < inputLockUntilMs);
  bool startTriggered = !controlsLocked && remoteJustStarted;
  bool stopTriggered  = !controlsLocked && remoteJustStopped;

  int strategy = 0;
  if (dip1) strategy += 1;
  if (dip2) strategy += 2;
  if (dip3) strategy += 4;

  // --- PRE-MATCH ---
  if (!matchStarted) {
    // Short press: toggle variant. Long press: reserved for future use.
    if (physicalBtnPressed) {
      if (!buttonActive) {
        buttonActive = true;
        buttonTimer = now;
      }
      if (!longPressActive && (now - buttonTimer >= BUTTON_LONG_PRESS_MS)) {
        longPressActive = true;
        // Future: add long-press behavior here.
      }
    } else if (buttonActive) {
      if (longPressActive) {
        longPressActive = false;
      } else if (!controlsLocked) {
        preferRightOpen = !preferRightOpen;
        inputLockUntilMs = now + INPUT_LOCK_MS;
        lastUiChangeMs = now;
        updateVariantLeds();
      }
      buttonActive = false;
      buttonTimer = 0;
    }
    // Detect changes to refresh display timer
    if (strategy != lastStrategy) {
      lastStrategy = strategy;
      lastUiChangeMs = now;
    }

    bool uiVisible = (now - lastUiChangeMs < UI_DISPLAY_MS);

    if (uiVisible) {
      updateVariantLeds();
      switch (strategy) {
        case 0: showColor(COLOR_OFF); break;
        case 1: showColor(COLOR_CYAN); break;
        case 2: showColor(COLOR_YELLOW); break;
        case 3: showColor(COLOR_RED); break;
        case 4: showColor(COLOR_BLUE); break;
        case 5: showColor(COLOR_MAGENTA); break;
        case 6: showColor(COLOR_ORANGE); break;
        case 7: showColor(COLOR_GREEN); break;
      }
    } else {
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, LOW);
      showColor(COLOR_OFF);
    }

    setMotors(0, 0);

    if (startTriggered && strategy == 0) {
      matchStarted = true;
      inputLockUntilMs = now + INPUT_LOCK_MS;
      return;
    }

    if (startTriggered && strategy != 0) {
      matchStarted = true;
      matchStartMs = now;
      lastTargetSeenMs = now;
      lastError = 0;
      lastSeen = 3;
      lineEvade.active = false;
      continuousTargetStartMs = 0;
      inPushingMatch = false;
      pushingMatchRampLevel = 0;
      strategy1Engaged = false;
      lastRampUpdateMs = 0;
      flagCoActiveStartMs = 0;
      inputLockUntilMs = now + INPUT_LOCK_MS;

      if (!flagServo.attached()) {
        flagServo.attach(SERVO_PIN, 500, 2500);
      }
      flagServo.write(90);
      servoDetachPending = true;
      servoDetachAtMs = now + SERVO_SETTLE_MS;
    }
    return;
  }

  // --- KILL SWITCH ---
  if (stopTriggered) {
    matchStarted = false;
    setMotors(0, 0);
    lineEvade.active = false;
    continuousTargetStartMs = 0;
    inPushingMatch = false;
    pushingMatchRampLevel = 0;
    strategy1Engaged = false;
    inputLockUntilMs = now + INPUT_LOCK_MS;

    if (!flagServo.attached()) {
      flagServo.attach(SERVO_PIN, 500, 2500);
    }
    flagServo.write(0);
    servoDetachPending = true;
    servoDetachAtMs = now + SERVO_SETTLE_MS;

    return;
  }

  updateVariantLeds();
  if (lineEvade.active) digitalWrite(LED1_PIN, HIGH);

  // --- SENSOR READING ---
  bool leftLine, rightLine;
  readLineSensors(leftLine, rightLine);

  int s1 = digitalRead(S1_PIN);
  int s2 = digitalRead(S2_PIN);
  int s3 = digitalRead(S3_PIN);
  int s4 = digitalRead(S4_PIN);
  int s5 = digitalRead(S5_PIN);
  bool hasTarget = (s1 || s2 || s3 || s4 || s5);
  unsigned long elapsed = now - matchStartMs;

  // STRATEGY 0: DEBUG / TEST — DIP: OFF OFF OFF
  if (strategy == 0) {
    // Inline evade: speed=50, backup=200ms, turnShort=185ms, turnLong=300ms
    if (lineEvade.active) {
      updateLineEvade(now);
      return;
    }
    if (leftLine || rightLine) {
      const CombatProfile s0evade = {20, 65, 40, 0, 0, 200, 120, 200};
      startLineEvade(leftLine, rightLine, s0evade, now);
      updateLineEvade(now);
      return;
    }
    setMotors(20, 20);
    return;
  }

  // STRATEGY 1: EDGE AMBUSH
  else if (strategy == 1) {
    if (hasTarget) strategy1Engaged = true;

    if (elapsed < EDGE_AMBUSH_SIDESTEP_MS) {
      if (preferRightOpen) setMotors(70, 45);
      else setMotors(45, 70);
    } else if (!strategy1Engaged) {
      setMotors(0, 0);
    } else {
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
                    PROFILE_EDGE_AMBUSH, hasTarget, now);
    }
  }

  // STRATEGY 2: WAIT THEN INCH
  else if (strategy == 2) {
    if (handleLineEvade(leftLine, rightLine, PROFILE_WAIT_INCH, now)) return;

    if (hasTarget) {
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
                    PROFILE_WAIT_INCH, hasTarget, now);
      return;
    }

    continuousTargetStartMs = 0;
    inPushingMatch = false;
    pushingMatchRampLevel = 0;

    if (elapsed < WAIT_INCH_HOLD_MS) {
      setMotors(0, 0);
    } else {
      unsigned long pulseCycleMs = WAIT_INCH_FORWARD_MS + WAIT_INCH_PAUSE_MS;
      unsigned long pulse = (elapsed - WAIT_INCH_HOLD_MS) % pulseCycleMs;
      if (pulse < WAIT_INCH_FORWARD_MS) setMotors(22, 22);
      else setMotors(0, 0);
    }
  }

  // STRATEGY 3: STATIC GUARD
  else if (strategy == 3) {
    if (hasTarget) strategy1Engaged = true;

    if (!strategy1Engaged) {
      // Pre-contact: hold still, line evade still active
      if (handleLineEvade(leftLine, rightLine, PROFILE_STATIC_GUARD, now)) return;
      setMotors(0, 0);
    } else {
      // Post-contact: full combat core (tracks when seen, searches when lost)
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
                    PROFILE_STATIC_GUARD, hasTarget, now);
    }
  }

  // STRATEGY 4: AGGRO RUSH
  else if (strategy == 4) {
    // Rush phase: drive hard if no target yet during opening window
    if (!hasTarget && elapsed < AGGRO_RUSH_MS) {
      if (handleLineEvade(leftLine, rightLine, PROFILE_AGGRO_RUSH, now)) return;
      setMotors(90, 90);
      return;
    }

    // After rush (or if target found during rush): full combat core
    runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
                  PROFILE_AGGRO_RUSH, hasTarget, now);
  }

  // STRATEGY 5: BALANCED PID
  else if (strategy == 5) {
    runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
                  PROFILE_BALANCED_PID, hasTarget, now);
  }

  // STRATEGY 6: FLAG-AWARE PID
  else if (strategy == 6) {
    if (handleLineEvade(leftLine, rightLine, PROFILE_FLAG_EXPERIMENT, now)) return;

    bool frontAny = (s2 || s3 || s4);
    bool leftFlagCase  = (s1 && frontAny && !s5);
    bool rightFlagCase = (s5 && frontAny && !s1);
    bool flagCombo = leftFlagCase || rightFlagCase;

    if (flagCombo) {
      if (flagCoActiveStartMs == 0) flagCoActiveStartMs = now;
      unsigned long coActiveDuration = now - flagCoActiveStartMs;

      if (coActiveDuration < S6_FLAG_GRACE_MS) {
        // Grace period: run PID with side sensors zeroed to prevent yank
        runCombatCore(leftLine, rightLine, 0, s2, s3, s4, 0,
                      PROFILE_FLAG_EXPERIMENT, hasTarget, now);
      } else {
        // Sustained: reverse and turn toward the active side sensor
        if (leftFlagCase) {
          setMotors(-S6_FLAG_REVERSE_SPEED, S6_FLAG_TURN_SPEED);
          lastSeen = 1;
        } else {
          setMotors(S6_FLAG_TURN_SPEED, -S6_FLAG_REVERSE_SPEED);
          lastSeen = 5;
        }
      }
    } else {
      flagCoActiveStartMs = 0;
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
                    PROFILE_FLAG_EXPERIMENT, hasTarget, now);
    }
  }

  // STRATEGY 7: SIDE FLANK
  else if (strategy == 7) {
    unsigned long flankTurnEnd = SIDE_FLANK_TURN_MS;
    unsigned long flankDriveEnd = flankTurnEnd + SIDE_FLANK_DRIVE_MS;
    unsigned long flankCounterTurnEnd = flankDriveEnd + SIDE_FLANK_COUNTER_TURN_MS;

    if (elapsed < flankTurnEnd) {
      if (preferRightOpen) setMotors(70, -40);
      else setMotors(-40, 70);
      return;
    }
    if (elapsed < flankDriveEnd) {
      setMotors(90, 90);
      return;
    }
    if (elapsed < flankCounterTurnEnd) {
      if (preferRightOpen) setMotors(-70, 85);
      else setMotors(85, -70);
      return;
    }

    runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
                  PROFILE_SIDE_FLANK, hasTarget, now);
  }
}
