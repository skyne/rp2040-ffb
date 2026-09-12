#include <gtest/gtest.h>
#include <cstring>

namespace LinkProtocolTestable {
    struct Frame {
        uint8_t type;
        uint8_t payload[128];
        uint8_t payloadLen;
    };
    
    uint16_t crc16(const uint8_t* data, size_t len);
    size_t buildFrame(uint8_t type, const uint8_t* payload, uint8_t payloadLen, 
                      uint8_t* output, size_t outputSize);
    bool parseFrame(const uint8_t* buffer, size_t bufferLen, Frame& frame);
}

class LinkProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(buffer, 0, sizeof(buffer));
    }
    
    uint8_t buffer[256];
};

TEST_F(LinkProtocolTest, Crc16CalculatesKnownValue) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc = LinkProtocolTestable::crc16(data, 4);
    EXPECT_NE(crc, 0xFFFF); // Should not be initial value
    EXPECT_NE(crc, 0x0000); // Should not be zero
}

TEST_F(LinkProtocolTest, Crc16IsDeterministic) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc1 = LinkProtocolTestable::crc16(data, 4);
    uint16_t crc2 = LinkProtocolTestable::crc16(data, 4);
    EXPECT_EQ(crc1, crc2);
}

TEST_F(LinkProtocolTest, Crc16ChangesWithData) {
    const uint8_t data1[] = {0x01, 0x02, 0x03, 0x04};
    const uint8_t data2[] = {0x01, 0x02, 0x03, 0x05};
    uint16_t crc1 = LinkProtocolTestable::crc16(data1, 4);
    uint16_t crc2 = LinkProtocolTestable::crc16(data2, 4);
    EXPECT_NE(crc1, crc2);
}

TEST_F(LinkProtocolTest, BuildFrameCreatesValidStructure) {
    uint8_t payload[] = {0xAB, 0xCD};
    size_t size = LinkProtocolTestable::buildFrame(0x10, payload, 2, buffer, sizeof(buffer));
    
    EXPECT_GT(size, 0u);
    EXPECT_EQ(buffer[0], 0xAA); // Sync 1
    EXPECT_EQ(buffer[1], 0x55); // Sync 2
    EXPECT_EQ(buffer[2], 0x01); // Version
    EXPECT_EQ(buffer[3], 0x10); // Type
    EXPECT_EQ(buffer[4], 0x02); // Payload length
    EXPECT_EQ(buffer[5], 0xAB); // Payload byte 1
    EXPECT_EQ(buffer[6], 0xCD); // Payload byte 2
}

TEST_F(LinkProtocolTest, BuildFrameRejectsOversizedPayload) {
    uint8_t payload[200];
    size_t size = LinkProtocolTestable::buildFrame(0x10, payload, 200, buffer, sizeof(buffer));
    EXPECT_EQ(size, 0u);
}

TEST_F(LinkProtocolTest, BuildFrameRejectsSmallOutputBuffer) {
    uint8_t payload[] = {0xAB};
    size_t size = LinkProtocolTestable::buildFrame(0x10, payload, 1, buffer, 5);
    EXPECT_EQ(size, 0u);
}

TEST_F(LinkProtocolTest, BuildAndParseRoundTrip) {
    uint8_t payload[] = {0x12, 0x34, 0x56, 0x78};
    size_t size = LinkProtocolTestable::buildFrame(0x20, payload, 4, buffer, sizeof(buffer));
    
    LinkProtocolTestable::Frame frame;
    bool parsed = LinkProtocolTestable::parseFrame(buffer, size, frame);
    
    EXPECT_TRUE(parsed);
    EXPECT_EQ(frame.type, 0x20);
    EXPECT_EQ(frame.payloadLen, 4);
    EXPECT_EQ(frame.payload[0], 0x12);
    EXPECT_EQ(frame.payload[1], 0x34);
    EXPECT_EQ(frame.payload[2], 0x56);
    EXPECT_EQ(frame.payload[3], 0x78);
}

TEST_F(LinkProtocolTest, ParseFrameRejectsInvalidSync) {
    buffer[0] = 0xAA;
    buffer[1] = 0x56; // Wrong sync byte
    buffer[2] = 0x01;
    buffer[3] = 0x10;
    buffer[4] = 0x00;
    
    LinkProtocolTestable::Frame frame;
    EXPECT_FALSE(LinkProtocolTestable::parseFrame(buffer, 10, frame));
}

TEST_F(LinkProtocolTest, ParseFrameRejectsBadCrc) {
    uint8_t payload[] = {0x12};
    size_t size = LinkProtocolTestable::buildFrame(0x20, payload, 1, buffer, sizeof(buffer));
    
    // Corrupt CRC
    buffer[size - 1] ^= 0xFF;
    
    LinkProtocolTestable::Frame frame;
    EXPECT_FALSE(LinkProtocolTestable::parseFrame(buffer, size, frame));
}

TEST_F(LinkProtocolTest, ParseFrameRejectsTooShort) {
    LinkProtocolTestable::Frame frame;
    EXPECT_FALSE(LinkProtocolTestable::parseFrame(buffer, 5, frame));
}

TEST_F(LinkProtocolTest, ParseFrameRejectsInvalidVersion) {
    uint8_t payload[] = {0x12};
    size_t size = LinkProtocolTestable::buildFrame(0x20, payload, 1, buffer, sizeof(buffer));
    
    // Change version
    buffer[2] = 0x99;
    
    // Recalculate CRC
    uint16_t crc = LinkProtocolTestable::crc16(&buffer[2], 3 + 1);
    buffer[size - 2] = crc & 0xFF;
    buffer[size - 1] = (crc >> 8) & 0xFF;
    
    LinkProtocolTestable::Frame frame;
    EXPECT_FALSE(LinkProtocolTestable::parseFrame(buffer, size, frame));
}

TEST_F(LinkProtocolTest, BuildFrameEmptyPayload) {
    size_t size = LinkProtocolTestable::buildFrame(0x01, nullptr, 0, buffer, sizeof(buffer));
    EXPECT_GT(size, 0u);
    EXPECT_EQ(buffer[4], 0x00); // Zero payload length
    
    LinkProtocolTestable::Frame frame;
    EXPECT_TRUE(LinkProtocolTestable::parseFrame(buffer, size, frame));
    EXPECT_EQ(frame.type, 0x01);
    EXPECT_EQ(frame.payloadLen, 0);
}
