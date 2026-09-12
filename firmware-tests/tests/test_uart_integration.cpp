/**
 * @file test_uart_integration.cpp
 * @brief Integration tests for UART protocol with ByteRing buffers
 */

#include <gtest/gtest.h>
#include <cstring>

// Mock Arduino environment
#include "../mocks/Arduino.h"

// Include shared protocol header
// For now, we'll define minimal test structures
namespace FfbLink {

static constexpr uint8_t kSync0 = 0xAA;
static constexpr uint8_t kSync1 = 0x55;
static constexpr uint8_t kVersion = 1;
static constexpr uint8_t kMaxPayload = 128;

enum Msg : uint8_t {
    Ping = 0x01,
    Pong = 0x02,
    Input = 0x10,
    Telemetry = 0x20,
};

// CRC-16/CCITT-FALSE
inline uint16_t crc16(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

// ByteRing (lock-free ring buffer)
template <uint16_t N> struct ByteRing {
    static_assert(N >= 4 && (N & (N - 1)) == 0, "ByteRing size must be power of 2");

    uint8_t buf[N];
    volatile uint16_t head = 0;
    volatile uint16_t tail = 0;

    static constexpr uint16_t mask() { return (uint16_t)(N - 1); }

    void clear() {
        head = 0;
        tail = 0;
    }

    uint16_t count() const { return (uint16_t)((head - tail) & mask()); }

    uint16_t freeSpace() const { return (uint16_t)(N - 1 - count()); }

    bool empty() const { return head == tail; }

    bool push(uint8_t b) {
        const uint16_t h = head;
        const uint16_t next = (uint16_t)((h + 1) & mask());
        if (next == tail)
            return false;
        buf[h] = b;
        head = next;
        return true;
    }

    bool pop(uint8_t& b) {
        const uint16_t t = tail;
        if (t == head)
            return false;
        b = buf[t];
        tail = (uint16_t)((t + 1) & mask());
        return true;
    }
};

// Frame builder
struct FrameBuilder {
    uint8_t buf[256];
    uint16_t len = 0;

    void reset() { len = 0; }

    void buildFrame(Msg msg, const uint8_t* payload, uint8_t payloadLen) {
        reset();
        buf[len++] = kSync0;
        buf[len++] = kSync1;
        buf[len++] = kVersion;
        buf[len++] = (uint8_t)msg;
        if (payload && payloadLen > 0) {
            memcpy(&buf[len], payload, payloadLen);
            len += payloadLen;
        }
        // CRC over version + msg + payload
        uint16_t crc = crc16(&buf[2], len - 2);
        buf[len++] = (uint8_t)(crc >> 8);
        buf[len++] = (uint8_t)(crc & 0xFF);
    }
};

} // namespace FfbLink

// --- Test Fixtures ---

class ByteRingTest : public ::testing::Test {
protected:
    FfbLink::ByteRing<16> ring;

    void SetUp() override {
        ArduinoMock::reset();
        ring.clear();
    }
};

class UartProtocolTest : public ::testing::Test {
protected:
    FfbLink::FrameBuilder builder;

    void SetUp() override {
        ArduinoMock::reset();
        builder.reset();
    }
};

// --- ByteRing Tests ---

TEST_F(ByteRingTest, InitiallyEmpty) {
    EXPECT_TRUE(ring.empty());
    EXPECT_EQ(ring.count(), 0);
    EXPECT_EQ(ring.freeSpace(), 15); // N-1 usable
}

TEST_F(ByteRingTest, PushPopSingle) {
    EXPECT_TRUE(ring.push(0x42));
    EXPECT_FALSE(ring.empty());
    EXPECT_EQ(ring.count(), 1);

    uint8_t val;
    EXPECT_TRUE(ring.pop(val));
    EXPECT_EQ(val, 0x42);
    EXPECT_TRUE(ring.empty());
}

TEST_F(ByteRingTest, PushPopMultiple) {
    for (uint8_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(ring.push(i));
    }
    EXPECT_EQ(ring.count(), 10);

    for (uint8_t i = 0; i < 10; ++i) {
        uint8_t val;
        EXPECT_TRUE(ring.pop(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(ring.empty());
}

TEST_F(ByteRingTest, FillCompletely) {
    // Fill to capacity (N-1 = 15)
    for (uint8_t i = 0; i < 15; ++i) {
        EXPECT_TRUE(ring.push(i));
    }
    EXPECT_EQ(ring.count(), 15);
    EXPECT_EQ(ring.freeSpace(), 0);

    // Next push should fail
    EXPECT_FALSE(ring.push(0xFF));
}

TEST_F(ByteRingTest, Wraparound) {
    // Fill and empty several times to test wraparound
    for (int iter = 0; iter < 3; ++iter) {
        for (uint8_t i = 0; i < 10; ++i) {
            EXPECT_TRUE(ring.push(i + iter * 10));
        }
        for (uint8_t i = 0; i < 10; ++i) {
            uint8_t val;
            EXPECT_TRUE(ring.pop(val));
            EXPECT_EQ(val, i + iter * 10);
        }
    }
}

TEST_F(ByteRingTest, PopFromEmpty) {
    uint8_t val;
    EXPECT_FALSE(ring.pop(val));
}

// --- UART Protocol Frame Tests ---

TEST_F(UartProtocolTest, PingFrameStructure) {
    builder.buildFrame(FfbLink::Msg::Ping, nullptr, 0);

    // Expected: AA 55 01 01 CRC16
    EXPECT_EQ(builder.len, 6);
    EXPECT_EQ(builder.buf[0], FfbLink::kSync0);
    EXPECT_EQ(builder.buf[1], FfbLink::kSync1);
    EXPECT_EQ(builder.buf[2], FfbLink::kVersion);
    EXPECT_EQ(builder.buf[3], (uint8_t)FfbLink::Msg::Ping);

    // Verify CRC
    uint16_t expectedCrc = FfbLink::crc16(&builder.buf[2], 2);
    uint16_t frameCrc = ((uint16_t)builder.buf[4] << 8) | builder.buf[5];
    EXPECT_EQ(frameCrc, expectedCrc);
}

TEST_F(UartProtocolTest, FrameWithPayload) {
    uint8_t payload[4] = {0x11, 0x22, 0x33, 0x44};
    builder.buildFrame(FfbLink::Msg::Telemetry, payload, 4);

    // Expected: AA 55 01 20 11 22 33 44 CRC16
    EXPECT_EQ(builder.len, 10);
    EXPECT_EQ(builder.buf[0], FfbLink::kSync0);
    EXPECT_EQ(builder.buf[1], FfbLink::kSync1);
    EXPECT_EQ(builder.buf[2], FfbLink::kVersion);
    EXPECT_EQ(builder.buf[3], (uint8_t)FfbLink::Msg::Telemetry);
    EXPECT_EQ(builder.buf[4], 0x11);
    EXPECT_EQ(builder.buf[5], 0x22);
    EXPECT_EQ(builder.buf[6], 0x33);
    EXPECT_EQ(builder.buf[7], 0x44);

    // Verify CRC
    uint16_t expectedCrc = FfbLink::crc16(&builder.buf[2], 6);
    uint16_t frameCrc = ((uint16_t)builder.buf[8] << 8) | builder.buf[9];
    EXPECT_EQ(frameCrc, expectedCrc);
}

TEST_F(UartProtocolTest, CrcDetectsCorruption) {
    builder.buildFrame(FfbLink::Msg::Ping, nullptr, 0);

    // Corrupt a byte
    uint8_t originalByte = builder.buf[3];
    builder.buf[3] ^= 0xFF;

    // Recalculate CRC and verify it differs
    uint16_t originalCrc = ((uint16_t)builder.buf[4] << 8) | builder.buf[5];
    uint16_t newCrc = FfbLink::crc16(&builder.buf[2], 2);
    EXPECT_NE(newCrc, originalCrc);

    // Restore
    builder.buf[3] = originalByte;
}

TEST_F(UartProtocolTest, MaxPayloadFrame) {
    uint8_t payload[FfbLink::kMaxPayload];
    for (uint8_t i = 0; i < FfbLink::kMaxPayload; ++i) {
        payload[i] = i;
    }

    builder.buildFrame(FfbLink::Msg::Input, payload, FfbLink::kMaxPayload);

    // Expected: AA 55 01 10 [128 payload bytes] CRC16
    EXPECT_EQ(builder.len, 4 + FfbLink::kMaxPayload + 2);
    EXPECT_EQ(builder.buf[0], FfbLink::kSync0);
    EXPECT_EQ(builder.buf[1], FfbLink::kSync1);

    // Verify payload
    for (uint8_t i = 0; i < FfbLink::kMaxPayload; ++i) {
        EXPECT_EQ(builder.buf[4 + i], i);
    }

    // Verify CRC
    uint16_t expectedCrc = FfbLink::crc16(&builder.buf[2], 2 + FfbLink::kMaxPayload);
    uint16_t frameCrc = ((uint16_t)builder.buf[builder.len - 2] << 8) | builder.buf[builder.len - 1];
    EXPECT_EQ(frameCrc, expectedCrc);
}

// --- Integration Tests: ByteRing + Protocol ---

class UartIntegrationTest : public ::testing::Test {
protected:
    FfbLink::ByteRing<256> rxRing;
    FfbLink::FrameBuilder builder;

    void SetUp() override {
        ArduinoMock::reset();
        rxRing.clear();
        builder.reset();
    }

    void sendFrame(FfbLink::Msg msg, const uint8_t* payload, uint8_t payloadLen) {
        builder.buildFrame(msg, payload, payloadLen);
        for (uint16_t i = 0; i < builder.len; ++i) {
            ASSERT_TRUE(rxRing.push(builder.buf[i]));
        }
    }
};

TEST_F(UartIntegrationTest, SendAndReceivePing) {
    sendFrame(FfbLink::Msg::Ping, nullptr, 0);

    EXPECT_EQ(rxRing.count(), 6); // AA 55 01 01 CRC16

    // Pop and verify sync bytes
    uint8_t b;
    EXPECT_TRUE(rxRing.pop(b));
    EXPECT_EQ(b, FfbLink::kSync0);
    EXPECT_TRUE(rxRing.pop(b));
    EXPECT_EQ(b, FfbLink::kSync1);
    EXPECT_TRUE(rxRing.pop(b));
    EXPECT_EQ(b, FfbLink::kVersion);
    EXPECT_TRUE(rxRing.pop(b));
    EXPECT_EQ(b, (uint8_t)FfbLink::Msg::Ping);
}

TEST_F(UartIntegrationTest, SendMultipleFrames) {
    // Send 3 frames
    sendFrame(FfbLink::Msg::Ping, nullptr, 0);
    uint8_t payload1[2] = {0xAA, 0xBB};
    sendFrame(FfbLink::Msg::Telemetry, payload1, 2);
    sendFrame(FfbLink::Msg::Pong, nullptr, 0);

    // Should have all bytes in ring
    uint16_t expectedLen = 6 + 8 + 6;
    EXPECT_EQ(rxRing.count(), expectedLen);
}

TEST_F(UartIntegrationTest, RingOverflowHandling) {
    // Try to send too many frames
    uint8_t payload[100];
    for (int i = 0; i < 5; ++i) {
        builder.buildFrame(FfbLink::Msg::Input, payload, 100);
        for (uint16_t j = 0; j < builder.len; ++j) {
            if (!rxRing.push(builder.buf[j])) {
                // Overflow expected
                EXPECT_GT(rxRing.count(), 200); // Should be nearly full
                return;
            }
        }
    }
}
