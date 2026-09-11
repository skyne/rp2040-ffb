#pragma once

#include <stdint.h>

namespace Homing {

enum class Phase : uint8_t {
    Idle = 0,
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
