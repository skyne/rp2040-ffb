// Testable wrapper for pedals module
#include <stdint.h>

namespace PedalsTestable {

struct AxisCalibration {
    int32_t minV;
    int32_t maxV;
    int32_t restV;
    bool haveRest;
    bool armed;
    bool inverted;
};

// Normalize axis value to 0..1 range
float normalizeAxis(int32_t raw, const AxisCalibration& cal) {
    if (!cal.armed) {
        return 0.0f;
    }
    
    int32_t min = cal.minV;
    int32_t max = cal.maxV;
    
    if (min == max) {
        return 0.0f;
    }
    
    float normalized = (float)(raw - min) / (float)(max - min);
    
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    
    if (cal.inverted) {
        normalized = 1.0f - normalized;
    }
    
    return normalized;
}

// Update calibration min/max during learning phase
void updateCalibration(AxisCalibration& cal, int32_t rawValue) {
    if (!cal.armed) {
        // First press - arm calibration
        if (!cal.haveRest) {
            cal.restV = rawValue;
            cal.haveRest = true;
        }
        cal.minV = rawValue;
        cal.maxV = rawValue;
        cal.armed = true;
        return;
    }
    
    // Update extents
    if (rawValue < cal.minV) {
        cal.minV = rawValue;
    }
    if (rawValue > cal.maxV) {
        cal.maxV = rawValue;
    }
}

// Check if calibration is ready for use
bool isCalibrationReady(const AxisCalibration& cal) {
    return cal.armed && cal.haveRest && (cal.maxV - cal.minV) > 100;
}

} // namespace PedalsTestable
