#pragma once

#include <stdint.h>

namespace HidWheel {

bool begin();
bool ready();

// paddleClutchL/R: 0..1 from rim ADS1115 (0 when chip absent / unlink).
// Mapped to HID Rx / Ry (sliderLeft / sliderRight).
void update(float axleDegrees, float throttle, float brake, float clutch,
            float paddleClutchL = 0.0f, float paddleClutchR = 0.0f,
            uint32_t buttons = 0);
int lastSteeringHid();

void setRangeDeg(float deg);
float rangeDeg();

}  // namespace HidWheel
