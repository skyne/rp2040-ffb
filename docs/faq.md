# Frequently Asked Questions (FAQ)

**Common questions about building and using rp2040-ffb**

---

## General Questions

### What is rp2040-ffb?

rp2040-ffb is an open-source force feedback steering wheel project using two Raspberry Pi Pico boards (RP2040 MCUs), a donor Logitech G920/G923 wheel base, and custom firmware. It provides:
- USB HID joystick (plug & play)
- Real force feedback (dual motors)
- Logitech pedal support
- Custom button panel with LEDs
- Open protocol for configurability

### Why build this instead of buying a commercial wheel?

**Advantages:**
- **Cost:** ~$350-550 vs. $1000+ for similar commercial wheels
- **Open source:** Fully customizable firmware and hardware
- **Learning:** Understand how FFB works, develop coding skills
- **Community:** Share improvements, get help
- **Upgradability:** Add features (displays, buttons, sensors)

**Disadvantages:**
- Requires assembly (10-20 hours)
- Need soldering/electronics skills
- No official support or warranty
- Debugging falls on you

### Is this legal? Can I sell builds?

**Legal status:**
- ✅ Building for personal use: Legal
- ✅ Sharing firmware/designs: Legal (open source license)
- ⚠️ Selling complete wheels: Check local laws
  - Some regions require CE/FCC certification
  - May need liability insurance
- ⚠️ Using Logitech branding: Trademark infringement

**Recommended:**
- Build for yourself or friends (non-commercial)
- If selling, consult lawyer for compliance

### Which Logitech wheel base should I buy?

**Recommended:**
- **G920** (for Xbox/PC): Best value, widely available used
- **G923** (newer): TrueForce (not used by this project), slightly better gears

**Compatible with modifications:**
- ⚠️ G25, G27, G29, DFGT (Driving Force GT) - require different angle sensor
  - These use optical rotary encoder instead of MLX90363 hall sensor
  - Firmware needs modification to read quadrature encoder
  - Same motors and mechanics otherwise
  - See [G25/G27/G29/DFGT Compatibility Note](#g25g27g29dfgt-compatibility) below

**Where to buy:**
- eBay, Facebook Marketplace, Craigslist
- Look for "broken" units (often just USB board failed, motors OK)
- Test motors manually before purchasing

---

## Hardware Questions

### Can I use a different MCU instead of Raspberry Pi Pico?

**Technically yes, but:**
- Requires extensive firmware porting
- Must have: USB HID, dual 12-bit ADC, SPI, I²C, UART, 6+ PWM channels
- Compatible MCUs: ESP32-S3, STM32F4, Teensy 4.1

**Recommendation:** Stick with Pico (cheap, well-supported)

### Do I need both Picos, or can I use just one?

**Two Picos recommended because:**
- Physical separation: Base in wheel housing, rim in detachable wheel
- Pin count: One Pico lacks enough GPIOs for all features
- Modularity: Swap rims without rewiring base
- UART communication: Proven protocol

**Single Pico possible:**
- Omit rim features (buttons, encoders, LEDs)
- Or use USB hub + two Picos as separate devices (no UART link)

### What if I don't have Logitech pedals?

**Alternatives:**
- Any 3-wire potentiometer pedals (10kΩ recommended)
- Connect: VCC (3.3V), GND, wiper → Pico ADC
- Calibrate with `:p` command

**No pedals at all:**
- Firmware works fine without pedals
- HID reports 0% throttle/brake/clutch

### Can I add a handbrake or sequential shifter?

**Yes!** Use spare ADC or GPIO pins:

**Handbrake (analog):**
- Connect pot to GP26/27/28 (ADC)
- Modify firmware to map ADC → HID axis

**Sequential shifter (digital):**
- Connect switch to GPIO with pullup
- Firmware reads button state → HID button

**Example code:**
```cpp
// In main.cpp
bool handbrake = analogRead(GP26) > 2048;
hid_report.buttons |= (handbrake << 15);  // Button 16
```

### What power supply do I need for the motors?

**Option 1: Reuse Stock PSU (Recommended - FREE!)**
- G920/G923 includes 24V power supply
- Stock motors are rated for 24V
- Already sized correctly for the motors
- Just rewire to BTS7960 drivers
- **Cost: $0** (you already have it!)

**Option 2: Buy New 24V PSU**
- **Voltage:** 24V DC (stock motor rating)
- **Current:** 5A minimum, 10A recommended
- **Type:** Switching PSU (laptop charger style)
- **Connectors:** Barrel jack or XT60

**Where to buy:**
- Amazon: "24V 10A power supply"
- Used 24V laptop chargers (Dell, HP)
- Meanwell LRS-350-24 (quality option)

**Experimental: 36V Upgrade (Coming Soon)**
- Planned future testing with 36V
- Requires additional cooling:
  - Motor heatsinks
  - Driver cooling fans
  - Temperature monitoring
- Higher torque potential
- **Not recommended yet** (under testing)

**Safety:**
- Fuse at 5A for 24V
- Isolate motor power from USB power
- Use proper wire gauge (18 AWG minimum)
- Monitor motor temperature (<60°C)

---

## Software Questions

### What games are supported?

**Any game with HID joystick support:**
- ✅ iRacing, Assetto Corsa, ACC
- ✅ Dirt Rally 2.0, WRC
- ✅ BeamNG.drive
- ✅ Euro Truck Simulator 2, American Truck Simulator
- ✅ rFactor 2
- ⚠️ Le Mans Ultimate (with FFB telemetry mode)

**Not supported:**
- ❌ Games requiring specific wheel drivers (rare)

### How does force feedback work?

**Two modes:**

**1. Spring mode (default):**
- Firmware generates centering spring force
- No game data needed
- Works in any game
- Simple but not realistic

**2. LMU telemetry mode:**
- Game sends FFB commands via UDP
- Firmware parses and applies forces
- Realistic road feel, curbs, bumps
- Requires compatible game (LMU, custom plugins)

### Can I use this on console (Xbox/PlayStation)?

**No, USB only works on PC.**

**Why:**
- Consoles require licensed authentication chips
- HID descriptor must match official wheels
- DRM prevents custom devices

**Workarounds:**
- Use console controller → PC adapter (high latency)
- Or build separate console-compatible device (different project)

### How do I update the firmware?

**Method 1: Desktop GUI (easiest, recommended):**

The `ffb-config` desktop app can flash both Picos as a package!

1. Download firmware pack (.zip) from releases
2. Open ffb-config GUI
3. Go to "Firmware Update" tab
4. Select the firmware pack file
5. Click "Flash Both MCUs"
6. GUI will automatically:
   - Flash base MCU firmware
   - Flash rim MCU firmware via base (over UART)
   - Verify both flashed successfully
   - No BOOTSEL button needed!
   - No manual file copying!

**Advantages:**
- ✅ One-click update for both Picos
- ✅ Automatic version matching (base + rim)
- ✅ Progress indication
- ✅ Verification built-in
- ✅ No need to physically access rim MCU

See: [tools/ffb-config/README.md](../tools/ffb-config/README.md) for GUI details

**Method 2: Manual UF2 (if GUI unavailable):**
1. Hold BOOTSEL button on Pico
2. Plug USB cable
3. Pico appears as USB drive
4. Copy `firmware.uf2` file to drive
5. Pico reboots with new firmware
6. Repeat for second Pico

**Method 3: PlatformIO (for developers):**
```bash
cd firmware-base
pio run -t upload

cd ../firmware-rim
pio run -t upload
```

**Settings are preserved** (stored in EEPROM)

**Note:** The desktop GUI method is recommended for most users as it handles both MCUs automatically and ensures version compatibility.

### Do I lose settings when updating firmware?

**No,** settings are stored in EEPROM (flash memory) separate from firmware.

**To reset settings:**
```
Serial> :reset
Settings reset to factory defaults
```

---

## Build Process Questions

### What if I've never soldered before?

**Start with practice:**
- Buy a soldering kit (cheap PCB + components)
- Watch YouTube tutorials: "soldering for beginners"
- Practice on scrap wire before real build

**This project requires:**
- Through-hole soldering (easy)
- Wire-to-board soldering (medium)
- No surface-mount soldering (unless optional displays)

**Alternatives:**
- Breadboard first (no solder, uses jumper wires)
- Or use pre-assembled modules (more expensive)

### How long does the build take?

**Time breakdown:**
| Stage | Time |
|-------|------|
| Parts ordering + shipping | 1-2 weeks |
| Electronics assembly | 8-12 hours |
| Mechanical assembly | 4-6 hours |
| Testing & calibration | 2-4 hours |
| Troubleshooting | 2-? hours |
| **Total** | **15-25 hours + shipping** |

**Spread over:**
- Weekend project: 2-3 weekends
- After-work project: 2-3 weeks

### Can I skip the rim MCU and just use the base?

**Yes,** the base MCU alone provides:
- Force feedback
- Pedals
- HID joystick

**You'll miss:**
- Button panel
- Rotary encoders
- LED shift lights
- TFT display

**Good for:**
- Testing FFB functionality
- Minimal builds
- Budget constraints

### What if I can't find a specific component?

**Substitutions:**

| Original Part | Substitute |
|---------------|------------|
| MCP23017 | MCP23008 (8-bit version) |
| ADS1115 | Pico ADC pins (lower resolution) |
| BTS7960 | IBT-2, VNH5019, L298N (lower power) |
| MLX90363 | MLX90393 (requires code changes) |
| WS2812B | Any addressable LED (WS2811, SK6812) |

**Missing features OK:**
- E-paper display: Optional
- TFT display: Optional
- Accelerometer: Optional

---

## Troubleshooting Questions

### My wheel doesn't appear in Windows devices

**Check:**
1. USB cable connected to **base Pico**
2. Green LED on Pico is lit
3. Device Manager shows "USB Input Device"
4. Try different USB port / cable

**Fix:**
- Re-flash firmware (hold BOOTSEL, copy UF2)
- Test with known-good USB cable
- Check Windows Update for drivers

### Force feedback doesn't work

**Checklist:**
- [ ] Motors enabled: `Serial> e`
- [ ] FFB mode active: `Serial> s` (spring)
- [ ] Motor power supply ON (12-24V)
- [ ] BTS7960 logic VCC connected to Pico 3V3
- [ ] BTS7960 B+/B- connected to PSU
- [ ] No `duty_cap=0.00` (check `:dump`)

**Test motors manually:**
```
Serial> :set duty_cap 0.15
Serial> e
Serial> s
# Turn wheel, should resist
```

### Pedals don't register

**Check:**
- [ ] DE-9 connector wired correctly
- [ ] Pedals powered from **3.3V** (not 5V!)
- [ ] Enable telemetry: `Serial> t`
- [ ] See changing values when pedals move

**Fix:**
- Run pedal calibration: `Serial> p`
- Test continuity: VCC→pin6/9, GND→pin1/5

### Rim buttons don't work

**Check:**
- [ ] UART link active: `Serial> :dump` shows `rim_link=1`
- [ ] I²C devices detected: `Serial> i` shows 0x20, 0x21
- [ ] Button wiring correct (GPA0-7 to GND)

**Fix:**
- Swap TX/RX on UART (base TX → rim RX)
- Check I²C pullups (4.7kΩ to 3V3)
- Test I²C with: `Serial> :i2cscan`

### Motors overheat or smoke

**IMMEDIATELY:**
1. Disconnect motor power
2. Let cool 30 minutes
3. Inspect for damage

**Causes:**
- `duty_cap` too high (>0.50)
- Prolonged high load
- Insufficient heatsinking
- Short circuit

**Prevention:**
- Start with `duty_cap=0.10`
- Add cooling fans to motors
- Add heatsinks to BTS7960
- Limit continuous use (<10 min)

---

## Advanced Questions

### Can I add force feedback to a different wheel base?

**Possible, but requires:**
- Dual DC motors with gearbox
- Rotary encoder or hall sensor
- Motor driver (BTS7960 or similar)
- Mechanical endstop removal

**Easier than starting from scratch:**
- Logitech G920/G923 has proven mechanics
- Direct drive wheels are much harder (high torque, expensive)

### How accurate is the position sensing?

**MLX90363 specifications:**
- Resolution: 0.09° (14-bit)
- Accuracy: ±0.5° typical
- After gear ratio (~18:1): ±0.03° wheel resolution

**Good enough for:**
- All racing games
- Precise steering input
- Professional sim racing

### Can I increase the force feedback strength?

**Yes, but carefully:**

**Safe limits (24V stock):**
- `duty_cap`: Up to 0.50 (50% power)
- Monitor motor temperature (<60°C)
- Test incrementally (+0.05 at a time)

**Unsafe (not recommended):**
- `duty_cap > 0.70`: High risk of:
  - Motor overheating
  - Gear damage
  - Driver failure
  - Fire hazard

**Better approaches:**
- Optimize FFB tuning (`spring_k`, `damper_k`)
- Use stock 24V PSU (reuse G920/G923 power supply)
- **Experimental 36V upgrade** (under testing):
  - Requires motor heatsinks (bolt-on aluminum)
  - Requires driver cooling fans (40mm)
  - Temperature monitoring (<60°C continuous)
  - ~50% more torque potential
  - **Status:** Not recommended yet (testing in progress)
  - Monitor community for test results

### Can I run the motors at 36V for more torque?

**Current status: EXPERIMENTAL (under testing)**

**Theory:**
- Stock motors rated 24V
- BTS7960 drivers support up to 43V
- Higher voltage = more torque (~50% increase)

**Required modifications:**
1. **Motor cooling:**
   - Bolt-on aluminum heatsinks
   - Thermal paste/pads
   - Airflow path to heatsinks
   
2. **Driver cooling:**
   - 40mm fans on BTS7960 modules
   - 12V fan powered from separate rail
   - Direct airflow to FET heatsinks
   
3. **Temperature monitoring:**
   - Infrared thermometer checks
   - Keep motors <60°C continuous
   - Keep drivers <70°C continuous
   
4. **Power supply:**
   - 36V PSU, 5A minimum
   - DO NOT use stock 24V PSU for this!

**Risks:**
- ⚠️ Motor lifespan reduction
- ⚠️ Increased gear wear
- ⚠️ Driver failure if cooling inadequate
- ⚠️ Fire risk without monitoring

**Status:**
- Testing planned by project maintainer
- Results will be published to community
- DO NOT attempt until validated
- Stick with 24V for reliable operation

**If you want to help test:**
- Contact maintainer first
- Document temperatures thoroughly
- Share results (success or failure!)
- Help develop cooling guide

---

### How do I contribute to the project?

See [CONTRIBUTING.md](../CONTRIBUTING.md) for:
- Code style guidelines
- Pull request process
- Issue reporting
- Community guidelines

**Ways to help:**
- Report bugs with detailed logs
- Share build photos and feedback
- Improve documentation
- Write code for new features
- Answer questions from other builders
- **Test 36V setup** (contact maintainer first)

---

## Safety Questions

### Is this safe to build and use?

**Risks exist:**
- ⚠️ Electric shock (if wired incorrectly)
- ⚠️ Motor torque (can pinch fingers)
- ⚠️ Overheating (fire risk if unmonitored)
- ⚠️ Mechanical failure (gears, mounts)

**Mitigation:**
- Follow build guide exactly
- Use proper wire gauges and fuses
- Start with low power (`duty_cap=0.10`)
- Monitor temperatures
- Have fire extinguisher nearby
- Don't leave running unattended

**Certification:**
- This is a DIY project (no UL/CE certification)
- Use at your own risk

### What safety features are built-in?

**Firmware protections:**
- Motor watchdog (thermal timeout at 60s continuous)
- Communication watchdog (disables motors if USB/UART lost)
- Settings validation (rejects dangerous values)
- Emergency stop command (`:d`)
- Duty cycle limits

**Hardware protections (recommended):**
- Fuse on motor power (5A)
- Thermal cutoff on motors (90°C)
- Current limiting on drivers
- Mechanical endstops (optional)

### What if the firmware crashes?

**Fail-safe behavior:**
- Motors automatically stop (watchdog triggers)
- BTS7960 drivers have built-in fault protection
- Pico reboots and re-initializes

**Recovery:**
- Unplug USB
- Disconnect motor power
- Reflash firmware via BOOTSEL

---

## Community & Support

### Where can I get help?

**Official channels:**
- GitHub Issues: Bug reports, feature requests
- GitHub Discussions: Build questions, sharing

**Unofficial:**
- Reddit: r/simracing (for general sim racing)
- Discord: (if community server exists)

### How do I report a bug?

**Include:**
1. Firmware version (`:version`)
2. Full serial log (`:dump`)
3. Steps to reproduce
4. Expected vs. actual behavior
5. Hardware setup (G920 vs. G923, etc.)

**Good bug report example:**
```
Title: Motors don't stop on emergency stop command

Firmware: v0.1.0
Hardware: G920, dual BTS7960

Steps:
1. Enable motors: `e`
2. Activate spring mode: `s`
3. Send emergency stop: `d`

Expected: Motors stop immediately
Actual: Motors continue running for 2 seconds

Serial log:
[paste log here]
```

### Can I hire someone to build this for me?

**Options:**
- Check GitHub Discussions for community builders
- Local makerspaces / hackerspaces
- Electronics assembly services (PCB only)

**Cost estimate:**
- Parts: $350-550
- Labor: $200-500 (depends on builder)
- **Total: $550-1050**

*At this price, commercial wheels may be competitive!*

---

## Future Features

### What's on the roadmap?

**Planned features:**
- Wireless button panel (Bluetooth)
- Direct drive motor support (high torque)
- Force feedback telemetry plugins (more games)
- GUI configuration tool improvements
- Automatic game detection
- Telemetry data logging

**Community requests:**
- Sequential shifter support
- Haptic feedback (vibration)
- Custom button LED animations
- TFT display menus

**Contribute ideas:**
- Open GitHub Discussion with [Feature Request] tag

---

## G25/G27/G29/DFGT Compatibility

### Can I use G25/G27/G29/DFGT instead of G920/G923?

**Yes, with firmware modifications!** The main difference is the angle sensor:

| Wheel | Angle Sensor | Compatibility |
|-------|-------------|---------------|
| **G920/G923** | MLX90363 hall sensor (SPI) | ✅ Works out-of-box |
| **G25/G27/G29/DFGT** | Optical rotary encoder (quadrature) | ⚠️ Requires firmware change |

### What needs to be changed?

**1. Angle sensing (main change):**

**Current (G920/G923):**
- MLX90363 hall sensor over SPI
- Reads absolute angle (0-360°)
- 14-bit resolution (0.09° per step)

**For G25/G27/G29/DFGT:**
- Optical rotary encoder (quadrature A/B signals)
- Incremental position (counts pulses)
- Needs homing on power-on
- Same encoder type across all these models

**Firmware changes needed:**
```cpp
// Replace in firmware-base/src/main.cpp

// Remove MLX90363 code:
// #include "mlx90363.h"
// Mlx90363::init();

// Add encoder code:
#include "encoder.h"
volatile int32_t encoderCount = 0;

void encoderISR_A() {
    if (digitalRead(ENCODER_A) == digitalRead(ENCODER_B)) {
        encoderCount++;
    } else {
        encoderCount--;
    }
}

void setup() {
    pinMode(ENCODER_A, INPUT_PULLUP);
    pinMode(ENCODER_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR_A, CHANGE);
}

// Convert encoder counts to angle
float getAngle() {
    // G25/G27/G29/DFGT encoder: ~2048 pulses per revolution (check yours!)
    // DFGT may vary - measure your specific unit
    const float PULSES_PER_REV = 2048.0f;
    float wheelAngle = (encoderCount / PULSES_PER_REV) * 360.0f;
    return wheelAngle;
}
```

**Pin assignments:**
- GP16 → Encoder A (was MLX90363 MISO)
- GP17 → Encoder B (was MLX90363 CS)
- GP18, GP19 freed up (were SCLK, MOSI)

**2. Homing procedure:**

Since encoder is incremental (not absolute), you need to home on startup:

**Option A: Use index magnet (recommended):**
- Keep the index hall sensor (A3144 on GP20)
- On power-on, rotate wheel slowly until index detected
- Set encoder count = 0 at this position
- Same as current firmware's homing logic

**Option B: Manual homing:**
- User centers wheel manually on power-on
- Press button to zero encoder count
- Less convenient but simpler

**3. Calibration changes:**

Gear ratio calibration works the same:
- Still measure encoder counts per wheel rotation
- Calculate ratio from encoder resolution

### What stays the same?

Everything else is identical:

- ✅ Motor drivers (BTS7960) - same wiring
- ✅ Dual motors - same motors as G920/G923
- ✅ Pedals - same DE-9 pinout
- ✅ Rim MCU - no changes needed
- ✅ Power supply - same requirements
- ✅ FFB algorithms - same code
- ✅ USB HID - same descriptor
- ✅ Settings system - same EEPROM

### Encoder specifications

**G25/G27/G29/DFGT optical encoder:**
- Type: Incremental quadrature (2-channel)
- Resolution: ~512-2048 pulses per revolution (varies by model)
- Output: Open collector (needs pullup resistors)
- Voltage: 5V tolerant, but 3.3V works with pullups
- DFGT: Similar specs to G25/G27/G29, but verify your unit

**Wiring (typical):**
```
Encoder → Pico Base
─────────────────────
Channel A → GP16 (with INPUT_PULLUP)
Channel B → GP17 (with INPUT_PULLUP)
VCC       → 3V3 or 5V
GND       → GND
```

### Performance comparison

| Aspect | MLX90363 (G920/G923) | Encoder (G25/G27/G29/DFGT) |
|--------|---------------------|---------------------------|
| **Resolution** | 0.09° (14-bit) | 0.18-0.7° (depends on encoder) |
| **Absolute position** | Yes (survives power cycle) | No (needs homing) |
| **Drift** | None | Can drift if pulses missed |
| **Wiring** | SPI (4 wires) | 2 GPIOs + pullups |
| **Code complexity** | Simple SPI reads | Interrupt handling |
| **Cost to add** | $15-20 (if buying sensor) | $0 (already in wheel) |

**Verdict:** Both work well. MLX90363 is slightly better (absolute), but encoder is free if you already have G25/G27/G29/DFGT.

### Implementation difficulty

**Estimated effort:**
- Code changes: 2-3 hours (if experienced)
- Testing: 1-2 hours
- Calibration: Same as G920/G923

**Skill level:**
- Moderate (requires understanding interrupts)
- Need to read encoder datasheet
- Debug timing issues if pulses missed

**Community support:**
- G920/G923 is primary platform (more tested)
- G25/G27/G29/DFGT users: share your mods!
- Consider documenting your changes for others

### Should I use G25/G27/G29/DFGT?

**Use G25/G27/G29/DFGT if:**
- ✅ You already own one
- ✅ Comfortable modifying firmware
- ✅ Want to save $15-20 (no MLX90363 needed)
- ✅ Don't mind homing on power-on
- ✅ Can find cheaper used units

**Use G920/G923 if:**
- ✅ Starting from scratch
- ✅ Want out-of-box firmware
- ✅ Prefer absolute position (no homing)
- ✅ Want most tested platform

### Additional notes on DFGT

**Logitech Driving Force GT specifics:**
- Released 2007 (for GT5 Prologue)
- Same dual-motor platform as G25/G27/G29
- 900° rotation (same as others)
- Good availability used ($80-150)
- Less buttons than G25/G27 (but you're building custom rim anyway!)
- Pedals: 2-pedal set (no clutch unless upgraded)

**Why DFGT is a good option:**
- ✅ Often cheaper than G25/G27/G29
- ✅ Same guts (motors, gearbox, encoder)
- ✅ Widely available (was bundled with GT5)
- ✅ Easy to disassemble
- ✅ Pedals less important (building custom anyway)

**Caveats:**
- Less popular in community (less documentation)
- May have slightly different encoder specs (measure yours!)
- Original rim/buttons not needed (you're replacing)

### Community contributions wanted!

**If you build with G25/G27/G29/DFGT:**
- Share your encoder pin mappings
- Document exact pulses-per-revolution (especially DFGT!)
- Test firmware with different models
- Photograph internal layout (helps others)
- Submit PR with encoder support
- Help others in Discussions

**Potential firmware addition:**
- Compile-time option: `#define USE_ENCODER` vs. `#define USE_MLX90363`
- Auto-detect encoder type at runtime
- Support both in single firmware

---

## Terminology

| Term | Definition |
|------|------------|
| **FFB** | Force Feedback - motors provide resistance/forces |
| **HID** | Human Interface Device - USB protocol for game controllers |
| **CDC** | Communication Device Class - USB serial port |
| **PWM** | Pulse Width Modulation - motor speed control |
| **Duty Cycle** | Percentage of time PWM is HIGH (0-100%) |
| **Torque** | Rotational force applied by motors |
| **Gear Ratio** | Motor rotations per wheel rotation |
| **Telemetry** | Data stream from game or firmware |
| **EEPROM** | Non-volatile memory for settings |
| **Calibration** | Mapping sensor values to usable range |

---

*Last updated: 2024-09-12*
*Questions not answered? Open a GitHub Discussion!*
