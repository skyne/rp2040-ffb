#include <gtest/gtest.h>

namespace PedalsTestable {
    struct AxisCalibration {
        int32_t minV;
        int32_t maxV;
        int32_t restV;
        bool haveRest;
        bool armed;
        bool inverted;
    };
    
    float normalizeAxis(int32_t raw, const AxisCalibration& cal);
    void updateCalibration(AxisCalibration& cal, int32_t rawValue);
    bool isCalibrationReady(const AxisCalibration& cal);
}

class PedalsTest : public ::testing::Test {
protected:
    void SetUp() override {
        cal = {0, 4095, 0, false, false, false};
    }
    
    PedalsTestable::AxisCalibration cal;
};

TEST_F(PedalsTest, NormalizeReturnsZeroWhenNotArmed) {
    cal.armed = false;
    EXPECT_FLOAT_EQ(PedalsTestable::normalizeAxis(2048, cal), 0.0f);
}

TEST_F(PedalsTest, NormalizeReturnsZeroForMinValue) {
    cal.armed = true;
    cal.minV = 100;
    cal.maxV = 4000;
    EXPECT_FLOAT_EQ(PedalsTestable::normalizeAxis(100, cal), 0.0f);
}

TEST_F(PedalsTest, NormalizeReturnsOneForMaxValue) {
    cal.armed = true;
    cal.minV = 100;
    cal.maxV = 4000;
    EXPECT_FLOAT_EQ(PedalsTestable::normalizeAxis(4000, cal), 1.0f);
}

TEST_F(PedalsTest, NormalizeReturnsMidpoint) {
    cal.armed = true;
    cal.minV = 100;
    cal.maxV = 4000;
    EXPECT_NEAR(PedalsTestable::normalizeAxis(2050, cal), 0.5f, 0.01f);
}

TEST_F(PedalsTest, NormalizeInvertsWhenFlagSet) {
    cal.armed = true;
    cal.minV = 100;
    cal.maxV = 4000;
    cal.inverted = true;
    EXPECT_FLOAT_EQ(PedalsTestable::normalizeAxis(100, cal), 1.0f);
    EXPECT_FLOAT_EQ(PedalsTestable::normalizeAxis(4000, cal), 0.0f);
}

TEST_F(PedalsTest, NormalizeClampsBelowMin) {
    cal.armed = true;
    cal.minV = 100;
    cal.maxV = 4000;
    EXPECT_FLOAT_EQ(PedalsTestable::normalizeAxis(50, cal), 0.0f);
}

TEST_F(PedalsTest, NormalizeClampsAboveMax) {
    cal.armed = true;
    cal.minV = 100;
    cal.maxV = 4000;
    EXPECT_FLOAT_EQ(PedalsTestable::normalizeAxis(5000, cal), 1.0f);
}

TEST_F(PedalsTest, UpdateCalibrationArmsOnFirstPress) {
    cal.armed = false;
    cal.haveRest = false;
    PedalsTestable::updateCalibration(cal, 500);
    EXPECT_TRUE(cal.armed);
    EXPECT_TRUE(cal.haveRest);
    EXPECT_EQ(cal.restV, 500);
    EXPECT_EQ(cal.minV, 500);
    EXPECT_EQ(cal.maxV, 500);
}

TEST_F(PedalsTest, UpdateCalibrationExpandsMin) {
    cal.armed = true;
    cal.minV = 500;
    cal.maxV = 3000;
    PedalsTestable::updateCalibration(cal, 200);
    EXPECT_EQ(cal.minV, 200);
}

TEST_F(PedalsTest, UpdateCalibrationExpandsMax) {
    cal.armed = true;
    cal.minV = 500;
    cal.maxV = 3000;
    PedalsTestable::updateCalibration(cal, 3500);
    EXPECT_EQ(cal.maxV, 3500);
}

TEST_F(PedalsTest, UpdateCalibrationKeepsExistingMinMax) {
    cal.armed = true;
    cal.minV = 500;
    cal.maxV = 3000;
    PedalsTestable::updateCalibration(cal, 2000);
    EXPECT_EQ(cal.minV, 500);
    EXPECT_EQ(cal.maxV, 3000);
}

TEST_F(PedalsTest, CalibrationNotReadyWhenNotArmed) {
    cal.armed = false;
    cal.haveRest = true;
    cal.minV = 100;
    cal.maxV = 4000;
    EXPECT_FALSE(PedalsTestable::isCalibrationReady(cal));
}

TEST_F(PedalsTest, CalibrationNotReadyWithoutRest) {
    cal.armed = true;
    cal.haveRest = false;
    cal.minV = 100;
    cal.maxV = 4000;
    EXPECT_FALSE(PedalsTestable::isCalibrationReady(cal));
}

TEST_F(PedalsTest, CalibrationNotReadyWithSmallRange) {
    cal.armed = true;
    cal.haveRest = true;
    cal.minV = 2000;
    cal.maxV = 2050; // Only 50 units
    EXPECT_FALSE(PedalsTestable::isCalibrationReady(cal));
}

TEST_F(PedalsTest, CalibrationReadyWithGoodData) {
    cal.armed = true;
    cal.haveRest = true;
    cal.minV = 100;
    cal.maxV = 4000;
    EXPECT_TRUE(PedalsTestable::isCalibrationReady(cal));
}
