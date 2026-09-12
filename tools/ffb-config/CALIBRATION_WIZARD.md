# Calibration Wizard

## Overview

The Calibration Wizard is a step-by-step guided process for setting up your rp2040-ffb wheel from scratch. It walks the user through all critical calibration steps in a user-friendly interface.

## Features

### 1. Gear Ratio Calibration
- **Purpose:** Determine the mechanical gear reduction between motor and wheel
- **Process:**
  1. Manually rotate wheel to left stop
  2. Mark left position
  3. Rotate to right stop
  4. Mark right position
  5. Wizard calculates gear ratio automatically
- **Result:** Accurate angle sensing for all FFB calculations

### 2. Pedal Calibration
- **Purpose:** Set min/max ranges for pedal inputs
- **Pedals:** Throttle, Brake, Clutch (optional)
- **Process:**
  1. Release pedal fully (mark min)
  2. Press pedal fully (mark max)
  3. Wizard saves calibration values
- **Result:** Full 0-100% pedal travel mapped correctly

### 3. Index Magnet Calibration
- **Purpose:** Set wheel center position for homing
- **Process:**
  1. Rotate wheel to center
  2. Fine-tune until index LED lights up
  3. Mark center position
- **Result:** Wheel knows absolute zero position for soft limits

### 4. FFB Settings
- **Purpose:** Set safe starting values for force feedback
- **Settings:**
  - `duty_cap`: Maximum PWM duty (default 0.5 for 24V)
  - `torque_cap`: Force limiter (default 0.5)
  - `spring_k`: Centering spring stiffness (default 0.04)
- **Safety warnings:** Displayed for high duty values

### 5. FFB Testing
- **Purpose:** Verify calibration with live spring centering
- **Process:**
  1. Enable spring mode
  2. Test wheel centering
  3. Adjust spring stiffness if needed
  4. Confirm everything works
- **Result:** Confidence that wheel is ready to use

## Integration

### Add to GUI

To integrate the wizard into `main.js`:

```javascript
import { CalibrationWizard, wizardStyles } from './calibration-wizard.js';

// Add styles to page
const styleSheet = document.createElement('style');
styleSheet.textContent = wizardStyles;
document.head.appendChild(styleSheet);

// Create wizard instance
let calibrationWizard = null;

// Add button to main UI
const wizardBtn = document.createElement('button');
wizardBtn.textContent = 'Calibration Wizard';
wizardBtn.onclick = () => {
  if (serialPort) {
    calibrationWizard = new CalibrationWizard(serialPort);
    calibrationWizard.start();
  } else {
    alert('Please connect to the wheel first');
  }
};
document.getElementById('toolbar').appendChild(wizardBtn);
```

### Usage Flow

1. User connects wheel via USB
2. User clicks "Calibration Wizard" button
3. Wizard opens in modal overlay
4. User follows step-by-step instructions
5. Wizard sends serial commands to firmware
6. Settings are saved to EEPROM automatically
7. User tests FFB before finishing
8. Wizard closes, wheel is ready to use

## Serial Commands Used

The wizard sends these commands to the firmware:

```
:get axle_deg              # Read current wheel angle
:get pedal_X_raw           # Read pedal ADC (X = throttle, brake, clutch)
:set axle_zero <angle>     # Set wheel center position
:set gear_ratio <ratio>    # Save calculated gear ratio
:set duty_cap <value>      # Set motor duty cap
:set spring_k <value>      # Set spring stiffness
:mode spring               # Enable spring centering mode
:motor on                  # Enable motors
:motor off                 # Disable motors
:save                      # Save all settings to EEPROM
```

## UI Components

### Modal Structure
- **Header:** Title + close button
- **Progress bar:** Visual step indicator
- **Body:** Instructions + interactive controls
- **Footer:** Back/Next buttons

### Step Types
1. **Informational:** Welcome, completion
2. **Measurement:** Gear ratio, pedals, index
3. **Configuration:** FFB settings
4. **Testing:** Live spring test

### Visual Feedback
- Real-time value display
- Progress percentage
- Status indicators (LED on/off)
- Warning messages for safety

## Styling

The wizard uses CSS variables for theming:
- `--bg-secondary`: Modal background
- `--text-primary`: Primary text color
- `--accent-color`: Primary button color
- `--border-color`: Borders and dividers

Dark/light theme support is automatic via CSS variables.

## Future Enhancements

1. **Auto-detect features:**
   - Detect index sensor presence
   - Detect ADXL345 accelerometer
   - Auto-configure based on hardware

2. **Advanced calibration:**
   - Pedal dead zone tuning
   - Soft limit angle adjustment
   - FFB curve shaping

3. **Validation:**
   - Range checks on all inputs
   - Warnings for unusual values
   - Retry failed measurements

4. **Telemetry:**
   - Real-time angle graph
   - Pedal input visualization
   - Torque output graph

5. **Presets:**
   - Save/load calibration profiles
   - Import/export settings
   - Share community presets

## Testing

### Manual Testing Checklist
- [ ] Wizard opens correctly
- [ ] All steps are accessible
- [ ] Back button works
- [ ] Next button enables when ready
- [ ] Measurements read from serial
- [ ] Commands sent correctly
- [ ] Values saved to EEPROM
- [ ] Spring test mode works
- [ ] Wizard closes on finish
- [ ] Settings persist after restart

### Automated Testing
- Unit tests for calibration logic (calculation functions)
- Integration tests with mock serial port
- E2E tests with Playwright (full wizard flow)

## Known Limitations

1. **Serial communication:** Assumes synchronous request/response
2. **Error handling:** Basic error messages only
3. **Validation:** Limited range checks
4. **Hardware detection:** Manual user confirmation required
5. **Multi-language:** English only (i18n future enhancement)

## Developer Notes

### State Management
- `currentStep`: Current wizard step index
- `calibrationData`: Accumulated measurements and settings
- `serialPort`: Reference to open serial connection

### Event Handling
- All buttons use onclick with global function calls
- Serial reads are async (await)
- Modal DOM is recreated on each step transition

### Extensibility
- Add new steps to `this.steps` array
- Implement `renderStepContent()` case for custom UI
- Add validation logic to `canProceedFromStep()`

---

**Last updated:** 2026-09-12  
**Status:** Ready for integration and testing
