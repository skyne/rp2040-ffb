#include "ffb_pid.h"

#include <Arduino.h>
#include <math.h>
#include <pico/mutex.h>
#include <string.h>

#include "hid_pid_reports.h"

namespace FfbPid {
namespace {

using namespace HidPid;

mutex_t pidMu_;
bool pidMuReady_ = false;

struct PidLock {
    PidLock() {
        if (pidMuReady_)
            mutex_enter_blocking(&pidMu_);
    }
    ~PidLock() {
        if (pidMuReady_)
            mutex_exit(&pidMu_);
    }
};

EffectState effects_[kMaxEffects + 1]; // 1-based indices
uint8_t nextFree_ = 1;
uint16_t ramAvailable_ = kMemorySize;
uint8_t deviceGain_ = 255;
bool actuatorsOn_ = true;
bool paused_ = false;
bool autoEnter_ = false;
bool autoExit_ = false;

float posNorm_ = 0.0f;
float velNorm_ = 0.0f;
float accelNorm_ = 0.0f;
float demoSpringTargetDeg_ = 0.0f;

PidBlockLoadFeature blockLoad_{};

float clampf(float v, float lo, float hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

bool validId(uint8_t id) {
    return id >= 1 && id <= kMaxEffects;
}

EffectState* effect(uint8_t id) {
    if (!validId(id))
        return nullptr;
    return &effects_[id];
}

void requestAutoEnter() {
    autoEnter_ = true;
    autoExit_ = false;
}

void requestAutoExit() {
    autoExit_ = true;
}

uint8_t allocEffect() {
    for (uint8_t id = 1; id <= kMaxEffects; ++id) {
        if (effects_[id].state == kStateFree) {
            effects_[id] = EffectState{};
            effects_[id].state = kStateAllocated;
            if (ramAvailable_ >= kEffectSlotBytes)
                ramAvailable_ = (uint16_t)(ramAvailable_ - kEffectSlotBytes);
            return id;
        }
    }
    return 0;
}

void freeEffect(uint8_t id) {
    EffectState* e = effect(id);
    if (!e || e->state == kStateFree)
        return;
    *e = EffectState{};
    if (ramAvailable_ + kEffectSlotBytes <= kMemorySize)
        ramAvailable_ = (uint16_t)(ramAvailable_ + kEffectSlotBytes);
}

void freeAll() {
    for (uint8_t id = 1; id <= kMaxEffects; ++id)
        effects_[id] = EffectState{};
    ramAvailable_ = kMemorySize;
    nextFree_ = 1;
}

void stopAll() {
    for (uint8_t id = 1; id <= kMaxEffects; ++id) {
        if (effects_[id].state & kStatePlaying)
            effects_[id].state = (uint8_t)(effects_[id].state & ~kStatePlaying);
    }
}

void startEffect(uint8_t id) {
    EffectState* e = effect(id);
    if (!e || e->state == kStateFree)
        return;
    e->state = (uint8_t)(kStateAllocated | kStatePlaying);
    e->elapsedTime = 0;
    e->startMs = millis();
    requestAutoEnter();
}

void stopEffect(uint8_t id) {
    EffectState* e = effect(id);
    if (!e)
        return;
    e->state = (uint8_t)(e->state & ~kStatePlaying);
}

int32_t applyGain(int32_t value, uint8_t gain) {
    return (value * (int32_t)gain) / 255;
}

int32_t applyEnvelope(EffectState& e, int32_t value) {
    if (e.magnitude == 0)
        return 0;
    int32_t magnitude = applyGain(e.magnitude, e.gain);
    int32_t attackLevel = applyGain(e.attackLevel, e.gain);
    int32_t fadeLevel = applyGain(e.fadeLevel, e.gain);
    int32_t newValue = magnitude;
    const int32_t attackTime = (int32_t)e.attackTime;
    const int32_t fadeTime = (int32_t)e.fadeTime;
    const int32_t elapsed = (int32_t)e.elapsedTime;
    const int32_t duration = (int32_t)e.duration;

    if (attackTime > 0 && elapsed < attackTime) {
        newValue = (magnitude - attackLevel) * elapsed / attackTime;
        newValue += attackLevel;
    }
    if (duration != (int32_t)kDurationInfinite && fadeTime > 0 && elapsed > (duration - fadeTime)) {
        int32_t den = fadeTime;
        if (den < 1)
            den = 1;
        newValue = (magnitude - fadeLevel) * (duration - elapsed) / den;
        newValue += fadeLevel;
    }
    if (magnitude != 0)
        newValue = newValue * value / magnitude;
    return newValue;
}

int32_t constantForce(EffectState& e) {
    return applyEnvelope(e, e.magnitude);
}

int32_t rampForce(EffectState& e) {
    int32_t temp = e.startMagnitude;
    if (e.duration > 0 && e.duration != kDurationInfinite) {
        temp = (int32_t)(e.startMagnitude + (int32_t)e.elapsedTime *
                                                (e.endMagnitude - e.startMagnitude) /
                                                (int32_t)e.duration);
    }
    return applyEnvelope(e, temp);
}

int32_t squareForce(EffectState& e) {
    const int16_t offset = (int16_t)(e.offset * 2);
    const int16_t magnitude = e.magnitude;
    const uint32_t period = e.period ? e.period : 1;
    const uint32_t phasetime = ((uint32_t)e.phase * period) / 255u;
    const uint32_t reminder = (e.elapsedTime + phasetime) % period;
    const int32_t temp = (reminder > (period / 2u)) ? (offset - magnitude) : (offset + magnitude);
    return applyEnvelope(e, temp);
}

int32_t sineForce(EffectState& e) {
    const int16_t offset = (int16_t)(e.offset * 2);
    const uint32_t period = e.period ? e.period : 1;
    const float angle =
        ((float)e.elapsedTime / (float)period) * 2.0f * (float)M_PI + ((float)e.phase / 36000.0f);
    const int32_t temp = (int32_t)(sinf(angle) * (float)e.magnitude) + offset;
    return applyEnvelope(e, temp);
}

int32_t triangleForce(EffectState& e) {
    const int16_t offset = (int16_t)(e.offset * 2);
    const int16_t magnitude = e.magnitude;
    const uint32_t period = e.period ? e.period : 1;
    const uint32_t phasetime = ((uint32_t)e.phase * period) / 255u;
    const uint32_t reminder = (e.elapsedTime + phasetime) % period;
    const int32_t maxM = offset + magnitude;
    const int32_t minM = offset - magnitude;
    const int32_t slope = ((maxM - minM) * 2) / (int32_t)period;
    int32_t temp = (reminder > (period / 2u)) ? slope * (int32_t)(period - reminder)
                                              : slope * (int32_t)reminder;
    temp += minM;
    return applyEnvelope(e, temp);
}

int32_t sawDownForce(EffectState& e) {
    const int16_t offset = (int16_t)(e.offset * 2);
    const int16_t magnitude = e.magnitude;
    const uint32_t period = e.period ? e.period : 1;
    const uint32_t phasetime = ((uint32_t)e.phase * period) / 255u;
    const uint32_t reminder = (e.elapsedTime + phasetime) % period;
    const int32_t maxM = offset + magnitude;
    const int32_t minM = offset - magnitude;
    const int32_t slope = (maxM - minM) / (int32_t)period;
    int32_t temp = slope * (int32_t)(period - reminder) + minM;
    return applyEnvelope(e, temp);
}

int32_t sawUpForce(EffectState& e) {
    const int16_t offset = (int16_t)(e.offset * 2);
    const int16_t magnitude = e.magnitude;
    const uint32_t period = e.period ? e.period : 1;
    const uint32_t phasetime = ((uint32_t)e.phase * period) / 255u;
    const uint32_t reminder = (e.elapsedTime + phasetime) % period;
    const int32_t maxM = offset + magnitude;
    const int32_t minM = offset - magnitude;
    const int32_t slope = (maxM - minM) / (int32_t)period;
    int32_t temp = slope * (int32_t)reminder + minM;
    return applyEnvelope(e, temp);
}

int32_t conditionForce(EffectState& e, float metric, uint8_t axis) {
    if (axis >= kMaxAxes)
        axis = 0;
    const EffectCondition& c = e.conditions[axis];
    const float deadBand = (float)c.deadBand;
    const float cpOffset = (float)c.cpOffset;
    // metric + condition fields use HID ±10000 units.
    float tempForce = 0.0f;
    if (metric < (cpOffset - deadBand)) {
        tempForce = (metric - (cpOffset - deadBand)) * (float)c.negativeCoefficient / 10000.0f;
        tempForce = clampf(tempForce, -(float)c.negativeSaturation, (float)c.negativeSaturation);
    } else if (metric > (cpOffset + deadBand)) {
        tempForce = (metric - (cpOffset + deadBand)) * (float)c.positiveCoefficient / 10000.0f;
        tempForce = clampf(tempForce, -(float)c.negativeSaturation, (float)c.positiveSaturation);
    } else {
        return 0;
    }
    // Restoring: positive position error → negative torque.
    tempForce = -tempForce * (float)e.gain / 255.0f;
    return (int32_t)tempForce;
}

float directionRatioX(const EffectState& e) {
    if (!(e.enableAxis & kDirectionEnable))
        return 1.0f;
    const float angle = ((float)e.directionX * 360.0f / 255.0f) * (float)M_PI / 180.0f;
    return sinf(angle);
}

int32_t effectForce(EffectState& e) {
    const bool useDirForCondition =
        (e.enableAxis & kDirectionEnable) && e.conditionBlocksCount <= 1;
    const float dir = directionRatioX(e);
    int32_t force = 0;

    switch (e.effectType) {
    case kEffectConstant:
        force = (int32_t)(constantForce(e) * dir);
        break;
    case kEffectRamp:
        force = (int32_t)(rampForce(e) * dir);
        break;
    case kEffectSquare:
        force = (int32_t)(squareForce(e) * dir);
        break;
    case kEffectSine:
        force = (int32_t)(sineForce(e) * dir);
        break;
    case kEffectTriangle:
        force = (int32_t)(triangleForce(e) * dir);
        break;
    case kEffectSawtoothDown:
        force = (int32_t)(sawDownForce(e) * dir);
        break;
    case kEffectSawtoothUp:
        force = (int32_t)(sawUpForce(e) * dir);
        break;
    case kEffectSpring:
        force = conditionForce(e, posNorm_ * 10000.0f, 0);
        if (useDirForCondition)
            force = (int32_t)(force * dir);
        break;
    case kEffectDamper:
        force = conditionForce(e, velNorm_ * 10000.0f, 0);
        if (useDirForCondition)
            force = (int32_t)(force * dir);
        break;
    case kEffectInertia:
        force = conditionForce(e, fabsf(accelNorm_) * 10000.0f, 0);
        if (accelNorm_ < 0.0f)
            force = -force;
        if (useDirForCondition)
            force = (int32_t)(force * dir);
        break;
    case kEffectFriction:
        force = conditionForce(e, velNorm_ * 10000.0f, 0);
        if (useDirForCondition)
            force = (int32_t)(force * dir);
        break;
    default:
        break;
    }

    e.elapsedTime = millis() - e.startMs;
    return force;
}

void handleSetEffect(const uint8_t* data, uint16_t len) {
    if (len < sizeof(SetEffectOut))
        return;
    SetEffectOut r{};
    memcpy(&r, data, sizeof(r));
    EffectState* e = effect(r.effectBlockIndex);
    if (!e)
        return;
    e->duration = r.duration;
    e->directionX = r.directionX;
    e->directionY = r.directionY;
    e->effectType = r.effectType;
    e->gain = r.gain;
    e->enableAxis = r.enableAxis;
}

void handleSetEnvelope(const uint8_t* data, uint16_t len) {
    if (len < sizeof(SetEnvelopeOut))
        return;
    SetEnvelopeOut r{};
    memcpy(&r, data, sizeof(r));
    EffectState* e = effect(r.effectBlockIndex);
    if (!e)
        return;
    e->attackLevel = (int16_t)r.attackLevel;
    e->fadeLevel = (int16_t)r.fadeLevel;
    e->attackTime = r.attackTime;
    e->fadeTime = r.fadeTime;
}

void handleSetCondition(const uint8_t* data, uint16_t len) {
    if (len < sizeof(SetConditionOut))
        return;
    SetConditionOut r{};
    memcpy(&r, data, sizeof(r));
    EffectState* e = effect(r.effectBlockIndex);
    if (!e)
        return;
    uint8_t axis = (uint8_t)(r.parameterBlockOffset & 0x0F);
    if (axis >= kMaxAxes)
        axis = 0;
    e->conditions[axis].cpOffset = r.cpOffset;
    e->conditions[axis].positiveCoefficient = r.positiveCoefficient;
    e->conditions[axis].negativeCoefficient = r.negativeCoefficient;
    e->conditions[axis].positiveSaturation = r.positiveSaturation ? r.positiveSaturation : 10000;
    e->conditions[axis].negativeSaturation = r.negativeSaturation ? r.negativeSaturation : 10000;
    e->conditions[axis].deadBand = r.deadBand;
    if (e->conditionBlocksCount < (uint8_t)(axis + 1))
        e->conditionBlocksCount = (uint8_t)(axis + 1);
}

void handleSetPeriodic(const uint8_t* data, uint16_t len) {
    if (len < sizeof(SetPeriodicOut))
        return;
    SetPeriodicOut r{};
    memcpy(&r, data, sizeof(r));
    EffectState* e = effect(r.effectBlockIndex);
    if (!e)
        return;
    e->magnitude = (int16_t)r.magnitude;
    e->offset = r.offset;
    e->phase = r.phase;
    e->period = r.period ? r.period : 1;
}

void handleSetConstant(const uint8_t* data, uint16_t len) {
    if (len < sizeof(SetConstantForceOut))
        return;
    SetConstantForceOut r{};
    memcpy(&r, data, sizeof(r));
    EffectState* e = effect(r.effectBlockIndex);
    if (!e)
        return;
    e->magnitude = r.magnitude;
}

void handleSetRamp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(SetRampForceOut))
        return;
    SetRampForceOut r{};
    memcpy(&r, data, sizeof(r));
    EffectState* e = effect(r.effectBlockIndex);
    if (!e)
        return;
    e->startMagnitude = r.startMagnitude;
    e->endMagnitude = r.endMagnitude;
}

void handleEffectOp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(EffectOperationOut))
        return;
    EffectOperationOut r{};
    memcpy(&r, data, sizeof(r));
    EffectState* e = effect(r.effectBlockIndex);
    if (!e)
        return;
    if (r.operation == 1) {
        if (r.loopCount > 0 && e->duration != kDurationInfinite && e->duration > 0) {
            uint32_t d = (uint32_t)e->duration * (uint32_t)r.loopCount;
            e->duration = d > 0x7FFF ? kDurationInfinite : (uint16_t)d;
        }
        if (r.loopCount == 0xFF)
            e->duration = kDurationInfinite;
        startEffect(r.effectBlockIndex);
    } else if (r.operation == 2) {
        stopAll();
        startEffect(r.effectBlockIndex);
    } else if (r.operation == 3) {
        stopEffect(r.effectBlockIndex);
    }
}

void handleBlockFree(const uint8_t* data, uint16_t len) {
    if (len < sizeof(BlockFreeOut))
        return;
    BlockFreeOut r{};
    memcpy(&r, data, sizeof(r));
    if (r.effectBlockIndex == 0xFF)
        freeAll();
    else
        freeEffect(r.effectBlockIndex);
}

void handleDeviceControl(const uint8_t* data, uint16_t len) {
    if (len < sizeof(DeviceControlOut))
        return;
    DeviceControlOut r{};
    memcpy(&r, data, sizeof(r));
    switch (r.control) {
    case 1: // Enable Actuators
        actuatorsOn_ = true;
        requestAutoEnter();
        break;
    case 2: // Disable Actuators
        actuatorsOn_ = false;
        stopAll();
        requestAutoExit();
        break;
    case 3: // Stop All Effects
        stopAll();
        break;
    case 4: // Reset
        freeAll();
        actuatorsOn_ = true;
        paused_ = false;
        deviceGain_ = 255;
        requestAutoExit();
        break;
    case 5: // Pause
        paused_ = true;
        break;
    case 6: // Continue
        paused_ = false;
        break;
    default:
        break;
    }
}

void handleDeviceGain(const uint8_t* data, uint16_t len) {
    if (len < sizeof(DeviceGainOut))
        return;
    DeviceGainOut r{};
    memcpy(&r, data, sizeof(r));
    deviceGain_ = r.gain;
}

void handleCreateEffect(const uint8_t* data, uint16_t len) {
    (void)data;
    (void)len;
    blockLoad_.effectBlockIndex = allocEffect();
    if (blockLoad_.effectBlockIndex == 0) {
        blockLoad_.loadStatus = 2; // Full
    } else {
        blockLoad_.loadStatus = 1; // Success
        requestAutoEnter();
    }
    blockLoad_.ramPoolAvailable = ramAvailable_;
}

} // namespace

void begin() {
    if (!pidMuReady_) {
        mutex_init(&pidMu_);
        pidMuReady_ = true;
    }
    PidLock lock;
    freeAll();
    deviceGain_ = 255;
    actuatorsOn_ = true;
    paused_ = false;
    autoEnter_ = false;
    autoExit_ = false;
    posNorm_ = velNorm_ = accelNorm_ = 0.0f;
    demoSpringTargetDeg_ = 0.0f;
    blockLoad_ = PidBlockLoadFeature{};
}

void onSetReport(uint8_t reportId, uint8_t reportType, const uint8_t* data, uint16_t len) {
    PidLock lock;
    // TinyUSB may leave report ID as first byte when it did not strip it.
    if (reportId == 0 && len > 0) {
        reportId = data[0];
        ++data;
        --len;
    }

    // Feature Create New Effect
    if (reportType == 3 /* FEATURE */ && reportId == 5) {
        handleCreateEffect(data, len);
        return;
    }

    // Output reports
    if (reportType != 2 /* OUTPUT */ && reportType != 0)
        return;

    switch (reportId) {
    case 1:
        handleSetEffect(data, len);
        break;
    case 2:
        handleSetEnvelope(data, len);
        break;
    case 3:
        handleSetCondition(data, len);
        break;
    case 4:
        handleSetPeriodic(data, len);
        break;
    case 5:
        handleSetConstant(data, len);
        break;
    case 6:
        handleSetRamp(data, len);
        break;
    case 7:
    case 8:
    case 14:
        // Custom force / samples — ignored in v1
        break;
    case 10:
        handleEffectOp(data, len);
        break;
    case 11:
        handleBlockFree(data, len);
        break;
    case 12:
        handleDeviceControl(data, len);
        break;
    case 13:
        handleDeviceGain(data, len);
        break;
    default:
        break;
    }
}

uint16_t onGetReport(uint8_t reportId, uint8_t reportType, uint8_t* data, uint16_t len) {
    PidLock lock;
    // Always satisfy GET_REPORT — returning 0 can stall the control pipe on Linux FFB probes.
    if (reportType == 1 /* INPUT */) {
        if (reportId == kReportIdPidState) {
            if (len < sizeof(PidStateInput))
                return 0;
            PidStateInput out{};
            // bit0 paused, bit1 actuators, bit2 safety, bit3 override, bit4 power
            out.status = (uint8_t)((paused_ ? 0x01 : 0) | (actuatorsOn_ ? 0x02 : 0) | 0x0C |
                                   (actuatorsOn_ ? 0x10 : 0));
            uint8_t playingId = 0;
            for (uint8_t id = 1; id <= kMaxEffects; ++id) {
                if (effects_[id].state & kStatePlaying) {
                    playingId = id;
                    break;
                }
            }
            out.effectBlockIndex = (uint8_t)((playingId ? 0x80 : 0) | (playingId & 0x7F));
            memcpy(data, &out, sizeof(out));
            return sizeof(out);
        }
        return 0;
    }

    if (reportType != 3 /* FEATURE */)
        return 0;

    if (reportId == 6) {
        if (len < sizeof(PidBlockLoadFeature))
            return 0;
        PidBlockLoadFeature out = blockLoad_;
        if (out.loadStatus == 0) {
            out.loadStatus = 1;
            out.effectBlockIndex = 1;
            out.ramPoolAvailable = ramAvailable_;
        }
        memcpy(data, &out, sizeof(out));
        return sizeof(out);
    }
    if (reportId == 7) {
        if (len < sizeof(PidPoolFeature))
            return 0;
        PidPoolFeature out{};
        out.ramPoolSize = kMemorySize;
        out.maxSimultaneousEffects = kMaxEffects;
        out.memoryManagement = 0x03; // device managed + shared
        memcpy(data, &out, sizeof(out));
        return sizeof(out);
    }
    return 0;
}

void setAxisState(float positionNorm, float velocityNorm, float accelNorm) {
    PidLock lock;
    posNorm_ = clampf(positionNorm, -1.5f, 1.5f);
    velNorm_ = clampf(velocityNorm, -1.5f, 1.5f);
    accelNorm_ = clampf(accelNorm, -1.5f, 1.5f);
}

float computeTorque() {
    PidLock lock;
    if (!actuatorsOn_ || paused_)
        return 0.0f;

    int32_t sum = 0;
    for (uint8_t id = 1; id <= kMaxEffects; ++id) {
        EffectState& e = effects_[id];
        if (!(e.state & kStatePlaying))
            continue;
        if (e.duration != kDurationInfinite && e.elapsedTime > e.duration) {
            e.state = (uint8_t)(e.state & ~kStatePlaying);
            continue;
        }
        if (!(e.enableAxis & (kXAxisEnable | kDirectionEnable)) && e.enableAxis != 0)
            continue;
        sum += effectForce(e);
    }

    // Device gain then map ±10000 → ±1
    float t = (float)sum * ((float)deviceGain_ / 255.0f) / 10000.0f;
    return clampf(t, -1.0f, 1.0f);
}

bool actuatorsEnabled() {
    PidLock lock;
    return actuatorsOn_;
}
bool devicePaused() {
    PidLock lock;
    return paused_;
}
uint8_t deviceGain() {
    PidLock lock;
    return deviceGain_;
}

uint8_t playingCount() {
    PidLock lock;
    uint8_t n = 0;
    for (uint8_t id = 1; id <= kMaxEffects; ++id) {
        if (effects_[id].state & kStatePlaying)
            ++n;
    }
    return n;
}

uint8_t allocatedCount() {
    PidLock lock;
    uint8_t n = 0;
    for (uint8_t id = 1; id <= kMaxEffects; ++id) {
        if (effects_[id].state != kStateFree)
            ++n;
    }
    return n;
}

bool autoEnterRequested() {
    PidLock lock;
    return autoEnter_;
}
void clearAutoEnter() {
    PidLock lock;
    autoEnter_ = false;
}
bool autoExitRequested() {
    PidLock lock;
    return autoExit_;
}
void clearAutoExit() {
    PidLock lock;
    autoExit_ = false;
}

void setDeviceGain(uint8_t gain) {
    PidLock lock;
    deviceGain_ = gain;
}

void demoReset() {
    PidLock lock;
    freeAll();
    actuatorsOn_ = true;
    paused_ = false;
    deviceGain_ = 255;
    demoSpringTargetDeg_ = 0.0f;
    blockLoad_ = PidBlockLoadFeature{};
    requestAutoEnter();
}

static uint8_t ensureSlot(uint8_t id, uint8_t effectType) {
    EffectState* e = effect(id);
    if (!e)
        return 0;
    if (e->state == kStateFree) {
        *e = EffectState{};
        e->state = kStateAllocated;
        if (ramAvailable_ >= kEffectSlotBytes)
            ramAvailable_ = (uint16_t)(ramAvailable_ - kEffectSlotBytes);
    }
    e->effectType = effectType;
    e->gain = 255;
    e->enableAxis = kXAxisEnable;
    e->directionX = 0;
    e->duration = kDurationInfinite;
    return id;
}

void demoArmEffects() {
    PidLock lock;
    ensureSlot(kDemoSpringId, kEffectSpring);
    ensureSlot(kDemoSineId, kEffectSine);
    ensureSlot(kDemoConstId, kEffectConstant);

    EffectState* spring = effect(kDemoSpringId);
    if (spring) {
        spring->conditions[0].cpOffset = 0;
        spring->conditions[0].positiveCoefficient = 10000;
        spring->conditions[0].negativeCoefficient = 10000;
        spring->conditions[0].positiveSaturation = 10000;
        spring->conditions[0].negativeSaturation = 10000;
        spring->conditions[0].deadBand = 80;
        spring->conditionBlocksCount = 1;
        startEffect(kDemoSpringId);
    }
    demoSpringTargetDeg_ = 0.0f;

    // Rumble / kick start stopped until host updates them.
    stopEffect(kDemoSineId);
    stopEffect(kDemoConstId);

    EffectState* sine = effect(kDemoSineId);
    if (sine) {
        sine->magnitude = 0;
        sine->offset = 0;
        sine->phase = 0;
        sine->period = 50;
    }
    EffectState* cst = effect(kDemoConstId);
    if (cst) {
        cst->magnitude = 0;
    }

    actuatorsOn_ = true;
    paused_ = false;
    requestAutoEnter();
}

void demoSetSpringDeg(float targetDeg, float halfRangeDeg) {
    PidLock lock;
    EffectState* spring = effect(kDemoSpringId);
    // After a runaway trip demoEnd() frees slots — re-arm spring so showcase
    // :pid_spring lines do not silently no-op for the rest of the lap.
    if (!spring || spring->state == kStateFree) {
        ensureSlot(kDemoSpringId, kEffectSpring);
        spring = effect(kDemoSpringId);
        if (!spring)
            return;
        spring->conditions[0].positiveCoefficient = 10000;
        spring->conditions[0].negativeCoefficient = 10000;
        spring->conditions[0].positiveSaturation = 10000;
        spring->conditions[0].negativeSaturation = 10000;
        spring->conditions[0].deadBand = 80;
        spring->conditionBlocksCount = 1;
        actuatorsOn_ = true;
        paused_ = false;
        requestAutoEnter();
    }
    if (halfRangeDeg < 1.0f)
        halfRangeDeg = 1.0f;
    float n = targetDeg / halfRangeDeg;
    if (n < -1.0f)
        n = -1.0f;
    if (n > 1.0f)
        n = 1.0f;
    spring->conditions[0].cpOffset = (int16_t)lroundf(n * 10000.0f);
    demoSpringTargetDeg_ = targetDeg;
    if (!(spring->state & kStatePlaying))
        startEffect(kDemoSpringId);
}

float demoSpringTargetDeg() {
    PidLock lock;
    return demoSpringTargetDeg_;
}

void demoSetRumble(float amp01, float hz) {
    PidLock lock;
    EffectState* sine = effect(kDemoSineId);
    if (!sine || sine->state == kStateFree)
        return;
    if (amp01 < 0.0f)
        amp01 = 0.0f;
    if (amp01 > 1.0f)
        amp01 = 1.0f;
    if (hz < 1.0f)
        hz = 1.0f;
    if (hz > 80.0f)
        hz = 80.0f;

    if (amp01 < 0.02f) {
        sine->magnitude = 0;
        stopEffect(kDemoSineId);
        return;
    }

    sine->magnitude = (int16_t)lroundf(amp01 * 10000.0f);
    sine->offset = 0;
    sine->period = (uint32_t)lroundf(1000.0f / hz);
    if (sine->period < 8)
        sine->period = 8;
    if (!(sine->state & kStatePlaying))
        startEffect(kDemoSineId);
}

void demoSetConstant(float mag01) {
    PidLock lock;
    EffectState* cst = effect(kDemoConstId);
    if (!cst || cst->state == kStateFree)
        return;
    if (mag01 < -1.0f)
        mag01 = -1.0f;
    if (mag01 > 1.0f)
        mag01 = 1.0f;

    if (fabsf(mag01) < 0.02f) {
        cst->magnitude = 0;
        stopEffect(kDemoConstId);
        return;
    }

    cst->magnitude = (int16_t)lroundf(mag01 * 10000.0f);
    if (!(cst->state & kStatePlaying))
        startEffect(kDemoConstId);
}

void demoEnd() {
    PidLock lock;
    stopAll();
    freeAll();
    demoSpringTargetDeg_ = 0.0f;
    actuatorsOn_ = true;
    paused_ = false;
    requestAutoExit();
}

} // namespace FfbPid
