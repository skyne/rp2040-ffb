#pragma once

#include "pedals.h"

namespace ControlTick {

// Optional CDC/CLI pump (set from main). Skipped when null.
using SerialHook = void (*)();

void begin(SerialHook serialHook = nullptr);

// Hall → encoder → index → homing → pedals → FFB → HID → rim UART → LEDs.
// Safe to call from the e-paper BUSY wait (re-entrant guarded).
void service();

void armIndexSyncLatch(); // serial 'i' — sync on next index edge

bool lastHallOk();
float lastAxleDeg();
float lastSensorDeg();
const Pedals::State& lastPedals();

} // namespace ControlTick
