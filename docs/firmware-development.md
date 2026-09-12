# Firmware Development Guide

**For developers who want to modify or extend rp2040-ffb firmware**

This guide covers the firmware architecture, development workflow, and how to contribute changes.

---

## Table of Contents

1. [Development Environment Setup](#development-environment-setup)
2. [Firmware Architecture](#firmware-architecture)
3. [Code Organization](#code-organization)
4. [Build System](#build-system)
5. [Debugging Techniques](#debugging-techniques)
6. [Testing](#testing)
7. [Contributing Guidelines](#contributing-guidelines)
8. [Common Development Tasks](#common-development-tasks)

---

## Development Environment Setup

### Prerequisites

**Required software:**
- Python 3.8+ (for PlatformIO)
- Git
- PlatformIO Core or VSCode + PlatformIO extension
- Serial terminal (screen, minicom, or PlatformIO monitor)

**Optional but recommended:**
- Logic analyzer (for UART/SPI/I²C debugging)
- USB serial adapter (3.3V for hardware debugging)
- Oscilloscope (for PWM verification)

### Quick Start

```bash
# Clone repository
git clone https://github.com/your-org/rp2040-ffb.git
cd rp2040-ffb

# Install PlatformIO
pip install -U platformio

# Build base firmware
cd firmware-base
pio run

# Build rim firmware
cd ../firmware-rim
pio run

# Upload to connected Pico (hold BOOTSEL, plug USB)
pio run -t upload

# Open serial monitor
pio device monitor -b 115200
```

### VSCode Setup

**Install extensions:**
1. PlatformIO IDE
2. C/C++ (Microsoft)
3. C/C++ Extension Pack

**Workspace settings (.vscode/settings.json):**

```json
{
  "C_Cpp.default.configurationProvider": "platformio",
  "editor.formatOnSave": true,
  "clang-format.executable": "clang-format",
  "[cpp]": {
    "editor.defaultFormatter": "xaver.clang-format"
  },
  "files.associations": {
    "*.h": "cpp",
    "*.cpp": "cpp"
  }
}
```

---

## Firmware Architecture

### High-Level Overview

```
┌──────────────────────────────────────────────────────┐
│                   USB Host (PC)                      │
└───────────────────┬──────────────────────────────────┘
                    │ USB HID + CDC
                    ▼
        ┌───────────────────────┐
        │    Base MCU (RP2040)  │
        │  ┌─────────────────┐  │
        │  │   USB Stack     │  │ ← TinyUSB (HID joystick + CDC serial)
        │  └────────┬────────┘  │
        │           │           │
        │  ┌────────▼────────┐  │
        │  │   Main Loop     │  │ ← setup() + loop()
        │  │  ┌───────────┐  │  │
        │  │  │ CDC Shell │  │  │ ← Colon commands (:set, :save, :dump)
        │  │  ├───────────┤  │  │
        │  │  │ Telemetry │  │  │ ← Streams (T, G, E, X)
        │  │  ├───────────┤  │  │
        │  │  │   FFB     │  │  │ ← Force feedback modes
        │  │  ├───────────┤  │  │
        │  │  │  Safety   │  │  │ ← Watchdogs, validation
        │  │  ├───────────┤  │  │
        │  │  │ Settings  │  │  │ ← EEPROM persistence
        │  │  └───────────┘  │  │
        │  └─────────────────┘  │
        │           │           │
        │  ┌────────▼────────┐  │
        │  │  Hardware I/O   │  │
        │  │ ┌─────────────┐ │  │
        │  │ │ MLX90363    │ │  │ ← SPI angle sensor
        │  │ │ BTS7960 ×2  │ │  │ ← PWM motor drivers
        │  │ │ Pedals ADC  │ │  │ ← 3× analog inputs
        │  │ │ UART Link   │ │  │ ← Binary framed protocol
        │  │ └─────────────┘ │  │
        │  └─────────────────┘  │
        └───────────┬───────────┘
                    │ UART 460800 baud
                    ▼
        ┌───────────────────────┐
        │    Rim MCU (RP2040)   │
        │  ┌─────────────────┐  │
        │  │   UART Link     │  │ ← Receives telemetry, sends inputs
        │  └────────┬────────┘  │
        │           │           │
        │  ┌────────▼────────┐  │
        │  │  Input Polling  │  │
        │  │ ┌─────────────┐ │  │
        │  │ │ MCP23017 ×2 │ │  │ ← I²C button matrix + LED matrix
        │  │ │ ADS1115     │ │  │ ← I²C analog halls (paddles)
        │  │ │ Encoders ×4 │ │  │ ← GPIO quadrature inputs
        │  │ │ WS2812B     │ │  │ ← LED strip (shift lights)
        │  │ │ ILI9341     │ │  │ ← Optional TFT display
        │  │ └─────────────┘ │  │
        │  └─────────────────┘  │
        └───────────────────────┘
```

### Base MCU Subsystems

**1. USB Stack (TinyUSB):**
- HID joystick device (appears as game controller)
- CDC serial port (configuration interface)
- Runs in USB interrupt context

**2. Force Feedback Engine:**
- Multiple modes: Off, Spring, Damper, Friction, LMU Telemetry
- PID controller for spring centering
- Duty cycle limiting for safety
- Motor direction control (left/right)

**3. Settings Manager:**
- Key-value storage in EEPROM
- Persistence across power cycles
- Validation hooks (via Safety module)
- Default factory settings

**4. Safety Module:**
- Input validation (range checks, warnings)
- Motor watchdog (thermal protection)
- Communication watchdog (USB + UART timeouts)
- Emergency stop on fault

**5. Telemetry System:**
- Streaming data formats (T, G, E, X)
- Configurable output rates
- Human-readable + parseable

**6. Hardware Drivers:**
- MLX90363: SPI hall sensor
- BTS7960: PWM H-bridge control
- Pedals: ADC with calibration
- UART: Binary framed protocol with CRC16

### Rim MCU Subsystems

**1. UART Link:**
- Receives telemetry from base (for LEDs/display)
- Sends button/encoder state to base
- CRC16 validation
- Automatic reconnection

**2. Input Polling:**
- MCP23017: I²C GPIO expander (buttons + LEDs)
- ADS1115: 16-bit ADC for analog paddles
- Encoders: Interrupt-driven quadrature decoding
- Debouncing and filtering

**3. Output Drivers:**
- WS2812B: Addressable RGB LEDs (shift lights)
- ILI9341: SPI TFT display
- MCP23017: LED matrix control

---

## Code Organization

### Directory Structure

```
rp2040-ffb/
├── firmware-base/          # Base MCU firmware
│   ├── src/
│   │   ├── main.cpp        # Entry point: setup() + loop()
│   │   ├── config.h        # Pin definitions, constants
│   │   ├── usb_descriptors.cpp  # TinyUSB HID/CDC config
│   │   ├── motor_bts7960.cpp    # Motor driver
│   │   ├── mlx90363.cpp    # Hall sensor driver
│   │   ├── ffb.cpp         # Force feedback logic
│   │   ├── settings.cpp    # EEPROM settings
│   │   ├── safety.cpp      # Safety module
│   │   ├── telemetry.cpp   # Streaming output
│   │   ├── accessory_link.cpp   # UART to rim
│   │   └── ...
│   ├── platformio.ini      # Build config
│   └── test/               # Unit tests
│
├── firmware-rim/           # Rim MCU firmware
│   ├── src/
│   │   ├── main.cpp        # Entry point
│   │   ├── config.h        # Pin definitions
│   │   ├── mcp23017.cpp    # I²C GPIO expander
│   │   ├── ads1115.cpp     # I²C ADC
│   │   ├── encoder.cpp     # Quadrature decoder
│   │   ├── ws2812.cpp      # LED strip driver
│   │   ├── accessory_link.cpp   # UART to base
│   │   └── ...
│   └── platformio.ini
│
├── shared/                 # Common code (linked by both)
│   ├── ffb_link.h          # UART protocol definitions
│   └── crc16.h             # CRC16 implementation
│
├── firmware-tests/         # GoogleTest unit tests
│   ├── test_ffb.cpp
│   ├── test_safety.cpp
│   └── ...
│
└── tools/
    ├── ffb-config/         # GUI configurator
    └── firmware-sim/       # Python simulator
```

### Key Files

**`firmware-base/src/main.cpp`:**
- `setup()`: Initialize hardware, USB, load settings
- `loop()`: Poll sensors, process commands, update FFB

**`firmware-base/src/config.h`:**
- Pin assignments (`MOTOR_*`, `PEDAL_*`, `HALL_*`)
- Hardware constants (`GEAR_RATIO`, `AXLE_INDEX_ANGLE_DEG`)
- Feature flags (`ENABLE_EPAPER`, `ENABLE_LEDS`)

**`firmware-base/src/ffb.cpp`:**
- FFB mode state machine
- PID controller for spring mode
- LMU telemetry parser
- Motor command generation

**`firmware-base/src/safety.cpp`:**
- `validateDutyCap()`, `validateTorqueCap()`, etc.
- `MotorWatchdog::update()`
- `CommunicationWatchdog::update()`

**`shared/ffb_link.h`:**
- Message IDs (`MSG_BUTTONS`, `MSG_ENCODERS`, etc.)
- Frame structure (`0x7E` start, CRC16, payload)
- CRC16 implementation

---

## Build System

### PlatformIO Configuration

**`platformio.ini` (base firmware):**

```ini
[env:pico]
platform = raspberrypi
board = rpipico
framework = arduino

; Core settings
board_build.core = earlephilhower
board_build.filesystem_size = 0.5m

; Build flags
build_flags = 
    -Wall -Wextra
    -DPICO_STDIO_USB
    -DUSE_TINYUSB
    -I ../shared

; Dependencies
lib_deps = 
    adafruit/Adafruit MLX90393@^2.0.0
    adafruit/Adafruit MCP23017 Arduino Library@^2.1.0
    fastled/FastLED@^3.6.0

; Upload settings
upload_protocol = picotool

; Monitor settings
monitor_speed = 115200
monitor_filters = direct
```

### Build Targets

```bash
# Clean build directory
pio run -t clean

# Compile only
pio run

# Upload to device (hold BOOTSEL)
pio run -t upload

# Monitor serial output
pio device monitor

# Upload + monitor
pio run -t upload && pio device monitor

# Build for different environment
pio run -e pico_debug

# Run unit tests (native)
pio test

# Run unit tests (on device)
pio test -e pico

# Format code
clang-format -i src/**/*.cpp src/**/*.h
```

### Compiler Flags

**Production build:**
```ini
build_flags = 
    -O2                      # Optimize for speed
    -DNDEBUG                 # Disable asserts
    -Wall -Wextra            # All warnings
```

**Debug build:**
```ini
build_flags = 
    -Og                      # Optimize for debugging
    -g                       # Debug symbols
    -DDEBUG                  # Enable debug prints
    -fsanitize=address       # Address sanitizer (native only)
```

---

## Debugging Techniques

### Serial Debugging

**Basic serial print:**

```cpp
Serial.begin(115200);
Serial.println("Debug message");
Serial.printf("Value: %d\n", value);
```

**Conditional debug prints:**

```cpp
#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// Usage:
DEBUG_PRINTLN("Entering setup()");
DEBUG_PRINT("Sensor value: ");
DEBUG_PRINTLN(sensorValue);
```

### Logic Analyzer

**GPIO pin toggling for timing analysis:**

```cpp
// In config.h
#define DEBUG_PIN_FFB GP22

// In code
pinMode(DEBUG_PIN_FFB, OUTPUT);

void updateFFB() {
    digitalWrite(DEBUG_PIN_FFB, HIGH);  // Mark start
    // ... FFB logic here ...
    digitalWrite(DEBUG_PIN_FFB, LOW);   // Mark end
}

// Connect logic analyzer to GP22 to measure execution time
```

### Crash Debugging

**Hard fault handler (Pico SDK):**

```cpp
extern "C" void HardFault_Handler(void) {
    Serial.println("!!! HARD FAULT !!!");
    Serial.flush();
    while (1) {
        // Blink LED rapidly
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        delay(100);
    }
}
```

**Stack trace (with debug build):**

1. Build with `-g` flag
2. Capture crash address from serial
3. Use `arm-none-eabi-addr2line -e firmware.elf <address>`

### USB Debugging

**Monitor USB HID reports:**

```bash
# Linux: Use evtest
sudo evtest /dev/input/event[X]

# Windows: Use Joystick Test application

# macOS: Use USB Prober
```

### UART Protocol Debugging

**Dump frames to serial:**

```cpp
void debugFrame(const uint8_t* frame, size_t len) {
    Serial.print("FRAME [");
    for (size_t i = 0; i < len; i++) {
        Serial.printf("%02X ", frame[i]);
    }
    Serial.println("]");
}
```

**Use logic analyzer on UART pins:**
- Capture TX/RX traffic at 460800 baud
- Check frame structure: `0x7E [ID] [LEN] [DATA...] [CRC16]`
- Verify CRC16 with online calculator

---

## Testing

### Unit Tests (GoogleTest)

**Test structure:**

```cpp
// firmware-tests/test_safety.cpp
#include <gtest/gtest.h>
#include "../firmware-base/src/safety.h"

TEST(SafetyTest, ValidateDutyCap_InRange) {
    auto result = Safety::validateDutyCap(0.25f);
    EXPECT_EQ(result.level, Safety::ValidationResult::OK);
}

TEST(SafetyTest, ValidateDutyCap_TooHigh) {
    auto result = Safety::validateDutyCap(1.5f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Error);
}
```

**Run tests:**

```bash
cd firmware-tests
cmake -S . -B build
cmake --build build
./build/test_runner
```

### Hardware-in-Loop Tests

**Simulator-based testing:**

```bash
# Start simulator
python3 tools/firmware-sim/sim_base.py /tmp/ffb-sim-cdc

# Connect GUI or script
tools/ffb-config/start-with-simulator.sh

# Run test script
python3 test-script.py /tmp/ffb-sim-cdc
```

**Example test script:**

```python
import serial

port = serial.Serial('/tmp/ffb-sim-cdc', 115200, timeout=1)

# Send command
port.write(b':version\n')
response = port.readline().decode().strip()
assert 'rp2040-ffb' in response

# Check telemetry
port.write(b't\n')  # Enable telemetry
data = port.read(100)
assert b'thr=' in data

print("Test passed!")
```

### Manual Testing Checklist

Before release:

- [ ] USB enumeration (lsusb shows device)
- [ ] HID joystick appears in game controllers
- [ ] CDC serial port accessible
- [ ] All commands respond (`:version`, `:dump`, `:selftest`)
- [ ] FFB spring mode centers wheel
- [ ] Motors stop on emergency stop (`d` command)
- [ ] Pedals read correctly (T stream)
- [ ] Rim buttons appear in HID
- [ ] LED strip responds to telemetry
- [ ] Settings persist after reboot
- [ ] Safety watchdogs trigger correctly

---

## Contributing Guidelines

### Code Style

**Follow `.clang-format` settings:**

```bash
clang-format -i src/**/*.cpp src/**/*.h
```

**Naming conventions:**
- Classes: `PascalCase` (e.g., `MotorBts7960`)
- Functions: `camelCase` (e.g., `updateFFB()`)
- Constants: `kPascalCase` (e.g., `kMaxDutyCap`)
- Macros: `UPPER_SNAKE_CASE` (e.g., `MOTOR_RPWM_PIN`)

**Comments:**
- Document non-obvious intent, not syntax
- Use `//` for single-line, `/* */` for multi-line
- Avoid redundant comments like `// Increment counter`

### Git Workflow

**Branch naming:**
```
feature/add-haptic-feedback
fix/uart-crc-error
docs/update-build-guide
```

**Commit messages:**
```
Short summary (50 chars or less)

Longer explanation if needed, wrapping at 72 characters.
Explain what changed and why, not how (code shows how).

Fixes #123
```

**Pull request checklist:**
- [ ] Code follows style guide
- [ ] All tests pass (`pio test`)
- [ ] Documentation updated (if needed)
- [ ] CHANGELOG.md updated
- [ ] PR description filled out (use template)
- [ ] No merge conflicts with main
- [ ] CI checks green

### Review Process

1. **Submit PR** with clear description
2. **Automated checks** run (lint, build, test)
3. **Code review** by maintainer
4. **Address feedback** with new commits
5. **Squash and merge** when approved

---

## Common Development Tasks

### Adding a New Setting

**1. Define in settings namespace:**

```cpp
// settings.h
namespace Settings {
    struct GlobalSettings {
        // ... existing fields ...
        float newParameter = 1.0f;  // Default value
    };
}
```

**2. Add getter/setter:**

```cpp
// settings.cpp
bool setFloat(const char* key, float value) {
    // ... existing code ...
    if (keyEq(key, "new_parameter")) {
        g.newParameter = value;
        markCustom();
        return true;
    }
    // ...
}
```

**3. Add to dump command:**

```cpp
void dumpSettings() {
    // ... existing dumps ...
    Serial.print("new_parameter=");
    Serial.println(g.newParameter, 4);
}
```

**4. Add EEPROM persistence (if needed):**

```cpp
struct EEPROMLayout {
    uint32_t magic;
    // ... existing fields ...
    float newParameter;
    // ...
};
```

### Adding a New FFB Mode

**1. Define mode enum:**

```cpp
// ffb.h
namespace Ffb {
    enum class Mode {
        Off,
        Spring,
        Damper,
        Friction,
        LmuTelemetry,
        NewMode  // Add here
    };
}
```

**2. Implement mode logic:**

```cpp
// ffb.cpp
void updateMode() {
    switch (currentMode) {
        case Mode::NewMode:
            updateNewMode();
            break;
        // ... other cases ...
    }
}

void updateNewMode() {
    // Your FFB logic here
    float torque = calculateNewModeTorque();
    MotorBts7960::applyTorque(torque);
}
```

**3. Add command to enter mode:**

```cpp
// main.cpp
if (cmd == "n") {
    Ffb::setMode(Ffb::Mode::NewMode);
    Serial.println("New mode activated");
}
```

### Adding Hardware Support

**Example: Adding a new sensor**

**1. Define pins:**

```cpp
// config.h
#define NEW_SENSOR_CS_PIN   GP23
#define NEW_SENSOR_SCK_PIN  GP24
```

**2. Create driver file:**

```cpp
// new_sensor.h
#pragma once
#include <Arduino.h>

class NewSensor {
public:
    static bool init();
    static float read();
private:
    static bool initialized_;
};
```

```cpp
// new_sensor.cpp
#include "new_sensor.h"

bool NewSensor::initialized_ = false;

bool NewSensor::init() {
    pinMode(NEW_SENSOR_CS_PIN, OUTPUT);
    digitalWrite(NEW_SENSOR_CS_PIN, HIGH);
    // ... SPI init ...
    initialized_ = true;
    return true;
}

float NewSensor::read() {
    if (!initialized_) return 0.0f;
    // ... SPI transaction ...
    return value;
}
```

**3. Integrate in main loop:**

```cpp
// main.cpp
#include "new_sensor.h"

void setup() {
    // ... existing setup ...
    if (NewSensor::init()) {
        Serial.println("New sensor OK");
    }
}

void loop() {
    float value = NewSensor::read();
    // Use value...
}
```

---

## Performance Optimization

### Timing Constraints

**Critical paths (must be fast):**
- USB HID report generation: < 1ms (1000Hz)
- FFB update loop: < 5ms (200Hz)
- UART frame processing: < 10ms

**Non-critical paths (can be slower):**
- CDC command parsing: < 100ms
- Settings EEPROM write: < 1s
- Telemetry output: < 50ms

### Profiling

**Measure loop time:**

```cpp
uint32_t loopStartUs = micros();
// ... loop body ...
uint32_t loopTimeUs = micros() - loopStartUs;

static uint32_t maxLoopTime = 0;
if (loopTimeUs > maxLoopTime) {
    maxLoopTime = loopTimeUs;
    Serial.printf("New max loop: %lu us\n", maxLoopTime);
}
```

**Optimize hot paths:**
- Avoid `Serial.print()` in fast loops
- Use `constexpr` for compile-time constants
- Cache sensor readings if polling is slow
- Use DMA for SPI/I²C if needed

---

## Further Reading

- [PlatformIO Documentation](https://docs.platformio.org/)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [Arduino-Pico Core](https://github.com/earlephilhower/arduino-pico)

---

*Last updated: 2024-09-12*
