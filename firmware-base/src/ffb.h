#pragma once

/**
 * @file ffb.h
 * @brief Force Feedback controller for rp2040-ffb steering wheel
 * 
 * This module computes the desired motor torque based on the current FFB mode,
 * wheel angle, and user settings. It does NOT drive the motors directly - it
 * only calculates the target torque value. Motor control is handled by motor_bts7960.
 * 
 * Supported modes:
 *  - Off: No force feedback (freewheel)
 *  - Manual: Apply constant test torque (for debugging/calibration)
 *  - Spring: Centering spring force (proportional to angle from center)
 * 
 * Future: Full USB PID effects from games
 */

namespace Ffb {

/**
 * @brief Force feedback operating modes
 */
enum class Mode {
    Off,    ///< Commanded torque = 0 (freewheel)
    Manual, ///< Constant test torque from serial command (for bench testing)
    Spring, ///< Recenter spring toward wheel zero (proportional control)
};

/**
 * @brief Initialize FFB module
 * 
 * Call once at startup. Loads settings from EEPROM and sets default mode (Off).
 */
void begin();

/**
 * @brief Set FFB operating mode
 * @param m Desired mode (Off, Manual, or Spring)
 */
void setMode(Mode m);

/**
 * @brief Get current FFB mode
 * @return Current mode
 */
Mode mode();

/**
 * @brief Set manual test torque (Manual mode only)
 * @param t Torque command in range [-1.0, +1.0]
 *          -1.0 = full left, 0 = off, +1.0 = full right
 */
void setManualTorque(float t);

/**
 * @brief Get current manual torque setting
 * @return Manual torque [-1.0, +1.0]
 */
float manualTorque();

/**
 * @brief Set spring stiffness coefficient (Spring mode only)
 * @param k Spring constant in Nm/deg (typically 0.01 - 0.10)
 *          Higher = stronger centering force
 */
void setSpringK(float k);

/**
 * @brief Get spring stiffness
 * @return Spring constant (Nm/deg)
 */
float springK();

/**
 * @brief Set spring deadzone (Spring mode only)
 * @param deg Deadzone width in degrees (typically 1-5°)
 *            No force applied within ±deg of center
 */
void setSpringDeadzone(float deg);

/**
 * @brief Get spring deadzone
 * @return Deadzone width (degrees)
 */
float springDeadzone();

/**
 * @brief Set maximum torque output cap (all modes)
 * @param t Maximum torque in range [0.0, 1.0]
 *          0.5 = 50% of max motor torque (safe default at 24V)
 * 
 * Safety: Never exceed 0.7 without active cooling!
 */
void setTorqueCap(float t);

/**
 * @brief Get torque cap setting
 * @return Max torque [0.0, 1.0]
 */
float torqueCap();

/**
 * @brief Enable/disable software rotation limits
 * @param on True to enable soft stops, false to disable
 * 
 * When enabled, applies increasing resistance force as wheel approaches
 * software-defined rotation limits (prevents over-rotation past ±limitDeg)
 */
void setSoftLimitEnabled(bool on);

/**
 * @brief Check if soft limits are enabled
 * @return True if enabled
 */
bool softLimitEnabled();

/**
 * @brief Set software rotation limit (half-travel)
 * @param deg Half-travel limit in degrees (e.g., 450° for 900° total range)
 *            0 = use HID wheel range/2 from Settings
 */
void setSoftLimitDeg(float deg);

/**
 * @brief Get soft limit angle
 * @return Half-travel limit (degrees)
 */
float softLimitDeg();

/**
 * @brief Set soft limit spring stiffness
 * @param k Spring constant for soft stops (Nm/deg)
 *          Higher = harder stops at rotation limits
 */
void setSoftLimitK(float k);

/**
 * @brief Get soft limit stiffness
 * @return Soft limit spring constant (Nm/deg)
 */
float softLimitK();

/**
 * @brief Compute FFB torque for current control tick
 * @param axleDegrees Current wheel angle in degrees relative to zero
 *                    (after calibration and gear ratio correction)
 * 
 * Call this every control tick (typically 500-1000 Hz).
 * Internally computes the desired torque based on current mode and settings.
 */
void update(float axleDegrees);

/**
 * @brief Get the computed torque command
 * @return Commanded torque in range [-1.0, +1.0]
 *         -1.0 = full left torque, +1.0 = full right torque
 * 
 * This value should be passed to the motor controller (motor_bts7960)
 */
float commandedTorque();

} // namespace Ffb
