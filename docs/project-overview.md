# Project Overview

**High-level introduction to rp2040-ffb for newcomers**

This document provides a bird's-eye view of the project to help you understand what it is, how it works, and where to start.

---

## What is rp2040-ffb?

rp2040-ffb is an **open-source DIY force feedback steering wheel** that you can build yourself. It uses:
- Two Raspberry Pi Pico boards (cheap microcontrollers)
- A donor Logitech G920/G923 wheel base (for motors and mechanics)
- Custom firmware you flash onto the Picos
- Desktop software for configuration

**Result:** A fully functional force feedback wheel that:
- ✅ Connects to PC via USB (plug & play)
- ✅ Works with all sim racing games (appears as standard joystick)
- ✅ Provides real force feedback (motors resist your turns)
- ✅ Supports Logitech pedals (throttle, brake, clutch)
- ✅ Has customizable buttons, LEDs, and displays
- ✅ Costs $350-590 (vs. $1000+ for commercial wheels)

---

## How It Works

### System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                          Your PC                                │
│                                                                 │
│  ┌──────────────┐         ┌──────────────┐                     │
│  │ Racing Game  │         │  ffb-config  │  (configuration)    │
│  │ (LMU, AC,    │         │     GUI      │                     │
│  │  iRacing)    │         └───────┬──────┘                     │
│  └──────┬───────┘                 │                            │
│         │                         │                            │
│    (HID Joystick)            (USB CDC Serial)                  │
└─────────┼───────────────────────────┼─────────────────────────┘
          │                           │
          │ USB Cable                 │
          │                           │
┌─────────▼───────────────────────────▼─────────────────────────┐
│                     Base MCU (Pico #1)                        │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  • Appears as USB joystick to game                      │  │
│  │  • Reads steering angle from hall sensor               │  │
│  │  • Reads pedals (ADC)                                   │  │
│  │  • Controls motors for force feedback                   │  │
│  │  • Talks to rim MCU over UART                          │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                │
│  Connected hardware:                                           │
│  - Angle sensor (MLX90363 on G920/G923, encoder on older)    │
│  - 2× BTS7960 motor drivers (force feedback)                  │
│  - Logitech pedals (throttle, brake, clutch)                  │
│  - Status LEDs (optional)                                      │
└────────────────────────┬───────────────────────────────────────┘
                         │
                         │ UART Cable (4-6 wires)
                         │
┌────────────────────────▼───────────────────────────────────────┐
│                     Rim MCU (Pico #2)                          │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  • Reads buttons and rotary encoders                    │  │
│  │  • Controls LED shift lights                            │  │
│  │  • Drives optional TFT display                          │  │
│  │  • Sends input states to base MCU                       │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                │
│  Connected hardware:                                           │
│  - 10 buttons (via MCP23017)                                   │
│  - 4 rotary encoders                                           │
│  - WS2812 LED strip (shift lights)                             │
│  - TFT display (optional)                                      │
└────────────────────────────────────────────────────────────────┘
```

### Data Flow

```
Game → USB HID → Base MCU → Motor Drivers → Motors → Steering Feel
  ↑                  ↑           ↑
  │                  │           └── Pedals (analog input)
  │                  │
  │                  └── UART ← Rim MCU ← Buttons/Encoders
  │
  └── (Steering position, pedal positions, button states)
```

---

## Key Components

### Hardware

**Two Raspberry Pi Pico boards:**
- Cheap ($4-6 each)
- Powerful (dual-core ARM, 264KB RAM)
- USB built-in
- Lots of GPIO pins

**Logitech G920/G923 wheel base (donor):**
- Dual DC motors (force feedback)
- 18:1 gearbox (already built)
- Sturdy mechanics
- Available used ($150-300)

**Note:** G25/G27/G29/DFGT also compatible with firmware modifications (optical encoder instead of hall sensor). See [FAQ](../docs/faq.md#g25g27g29dfgt-compatibility). DFGT often cheapest option!

**Sensors:**
- Angle sensor: MLX90363 hall (G29/G920/G923 stock) or optical encoder (G25/G27/DFGT)
- A3144: Digital hall sensor (index position for G920/G923)
- Pedals: Logitech's built-in potentiometers

**Motor drivers:**
- BTS7960 H-bridges (control motor direction and speed)
- Handle high current (up to 43A)
- Cheap ($5-10 each)

**Optional add-ons:**
- Button panel (MCP23017 I²C GPIO expanders)
- Rotary encoders (menu navigation)
- LED strip (shift lights)
- TFT display (dashboard)
- E-paper display (status)

### Software

**Firmware (C++):**
- Runs on both Picos
- Built with PlatformIO + Arduino framework
- ~30KB compiled size (tiny!)
- Open source, fully customizable

**GUI Configurator (Tauri):**
- Cross-platform (Windows, Linux, macOS)
- Set parameters (FFB strength, pedal calibration)
- Monitor telemetry (real-time data)
- Update firmware (OTA)

**Simulator (Python):**
- Hardware-free testing
- Emulates base MCU behavior
- Develop GUI without building hardware

---

## Project Structure

### Repository Organization

```
rp2040-ffb/
├── firmware-base/          # Base MCU firmware (motor control, sensors)
├── firmware-rim/           # Rim MCU firmware (buttons, LEDs)
├── shared/                 # Common code (UART protocol)
├── tools/
│   ├── ffb-config/         # Desktop configurator GUI
│   └── firmware-sim/       # Python simulator
├── docs/                   # ← YOU ARE HERE
│   ├── build-guide.md      # How to build hardware
│   ├── wiring-diagrams.md  # Connection details
│   ├── calibration-and-tuning.md  # Optimize performance
│   ├── firmware-development.md    # Code architecture
│   ├── troubleshooting.md  # Fix problems
│   ├── faq.md              # Common questions
│   └── quick-reference.md  # Command cheat sheet
├── firmware-tests/         # Unit tests
├── scripts/                # Build automation
├── CONTRIBUTING.md         # How to contribute
├── CHANGELOG.md            # Version history
└── README.md               # Project overview
```

### Documentation Map

**"I want to build this"** → Start with:
1. [FAQ](faq.md) - Is this right for me?
2. [Build Guide](build-guide.md) - Step-by-step assembly
3. [Wiring Diagrams](wiring-diagrams.md) - Connection reference
4. [Calibration](calibration-and-tuning.md) - Optimize performance

**"I want to modify the code"** → Start with:
1. [CONTRIBUTING.md](../CONTRIBUTING.md) - Guidelines
2. [Firmware Development](firmware-development.md) - Architecture
3. [Simulator](simulator-integration.md) - Test without hardware

**"I have a problem"** → Start with:
1. [Troubleshooting](troubleshooting.md) - Common issues
2. [Quick Reference](quick-reference.md) - Commands
3. [FAQ](faq.md) - Known issues

---

## Build Timeline

### Week 1-2: Planning & Ordering

- [ ] Read build guide and FAQ
- [ ] Order parts (see BOM in build guide)
- [ ] Gather tools (soldering iron, multimeter, etc.)
- [ ] Study wiring diagrams

**Time:** 2-4 hours research + 1-2 weeks shipping

### Week 3: Electronics Assembly

- [ ] Breadboard test (verify components work)
- [ ] Solder base MCU connections
- [ ] Test motor drivers (bench test, no wheel yet)
- [ ] Solder rim MCU connections
- [ ] Test I²C devices

**Time:** 8-12 hours

### Week 4: Mechanical Integration

- [ ] Disassemble donor wheel
- [ ] Remove plastic endstop
- [ ] Install index magnet
- [ ] Mount sensors
- [ ] Route wiring
- [ ] Connect motors

**Time:** 4-6 hours

### Week 5: Testing & Calibration

- [ ] Flash firmware
- [ ] First power-on (use 24V stock PSU, low duty_cap!)
- [ ] Calibrate gear ratio
- [ ] Calibrate pedals
- [ ] Tune FFB parameters
- [ ] Test with games

**Time:** 2-4 hours + testing

**Total: 15-25 hours + 2-3 weeks shipping**

---

## Technology Stack

### Firmware (Embedded C++)

**Platform:** PlatformIO + Arduino-Pico framework

**Key libraries:**
- TinyUSB (USB HID + CDC)
- SPI (hall sensor communication)
- I²C (button/LED expanders, ADC)
- EEPROM (settings storage)

**Features:**
- Dual-core (Pico 2 only)
- Interrupt-driven I/O
- Real-time motor control
- Framed UART protocol

### GUI (Tauri + JavaScript)

**Platform:** Tauri v1 (Rust + Web)

**Frontend:**
- HTML/CSS/JavaScript (vanilla, no framework)
- Serial port communication (Web Serial API)
- Real-time telemetry display

**Features:**
- Cross-platform (single codebase)
- Native performance
- Small binary size (~5MB)
- No Electron overhead

### Simulator (Python)

**Language:** Python 3.8+

**Key libraries:**
- pyserial (virtual serial ports)
- pytest (testing)

**Features:**
- Hardware-free development
- Full protocol emulation
- Scriptable test scenarios

---

## Design Philosophy

### Why Two Picos?

**Modularity:**
- Detachable wheel rim
- Swap rims without rewiring base
- Physical separation (housing vs. wheel)

**Pin count:**
- Base needs: 15+ GPIOs (motors, sensors, pedals)
- Rim needs: 20+ GPIOs (buttons, encoders, LEDs)
- One Pico doesn't have enough!

**Proven protocol:**
- UART is reliable and fast (460800 baud)
- CRC16 validation
- Easy to debug

### Why Logitech Wheels (DFGT/G25/G27/G29/G920/G923)?

**Proven mechanics:**
- Dual-motor design (redundancy, smooth torque)
- Quality gearbox (~18:1 ratio)
- Sturdy construction
- Widely available used
- Same core platform across DFGT/G25/G27/G29/G920/G923
- Note: G29 = G920/G923 (MLX sensor), only G25/G27/DFGT use optical

**Cost:**
- Cheaper than building from scratch
- Motors + gearbox alone cost $200+
- Housing, bearings, mounting included
- DFGT often cheapest ($80-150 used)
- G25/G27/G29 also affordable ($100-200 used)

**Community:**
- Many builders use Logitech base platform
- Common troubleshooting knowledge
- Parts compatibility across models
- Easy to find used units

### Why Open Source?

**Transparency:**
- No black-box firmware
- Understand how FFB works
- Learn embedded systems

**Customization:**
- Add features you want
- Optimize for your games
- Experiment with algorithms

**Community:**
- Share improvements
- Collective debugging
- Educational resource

---

## Safety Considerations

### ⚠️ Risks

**Electrical:**
- Motor power (24V stock, 5-10A) can cause shock or fire
- 36V experimental (requires cooling, not recommended yet)
- Incorrect wiring can damage components
- No UL/CE certification (DIY project)

**Mechanical:**
- Motors generate significant torque (can pinch fingers)
- Spinning components (belt, gears)
- Sharp edges on disassembled parts

**Thermal:**
- Motors can overheat (>60°C)
- Motor drivers generate heat
- Fire risk if unmonitored

### ✅ Mitigations

**Built into firmware:**
- Motor watchdog (stops after 60s continuous)
- Communication watchdog (stops if USB lost)
- Settings validation (rejects dangerous values)
- Emergency stop command (`:d`)

**Build practices:**
- Proper wire gauge (18 AWG for motors)
- Fused power supply (5A for 24V stock)
- Isolated motor power (separate from USB)
- Can reuse stock G920/G923 24V PSU (free!)
- Heatsinks on drivers
- Temperature monitoring

**Usage:**
- Start with low power (`duty_cap=0.10` at 24V)
- Stick with 24V until 36V testing completes
- Monitor temperature during initial tests (<60°C)
- Don't leave running unattended during initial testing
- Use fused power supply

**See:** [Build Guide Safety Section](build-guide.md#safety-equipment)

---

## Community & Support

### Official Channels

**GitHub:**
- Issues: Bug reports, feature requests
- Discussions: Questions, build logs, sharing
- Pull Requests: Code contributions

**Documentation:**
- All guides in `docs/` directory
- FAQ for common questions
- Troubleshooting for problems

### Getting Help

**Before asking:**
1. Check [FAQ](faq.md)
2. Search GitHub Issues/Discussions
3. Review [Troubleshooting Guide](troubleshooting.md)

**When asking:**
- Provide firmware version (`:version`)
- Share serial log (`:dump`)
- Describe what you tried
- Include photos of wiring (if relevant)

**Response time:**
- Maintainers are volunteers
- Expect 1-2 weeks for responses
- Community members often help faster!

---

## Next Steps

### For Builders

**Ready to start?**

1. ✅ Read [FAQ](faq.md) - Make sure this is right for you
2. ✅ Read [Build Guide](build-guide.md) - Understand the process
3. ✅ Order parts - See BOM in build guide
4. ✅ Join community - GitHub Discussions

**Need more info?**
- Cost breakdown: [Build Guide > Project Cost](build-guide.md#project-cost-estimate)
- Skills needed: [Build Guide > Skill Level](build-guide.md#skill-level)
- Timeline: [Build Guide > Time Investment](build-guide.md#skill-level)

### For Developers

**Want to contribute?**

1. ✅ Read [CONTRIBUTING.md](../CONTRIBUTING.md) - Guidelines
2. ✅ Set up dev environment - See Getting Started
3. ✅ Try simulator - [Simulator Integration](simulator-integration.md)
4. ✅ Pick an issue - "good first issue" label

**Ideas to contribute:**
- Fix bugs (check GitHub Issues)
- Add features (suggest in Discussions first)
- Improve documentation
- Test on different hardware

### For Existing Owners

**Got your wheel working?**

1. ✅ Calibrate properly - [Calibration Guide](calibration-and-tuning.md)
2. ✅ Tune for your games - [Per-Game Settings](calibration-and-tuning.md#per-game-settings)
3. ✅ Share your build - Post photos in Discussions!
4. ✅ Help others - Answer questions

---

## Project Status

**Current state (v0.1.0):**
- ✅ Basic FFB (spring, manual torque)
- ✅ USB HID joystick
- ✅ Pedal support with auto-calibration
- ✅ Button panel + encoders
- ✅ LED shift lights
- ✅ Desktop configurator
- ✅ Comprehensive documentation
- ⚠️ Game PID effects (planned)
- ⚠️ Telemetry plugins for more games (planned)

**Roadmap:**
- Full game PID support (wheel acts like Logitech G29)
- More telemetry plugins (iRacing, ACC, etc.)
- Wireless button panel (Bluetooth)
- Direct drive motor support (higher torque)
- Custom PCB (cleaner assembly)
- 3D printable parts (coming soon)

**See:** [CHANGELOG.md](../CHANGELOG.md) for version history

---

## Frequently Asked Questions

### Is this better than a Logitech G29?

**Pros:**
- ✅ Fully customizable firmware
- ✅ Open protocol (add features)
- ✅ Better sensor (hall vs. potentiometer)
- ✅ Expandable (more buttons, displays, etc.)
- ✅ Learning experience

**Cons:**
- ❌ Requires assembly (15-25 hours)
- ❌ No warranty or support
- ❌ Similar FFB strength (same motors)
- ❌ You're responsible for safety

### Can I sell wheels I build?

**Legal:**
- Personal use: ✅ Legal
- Sharing designs/firmware: ✅ Legal (open source)
- Selling complete wheels: ⚠️ Check local laws (may need certification)
- Using Logitech branding: ❌ Trademark infringement

**Practical:**
- Building for friends: OK
- Small-scale sales: Risky (liability, certification)
- Better to share knowledge than products

### How hard is this to build?

**Skill level required:**
- Soldering: Intermediate (through-hole, wire-to-board)
- Electronics: Basic (voltage, resistance, polarity)
- Mechanical: Basic (screwdrivers, disassembly)
- Software: Basic (flashing firmware, serial terminal)

**Absolute beginner?**
- Start with soldering practice kits
- Watch YouTube tutorials
- Ask for help in community
- Budget extra time for learning

### What if something breaks?

**Common failures:**
- ❌ Blown Pico ($4-6 to replace)
- ❌ Fried motor driver ($5-10)
- ❌ Burned motor (hard to replace)

**Prevention:**
- Follow build guide exactly
- Double-check wiring before power-on
- Start with low power settings
- Use fuses on motor power

**Support:**
- Community can help diagnose
- Replacement parts are cheap (except motors)
- No manufacturer warranty (DIY risk)

---

## Credits & License

**Project maintainer:** [See GitHub contributors]

**License:** [Check LICENSE file in repository]

**Third-party libraries:**
- TinyUSB (MIT)
- Arduino-Pico (LGPL)
- Tauri (MIT/Apache)

**Community contributions:**
- Bug reports and testing
- Documentation improvements
- Code contributions
- Build guides and photos

**Thank you to all contributors!** 🎉

---

*Welcome to rp2040-ffb! We're excited to have you join the community.* 🏁

*Questions? Open a GitHub Discussion!*

*Last updated: 2024-09-12*
