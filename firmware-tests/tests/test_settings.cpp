#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Declare testable functions
namespace SettingsTestable {
    float clampf(float v, float lo, float hi);
    float effectiveSoftLimitDeg(float softLimitDeg, float hidRange);
    bool isValidProfileSlot(uint8_t slot, uint8_t maxSlots);
}

class SettingsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SettingsTest, ClampfReturnsLowBound) {
    EXPECT_FLOAT_EQ(SettingsTestable::clampf(5.0f, 10.0f, 20.0f), 10.0f);
}

TEST_F(SettingsTest, ClampfReturnsHighBound) {
    EXPECT_FLOAT_EQ(SettingsTestable::clampf(25.0f, 10.0f, 20.0f), 20.0f);
}

TEST_F(SettingsTest, ClampfReturnsValueInRange) {
    EXPECT_FLOAT_EQ(SettingsTestable::clampf(15.0f, 10.0f, 20.0f), 15.0f);
}

TEST_F(SettingsTest, ClampfHandlesSameMinMax) {
    EXPECT_FLOAT_EQ(SettingsTestable::clampf(5.0f, 10.0f, 10.0f), 10.0f);
}

TEST_F(SettingsTest, EffectiveSoftLimitUsesExplicitValue) {
    EXPECT_FLOAT_EQ(SettingsTestable::effectiveSoftLimitDeg(100.0f, 900.0f), 100.0f);
}

TEST_F(SettingsTest, EffectiveSoftLimitUsesHalfRangeWhenZero) {
    EXPECT_FLOAT_EQ(SettingsTestable::effectiveSoftLimitDeg(0.0f, 900.0f), 450.0f);
}

TEST_F(SettingsTest, EffectiveSoftLimitUsesHalfRangeWhenBelowOne) {
    EXPECT_FLOAT_EQ(SettingsTestable::effectiveSoftLimitDeg(0.5f, 900.0f), 450.0f);
}

TEST_F(SettingsTest, ValidProfileSlotAcceptsZero) {
    EXPECT_TRUE(SettingsTestable::isValidProfileSlot(0, 4));
}

TEST_F(SettingsTest, ValidProfileSlotAcceptsMaxMinus1) {
    EXPECT_TRUE(SettingsTestable::isValidProfileSlot(3, 4));
}

TEST_F(SettingsTest, ValidProfileSlotRejectsMax) {
    EXPECT_FALSE(SettingsTestable::isValidProfileSlot(4, 4));
}

TEST_F(SettingsTest, ValidProfileSlotRejectsAboveMax) {
    EXPECT_FALSE(SettingsTestable::isValidProfileSlot(10, 4));
}
