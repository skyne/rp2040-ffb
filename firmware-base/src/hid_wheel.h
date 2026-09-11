#pragma once

#include <stdint.h>

namespace HidWheel {

bool begin();
bool ready();
void update(float axleDegrees, float throttle, float brake, float clutch,
            uint32_t buttons = 0);
int lastSteeringHid();

void setRangeDeg(float deg);
float rangeDeg();

}  // namespace HidWheel
