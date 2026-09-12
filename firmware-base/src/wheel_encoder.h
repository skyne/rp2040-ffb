#pragma once

#include <Arduino.h>

namespace WheelEncoder {

void begin(float gearRatio = 18.0f);

// Feed absolute 0..360 magnet angle; returns axle degrees relative to zero.
float update(float sensorRawDeg);

float axleDegrees();
float sensorUnwrappedDegrees();
float gearRatio();
void setGearRatio(float ratio);

float zeroOffset();
void setZeroOffset(float offset);

void zeroHere();

// Force current pose to report `deg` (adjusts zero offset).
void setAxleDegrees(float deg);

// Snap to `indexDeg + 360*n` closest to current axle (multi-turn safe).
void syncToIndexAngle(float indexDeg);

// Make a previously measured axle reading (`measuredCenterAxle`) report as
// `indexDeg + 360*n` without requiring the wheel to still sit on that pose.
void syncMeasuredIndex(float measuredCenterAxle, float indexDeg);

// Two-step cal: startCal() → turn axle +360° → finishCal() sets ratio.
void startCal();
bool finishCal(); // false if movement too small
bool calibrating();

} // namespace WheelEncoder
