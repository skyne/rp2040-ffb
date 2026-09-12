#include "safety.h"

#include <Arduino.h>

#include "ffb.h"
#include "motor_bts7960.h"

namespace Safety {

// Global instances
MotorWatchdog gMotorWatchdog;
CommunicationWatchdog gCommWatchdog;

// ============================================================================
// Validation functions
// ============================================================================

ValidationError validateDutyCap(float value) {
    if (value < Limits::kDutyCapMin || value > Limits::kDutyCapMax) {
        return {ValidationResult::Error, "duty_cap out of range [0.0, 1.0]",
                constrain(value, Limits::kDutyCapMin, Limits::kDutyCapMax)};
    }

    if (value > Limits::kDutyCapSafeMax) {
        return {ValidationResult::Warning, "duty_cap > 0.50 - USE CAUTION! High torque!", value};
    }

    return {ValidationResult::Ok, nullptr, value};
}

ValidationError validateTorqueCap(float value) {
    if (value < Limits::kTorqueCapMin || value > Limits::kTorqueCapMax) {
        return {ValidationResult::Error, "torque_cap out of range [0.0, 1.0]",
                constrain(value, Limits::kTorqueCapMin, Limits::kTorqueCapMax)};
    }

    return {ValidationResult::Ok, nullptr, value};
}

ValidationError validateSpringK(float value) {
    if (value < Limits::kSpringKMin || value > Limits::kSpringKMax) {
        return {ValidationResult::Error, "spring_k out of safe range [0.0, 0.1]",
                constrain(value, Limits::kSpringKMin, Limits::kSpringKMax)};
    }

    // Warn if spring is very aggressive
    if (value > 0.02f) {
        return {ValidationResult::Warning, "spring_k > 0.02 - Very stiff! Start low and test!",
                value};
    }

    return {ValidationResult::Ok, nullptr, value};
}

ValidationError validateGearRatio(float value) {
    if (value < Limits::kGearRatioMin || value > Limits::kGearRatioMax) {
        return {ValidationResult::Error, "gear_ratio out of physical range [5.0, 50.0]",
                constrain(value, Limits::kGearRatioMin, Limits::kGearRatioMax)};
    }

    return {ValidationResult::Ok, nullptr, value};
}

ValidationError validateHidRange(float value) {
    if (value < Limits::kHidRangeMin || value > Limits::kHidRangeMax) {
        return {ValidationResult::Error, "hid_range out of range [90.0, 2700.0]",
                constrain(value, Limits::kHidRangeMin, Limits::kHidRangeMax)};
    }

    return {ValidationResult::Ok, nullptr, value};
}

// ============================================================================
// MotorWatchdog
// ============================================================================

void MotorWatchdog::init() {
    motorStartMs_ = 0;
    lastCooldownMs_ = 0;
    motorRunning_ = false;
    totalOnTimeMs_ = 0;
}

void MotorWatchdog::notifyMotorEnabled() {
    if (!motorRunning_) {
        motorStartMs_ = millis();
        motorRunning_ = true;
        Serial.println("WATCHDOG: Motors enabled");
    }
}

void MotorWatchdog::notifyMotorDisabled() {
    if (motorRunning_) {
        uint32_t runtime = millis() - motorStartMs_;
        totalOnTimeMs_ += runtime;
        motorRunning_ = false;
        Serial.print("WATCHDOG: Motors disabled after ");
        Serial.print(runtime);
        Serial.println(" ms");
    }
}

void MotorWatchdog::update() {
    if (!motorRunning_)
        return;

    uint32_t now = millis();
    uint32_t runTime = now - motorStartMs_;

    // Force cooldown if running too long
    if (runTime > Limits::kMaxMotorOnTimeMs) {
        Serial.println("!!! MOTOR WATCHDOG: Thermal timeout - forcing cooldown!");
        MotorBts7960::stop();
        Ffb::setMode(Ffb::Mode::Off);
        motorRunning_ = false;
        lastCooldownMs_ = now;
    }
}

bool MotorWatchdog::isMotorSafe() const {
    if (!motorRunning_)
        return true;

    uint32_t now = millis();
    uint32_t runTime = now - motorStartMs_;
    return runTime < Limits::kMaxMotorOnTimeMs;
}

uint32_t MotorWatchdog::getThermalBudgetMs() const {
    if (!motorRunning_)
        return Limits::kMaxMotorOnTimeMs;

    uint32_t now = millis();
    uint32_t runTime = now - motorStartMs_;

    if (runTime >= Limits::kMaxMotorOnTimeMs)
        return 0;
    return Limits::kMaxMotorOnTimeMs - runTime;
}

bool MotorWatchdog::needsCooldown() const {
    if (lastCooldownMs_ == 0)
        return false;

    uint32_t now = millis();
    uint32_t timeSinceCooldown = now - lastCooldownMs_;
    return timeSinceCooldown < Limits::kMotorCooldownMs;
}

// ============================================================================
// CommunicationWatchdog
// ============================================================================

void CommunicationWatchdog::init() {
    lastUsbMs_ = millis();
    lastRimMs_ = millis();
    usbAlive_ = true;
    rimAlive_ = false;
}

void CommunicationWatchdog::notifyUsbActivity() {
    uint32_t now = millis();
    bool wasAlive = usbAlive_;
    usbAlive_ = true;
    lastUsbMs_ = now;

    if (!wasAlive) {
        Serial.println("WATCHDOG: USB connection restored");
    }
}

void CommunicationWatchdog::notifyRimActivity() {
    uint32_t now = millis();
    bool wasAlive = rimAlive_;
    rimAlive_ = true;
    lastRimMs_ = now;

    if (!wasAlive) {
        Serial.println("WATCHDOG: Rim link restored");
    }
}

void CommunicationWatchdog::update() {
    uint32_t now = millis();

    // Check USB timeout
    if (usbAlive_ && (now - lastUsbMs_ > Limits::kUsbTimeoutMs)) {
        usbAlive_ = false;
        Serial.println("!!! COMMS WATCHDOG: USB timeout - disabling motors!");

        // Safety disable motors on USB loss
        if (MotorBts7960::enabled()) {
            MotorBts7960::stop();
            Ffb::setMode(Ffb::Mode::Off);
        }
    }

    // Check rim timeout (warning only, base can function without rim)
    if (rimAlive_ && (now - lastRimMs_ > Limits::kRimTimeoutMs)) {
        rimAlive_ = false;
        Serial.println("WARNING: Rim link timeout");
    }
}

bool CommunicationWatchdog::isUsbAlive() const {
    return usbAlive_;
}

bool CommunicationWatchdog::isRimAlive() const {
    return rimAlive_;
}

uint32_t CommunicationWatchdog::getUsbIdleMs() const {
    uint32_t now = millis();
    return now - lastUsbMs_;
}

uint32_t CommunicationWatchdog::getRimIdleMs() const {
    uint32_t now = millis();
    return now - lastRimMs_;
}

} // namespace Safety
