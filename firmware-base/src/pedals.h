#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace Pedals {

struct State {
    float throttle;  // 0..1 pressed
    float brake;
    float clutch;
    uint16_t rawThrottle;
    uint16_t rawBrake;
    uint16_t rawClutch;
};

struct AxisCal {
    int32_t minV = 0;
    int32_t maxV = 0;
    int32_t restV = 0;
    uint8_t haveRest = 0;
    uint8_t armed = 0;
    uint8_t inverted = 1;
    uint8_t _pad = 0;
};

void begin();
State read();

// Starts at 0 until pedals are plugged in and each axis sees a first press;
// then live min/max learning. `p` resets (HID stays 0 until pressed again).
void resetCalibration();
void captureExtents(const State &s);
bool calibrationReady();

void getCalibration(AxisCal &thr, AxisCal &brk, AxisCal &clu);
void setCalibration(const AxisCal &thr, const AxisCal &brk, const AxisCal &clu);

}  // namespace Pedals
