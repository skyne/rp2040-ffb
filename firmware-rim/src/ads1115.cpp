#include "ads1115.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "config.h"

namespace Ads1115 {
namespace {

constexpr uint8_t kRegConversion = 0x00;
constexpr uint8_t kRegConfig = 0x01;

// Single-shot, AIN0–AIN3 vs GND, ±4.096 V, 860 SPS, comparator disabled.
// OS=1 MUX=000 PGA=001 MODE=1 DR=111 COMP_QUE=11 → 0x83E3 (MUX filled per channel).
constexpr uint16_t kConfigTemplate = 0x83E3;
constexpr uint16_t kMuxShift = 12;
constexpr uint16_t kMuxAin0 = 0x4;  // 100
constexpr uint16_t kOsReady = 0x8000;

bool present_ = false;
uint8_t channel_ = 0;
bool pending_ = false;
int16_t raw_[FfbLink::kAnalogCount] = {};

bool writeReg16(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(ADS_ADDR);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xFF));
    return Wire.endTransmission() == 0;
}

bool readReg16(uint8_t reg, uint16_t &value) {
    Wire.beginTransmission(ADS_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)ADS_ADDR, 2) != 2) return false;
    const uint8_t hi = (uint8_t)Wire.read();
    const uint8_t lo = (uint8_t)Wire.read();
    value = (uint16_t)(((uint16_t)hi << 8) | lo);
    return true;
}

uint16_t configForChannel(uint8_t ch) {
    const uint16_t mux = (uint16_t)(kMuxAin0 + (ch & 3u));
    return (uint16_t)((kConfigTemplate & ~(0x7u << kMuxShift)) | (mux << kMuxShift));
}

bool startConversion(uint8_t ch) {
    return writeReg16(kRegConfig, configForChannel(ch));
}

bool conversionReady() {
    uint16_t cfg = 0;
    if (!readReg16(kRegConfig, cfg)) return false;
    return (cfg & kOsReady) != 0;
}

bool readConversion(int16_t &out) {
    uint16_t raw = 0;
    if (!readReg16(kRegConversion, raw)) return false;
    out = (int16_t)raw;
    return true;
}

}  // namespace

void begin() {
    present_ = false;
    pending_ = false;
    channel_ = 0;
    memset(raw_, 0, sizeof(raw_));

    Wire.beginTransmission(ADS_ADDR);
    if (Wire.endTransmission() != 0) return;

    // Touch config register — NACK / bus error → absent.
    uint16_t cfg = 0;
    if (!readReg16(kRegConfig, cfg)) return;

    if (!startConversion(0)) return;
    present_ = true;
    pending_ = true;
    channel_ = 0;
}

bool present() { return present_; }

void update() {
    if (!present_) return;

    if (pending_) {
        if (!conversionReady()) return;
        int16_t sample = 0;
        if (!readConversion(sample)) {
            // Transient I2C glitch — keep last value, retry same channel.
            (void)startConversion(channel_);
            return;
        }
        raw_[channel_] = sample;
        channel_ = (uint8_t)((channel_ + 1u) % FfbLink::kAnalogCount);
    }

    if (!startConversion(channel_)) {
        // Chip dropped off the bus — soft-fail without touching other I2C devices.
        present_ = false;
        pending_ = false;
        return;
    }
    pending_ = true;
}

void fillAnalog(int16_t out[FfbLink::kAnalogCount]) {
    if (!out) return;
    if (!present_) {
        memset(out, 0, sizeof(int16_t) * FfbLink::kAnalogCount);
        return;
    }
    memcpy(out, raw_, sizeof(raw_));
}

}  // namespace Ads1115
