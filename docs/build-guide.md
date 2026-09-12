# Complete Build Guide

**Step-by-step guide to building your own rp2040-ffb force feedback wheel**

This guide walks you through the entire build process from ordering parts to your first working session.

---

## Table of Contents

1. [Before You Start](#before-you-start)
2. [Ordering Parts](#ordering-parts)
3. [Required Tools](#required-tools)
4. [Build Stages](#build-stages)
5. [Assembly Steps](#assembly-steps)
6. [First Power-On](#first-power-on)
7. [Calibration](#calibration)
8. [Common Mistakes](#common-mistakes)

---

## Before You Start

### Skill Level

**Recommended skills:**
- ✅ Basic soldering (through-hole components)
- ✅ Wire crimping / connector assembly
- ✅ Multimeter usage
- ✅ PC terminal / serial console
- ⚠️ Understanding DC voltage safety
- ⚠️ Comfort with firmware flashing

**Time investment:**
- Electronics assembly: 8-12 hours
- Mechanical assembly: 4-6 hours
- Testing & calibration: 2-4 hours
- **Total: 15-25 hours**

### What You'll Build

A complete force feedback steering wheel with:
- Hall effect steering sensor (no potentiometer wear)
- Dual motor force feedback
- Logitech pedals support
- Button panel with 10 buttons + 4 encoders
- LED shift lights
- Optional TFT display
- USB HID (plug & play)

### Project Cost Estimate

| Category | Estimated Cost (USD) |
|----------|---------------------|
| Donor wheel base (G920/G923) | $150-300 (used) |
| Electronics (PCBs, sensors, modules) | $80-120 |
| Wiring & connectors | $20-40 |
| Power supply | $0-50 | Can reuse stock G920/G923 24V PSU or buy new |
| Buttons & switches | $30-50 |
| Miscellaneous (screws, standoffs) | $20-30 |
| **Total** | **$330-590** |

**Note:** Using a donor G920/G923 significantly reduces cost vs. building from scratch.

**G29 users:** Your wheel is identical to G920/G923 (uses MLX90363 hall sensor). Follow the G920/G923 instructions!

**G25/G27/DFGT users:** These wheels work great! Use the stock optical encoder (firmware modifications required). Cost is slightly less (no MLX90363 to buy). Optional: upgrade to MLX90363 hall sensor (see BOM note below). DFGT is often the cheapest option ($80-150 used). See the [FAQ G25/G27/DFGT section](faq.md#g25g27dfgt-compatibility) for details.

---

## Ordering Parts

### Core Electronics BOM

Download the latest BOM: [Coming Soon - hardware/BOM.csv]

**Where to buy:**
- **AliExpress:** Cheapest for modules (MCP23017, ADS1115, WS2812)
- **Amazon:** Faster shipping for common items (Picos, wire, connectors)
- **eBay:** Good for used Logitech wheels
- **Adafruit/SparkFun:** Quality modules with documentation
- **LCSC/JLCPCB:** If ordering custom PCBs

### Critical Components

#### Must-Haves (Project won't work without these):

| Part | Qty | Where to Buy | Notes |
|------|-----|--------------|-------|
| Raspberry Pi Pico | 2 | Amazon, Adafruit | Get genuine ones, not clones |
| MLX90363 board | 0 or 1 | eBay, AliExpress | **G29/G920/G923: Already included (don't buy!)** **G25/G27/DFGT: Optional upgrade** (stock optical encoder works fine, see note below) |
| BTS7960 / IBT-2 H-bridge | 2 | Amazon, AliExpress | Look for heatsinks included |
| G920/G923 wheel base | 1 | eBay, Facebook Marketplace | Test motors before buying! |
| Digital hall sensor (A3144) | 0 or 1 | Amazon, AliExpress | **Optional:** Only if removing endstop |
| M2 heatset insert | 0 or 1 | Amazon, AliExpress | **Optional:** Only if removing endstop |
| Ring magnet 6×2mm (2mm hole) | 0 or 1 | Amazon, AliExpress | **Optional:** Only if removing endstop |
| Disc magnet 6×2mm | 0 or 1 | Amazon, AliExpress | **Optional:** Only if removing endstop |
| 2-part epoxy | 0 or 1 | Hardware store | **Optional:** Only if removing endstop |
| M2 × 6mm screw | 0 or 1 | Hardware store | **Optional:** Only if removing endstop |

**📝 Note on Index/Homing Sensors:**
- **If you remove the endstop, choose ONE homing method:**
  - **Option A: Index hall sensor (A3144 + magnets)** - Reliable, hardware-based
  - **Option B: ADXL345 accelerometer** - Gravity-based homing (can replace hall sensor)
- **If you keep the endstop:** Skip both (endstop provides mechanical limit)
- All approaches work! Keeping endstop = simplest build, no functional difference for FFB

| MCP23017 I²C module | 2 | AliExpress | "MCP23017 slim module" |
| ADS1115 ADC module | 1 | Amazon, AliExpress | 16-bit ADC |

**📝 Note on MLX90363 Angle Sensor:**
- **G29/G920/G923 users:** The MLX90363 is already built into your donor wheel. **Do NOT buy one!** You'll reuse the stock sensor.
- **G25/G27/DFGT users:** Your wheels use optical encoders (no purchase needed). The encoder works great!
  - **Optional upgrade to MLX90363:** If you want magnetic sensing instead, you need:
    - MLX90363 board (~$15-20)
    - Ring magnet (6mm diameter)
    - Custom mounting bracket
    - Firmware modification
    - See: [G25 Hall Sensor Mod Guide](https://www.racedepartment.com/threads/logitech-g25-g27-hall-sensor-mod.145782/) or search "G25 hall encoder mod"
  - **Recommendation:** Stick with stock optical encoder unless you have specific needs

#### Recommended (Better experience):

| Part | Qty | Where to Buy | Notes |
|------|-----|--------------|-------|
| WS2812B LED strip | 1 (11 LEDs) | Amazon | Shift lights |
| ILI9341 TFT display | 1 | Amazon, AliExpress | 2.4" or 2.8" |
| EC12 rotary encoders | 4 | Amazon, AliExpress | Menu navigation |
| Illuminated buttons (TSD-1166) | 10 | AliExpress | LED feedback |
| E-paper display 3.7" | 1 | AliExpress | Base status (WeAct Studio) |
| ADXL345 accelerometer | 1 | Amazon | Gravity-based homing (can replace index sensor) |

#### Power & Wiring:

| Part | Qty | Notes |
|------|-----|-------|
| 24V power supply | 1 | Stock G920/G923 PSU works! Or 5A+ aftermarket |
| 22 AWG wire | 10m | Mix of colors |
| Dupont connectors | 50+ | 2.54mm pitch |
| Heat shrink tubing | Assorted | Multiple sizes |
| JST-XH connectors | 10+ sets | For panel connections |
| Micro-USB cables | 2 | For Pico programming |

### Shopping List Templates

**Budget Build (~$350):**
- Used G920 or G923 wheel base (includes MLX90363 sensor stock)
- 2× Pico, 2× BTS7960
- Basic MCP23017 + button panel
- Skip: TFT display, WS2812, E-paper

**Full-Featured Build (~$550):**
- G920 or G923 wheel base (functionally identical for this project)
- All electronics from BOM
- TFT display + WS2812 + E-paper
- Quality buttons & encoders
- Proper PCB enclosures

---

## Required Tools

### Essential:

- ✅ Soldering iron (temperature controlled, 30-60W)
- ✅ Solder (leaded or lead-free)
- ✅ Wire strippers
- ✅ Crimping tool (dupont/JST)
- ✅ Multimeter
- ✅ Small screwdrivers (Phillips, Torx)
- ✅ Flush cutters
- ✅ Helping hands / PCB holder

### Recommended:

- 🔧 Heat gun (for heat shrink)
- 🔧 Label maker
- 🔧 Continuity tester / cable tester
- 🔧 Logic analyzer (debugging)
- 🔧 USB serial adapter (3.3V)
- 🔧 Breadboard (for testing modules)

### Safety Equipment:

- ⚠️ Safety glasses (for cutting/drilling)
- ⚠️ ESD wrist strap (optional but good practice)
- ⚠️ Ventilation for soldering

---

## Build Stages

Build in stages to isolate problems:

### Stage 1: Base Electronics Breadboard (2-3 hours)

**Goal:** Verify all components work before permanent wiring

**What to test:**
1. Both Picos flash and respond to serial
2. Pedals read correctly on ADC
3. Optional: Test any external I²C/SPI modules (MCP23017, displays)

**Note:** Angle sensor (MLX90363/encoder) is already in donor wheel—test after disassembly and wiring.

**Stop if:** Any component doesn't respond

### Stage 2: Motor Test Bench (2-4 hours)

**Goal:** Verify motor driver safety before connecting to wheel

**What to test:**
1. BTS7960 enable signals work
2. PWM generates correct duty cycle
3. Motors spin in both directions
4. Emergency stop works
5. No smoke or unusual heat

**Stop if:** Motors don't respond or drivers overheat

### Stage 3: Wheel Disassembly (2-4 hours)

**Goal:** Extract motors, gears, and angle sensor from donor wheel

**What to do:**
1. Disassemble donor wheel (G29/G920/G923 or G25/G27/DFGT)
2. Extract motors with attached gears
3. Remove MLX90363 sensor board (G29/G920/G923) or note encoder location (G25/G27/DFGT)
4. Keep all mounting hardware and brackets
5. Document original wiring/connector pinouts

**Stop if:** You damage motors or angle sensor during removal

### Stage 4: Base MCU Assembly (4-6 hours)

**Goal:** Complete base Pico with all sensors

**What to build:**
1. Wire stock angle sensor to Pico (MLX90363 from G29/G920/G923, or encoder from G25/G27/DFGT)
2. Index magnet & digital hall (or endstop for optical wheels)
3. Pedal DE-9 connector
4. Motor driver connections
5. Status LEDs (optional)
6. E-paper display (optional)

**Stop if:** Firmware won't upload or sensors don't read

### Stage 5: Rim MCU Assembly (4-6 hours)

**Goal:** Complete rim Pico with buttons & encoders

**What to build:**
1. MCP23017 button matrix
2. ADS1115 paddle inputs
3. Rotary encoders
4. WS2812 LED strip (optional)
5. TFT display (optional)

**Stop if:** I²C devices don't enumerate

### Stage 6: Integration (2-3 hours)

**Goal:** Connect base + rim, test UART link

**What to test:**
1. UART communication at 460800 baud
2. Button presses appear in HID
3. Encoders generate pulses
4. LED strip responds to telemetry

### Stage 7: Mechanical Installation (4-6 hours)

**Goal:** Install electronics into wheel base

**What to do:**
1. **Choose endstop approach:**
   - Remove for index magnet/sensor (full flexibility)
   - Keep for simpler build (no index sensor needed)
2. If removed: Install index magnet on axle (G29/G920/G923) or configure encoder index (G25/G27/DFGT)
3. Verify angle sensor mounting (stock MLX90363 or optical encoder)
4. Route wiring safely
5. Secure motor drivers with heatsinks

### Stage 8: Calibration & Testing (2-4 hours)

**Goal:** Tune settings for smooth operation

**What to calibrate:**
1. Gear ratio measurement
2. Pedal min/max/rest values
3. FFB spring constants
4. Duty cap limits
5. Encoder detent counts

---

## Assembly Steps

### Step 1: Prepare the Donor Wheel

**Disassemble G920/G923:**

1. **Unplug everything**
   - Power cable
   - USB cable
   - Pedals

2. **Remove bottom cover:**
   - 6 screws (T10 Torx)
   - Carefully separate clips on sides
   - Keep screws organized

3. **Locate key components:**
   - Dual motors (should spin freely)
   - Optical encoder (leave this alone!)
   - Plastic endstop arm (optional: remove or keep)
   - Main gear (18:1 ratio)

4. **Choose endstop configuration:**

   **Option A: Remove endstop + add index sensor (flexible)**
   - Remove 2 screws holding endstop
   - Gently lift out plastic piece
   - Axle rotates 360° freely
   - Continue to step 5 (install index magnet)
   
   **Option B: Keep endstop (simpler)**
   - Leave endstop in place
   - Skip index magnet installation
   - Skip index hall sensor wiring
   - 900° hard stop remains
   - Slightly higher resistance but works great
   - Skip to motor driver wiring

5. **Install index magnet (if endstop removed):**

   **Recommended method (secure mounting):**
   
   The G920/G923 has a plastic blob on the main gear that originally drove the endstop switch. This is the perfect location for the index magnet.
   
   ```
   Materials needed:
   - M2 heatset insert
   - Ring magnet with 2mm center hole (6mm OD × 2mm thick)
   - Second magnet (6mm × 2mm, solid)
   - 2-part epoxy
   - M2 × 6mm screw
   
   Procedure:
   1. Locate the plastic mounting boss on main gear (where endstop arm attached)
   2. Install M2 heatset insert into the plastic boss:
      - Use soldering iron at 200-220°C
      - Press insert straight and level
      - Let cool completely
   
   3. Attach first magnet (ring type):
      - Thread M2 screw through magnet's center hole
      - Screw into heatset insert
      - Tighten until magnet sits flat against plastic
   
   4. Stack second magnet on top:
      - Align with first magnet (they will snap together)
      - Apply 2-part epoxy between magnets
      - Clamp or hold for 5 minutes
      - Let cure 24 hours before use
   
   5. Note the angle for calibration:
      - Mark wheel position when perfectly straight
      - This angle becomes AXLE_INDEX_ANGLE_DEG
   ```
   
   **Alternative method (if no heatset insert):**
   ```
   - Single 6×2mm neodymium magnet
   - Cyanoacrylate (super glue)
   - Glue to plastic boss on main gear
   - Position 5-10mm from gear edge
   ```
   
   **Why stack two magnets:**
   - Stronger magnetic field (better detection range)
   - Screw-mount is removable/replaceable
   - Epoxy top magnet prevents it from flying off
   - More reliable than single glued magnet

6. **Test motors manually:**
   - Apply 5V to motor terminals (brief!)
   - Both motors should spin smoothly
   - No grinding or clicking sounds

**⚠️ SAFETY:** Never feed motor voltage (24V or higher) into Pico GPIO pins!

### Step 2: Base Electronics Breadboard

**Angle Sensor:**

**For G29/G920/G923 users:**
- The MLX90363 is already installed in your donor wheel (stock sensor)
- You'll wire it to the Pico after disassembly
- No need to test it separately—it's already working!
- Skip to the next component (Index Hall Sensor below)

**For G25/G27/DFGT users:**
- You're using the stock optical encoder instead
- Wire to GP16/GP17 per encoder pinout after disassembly
- See [G25/G27/DFGT Compatibility](faq.md#g25g27dfgt-compatibility)

**MLX90363 Pinout Reference (G920/G923, for later wiring):**

```
MLX Board → Pico Base
─────────────────────
RED    (5V)  → VBUS (pin 40)
BLACK  (GND) → GND
BLUE  (MISO) → GP16
YELLOW  (SS) → GP17
ORANGE (SCLK)→ GP18
GREEN (MOSI) → GP19
```

**Index Hall Sensor:**

```
A3144 Hall → Pico Base
──────────────────────
VCC → 3V3
GND → GND
OUT → GP20 (with INPUT_PULLUP)
```

**Test:** Bring index magnet near → serial should show `idx=1`

**Pedals (Logitech DE-9):**

```
DE-9 Pin → Pico Base
────────────────────
1, 5     → GND
2        → GP26 (ADC0) Throttle
3        → GP27 (ADC1) Brake
4        → GP28 (ADC2) Clutch
6, 9     → 3V3 (NOT 5V!)
```

**⚠️ IMPORTANT:** Pedals MUST be powered from 3.3V to protect Pico ADC!
- Potentiometers can handle 5V, but Pico ADC is 3.3V max
- Using 3.3V ensures even at 0Ω pot resistance, ADC sees safe voltage

**Test:** Serial should show `T ... thr=2890 brk=2950 clu=3100` (unpressed)

### Step 3: Motor Driver Wiring

**BTS7960 H-Bridge (per motor):**

```
BTS7960 Module → Connections
────────────────────────────
Logic Side (3.3V):
  VCC  → Pico 3V3
  GND  → Pico GND
  RPWM → GP10 (motor 1) / GP13 (motor 2)
  LPWM → GP11 (motor 1) / GP14 (motor 2)
  R_EN ┐
  L_EN ┴→ GP12 (motor 1) / GP15 (motor 2)

Motor Side (HIGH VOLTAGE!):
  B+   → PSU positive (24V stock, or 36V experimental)
  B-   → PSU negative
  M+   → Motor red wire
  M-   → Motor black wire
```

**⚠️ CRITICAL SAFETY:**
- Motor power is SEPARATE from Pico USB power
- NEVER connect B+ or B- to Pico pins
- NEVER connect motor wires to Pico
- Use thick wire (18-22 AWG) for motor power
- Add heatsinks to BTS7960 modules
- Fuse the motor power supply (5A for 24V)
- Stock motors rated 24V (can reuse G920/G923 PSU)

**Test procedure (CAREFUL!):**

1. **Disconnect motors from BTS7960 first**
2. Flash firmware with `duty_cap=0.10` (10%)
3. Connect serial monitor
4. Send command: `e` (enable motors)
5. Measure PWM on RPWM/LPWM with multimeter
6. Should see ~3.3V pulses
7. **Only if PWM OK:** Connect motors
8. Send: `s` (spring mode)
9. Manually turn motor shaft - should resist gently
10. Send: `d` (disable) immediately if anything wrong

### Step 4: Rim Electronics

**MCP23017 I²C Expanders:**

```
MCP23017 Module → Pico Rim
──────────────────────────
VCC → 3V3
GND → GND
SDA → GP4
SCL → GP5
A0, A1, A2 → Configure addresses:
  0x20: All open (buttons)
  0x21: A0 closed (LEDs)
```

**Button Matrix (MCP @ 0x20):**

```
Buttons 1-10 connect between:
  GPA0-7, GPB0-1 (MCP inputs)
  GND

Configure MCP with pullups, active-low
```

**LED Matrix (MCP @ 0x21):**

```
LEDs 1-10 connect:
  GPA0-7, GPB0-1 (MCP outputs) → LED anode (+)
  LED cathode (-) → GND
  With 100Ω series resistor per LED
```

**ADS1115 (paddle inputs):**

```
ADS Module → Pico Rim
─────────────────────
VCC → 3V3
GND → GND
SDA → GP4 (shared with MCP)
SCL → GP5 (shared with MCP)
ADDR → GND (for 0x48)

A0 → Clutch Left hall sensor
A1 → Clutch Right hall sensor
A2 → Shifter A paddle
A3 → Shifter B paddle
```

**Rotary Encoders (EC12):**

```
Encoder 0: A→GP6,  B→GP7
Encoder 1: A→GP8,  B→GP9
Encoder 2: A→GP10, B→GP11
Encoder 3: A→GP12, B→GP13
Common: GND
```

Add 100nF capacitor across A-GND and B-GND for debouncing (optional).

**WS2812B LED Strip:**

```
Strip → Pico Rim
────────────────
VCC → VBUS (5V)
GND → GND
DIN → GP14 (with 330Ω series resistor)
```

**Test each subsystem:**

```bash
cd firmware-rim
pio run -t upload

# Serial should show:
# I2C scan: 0x20 0x21 0x48
# MCP btn OK
# MCP led OK
# ADS OK
```

### Step 5: Base ↔ Rim UART Link

**Critical connections (cross TX/RX!):**

```
Base GP0 (TX) ──────► Rim GP0 (RX)
Base GP1 (RX) ◄────── Rim GP1 (TX)
Base GND ──────────── Rim GND

Optional remote control:
Base GP2 ────────────► Rim RUN (reset)
Base GP3 ────────────► Rim BOOTSEL
```

**Cable recommendations:**
- Use twisted pair for TX/RX
- Keep under 1 meter for 460800 baud
- Shielded cable if running near motor wires
- Add ferrite beads if experiencing errors

**Test:**

```
Base serial should show:
OK link 1 pong/s
rim_link=1
rim_fw=<version>
```

### Step 6: Power Distribution

**Two separate power domains:**

```
Domain 1: Logic (3.3V / 5V USB)
─────────────────────────────
Source: Pico USB
Powers: Both Picos, sensors, LED logic
Current: ~500mA max
Cable: USB A to Micro-USB

Domain 2: Motor (24V stock)
────────────────────────
Source: Stock G920/G923 PSU (24V) or aftermarket
Powers: BTS7960 motor drivers only
Current: 5-10A peak (dual motors)
Cable: 18 AWG minimum
FUSED: 5A fuse recommended
Note: 36V experimental (requires cooling)
```

**Ground connection:**
- Motor PSU GND connects to BTS7960 B-
- BTS7960 logic GND connects to Pico GND
- Single common ground plane
- NO current path: PSU → Pico directly

---

## First Power-On

### Pre-Flight Checklist

Before applying power:

**Visual Inspection:**
- [ ] All solder joints shiny (no cold joints)
- [ ] No solder bridges between pins
- [ ] Correct polarity on all components
- [ ] No loose wires touching
- [ ] Heatsinks on BTS7960 modules
- [ ] Motor wires not near Pico pins

**Continuity Checks:**
- [ ] VCC to GND: Open circuit (no short!)
- [ ] Motor B+ to logic: Open circuit
- [ ] Each signal wire: Continuity to correct pin
- [ ] All GNDs common

**Voltage Checks (no load):**
- [ ] Pico VBUS: 5V ±0.2V
- [ ] Pico 3V3OUT: 3.3V ±0.1V
- [ ] Motor PSU: 24V (stock PSU works, or aftermarket)

### Power-On Sequence

**Step 1: USB Only (Logic Power)**

1. Connect base Pico USB
2. Watch for smoke (should be none!)
3. LED on Pico should light
4. Open serial monitor (115200 baud)
5. Should see boot messages

**Expected output:**
```
rp2040-ffb base-mcu
Hall init...
MLX90363 OK
...
Press 'h' for help
```

**Step 2: Rim Power (via UART or separate USB)**

1. Connect rim to base via UART
2. Base serial should show rim link
3. Or connect rim via USB separately

**Expected:**
```
OK link 1 pong/s
rim_link=1
rim_fw=<version>
```

**Step 3: Motor Power (CAREFUL!)**

⚠️ **DO NOT SKIP:** Set duty cap first!

```
:set duty_cap 0.10    # 10% maximum
:save
```

1. Turn on motor PSU
2. Measure BTS7960 B+: Should match PSU voltage
3. Send: `e` (enable motors)
4. No movement yet - this is OK
5. Send: `s` (spring mode)
6. Gently turn wheel - should resist
7. **If anything wrong:** Send `d` immediately!

---

## Calibration

### 1. Gear Ratio

```bash
# In serial monitor:
c                           # Start calibration
# Slowly rotate wheel exactly 360° (one full rotation)
c                           # Finish calibration

# Output: Gear CAL ok, ratio=18.1234
:set gear_ratio 18.1234
:save
```

### 2. Index Magnet Angle

```bash
# Align wheel perfectly straight (visual)
# Note the angle from serial: axle=5.2
:sim axle 5.2      # Or whatever angle shown
# Update in config.h: AXLE_INDEX_ANGLE_DEG
```

### 3. Pedals

```bash
p                  # Reset calibration

# Press throttle fully once → release
# Press brake fully once → release
# Press clutch fully once → release

:save              # Persist calibration

# Verify:
:dump
# Should show realistic min/max/rest values:
# thr_rest=2890 thr_min=450 thr_max=3200
```

### 4. FFB Tuning

Start conservative:

```bash
:set duty_cap 0.15      # 15% - very gentle
:set spring_k 0.004     # Light spring
:set torque_cap 0.15    # Match duty cap
:save

# Test spring mode
e                       # Enable motors
s                       # Spring mode
# Turn wheel - should center gently

# Gradually increase if comfortable:
:set duty_cap 0.25
:set torque_cap 0.25
```

---

## Common Mistakes

### Electrical

❌ **Feeding motor voltage into Pico**
- Stock motors use 24V (or 36V experimental)
- Pico GPIO is 3.3V only
- Connecting motor power to GPIO = dead Pico

✅ Fix: Always isolate logic (3.3V) from motor power (24V+)

❌ **Reversed TX/RX on UART**
- TX must go to RX (crossed)
- If same-to-same, link fails

✅ Fix: Base TX → Rim RX, Base RX ← Rim TX

❌ **Powering pedals from 5V**
- Pico ADC maximum input: 3.3V
- At 0Ω pot resistance, 5V would go straight to ADC → damage!
- Potentiometers themselves can handle 5V (they're just resistors)

✅ Fix: Always use Pico 3V3OUT for pedals (protects ADC)

❌ **No current limiting on LEDs**
- LEDs without resistors = overcurrent
- Can damage MCP23017 or LED

✅ Fix: 100-220Ω resistor per LED

❌ **Wrong I²C addresses**
- MCP23017 address set by A0/A1/A2 pads
- Two devices can't share address

✅ Fix: Configure 0x20 and 0x21 correctly

### Mechanical

**ℹ️ Endstop: Remove or Keep?**

**Option 1: Remove endstop (flexible)**
- Endless rotation (no physical limit)
- FFB provides soft 900° limit
- **Homing options:**
  - **1a) Index hall sensor (A3144 + magnet):** Hardware-based, reliable
  - **1b) ADXL345 accelerometer:** Gravity-based homing (experimental)
    - Detects "down" direction to find center
    - Can replace index sensor entirely
    - Requires I²C wiring and calibration
- Requires GP20 (hall) or I²C (ADXL) wiring

**Option 2: Keep endstop (simplest)**
- Physical 900° hard stop
- No index sensor needed (saves wiring)
- No accelerometer needed
- Slightly higher turning resistance
- FFB works perfectly fine
- Good for first-time builders

All options work! Choose based on your preference.

❌ **Magnet too far from hall**
- MLX90363 needs <2mm gap
- Index hall needs <5mm gap

✅ Fix: Position sensors closer

❌ **Loose wire connections**
- Vibration causes intermittent faults
- Hard to debug

✅ Fix: Use proper crimp connectors, strain relief

### Software

❌ **Not saving settings**
- Settings reset on power cycle
- Calibration lost

✅ Fix: Always `:save` after changes

❌ **High duty cap on first test**
- Motors at full power = dangerous
- Can damage gears or drivers

✅ Fix: Start at 0.10, increase slowly

❌ **Wrong gear ratio**
- FFB feels wrong
- HID position incorrect

✅ Fix: Run gear calibration procedure

---

## Next Steps

After successful build:

1. ✅ Read [Troubleshooting Guide](troubleshooting.md) - Common issues
2. ✅ Join community Discord/forum - Share your build!
3. ✅ Configure [ffb-config GUI](../tools/ffb-config/README.md)
4. ✅ Test with [simulator first](simulator-integration.md)
5. ✅ Try racing with LMU telemetry
6. ✅ Share photos and feedback

---

## Getting Help

**Before asking:**
- Check [troubleshooting guide](troubleshooting.md)
- Run `:selftest` and share output
- Check all connections with multimeter
- Share serial log and settings (`:dump`)

**Where to ask:**
- GitHub Issues: Bug reports and feature requests
- Discussions: Build questions and sharing

**What to include:**
- Firmware version (`:version`)
- Full serial log
- Photos of wiring
- What you've already tried

---

*Last updated: 2024-09-12*
*Firmware version: v0.1.0*
