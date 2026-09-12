#pragma once

#include <stdint.h>

namespace Safety {

enum class ValidationResult { Ok, Warning, Error };

struct ValidationError {
    ValidationResult level;
    const char* message;
    float suggestedValue;
};

// Configuration limits
struct Limits {
    static constexpr float kDutyCapMin = 0.0f;
    static constexpr float kDutyCapMax = 1.0f;
    static constexpr float kDutyCapSafeMax = 0.50f; // Warn above this

    static constexpr float kTorqueCapMin = 0.0f;
    static constexpr float kTorqueCapMax = 1.0f;

    static constexpr float kSpringKMin = 0.0f;
    static constexpr float kSpringKMax = 0.1f; // Extremely stiff

    static constexpr float kGearRatioMin = 5.0f;
    static constexpr float kGearRatioMax = 50.0f;

    static constexpr float kHidRangeMin = 90.0f;
    static constexpr float kHidRangeMax = 2700.0f;

    static constexpr uint32_t kMaxMotorOnTimeMs = 60000;  // 1 min continuous
    static constexpr uint32_t kMotorCooldownMs = 10000;   // 10s between long runs
    static constexpr uint32_t kUsbTimeoutMs = 5000;       // USB activity timeout
    static constexpr uint32_t kRimTimeoutMs = 2000;       // Rim link timeout
};

// Validate a setting change before applying
ValidationError validateDutyCap(float value);
ValidationError validateTorqueCap(float value);
ValidationError validateSpringK(float value);
ValidationError validateGearRatio(float value);
ValidationError validateHidRange(float value);

// Runtime safety monitors
class MotorWatchdog {
public:
    void init();
    void update(); // Call in main loop

    // Track motor usage
    void notifyMotorEnabled();
    void notifyMotorDisabled();

    bool isMotorSafe() const;
    uint32_t getThermalBudgetMs() const; // Time remaining before forced cooldown
    bool needsCooldown() const;

private:
    uint32_t motorStartMs_ = 0;
    uint32_t lastCooldownMs_ = 0;
    bool motorRunning_ = false;
    uint32_t totalOnTimeMs_ = 0;
};

class CommunicationWatchdog {
public:
    void init();
    void update();

    void notifyUsbActivity();
    void notifyRimActivity();

    bool isUsbAlive() const;
    bool isRimAlive() const;
    uint32_t getUsbIdleMs() const;
    uint32_t getRimIdleMs() const;

private:
    uint32_t lastUsbMs_ = 0;
    uint32_t lastRimMs_ = 0;
    bool usbAlive_ = false;
    bool rimAlive_ = false;
};

// Global watchdog instances
extern MotorWatchdog gMotorWatchdog;
extern CommunicationWatchdog gCommWatchdog;

} // namespace Safety
