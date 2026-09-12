#include <gtest/gtest.h>
#include <cmath>

namespace FfbTestable {
    float calculateSpringForce(float axleDeg, float springK, float springDeadzoneDeg);
    float calculateSoftLimitForce(float axleDeg, float limitDeg, float limitK);
    float applyTorqueCap(float torque, float cap);
    float calculateTotalForce(float axleDeg, float springK, float springDz, 
                              float limitDeg, float limitK, bool limitEnabled);
}

class FfbTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FfbTest, SpringForceZeroInDeadzone) {
    EXPECT_FLOAT_EQ(FfbTestable::calculateSpringForce(1.0f, 0.004f, 2.0f), 0.0f);
    EXPECT_FLOAT_EQ(FfbTestable::calculateSpringForce(-1.0f, 0.004f, 2.0f), 0.0f);
}

TEST_F(FfbTest, SpringForceProportionalOutsideDeadzone) {
    float force = FfbTestable::calculateSpringForce(10.0f, 0.004f, 2.0f);
    EXPECT_LT(force, 0.0f); // Should be negative (resisting)
    EXPECT_NEAR(force, -0.032f, 0.001f); // -0.004 * (10 - 2)
}

TEST_F(FfbTest, SpringForceSymmetricNegativeAngle) {
    float forcePos = FfbTestable::calculateSpringForce(10.0f, 0.004f, 2.0f);
    float forceNeg = FfbTestable::calculateSpringForce(-10.0f, 0.004f, 2.0f);
    EXPECT_NEAR(forcePos, -forceNeg, 0.001f);
}

TEST_F(FfbTest, SoftLimitZeroWithinLimit) {
    EXPECT_FLOAT_EQ(FfbTestable::calculateSoftLimitForce(100.0f, 450.0f, 0.012f), 0.0f);
}

TEST_F(FfbTest, SoftLimitIncreasesWithOvershoot) {
    float force1 = FfbTestable::calculateSoftLimitForce(460.0f, 450.0f, 0.012f);
    float force2 = FfbTestable::calculateSoftLimitForce(470.0f, 450.0f, 0.012f);
    EXPECT_LT(force1, 0.0f); // Resisting
    EXPECT_LT(force2, force1); // Stronger resistance
}

TEST_F(FfbTest, SoftLimitQuadraticIncrease) {
    // Overshoot of 10 degrees
    float force1 = FfbTestable::calculateSoftLimitForce(460.0f, 450.0f, 0.012f);
    // Overshoot of 20 degrees (double)
    float force2 = FfbTestable::calculateSoftLimitForce(470.0f, 450.0f, 0.012f);
    // Force should be roughly 4x (quadratic)
    EXPECT_NEAR(force2 / force1, 4.0f, 0.5f);
}

TEST_F(FfbTest, TorqueCapLimitsPositive) {
    EXPECT_FLOAT_EQ(FfbTestable::applyTorqueCap(0.5f, 0.35f), 0.35f);
}

TEST_F(FfbTest, TorqueCapLimitsNegative) {
    EXPECT_FLOAT_EQ(FfbTestable::applyTorqueCap(-0.5f, 0.35f), -0.35f);
}

TEST_F(FfbTest, TorqueCapPreservesValueInRange) {
    EXPECT_FLOAT_EQ(FfbTestable::applyTorqueCap(0.25f, 0.35f), 0.25f);
    EXPECT_FLOAT_EQ(FfbTestable::applyTorqueCap(-0.25f, 0.35f), -0.25f);
}

TEST_F(FfbTest, TotalForceOnlySpringWhenLimitDisabled) {
    float total = FfbTestable::calculateTotalForce(500.0f, 0.004f, 2.0f, 450.0f, 0.012f, false);
    float spring = FfbTestable::calculateSpringForce(500.0f, 0.004f, 2.0f);
    EXPECT_FLOAT_EQ(total, spring);
}

TEST_F(FfbTest, TotalForceCombinesSpringAndLimitWhenEnabled) {
    float total = FfbTestable::calculateTotalForce(500.0f, 0.004f, 2.0f, 450.0f, 0.012f, true);
    float spring = FfbTestable::calculateSpringForce(500.0f, 0.004f, 2.0f);
    float limit = FfbTestable::calculateSoftLimitForce(500.0f, 450.0f, 0.012f);
    EXPECT_FLOAT_EQ(total, spring + limit);
}
