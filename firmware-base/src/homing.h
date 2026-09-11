#pragma once

#include <stdint.h>

namespace Homing {

enum class Phase : uint8_t {
    Idle = 0,
    ProbeAdxl,     // brief AccelGet; fall back to magnet if absent
    SeekGravity,   // drive/hand until calibrated X ≈ 0, then encoder zero
    SeekIndex,     // clear magnet if needed, approach until enter
    MeasureIndex,  // slow pass(es): enter/exit → window mid
    SeekZero,      // toward true 0 using measured mid + config offset
};

void begin();

void start();
void startOrCancel();
bool active();
Phase phase();
bool usingMotors();

void update(bool indexEdge, float axleDeg);

const char *phaseName();

}  // namespace Homing
