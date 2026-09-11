#include "ffb.h"

#include <math.h>

#include "config.h"
#include "motor_bts7960.h"

namespace Ffb {
namespace {

Mode mode_ = Mode::Off;
float manual_ = 0.0f;
float cmd_ = 0.0f;
float springK_ = FFB_SPRING_K;
float springDz_ = FFB_SPRING_DEADZONE_DEG;
float torqueCap_ = FFB_TORQUE_CAP;

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

}  // namespace

void begin() {
    mode_ = Mode::Off;
    manual_ = 0.0f;
    cmd_ = 0.0f;
    springK_ = FFB_SPRING_K;
    springDz_ = FFB_SPRING_DEADZONE_DEG;
    torqueCap_ = FFB_TORQUE_CAP;
    MotorBts7960::coast();
}

void setMode(Mode m) { mode_ = m; }
Mode mode() { return mode_; }

void setManualTorque(float t) {
    manual_ = clampf(t, -1.0f, 1.0f);
}

float manualTorque() { return manual_; }

void setSpringK(float k) { springK_ = k; }
float springK() { return springK_; }
void setSpringDeadzone(float deg) { springDz_ = deg; }
float springDeadzone() { return springDz_; }
void setTorqueCap(float t) { torqueCap_ = t; }
float torqueCap() { return torqueCap_; }

void update(float axleDegrees) {
    switch (mode_) {
        case Mode::Off:
            cmd_ = 0.0f;
            break;
        case Mode::Manual:
            cmd_ = manual_;
            break;
        case Mode::Spring: {
            float x = axleDegrees;
            if (fabsf(x) < springDz_) {
                cmd_ = 0.0f;
            } else {
                cmd_ = clampf(-springK_ * x, -torqueCap_, torqueCap_);
            }
            break;
        }
    }

    if (MotorBts7960::enabled()) {
        MotorBts7960::setBoth(cmd_);
    } else {
        cmd_ = 0.0f;
    }
}

float commandedTorque() { return cmd_; }

}  // namespace Ffb
