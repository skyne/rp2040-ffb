#pragma once

#include <stdint.h>

namespace Homing {

enum class Phase : uint8_t {
    Idle = 0,
    ProbeAdxl,    // brief AccelGet; fall back to magnet if absent
    SeekGravity,  // drive/hand until calibrated X ≈ 0, then encoder zero
    SeekIndex,    // clear magnet if needed, approach until enter
    MeasureIndex, // exit → enter → exit (optional reverse passes) → window mid
    SeekZero,     // motor to virtual 0 (encoder target from index offset; no sensor)
};

void begin();

/** Boot / auto INIT — motors only if HOME_BOOT_USE_MOTORS or already armed. */
void start();
/**
 * Same as start(), but prefer motorized seek when the driver is safe
 * (explicit `n` / `:home` from the user).
 */
void startMotorized();
void startOrCancel();
bool active();
Phase phase();
bool usingMotors();

void update(bool indexEdge, float axleDeg, bool axleFresh = true);

const char* phaseName();

} // namespace Homing
