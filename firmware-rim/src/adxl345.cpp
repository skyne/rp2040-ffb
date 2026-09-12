#include "adxl345.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "config.h"

namespace Adxl345 {
namespace {

constexpr uint8_t kRegDevid = 0x00;
constexpr uint8_t kRegBwRate = 0x2C;
constexpr uint8_t kRegPowerCtl = 0x2D;
constexpr uint8_t kRegDataFormat = 0x31;
constexpr uint8_t kRegDataX0 = 0x32;
constexpr uint8_t kExpectedDevid = 0xE5;

bool present_ = false;
bool powerSave_ = false;
bool wakeEdge_ = false;
uint32_t lastMotionMs_ = 0;
uint32_t lastPollMs_ = 0;
int16_t lastAx_ = 0, lastAy_ = 0, lastAz_ = 0;
bool haveSample_ = false;

bool writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(ADXL_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(ADXL_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
        return false;
    if (Wire.requestFrom((int)ADXL_ADDR, (int)len) != len)
        return false;
    for (uint8_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)Wire.read();
    }
    return true;
}

bool readRaw(int16_t& ax, int16_t& ay, int16_t& az) {
    uint8_t buf[6];
    if (!readRegs(kRegDataX0, buf, 6))
        return false;
    ax = (int16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
    ay = (int16_t)((uint16_t)buf[2] | ((uint16_t)buf[3] << 8));
    az = (int16_t)((uint16_t)buf[4] | ((uint16_t)buf[5] << 8));
    return true;
}

void noteMotion(int16_t ax, int16_t ay, int16_t az) {
    const uint32_t now = millis();
    if (!haveSample_) {
        lastAx_ = ax;
        lastAy_ = ay;
        lastAz_ = az;
        haveSample_ = true;
        lastMotionMs_ = now;
        return;
    }
    const int32_t dx = (int32_t)ax - lastAx_;
    const int32_t dy = (int32_t)ay - lastAy_;
    const int32_t dz = (int32_t)az - lastAz_;
    lastAx_ = ax;
    lastAy_ = ay;
    lastAz_ = az;
    const int32_t mag2 = dx * dx + dy * dy + dz * dz;
    const int32_t thr = (int32_t)ADXL_MOTION_THRESHOLD_RAW;
    if (mag2 >= thr * thr) {
        lastMotionMs_ = now;
        if (powerSave_) {
            powerSave_ = false;
            wakeEdge_ = true;
        }
    }
}

} // namespace

void begin() {
    present_ = false;
    powerSave_ = false;
    wakeEdge_ = false;
    haveSample_ = false;
    lastMotionMs_ = millis();
    lastPollMs_ = 0;

    Wire.beginTransmission(ADXL_ADDR);
    if (Wire.endTransmission() != 0)
        return;

    uint8_t devid = 0;
    if (!readRegs(kRegDevid, &devid, 1) || devid != kExpectedDevid)
        return;

    // ±2g full-res, 100 Hz — quiet enough for incline + idle detect.
    if (!writeReg(kRegDataFormat, 0x08))
        return;
    if (!writeReg(kRegBwRate, 0x0A))
        return; // 100 Hz
    if (!writeReg(kRegPowerCtl, 0x08))
        return; // Measure

    present_ = true;
    lastMotionMs_ = millis();
}

bool present() {
    return present_;
}

bool read(FfbLink::AccelReportPayload& out) {
    out = FfbLink::AccelReportPayload{};
    out.present = present_ ? 1 : 0;
    if (!present_)
        return false;
    int16_t ax = 0, ay = 0, az = 0;
    if (!readRaw(ax, ay, az)) {
        out.ok = 0;
        return false;
    }
    out.ok = 1;
    out.ax = ax;
    out.ay = ay;
    out.az = az;
    out.samples = 1;
    noteMotion(ax, ay, az);
    return true;
}

bool readAverage(FfbLink::AccelReportPayload& out, uint8_t count) {
    out = FfbLink::AccelReportPayload{};
    out.present = present_ ? 1 : 0;
    if (!present_)
        return false;
    if (count == 0)
        count = 100;
    if (count > 100)
        count = 100;

    int32_t sx = 0, sy = 0, sz = 0;
    uint8_t got = 0;
    for (uint8_t i = 0; i < count; ++i) {
        int16_t ax = 0, ay = 0, az = 0;
        if (!readRaw(ax, ay, az))
            continue;
        sx += ax;
        sy += ay;
        sz += az;
        got++;
        noteMotion(ax, ay, az);
        delay(2);
    }
    if (got == 0) {
        out.ok = 0;
        return false;
    }
    out.ok = 1;
    out.ax = (int16_t)(sx / got);
    out.ay = (int16_t)(sy / got);
    out.az = (int16_t)(sz / got);
    out.samples = got;
    return true;
}

void update() {
    if (!present_)
        return;
    const uint32_t now = millis();
    if (now - lastPollMs_ < ADXL_POLL_MS)
        return;
    lastPollMs_ = now;

    int16_t ax = 0, ay = 0, az = 0;
    if (!readRaw(ax, ay, az))
        return;
    noteMotion(ax, ay, az);

    if (!powerSave_ && (now - lastMotionMs_ >= ADXL_IDLE_MS)) {
        powerSave_ = true;
    }
}

bool motionRecent() {
    if (!present_)
        return false;
    return (millis() - lastMotionMs_) < ADXL_MOTION_HOLD_MS;
}

void clearMotion() {
    lastMotionMs_ = millis();
    if (powerSave_) {
        powerSave_ = false;
        wakeEdge_ = true;
    }
}

bool powerSaveActive() {
    return present_ && powerSave_;
}

bool consumeWakeEdge() {
    if (!wakeEdge_)
        return false;
    wakeEdge_ = false;
    return true;
}

} // namespace Adxl345
