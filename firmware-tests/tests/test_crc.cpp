#include <gtest/gtest.h>

namespace SettingsTestable {
    uint16_t crc16Ccitt(const uint8_t* data, size_t len);
    uint32_t crc32Ieee(const uint8_t* data, size_t len);
}

class CrcTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(CrcTest, Crc16CcittKnownVector) {
    const uint8_t data[] = "123456789";
    uint16_t crc = SettingsTestable::crc16Ccitt(data, 9);
    EXPECT_EQ(crc, 0x29B1); // Known CRC16-CCITT value for "123456789"
}

TEST_F(CrcTest, Crc16CcittEmptyData) {
    uint16_t crc = SettingsTestable::crc16Ccitt(nullptr, 0);
    EXPECT_EQ(crc, 0xFFFF); // Initial value for empty data
}

TEST_F(CrcTest, Crc16CcittIsDeterministic) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc1 = SettingsTestable::crc16Ccitt(data, 4);
    uint16_t crc2 = SettingsTestable::crc16Ccitt(data, 4);
    EXPECT_EQ(crc1, crc2);
}

TEST_F(CrcTest, Crc16CcittChangesWithData) {
    const uint8_t data1[] = {0x01, 0x02, 0x03};
    const uint8_t data2[] = {0x01, 0x02, 0x04};
    uint16_t crc1 = SettingsTestable::crc16Ccitt(data1, 3);
    uint16_t crc2 = SettingsTestable::crc16Ccitt(data2, 3);
    EXPECT_NE(crc1, crc2);
}

TEST_F(CrcTest, Crc32IeeeKnownVector) {
    const uint8_t data[] = "123456789";
    uint32_t crc = SettingsTestable::crc32Ieee(data, 9);
    EXPECT_EQ(crc, 0xCBF43926); // Known CRC32-IEEE value for "123456789"
}

TEST_F(CrcTest, Crc32IeeeEmptyData) {
    uint32_t crc = SettingsTestable::crc32Ieee(nullptr, 0);
    EXPECT_EQ(crc, 0x00000000); // ~0xFFFFFFFF for empty data
}

TEST_F(CrcTest, Crc32IeeeIsDeterministic) {
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t crc1 = SettingsTestable::crc32Ieee(data, 4);
    uint32_t crc2 = SettingsTestable::crc32Ieee(data, 4);
    EXPECT_EQ(crc1, crc2);
}

TEST_F(CrcTest, Crc32IeeeChangesWithData) {
    const uint8_t data1[] = {0xDE, 0xAD, 0xBE, 0xEF};
    const uint8_t data2[] = {0xDE, 0xAD, 0xBE, 0xF0};
    uint32_t crc1 = SettingsTestable::crc32Ieee(data1, 4);
    uint32_t crc2 = SettingsTestable::crc32Ieee(data2, 4);
    EXPECT_NE(crc1, crc2);
}

TEST_F(CrcTest, Crc32IeeeDifferentLengths) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t crc3 = SettingsTestable::crc32Ieee(data, 3);
    uint32_t crc4 = SettingsTestable::crc32Ieee(data, 4);
    EXPECT_NE(crc3, crc4);
}
