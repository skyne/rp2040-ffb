#pragma once

/**
 * @file motor_bts7960.h
 * @brief BTS7960 Dual H-Bridge Motor Driver for rp2040-ffb
 * 
 * Controls two BTS7960 IBT-2 motor driver modules for dual-motor force feedback.
 * Each BTS7960 is a 43A half-bridge driver capable of handling the Logitech G920/G923
 * brushed DC motors (stock 24V, experimental 36V with cooling).
 * 
 * Hardware Interface:
 *  - Motor 1 (left): RPWM1, LPWM1, EN1
 *  - Motor 2 (right): RPWM2, LPWM2, EN2
 *  - PWM frequency: 20 kHz (ultrasonic, reduces motor whine)
 *  - Resolution: 8-bit (0-255 duty cycle steps)
 * 
 * Control Method:
 *  - Sign-magnitude PWM (not locked antiphase)
 *  - Positive torque: RPWM active, LPWM low
 *  - Negative torque: LPWM active, RPWM low
 *  - Zero torque: Both PWM low (fast decay, coast if EN high)
 *  - Disabled (EN low): High-impedance output (freewheel)
 * 
 * Safety Features:
 *  - Duty cap: Global maximum PWM duty (typically 50% at 24V)
 *  - Enable control: Master on/off for both motors
 *  - Coast mode: Zero torque without disabling drivers
 *  - Stop mode: Zero torque + disable drivers
 * 
 * Typical Usage:
 *  1. Call begin() once at startup
 *  2. setEnabled(true) to enable motor drivers
 *  3. setBoth(torque) to apply FFB torque [-1.0, +1.0]
 *  4. setEnabled(false) or stop() to disable on error/idle
 * 
 * Pin Assignments: See wiring-diagrams.md
 */

namespace MotorBts7960 {

/**
 * @brief Initialize motor driver hardware
 * 
 * Configures GPIO pins, PWM channels, and sets safe defaults:
 *  - EN pins: output low (disabled)
 *  - RPWM/LPWM: output low (zero torque)
 *  - PWM frequency: 20 kHz
 * 
 * Call once at startup before using other functions.
 */
void begin();

/**
 * @brief Enable or disable motor driver bridges
 * @param on True to enable (EN pins high), false to disable (EN pins low)
 * 
 * When disabled, motor outputs are high-impedance (freewheel).
 * PWM signals still update but have no effect until re-enabled.
 * 
 * Safety: Always disable when:
 *  - Not using wheel (reduce power consumption)
 *  - Detected fault condition (watchdog trip, sensor error)
 *  - Emergency stop commanded
 */
void setEnabled(bool on);

/**
 * @brief Check if motor drivers are enabled
 * @return True if enabled (EN high)
 */
bool enabled();

/**
 * @brief Set motor 1 (left) torque command
 * @param cmd Torque in range [-1.0, +1.0]
 *            -1.0 = full CCW, 0.0 = off, +1.0 = full CW
 * 
 * Internally clamped by duty cap setting.
 */
void setMotor1(float cmd);

/**
 * @brief Set motor 2 (right) torque command
 * @param cmd Torque in range [-1.0, +1.0]
 * 
 * Internally clamped by duty cap setting.
 */
void setMotor2(float cmd);

/**
 * @brief Set both motors to the same torque (typical for G920/G923)
 * @param cmd Torque in range [-1.0, +1.0]
 * 
 * The G920/G923 use two motors mechanically coupled via a belt drive.
 * Both motors should receive the same command for proper operation.
 * 
 * Equivalent to:
 *   setMotor1(cmd);
 *   setMotor2(cmd);
 */
void setBoth(float cmd);

/**
 * @brief Set zero torque (coast) without disabling drivers
 * 
 * PWM duty = 0, EN pins remain as-is.
 * Motors will coast freely (no holding torque).
 * 
 * Use when temporarily releasing force (e.g., paused game).
 */
void coast();

/**
 * @brief Emergency stop: zero torque AND disable drivers
 * 
 * PWM duty = 0, EN pins go low.
 * Motors enter high-impedance freewheel.
 * 
 * Use for:
 *  - Shutdown sequence
 *  - Safety fault conditions
 *  - Long-term idle (power saving)
 */
void stop();

/**
 * @brief Get last commanded torque for motor 1
 * @return Torque [-1.0, +1.0]
 */
float lastCmd1();

/**
 * @brief Get last commanded torque for motor 2
 * @return Torque [-1.0, +1.0]
 */
float lastCmd2();

/**
 * @brief Set maximum PWM duty cycle cap (safety limit)
 * @param cap Maximum duty in range [0.0, 1.0]
 *            0.5 = 50% max duty (safe at 24V without active cooling)
 *            0.7 = 70% max duty (36V experimental, requires heatsinks + fans)
 * 
 * This globally limits all motor commands to prevent overheating.
 * Commands are scaled: cmd * cap before output.
 * 
 * Safety: Never exceed 0.7 without verified thermal management!
 */
void setDutyCap(float cap);

/**
 * @brief Get current duty cap setting
 * @return Duty cap [0.0, 1.0]
 */
float dutyCap();

} // namespace MotorBts7960
