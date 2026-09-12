# rp2040-ffb Firmware Simulator

Virtual wheel emulator for testing firmware logic without hardware.

## Overview

The firmware simulator allows you to:
- Test protocol parsing and frame generation
- Validate encoder logic and HID mapping
- Debug FFB calculations
- Test settings validation
- Run integration tests without physical hardware
- Develop the GUI without a connected wheel

## Architecture

```
┌─────────────────────────────────┐
│   ffb-config (or test suite)    │
│                                 │
└────────────┬────────────────────┘
             │ Virtual COM port
             │ (socat / PTY)
             ▼
┌─────────────────────────────────┐
│     firmware-sim (Python)       │
│                                 │
│  - Simulates base CDC responses │
│  - Emulates rim UART frames     │
│  - Virtual sensor readings      │
│  - Playback recorded sessions   │
│                                 │
└─────────────────────────────────┘
```

## Quick Start

### Install Dependencies

```bash
cd tools/firmware-sim
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```

### Run Simulator

```bash
# Create virtual serial ports
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Note the PTY paths, e.g. /dev/pts/4 and /dev/pts/5

# In another terminal, run simulator on one PTY
python sim_base.py --port /dev/pts/4

# Connect ffb-config to the other PTY: /dev/pts/5
```

### Windows (COM Port)

```bash
# Use com0com to create virtual serial ports
# Download from: https://sourceforge.net/projects/com0com/

# After installation, create port pair COM10<->COM11
# Run simulator on COM10, connect GUI to COM11
python sim_base.py --port COM10
```

## Features

### 1. CDC Command Simulation

Responds to all base CDC commands:

```python
# sim_base.py responds to:
:dump      → Full settings output
:get <key> → Individual setting
:set <key> <value> → Update setting (with validation)
:save      → Persist to virtual EEPROM
:version   → Firmware version
:selftest  → Simulated hardware check
h          → Help menu
```

### 2. Telemetry Stream

Generates realistic telemetry output:

```python
# Simulates steering, pedals, RPM
T axle=123.4 steer=342.1 rpm=6500 gear=3 flags=0 thr=2890 brk=2950 clu=3100
```

Configurable patterns:
- Sine wave steering
- Random pedal inputs
- RPM ramping
- Gear shifts

### 3. Rim Link Simulation

Emulates base ↔ rim UART:

```python
# sim_rim.py generates Input frames
# sim_base.py generates Telemetry/CfgSync frames
# Both parse and respond to protocol messages
```

### 4. Sensor Playback

Record and replay real sensor data:

```bash
# Record from real hardware
python record_session.py --port /dev/ttyACM0 --output session.json

# Replay in simulator
python sim_base.py --playback session.json
```

### 5. Fault Injection

Test error handling:

```python
# Simulate link timeouts
python sim_base.py --rim-timeout 2.0

# Inject CRC errors
python sim_base.py --crc-error-rate 0.05

# Simulate EEPROM corruption
python sim_base.py --corrupt-eeprom
```

## File Structure

```
tools/firmware-sim/
├── README.md
├── requirements.txt
├── sim_base.py          # Base MCU simulator
├── sim_rim.py           # Rim MCU simulator
├── protocol.py          # Frame parsing/generation
├── settings.py          # Virtual EEPROM
├── sensors.py           # Virtual sensor models
├── record_session.py    # Hardware session recorder
├── playback_session.py  # Session player
└── tests/
    ├── test_protocol.py
    ├── test_settings.py
    └── test_integration.py
```

## Usage Examples

### Test Settings Validation

```python
from sim_base import BaseSimulator

sim = BaseSimulator()

# Test invalid setting
result = sim.handle_command(":set duty_cap 1.5")
assert "out of range" in result

# Test valid setting with warning
result = sim.handle_command(":set duty_cap 0.60")
assert "WARNING" in result

# Test persistence
sim.handle_command(":set spring_k 0.005")
sim.handle_command(":save")
sim.restart()  # Simulate reboot
result = sim.handle_command(":get spring_k")
assert "0.005" in result
```

### Test Protocol Parsing

```python
from protocol import parse_frame, build_frame, crc16

# Build a valid frame
payload = b"\x01\x02\x03"
frame = build_frame(msg_type=0x10, payload=payload)

# Parse it back
msg_type, payload_out = parse_frame(frame)
assert msg_type == 0x10
assert payload_out == payload

# Test CRC validation
frame_corrupted = frame[:-1] + b"\xFF"
with pytest.raises(ValueError, match="CRC"):
    parse_frame(frame_corrupted)
```

### Integration Test

```python
import pytest
from sim_base import BaseSimulator

def test_pedal_calibration():
    sim = BaseSimulator()
    
    # Reset calibration
    sim.handle_command("p")
    
    # Check initial state (should be 0)
    result = sim.handle_command(":dump")
    assert "thr_min=0" in result
    
    # Simulate pedal press
    sim.set_pedal_raw("throttle", 500)
    sim.tick(100)  # Wait for calibration
    
    # Check learned min
    result = sim.handle_command(":dump")
    assert "thr_min=500" in result or similar
```

## API Reference

### BaseSimulator

```python
class BaseSimulator:
    def __init__(self, port=None):
        """Initialize simulator, optionally with serial port"""
    
    def handle_command(self, cmd: str) -> str:
        """Process CDC command, return response"""
    
    def tick(self, dt_ms: float):
        """Advance simulation time"""
    
    def set_pedal_raw(self, axis: str, value: int):
        """Set raw ADC value for pedal (0-4095)"""
    
    def set_axle_angle(self, deg: float):
        """Set absolute axle position"""
    
    def get_hid_state(self) -> dict:
        """Get current HID joystick state"""
    
    def inject_rim_frame(self, frame: bytes):
        """Simulate incoming rim frame"""
    
    def restart(self):
        """Simulate MCU reboot"""
```

### RimSimulator

```python
class RimSimulator:
    def __init__(self):
        """Initialize rim simulator"""
    
    def press_button(self, button_id: int):
        """Press panel button 1-10"""
    
    def release_button(self, button_id: int):
        """Release panel button"""
    
    def rotate_encoder(self, enc_id: int, steps: int):
        """Rotate encoder (negative = CCW)"""
    
    def set_paddle(self, paddle: str, value: int):
        """Set clutch paddle analog value"""
    
    def tick(self, dt_ms: float):
        """Generate Input frame every ~2ms"""
```

## Testing with pytest

```bash
cd tools/firmware-sim
pytest tests/ -v
```

Example test structure:

```python
# tests/test_integration.py
def test_full_startup_sequence(base_sim):
    # Test boot messages
    output = base_sim.get_boot_log()
    assert "rp2040-ffb" in output
    assert "OK selftest" in output
    
    # Test link establishment
    base_sim.connect_rim(rim_sim)
    assert base_sim.is_rim_linked()
    
    # Test config sync
    base_sim.handle_command(":rim_sync")
    assert rim_sim.has_received_config()
```

## GUI Development Mode

Run simulator automatically when GUI starts (no hardware):

```bash
# In ffb-config package.json
"scripts": {
  "dev:sim": "concurrently \"python ../firmware-sim/sim_base.py\" \"tauri dev\""
}

npm run dev:sim
```

The GUI will detect the simulator and show a banner: **"SIMULATOR MODE - No hardware required"**

## Continuous Integration

Use simulator in CI for integration tests:

```yaml
# .github/workflows/test.yml
- name: Run simulator tests
  run: |
    cd tools/firmware-sim
    pip install -r requirements.txt
    pytest tests/ -v --cov=. --cov-report=xml
```

## Limitations

- Motor torque feedback not simulated (no force output)
- Timing may not match real hardware exactly
- USB HID enumeration not emulated (CDC only)
- No E-paper display rendering
- No I²C device simulation on rim (MCP/ADS/ADXL always present)

## Future Enhancements

- [ ] WebUSB simulator in browser (no Python needed)
- [ ] 3D visualization of wheel position
- [ ] Load cell FFB output to G-Wheel
- [ ] Real-time profiling / performance analysis
- [ ] Fuzz testing for protocol robustness
- [ ] Multi-client testing (multiple GUIs)

## Contributing

When adding new firmware features:
1. Update `sim_base.py` or `sim_rim.py` to match
2. Add test cases in `tests/`
3. Update this README if API changes

This ensures features can be tested without hardware access.
