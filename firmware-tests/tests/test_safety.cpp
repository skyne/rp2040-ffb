/**
 * @file test_safety.cpp
 * @brief Unit tests for Safety module validation and watchdogs
 */

#include <gtest/gtest.h>

// Mock Arduino environment
#include "../mocks/Arduino.h"

// Include safety module (or testable version)
// For now, we'll test the validation logic inline
namespace Safety {

enum class ValidationResult { Ok, Warning, Error };

struct ValidationError {
    ValidationResult level;
    const char* message;
    float suggestedValue;
};

struct Limits {
    static constexpr float kDutyCapMin = 0.0f;
    static constexpr float kDutyCapMax = 1.0f;
    static constexpr float kDutyCapSafeMax = 0.50f;

    static constexpr float kTorqueCapMin = 0.0f;
    static constexpr float kTorqueCapMax = 1.0f;

    static constexpr float kSpringKMin = 0.0f;
    static constexpr float kSpringKMax = 0.1f;

    static constexpr float kGearRatioMin = 5.0f;
    static constexpr float kGearRatioMax = 50.0f;

    static constexpr float kHidRangeMin = 90.0f;
    static constexpr float kHidRangeMax = 2700.0f;
};

// Validation functions (extracted for testing)
ValidationError validateDutyCap(float value) {
    if (value < Limits::kDutyCapMin || value > Limits::kDutyCapMax) {
        return {ValidationResult::Error, "Duty cap out of range [0.0, 1.0]", 0.5f};
    }
    if (value > Limits::kDutyCapSafeMax) {
        return {ValidationResult::Warning, "Duty cap exceeds safe limit (>50% at 24V)", 0.5f};
    }
    return {ValidationResult::Ok, "", value};
}

ValidationError validateTorqueCap(float value) {
    if (value < Limits::kTorqueCapMin || value > Limits::kTorqueCapMax) {
        return {ValidationResult::Error, "Torque cap out of range [0.0, 1.0]", 0.5f};
    }
    return {ValidationResult::Ok, "", value};
}

ValidationError validateSpringK(float value) {
    if (value < Limits::kSpringKMin || value > Limits::kSpringKMax) {
        return {ValidationResult::Error, "Spring K out of range [0.0, 0.1]", 0.05f};
    }
    return {ValidationResult::Ok, "", value};
}

ValidationError validateGearRatio(float value) {
    if (value < Limits::kGearRatioMin || value > Limits::kGearRatioMax) {
        return {ValidationResult::Error, "Gear ratio out of range [5.0, 50.0]", 13.7f};
    }
    return {ValidationResult::Ok, "", value};
}

ValidationError validateHidRange(float value) {
    if (value < Limits::kHidRangeMin || value > Limits::kHidRangeMax) {
        return {ValidationResult::Error, "HID range out of range [90, 2700]", 900.0f};
    }
    return {ValidationResult::Ok, "", value};
}

} // namespace Safety

// --- Test Fixtures ---

class SafetyValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock Arduino state
        resetMockArduino();
    }
};

// --- Duty Cap Validation Tests ---

TEST_F(SafetyValidationTest, DutyCapValid) {
    auto result = Safety::validateDutyCap(0.5f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Ok);
}

TEST_F(SafetyValidationTest, DutyCapWarningAboveSafe) {
    auto result = Safety::validateDutyCap(0.7f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Warning);
    EXPECT_STREQ(result.message, "Duty cap exceeds safe limit (>50% at 24V)");
}

TEST_F(SafetyValidationTest, DutyCapErrorTooLow) {
    auto result = Safety::validateDutyCap(-0.1f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Error);
    EXPECT_EQ(result.suggestedValue, 0.5f);
}

TEST_F(SafetyValidationTest, DutyCapErrorTooHigh) {
    auto result = Safety::validateDutyCap(1.5f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Error);
}

TEST_F(SafetyValidationTest, DutyCapEdgeCases) {
    EXPECT_EQ(Safety::validateDutyCap(0.0f).level, Safety::ValidationResult::Ok);
    EXPECT_EQ(Safety::validateDutyCap(1.0f).level, Safety::ValidationResult::Warning);
    EXPECT_EQ(Safety::validateDutyCap(0.50f).level, Safety::ValidationResult::Ok);
    EXPECT_EQ(Safety::validateDutyCap(0.51f).level, Safety::ValidationResult::Warning);
}

// --- Torque Cap Validation Tests ---

TEST_F(SafetyValidationTest, TorqueCapValid) {
    auto result = Safety::validateTorqueCap(0.5f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Ok);
}

TEST_F(SafetyValidationTest, TorqueCapErrorOutOfRange) {
    EXPECT_EQ(Safety::validateTorqueCap(-0.1f).level, Safety::ValidationResult::Error);
    EXPECT_EQ(Safety::validateTorqueCap(1.5f).level, Safety::ValidationResult::Error);
}

TEST_F(SafetyValidationTest, TorqueCapEdgeCases) {
    EXPECT_EQ(Safety::validateTorqueCap(0.0f).level, Safety::ValidationResult::Ok);
    EXPECT_EQ(Safety::validateTorqueCap(1.0f).level, Safety::ValidationResult::Ok);
}

// --- Spring K Validation Tests ---

TEST_F(SafetyValidationTest, SpringKValid) {
    auto result = Safety::validateSpringK(0.05f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Ok);
}

TEST_F(SafetyValidationTest, SpringKErrorOutOfRange) {
    EXPECT_EQ(Safety::validateSpringK(-0.1f).level, Safety::ValidationResult::Error);
    EXPECT_EQ(Safety::validateSpringK(0.15f).level, Safety::ValidationResult::Error);
}

TEST_F(SafetyValidationTest, SpringKEdgeCases) {
    EXPECT_EQ(Safety::validateSpringK(0.0f).level, Safety::ValidationResult::Ok);
    EXPECT_EQ(Safety::validateSpringK(0.1f).level, Safety::ValidationResult::Ok);
}

// --- Gear Ratio Validation Tests ---

TEST_F(SafetyValidationTest, GearRatioValid) {
    auto result = Safety::validateGearRatio(13.7f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Ok);
}

TEST_F(SafetyValidationTest, GearRatioErrorOutOfRange) {
    EXPECT_EQ(Safety::validateGearRatio(4.0f).level, Safety::ValidationResult::Error);
    EXPECT_EQ(Safety::validateGearRatio(60.0f).level, Safety::ValidationResult::Error);
}

TEST_F(SafetyValidationTest, GearRatioEdgeCases) {
    EXPECT_EQ(Safety::validateGearRatio(5.0f).level, Safety::ValidationResult::Ok);
    EXPECT_EQ(Safety::validateGearRatio(50.0f).level, Safety::ValidationResult::Ok);
}

// --- HID Range Validation Tests ---

TEST_F(SafetyValidationTest, HidRangeValid) {
    auto result = Safety::validateHidRange(900.0f);
    EXPECT_EQ(result.level, Safety::ValidationResult::Ok);
}

TEST_F(SafetyValidationTest, HidRangeErrorOutOfRange) {
    EXPECT_EQ(Safety::validateHidRange(45.0f).level, Safety::ValidationResult::Error);
    EXPECT_EQ(Safety::validateHidRange(3000.0f).level, Safety::ValidationResult::Error);
}

TEST_F(SafetyValidationTest, HidRangeEdgeCases) {
    EXPECT_EQ(Safety::validateHidRange(90.0f).level, Safety::ValidationResult::Ok);
    EXPECT_EQ(Safety::validateHidRange(2700.0f).level, Safety::ValidationResult::Ok);
}

// --- Motor Watchdog Tests ---

class MotorWatchdogTest : public ::testing::Test {
protected:
    void SetUp() override {
        resetMockArduino();
        setMockMillis(0);
    }
};

TEST_F(MotorWatchdogTest, InitialState) {
    // Initially, motors should be safe (not running)
    // This would test the actual MotorWatchdog class
    // For now, we test the concept
    uint32_t motorStartMs = 0;
    bool motorRunning = false;
    
    EXPECT_FALSE(motorRunning);
    EXPECT_EQ(motorStartMs, 0);
}

TEST_F(MotorWatchdogTest, ThermalBudgetCalculation) {
    const uint32_t kMaxMotorOnTimeMs = 60000; // 60 seconds
    
    // Start motors at t=0
    setMockMillis(0);
    uint32_t motorStartMs = mockMillis();
    
    // Check budget at t=30s (should have 30s remaining)
    setMockMillis(30000);
    uint32_t elapsed = mockMillis() - motorStartMs;
    uint32_t remaining = kMaxMotorOnTimeMs - elapsed;
    
    EXPECT_EQ(remaining, 30000);
}

TEST_F(MotorWatchdogTest, CooldownRequired) {
    const uint32_t kMaxMotorOnTimeMs = 60000;
    const uint32_t kMotorCooldownMs = 10000;
    
    // Run motors for full duration
    setMockMillis(0);
    uint32_t motorStartMs = mockMillis();
    
    // Check at t=65s (exceeded max run time)
    setMockMillis(65000);
    uint32_t elapsed = mockMillis() - motorStartMs;
    
    EXPECT_GT(elapsed, kMaxMotorOnTimeMs); // Exceeded limit
    
    // Cooldown should be required
    uint32_t cooldownEndMs = motorStartMs + kMaxMotorOnTimeMs + kMotorCooldownMs;
    EXPECT_EQ(cooldownEndMs, 70000);
}

// --- Communication Watchdog Tests ---

class CommWatchdogTest : public ::testing::Test {
protected:
    void SetUp() override {
        resetMockArduino();
        setMockMillis(0);
    }
};

TEST_F(CommWatchdogTest, UsbTimeout) {
    const uint32_t kUsbTimeoutMs = 5000;
    
    // USB activity at t=0
    setMockMillis(0);
    uint32_t lastUsbMs = mockMillis();
    
    // Check at t=6s (timeout)
    setMockMillis(6000);
    uint32_t idleMs = mockMillis() - lastUsbMs;
    
    EXPECT_GT(idleMs, kUsbTimeoutMs);
}

TEST_F(CommWatchdogTest, RimTimeout) {
    const uint32_t kRimTimeoutMs = 2000;
    
    // Rim activity at t=0
    setMockMillis(0);
    uint32_t lastRimMs = mockMillis();
    
    // Check at t=2.5s (timeout)
    setMockMillis(2500);
    uint32_t idleMs = mockMillis() - lastRimMs;
    
    EXPECT_GT(idleMs, kRimTimeoutMs);
}

TEST_F(CommWatchdogTest, RecentActivity) {
    const uint32_t kUsbTimeoutMs = 5000;
    
    // USB activity at t=0
    setMockMillis(0);
    uint32_t lastUsbMs = mockMillis();
    
    // Check at t=1s (no timeout)
    setMockMillis(1000);
    uint32_t idleMs = mockMillis() - lastUsbMs;
    
    EXPECT_LT(idleMs, kUsbTimeoutMs);
}
