# Troubleshooting Guide

Complete diagnostic and recovery procedures for rp2040-ffb hardware and firmware issues.

## Quick Diagnostic Commands

Connect to the base CDC port (115200 baud) and run these commands:

```
:selftest      # Comprehensive hardware check
:dump          # Show all settings
:version       # Firmware versions
h              # Single-key help menu
```

---

## Table of Contents

1. [No Serial Output](#no-serial-output)
2. [Link Failures (Base ↔ Rim)](#link-failures)
3. [Steering/Hall Sensor Issues](#steeringhall-sensor-issues)
4. [Motor Not Responding](#motor-not-responding)
5. [Pedals Not Working](#pedals-not-working)
6. [Rim Buttons/Encoders Issues](#rim-buttonsencoders-issues)
7. [LED Strip Problems](#led-strip-problems)
8. [Firmware Update Failures](#firmware-update-failures)
9. [EEPROM/Settings Corruption](#eepromsettings-corruption)
10. [Recovery Procedures](#recovery-procedures)

---

## No Serial Output

### Symptoms
- No response when connecting to USB CDC
- PlatformIO upload fails
- ffb-config can't find port

### Diagnostic Steps

**1. Verify USB Cable**
- Try a different USB cable (must be data-capable, not charge-only)
- Try different USB port on your computer
- Avoid USB hubs if possible (connect directly to PC)

**2. Check for BOOTSEL Mode**
- Disconnect USB
- Hold BOOTSEL button on Pico
- Connect USB while holding BOOTSEL
- **Expected:** `RPI-RP2` drive appears
- **If not:** Hardware issue with Pico or USB connection

**3. Voltage Check**
```
Measure with multimeter:
- VBUS (pin 40): 4.8 - 5.2V
- 3V3OUT (pin 36): 3.2 - 3.4V
- GND: Continuity to USB shield
```

**4. Test with Factory Firmware**
- Download `blink.uf2` from Raspberry Pi Pico examples
- Hold BOOTSEL, copy to `RPI-RP2` drive
- **Expected:** Onboard LED blinks
- **If working:** rp2040-ffb firmware issue
- **If not:** Hardware fault

### Solutions

| Problem | Solution |
|---------|----------|
| BOOTSEL works, firmware doesn't | Re-flash latest firmware UF2 |
| No BOOTSEL drive | Check micro-USB solder joints, try different Pico |
| Voltage wrong | Power supply issue, check regulators |
| Works in BOOTSEL only | Firmware crashed - see recovery section |

---

## Link Failures (Base ↔ Rim)

### Symptoms
- `rim_link=0` in `:dump` output
- `rim_fw=?` unknown
- No encoder inputs
- No shift LEDs responding

### Diagnostic Serial Output

**Healthy Link:**
```
OK link 1 pong/s
rim_link=1
rim_fw=20240912 2024-09-12T14:30:00Z
```

**Failed Link:**
```
WARN link timeout (no pong)
rim_link=0
rim_fw=?
```

### Physical Checks

**1. Wiring Verification**
```
Base GP0 (TX) ───────► Rim GP0 (RX)
Base GP1 (RX) ◄─────── Rim GP1 (TX)
Base GND ─────────────── Rim GND
```

**Measure continuity with multimeter in diode/continuity mode:**
- Base GP0 to Rim GP0: beep (< 5Ω)
- Base GP1 to Rim GP1: beep
- Base GND to Rim GND: beep
- Base GP0 to Rim GP1: NO beep (not crossed)

**2. Voltage at Rim**
- Rim VSYS or VBUS: 4.8 - 5.2V
- Rim 3V3OUT: 3.2 - 3.4V

**3. Cable Length**
- UART works reliably up to ~1 meter at 460800 baud
- Longer cables: add 100Ω series resistors at TX ends
- Twisted pair recommended for TX/RX

### Software Checks

**1. Test Rim Independently**
- Disconnect rim from base
- Connect rim directly to USB
- Flash `firmware-rim` with `pio run -d firmware-rim -t upload`
- Rim should boot, blink LEDs in startup sequence

**2. Force Rim Reset from Base**
```
:rim_reset         # Pulse RUN pin
:rim_sync          # Request config
```

Wait 5 seconds, check `:dump` for `rim_link=1`

**3. Check Baud Rate**
Both firmwares must agree on baud (currently 460800):
- Base: `firmware-base/src/config.h` → `PIN_RIM_UART_*`
- Rim: `firmware-rim/src/config.h` → `PIN_LINK_UART_*`

### Common Causes

| Issue | Fix |
|-------|-----|
| Swapped TX/RX | Cross the wires (TX → RX) |
| Missing ground | Add GND wire between boards |
| Cable too long | Shorten cable or add series resistors |
| Rim not powered | Check rim VBUS/VSYS supply |
| Wrong baud rate | Reflash both with matching firmware |
| EMI from motors | Route UART away from motor wires, add ferrite |

---

## Steering/Hall Sensor Issues

### Symptoms
- No angle reading in `:dump` output
- `axle_deg=0.0` always
- HID joystick X-axis stuck at center
- `:selftest` reports hall failure

### Expected Serial Output

**Healthy:**
```
OK selftest
hall_ok=1 angle=123.4 index=0
T axle=123.4 steer=342.1 rpm=0 ...
```

**Failed:**
```
FAIL selftest hall
hall_ok=0 angle=0.0 index=0
```

### Diagnostic Steps

**1. Verify Wiring (MLX90363 to Base)**

Standard wire colors (confirm with your sensor board):
```
RED    → Pico VBUS (5V) or VSYS
BLACK  → GND
BLUE   → GP16 (MISO)
YELLOW → GP17 (SS/CS)
ORANGE → GP18 (SCLK)
GREEN  → GP19 (MOSI)
```

**Measure voltages:**
- VCC at sensor: 4.9 - 5.1V
- Logic pins (MISO/MOSI/SCLK/SS) at idle: 0V - 3.3V
- SS should be HIGH (3.3V) when idle

**2. Check Continuity**
- Sensor VCC to Pico pin 40 (VBUS): < 1Ω
- Sensor GND to Pico GND: < 1Ω
- Each data line sensor → Pico: < 10Ω

**3. Logic Analyzer Capture (Advanced)**

If you have a logic analyzer:
- Capture SCLK, MOSI, MISO, SS pins
- Expected: SPI transactions at 500 kHz every control loop
- SS goes LOW, 16-bit transfers, SS goes HIGH
- MISO should show response data (not all zeros)

**4. Sensor Alignment**

The MLX90363 reads the G920/G923 magnetic rotary encoder:
- Sensor PCB should be ~1-2mm from target magnet ring
- Parallel alignment critical
- Check for mechanical interference

**5. Test Index Magnet**

Separate from the MLX sensor, the axle index uses a digital hall:
```
:get axle_index      # Should show 0 or 1
```

Slowly rotate axle by hand, watching value. Should toggle once per ~360° of wheel rotation.

### Solutions

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| `hall_ok=0` | Wiring or power | Check voltages, connections |
| Angle stuck at one value | SPI communication failure | Verify MISO/MOSI not swapped |
| Angle jumps randomly | EMI or loose connection | Shield cable, secure connectors |
| Angle reads but wrong | Wrong `gear_ratio` | Run gear calibration (press 'c') |
| Index never triggers | Magnet too far or wrong polarity | Adjust magnet position |

**Run Gear Ratio Calibration:**
```
Serial: c                          # Start cal
<Rotate wheel exactly 360° slowly>
Serial: c                          # Finish cal

Expected output:
Gear CAL ok, ratio=18.1234
```

Then `:set gear_ratio 18.1234` and `:save`.

---

## Motor Not Responding

### Symptoms
- FFB commands do nothing
- Motor doesn't move during INIT
- `:selftest` passes but no torque

### Safety First

⚠️  **NEVER apply full power without testing!**
- Start with `duty_cap=0.10` (10%)
- Gradually increase after verifying direction
- Be ready to disable motors (press 'd')

### Diagnostic Commands

```
:dump              # Check motor settings
:get duty_cap      # Should be > 0
:get ffb_mode      # Check current mode

e                  # Enable motors (careful!)
s                  # Spring mode
x                  # Disable motors / coast
```

### Physical Checks

**1. Motor Driver Wiring (BTS7960)**

Each motor has one BTS7960 driver:
```
Logic Side (to Pico 3.3V):
  VCC  → Pico 3V3
  GND  → Pico GND
  RPWM → GP10 (motor 1) or GP13 (motor 2)
  LPWM → GP11 (motor 1) or GP14 (motor 2)
  R_EN ┐
  L_EN ┴→ GP12 (motor 1) or GP15 (motor 2)

Motor Side (HIGH VOLTAGE!):
  B+  → Separate 24V PSU positive (stock G920/G923 PSU)
  B-  → Separate PSU negative
  M+  → Motor red wire
  M-  → Motor black wire
```

**CRITICAL:** Motor power supply must be ISOLATED from Pico logic!

**2. Voltage Checks**

With motors disabled:
- BTS7960 VCC: 3.2 - 3.4V (from Pico)
- BTS7960 B+: 24V stock (from G920/G923 PSU or aftermarket)
- Pico VBUS: 4.8 - 5.2V (USB)

**3. Test Motor Directly (Advanced)**

Bypass firmware temporarily:
1. Disconnect motor from BTS7960
2. Apply 5V DC directly to motor terminals
3. **Expected:** Motor spins, draws < 2A
4. **If not:** Motor itself is faulty

### Common Issues

| Problem | Cause | Fix |
|---------|-------|-----|
| No movement at all | EN pin not high | Check wiring, measure EN pin |
| Motor hums but doesn't turn | Phase wires swapped | Swap M+ and M- |
| Stuttering / cogging | PWM frequency wrong | Verify firmware PWM setup |
| Motor runs full speed | PWM stuck high | GPIO short or driver fault |
| Overheating | No heatsink or inadequate PSU | Add heatsink, check PSU current |
| Inconsistent torque | Loose connections | Secure all screw terminals |
| `motor_fault=ACTIVE` (MC33926 bench) | Overcurrent / thermal latch | Lower `duty_cap`, fix shorts, then `:motor_fault_clear` |
| Enable blocked after fault | Latched SF line still LOW | `:motor_fault_clear` or `e` after fixing cause |

### MC33926 bench build (Pololu shield)

If you flashed `pico-mc33926` firmware, wiring uses **DIR+PWM** on GP10–14 and **D2/SF** on GP12/GP15 — not BTS7960 RPWM/LPWM/EN. See [Motor Driver Details — MC33926](wiring-diagrams.md#pololu-dual-mc33926-bench-build).

**Fault recovery sequence:**
```
d                      # Disable motors
:set duty_cap 0.10     # Reduce load
:motor_fault_clear     # Toggle D2 to clear latched fault
:selftest              # motor_fault=ok
e                      # Re-enable when safe
```

Firmware prints `!!! MOTOR WATCHDOG: Driver fault — emergency stop!` when SF goes low during operation.

### Software Configuration

**1. Check Settings**
```
:get duty_cap       # Must be > 0 (try 0.15 for testing)
:get spring_k       # Try 0.004 initially
:get torque_cap     # Try 0.35 initially
```

**2. Test Sequence**
```
# Start safe
:set duty_cap 0.10
:save

e                   # Enable motors
s                   # Spring mode

# Turn wheel - should feel resistance
# If nothing: increase duty
:set duty_cap 0.15

# If working, gradually increase
:set duty_cap 0.25
```

**3. Direction Calibration**

If motor fights you instead of assisting:
```
# Check FFB sign in firmware
# May need to invert motor polarity in code
```

### Recovery

If motors behave dangerously:
```
d                   # EMERGENCY STOP
:set duty_cap 0.10  # Lower power
:save
:defaults           # Reset everything
```

---

## Pedals Not Working

### Symptoms
- HID shows 0% on throttle/brake/clutch
- `:dump` shows `thr_rest=0` or invalid values
- Pedals stick at 100% or 0%

### Expected Behavior

When pedals are connected and calibrated:
```
T ... thr=512 brk=234 clu=67 ...

:dump shows:
thr_rest=2890  thr_min=450  thr_max=3200
brk_rest=2950  brk_min=520  brk_max=3150
clu_rest=3100  clu_min=1200 clu_max=3050
```

HID axes (0-1023):
- Unpressed: near 0
- Fully pressed: near 1023

### Diagnostic Steps

**1. Verify Pedal Connection**

G29/G920/G923 pedal DE-9 pinout:
```
Looking at male connector on pedal cable:
  1  GND       6  VCC
  2  Throttle  7  (unused)
  3  Brake     8  (unused)
  4  Clutch    9  VCC
  5  (unused)
```

**Connect to Base Pico:**
```
DE-9 Pin 1,5 → Pico GND
DE-9 Pin 6,9 → Pico 3V3 (NOT 5V!)
DE-9 Pin 2   → Pico GP26 (ADC0)
DE-9 Pin 3   → Pico GP27 (ADC1)
DE-9 Pin 4   → Pico GP28 (ADC2)
```

**2. Voltage Checks**

With pedals connected but NOT pressed:
- Pedal VCC (DE-9 pins 6/9): 3.2 - 3.4V
- Throttle pin (GP26): 2.8 - 3.3V (high at rest)
- Brake pin (GP27): 2.8 - 3.3V
- Clutch pin (GP28): 2.8 - 3.3V

Press each pedal fully:
- Voltage should DROP to 0.3 - 1.0V

**If voltage doesn't change:** Wiring or pedal fault.

**3. Raw ADC Values**

Check raw readings in serial output:
```
T ... thr=2890 brk=2950 clu=3100 ...
```

12-bit ADC (0-4095):
- Unpressed (rest): 2800 - 3200
- Fully pressed: 400 - 1000

**4. Reset Calibration**

```
p                  # Reset pedal cal
```

Then press each pedal fully once to re-learn min/max.

### Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| All pedals show 0 | VCC not connected | Check 3V3 supply to pedals (must be 3.3V for ADC safety) |
| Pedals inverted (0% when pressed) | Normal! | Firmware expects this |
| One pedal dead | Broken wire | Check continuity |
| Erratic readings | EMI from motors | Route pedal cable away from motor power |
| Values drift | Bad connection | Secure connector |
| Calibration won't save | EEPROM issue | Run `:save` after pressing 'p' |

### Calibration Procedure

```
1. Connect pedals
2. Boot system
3. Serial: p            # Reset cal
4. Wait for "Pedal CAL reset" message
5. Press throttle FULLY once
6. Press brake FULLY once  
7. Press clutch FULLY once
8. Serial: :save        # Persist calibration
9. Test in jstest or game
```

After cal, `:dump` should show realistic min/max/rest values around 400/3200/2900.

---

## Rim Buttons/Encoders Issues

### Symptoms
- Buttons not registering in HID
- Encoders don't produce pulses
- LEDs on button panel don't light
- I²C devices missing

### Expected Output

```
:dump shows:
rim_link=1
mcp_btn=1     # MCP23017 @ 0x20 (switches) detected
mcp_led=1     # MCP23017 @ 0x21 (LEDs) detected  
ads=1         # ADS1115 @ 0x48 detected
```

HID buttons 1-10 correspond to panel switches.

### Diagnostic Steps

**1. Verify I²C Bus**

Check rim serial output (connect rim directly to USB):
```
OK boot
I2C scan: 0x20 0x21 0x48 0x53
MCP btn OK
MCP led OK
ADS OK
ADXL OK (optional)
```

If devices missing:
- Check I²C wiring (SDA/SCL to GP4/GP5)
- Verify pullup resistors (usually on module)
- Check device addresses (solder jumpers)

**2. I²C Address Configuration**

MCP23017 modules usually have A0/A1/A2 pads:
```
For 0x20: All pads OPEN
For 0x21: Solder A0 closed
```

ADS1115 typically defaults to 0x48 (check module markings).

**3. Test Button Reads**

With rim connected to base and ffb-config open:
- Press each button 1-10
- Watch HID status in ffb-config or jstest
- **Expected:** Corresponding button number lights up

**4. Test Encoder Rotation**

Turn each encoder (0-3):
- CW: HID button pairs (11/13/15/17) pulse
- CCW: HID button pairs (12/14/16/18) pulse

### Encoder Configuration

Encoders support multiple modes (`:set encN_mode <0|1|2>`):

**Mode 0 - Relative (default):**
- CW/CCW pulses sent to HID
- Suitable for menu navigation

**Mode 1 - Hold:**
- Button held while turning
- Released after idle time

**Mode 2 - Absolute:**
- Internal value 0-100
- For volume sliders, etc.

Test encoder 0:
```
:set enc0_mode 0      # Relative pulses
:set enc0_steps 1     # 1 detent = 1 click
:rim_save             # Persist on rim
```

### LED Panel Control

Test panel LEDs:
```
:btnleds 0x3FF        # All 10 LEDs on (bits 0-9)
:btnleds 0x001        # Only LED 1 on
:btnleds 0x000        # All off
```

Auto-follow mode (LED tracks button state):
```
:btnleds auto 1       # LED mirrors button press
:btnleds auto 0       # Manual control
```

### Common Issues

| Problem | Cause | Fix |
|---------|-------|-----|
| All I²C devices missing | SDA/SCL not connected | Check GP4/GP5 wiring |
| One device missing | Wrong address | Verify solder jumpers |
| Buttons intermittent | Loose connector | Secure panel cable |
| Encoders don't work | Wrong pin mapping | Check GP6-13 wiring |
| LEDs don't light | Wrong MCP or no power | Check 0x21, verify VCC |
| Encoders reversed | Need invert | `:set encN_invert 1` |

---

## LED Strip Problems

### Symptoms
- WS2812 strip doesn't light
- Wrong colors
- Strip flickers
- Only first few LEDs work

### Expected Behavior

**Boot sequence:**
- Rainbow sweep
- Center LED blue blink (idle)
- Responds to telemetry RPM

### Diagnostic Commands

```
:leds_boot         # Replay startup animation
:leds_solid 255 0 0    # All red
:leds_fill 5 0 255 0   # First 5 green
:leds_off          # Turn off
:leds_auto         # Resume telemetry mode
```

### Physical Checks

**1. WS2812 Wiring**
```
Strip     Rim Pico
VCC   →   VBUS (5V) or external 5V
GND   →   GND
DIN   →   GP14 (through 330Ω resistor recommended)
```

**Current draw:** ~50-60mA per LED at full white
- 11 LEDs × 60mA = 660mA max
- USB 5V can handle this, but use external supply for longer strips

**2. Voltage at Strip**
- VCC: 4.5 - 5.5V
- DIN: 0V idle, 3.3V pulses during updates

**3. Series Resistor**

Add 330Ω resistor between GP14 and strip DIN:
- Protects first LED from voltage spikes
- Improves signal integrity

**4. LED Count Configuration**

```
:get shift_led_count   # Should match your strip (default 11)

If different:
:set shift_led_count 16   # For 16-LED strip
:rim_save
```

### Common Issues

| Problem | Cause | Fix |
|---------|-------|-----|
| No LEDs light | Power or data issue | Check VCC and GND first |
| First LED works, rest don't | Power insufficient | Add external 5V supply |
| Wrong colors | GRB vs RGB order | Firmware assumes GRB (WS2812B) |
| Flickers | Data line too long | Keep under 1m, add resistor |
| Random colors | EMI from motors | Route strip away from motor wires |
| Stuck on boot colors | Firmware crashed | Reflash rim firmware |

### Data Line Troubleshooting

If strip is erratic:
1. Keep data line SHORT (<1m)
2. Use shielded cable for longer runs
3. Add 330-470Ω resistor at DIN
4. Add 1000µF capacitor across VCC/GND near strip
5. Lower brightness: `:set shift_led_bright 40`

### Brightness Settings

```
:get shift_led_bright     # 0-255, default 60
:get panel_led_bright     # Panel button LEDs

# Reduce for lower current draw:
:set shift_led_bright 30
:set panel_led_bright 60
:rim_save
```

---

## Firmware Update Failures

### Normal Update Process (GUI)

**Recommended method:** Use the `ffb-config` desktop GUI

1. Download firmware pack (.zip) from releases
2. Open ffb-config GUI → "Firmware Update" tab
3. Select firmware pack file
4. Click "Flash Both MCUs"
5. GUI automatically:
   - Flashes rim via OTA (over UART from base)
   - Flashes base via BOOTSEL or USB serial
   - Verifies both successfully
   - Shows progress for each step

**This is the easiest method** - handles both Picos in one operation with automatic version matching.

**If GUI update succeeds:** You're done! No troubleshooting needed.

**If GUI update fails:** See troubleshooting sections below.

---

### OTA Update (Rim)

**Symptoms:**
- GUI says "Update failed"
- Orange middle LED stays on (soft updater)
- Rim doesn't respond after OTA
- Download hits 100% then never finishes / stays orange

**What orange means:** Rim is in the soft OTA updater (or parked right after staging). Chunk download finishing is not the end — rim must CRC, write upper flash, send `FwDone`, reboot, then copy into slot 0.

**If download reached 100% and LED stays orange:**
1. Power-cycle the **rim** Pico (or send `:rim_reset` if base GP2→rim RUN is wired).
2. A staged image often applies on that reboot even when soft-reset hung — then check `:version` / `rim_fw`.
3. Retry pack OTA once the rim is running again.

**Recovery:**

1. **Check base serial output:**
```
OK FW 41          # UpdaterReady
OK FW 44 …        # FwAck during transfer
OK FW 47          # FwDone — staging OK; rim should reboot
OK FW 48 <reason> # FwFail — see link-protocol.md
```

2. **Manual rim reset:**
```
:rim_reset         # Force rim reboot (needs GP2→RUN)
```

3. **Direct rim USB flash (bypasses OTA):**
```
# Connect rim directly to PC via USB
pio run -d firmware-rim -t upload
```

4. **BOOTSEL recovery:**
```
# If rim completely bricked:
- Disconnect rim from base
- Hold BOOTSEL on rim Pico
- Connect rim to PC USB
- Copy firmware-rim/.pio/build/pico/firmware.uf2 to RPI-RP2 drive
```

### Base Update Issues

**Symptoms:**
- UF2 copy fails
- BOOTSEL drive doesn't appear
- Update hangs at 45s

**Recovery:**

1. **Force BOOTSEL from base CDC:**
```
:bootsel           # Enters BOOTSEL mode

# RPI-RP2 drive should appear in ~3 seconds
# If not, manual BOOTSEL:
```

2. **Manual BOOTSEL:**
```
- Disconnect base USB
- Hold BOOTSEL button
- Connect USB while holding
- Copy base.uf2 to RPI-RP2 drive
```

3. **Firmware compatibility check:**

After update, verify versions match:
```
:version

Expected:
base_fw=20240912 2024-09-12T14:30:00Z
rim_fw=20240912 2024-09-12T14:30:00Z

If mismatch: update both again
```

### Pack Update (Both MCUs)

**Process:**
1. GUI flashes rim via OTA
2. Base enters BOOTSEL
3. User copies base.uf2

**If stuck at step 2:**
- Timeout is 45s
- Unzip pack manually
- Copy `base.uf2` to BOOTSEL drive by hand

**If rim OTA fails first:**
- Base serial shows error
- Click "Flash rim only" to retry
- Or flash rim directly via USB

---

## EEPROM/Settings Corruption

### Symptoms
- `:dump` shows garbage values
- Settings don't persist after `:save`
- Boot fails with error message
- Weird FFB behavior after power cycle

### Detection

Serial output on corrupt EEPROM:
```
EEPROM CORRUPT: magic mismatch
Loading defaults...
```

Or specific field errors:
```
EEPROM CORRUPT: duty_cap out of range
EEPROM CORRUPT: gear_ratio impossible value
```

### Recovery Procedure

**1. Load defaults:**
```
:defaults          # Reset to factory settings
:save              # Write to EEPROM
```

**2. Verify write succeeded:**
```
:dump              # Check values
# Power cycle
:dump              # Should match previous dump
```

**3. If EEPROM still fails:**

May indicate hardware EEPROM failure:
```
# Check EEPROM size in firmware
# RP2040 Flash: emulated EEPROM OK up to 4KB

# Reflash firmware to reset EEPROM area:
pio run -d firmware-base -t upload
```

### Prevent Corruption

**Best practices:**
- Always `:save` after changing settings
- Don't power off during `:save` operation
- Use `:dump` to backup settings (copy to file)
- Test new settings before saving

### Manual EEPROM Clear

If firmware won't boot due to corrupt EEPROM:

1. Flash blank firmware that clears EEPROM
2. Or flash any other RP2040 project
3. Then reflash rp2040-ffb firmware

---

## Recovery Procedures

### Complete Firmware Recovery

**Both MCUs unresponsive:**

1. **Base recovery:**
```bash
# Hold BOOTSEL, connect USB
# Copy known-good base.uf2
pio run -d firmware-base
# Copy .pio/build/pico/firmware.uf2 to RPI-RP2
```

2. **Rim recovery:**
```bash
# Disconnect rim from base
# Hold BOOTSEL, connect rim to USB
pio run -d firmware-rim
# Copy .pio/build/pico/firmware.uf2 to RPI-RP2
```

3. **Verify:**
```
# Connect base to PC
# Open serial 115200
# Press 'h' for help

Expected: help menu appears
```

### Factory Reset

Complete reset to defaults:

**Base:**
```
:defaults          # Load factory defaults
:save              # Persist to EEPROM
p                  # Reset pedal calibration
```

**Rim:**
```
:rim_reset         # Reboot rim
:rim_sync          # Pull fresh config
:rim_save          # Save defaults to rim EEPROM
```

**Verify:**
```
:dump              # Should show conservative defaults
duty_cap=0.35
spring_k=0.004
torque_cap=0.35
...
```

### Hardware Reset (Physical)

**Full power cycle:**
1. Disconnect USB from base
2. Disconnect rim from base (UART)
3. Wait 10 seconds
4. Reconnect rim to base (UART/power)
5. Reconnect base USB
6. Wait for boot sequence

**BOOTSEL button locations:**
- Base: On Pico board, marked "BOOTSEL"
- Rim: On Pico board
- Base can trigger rim BOOTSEL: `:rim_bootsel` (if GP3 wired)

### Emergency Contacts

If you're truly stuck:

1. **Check GitHub Issues:** https://github.com/skyne/rp2040-ffb/issues
2. **Serial output:** Copy full serial log when reporting issues
3. **Include:** `:dump`, `:version`, `:selftest` output
4. **Multimeter readings:** Voltages help diagnose hardware

---

## Appendix: Serial Output Reference

### Normal Boot Sequence

```
rp2040-ffb base v0.1.0
Hall init...
MLX90363 OK
Index init...
Pedals init...
Motor init...
Rim UART 460800
Status LEDs init...
EPD init...
HID init...

Boot INIT: Manual zero at axle=0.0
Press 'h' for help

OK selftest
hall_ok=1 angle=123.4 index=0
pedals: thr=1 brk=1 clu=1
rim_link=0 (probing...)
OK link 1 pong/s
rim_fw=20240912 2024-09-12T14:30:00Z
mcp_btn=1 mcp_led=1 ads=1

T axle=123.4 steer=0.0 rpm=0 gear=0 flags=0 thr=2890 brk=2950 clu=3100
```

### Error Patterns

**Hall failure:**
```
Hall init...
FAIL MLX90363 no response
hall_ok=0
```

**Link timeout:**
```
OK link 0 pong/s
WARN link timeout (no pong)
rim_link=0
```

**Motor emergency stop:**
```
!!! MOTOR WATCHDOG: Thermal timeout - forcing cooldown!
FFB off
```

**EEPROM corruption:**
```
EEPROM CORRUPT: duty_cap out of range
Loading defaults...
```

---

## LED Status Codes

### Base Status Ring (HW-159)

- **Boot:** Rainbow sweep
- **Idle:** Blue rotating pulse
- **Active:** Green center
- **Error:** Red flashing
- **INIT:** Yellow scanning
- **USB disconnect:** All off

### Rim Shift Strip (WS2812)

- **Boot:** Rainbow chase
- **Link down:** Blue center blink
- **Telemetry active:** RPM bar + flags
- **OTA update:** Orange middle LED steady
- **Error:** Red flashing

### E-Paper Display (Base, if equipped)

- **Boot:** Shows firmware version, status
- **Running:** Axle angle, FFB mode, link status
- **Error:** Displays error message

---

*Last updated: 2024-09-12*
*Firmware version: v0.1.0*
