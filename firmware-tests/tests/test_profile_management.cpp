#include <gtest/gtest.h>

// Test profile management logic
namespace SettingsTestable {
    bool isValidProfileSlot(uint8_t slot, uint8_t maxSlots);
}

class ProfileManagementTest : public ::testing::Test {
protected:
    static constexpr uint8_t kProfileCount = 4;
};

TEST_F(ProfileManagementTest, ProfileSlotValidation) {
    for (uint8_t i = 0; i < kProfileCount; ++i) {
        EXPECT_TRUE(SettingsTestable::isValidProfileSlot(i, kProfileCount));
    }
    
    EXPECT_FALSE(SettingsTestable::isValidProfileSlot(kProfileCount, kProfileCount));
    EXPECT_FALSE(SettingsTestable::isValidProfileSlot(255, kProfileCount));
}

// Add more profile tests here as we expose more testable profile logic
