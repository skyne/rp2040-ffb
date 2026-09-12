# Contributing to rp2040-ffb

Thank you for your interest in contributing to rp2040-ffb! This document provides guidelines and instructions for contributing to the project.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [How Can I Contribute?](#how-can-i-contribute)
3. [Getting Started](#getting-started)
4. [Development Workflow](#development-workflow)
5. [Coding Standards](#coding-standards)
6. [Testing Requirements](#testing-requirements)
7. [Documentation](#documentation)
8. [Pull Request Process](#pull-request-process)
9. [Issue Guidelines](#issue-guidelines)
10. [Community](#community)

---

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for all contributors, regardless of:
- Experience level
- Background or identity
- Technology preferences
- Hardware or software setup

### Expected Behavior

- ✅ Be respectful and considerate
- ✅ Welcome newcomers and help them learn
- ✅ Accept constructive criticism gracefully
- ✅ Focus on what's best for the project
- ✅ Show empathy towards other community members

### Unacceptable Behavior

- ❌ Harassment, discrimination, or personal attacks
- ❌ Trolling, insulting comments, or derailing discussions
- ❌ Publishing others' private information
- ❌ Spamming or excessive self-promotion

**Enforcement:** Violations may result in warnings, temporary bans, or permanent removal from the project.

---

## How Can I Contribute?

### 1. Reporting Bugs

Found a bug? Please open a GitHub Issue with:

**Required information:**
- Firmware version (`:version` command)
- Hardware setup (G920/G923, custom parts)
- Steps to reproduce
- Expected vs. actual behavior
- Serial log output (`:dump` command)
- Screenshots or videos (if applicable)

**Bug report template:**
```markdown
**Firmware version:** v0.1.0
**Hardware:** G920, dual BTS7960

**Steps to reproduce:**
1. Enable motors: `e`
2. Activate spring mode: `s`
3. ...

**Expected:** Motors should center wheel
**Actual:** Motors do not activate

**Serial log:**
[paste output here]
```

### 2. Suggesting Features

Have an idea? Open a GitHub Discussion (not an Issue) with:

- Clear description of the feature
- Use cases and benefits
- Potential implementation approach
- Hardware/software requirements
- Alternatives considered

**Feature request template:**
```markdown
**Feature:** Add support for haptic feedback

**Use case:** Provide vibration feedback for curbs, crashes

**Proposed implementation:**
- Add vibration motor to rim
- Parse game telemetry for vibration events
- PWM control via spare GPIO

**Hardware needed:** Vibration motor, driver circuit

**Alternatives:**
- Use WS2812 LEDs for visual feedback instead
```

### 3. Writing Code

Contributions are welcome for:
- Bug fixes
- New features
- Performance improvements
- Code refactoring
- Hardware support (new sensors, displays)

**Before starting:**
1. Check existing Issues and Pull Requests
2. Open an Issue to discuss major changes
3. Fork the repository
4. Create a feature branch

### 4. Improving Documentation

Documentation is crucial! Contribute by:
- Fixing typos or errors
- Adding examples and tutorials
- Translating to other languages
- Creating build logs / photos
- Writing troubleshooting guides

**Documentation types:**
- `README.md`: Project overview
- `docs/`: Detailed guides (build, firmware, calibration)
- Code comments: Explain non-obvious logic
- CHANGELOG.md: Track changes

### 5. Helping Others

Not a coder? You can still help by:
- Answering questions in Discussions
- Sharing your build experience
- Testing beta features
- Reporting what works (not just what breaks!)

---

## Getting Started

### Prerequisites

**Required:**
- Git
- Python 3.8+
- PlatformIO Core (`pip install platformio`)
- C++ compiler (via PlatformIO)

**Recommended:**
- VSCode + PlatformIO IDE extension
- Raspberry Pi Pico (for hardware testing)
- Serial terminal (screen, minicom, or PlatformIO monitor)

### Fork and Clone

```bash
# Fork on GitHub first, then:
git clone https://github.com/YOUR_USERNAME/rp2040-ffb.git
cd rp2040-ffb

# Add upstream remote
git remote add upstream https://github.com/ORIGINAL_OWNER/rp2040-ffb.git
```

### Build the Project

```bash
# Base firmware
cd firmware-base
pio run

# Rim firmware
cd ../firmware-rim
pio run

# Run tests
cd ../firmware-tests
cmake -S . -B build
cmake --build build
./build/test_runner
```

### Keep Your Fork Updated

```bash
git fetch upstream
git checkout main
git merge upstream/main
git push origin main
```

---

## Development Workflow

### Branch Naming

Use descriptive names:

```
feature/add-sequential-shifter
fix/uart-crc-validation
docs/improve-build-guide
refactor/safety-module
test/add-ffb-unit-tests
```

**Pattern:** `type/short-description`

**Types:**
- `feature/`: New functionality
- `fix/`: Bug fixes
- `docs/`: Documentation changes
- `refactor/`: Code restructuring (no behavior change)
- `test/`: Test additions or fixes
- `ci/`: CI/CD changes

### Commit Messages

**Format:**

```
Short summary (50 chars or less)

Longer explanation if needed, wrapping at 72 characters.
Explain what changed and WHY, not how (code shows how).

- Bullet points are OK
- Use present tense: "Add feature" not "Added feature"

Fixes #123
```

**Examples:**

Good:
```
Add motor thermal protection watchdog

Motors can overheat during prolonged use, damaging hardware.
This adds a watchdog that stops motors after 60 seconds of
continuous operation, forcing a 10-second cooldown.

Fixes #45
```

Bad:
```
Updated stuff
```

### Typical Workflow

1. **Create branch:**
   ```bash
   git checkout -b feature/my-feature
   ```

2. **Make changes:**
   - Write code
   - Add tests
   - Update documentation

3. **Test locally:**
   ```bash
   pio run              # Build
   pio test             # Run tests
   clang-format -i src/**/*.{cpp,h}  # Format code
   ```

4. **Commit:**
   ```bash
   git add .
   git commit -m "Add my feature"
   ```

5. **Push:**
   ```bash
   git push origin feature/my-feature
   ```

6. **Open Pull Request** on GitHub

---

## Coding Standards

### C++ Style (Firmware)

**Follow `.clang-format` configuration:**

```bash
clang-format -i firmware-base/src/**/*.cpp
clang-format -i firmware-base/src/**/*.h
```

**Naming conventions:**

```cpp
// Classes: PascalCase
class MotorBts7960 { };

// Functions: camelCase
void updateFFB();

// Constants: kPascalCase
constexpr float kMaxDutyCap = 1.0f;

// Macros: UPPER_SNAKE_CASE
#define MOTOR_RPWM_PIN GP10

// Variables: camelCase
float dutyCap = 0.5f;
int motorState = 0;

// Namespaces: PascalCase
namespace Safety { }
```

**Comments:**

```cpp
// Good: Explain WHY, not WHAT
// Disable motors if USB drops to prevent runaway
if (!usbAlive) {
    MotorBts7960::stop();
}

// Bad: Redundant
// Set dutyCap to 0.5
dutyCap = 0.5f;
```

**File organization:**

```cpp
// 1. Header guard / pragma once
#pragma once

// 2. System includes
#include <Arduino.h>
#include <SPI.h>

// 3. Project includes
#include "config.h"
#include "motor_bts7960.h"

// 4. Namespace / class declaration
namespace Ffb {

class Controller {
public:
    // Public interface first
    static void init();
    static void update();
    
private:
    // Private implementation last
    static Mode currentMode_;
    static void updateSpring();
};

} // namespace Ffb
```

### JavaScript Style (GUI)

**Use ESLint and Prettier:**

```bash
cd tools/ffb-config
npm run lint
npm run format
```

**Conventions:**

```javascript
// Variables: camelCase
const serialPort = '/dev/ttyACM0';

// Constants: UPPER_SNAKE_CASE
const DEFAULT_BAUD_RATE = 115200;

// Functions: camelCase
function connectToDevice() { }

// Classes: PascalCase
class SerialManager { }

// Use const/let, not var
const immutableValue = 42;
let mutableValue = 0;

// Arrow functions for callbacks
button.addEventListener('click', () => {
    console.log('Clicked');
});
```

### Python Style (Simulator)

**Follow PEP 8:**

```python
# Functions: snake_case
def calculate_crc16(data: bytes) -> int:
    pass

# Classes: PascalCase
class BaseSimulator:
    pass

# Constants: UPPER_SNAKE_CASE
DEFAULT_SERIAL_PORT = '/tmp/ffb-sim-cdc'

# Use type hints
def parse_frame(data: bytes) -> Optional[dict]:
    pass
```

---

## Testing Requirements

### Unit Tests (Required for New Code)

**Write tests for:**
- New algorithms (FFB calculations, filtering)
- Safety validations
- Data parsing (UART protocol, telemetry)
- Settings management

**Test framework:** GoogleTest (C++) or pytest (Python)

**Example:**

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
    EXPECT_FLOAT_EQ(result.suggestedValue, 0.50f);
}
```

**Run tests:**

```bash
cd firmware-tests
cmake -S . -B build && cmake --build build
./build/test_runner
```

### Integration Tests (Recommended)

**Test with simulator:**

```bash
# Start simulator
python3 tools/firmware-sim/sim_base.py /tmp/ffb-sim-cdc

# Run test script
python3 test-integration.py /tmp/ffb-sim-cdc
```

**Example integration test:**

```python
import serial
import time

def test_ffb_enable():
    port = serial.Serial('/tmp/ffb-sim-cdc', 115200, timeout=1)
    
    # Enable motors
    port.write(b'e\n')
    response = port.readline().decode()
    assert 'Motors enabled' in response
    
    # Verify telemetry shows enabled state
    port.write(b't\n')
    time.sleep(0.1)
    data = port.read(100).decode()
    assert 'motor_en=1' in data
    
    print("✓ FFB enable test passed")

test_ffb_enable()
```

### Manual Testing Checklist

**Before submitting PR:**

- [ ] Code compiles without warnings
- [ ] All unit tests pass
- [ ] Tested on actual hardware (if hardware change)
- [ ] Tested with simulator (for firmware changes)
- [ ] No regressions (existing features still work)
- [ ] Documentation updated

---

## Documentation

### Code Documentation

**Document:**
- Public APIs (functions, classes)
- Non-obvious algorithms
- Hardware assumptions (pin assignments, timing)
- Safety-critical code
- Configuration parameters

**Example:**

```cpp
/**
 * Apply torque to motors with safety limits
 * 
 * @param torque Desired torque [-1.0, +1.0]
 *               Positive = turn right
 * @param dutyCap Maximum PWM duty cycle [0.0, 1.0]
 * 
 * @note Torque is clamped to [-dutyCap, +dutyCap]
 *       Motors must be enabled first via enable()
 */
void MotorBts7960::applyTorque(float torque, float dutyCap);
```

### User Documentation

**Update when adding features:**
- README.md: High-level overview
- docs/build-guide.md: Hardware assembly
- docs/firmware-development.md: Code structure
- docs/calibration-and-tuning.md: User procedures
- CHANGELOG.md: Version history

**Documentation style:**
- Clear, concise language
- Step-by-step instructions
- Examples and code snippets
- Screenshots/diagrams where helpful
- Troubleshooting tips

---

## Pull Request Process

### Before Submitting

**Checklist:**

- [ ] Code follows style guide (run `clang-format`, `eslint`)
- [ ] All tests pass (`pio test`, `npm test`)
- [ ] No compiler warnings
- [ ] Documentation updated (if needed)
- [ ] CHANGELOG.md updated (under "Unreleased")
- [ ] Commit messages are clear
- [ ] PR description filled out (see template)
- [ ] No merge conflicts with `main`

### PR Description Template

**Use the template (`.github/pull_request_template.md`):**

```markdown
## Description
Brief summary of changes (what and why)

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
How did you test this?
- [ ] Unit tests added/updated
- [ ] Tested on hardware
- [ ] Tested with simulator

## Checklist
- [ ] Code follows style guide
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] No new warnings

## Safety Considerations
Any safety implications? (motor control, power, etc.)
```

### Review Process

1. **Automated checks** run (CI/CD)
   - Build test
   - Lint checks
   - Unit tests
   - Format validation

2. **Code review** by maintainer(s)
   - Functionality correctness
   - Code quality
   - Test coverage
   - Documentation

3. **Feedback addressed**
   - Make changes in new commits
   - Push to same branch
   - CI re-runs automatically

4. **Approval and merge**
   - Maintainer approves
   - Squash and merge (or rebase)
   - Branch deleted

### After Merge

- Delete your feature branch (locally and remote)
- Pull latest `main`
- Start next contribution!

```bash
git checkout main
git pull upstream main
git branch -d feature/my-feature
git push origin --delete feature/my-feature
```

---

## Issue Guidelines

### Issue Types

**Bug Report:**
- Use "Bug Report" template
- Include reproduction steps
- Provide logs and firmware version

**Feature Request:**
- Use "Feature Request" template
- Explain use case and benefits
- Suggest implementation approach

**Question / Discussion:**
- Use GitHub Discussions (not Issues)
- Search existing discussions first
- Provide context for your question

### Labels

Maintainers will add labels:

| Label | Meaning |
|-------|---------|
| `bug` | Something isn't working |
| `enhancement` | New feature or request |
| `documentation` | Improvements to docs |
| `good first issue` | Good for newcomers |
| `help wanted` | Extra attention needed |
| `wontfix` | Not planned to be fixed |
| `duplicate` | Already reported |
| `hardware` | Hardware-related issue |
| `firmware` | Firmware code issue |
| `gui` | GUI configurator issue |

### Closing Issues

**Issues are closed when:**
- Bug is fixed and merged
- Feature is implemented
- Question is answered
- Duplicate of existing issue
- Not reproducible / stale

**Reopen if:**
- Bug reappears in new version
- Solution doesn't work

---

## Community

### Communication Channels

**GitHub:**
- Issues: Bug reports, feature requests
- Discussions: Questions, build logs, general chat
- Pull Requests: Code contributions

**Unofficial:**
- Reddit: r/simracing
- Discord: (if community server exists)

### Getting Help

**Stuck? Ask for help!**

- Open a Discussion (not Issue) for questions
- Provide context and what you've tried
- Be patient - maintainers are volunteers

**Good question:**
```markdown
I'm trying to add support for a new sensor (BMI160 accelerometer).
I've wired it to the I²C bus and can see it at address 0x68.

My code compiles but the sensor returns all zeros. I suspect
timing or register initialization is wrong.

Here's my driver code: [link to gist]

Any suggestions on what to check?
```

### Maintainer Response Time

**Expectations:**
- Issues/PRs reviewed within 1-2 weeks
- Simple fixes merged quickly
- Complex features may take longer
- Maintainers are volunteers (patience appreciated!)

---

## Recognition

**Contributors are valued!**

- All contributors listed in README.md
- Significant contributions acknowledged in CHANGELOG.md
- Community showcases (build of the month, etc.)

**Thank you for contributing to rp2040-ffb!** 🎉

---

*Last updated: 2024-09-12*
