// Testable wrapper for settings module
// This file extracts testable logic from settings.cpp for unit testing
#include <stdint.h>
#include <math.h>

namespace SettingsTestable {

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float effectiveSoftLimitDeg(float softLimitDeg, float hidRange) {
    if (softLimitDeg >= 1.0f) return softLimitDeg;
    return hidRange * 0.5f;
}

bool isValidProfileSlot(uint8_t slot, uint8_t maxSlots) {
    return slot < maxSlots;
}

// CRC16-CCITT implementation for link protocol
uint16_t crc16Ccitt(const uint8_t* data, size_t len) {
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

// CRC32-IEEE for firmware validation
uint32_t crc32Ieee(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

} // namespace SettingsTestable
