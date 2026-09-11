#pragma once

namespace MotorBts7960 {

void begin();

// Enable/disable both bridges (EN pins). Disabled = high-Z / coast.
void setEnabled(bool on);
bool enabled();

// Command per motor in [-1, +1]. Sign = direction, magnitude = PWM duty.
// +1 = RPWM driven, LPWM off; -1 = LPWM driven, RPWM off; 0 = both PWM low.
void setMotor1(float cmd);
void setMotor2(float cmd);

// Drive both motors the same (typical G920 dual-motor FFB).
void setBoth(float cmd);

void coast();  // duty 0, leave EN as-is
void stop();   // duty 0 + disable EN

float lastCmd1();
float lastCmd2();

void setDutyCap(float cap);
float dutyCap();

}  // namespace MotorBts7960
