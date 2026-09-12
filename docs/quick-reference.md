# Quick Reference Card

**Essential commands and parameters for rp2040-ffb**

Print this page for quick access during setup and use!

---

## Serial Commands

**Connect via USB CDC at 115200 baud**

### Basic Commands

| Command | Description | Example |
|---------|-------------|---------|
| `h` | Help - show all commands | `h` |
| `v` | Version info | `v` |
| `d` | **Disable motors (EMERGENCY STOP)** | `d` |
| `e` | Enable motors | `e` |
| `s` | Spring mode (centering) | `s` |
| `m` | Manual FFB mode | `m` |
| `o` | FFB off | `o` |
| `c` | Start/stop gear ratio calibration | `c` |
| `p` | Auto-calibrate pedals | `p` |
| `t` | Toggle telemetry stream | `t` |
| `i` | I²C bus scan | `i` |

### Settings Commands

| Command | Description | Example |
|---------|-------------|---------|
| `:set <key> <value>` | Set parameter | `:set duty_cap 0.25` |
| `:dump` | Show all settings | `:dump` |
| `:save` | Save settings to EEPROM | `:save` |
| `:reset` | Reset to factory defaults | `:reset` |
| `:version` | Full version info | `:version` |
| `:selftest` | Run hardware tests | `:selftest` |

---

## Key Parameters

### Safety Parameters

| Parameter | Safe Range | Default | Description |
|-----------|------------|---------|-------------|
| `duty_cap` | 0.05 - 0.50 | 0.15 | Maximum motor PWM (0-1) |
| `torque_cap` | 0.05 - 0.50 | 0.15 | Maximum torque output (0-1) |
| `spring_k` | 0.001 - 0.012 | 0.005 | Spring stiffness |
| `damper_k` | 0.0 - 0.001 | 0.0005 | Velocity damping |

**⚠️ WARNING:** Values above safe range risk hardware damage!

### FFB Tuning

| Parameter | Typical Values | Description |
|-----------|---------------|-------------|
| `hid_range` | 540, 900, 1080 | Total steering rotation (degrees) |
| `ffb_gain` | 0.5 - 2.0 | Global FFB multiplier |
| `ffb_deadzone` | 0.0 - 5.0 | Center dead zone (degrees) |
| `motor_polarity` | +1 or -1 | Motor direction |

### Calibration Values

| Parameter | Description | How to Set |
|-----------|-------------|------------|
| `gear_ratio` | Motor:wheel ratio | Run `c` calibration |
| `thr_rest`, `thr_min`, `thr_max` | Throttle pedal range | Run `p` calibration |
| `brk_rest`, `brk_min`, `brk_max` | Brake pedal range | Run `p` calibration |
| `clu_rest`, `clu_min`, `clu_max` | Clutch pedal range | Run `p` calibration |

---

## Pin Assignments

### Base MCU (Raspberry Pi Pico)

**Motors:**
- GP10: Motor 1 RPWM
- GP11: Motor 1 LPWM
- GP12: Motor 1 EN
- GP13: Motor 2 RPWM
- GP14: Motor 2 LPWM
- GP15: Motor 2 EN

**Sensors:**
- GP16-19: MLX90363 SPI (MISO/CS/SCLK/MOSI)
- GP20: Index hall sensor
- GP26-28: Pedals ADC (Throttle/Brake/Clutch)

**Communication:**
- GP0: UART TX to rim
- GP1: UART RX from rim
- GP2: Rim RUN (reset)
- USB: HID + CDC

### Rim MCU (Raspberry Pi Pico)

**I²C Bus (GP4/GP5):**
- 0x20: MCP23017 buttons
- 0x21: MCP23017 LEDs
- 0x48: ADS1115 paddles

**Encoders:**
- GP6/7: Encoder 0
- GP8/9: Encoder 1
- GP10/11: Encoder 2
- GP12/13: Encoder 3

**Other:**
- GP14: WS2812 LED strip
- GP0: UART RX from base
- GP1: UART TX to base

---

## Common Procedures

### First Power-On

```
1. Set safety limits:
   :set duty_cap 0.10
   :set torque_cap 0.10
   :save

2. Enable motors:
   e

3. Test spring mode:
   s
   (Turn wheel - should center gently)

4. If OK, gradually increase:
   :set duty_cap 0.15
   :set duty_cap 0.20
   ...
```

### Gear Ratio Calibration

```
1. Start calibration:
   c

2. Rotate wheel exactly 360° (slow and steady)

3. Finish:
   c
   (Note displayed ratio)

4. Save:
   :set gear_ratio 18.1234
   :save
```

### Pedal Calibration

```
1. Reset calibration:
   p

2. Press each pedal fully once, then release:
   - Throttle: 0% → 100% → 0%
   - Brake: 0% → 100% → 0%
   - Clutch: 0% → 100% → 0%

3. Save:
   :save

4. Verify with telemetry:
   t
   (Check percentages)
```

### Troubleshooting Steps

**Motors don't work:**
```
1. Check power supply ON (12-24V)
2. Verify settings: :dump
3. Enable motors: e
4. Activate mode: s
5. Check serial for errors
```

**HID not detected:**
```
1. Try different USB port/cable
2. Check Device Manager (Windows)
3. Re-flash firmware (BOOTSEL mode)
```

**Pedals wrong:**
```
1. Run calibration: p
2. Check wiring (3.3V not 5V!)
3. Verify ADC readings: t
```

---

## Telemetry Streams

Enable with `t` command. Format: `<PREFIX> ... data ...`

| Prefix | Data |
|--------|------|
| `T` | Throttle/brake/clutch percentages + ADC values |
| `G` | Angle, velocity, motor state |
| `E` | Encoder positions |
| `X` | Extended status (link, temperature) |

**Example:**
```
T ... thr=75% brk=0% clu=0% raw_thr=1250
G ... axle=5.2 hid=0.0 vel=12.5 motor_en=1
```

---

## Safety Warnings

### ⚠️ CRITICAL SAFETY

- **Motor Power:** NEVER connect motor voltage (12-24V) to Pico GPIO!
- **Pedal Power:** Logitech pedals use 3.3V, NOT 5V!
- **Emergency Stop:** Press `d` command immediately if anything wrong
- **Temperature:** Motors should stay < 60°C (warm, not hot)
- **Duty Cap:** Start at 0.10, increase slowly, max safe = 0.50

### 🔥 Fire Risk

- Use proper wire gauge (18 AWG for motors)
- Fuse motor power supply (5A recommended)
- Never leave running unattended
- Have fire extinguisher nearby

### ⚡ Electrical Safety

- Isolate motor power from USB/logic power
- Common ground required
- Check for shorts with multimeter before power-on
- Use proper connectors (no loose wires)

---

## Quick Settings Presets

### Testing (safe, low power)
```
:set duty_cap 0.10
:set torque_cap 0.10
:set spring_k 0.002
:save
```

### Normal Use (most games)
```
:set duty_cap 0.25
:set torque_cap 0.25
:set spring_k 0.006
:set hid_range 900
:save
```

### Rally / Strong FFB
```
:set duty_cap 0.35
:set torque_cap 0.35
:set spring_k 0.008
:set hid_range 540
:save
```

### Truck Sim (large rotation)
```
:set duty_cap 0.25
:set torque_cap 0.25
:set spring_k 0.004
:set hid_range 1080
:save
```

---

## Support Resources

| Resource | Link |
|----------|------|
| Build Guide | [docs/build-guide.md](build-guide.md) |
| Troubleshooting | [docs/troubleshooting.md](troubleshooting.md) |
| Calibration | [docs/calibration-and-tuning.md](calibration-and-tuning.md) |
| FAQ | [docs/faq.md](faq.md) |
| Wiring | [docs/wiring-diagrams.md](wiring-diagrams.md) |
| GitHub Issues | https://github.com/YOUR_ORG/rp2040-ffb/issues |
| Discussions | https://github.com/YOUR_ORG/rp2040-ffb/discussions |

---

## Version Info

**Firmware version:** Check with `:version` command

**Expected output:**
```
rp2040-ffb base-mcu
Version: vX.Y.Z
Build: YYYYMMDD-HHMMSS
```

---

*Print this reference for quick access during calibration and tuning!*

*Last updated: 2024-09-12*
