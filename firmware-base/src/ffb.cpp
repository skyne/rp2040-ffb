#include "ffb.h"

#include <Arduino.h>
#include <math.h>

#include "config.h"
#include "ffb_pid.h"
#include "hid_wheel.h"
#include "motor_driver.h"
#include "safety.h"

namespace Ffb {
namespace {

Mode mode_ = Mode::Off;
bool autoPid_ = false; // true when Pid was entered via USB auto-switch
float manual_ = 0.0f;
float cmd_ = 0.0f;
float springK_ = FFB_SPRING_K;
float springDz_ = FFB_SPRING_DEADZONE_DEG;
float springD_ = 0.006f;
float targetCmd_ = 0.0f;
float targetSmooth_ = 0.0f;
float torqueCap_ = FFB_TORQUE_CAP;
float ffbGain_ = 1.0f;
bool softEn_ = true;
float softDeg_ = 0.0f;
float softK_ = 0.012f;
float prevAxle_ = 0.0f;
float velFilt_ = 0.0f;
float accelFilt_ = 0.0f;
float cmdFilt_ = 0.0f;
bool havePrevAxle_ = false;
uint32_t prevMs_ = 0;
uint32_t runawaySameSignMs_ = 0;

constexpr float kTargetClampDeg = 900.0f;
constexpr float kTargetSlewDegPerSec = 1400.0f;
constexpr float kVelFiltAlpha = 0.10f;
constexpr float kAccelFiltAlpha = 0.15f;
constexpr float kCmdFiltAlpha = 0.45f;
constexpr float kMaxVelNormDegPerSec = 400.0f;
constexpr float kMaxAccelNormDegPerSec2 = 4000.0f;
// Same-sign torque + velocity for this long → emergency stop (anti-runaway).
constexpr float kRunawayVelDegPerSec = 90.0f;
constexpr float kRunawayCmdMin = 0.08f;
constexpr uint32_t kRunawayHoldMs = 120u;
constexpr float kRunawayHardVelDegPerSec = 420.0f;
constexpr float kPidTorqueSlewPerSec = 2.5f; // limit host PID snap to full duty

float clampf(float v, float lo, float hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

float applySoftLimit(float base, float axleDegrees) {
    if (!softEn_ || softDeg_ < 1.0f)
        return base;
    const float lim = softDeg_;
    const float over = fabsf(axleDegrees) - lim;
    if (over <= 0.0f)
        return base;
    const float dir = axleDegrees > 0.0f ? 1.0f : -1.0f;
    const float bump = clampf(-dir * softK_ * over, -torqueCap_, torqueCap_);
    return clampf(base + bump, -torqueCap_, torqueCap_);
}

void tripRunaway(const char* why, float vel, float cmd) {
    Serial.print("SAFETY RUNAWAY: ");
    Serial.print(why);
    Serial.print(" vel=");
    Serial.print(vel, 1);
    Serial.print(" cmd=");
    Serial.println(cmd, 3);
    cmd_ = 0.0f;
    cmdFilt_ = 0.0f;
    runawaySameSignMs_ = 0;
    mode_ = Mode::Off;
    autoPid_ = false;
    MotorDriver::stop();
    Safety::gMotorWatchdog.notifyMotorDisabled();
    FfbPid::demoEnd(); // stop playing host/demo effects
}

} // namespace

void begin() {
    mode_ = Mode::Off;
    autoPid_ = false;
    manual_ = 0.0f;
    cmd_ = 0.0f;
    springK_ = FFB_SPRING_K;
    springDz_ = FFB_SPRING_DEADZONE_DEG;
    springD_ = 0.006f;
    targetCmd_ = 0.0f;
    targetSmooth_ = 0.0f;
    torqueCap_ = FFB_TORQUE_CAP;
    ffbGain_ = 1.0f;
    softEn_ = true;
    softDeg_ = 0.0f;
    softK_ = 0.012f;
    prevAxle_ = 0.0f;
    velFilt_ = 0.0f;
    accelFilt_ = 0.0f;
    cmdFilt_ = 0.0f;
    havePrevAxle_ = false;
    prevMs_ = 0;
    runawaySameSignMs_ = 0;
    FfbPid::begin();
    MotorDriver::coast();
}

void setMode(Mode m) {
    mode_ = m;
    autoPid_ = false; // explicit user/CLI mode
    if (m != Mode::Track) {
        targetCmd_ = 0.0f;
        targetSmooth_ = 0.0f;
    }
}
Mode mode() {
    return mode_;
}

void setManualTorque(float t) {
    manual_ = clampf(t, -1.0f, 1.0f);
}
float manualTorque() {
    return manual_;
}

void setSpringK(float k) {
    springK_ = k;
}
float springK() {
    return springK_;
}
void setSpringDeadzone(float deg) {
    springDz_ = deg;
}
float springDeadzone() {
    return springDz_;
}

void setTargetDeg(float deg) {
    targetCmd_ = clampf(deg, -kTargetClampDeg, kTargetClampDeg);
}
float targetDeg() {
    return targetSmooth_;
}

void setSpringD(float d) {
    springD_ = clampf(d, 0.0f, 0.05f);
}
float springD() {
    return springD_;
}

void setTorqueCap(float t) {
    torqueCap_ = t;
}
float torqueCap() {
    return torqueCap_;
}

void setFfbGain(float g) {
    ffbGain_ = clampf(g, 0.0f, 1.0f);
}
float ffbGain() {
    return ffbGain_;
}

void setSoftLimitEnabled(bool on) {
    softEn_ = on;
}
bool softLimitEnabled() {
    return softEn_;
}
void setSoftLimitDeg(float deg) {
    softDeg_ = deg < 0.0f ? 0.0f : deg;
}
float softLimitDeg() {
    return softDeg_;
}
void setSoftLimitK(float k) {
    softK_ = k < 0.0f ? 0.0f : k;
}
float softLimitK() {
    return softK_;
}

void update(float axleDegrees) {
    const uint32_t now = millis();
    float dt = 0.01f;
    float rawVel = 0.0f;
    if (havePrevAxle_) {
        dt = (now - prevMs_) * 0.001f;
        if (dt < 0.001f)
            dt = 0.001f;
        if (dt > 0.05f)
            dt = 0.05f;
        rawVel = (axleDegrees - prevAxle_) / dt;
        const float prevVel = velFilt_;
        velFilt_ += kVelFiltAlpha * (rawVel - velFilt_);
        const float rawAccel = (velFilt_ - prevVel) / dt;
        accelFilt_ += kAccelFiltAlpha * (rawAccel - accelFilt_);
    }

    // Feed USB PID mixer with normalized axis state.
    const float half = HidWheel::rangeDeg() * 0.5f;
    const float posN = (half > 1.0f) ? (axleDegrees / half) : 0.0f;
    const float velN = velFilt_ / kMaxVelNormDegPerSec;
    const float accN = accelFilt_ / kMaxAccelNormDegPerSec2;
    FfbPid::setAxisState(posN, velN, accN);

    // Auto Off ↔ Pid from host lifecycle (does not steal Manual/Spring/Track).
    if (mode_ == Mode::Off && FfbPid::autoEnterRequested()) {
        mode_ = Mode::Pid;
        autoPid_ = true;
        FfbPid::clearAutoEnter();
        Serial.print("STAGE t=");
        Serial.print(now);
        Serial.print(" ffb: auto → Pid  playing=");
        Serial.print(FfbPid::playingCount());
        Serial.print(" alloc=");
        Serial.print(FfbPid::allocatedCount());
        Serial.print(" motors=");
        Serial.println(MotorDriver::enabled() ? 1 : 0);
    }
    if (mode_ == Mode::Pid && autoPid_ && FfbPid::autoExitRequested()) {
        mode_ = Mode::Off;
        autoPid_ = false;
        FfbPid::clearAutoExit();
        cmd_ = 0.0f;
        cmdFilt_ = 0.0f;
        Serial.print("STAGE t=");
        Serial.print(now);
        Serial.println(" ffb: auto → Off");
    } else if (FfbPid::autoExitRequested() && !(mode_ == Mode::Pid && autoPid_)) {
        FfbPid::clearAutoExit();
    }
    if (FfbPid::autoEnterRequested() && mode_ != Mode::Off) {
        FfbPid::clearAutoEnter();
    }

    switch (mode_) {
    case Mode::Off:
        cmd_ = 0.0f;
        cmdFilt_ = 0.0f;
        break;
    case Mode::Manual:
        cmd_ = manual_;
        cmdFilt_ = manual_;
        break;
    case Mode::Pid: {
        float raw = FfbPid::computeTorque() * ffbGain_;
        raw = clampf(raw, -torqueCap_, torqueCap_);
        // Slew-limit host torque so a bad effect can't slam to full duty in one tick.
        const float maxStep = kPidTorqueSlewPerSec * dt;
        float delta = raw - cmdFilt_;
        if (delta > maxStep)
            delta = maxStep;
        else if (delta < -maxStep)
            delta = -maxStep;
        cmdFilt_ += delta;
        cmd_ = cmdFilt_;
        break;
    }
    case Mode::Spring:
    case Mode::Track: {
        const float goalCmd = (mode_ == Mode::Track) ? targetCmd_ : 0.0f;
        const float maxStep = kTargetSlewDegPerSec * dt;
        const float errT = goalCmd - targetSmooth_;
        if (errT > maxStep)
            targetSmooth_ += maxStep;
        else if (errT < -maxStep)
            targetSmooth_ -= maxStep;
        else
            targetSmooth_ = goalCmd;

        const float x = axleDegrees - targetSmooth_;
        float raw = 0.0f;
        if (fabsf(x) >= springDz_ || fabsf(velFilt_) >= 8.0f) {
            raw = -springK_ * x - springD_ * velFilt_;
        }
        raw = clampf(raw, -torqueCap_, torqueCap_);
        cmdFilt_ += kCmdFiltAlpha * (raw - cmdFilt_);
        cmd_ = cmdFilt_;
        break;
    }
    }

    prevAxle_ = axleDegrees;
    prevMs_ = now;
    havePrevAxle_ = true;

    cmd_ = applySoftLimit(cmd_, axleDegrees);

    // Runaway: torque accelerating further *past* the effect target (not toward it).
    // Spring→center, Track→targetSmooth_, Pid demo→spring cpOffset target.
    // Same-sign cmd×vel alone is normal when FFB is chasing a corner angle.
    if (MotorDriver::enabled() && mode_ != Mode::Off) {
        float targetDeg = 0.0f;
        if (mode_ == Mode::Track) {
            targetDeg = targetSmooth_;
        } else if (mode_ == Mode::Pid) {
            targetDeg = FfbPid::demoSpringTargetDeg();
        }
        const float err = axleDegrees - targetDeg;
        const bool hard = fabsf(velFilt_) >= kRunawayHardVelDegPerSec;
        const bool spinOut = (cmd_ * velFilt_) > 0.0f && (err * velFilt_) > 0.0f &&
                             fabsf(cmd_) >= kRunawayCmdMin &&
                             fabsf(velFilt_) >= kRunawayVelDegPerSec;
        if (hard) {
            tripRunaway("hard_vel", velFilt_, cmd_);
        } else if (spinOut) {
            runawaySameSignMs_ += (uint32_t)(dt * 1000.0f);
            if (runawaySameSignMs_ >= kRunawayHoldMs)
                tripRunaway("spin_out", velFilt_, cmd_);
        } else {
            runawaySameSignMs_ = 0;
        }
    } else {
        runawaySameSignMs_ = 0;
    }

    if (MotorDriver::enabled()) {
        MotorDriver::setBoth(cmd_);
    } else {
        cmd_ = 0.0f;
        cmdFilt_ = 0.0f;
    }
}

float commandedTorque() {
    return cmd_;
}

} // namespace Ffb
