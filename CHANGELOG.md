# Changelog

All notable changes to rp2040-ffb will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Comprehensive troubleshooting guide (docs/troubleshooting.md)
- CI/CD pipeline with linting, testing, and quality checks
- Firmware simulator for development without hardware (tools/firmware-sim/)
- Code formatting with clang-format and Prettier
- Pull request template with safety checklist
- Protocol version compatibility checking in CI
- Firmware size tracking in pull requests

### Changed
- Enhanced .gitignore with simulator artifacts

### Fixed

### Security

## [0.1.0] - 2024-09-12

### Added
- Initial release
- Dual RP2040 architecture (base + rim MCUs)
- MLX90363 hall sensor support
- BTS7960 motor driver integration
- Logitech pedal support (G29/G920/G923)
- UART link protocol (460.8 kbaud)
- Rim button panel (MCP23017 × 2)
- Paddle analog inputs (ADS1115)
- WS2812 shift light strip
- ILI9341 TFT display with multi-page layouts
- Tauri desktop configurator (ffb-config)
- LMU telemetry integration
- OTA firmware updates for rim
- Profile system (4 slots)
- E-paper status display
- ADXL345 accelerometer support for gravity-based homing

### Known Issues
- Full USB HID PID effects not yet implemented (spring mode only)
- Motorized homing still WIP (manual/magnet-based INIT works)

---

## Release Notes Guidelines

When creating a release, include:

### Version Number
- **MAJOR**: Breaking changes (incompatible firmware protocol, hardware changes)
- **MINOR**: New features, non-breaking additions
- **PATCH**: Bug fixes, documentation updates

### For Each PR
- Categorize under Added/Changed/Fixed/Deprecated/Removed/Security
- Reference issue numbers: `Fixes #123`
- Mention breaking changes prominently
- Note if base+rim firmware must be updated together

### Example Entry
```markdown
## [0.2.0] - 2024-10-01

### Added
- Full USB HID PID effect support (#42)
- Temperature monitoring for motor drivers (#56)
- SimHub integration alongside LMU (#61)

### Changed
- **BREAKING**: Protocol version 2 - requires base+rim update (#58)
- Increased max firmware size to 224KB for OTA staging (#59)

### Fixed
- Encoder debounce causing missed inputs (#54)
- CRC errors at high UART baud rates (#55)
- Memory leak in display page switching (#60)

### Security
- Added settings validation to prevent motor damage (#57)
```

[Unreleased]: https://github.com/skyne/rp2040-ffb/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/skyne/rp2040-ffb/releases/tag/v0.1.0
