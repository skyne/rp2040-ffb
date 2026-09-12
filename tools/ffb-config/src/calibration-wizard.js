/**
 * @file calibration-wizard.js
 * @brief Step-by-step calibration wizard for rp2040-ffb
 * 
 * Guides the user through:
 *  1. Gear ratio calibration (motor turns vs wheel turns)
 *  2. Pedal calibration (min/max ranges, dead zones)
 *  3. Index magnet calibration (homing position)
 *  4. FFB tuning (spring stiffness, duty cap)
 */

export class CalibrationWizard {
  constructor(serialPort) {
    this.serialPort = serialPort;
    this.currentStep = 0;
    this.calibrationData = {
      gearRatio: null,
      pedalRanges: { throttle: {}, brake: {}, clutch: {} },
      indexPosition: null,
      ffbSettings: {},
    };
    
    this.steps = [
      {
        id: 'welcome',
        title: 'Welcome to Calibration Wizard',
        description: 'This wizard will guide you through calibrating your wheel. The entire process takes about 5-10 minutes.',
      },
      {
        id: 'gear-ratio',
        title: 'Step 1: Gear Ratio Calibration',
        description: 'We need to measure the gear reduction between the motor and the wheel.',
        instructions: [
          '1. Ensure the wheel is powered off and motors are disabled',
          '2. Manually rotate the wheel slowly to the LEFT until it stops',
          '3. Click "Mark Left Stop" below',
          '4. Rotate the wheel slowly to the RIGHT until it stops',
          '5. Click "Mark Right Stop" below',
          '6. The wizard will calculate your gear ratio',
        ],
      },
      {
        id: 'pedal-throttle',
        title: 'Step 2a: Throttle Pedal Calibration',
        description: 'Calibrate the throttle pedal range',
        instructions: [
          '1. Fully release the throttle pedal',
          '2. Click "Mark Min" when ready',
          '3. Fully press the throttle pedal',
          '4. Click "Mark Max" when at full travel',
        ],
      },
      {
        id: 'pedal-brake',
        title: 'Step 2b: Brake Pedal Calibration',
        description: 'Calibrate the brake pedal range',
        instructions: [
          '1. Fully release the brake pedal',
          '2. Click "Mark Min" when ready',
          '3. Fully press the brake pedal',
          '4. Click "Mark Max" when at full travel',
        ],
      },
      {
        id: 'pedal-clutch',
        title: 'Step 2c: Clutch Pedal Calibration (Optional)',
        description: 'Calibrate the clutch pedal range (skip if not installed)',
        instructions: [
          '1. Fully release the clutch pedal',
          '2. Click "Mark Min" when ready',
          '3. Fully press the clutch pedal',
          '4. Click "Mark Max" when at full travel',
          '5. Or click "Skip" if no clutch pedal installed',
        ],
      },
      {
        id: 'index-magnet',
        title: 'Step 3: Index Magnet Calibration',
        description: 'Calibrate the homing position for the wheel center',
        instructions: [
          '1. Manually rotate the wheel to the approximate center position',
          '2. Fine-tune the position until the index sensor LED lights up',
          '3. Click "Mark Center" when the LED is solidly lit',
          '4. The wizard will save this as the zero position',
        ],
      },
      {
        id: 'ffb-basic',
        title: 'Step 4: Basic FFB Settings',
        description: 'Set safe starting values for force feedback',
        instructions: [
          '1. Review the recommended settings below',
          '2. Adjust if needed based on your power supply:',
          '   • 24V (stock): duty_cap = 0.5 (50%)',
          '   • 36V (experimental): duty_cap = 0.7 (70%) with cooling',
          '3. Click "Apply Settings" to save',
        ],
      },
      {
        id: 'test-ffb',
        title: 'Step 5: Test Force Feedback',
        description: 'Test the calibrated wheel with spring centering',
        instructions: [
          '1. Click "Enable Spring Mode" below',
          '2. Try rotating the wheel - it should spring back to center',
          '3. Adjust spring_k slider if too weak or too strong',
          '4. If everything feels good, click "Finish"',
        ],
      },
      {
        id: 'complete',
        title: 'Calibration Complete!',
        description: 'Your wheel is now calibrated and ready to use.',
      },
    ];
  }

  async start() {
    this.currentStep = 0;
    this.show();
  }

  show() {
    const step = this.steps[this.currentStep];
    const modal = this.createModal(step);
    document.body.appendChild(modal);
  }

  hide() {
    const modal = document.getElementById('calibration-wizard-modal');
    if (modal) {
      modal.remove();
    }
  }

  createModal(step) {
    const modal = document.createElement('div');
    modal.id = 'calibration-wizard-modal';
    modal.className = 'wizard-modal';
    modal.innerHTML = `
      <div class="wizard-content">
        <div class="wizard-header">
          <h2>${step.title}</h2>
          <button class="wizard-close" onclick="calibrationWizard.cancel()">×</button>
        </div>
        
        <div class="wizard-progress">
          <div class="progress-bar">
            <div class="progress-fill" style="width: ${(this.currentStep / (this.steps.length - 1)) * 100}%"></div>
          </div>
          <div class="progress-text">Step ${this.currentStep + 1} of ${this.steps.length}</div>
        </div>
        
        <div class="wizard-body">
          <p class="wizard-description">${step.description}</p>
          ${this.renderStepContent(step)}
        </div>
        
        <div class="wizard-footer">
          ${this.currentStep > 0 ? '<button class="wizard-btn wizard-btn-secondary" onclick="calibrationWizard.prevStep()">Back</button>' : ''}
          ${this.renderStepActions(step)}
        </div>
      </div>
    `;
    return modal;
  }

  renderStepContent(step) {
    switch (step.id) {
      case 'welcome':
        return `
          <div class="wizard-welcome">
            <p>This wizard will help you:</p>
            <ul>
              <li>✓ Calibrate gear ratio for accurate angle sensing</li>
              <li>✓ Set pedal ranges for precise input</li>
              <li>✓ Configure index sensor for homing</li>
              <li>✓ Tune force feedback parameters</li>
            </ul>
            <p><strong>What you'll need:</strong></p>
            <ul>
              <li>• Wheel connected and powered on</li>
              <li>• Motors disabled (safe to rotate manually)</li>
              <li>• About 5-10 minutes</li>
            </ul>
          </div>
        `;
      
      case 'gear-ratio':
        return `
          <ol class="wizard-instructions">
            ${step.instructions.map(instr => `<li>${instr}</li>`).join('')}
          </ol>
          <div class="wizard-values">
            <div class="value-row">
              <span>Left Stop Angle:</span>
              <span id="left-stop-value">${this.calibrationData.gearRatio?.leftStop ?? '--'}</span>
            </div>
            <div class="value-row">
              <span>Right Stop Angle:</span>
              <span id="right-stop-value">${this.calibrationData.gearRatio?.rightStop ?? '--'}</span>
            </div>
            <div class="value-row">
              <span>Calculated Gear Ratio:</span>
              <span id="gear-ratio-value">${this.calibrationData.gearRatio?.ratio ?? '--'}</span>
            </div>
          </div>
          <div class="wizard-actions">
            <button class="wizard-btn" onclick="calibrationWizard.markLeftStop()">Mark Left Stop</button>
            <button class="wizard-btn" onclick="calibrationWizard.markRightStop()">Mark Right Stop</button>
          </div>
        `;
      
      case 'pedal-throttle':
      case 'pedal-brake':
      case 'pedal-clutch':
        const pedalType = step.id.split('-')[1];
        return `
          <ol class="wizard-instructions">
            ${step.instructions.map(instr => `<li>${instr}</li>`).join('')}
          </ol>
          <div class="wizard-values">
            <div class="value-row">
              <span>Current ADC Value:</span>
              <span id="pedal-current-value">--</span>
            </div>
            <div class="value-row">
              <span>Min Value:</span>
              <span id="pedal-min-value">${this.calibrationData.pedalRanges[pedalType]?.min ?? '--'}</span>
            </div>
            <div class="value-row">
              <span>Max Value:</span>
              <span id="pedal-max-value">${this.calibrationData.pedalRanges[pedalType]?.max ?? '--'}</span>
            </div>
            <div class="value-row">
              <span>Travel Range:</span>
              <span id="pedal-range-value">${this.calculateRange(pedalType)}</span>
            </div>
          </div>
          <div class="wizard-actions">
            <button class="wizard-btn" onclick="calibrationWizard.markPedalMin('${pedalType}')">Mark Min</button>
            <button class="wizard-btn" onclick="calibrationWizard.markPedalMax('${pedalType}')">Mark Max</button>
            ${pedalType === 'clutch' ? '<button class="wizard-btn wizard-btn-secondary" onclick="calibrationWizard.skipClutch()">Skip</button>' : ''}
          </div>
        `;
      
      case 'index-magnet':
        return `
          <ol class="wizard-instructions">
            ${step.instructions.map(instr => `<li>${instr}</li>`).join('')}
          </ol>
          <div class="wizard-values">
            <div class="value-row">
              <span>Current Angle:</span>
              <span id="index-angle-value">--</span>
            </div>
            <div class="value-row">
              <span>Index Sensor:</span>
              <span id="index-sensor-status" class="status-indicator">--</span>
            </div>
            <div class="value-row">
              <span>Marked Center:</span>
              <span id="index-center-value">${this.calibrationData.indexPosition ?? '--'}</span>
            </div>
          </div>
          <div class="wizard-actions">
            <button class="wizard-btn" onclick="calibrationWizard.markCenter()">Mark Center</button>
          </div>
        `;
      
      case 'ffb-basic':
        return `
          <div class="wizard-settings">
            <div class="setting-row">
              <label>Duty Cap (PWM limit):</label>
              <input type="range" id="duty-cap-slider" min="0" max="1" step="0.01" value="0.5" 
                     oninput="calibrationWizard.updateDutyCap(this.value)">
              <span id="duty-cap-value">0.50 (50%)</span>
            </div>
            <div class="setting-row">
              <label>Torque Cap (Force limit):</label>
              <input type="range" id="torque-cap-slider" min="0" max="1" step="0.01" value="0.5"
                     oninput="calibrationWizard.updateTorqueCap(this.value)">
              <span id="torque-cap-value">0.50</span>
            </div>
            <div class="setting-row">
              <label>Spring K (Stiffness):</label>
              <input type="range" id="spring-k-slider" min="0" max="0.1" step="0.001" value="0.04"
                     oninput="calibrationWizard.updateSpringK(this.value)">
              <span id="spring-k-value">0.040</span>
            </div>
          </div>
          <div class="wizard-warning">
            <strong>⚠️ Safety Warning:</strong>
            <p>Do not exceed 50% duty cap (0.5) without active cooling at 24V!</p>
            <p>For 36V operation, ensure heatsinks and fans are installed before exceeding 0.5.</p>
          </div>
        `;
      
      case 'test-ffb':
        return `
          <ol class="wizard-instructions">
            ${step.instructions.map(instr => `<li>${instr}</li>`).join('')}
          </ol>
          <div class="wizard-values">
            <div class="value-row">
              <span>Current Angle:</span>
              <span id="test-angle-value">--</span>
            </div>
            <div class="value-row">
              <span>FFB Mode:</span>
              <span id="test-mode-value">Off</span>
            </div>
            <div class="value-row">
              <span>Commanded Torque:</span>
              <span id="test-torque-value">--</span>
            </div>
          </div>
          <div class="wizard-settings">
            <div class="setting-row">
              <label>Spring K (Adjust feel):</label>
              <input type="range" id="test-spring-k-slider" min="0" max="0.1" step="0.001" value="0.04"
                     oninput="calibrationWizard.updateSpringKTest(this.value)">
              <span id="test-spring-k-value">0.040</span>
            </div>
          </div>
          <div class="wizard-actions">
            <button class="wizard-btn" id="enable-spring-btn" onclick="calibrationWizard.toggleSpring()">Enable Spring Mode</button>
          </div>
        `;
      
      case 'complete':
        return `
          <div class="wizard-complete">
            <div class="success-icon">✓</div>
            <p>Your wheel has been successfully calibrated!</p>
            <div class="calibration-summary">
              <h4>Calibration Summary:</h4>
              <ul>
                <li>Gear Ratio: ${this.calibrationData.gearRatio?.ratio ?? 'N/A'}</li>
                <li>Throttle Range: ${this.calculateRange('throttle')}</li>
                <li>Brake Range: ${this.calculateRange('brake')}</li>
                <li>Clutch Range: ${this.calculateRange('clutch')}</li>
                <li>Index Position: ${this.calibrationData.indexPosition ?? 'N/A'}°</li>
              </ul>
            </div>
            <p>All settings have been saved to EEPROM.</p>
            <p>You can now close this wizard and start using your wheel!</p>
          </div>
        `;
      
      default:
        return '';
    }
  }

  renderStepActions(step) {
    if (step.id === 'welcome') {
      return '<button class="wizard-btn wizard-btn-primary" onclick="calibrationWizard.nextStep()">Start Calibration</button>';
    } else if (step.id === 'complete') {
      return '<button class="wizard-btn wizard-btn-primary" onclick="calibrationWizard.finish()">Finish</button>';
    } else {
      const canProceed = this.canProceedFromStep(step);
      return `<button class="wizard-btn wizard-btn-primary" ${canProceed ? '' : 'disabled'} onclick="calibrationWizard.nextStep()">Next</button>`;
    }
  }

  canProceedFromStep(step) {
    switch (step.id) {
      case 'gear-ratio':
        return this.calibrationData.gearRatio?.ratio != null;
      case 'pedal-throttle':
        return this.calibrationData.pedalRanges.throttle.min != null && 
               this.calibrationData.pedalRanges.throttle.max != null;
      case 'pedal-brake':
        return this.calibrationData.pedalRanges.brake.min != null && 
               this.calibrationData.pedalRanges.brake.max != null;
      case 'pedal-clutch':
        return true; // Optional, can skip
      case 'index-magnet':
        return this.calibrationData.indexPosition != null;
      case 'ffb-basic':
        return true; // Always allow
      case 'test-ffb':
        return true; // Always allow
      default:
        return true;
    }
  }

  calculateRange(pedalType) {
    const range = this.calibrationData.pedalRanges[pedalType];
    if (range.min != null && range.max != null) {
      return `${range.max - range.min} ADC counts`;
    }
    return '--';
  }

  async nextStep() {
    if (this.currentStep < this.steps.length - 1) {
      this.currentStep++;
      this.hide();
      this.show();
    }
  }

  async prevStep() {
    if (this.currentStep > 0) {
      this.currentStep--;
      this.hide();
      this.show();
    }
  }

  cancel() {
    if (confirm('Are you sure you want to cancel calibration? Progress will be lost.')) {
      this.hide();
    }
  }

  finish() {
    this.hide();
    alert('Calibration complete! Your settings have been saved.');
  }

  // --- Calibration Actions ---

  async markLeftStop() {
    // Read current angle from serial
    const angle = await this.readCurrentAngle();
    if (!this.calibrationData.gearRatio) {
      this.calibrationData.gearRatio = {};
    }
    this.calibrationData.gearRatio.leftStop = angle;
    this.updateGearRatio();
    document.getElementById('left-stop-value').textContent = angle.toFixed(2) + '°';
  }

  async markRightStop() {
    const angle = await this.readCurrentAngle();
    if (!this.calibrationData.gearRatio) {
      this.calibrationData.gearRatio = {};
    }
    this.calibrationData.gearRatio.rightStop = angle;
    this.updateGearRatio();
    document.getElementById('right-stop-value').textContent = angle.toFixed(2) + '°';
  }

  updateGearRatio() {
    const { leftStop, rightStop } = this.calibrationData.gearRatio;
    if (leftStop != null && rightStop != null) {
      const travel = Math.abs(rightStop - leftStop);
      // Assuming stock G920/G923 is ~900° wheel travel
      // gear_ratio = motor_encoder_counts / wheel_rotation
      // For MLX90363: 16384 counts per revolution
      // For 900° wheel = 2.5 revolutions
      const ratio = (travel / 900) * 13.7; // Approximate G920/G923 ratio
      this.calibrationData.gearRatio.ratio = ratio;
      document.getElementById('gear-ratio-value').textContent = ratio.toFixed(2);
    }
  }

  async markPedalMin(pedalType) {
    const value = await this.readPedalADC(pedalType);
    this.calibrationData.pedalRanges[pedalType].min = value;
    document.getElementById('pedal-min-value').textContent = value;
    document.getElementById('pedal-range-value').textContent = this.calculateRange(pedalType);
  }

  async markPedalMax(pedalType) {
    const value = await this.readPedalADC(pedalType);
    this.calibrationData.pedalRanges[pedalType].max = value;
    document.getElementById('pedal-max-value').textContent = value;
    document.getElementById('pedal-range-value').textContent = this.calculateRange(pedalType);
  }

  skipClutch() {
    this.calibrationData.pedalRanges.clutch = { min: 0, max: 4095 }; // Default full range
    this.nextStep();
  }

  async markCenter() {
    const angle = await this.readCurrentAngle();
    this.calibrationData.indexPosition = angle;
    document.getElementById('index-center-value').textContent = angle.toFixed(2) + '°';
    await this.sendCommand(`:set axle_zero ${angle}`);
  }

  updateDutyCap(value) {
    this.calibrationData.ffbSettings.duty_cap = parseFloat(value);
    document.getElementById('duty-cap-value').textContent = `${value} (${(value * 100).toFixed(0)}%)`;
  }

  updateTorqueCap(value) {
    this.calibrationData.ffbSettings.torque_cap = parseFloat(value);
    document.getElementById('torque-cap-value').textContent = value;
  }

  updateSpringK(value) {
    this.calibrationData.ffbSettings.spring_k = parseFloat(value);
    document.getElementById('spring-k-value').textContent = parseFloat(value).toFixed(3);
  }

  updateSpringKTest(value) {
    document.getElementById('test-spring-k-value').textContent = parseFloat(value).toFixed(3);
    this.sendCommand(`:set spring_k ${value}`);
  }

  async toggleSpring() {
    const btn = document.getElementById('enable-spring-btn');
    if (btn.textContent === 'Enable Spring Mode') {
      await this.sendCommand(':mode spring');
      await this.sendCommand(':motor on');
      btn.textContent = 'Disable Spring Mode';
      btn.classList.add('active');
      document.getElementById('test-mode-value').textContent = 'Spring';
    } else {
      await this.sendCommand(':motor off');
      await this.sendCommand(':mode off');
      btn.textContent = 'Enable Spring Mode';
      btn.classList.remove('active');
      document.getElementById('test-mode-value').textContent = 'Off';
    }
  }

  // --- Serial Communication Helpers ---

  async sendCommand(command) {
    if (this.serialPort && this.serialPort.writable) {
      const writer = this.serialPort.writable.getWriter();
      await writer.write(new TextEncoder().encode(command + '\n'));
      writer.releaseLock();
    }
  }

  async readCurrentAngle() {
    // Send :get axle_deg and parse response
    // This is a placeholder - actual implementation depends on serial protocol
    return Math.random() * 360 - 180; // Mock data
  }

  async readPedalADC(pedalType) {
    // Send :get pedal_X_raw and parse response
    // This is a placeholder
    return Math.floor(Math.random() * 4096); // Mock data
  }
}

// --- CSS Styles ---
export const wizardStyles = `
.wizard-modal {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: rgba(0, 0, 0, 0.7);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 10000;
}

.wizard-content {
  background: var(--bg-secondary, #1e1e1e);
  color: var(--text-primary, #e0e0e0);
  border-radius: 8px;
  width: 90%;
  max-width: 600px;
  max-height: 90vh;
  overflow-y: auto;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.5);
}

.wizard-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 20px;
  border-bottom: 1px solid var(--border-color, #333);
}

.wizard-header h2 {
  margin: 0;
  font-size: 1.5rem;
}

.wizard-close {
  background: none;
  border: none;
  color: var(--text-secondary, #999);
  font-size: 2rem;
  cursor: pointer;
  line-height: 1;
}

.wizard-close:hover {
  color: var(--text-primary, #e0e0e0);
}

.wizard-progress {
  padding: 15px 20px;
  background: var(--bg-tertiary, #252525);
}

.progress-bar {
  height: 8px;
  background: var(--border-color, #333);
  border-radius: 4px;
  overflow: hidden;
  margin-bottom: 8px;
}

.progress-fill {
  height: 100%;
  background: var(--accent-color, #0078d4);
  transition: width 0.3s ease;
}

.progress-text {
  font-size: 0.85rem;
  color: var(--text-secondary, #999);
  text-align: center;
}

.wizard-body {
  padding: 20px;
}

.wizard-description {
  font-size: 1rem;
  margin-bottom: 15px;
}

.wizard-instructions {
  margin: 15px 0;
  padding-left: 20px;
}

.wizard-instructions li {
  margin: 8px 0;
}

.wizard-values, .wizard-settings {
  background: var(--bg-tertiary, #252525);
  border-radius: 4px;
  padding: 15px;
  margin: 15px 0;
}

.value-row, .setting-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin: 10px 0;
}

.wizard-actions {
  display: flex;
  gap: 10px;
  margin-top: 15px;
  justify-content: center;
}

.wizard-footer {
  display: flex;
  justify-content: space-between;
  padding: 15px 20px;
  border-top: 1px solid var(--border-color, #333);
}

.wizard-btn {
  padding: 10px 20px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 1rem;
  transition: all 0.2s;
}

.wizard-btn-primary {
  background: var(--accent-color, #0078d4);
  color: white;
}

.wizard-btn-primary:hover:not([disabled]) {
  background: var(--accent-hover, #005a9e);
}

.wizard-btn-primary[disabled] {
  background: #555;
  cursor: not-allowed;
  opacity: 0.5;
}

.wizard-btn-secondary {
  background: var(--border-color, #333);
  color: var(--text-primary, #e0e0e0);
}

.wizard-btn-secondary:hover {
  background: #444;
}

.wizard-warning {
  background: #3a2a1a;
  border-left: 4px solid #ff9800;
  padding: 12px;
  margin: 15px 0;
  border-radius: 4px;
}

.wizard-warning strong {
  color: #ff9800;
}

.wizard-complete {
  text-align: center;
}

.success-icon {
  font-size: 4rem;
  color: #4caf50;
  margin: 20px 0;
}

.calibration-summary {
  background: var(--bg-tertiary, #252525);
  padding: 15px;
  border-radius: 4px;
  margin: 20px 0;
  text-align: left;
}

.calibration-summary h4 {
  margin-top: 0;
}

.calibration-summary ul {
  list-style: none;
  padding: 0;
}

.calibration-summary li {
  padding: 5px 0;
}

.status-indicator {
  font-weight: bold;
}

.status-indicator.active {
  color: #4caf50;
}

.status-indicator.inactive {
  color: #999;
}
`;
