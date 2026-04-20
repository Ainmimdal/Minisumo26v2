#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Servo.h> // ---> NEW: Added Servo Library

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
const int SERVO_PIN = 1;      // ---> NEW: Servo Pin
const int LED1_PIN = 10;
const int LED2_PIN = 9;
const int BATT_SENSE_PIN = 28; // Optional: Battery Voltage Sensing (Analog)

const int RGB_PIN = 16;
Adafruit_NeoPixel pixel(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

// ---> NEW: Create Servo Object
Servo flagServo; 

// ==========================================
// 2. UI COLORS (READABLE NAMES)
// ==========================================
struct NamedColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

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

void showColor(const NamedColor &c) {
  uint32_t packed = pixel.Color(c.r, c.g, c.b);
  if (hasShownColor && packed == lastShownColor) return;

  lastShownColor = packed;
  hasShownColor = true;
  pixel.setPixelColor(0, packed);
  pixel.show();
}

// ==========================================
// 3. TUNING KNOBS (FAST TO ADJUST)
// ==========================================
// Global/shared: input lock for button/IR edge handling.
const unsigned long INPUT_LOCK_MS = 220;

// Shared line safety (used by all strategies through handleLineEvade/runCombatCore).
const unsigned long EVADE_BACKUP_MS = 200;
const unsigned long EVADE_TURN_SHORT_MS = 200;
const unsigned long EVADE_TURN_LONG_MS = 300;

// Shared lost-target behavior (used by all strategies through runCombatCore).
// Stage 2: after timeout, drive straight with a small speed boost until reacquire.
const unsigned long LOST_TARGET_TIMEOUT_MS = 1400;
const int LOST_TARGET_STRAIGHT_BOOST = 8;

// Braking behavior when commanded speed is 0.
// true: active brake, false: coast.
const bool ENABLE_ACTIVE_BRAKE = true;
const int ACTIVE_BRAKE_PWM = 255;

// PWM trim for straight-line compensation.
// Positive value makes that side stronger. Tune in +/-1 steps.
const int LEFT_PWM_TRIM = 2;
const int RIGHT_PWM_TRIM = 0;

struct CombatProfile {
  int baseSpeed;
  int evadeSpeed;
  int searchSpeed;
  float kp;
  float kd;
};

// STRATEGY 1: EDGE AMBUSH
// Overall: make a small opening sidestep, wait, then attack only after detection.
// Steps: use EDGE_AMBUSH_SIDESTEP_MS for the opener, then hold until target and run PROFILE_EDGE_AMBUSH PID.
const unsigned long EDGE_AMBUSH_SIDESTEP_MS = 200;
const CombatProfile PROFILE_EDGE_AMBUSH    = {30, 50, 30, 10.0, 23.0};

// STRATEGY 2: WAIT THEN INCH FORWARD
// Overall: delay engagement, then probe forward in pulses.
// Steps: hold for WAIT_INCH_HOLD_MS; drive for WAIT_INCH_FORWARD_MS; pause for WAIT_INCH_PAUSE_MS; repeat; on target run PROFILE_WAIT_INCH PID.
const unsigned long WAIT_INCH_HOLD_MS = 1700;
const unsigned long WAIT_INCH_FORWARD_MS = 180;
const unsigned long WAIT_INCH_PAUSE_MS = 720;
const CombatProfile PROFILE_WAIT_INCH      = {60, 50, 30, 10.0, 23.0};

// STRATEGY 3: STATIC GUARD THEN ATTACK
// Overall: pure defensive hold until enemy appears.
// Steps: no dedicated timing knobs; tune PROFILE_STATIC_GUARD only.
const CombatProfile PROFILE_STATIC_GUARD   = {30, 50, 30, 10.0, 23.0};

// STRATEGY 4: AGGRESSIVE RUSH + SEARCH
// Overall: aggressive opening to force early contact.
// Steps: rush straight for AGGRO_RUSH_MS; if still no target spin-search using lastSeen; on target run PROFILE_AGGRO_RUSH PID.
const unsigned long AGGRO_RUSH_MS = 700;
const CombatProfile PROFILE_AGGRO_RUSH     = {30, 50, 30, 10.0, 23.0};

// STRATEGY 5: BALANCED PID COMBAT
// Overall: standard all-rounder combat behavior.
// Steps: no opener timing knobs; tune PROFILE_BALANCED_PID only.
const CombatProfile PROFILE_BALANCED_PID   = {30, 50, 30, 10.0, 23.0};

// STRATEGY 6: OFFENSIVE STALK
// Overall: controlled hunt pattern before full attack.
// Steps: wait STALK_WAIT_MS; sweep left for STALK_SWEEP_LEFT_MS; sweep right for STALK_SWEEP_RIGHT_MS; repeat; on target run PROFILE_OFFENSIVE_STALK PID.
const unsigned long STALK_WAIT_MS = 900;
const unsigned long STALK_SWEEP_LEFT_MS = 500;
const unsigned long STALK_SWEEP_RIGHT_MS = 500;
const CombatProfile PROFILE_OFFENSIVE_STALK= {30, 50, 30, 10.0, 23.0};

// STRATEGY 7: SIDE FLANK OPENING
// Overall: flank first, then engage.
// Steps: turn for SIDE_FLANK_TURN_MS; drive for SIDE_FLANK_DRIVE_MS; counter-turn opposite for SIDE_FLANK_COUNTER_TURN_MS; then run PROFILE_SIDE_FLANK PID.
const unsigned long SIDE_FLANK_TURN_MS = 50;
// Set to 10 to preserve the previous effective drive time after switching to per-move timing.
const unsigned long SIDE_FLANK_DRIVE_MS = 200;
const unsigned long SIDE_FLANK_COUNTER_TURN_MS = 100;
const CombatProfile PROFILE_SIDE_FLANK     = {75, 60, 30, 15.0, 23.0};

// ==========================================
// 4. COMBAT VARIABLES
// ==========================================
bool matchStarted = false;
bool lastRemoteState = false; // Tracks what the remote sent last cycle
bool lastVariantBtnState = false;
int lastSeen = 3; 
unsigned long matchStartMs = 0;
bool preferRightOpen = true;
unsigned long inputLockUntilMs = 0;
unsigned long lastTargetSeenMs = 0;

// Global PID state
float lastError = 0;

struct LineEvadeState {
  bool active;
  uint8_t phase; // 0=backup, 1=turn
  int speed;
  int turnDir;   // +1 turn right, -1 turn left
  unsigned long phaseStartMs;
  unsigned long turnDurationMs;
};

LineEvadeState lineEvade = {false, 0, 0, 1, 0, 0};

void updateVariantLeds() {
  // LED2 ON => right variant, LED1 ON => left variant.
  digitalWrite(LED2_PIN, preferRightOpen ? HIGH : LOW);
  digitalWrite(LED1_PIN, preferRightOpen ? LOW : HIGH);
}

// ==========================================
// 5. MOTOR HELPER FUNCTION
// ==========================================
void setMotors(int leftSpeed, int rightSpeed) {
  bool useActiveBrakeNow = ENABLE_ACTIVE_BRAKE && matchStarted;

  leftSpeed = constrain(leftSpeed, -100, 100);
  rightSpeed = constrain(rightSpeed, -100, 100);

  int leftPWM = map(abs(leftSpeed), 0, 100, 0, 255);
  int rightPWM = map(abs(rightSpeed), 0, 100, 0, 255);

  if (leftSpeed != 0) leftPWM = constrain(leftPWM + LEFT_PWM_TRIM, 0, 255);
  if (rightSpeed != 0) rightPWM = constrain(rightPWM + RIGHT_PWM_TRIM, 0, 255);

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

void startLineEvade(bool leftLine, bool rightLine, int evadeSpeed, unsigned long now) {
  lineEvade.active = true;
  lineEvade.phase = 0;
  lineEvade.speed = evadeSpeed;
  lineEvade.phaseStartMs = now;

  if (rightLine && !leftLine) {
    lineEvade.turnDir = -1;
    lineEvade.turnDurationMs = EVADE_TURN_SHORT_MS;
  } else if (leftLine && !rightLine) {
    lineEvade.turnDir = 1;
    lineEvade.turnDurationMs = EVADE_TURN_SHORT_MS;
  } else {
    lineEvade.turnDir = 1;
    lineEvade.turnDurationMs = EVADE_TURN_LONG_MS;
  }
}

void updateLineEvade(unsigned long now) {
  if (!lineEvade.active) return;

  unsigned long elapsed = now - lineEvade.phaseStartMs;

  if (lineEvade.phase == 0) {
    setMotors(-lineEvade.speed, -lineEvade.speed);
    if (elapsed >= EVADE_BACKUP_MS) {
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

bool handleLineEvade(bool leftLine, bool rightLine, int evadeSpeed, unsigned long now) {
  if (lineEvade.active) {
    updateLineEvade(now);
    return true;
  }

  if (leftLine || rightLine) {
    showColor(COLOR_WHITE);
    startLineEvade(leftLine, rightLine, evadeSpeed, now);
    updateLineEvade(now);
    return true;
  }

  return false;
}

void runCombatCore(bool leftLine, bool rightLine, int s1, int s2, int s3, int s4, int s5,
                   int baseSpeed, int evadeSpeed, int searchSpeed, float currentKp, float currentKd,
                   bool hasTarget, unsigned long now) {

  if (handleLineEvade(leftLine, rightLine, evadeSpeed, now)) {
    return;
  }

  if (hasTarget) {
    lastTargetSeenMs = now;
    float error = (s1 * -5.0) + (s2 * -2.0) + (s3 * 0.0) + (s4 * 2.0) + (s5 * 5.0);
    float pidOutput = (currentKp * error) + (currentKd * (error - lastError));
    lastError = error;

    setMotors(baseSpeed + pidOutput, baseSpeed - pidOutput);

    if (error <= -2.0) lastSeen = 1;
    else if (error >= 2.0) lastSeen = 5;
    else lastSeen = 3;
  } else {
    unsigned long lostForMs = now - lastTargetSeenMs;

    // Stage 2 lost-target behavior: move straight across the field
    // until opponent sensors reacquire or line safety interrupts.
    if (lostForMs >= LOST_TARGET_TIMEOUT_MS) {
      int fastForward = searchSpeed + LOST_TARGET_STRAIGHT_BOOST;
      setMotors(fastForward, fastForward);
    } else {
      if (lastSeen == 1) setMotors(-searchSpeed, searchSpeed);
      else if (lastSeen == 5) setMotors(searchSpeed, -searchSpeed);
      else setMotors(searchSpeed, searchSpeed);
    }
  }
}

// ==========================================
// 6. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  pixel.begin();
  pixel.setBrightness(40);

  // ---> NEW: Initialize and Reset Servo <---
  flagServo.attach(SERVO_PIN,500, 2500);
  flagServo.write(0); // Starts at 0 degrees on boot

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
  
  // Physical Button stays PULLUP
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // IR Remote gets PULLDOWN (Defaults to LOW if completely disconnected)
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

  bool dip1 = !digitalRead(DIP1_PIN);
  bool dip2 = !digitalRead(DIP2_PIN);
  bool dip3 = !digitalRead(DIP3_PIN);
  
  // --- SMART START/STOP LOGIC ---
  bool physicalBtnPressed = (digitalRead(BUTTON_PIN) == LOW);
  bool currentRemoteState = (digitalRead(START_BTN_PIN) == HIGH); // IR Module is Active HIGH
  bool variantBtnRisingEdge = (physicalBtnPressed && !lastVariantBtnState);
  lastVariantBtnState = physicalBtnPressed;
  
  // Edge Detection: Only trigger when the remote signal *changes*
  bool remoteJustStarted = (currentRemoteState == true && lastRemoteState == false);
  bool remoteJustStopped = (currentRemoteState == false && lastRemoteState == true);
  lastRemoteState = currentRemoteState; // Save for next loop

  bool controlsLocked = (now < inputLockUntilMs);

  // Start/stop are controlled by IR only.
  bool startTriggered = !controlsLocked && remoteJustStarted;
  bool stopTriggered = !controlsLocked && remoteJustStopped;

  int strategy = 0;
  if (dip1) strategy += 1; 
  if (dip2) strategy += 2; 
  if (dip3) strategy += 4; 

  // --- PRE-MATCH / STRATEGY SELECTION ---
  if (!matchStarted) {
    // BUTTON_PIN now toggles left/right variant in pre-match.
    if (!controlsLocked && variantBtnRisingEdge) {
      preferRightOpen = !preferRightOpen;
      inputLockUntilMs = now + INPUT_LOCK_MS;
    }
    updateVariantLeds();

    switch(strategy) {
      case 0: showColor(COLOR_OFF); break;
      case 1: showColor(COLOR_CYAN); break;    // Edge Ambush
      case 2: showColor(COLOR_YELLOW); break;  // Wait + Inch
      case 3: showColor(COLOR_RED); break;     // Static Guard
      case 4: showColor(COLOR_BLUE); break;    // Aggro Rush
      case 5: showColor(COLOR_MAGENTA); break; // Balanced PID
      case 6: showColor(COLOR_ORANGE); break;  // Offensive Stalk
      case 7: showColor(COLOR_GREEN); break;   // Side Flank
    }

    // No movement before match starts.
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
      inputLockUntilMs = now + INPUT_LOCK_MS;
      
      // ---> NEW: Deploy Servo! <---
      flagServo.write(90); 
    }
    return; 
  }

  // --- KILL SWITCH ---
  if (stopTriggered) {
    matchStarted = false;
    setMotors(0, 0); 
    lineEvade.active = false;
    inputLockUntilMs = now + INPUT_LOCK_MS;
    
    // ---> NEW: Retract Servo! <---
    flagServo.write(0); 
    
    showColor(COLOR_RED); // Flash red to confirm stop
    return; 
  }

  updateVariantLeds();

  // --- SENSOR READING ---
  bool leftLine = (digitalRead(E2_PIN) == LOW); 
  bool rightLine = (digitalRead(E1_PIN) == LOW);
  
  int s1 = digitalRead(S1_PIN); 
  int s2 = digitalRead(S2_PIN); 
  int s3 = digitalRead(S3_PIN); 
  int s4 = digitalRead(S4_PIN); 
  int s5 = digitalRead(S5_PIN); 
  bool hasTarget = (s1 || s2 || s3 || s4 || s5);
  unsigned long elapsed = now - matchStartMs;

 
  if (strategy == 0) {
    showColor(COLOR_WHITE);
    setMotors(10, 10);
    return;
  }

  // STRATEGY 1: EDGE AMBUSH
  // Overall: make a small opening sidestep, wait, then attack only after detection.
  // Steps: for EDGE_AMBUSH_SIDESTEP_MS move toward variant side; then hold at 0; on target run PROFILE_EDGE_AMBUSH PID.
  else if (strategy == 1) {
    showColor(COLOR_CYAN);

    if (elapsed < EDGE_AMBUSH_SIDESTEP_MS) {
      if (preferRightOpen) setMotors(45, 15);
      else setMotors(15, 45);
    } else if (!hasTarget) {
      setMotors(0, 0);
    } else {
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
            PROFILE_EDGE_AMBUSH.baseSpeed, PROFILE_EDGE_AMBUSH.evadeSpeed,
        PROFILE_EDGE_AMBUSH.searchSpeed, PROFILE_EDGE_AMBUSH.kp, PROFILE_EDGE_AMBUSH.kd,
            hasTarget, now);
    }
  }
  
  // STRATEGY 2: WAIT THEN INCH FORWARD
  // Overall: delay engagement, then probe forward in pulses.
  // Steps: hold for WAIT_INCH_HOLD_MS; drive for WAIT_INCH_FORWARD_MS; pause for WAIT_INCH_PAUSE_MS; repeat; on target run PROFILE_WAIT_INCH PID.
  else if (strategy == 2) {
    showColor(COLOR_YELLOW);

    if (handleLineEvade(leftLine, rightLine, 45, now)) {
      return;
    }

    if (hasTarget) {
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
            PROFILE_WAIT_INCH.baseSpeed, PROFILE_WAIT_INCH.evadeSpeed,
        PROFILE_WAIT_INCH.searchSpeed, PROFILE_WAIT_INCH.kp, PROFILE_WAIT_INCH.kd,
            hasTarget, now);
      return;
    }

    if (elapsed < WAIT_INCH_HOLD_MS) {
      setMotors(0, 0);
    } else {
      unsigned long pulseCycleMs = WAIT_INCH_FORWARD_MS + WAIT_INCH_PAUSE_MS;
      unsigned long pulse = (elapsed - WAIT_INCH_HOLD_MS) % pulseCycleMs;
      if (pulse < WAIT_INCH_FORWARD_MS) setMotors(22, 22);
      else setMotors(0, 0);
    }
  }

  // STRATEGY 3: STATIC GUARD THEN ATTACK
  // Overall: pure defensive hold until enemy appears.
  // Steps: stay at 0 while no target; line safety still active; on target run PROFILE_STATIC_GUARD PID.
  else if (strategy == 3) {
    showColor(COLOR_RED);

    if (!hasTarget) {
      if (handleLineEvade(leftLine, rightLine, 45, now)) return;
      else setMotors(0, 0);
    } else {
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
            PROFILE_STATIC_GUARD.baseSpeed, PROFILE_STATIC_GUARD.evadeSpeed,
        PROFILE_STATIC_GUARD.searchSpeed, PROFILE_STATIC_GUARD.kp, PROFILE_STATIC_GUARD.kd,
            hasTarget, now);
    }
  }

  // STRATEGY 4: AGGRESSIVE RUSH + SEARCH
  // Overall: aggressive opening to force early contact.
  // Steps: rush straight for AGGRO_RUSH_MS; if still no target spin-search using lastSeen; on target run PROFILE_AGGRO_RUSH PID.
  else if (strategy == 4) {
    showColor(COLOR_BLUE);

    if (hasTarget) {
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
            PROFILE_AGGRO_RUSH.baseSpeed, PROFILE_AGGRO_RUSH.evadeSpeed,
        PROFILE_AGGRO_RUSH.searchSpeed, PROFILE_AGGRO_RUSH.kp, PROFILE_AGGRO_RUSH.kd,
            hasTarget, now);
      return;
    }

    if (handleLineEvade(leftLine, rightLine, 55, now)) {
      return;
    }

    if (elapsed < AGGRO_RUSH_MS) {
      setMotors(58, 58);
    } else {
      if (lastSeen == 1) setMotors(-30, 30);
      else if (lastSeen == 5) setMotors(30, -30);
      else setMotors(30, -30);
    }
  }

  // STRATEGY 5: BALANCED PID COMBAT
  // Overall: standard all-rounder combat behavior.
  // Steps: no opener delay; immediately run PROFILE_BALANCED_PID PID with normal line safety.
  else if (strategy == 5) {
    showColor(COLOR_MAGENTA);
    runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
            PROFILE_BALANCED_PID.baseSpeed, PROFILE_BALANCED_PID.evadeSpeed,
          PROFILE_BALANCED_PID.searchSpeed, PROFILE_BALANCED_PID.kp, PROFILE_BALANCED_PID.kd,
            hasTarget, now);
  }

  // STRATEGY 6: OFFENSIVE STALK (MIDDLE START)
  // Overall: controlled hunt pattern before full attack.
  // Steps: wait STALK_WAIT_MS; sweep left for STALK_SWEEP_LEFT_MS; sweep right for STALK_SWEEP_RIGHT_MS; repeat; on target run PROFILE_OFFENSIVE_STALK PID.
  else if (strategy == 6) {
    showColor(COLOR_ORANGE);

    if (hasTarget) {
      runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
            PROFILE_OFFENSIVE_STALK.baseSpeed, PROFILE_OFFENSIVE_STALK.evadeSpeed,
        PROFILE_OFFENSIVE_STALK.searchSpeed, PROFILE_OFFENSIVE_STALK.kp, PROFILE_OFFENSIVE_STALK.kd,
            hasTarget, now);
      return;
    }

    if (handleLineEvade(leftLine, rightLine, 55, now)) {
      return;
    }

    if (elapsed < STALK_WAIT_MS) {
      setMotors(0, 0);
    } else {
      unsigned long sweepCycleMs = STALK_SWEEP_LEFT_MS + STALK_SWEEP_RIGHT_MS;
      unsigned long phase = (elapsed - STALK_WAIT_MS) % sweepCycleMs;
      if (phase < STALK_SWEEP_LEFT_MS) setMotors(22, 30);
      else setMotors(30, 22);
    }
  }

  // STRATEGY 7: SIDE FLANK OPENING (VARIANT LEFT/RIGHT)
  // Overall: flank first, then engage.
  // Steps: turn for SIDE_FLANK_TURN_MS; drive for SIDE_FLANK_DRIVE_MS; counter-turn opposite for SIDE_FLANK_COUNTER_TURN_MS; then run PROFILE_SIDE_FLANK PID.
  else if (strategy == 7) {
    showColor(COLOR_GREEN);

    unsigned long flankTurnEnd = SIDE_FLANK_TURN_MS;
    unsigned long flankDriveEnd = flankTurnEnd + SIDE_FLANK_DRIVE_MS;
    unsigned long flankCounterTurnEnd = flankDriveEnd + SIDE_FLANK_COUNTER_TURN_MS;

    if (elapsed < flankTurnEnd) {
      if (preferRightOpen) setMotors(70, -40);
      else setMotors(-40, 70);
      return;
    }

    if (elapsed < flankDriveEnd) {
      setMotors(85, 85);
      return;
    }

    if (elapsed < flankCounterTurnEnd) {
      if (preferRightOpen) setMotors(-70, 85);
      else setMotors(85, -70); 
      return;
    }
    
    runCombatCore(leftLine, rightLine, s1, s2, s3, s4, s5,
                  PROFILE_SIDE_FLANK.baseSpeed, PROFILE_SIDE_FLANK.evadeSpeed,
                  PROFILE_SIDE_FLANK.searchSpeed, PROFILE_SIDE_FLANK.kp, PROFILE_SIDE_FLANK.kd,
                  hasTarget, now);
  }
}