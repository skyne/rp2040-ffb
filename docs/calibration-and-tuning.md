# Calibration and Tuning Guide

**How to calibrate sensors and tune force feedback for optimal performance**

This guide covers all calibration procedures and force feedback tuning to get the best experience from your rp2040-ffb wheel.

---

## Table of Contents

1. [Overview](#overview)
2. [Gear Ratio Calibration](#gear-ratio-calibration)
3. [Pedal Calibration](#pedal-calibration)
4. [Index Magnet Alignment](#index-magnet-alignment)
5. [Force Feedback Tuning](#force-feedback-tuning)
6. [Advanced FFB Parameters](#advanced-ffb-parameters)
7. [Per-Game Settings](#per-game-settings)
8. [Troubleshooting Calibration](#troubleshooting-calibration)

---

## Overview

### Why Calibration Matters

**Uncalibrated symptoms:**
- ❌ HID angle doesn't match physical position
- ❌ Pedals don't reach 0% or 100%
- ❌ FFB feels too weak or dangerously strong
- ❌ Wheel oscillates or vibrates
- ❌ Center spring pulls to wrong position

**Calibrated results:**
- ✅ Accurate position reporting
- ✅ Full pedal travel
- ✅ Smooth, predictable force feedback
- ✅ Safe motor operation
- ✅ Optimal gaming experience

### Calibration Order

**Follow this sequence:**

1. **Gear ratio** (mechanical accuracy)
2. **Index magnet** (absolute position reference)
3. **Pedals** (input range)
4. **FFB tuning** (feel and safety)
5. **Game-specific** (per-title adjustments)

### Required Tools

- Serial terminal (PlatformIO monitor, screen, or ffb-config GUI)
- Ruler or measuring tape
- Level (for wheel alignment)
- Notebook (record values)

---

## Gear Ratio Calibration

### Purpose

The gear ratio converts motor angle (from MLX90363) to wheel angle (what the game sees). An accurate ratio ensures:
- HID position matches physical rotation
- FFB torque scales correctly
- Calibration survives firmware updates

### Procedure

**1. Enter calibration mode:**

```
Serial> c
Starting gear calibration...
Rotate wheel now
```

**2. Rotate wheel slowly:**
- **Direction:** Either direction (CW or CCW)
- **Amount:** Exactly **one full rotation** (360°)
- **Speed:** Slow and steady (~5 seconds per rotation)
- **Starting position:** Doesn't matter

**3. Finish calibration:**

```
Serial> c
Gear CAL ok, ratio=18.1234
Turned 6527.45° axle → 360.00° wheel
```

**4. Save the ratio:**

```
Serial> :set gear_ratio 18.1234
Serial> :save
Settings saved to EEPROM
```

### Understanding the Output

**Example output:**
```
Gear CAL ok, ratio=18.1234
Turned 6527.45° axle → 360.00° wheel
```

- **`ratio=18.1234`**: Measured gear ratio (axle:wheel)
- **`6527.45° axle`**: Total axle rotation during calibration
- **`360.00° wheel`**: One full wheel rotation

**Expected ranges:**
- G920/G923: 18.0 - 18.5 typical
- Custom builds: Depends on gearbox

### Validation

**Check accuracy:**

1. Align wheel perfectly straight (use level)
2. Note HID angle: `Serial> :dump` → `hid_angle=0.0`
3. Rotate wheel exactly 90° right
4. Check HID angle: Should be close to `90.0°`
5. Rotate back to center
6. Repeat for 90° left

**Tolerance:**
- Good: ±2° error over 90° rotation
- Acceptable: ±5° error
- Bad: >10° error → Re-run calibration

### Tips

**Common mistakes:**
- ❌ Rotating too fast (firmware misses encoder ticks)
- ❌ Not completing full 360° rotation
- ❌ Changing direction mid-calibration
- ❌ Forgetting to save (`:save`)

**Best practices:**
- ✅ Mark starting position with tape
- ✅ Use slow, smooth rotation
- ✅ Verify wheel returns to mark after 360°
- ✅ Run calibration 2-3 times, average results

---

## Pedal Calibration

### Purpose

Pedal calibration maps the raw ADC values (0-4095) to usable input range (0-100%). This compensates for:
- Different potentiometer ranges
- Mechanical travel limits
- Dead zones

### Automatic Calibration

**1. Reset calibration:**

```
Serial> p
Pedal auto-cal reset
Press each pedal fully, then release
```

**2. Press each pedal:**
- **Throttle:** Press to 100%, release completely
- **Brake:** Press to 100%, release completely
- **Clutch:** Press to 100%, release completely

**Order doesn't matter, but press each at least once.**

**3. Observe learning:**

```
thr: rest=2890 min=450 max=3200 → 0%
brk: rest=2950 min=520 max=3180 → 0%
clu: rest=3100 min=600 max=3220 → 0%
```

**4. Save calibration:**

```
Serial> :save
Settings saved to EEPROM
```

### Manual Calibration

If auto-calibration fails, set values manually:

**1. Record raw values:**

Enable telemetry and note ADC readings:

```
Serial> t
T ... thr=2890 brk=2950 clu=3100   # Rest position
```

Press throttle fully:
```
T ... thr=450 brk=2950 clu=3100    # Throttle full
```

Press brake fully:
```
T ... thr=2890 brk=520 clu=3100    # Brake full
```

Press clutch fully:
```
T ... thr=2890 brk=2950 clu=600    # Clutch full
```

**2. Set calibration values:**

```
Serial> :set thr_rest 2890
Serial> :set thr_min 450
Serial> :set thr_max 3200

Serial> :set brk_rest 2950
Serial> :set brk_min 520
Serial> :set brk_max 3180

Serial> :set clu_rest 3100
Serial> :set clu_min 600
Serial> :set clu_max 3220

Serial> :save
```

### Validation

**Test pedal travel:**

1. Enable telemetry: `t`
2. Press each pedal slowly from 0% → 100%
3. Observe percentage values:
   ```
   T ... thr=0% brk=0% clu=0%      # Rest
   T ... thr=50% brk=0% clu=0%     # Half throttle
   T ... thr=100% brk=0% clu=0%    # Full throttle
   ```

**Expected behavior:**
- Rest position: 0% (±2%)
- Full press: 100% (±2%)
- Smooth progression: No sudden jumps
- No jitter: ±1% at rest

### Troubleshooting

**Pedal always reads 100%:**
- Wiring reversed (swap min/max values)
- Or wrong ADC pin

**Pedal jitters at rest:**
- Electrical noise → Add 100nF capacitor
- Or increase dead zone: `:set pedal_deadzone 50`

**Pedal doesn't reach 100%:**
- Mechanical stop before full travel
- Or increase max value: `:set thr_max 3300`

---

## Index Magnet Alignment

### Purpose

The index magnet provides an absolute position reference, allowing the firmware to:
- Know wheel orientation after power-on
- Correct for drift
- Align HID center with physical center

### Procedure

**1. Align wheel physically straight:**
- Use level or visual alignment
- Should be "driving straight" position
- Mark this position with tape

**2. Check current angle:**

```
Serial> :dump
...
axle=5.2 (raw MLX angle)
hid_angle=0.0 (reported to game)
...
```

**3. Record raw angle:**

Note the `axle=` value when wheel is centered. Example: `5.2°`

**4. Update firmware:**

Edit `firmware-base/src/config.h`:

```cpp
// Before:
#define AXLE_INDEX_ANGLE_DEG 0.0f

// After:
#define AXLE_INDEX_ANGLE_DEG 5.2f
```

**5. Rebuild and upload:**

```bash
cd firmware-base
pio run -t upload
```

**6. Verify alignment:**

After reboot, wheel should:
- Report `hid_angle=0.0` when physically centered
- Report positive angles turning right
- Report negative angles turning left

### Advanced: Using Simulator Mode

If you don't want to rebuild firmware:

```
Serial> :sim axle 5.2
Simulator: axle offset set to 5.2°
```

This temporarily shifts the angle reference. Changes are lost on reboot.

---

## Force Feedback Tuning

### Safety First

**Start conservative:**

```
Serial> :set duty_cap 0.10      # 10% max power
Serial> :set torque_cap 0.10
Serial> :set spring_k 0.002     # Very light spring
Serial> :save
```

**Test incrementally:**

```
Serial> e                       # Enable motors
Serial> s                       # Spring mode
# Turn wheel - should center gently
```

**Gradually increase if comfortable:**

```
Serial> :set duty_cap 0.15
Serial> :set torque_cap 0.15
Serial> :set spring_k 0.004
```

### Key Parameters

#### `duty_cap` (Motor Power Limit)

**What it does:** Limits PWM duty cycle (0.0 - 1.0)

**Effects:**
- **Low (0.10):** Gentle forces, safe for testing
- **Medium (0.25):** Good for most games
- **High (0.50):** Strong forces, monitor temperature
- **Unsafe (>0.70):** Risk of motor/driver damage

**Tuning process:**

1. Start at `0.10`
2. Test spring mode (`s`)
3. Increase by `0.05` if too weak
4. Stop when force feels adequate
5. Leave headroom for safety (don't max out)

**Recommended values:**
- Testing: `0.10 - 0.15`
- Normal use: `0.20 - 0.35`
- Maximum safe: `0.50` (with monitoring)

#### `torque_cap` (Software Torque Limit)

**What it does:** Limits commanded torque before duty cycle conversion

**Relationship to duty_cap:**
- Usually set equal: `torque_cap = duty_cap`
- Can be lower for finer control
- Should never exceed `duty_cap`

#### `spring_k` (Spring Centering Stiffness)

**What it does:** Controls how strongly wheel centers

**Effects:**
- **Low (0.001):** Very light centering
- **Medium (0.005):** Moderate centering
- **High (0.010):** Strong centering
- **Too high (>0.015):** Oscillation, unstable

**Tuning process:**

1. Enable spring mode: `s`
2. Start at `0.002`
3. Increase by `0.001` until centering feels right
4. If wheel oscillates, reduce by `0.002`

**Formula:**
```
Torque = spring_k × (angle_error) × torque_cap
```

**Recommended values:**
- Light: `0.002 - 0.004`
- Medium: `0.004 - 0.008`
- Heavy: `0.008 - 0.012`

#### `damper_k` (Velocity Damping)

**What it does:** Resists fast wheel movement

**Effects:**
- Smooths oscillations
- Adds weight to wheel feel
- Can feel sluggish if too high

**Tuning process:**

1. Enable damper mode: `:set ffb_mode damper`
2. Start at `0.0001`
3. Increase until oscillation stops
4. Back off slightly if wheel feels heavy

**Recommended values:**
- Light: `0.0001 - 0.0003`
- Medium: `0.0003 - 0.0006`
- Heavy: `0.0006 - 0.001`

### Tuning by Feel

**Goal: Smooth, responsive, safe**

**Checklist:**

1. **Spring centering test:**
   - Does wheel return to center smoothly?
   - No oscillation (back-and-forth wobble)?
   - Force proportional to angle?

2. **Manual resistance test:**
   - Turn wheel with motors enabled
   - Should resist proportionally
   - No sudden jerks or dead spots

3. **Damper test:**
   - Quick flick of the wheel
   - Should slow down smoothly
   - Not overdamped (heavy/sluggish)

4. **Safety test:**
   - Motors should stop on `d` command immediately
   - No unusual heat after 5 minutes
   - No grinding or clicking sounds

### Temperature Monitoring

**Safe operation:**
- Motor case: < 60°C (warm to touch)
- BTS7960 heatsink: < 70°C (hot to touch)

**Overheating signs:**
- Motor too hot to hold
- Burning smell
- Reduced torque output
- Firmware motor watchdog triggers

**Cooling down:**
```
Serial> d                       # Disable motors
# Wait 10 minutes before re-enabling
```

---

## Advanced FFB Parameters

### `hid_range` (Steering Wheel Range)

**What it does:** Sets the total rotation range reported to games

**Common values:**
- **900°:** Realistic road cars (most common)
- **540°:** Race cars
- **1080°:** Heavy trucks (ETS2, ATS)
- **360°:** Go-karts, arcade

**How to set:**

```
Serial> :set hid_range 900
Serial> :save
```

**Effect on HID reports:**
- Physical rotation: -900° to +900° (180° left/right)
- Mapped to: HID axis -32767 to +32767

### `ffb_gain` (Global FFB Multiplier)

**What it does:** Scales all FFB effects (0.0 - 2.0)

**Uses:**
- Quickly adjust overall strength
- Per-game fine-tuning
- Accessibility (reduce for less force)

**Example:**

```
Serial> :set ffb_gain 0.5       # Half strength
Serial> :set ffb_gain 1.5       # 50% stronger
```

### `ffb_deadzone` (Center Dead Zone)

**What it does:** No FFB torque within ±N degrees of center

**Uses:**
- Eliminate small jitter
- Smoother on-center feel

**Typical values:**
- None: `0.0°`
- Minimal: `1.0 - 2.0°`
- Noticeable: `3.0 - 5.0°`

### `motor_polarity` (Reverse Motor Direction)

**What it does:** Flips motor direction

**When to use:**
- FFB pulls away from center (should pull toward)
- Left/right forces are reversed

**How to set:**

```
Serial> :set motor_polarity -1  # Reverse both motors
Serial> :save
```

---

## Per-Game Settings

### Game Profiles

Save different settings for each game:

**Example: LMU (Le Mans Ultimate)**

```
# LMU expects strong FFB signals
:set duty_cap 0.35
:set spring_k 0.008
:set ffb_gain 1.2
:save
```

**Example: ETS2 (Euro Truck Simulator)**

```
# ETS needs large wheel range
:set hid_range 1080
:set duty_cap 0.25
:set spring_k 0.005
:save
```

**Example: Assetto Corsa**

```
# AC has strong FFB in-game
:set duty_cap 0.20
:set ffb_gain 0.8
:set spring_k 0.004
:save
```

### Switching Profiles

**Option 1: Manual commands**

Keep a text file with settings, paste into serial terminal.

**Option 2: GUI profiles (future)**

The `ffb-config` GUI will support saving/loading profiles.

**Option 3: Firmware presets**

Add common presets to firmware:

```cpp
// In settings.cpp
void loadPreset(const char* name) {
    if (strcmp(name, "lmu") == 0) {
        g.dutyCap = 0.35f;
        g.springK = 0.008f;
        // ...
    }
}
```

Then use: `Serial> :preset lmu`

---

## Troubleshooting Calibration

### Gear Ratio Issues

**Symptom: HID angle drifts over time**
- **Cause:** Incorrect gear ratio
- **Fix:** Re-run calibration, ensure full 360° rotation

**Symptom: HID angle is off by constant amount**
- **Cause:** Index magnet misaligned
- **Fix:** Adjust `AXLE_INDEX_ANGLE_DEG`

**Symptom: HID angle jumps around**
- **Cause:** MLX90363 sensor issue
- **Fix:** Check SPI wiring, magnet distance

### Pedal Issues

**Symptom: Pedal stuck at 50%**
- **Cause:** Not calibrated
- **Fix:** Run `p` auto-calibration

**Symptom: Pedal inverted (0% when pressed)**
- **Cause:** Min/max swapped
- **Fix:** Swap values or reverse wiring

**Symptom: Pedal jittery**
- **Cause:** Electrical noise
- **Fix:** Add 100nF capacitor, increase deadzone

### FFB Issues

**Symptom: Wheel oscillates violently**
- **Cause:** `spring_k` too high
- **Fix:** Reduce by 50%, add damping

**Symptom: FFB too weak**
- **Cause:** `duty_cap` / `torque_cap` too low
- **Fix:** Increase gradually, test safety

**Symptom: FFB pulls to one side**
- **Cause:** Motor polarity wrong
- **Fix:** `:set motor_polarity -1`

**Symptom: Motors overheat**
- **Cause:** `duty_cap` too high, or prolonged use
- **Fix:** Reduce duty cap, add cooling fans

---

## Reference Tables

### Quick Settings Presets

| Use Case | duty_cap | torque_cap | spring_k | hid_range |
|----------|----------|------------|----------|-----------|
| Testing | 0.10 | 0.10 | 0.002 | 900 |
| Casual sim | 0.20 | 0.20 | 0.005 | 900 |
| Serious sim | 0.30 | 0.30 | 0.008 | 900 |
| Truck sim | 0.25 | 0.25 | 0.004 | 1080 |
| Rally | 0.35 | 0.35 | 0.006 | 540 |

### Safety Limits

| Parameter | Safe Min | Safe Max | Danger Zone |
|-----------|----------|----------|-------------|
| duty_cap | 0.05 | 0.50 | > 0.70 |
| spring_k | 0.001 | 0.012 | > 0.015 |
| damper_k | 0.0 | 0.001 | > 0.002 |
| Motor temp | — | 60°C | > 70°C |
| Driver temp | — | 70°C | > 85°C |

---

## Calibration Checklist

**Before first use:**

- [ ] Gear ratio calibrated (`:c` command)
- [ ] Index magnet aligned (physical center = 0°)
- [ ] Pedals calibrated (`:p` command)
- [ ] Settings saved (`:save`)
- [ ] FFB tested at low power (`duty_cap=0.10`)
- [ ] Temperature monitored (motors < 60°C)
- [ ] Emergency stop tested (`:d` command)

**Regular maintenance:**

- [ ] Re-calibrate pedals every 3-6 months
- [ ] Check gear ratio if HID drifts
- [ ] Inspect wiring for loose connections
- [ ] Verify settings after firmware update

---

*Last updated: 2024-09-12*
