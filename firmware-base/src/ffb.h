#pragma once

namespace Ffb {

enum class Mode {
    Off,      // commanded torque = 0
    Manual,   // constant test torque from serial
    Spring,   // recenter toward wheel zero
};

void begin();

void setMode(Mode m);
Mode mode();

// Manual test command in [-1, +1]
void setManualTorque(float t);
float manualTorque();

void setSpringK(float k);
float springK();
void setSpringDeadzone(float deg);
float springDeadzone();
void setTorqueCap(float t);
float torqueCap();

// Call every control tick with axle angle (deg relative to zero).
void update(float axleDegrees);

float commandedTorque();

}  // namespace Ffb
