// Testable wrapper for FFB (force feedback) module
#include <math.h>

namespace FfbTestable {

// Spring force calculation
float calculateSpringForce(float axleDeg, float springK, float springDeadzoneDeg) {
    float absAngle = fabsf(axleDeg);
    if (absAngle < springDeadzoneDeg) {
        return 0.0f;
    }
    float effectiveAngle = axleDeg > 0.0f ? 
        (axleDeg - springDeadzoneDeg) : 
        (axleDeg + springDeadzoneDeg);
    return -springK * effectiveAngle;
}

// Soft limit force calculation
float calculateSoftLimitForce(float axleDeg, float limitDeg, float limitK) {
    float absAngle = fabsf(axleDeg);
    if (absAngle < limitDeg) {
        return 0.0f;
    }
    float overshoot = absAngle - limitDeg;
    float force = -limitK * overshoot * overshoot;
    return axleDeg > 0.0f ? force : -force;
}

// Apply torque cap
float applyTorqueCap(float torque, float cap) {
    if (torque > cap) return cap;
    if (torque < -cap) return -cap;
    return torque;
}

// Combine spring and soft limit forces
float calculateTotalForce(float axleDeg, float springK, float springDz, 
                          float limitDeg, float limitK, bool limitEnabled) {
    float spring = calculateSpringForce(axleDeg, springK, springDz);
    if (!limitEnabled) {
        return spring;
    }
    float limit = calculateSoftLimitForce(axleDeg, limitDeg, limitK);
    return spring + limit;
}

} // namespace FfbTestable
