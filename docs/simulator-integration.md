# Simulator Integration Guide

The rp2040-ffb GUI can run in **Simulator Mode** for hardware-free development and testing.

## Features

- ✅ Full GUI functionality without physical hardware
- ✅ Test settings, validation, and configuration
- ✅ Develop new features without needing a wheel
- ✅ Visual simulator mode indicator in GUI
- ✅ Auto-detection of simulator ports
- ✅ Realistic CDC command responses
- ✅ Settings persistence and validation

## Quick Start

### Option 1: Automatic (Recommended)

```bash
cd tools/ffb-config
npm run dev:sim
```

This will:
1. Create virtual serial ports
2. Start the firmware simulator
3. Launch the GUI
4. Show simulator mode banner

### Option 2: Manual Setup

**Terminal 1 - Create Virtual Ports:**

Linux/macOS:
```bash
socat -d -d pty,raw,echo=0,link=/tmp/ffb-sim-base pty,raw,echo=0,link=/tmp/ffb-sim-gui
```

Windows:
- Install com0com (search for "com0com windows virtual serial" - project hosted on various sites)
- Create COM10 <-> COM11 port pair

**Terminal 2 - Start Simulator:**

```bash
cd tools/firmware-sim
python3 -m venv venv
source venv/bin/activate  # Windows: venv\Scripts\activate
pip install -r requirements.txt
python sim_base.py --port /tmp/ffb-sim-base  # or COM10 on Windows
```

**Terminal 3 - Start GUI:**

```bash
cd tools/ffb-config
npm run tauri dev
```

**In GUI:**
- Select the simulator port (`/tmp/ffb-sim-gui` or `COM11`)
- Click Connect
- 🎮 Simulator banner should appear!

## Simulator Mode Indicators

When running in simulator mode, you'll see:

1. **Purple Banner** at the top of the GUI:
   ```
   🎮 SIMULATOR MODE - No hardware required
   ```

2. **Highlighted Ports** in the dropdown:
   - Simulator ports appear with 🎮 icon
   - Purple background color
   - "(Simulator)" suffix

3. **Status Messages:**
   - "🎮 Simulator mode active - Connect to virtual port"

## What Works in Simulator Mode

### ✅ Fully Functional

- All CDC commands (`:dump`, `:get`, `:set`, `:save`, etc.)
- Settings validation with error messages
- Firmware version reporting
- Self-test command
- Telemetry stream display
- All tabs and UI elements

### 🎮 Simulated Behavior

- **Pedal values:** Fixed at rest position (can be changed via `:sim` commands)
- **Steering angle:** Simulated sine wave or manual control
- **Rim link:** Enabled/disabled via `:sim rimlink 1/0`
- **Motor state:** Logged but no physical output

### ❌ Not Simulated

- Motor force feedback (no physical output)
- Actual button presses (no HID enumeration)
- E-paper display rendering
- Real-time sensor values

## Simulator Commands

Special commands available only in simulator:

```bash
# Set axle position
:sim axle 45.0         # Set to 45 degrees

# Set steering position  
:sim steer 180.0       # Set to 180 degrees

# Set pedal raw ADC value
:sim pedal thr 2000    # Set throttle to 2000 (0-4095)
:sim pedal brk 3500    # Set brake to 3500
:sim pedal clu 1500    # Set clutch to 1500

# Enable/disable rim link
:sim rimlink 1         # Enable rim
:sim rimlink 0         # Disable rim
```

## Testing Scenarios

### Test Settings Validation

```bash
# Connect to simulator
:set duty_cap 1.5
# ERROR: duty_cap out of range [0.0, 1.0]

:set duty_cap 0.60
# WARNING: duty_cap > 0.50 - USE CAUTION!

:set spring_k 0.05
# ERROR: spring_k out of safe range
```

### Test Profiles

```bash
:profile list
:profile save 0 TestProfile
:set duty_cap 0.25
:profile load 0
# Settings restored to profile
```

### Test Persistence

```bash
:set spring_k 0.008
:save
# Restart simulator
:dump
# spring_k=0.008 (persisted in virtual EEPROM)
```

## Development Workflow

### GUI Feature Development

1. Start simulator mode: `npm run dev:sim`
2. Develop UI features
3. Test settings changes
4. No hardware needed!

### Protocol Testing

1. Start simulator with logging
2. Send CDC commands from GUI
3. Verify responses in `/tmp/ffb-simulator.log`

### Integration Tests

```javascript
// tools/ffb-config/src/test/simulator.test.js
import { test, expect } from 'vitest';

test('simulator responds to dump command', async () => {
  // Connect to simulator port
  // Send :dump
  // Verify response contains expected keys
});
```

## Troubleshooting

### "No serial ports found"

**Linux/macOS:**
- Ensure `socat` is installed: `sudo apt-get install socat` (Linux) or `brew install socat` (macOS)
- Check virtual ports exist: `ls -l /tmp/ffb-sim-*`
- Restart socat if ports are stale

**Windows:**
- Install com0com (search for "com0com windows virtual serial")
- Configure COM10 <-> COM11 pair in setup
- Check ports in Device Manager

### "Simulator failed to start"

Check simulator log:
```bash
tail -f /tmp/ffb-simulator.log
```

Common issues:
- Python dependencies missing: `pip install -r requirements.txt`
- Port already in use: Kill existing simulator process
- Serial port permissions: Add user to `dialout` group (Linux)

### GUI doesn't detect simulator port

1. Refresh ports manually (click refresh button)
2. Check port exists: `ls -l /tmp/ffb-sim-gui`
3. Manually select port from dropdown
4. Restart GUI

### Commands not responding

1. Check simulator is running: `ps aux | grep sim_base`
2. Check virtual port connection: `ls -l /tmp/ffb-sim-*`
3. Restart simulator
4. Reconnect in GUI

## Platform-Specific Notes

### Linux

✅ Works out of the box with socat
- Install: `sudo apt-get install socat`
- Virtual ports: `/tmp/ffb-sim-base` and `/tmp/ffb-sim-gui`

### macOS

✅ Works with socat
- Install: `brew install socat`
- Virtual ports: `/tmp/ffb-sim-base` and `/tmp/ffb-sim-gui`

### Windows

⚠️ Requires com0com setup
- Download: Search for "com0com windows virtual serial"
- Configure COM port pair in setup
- Use ports: `COM10` (simulator) and `COM11` (GUI)
- Alternative: Use WSL2 with Linux instructions

## Advanced Usage

### Multiple Simulators

Run different simulator configurations:

```bash
# Terminal 1: Base with modified settings
python sim_base.py --port /tmp/ffb-sim1

# Terminal 2: Base with defaults
python sim_base.py --port /tmp/ffb-sim2
```

### Playback Mode

Record real hardware session and replay:

```bash
# On real hardware
python tools/firmware-sim/record_session.py --port /dev/ttyACM0 --output session.json

# In simulator
python sim_base.py --playback session.json --port /tmp/ffb-sim-base
```

### CI Integration

Run automated tests against simulator:

```yaml
# .github/workflows/gui-tests.yml
- name: Start simulator
  run: |
    python tools/firmware-sim/sim_base.py --port /tmp/ffb-sim &
    sleep 2

- name: Run GUI tests
  run: |
    cd tools/ffb-config
    npm run test:e2e -- --project=simulator
```

## Contributing

When adding new firmware features:

1. Update `sim_base.py` to handle new commands
2. Add test cases in `firmware-sim/tests/`
3. Document simulator behavior here
4. Test with `npm run dev:sim`

## See Also

- [Firmware Simulator README](../tools/firmware-sim/README.md)
- [Link Protocol](link-protocol.md)
- [GUI Development](../tools/ffb-config/README.md)
- [Testing Guide](../firmware-tests/README.md)
