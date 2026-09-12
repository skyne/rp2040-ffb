#pragma once

#include <Arduino.h>

namespace Mlx90363 {

bool begin();
// Absolute magnet angle 0..360. Returns false on CRC error.
bool readAlphaDegrees(float& degreesOut);

} // namespace Mlx90363
