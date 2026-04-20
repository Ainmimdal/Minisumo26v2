# Mini Sumo Competition Guide

This guide explains the full behavior of your robot in one place.

Reference code: [Minisumo26v2.ino](Minisumo26v2.ino)

## 1. What The Program Does

- Uses 3 DIP switches to select 1 of 8 modes (0 to 7).
- Starts and stops from either:
  - Physical button
  - IR start module signal edge
- Strategy 0 is a wheel-cleaning mode (20% forward spin).
- Deploys flags automatically on match start for strategy 1 to 7.
- Retracts flags on stop.
- Uses line sensors as top safety priority.
- Uses opponent sensors and PID steering for attack.
- Uses non-blocking timing with millis (no delay anywhere).

## 2. Safety And Match Control

### Start behavior

When start is triggered:

- If strategy is 0:
  - matchStarted becomes true
  - cleaning mode runs via strategy 0 branch (white LED, wheels 20%)
  - no flag deploy

- If strategy is 1 to 7:
  - matchStarted becomes true
  - match timer starts
  - PID memory resets
  - flank preference toggles (for alternating flank strategy)
  - line evade state is reset
  - input lock starts (short lockout to prevent double trigger)
  - flag servo deploys to 90

### Stop behavior

When stop is triggered:

- matchStarted becomes false
- motors stop
- line evade state is reset
- input lock starts
- flag servo retracts to 0
- pixel turns red

## 3. DIP Strategy Map

DIP value is built from DIP1 + DIP2 + DIP3 as binary weights:

- DIP1 = 1
- DIP2 = 2
- DIP3 = 4

So strategy is 0 to 7.

- 0: Cleaning Mode (white while active)
- 1: Edge Ambush (cyan)
- 2: Wait + Inch (yellow)
- 3: Static Guard (red)
- 4: Aggro Rush (blue)
- 5: Balanced PID (magenta)
- 6: Offensive Stalk (orange)
- 7: Side Flank (green)

## 4. Strategy Details

### Strategy 0 (White): Cleaning Mode

- Starts only after pressing start while DIP=0
- Wheels spin forward at 20% (`setMotors(20, 20)`)
- Stops with stop trigger

Best use:
- Cleaning or warming tires before matches

Tune first:
- Cleaning speed in strategy 0 block (`setMotors(20, 20)`)

### Strategy 1 (Cyan): Edge Ambush

- Short sidestep at start (left or right alternates each match)
- Then hold position
- When target appears, enters combat core

Best use:
- Robot placed near edge with a trap plan

Tune first:
- EDGE_AMBUSH_SIDESTEP_MS: how long sidestep lasts before waiting
- baseSpeed in strategy 1 combat call: how hard it pushes after detection
- currentKp in strategy 1 combat call: how aggressively it steers into target

### Strategy 2 (Yellow): Wait Then Inch Forward

- Waits still initially
- If no target, pulses forward in short bursts
- Attacks once target is detected
- Always obeys line evade first

Best use:
- Conservative opening with controlled forward probing

Tune first:
- WAIT_INCH_HOLD_MS: initial still time before inching begins
- WAIT_INCH_PERIOD_MS: total pulse cycle length
- WAIT_INCH_DRIVE_MS: forward-on portion inside the pulse
- baseSpeed and searchSpeed in strategy 2 combat call after lock-on

### Strategy 3 (Red): Static Guard Then Attack

- Holds still while scanning
- No movement until target appears
- Attacks after detection
- If on line, performs evade

Best use:
- Opponent likely rushes first

Tune first:
- baseSpeed in strategy 3 combat call: first push power after detection
- currentKp and Kd: steering sharpness and damping during attack
- evadeSpeed in strategy 3 combat call: how strongly it escapes line events

### Strategy 4 (Blue): Aggressive Rush + Search

- Hard forward rush for a short time
- If still no target, transitions to rotational search
- When target appears, attacks with high speed PID

Best use:
- Fast first-contact rounds

Tune first:
- AGGRO_RUSH_MS: initial straight rush duration
- rush motor command values (currently 58,58): launch aggression
- search turn motor command values (currently +/-30): post-rush scan speed
- baseSpeed/currentKp in strategy 4 combat call: lock-on attack quality

### Strategy 5 (Magenta): Balanced PID Combat

- Immediate normal combat behavior
- Good all-around default profile

Best use:
- Unknown opponent style

Tune first:
- baseSpeed: default pushing strength
- searchSpeed: reacquire speed when target is lost briefly
- currentKp and Kd: balance between responsiveness and oscillation

### Strategy 6 (Orange): Offensive Stalk

- Short initial wait
- Alternating left-right sweep movement
- High-speed attack once detection occurs

Best use:
- Mid-dohyo start with active hunt behavior

Tune first:
- STALK_WAIT_MS: delay before sweep hunt starts
- STALK_SWEEP_PERIOD_MS: left-right sweep rhythm
- sweep motor command values (currently 22,30 and 30,22): arc shape
- baseSpeed/currentKp in strategy 6 combat call for final engagement

### Strategy 7 (Green): Side Flank Opening

- Quick in-place turn to one side (alternates each match)
- Short forward burst
- Then full aggressive combat

Best use:
- Corner attack attempt against front-facing opponent

Tune first:
- SIDE_FLANK_TURN_MS: flank turn duration
- flank turn motor command values (currently 60,-60): turn snap speed
- SIDE_FLANK_DRIVE_MS: forward burst window after turn
- baseSpeed/searchSpeed/currentKp in strategy 7 combat call

## 5. Non-Blocking Architecture (Why It Is Fast)

The sketch uses millis timing instead of delay.

Benefits:
- Sensor checks continue every loop
- Start/stop responsiveness stays high
- No long blind periods during maneuvers
- Easier to tune timing constants

Line evade is implemented as a state machine:

- Phase 0: backup
- Phase 1: turn
- Then return to normal combat

## 6. Tuning Knobs (What Each One Means)

These constants are your pit-stop tuning controls.

### Input and opening timing

- INPUT_LOCK_MS: short ignore window after start/stop so one press does not trigger twice.
- EDGE_AMBUSH_SIDESTEP_MS: strategy 1 side-move time before waiting in ambush.
- WAIT_INCH_HOLD_MS: strategy 2 initial stationary wait time.
- WAIT_INCH_PERIOD_MS: strategy 2 inching cycle duration.
- WAIT_INCH_DRIVE_MS: strategy 2 forward pulse width inside each cycle.
- AGGRO_RUSH_MS: strategy 4 full-speed forward rush time.
- STALK_WAIT_MS: strategy 6 initial hold before sweep starts.
- STALK_SWEEP_PERIOD_MS: strategy 6 left-right sweep cycle length.
- SIDE_FLANK_TURN_MS: strategy 7 first flank turn duration.
- SIDE_FLANK_DRIVE_MS: strategy 7 forward burst phase end time.

### Line evade timing

- EVADE_BACKUP_MS: reverse duration when line is detected.
- EVADE_TURN_SHORT_MS: turn duration when only one line sensor sees edge.
- EVADE_TURN_LONG_MS: turn duration when both line sensors see edge.

### Combat control values (passed per strategy)

Each strategy feeds values into combat core:

- baseSpeed: forward drive level during active target lock.
- evadeSpeed: speed used in line safety escape behavior.
- searchSpeed: speed used for search/spin when no immediate target.
- currentKp: proportional steering gain for target-centering.

Global derivative gain:

- Kd: derivative damping for PID steering. Higher values reduce overshoot, too high can feel sluggish/noisy.

### Combat profile note (current code)

Right now all strategy combat profiles are intentionally set to the same values for easy testing:

- baseSpeed = 30
- evadeSpeed = 50
- searchSpeed = 30
- kp = 15.0

This means most differences between strategies currently come from opening behavior and scan behavior, not combat profile numbers.

### Lost target escape

When opponent sensors have no detection for a while, robot exits normal PID/search lock and performs an aggressive alternating spin scan to reacquire.

Tune with:

- LOST_TARGET_TIMEOUT_MS: time with no opponent before normal PID/search is abandoned.
- LOST_TARGET_ESCAPE_PERIOD_MS: tempo of alternating escape scan once timeout is reached.

## 7. Per-Strategy Variable Map

Use this map when you want to tune a specific strategy quickly.

- Strategy 1 Edge Ambush: EDGE_AMBUSH_SIDESTEP_MS, baseSpeed, currentKp
- Strategy 2 Wait + Inch: WAIT_INCH_HOLD_MS, WAIT_INCH_PERIOD_MS, WAIT_INCH_DRIVE_MS, baseSpeed
- Strategy 3 Static Guard: baseSpeed, currentKp, Kd, evadeSpeed
- Strategy 4 Aggro Rush: AGGRO_RUSH_MS, rush command values, search turn values, currentKp
- Strategy 5 Balanced PID: baseSpeed, searchSpeed, currentKp, Kd
- Strategy 6 Offensive Stalk: STALK_WAIT_MS, STALK_SWEEP_PERIOD_MS, sweep command values, baseSpeed
- Strategy 7 Side Flank: SIDE_FLANK_TURN_MS, SIDE_FLANK_DRIVE_MS, flank turn values, baseSpeed
- Strategy 0 Cleaning: strategy 0 motor command value (currently 20,20)
- All strategies: EVADE_BACKUP_MS, EVADE_TURN_SHORT_MS, EVADE_TURN_LONG_MS, LOST_TARGET_TIMEOUT_MS, LOST_TARGET_ESCAPE_PERIOD_MS

## 8. Suggested Competition Tuning Order

1. Tune line safety first
- Increase/decrease evade speed and evade timing until robot reliably stays in ring.

2. Tune target lock behavior
- Adjust Kp and Kd for clean steering without oscillation.

3. Tune opening moves
- Adjust strategy opening times to your dohyo and motor acceleration.

4. Tune aggression levels
- Raise or lower base/search speeds per strategy for risk level.

5. Tune lost-target recovery
- If robot waits too long after losing enemy, lower LOST_TARGET_TIMEOUT_MS.
- If scan looks too twitchy or too slow, adjust LOST_TARGET_ESCAPE_PERIOD_MS.

## 9. LED Color Meaning

- White: strategy 0 active cleaning mode
- Cyan: strategy 1
- Yellow: strategy 2
- Red: strategy 3 (also stop confirmation)
- Blue: strategy 4
- Magenta: strategy 5
- Orange: strategy 6
- Green: strategy 7
- White also appears during line evade active state

## 10. Current VS Code Include Warning Note

If code compiles and runs but VS Code still shows include warnings on:

- Arduino.h
- Adafruit_NeoPixel.h
- Servo.h

that is usually IntelliSense include-path indexing for RP2040 core internals, not actual runtime logic failure.

For your Waveshare RP2040 Zero, this is commonly harmless if verify/upload succeed.

## 11. Match Day Checklist

- Confirm correct DIP strategy before each round.
- Confirm start source works (button and IR edge).
- Confirm flags deploy on start and retract on stop.
- For strategy 0, confirm no flag deploy (cleaning mode only).
- Confirm line sensors trigger white LED evade behavior.
- Keep one default strategy as fallback (recommended: strategy 5).
- Record tuning changes between rounds.

## 12. Fast Notes Area (Edit Before Event)

- Best default strategy: 
- Best evade speed: 
- Best Kp/Kd pair: 
- Best flank side on first match: 
- Notes versus aggressive opponents: 
- Notes versus defensive opponents: 
