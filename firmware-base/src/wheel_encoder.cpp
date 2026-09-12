#include "wheel_encoder.h"

#include <math.h>

namespace WheelEncoder {
namespace {

float gear = 18.0f;
float unwrapped = 0.0f;
float zero = 0.0f;
float lastRaw = 0.0f;
float axle = 0.0f;
bool haveSample = false;
bool calActive = false;
float calStart = 0.0f;

float unwrap(float rawDeg) {
    if (!haveSample) {
        lastRaw = rawDeg;
        unwrapped = rawDeg;
        haveSample = true;
        return unwrapped;
    }

    float delta = rawDeg - lastRaw;
    if (delta > 180.0f)
        delta -= 360.0f;
    if (delta < -180.0f)
        delta += 360.0f;

    unwrapped += delta;
    lastRaw = rawDeg;
    return unwrapped;
}

} // namespace

void begin(float gearRatio) {
    gear = gearRatio;
    haveSample = false;
    calActive = false;
    zero = 0.0f;
    axle = 0.0f;
}

float update(float sensorRawDeg) {
    const float sens = unwrap(sensorRawDeg);
    axle = (sens - zero) / gear;
    return axle;
}

float axleDegrees() {
    return axle;
}
float sensorUnwrappedDegrees() {
    return unwrapped - zero;
}
float gearRatio() {
    return gear;
}

void setGearRatio(float ratio) {
    gear = ratio;
}

float zeroOffset() {
    return zero;
}

void setZeroOffset(float offset) {
    zero = offset;
    if (haveSample)
        axle = (unwrapped - zero) / gear;
}

void zeroHere() {
    zero = unwrapped;
    axle = 0.0f;
}

void setAxleDegrees(float deg) {
    // axle = (unwrapped - zero) / gear  →  zero = unwrapped - deg * gear
    zero = unwrapped - deg * gear;
    axle = deg;
}

void syncToIndexAngle(float indexDeg) {
    const float n = roundf((axle - indexDeg) / 360.0f);
    setAxleDegrees(indexDeg + n * 360.0f);
}

void syncMeasuredIndex(float measuredCenterAxle, float indexDeg) {
    const float n = roundf((measuredCenterAxle - indexDeg) / 360.0f);
    const float target = indexDeg + n * 360.0f;
    setAxleDegrees(axle + (target - measuredCenterAxle));
}

void startCal() {
    calStart = unwrapped;
    calActive = true;
}

bool finishCal() {
    if (!calActive)
        return false;
    calActive = false;

    const float sensorDelta = unwrapped - calStart;
    const float ratio = sensorDelta / 360.0f;
    if (fabsf(ratio) < 0.1f)
        return false;

    gear = ratio; // signed: encodes direction
    return true;
}

bool calibrating() {
    return calActive;
}

} // namespace WheelEncoder
