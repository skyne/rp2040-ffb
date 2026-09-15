#pragma once

#include <stdint.h>

namespace HidWheel {

/**
 * Register PID joystick HID once at boot (no later USB re-enum).
 * FFB torque still waits for INIT + motor arm; this only brings up the USB iface.
 */
bool begin();

/**
 * After INIT settles: allow HID input reports + host FFB auto-enter.
 * Does NOT disconnect/reconnect USB.
 */
void serviceAttach(bool homingActive);

bool ready();
bool attached();

/**
 * Core0: update snapshot and optionally pump TinyUSB (non-blocking).
 * Prefer calling this from the control tick; never block on USB.mutex.
 */
void update(float axleDegrees, float throttle, float brake, float clutch,
            float paddleClutchL = 0.0f, float paddleClutchR = 0.0f, uint32_t buttons = 0);

/** TinyUSB pump only (no new joystick report) — use during INIT. */
void serviceUsb();

int lastSteeringHid();

void setRangeDeg(float deg);
float rangeDeg();

} // namespace HidWheel
