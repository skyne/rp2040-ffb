#pragma once

/**
 * @file safety.h
 * @brief Safety validation and runtime protection for rp2040-ffb
 *
 * This module provides:
 *  1. Configuration validation (reject dangerous settings)
 *  2. Motor thermal protection (prevent overheating)
 *  3. Communication watchdogs (detect disconnections)
 *  4. Runtime safety monitoring
 *
 * All safety-critical limits are defined as constants in the Limits struct.
 * Watchdogs must be updated in the main loop for proper protection.
 */

#include <stdint.h>

namespace Safety {

/**
 * @brief Validation result severity levels
 */
enum class ValidationResult {
    Ok,      ///< Value is safe and within normal operating range
    Warning, ///< Value is acceptable but outside recommended range
    Error    ///< Value is rejected (unsafe or out of hardware limits)
};

/**
 * @brief Result of a configuration validation check
 */
struct ValidationError {
    ValidationResult level; ///< Severity (Ok, Warning, or Error)
    const char* message;    ///< Human-readable explanation
    float suggestedValue;   ///< Recommended safe alternative value
};

/**
 * @brief Safety limits and constants
 *
 * These define the acceptable ranges for all user-configurable parameters.
 * Values outside these ranges will be rejected or warned about.
 */
struct Limits {
    // Motor duty cycle limits (PWM power output)
    static constexpr float kDutyCapMin = 0.0f; ///< Minimum duty cycle (0%)
    static constexpr float kDutyCapMax = 1.0f; ///< Maximum duty cycle (100%)
    static constexpr float kDutyCapSafeMax =
#if defined(MOTOR_DRIVER_MC33926)
        0.98f; ///< Bench Pololu — warn only near flat-out
#else
        0.50f; ///< Safe maximum at 24V on BTS7960 (50%, warns above this)
#endif

    // Torque limits
    static constexpr float kTorqueCapMin = 0.0f; ///< Minimum torque cap
    static constexpr float kTorqueCapMax = 1.0f; ///< Maximum torque cap

    // USB PID user gain
    static constexpr float kFfbGainMin = 0.0f;
    static constexpr float kFfbGainMax = 1.0f;

    // FFB spring parameters
    static constexpr float kSpringKMin = 0.0f; ///< Minimum spring stiffness (off)
    static constexpr float kSpringKMax = 0.1f; ///< Maximum spring stiffness (extremely stiff)

    // Mechanical configuration
    static constexpr float kGearRatioMin = 5.0f;  ///< Minimum gear ratio (direct drive-ish)
    static constexpr float kGearRatioMax = 50.0f; ///< Maximum gear ratio (very high reduction)

    // HID wheel configuration
    static constexpr float kHidRangeMin = 90.0f;   ///< Minimum wheel range (90° total)
    static constexpr float kHidRangeMax = 2700.0f; ///< Maximum wheel range (2700° total)

    // Thermal protection timing (0 = disabled — bench builds may override)
#ifndef MOTOR_WATCHDOG_MAX_ON_MS
#define MOTOR_WATCHDOG_MAX_ON_MS 60000u
#endif
#ifndef MOTOR_WATCHDOG_COOLDOWN_MS
#define MOTOR_WATCHDOG_COOLDOWN_MS 10000u
#endif
    static constexpr uint32_t kMaxMotorOnTimeMs = MOTOR_WATCHDOG_MAX_ON_MS;
    static constexpr uint32_t kMotorCooldownMs = MOTOR_WATCHDOG_COOLDOWN_MS;

    // Communication timeouts (status only — do not kill local FFB on USB idle)
#ifndef USB_WATCHDOG_TIMEOUT_MS
#define USB_WATCHDOG_TIMEOUT_MS 30000u
#endif
    static constexpr uint32_t kUsbTimeoutMs =
        USB_WATCHDOG_TIMEOUT_MS;                    ///< USB idle → mark disconnected
    static constexpr uint32_t kRimTimeoutMs = 2000; ///< Rim link timeout (2 seconds)
};

/**
 * @brief Validate duty cycle cap before applying
 * @param value Proposed duty cap [0.0, 1.0]
 * @return Validation result with severity and message
 *
 * Returns Error if value is outside [kDutyCapMin, kDutyCapMax].
 * Returns Warning if value exceeds kDutyCapSafeMax (thermal concerns at 24V).
 * Returns Ok otherwise.
 */
ValidationError validateDutyCap(float value);

/**
 * @brief Validate torque cap before applying
 * @param value Proposed torque cap [0.0, 1.0]
 * @return Validation result
 */
ValidationError validateTorqueCap(float value);

/**
 * @brief Validate USB PID user gain
 * @param value Proposed gain [0.0, 1.0]
 * @return Validation result
 */
ValidationError validateFfbGain(float value);

/**
 * @brief Validate spring stiffness coefficient
 * @param value Proposed spring K (Nm/deg)
 * @return Validation result
 */
ValidationError validateSpringK(float value);

/**
 * @brief Validate gear ratio configuration
 * @param value Proposed gear ratio (motor_turns / wheel_turns)
 * @return Validation result
 */
ValidationError validateGearRatio(float value);

/**
 * @brief Validate HID wheel rotation range
 * @param value Proposed rotation range in degrees (total, not half-travel)
 * @return Validation result
 */
ValidationError validateHidRange(float value);

/**
 * @brief Motor thermal protection watchdog
 *
 * Monitors cumulative motor on-time to prevent overheating.
 * Enforces mandatory cooldown periods after extended use.
 *
 * Usage:
 *  1. Call init() at startup
 *  2. Call update() in main loop
 *  3. Call notifyMotorEnabled()/notifyMotorDisabled() when motor state changes
 *  4. Check isMotorSafe() before enabling motors
 *
 * Safety: If motors exceed kMaxMotorOnTimeMs continuous operation,
 * the watchdog will require a cooldown period (kMotorCooldownMs).
 */
class MotorWatchdog {
  public:
    /**
     * @brief Initialize motor watchdog
     * Call once at startup
     */
    void init();

    /**
     * @brief Update watchdog timers
     * Call in main loop (every few ms)
     */
    void update();

    /**
     * @brief Notify watchdog that motors have been enabled
     * Call when transitioning from off to on
     */
    void notifyMotorEnabled();

    /**
     * @brief Notify watchdog that motors have been disabled
     * Call when transitioning from on to off
     */
    void notifyMotorDisabled();

    /**
     * @brief Check if motors are safe to enable
     * @return False if driver fault, cooldown required, or thermal budget exceeded
     */
    bool isMotorSafe() const;

    /**
     * @brief Check if the motor driver reports a hardware fault (MC33926 nSF).
     * @return True when faultActive() on the active MotorDriver backend
     */
    bool driverFaultActive() const;

    /**
     * @brief Attempt to clear a latched MC33926 fault (D2 toggle). No-op on BTS7960.
     * @return True if fault line is inactive after the clear sequence
     */
    bool clearDriverFault();

    /**
     * @brief Get remaining thermal budget time
     * @return Milliseconds remaining before forced cooldown (0 if in cooldown)
     */
    uint32_t getThermalBudgetMs() const;

    /**
     * @brief Check if currently in mandatory cooldown period
     * @return True if cooldown required
     */
    bool needsCooldown() const;

  private:
    uint32_t motorStartMs_ = 0;    ///< Timestamp when motors were last enabled
    uint32_t lastCooldownMs_ = 0;  ///< Timestamp of last cooldown completion
    bool motorRunning_ = false;    ///< Current motor state
    uint32_t totalOnTimeMs_ = 0;   ///< Cumulative on-time since last cooldown
    bool faultStopIssued_ = false; ///< Avoid spamming fault logs every loop tick
};

/**
 * @brief Communication link watchdog
 *
 * Monitors USB and rim UART links for activity.
 * Detects disconnections and stale communication.
 *
 * Usage:
 *  1. Call init() at startup
 *  2. Call update() in main loop
 *  3. Call notifyUsbActivity()/notifyRimActivity() on each message received
 *  4. Check isUsbAlive()/isRimAlive() to detect disconnections
 *
 * Safety: USB loss only updates isUsbAlive() — motors stay armed for local
 * Spring/Manual/Track/INIT. Rim loss is a warning; base keeps running.
 */
class CommunicationWatchdog {
  public:
    /**
     * @brief Initialize communication watchdog
     * Call once at startup
     */
    void init();

    /**
     * @brief Update watchdog timers
     * Call in main loop
     */
    void update();

    /**
     * @brief Notify watchdog of USB activity
     * Call whenever any USB packet is received
     */
    void notifyUsbActivity();

    /**
     * @brief Notify watchdog of rim UART activity
     * Call whenever any rim message is received
     */
    void notifyRimActivity();

    /**
     * @brief Check if USB link is alive
     * @return True if recent activity, false if timeout
     */
    bool isUsbAlive() const;

    /**
     * @brief Check if rim UART link is alive
     * @return True if recent activity, false if timeout
     */
    bool isRimAlive() const;

    /**
     * @brief Get USB idle time
     * @return Milliseconds since last USB activity
     */
    uint32_t getUsbIdleMs() const;

    /**
     * @brief Get rim idle time
     * @return Milliseconds since last rim activity
     */
    uint32_t getRimIdleMs() const;

  private:
    uint32_t lastUsbMs_ = 0; ///< Timestamp of last USB activity
    uint32_t lastRimMs_ = 0; ///< Timestamp of last rim activity
    bool usbAlive_ = false;  ///< USB link status
    bool rimAlive_ = false;  ///< Rim link status
};

// Global watchdog instances
extern MotorWatchdog gMotorWatchdog;        ///< Global motor thermal watchdog
extern CommunicationWatchdog gCommWatchdog; ///< Global communication watchdog

} // namespace Safety
