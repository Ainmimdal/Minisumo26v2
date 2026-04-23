#line 1 "C:\\Users\\Mimdal\\Documents\\Arduino\\Minisumo26v2\\CombatProfile.h"
#pragma once

// Combat profile — all per-strategy tuning in one place.
// baseSpeed:        drive speed when tracking target (0-100)
// evadeSpeed:       motor speed during line evade (0-100)
// searchSpeed:      forward speed when target lost (0-100)
// kp / kd:          PID gains for steering
// evadeBackupMs:    reverse duration on line detect (ms)
// evadeTurnShortMs: turn duration, ONE sensor triggered (ms)
// evadeTurnLongMs:  turn duration, BOTH sensors triggered (ms)
struct CombatProfile {
  int baseSpeed;
  int evadeSpeed;
  int searchSpeed;
  float kp;
  float kd;
  unsigned long evadeBackupMs;
  unsigned long evadeTurnShortMs;
  unsigned long evadeTurnLongMs;
};