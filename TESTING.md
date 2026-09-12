# Testing Guide

This document describes the comprehensive testing strategy for the rp2040-ffb project.

## Overview

The project uses a multi-layered testing approach:

1. **C++ Unit Tests** - Firmware logic testing with Google Test
2. **Rust Unit Tests** - Tauri backend testing with cargo test  
3. **JavaScript Unit Tests** - Frontend testing with Vitest
4. **Mutation Testing** - Test quality verification with Stryker & cargo-mutants
5. **Coverage Reporting** - Code coverage tracking across all layers

## Quick Start

### Run All Tests

```bash
# Firmware tests
cd firmware-tests && mkdir -p build && cd build
cmake .. && cmake --build .
./firmware_unit_tests

# Rust backend tests
cd tools/ffb-config/src-tauri
cargo test

# JavaScript frontend tests
cd tools/ffb-config
npm test

# With coverage
npm run test:coverage
```

### Run Mutation Tests

```bash
# JavaScript mutation testing
cd tools/ffb-config
npm run test:mutants

# Rust mutation testing
cd tools/ffb-config/src-tauri
cargo mutants
```

## Test Structure

```
rp2040-ffb/
├── firmware-tests/              # C++ firmware unit tests
│   ├── CMakeLists.txt
│   ├── mocks/                   # Hardware abstraction mocks
│   ├── testable/                # Extracted testable logic
│   ├── tests/                   # Test files
│   └── README.md
│
├── tools/ffb-config/
│   ├── src/test/                # JavaScript tests
│   │   ├── setup.js
│   │   ├── telemetry.test.js
│   │   ├── display-editor.test.js
│   │   └── utils.test.js
│   ├── src-tauri/src/
│   │   └── lib_test.rs          # Rust tests
│   ├── vitest.config.js
│   ├── stryker.config.json
│   └── .cargo-mutants.toml
│
└── .github/workflows/test.yml   # CI configuration
```

## Firmware Tests (C++)

### Architecture

The firmware tests use a hardware abstraction layer (HAL) to mock Arduino APIs:

- **Arduino.h** - Core Arduino functions (digitalWrite, millis, etc.)
- **EEPROM.h** - Persistent storage simulation
- **MockSerial** - Serial output capture

### Test Organization

- `test_settings.cpp` - Configuration & profile management
- `test_ffb.cpp` - Force feedback calculations
- `test_pedals.cpp` - Pedal calibration & normalization
- `test_link_protocol.cpp` - Base↔rim communication protocol
- `test_crc.cpp` - CRC16/CRC32 implementations
- `test_profile_management.cpp` - Profile slot validation

### Running Firmware Tests

```bash
cd firmware-tests
mkdir -p build && cd build
cmake ..
cmake --build .
./firmware_unit_tests

# With verbose output
./firmware_unit_tests --gtest_verbose

# Run specific test suite
./firmware_unit_tests --gtest_filter=FfbTest.*
```

### Coverage

```bash
cmake -DCODE_COVERAGE=ON ..
cmake --build .
./firmware_unit_tests
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/googletest/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
# Open coverage_html/index.html in browser
```

## Rust Tests

### Test Organization

Rust tests are embedded in `src-tauri/src/lib_test.rs` and test:

- CRC calculations (CRC16-CCITT, CRC32-IEEE)
- UF2 file detection
- Frame building/parsing for link protocol
- Port detection logic
- Telemetry parsing
- Key-value map parsing

### Running Rust Tests

```bash
cd tools/ffb-config/src-tauri
cargo test

# With output
cargo test -- --nocapture

# Specific test
cargo test test_crc16

# Coverage
cargo install cargo-llvm-cov
cargo llvm-cov test --html
# Open target/llvm-cov/html/index.html
```

### Mutation Testing (Rust)

```bash
cargo install cargo-mutants
cargo mutants -- --all-features
# Results in mutants.out/
```

## JavaScript Tests

### Test Organization

- `telemetry.test.js` - Telemetry line parsing
- `display-editor.test.js` - RGB565 conversion, layout encoding/decoding
- `utils.test.js` - Utility functions (clamping, time formatting)

### Running JavaScript Tests

```bash
cd tools/ffb-config
npm test

# Watch mode
npm run test:watch

# With UI
npm run test:ui

# Coverage
npm run test:coverage
# Open coverage/index.html
```

### Mutation Testing (JavaScript)

```bash
npm run test:mutants
# Results in reports/mutation/html/index.html
```

## Continuous Integration

Tests run automatically on:
- Push to `main` or `dev` branches
- Pull requests
- Manual trigger via GitHub Actions UI

The CI workflow (`.github/workflows/test.yml`) runs:

1. **Firmware Tests** - C++ unit tests with coverage
2. **Rust Tests** - Backend tests with coverage
3. **JavaScript Tests** - Frontend tests with coverage
4. **Mutation Testing** (PR only) - Both Stryker and cargo-mutants
5. **Test Summary** - Aggregates results and blocks merge if any test fails

Coverage reports are uploaded to Codecov.

### Branch Protection & Merge Gates

The `main` branch is protected with required status checks:

✅ **Required Checks (Must Pass to Merge):**
- `Test Summary` - Overall pass/fail gate
- `firmware-tests` - C++ unit tests
- `rust-tests` - Rust backend tests  
- `javascript-tests` - JavaScript frontend tests

❌ **PRs cannot be merged** until all required checks pass.

**Setup branch protection:**
```bash
.github/scripts/setup-branch-protection.sh
```

See `.github/BRANCH_PROTECTION.md` for detailed configuration.

## Coverage Goals

| Component  | Target | Current |
|------------|--------|---------|
| Firmware   | 70%+   | ~60%    |
| Rust       | 80%+   | ~65%    |
| JavaScript | 80%+   | ~70%    |

## Mutation Testing Thresholds

### JavaScript (Stryker)
- **High**: 80% mutation score
- **Low**: 60% mutation score  
- **Break**: 50% mutation score (fails build)

### Rust (cargo-mutants)
- Target: Catch 70%+ of introduced mutants
- Timeout: 2x baseline test time

## Best Practices

### Writing Tests

1. **Test behavior, not implementation** - Focus on inputs/outputs
2. **One assertion per concept** - Each test should verify one thing
3. **Descriptive names** - Test names should describe what they verify
4. **Arrange-Act-Assert** - Structure tests clearly
5. **Independent tests** - Tests should not depend on each other

### Firmware Tests

```cpp
TEST_F(FfbTest, SpringForceZeroInDeadzone) {
    // Arrange
    float axleDeg = 1.0f;
    float springK = 0.004f;
    float deadzone = 2.0f;
    
    // Act
    float force = FfbTestable::calculateSpringForce(axleDeg, springK, deadzone);
    
    // Assert
    EXPECT_FLOAT_EQ(force, 0.0f);
}
```

### Rust Tests

```rust
#[test]
fn test_crc16_deterministic() {
    let data = vec![0x01, 0x02, 0x03, 0x04];
    let crc1 = crc16_ccitt(&data);
    let crc2 = crc16_ccitt(&data);
    assert_eq!(crc1, crc2, "CRC16 should be deterministic");
}
```

### JavaScript Tests

```javascript
it('should clamp below minimum', () => {
    expect(clampf(-5, 0, 10)).toBe(0);
});
```

## Adding New Tests

### Firmware

1. Extract testable logic to `testable/<module>_testable.cpp`
2. Create test file `tests/test_<module>.cpp`
3. Update `CMakeLists.txt` if needed
4. Build and run

### Rust

1. Add tests to `lib_test.rs` or inline with `#[cfg(test)]`
2. Run `cargo test`

### JavaScript

1. Create `*.test.js` in `src/test/`
2. Import functions to test
3. Write tests with Vitest
4. Run `npm test`

## Troubleshooting

### Firmware tests won't build

- Ensure CMake 3.14+
- Check C++17 compiler available
- Try `rm -rf build && mkdir build`

### Rust tests fail to compile

- Run `cargo clean`
- Check Rust toolchain: `rustup update`
- Verify system dependencies installed

### JavaScript tests timeout

- Increase timeout in `vitest.config.js`
- Check for infinite loops in test code
- Verify mocks are properly set up

### Mutation tests take too long

- Reduce `concurrency` in config
- Use `--timeout` flag to limit per-mutant time
- Test specific files: `stryker run --mutate src/specific.js`

## Resources

- [Google Test Documentation](https://google.github.io/googletest/)
- [Vitest Documentation](https://vitest.dev/)
- [Stryker Mutator](https://stryker-mutator.io/)
- [cargo-mutants](https://github.com/sourcefrog/cargo-mutants)
- [Coverage.py](https://coverage.readthedocs.io/)

## Contributing

When contributing code:

1. ✅ Write tests for new features
2. ✅ Ensure existing tests pass
3. ✅ Maintain or improve coverage
4. ✅ Run mutation tests on changed code
5. ✅ Update this document if test structure changes
