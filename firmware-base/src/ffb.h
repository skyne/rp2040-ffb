#pragma once

/**
 * @file ffb.h
 * @brief Force Feedback controller for rp2040-ffb steering wheel
 *
 * Computes desired motor torque from the active FFB mode. Motor drive is
 * handled by motor_driver.
 *
 * Modes:
 *  - Off: freewheel (USB PID may auto-enter Pid when the host enables effects)
 *  - Manual: constant test torque
 *  - Spring: local PD recenter at 0°
 *  - Track: local PD spring toward host target angle
 *  - Pid: USB HID PID effects from the game host
 */

namespace Ffb {

enum class Mode {
    Off,
    Manual,
    Spring,
    Track,
    Pid,
};

void begin();

void setMode(Mode m);
Mode mode();

void setManualTorque(float t);
float manualTorque();

void setSpringK(float k);
float springK();

void setSpringDeadzone(float deg);
float springDeadzone();

void setTargetDeg(float deg);
float targetDeg();

void setSpringD(float d);
float springD();

void setTorqueCap(float t);
float torqueCap();

/** Global user gain applied on top of USB device gain (Pid mode). Range 0..1. */
void setFfbGain(float g);
float ffbGain();

void setSoftLimitEnabled(bool on);
bool softLimitEnabled();

void setSoftLimitDeg(float deg);
float softLimitDeg();

void setSoftLimitK(float k);
float softLimitK();

void update(float axleDegrees);

float commandedTorque();

} // namespace Ffb
