# Firmware Unit Tests

This directory contains unit tests for the rp2040-ffb firmware using Google Test.

## Structure

```
firmware-tests/
├── CMakeLists.txt          # Build configuration
├── mocks/                  # Hardware abstraction layer mocks
│   ├── Arduino.h           # Arduino API mock
│   ├── arduino_mock.cpp
│   ├── EEPROM.h            # EEPROM mock
│   └── hal_*.cpp           # GPIO, SPI, I2C mocks
├── testable/               # Testable firmware logic
│   ├── settings_testable.cpp
│   ├── ffb_testable.cpp
│   ├── pedals_testable.cpp
│   └── link_protocol_testable.cpp
└── tests/                  # Unit tests
    ├── test_settings.cpp
    ├── test_ffb.cpp
    ├── test_pedals.cpp
    ├── test_link_protocol.cpp
    ├── test_crc.cpp
    └── test_profile_management.cpp
```

## Building and Running

### Prerequisites

- CMake 3.14+
- C++17 compiler (GCC, Clang, or MSVC)
- Internet connection (for downloading GoogleTest)

### Build

```bash
cd firmware-tests
mkdir build
cd build
cmake ..
cmake --build .
```

### Run Tests

```bash
# Run all tests
./firmware_unit_tests

# Run with verbose output
./firmware_unit_tests --gtest_verbose

# Run specific test suite
./firmware_unit_tests --gtest_filter=FfbTest.*

# Generate XML output for CI
./firmware_unit_tests --gtest_output=xml:test_results.xml
```

### Coverage

To generate coverage reports:

```bash
cmake -DCODE_COVERAGE=ON ..
cmake --build .
./firmware_unit_tests
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --list coverage.info
genhtml coverage.info --output-directory coverage_html
```

## Test Organization

### Settings Tests (`test_settings.cpp`)
- Configuration clamping
- Soft limit calculations
- Profile slot validation
- CRC implementations

### FFB Tests (`test_ffb.cpp`)
- Spring force calculations
- Soft limit forces
- Torque capping
- Force combination logic

### Pedals Tests (`test_pedals.cpp`)
- Axis normalization
- Calibration learning
- Min/max tracking
- Inversion logic

### Link Protocol Tests (`test_link_protocol.cpp`)
- Frame building/parsing
- CRC validation
- Sync byte handling
- Error detection

### CRC Tests (`test_crc.cpp`)
- CRC16-CCITT implementation
- CRC32-IEEE implementation
- Known test vectors

## Adding New Tests

1. Extract testable logic from firmware source
2. Add to `testable/<module>_testable.cpp`
3. Create test file in `tests/test_<module>.cpp`
4. Update `CMakeLists.txt` if adding new files
5. Build and run to verify

## Mocking Strategy

The HAL mocks provide:
- **Arduino.h**: Core Arduino functions (pinMode, digitalWrite, millis, etc.)
- **EEPROM.h**: Persistent storage simulation
- **Serial**: Output capture for verification

Test control functions allow setting pin states, time, and verifying outputs.

## Current Coverage

The tests focus on:
- ✅ Pure logic functions (calculations, algorithms)
- ✅ Data structure manipulation
- ✅ Protocol handling
- ✅ CRC/checksum validation
- ✅ Safety validation (duty cap, torque cap, limits)
- ✅ UART integration (ByteRing + framing)
- ✅ Watchdog timers (motor thermal, communication)
- ⚠️ Hardware interaction (mocked)
- ❌ I2C/SPI device communication (stubs)
- ❌ Motor control timing (future)

## Simulator Testing

The telemetry simulator has its own Python test suite:

```bash
cd tools
python3 test_lmu_telem_sim.py
```

Tests cover:
- Track waypoint consistency
- Speed interpolation
- Gear and RPM calculation
- Fuel consumption
- Lap timing and position wraparound
- UDP packet generation
- Full lap simulation
