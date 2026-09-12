#pragma once

#include <stdint.h>

#include "ffb_link.h"

// Optional ADXL345 on shared I2C (addr 0x53). Soft-fails when absent.
namespace Adxl345 {

void begin(); // probe only; safe if chip missing
bool present();

// Single sample into out; false if missing or I2C error.
bool read(FfbLink::AccelReportPayload& out);

// Average `count` samples (clamped 1..100). Blocking ~count ms.
bool readAverage(FfbLink::AccelReportPayload& out, uint8_t count);

// Periodic poll for idle detection (call from Core0 loop).
void update();

// True if significant motion since last clear / boot (clears sticky wake).
bool motionRecent();
void clearMotion();

// Idle power-save: true after ADXL_IDLE_MS of stillness while present.
bool powerSaveActive();
bool consumeWakeEdge(); // true once when leaving power-save on motion

} // namespace Adxl345
