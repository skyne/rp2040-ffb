// Testable wrapper for link protocol between base and rim MCUs
#include <stdint.h>
#include <string.h>

namespace LinkProtocolTestable {

// Frame structure: 0xAA 0x55 [ver] [type] [len] [payload...] [crc_lo] [crc_hi]

struct Frame {
    uint8_t type;
    uint8_t payload[128];
    uint8_t payloadLen;
};

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// Build a frame for transmission
size_t buildFrame(uint8_t type, const uint8_t* payload, uint8_t payloadLen, 
                  uint8_t* output, size_t outputSize) {
    if (payloadLen > 128) {
        return 0;
    }
    
    size_t frameSize = 2 + 3 + payloadLen + 2; // sync + header + payload + crc
    if (frameSize > outputSize) {
        return 0;
    }
    
    // Sync bytes
    output[0] = 0xAA;
    output[1] = 0x55;
    
    // Header
    output[2] = 1; // version
    output[3] = type;
    output[4] = payloadLen;
    
    // Payload
    if (payloadLen > 0) {
        memcpy(&output[5], payload, payloadLen);
    }
    
    // CRC over header + payload
    uint16_t crc = crc16(&output[2], 3 + payloadLen);
    output[5 + payloadLen] = crc & 0xFF;
    output[5 + payloadLen + 1] = (crc >> 8) & 0xFF;
    
    return frameSize;
}

// Parse a complete frame from buffer
bool parseFrame(const uint8_t* buffer, size_t bufferLen, Frame& frame) {
    if (bufferLen < 7) { // minimum frame size
        return false;
    }
    
    // Check sync
    if (buffer[0] != 0xAA || buffer[1] != 0x55) {
        return false;
    }
    
    uint8_t version = buffer[2];
    uint8_t type = buffer[3];
    uint8_t payloadLen = buffer[4];
    
    if (version != 1) {
        return false;
    }
    
    if (payloadLen > 128) {
        return false;
    }
    
    size_t expectedSize = 2 + 3 + payloadLen + 2;
    if (bufferLen < expectedSize) {
        return false;
    }
    
    // Verify CRC
    uint16_t expectedCrc = crc16(&buffer[2], 3 + payloadLen);
    uint16_t receivedCrc = buffer[5 + payloadLen] | (buffer[5 + payloadLen + 1] << 8);
    
    if (expectedCrc != receivedCrc) {
        return false;
    }
    
    // Extract frame data
    frame.type = type;
    frame.payloadLen = payloadLen;
    if (payloadLen > 0) {
        memcpy(frame.payload, &buffer[5], payloadLen);
    }
    
    return true;
}

} // namespace LinkProtocolTestable
