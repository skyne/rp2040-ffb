#pragma once

/**
 * @file motor_driver.h
 * @brief Swappable motor driver backend for rp2040-ffb base MCU
 *
 * Two backends (select at build time via MOTOR_DRIVER_MC33926):
 *  - BTS7960 / IBT-2 (default): sign-magnitude RPWM/LPWM + EN per channel
 *  - Pololu Dual MC33926: DIR + PWM per channel, shared D2 enable
 *
 * All FFB / homing / safety code talks to namespace MotorDriver only.
 */

namespace MotorDriver {

/** Human-readable backend id ("bts7960" or "mc33926"). */
const char* backendName();

void begin();

void setEnabled(bool on);
bool enabled();

void setMotor1(float cmd);
void setMotor2(float cmd);
void setBoth(float cmd);

void coast();
void stop();

float lastCmd1();
float lastCmd2();

void setDutyCap(float cap);
float dutyCap();

/** MC33926 only: latched fault on nSF (active low). Always false on BTS7960. */
bool faultActive();

/** MC33926 only: toggle D2 to clear latched overcurrent faults. No-op on BTS7960. */
void clearFault();

/** MC33926 only: motor current in amps when FB pins wired; else 0. */
float motor1CurrentA();
float motor2CurrentA();

} // namespace MotorDriver
