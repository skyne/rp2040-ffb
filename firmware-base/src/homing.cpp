#include "homing.h"

#include <Arduino.h>
#include <math.h>

#include "accessory_link.h"
#include "axle_index.h"
#include "config.h"
#include "ffb.h"
#include "motor_driver.h"
#include "safety.h"
#include "settings.h"
#include "wheel_encoder.h"

namespace Homing {
namespace {

Phase phase_ = Phase::Idle;
uint32_t nearZeroSinceMs_ = 0;
uint32_t phaseStartMs_ = 0;
uint32_t lastHintMs_ = 0;
uint32_t lastAccelReqMs_ = 0;
bool useMotors_ = false;
float seekDir_ = 1.0f;
float startAxle_ = 0.0f;
float axleAtIndex_ = 0.0f; // synced axle at magnet mid (== offset ± 360n)
Ffb::Mode savedFfb_ = Ffb::Mode::Off;
uint8_t seekSweep_ = 0; // 0 = first 360° pass, 1 = reverse pass
float seekZeroStartAxle_ = 0.0f;
float to0Cmd_ = 1.0f;      // signed duty direction; flip if |axle| grows
float to0BestErr_ = -1.0f; // min |axle| since last reverse / start
uint32_t to0ReverseMs_ = 0;
uint8_t to0ReverseCount_ = 0;
float lastMotionAxle_ = 0.0f;
uint32_t lastMotionMs_ = 0;
uint32_t hallLostSinceMs_ = 0; // 0 = have fresh hall

// MeasureIndex: prime exit → clearance → enter/exit (+dir) → clearance →
// enter/exit (−dir) → average mids (cancels hall approach hysteresis).
enum class MeasureStep : uint8_t {
    PrimeExit = 0, // leave magnet after first contact
    Clearance,     // travel past magnet before reverse
    WaitEnter,     // rising edge (interpolated)
    WaitExit,      // falling edge → window mid
};
MeasureStep measureStep_ = MeasureStep::PrimeExit;
bool backingOff_ = false; // SeekIndex: leave magnet before approach
uint8_t passIndex_ = 0;
float enterAxle_ = 0.0f;
float midSum_ = 0.0f;
float clearanceFrom_ = 0.0f;
float axlePrev_ = 0.0f; // prior sample for edge interpolation
bool indexWasActive_ = false;
int16_t lastCalX_ = 0;
int16_t lastAbsCalX_ = 32767;

float clampf(float v, float lo, float hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

void drive(float cmd) {
    if (!useMotors_)
        return;
    if (!MotorDriver::enabled())
        return; // update() aborts with a clear reason
    // Creep through the magnet window; seek/to0 can run a bit faster.
    const float cap = (phase_ == Phase::MeasureIndex) ? HOME_MEASURE_DUTY : HOME_MOTOR_DUTY;
    MotorDriver::setBoth(clampf(cmd, -cap, cap));
}

void stopDrive() {
    if (useMotors_)
        MotorDriver::coast();
}

void finishOk() {
    stopDrive();
    const float held = WheelEncoder::axleDegrees();
    // Only snap when we actually made the band — never hide a failed to0.
    if (fabsf(held) <= HOME_ZERO_TOLERANCE_DEG * 2.0f)
        WheelEncoder::setAxleDegrees(0.0f);
    if (MotorDriver::enabled()) {
        MotorDriver::stop();
        Safety::gMotorWatchdog.notifyMotorDisabled();
    }
    useMotors_ = false;
    phase_ = Phase::Idle;
    nearZeroSinceMs_ = 0;
    Ffb::setMode(savedFfb_);
    Serial.print("INIT done — held_axle=");
    Serial.print(held, 1);
    Serial.print(fabsf(held) <= HOME_ZERO_TOLERANCE_DEG * 2.0f ? " → snapped 0;" : " (no snap);");
    Serial.println(" motors DISARMED (press e to arm)");
}

void abortHome(const char* why) {
    stopDrive();
    if (MotorDriver::enabled()) {
        MotorDriver::stop();
        Safety::gMotorWatchdog.notifyMotorDisabled();
    }
    useMotors_ = false;
    phase_ = Phase::Idle;
    nearZeroSinceMs_ = 0;
    Ffb::setMode(savedFfb_);
    Serial.print("INIT aborted: ");
    Serial.println(why);
}

void printPrompt() {
    const char* how = useMotors_ ? "motors" : "hand";
    switch (phase_) {
    case Phase::Idle:
        break;
    case Phase::ProbeAdxl:
        Serial.print("INIT 0/3 [");
        Serial.print(how);
        Serial.println("]: probing rim ADXL…");
        break;
    case Phase::SeekGravity:
        Serial.print("INIT gravity [");
        Serial.print(how);
        Serial.println("]: turn until ADXL X ≈ center (n cancels)");
        break;
    case Phase::SeekIndex:
        Serial.print("INIT 1/3 [");
        Serial.print(how);
        Serial.println(backingOff_ ? "]: turn OFF index magnet" : "]: turn until INDEX");
        break;
    case Phase::MeasureIndex:
        Serial.print("INIT 2/3 [");
        Serial.print(how);
        Serial.print("]: window pass ");
        Serial.print(passIndex_ + 1);
        Serial.print('/');
        Serial.print(HOME_INDEX_WINDOW_PASSES);
        if (measureStep_ == MeasureStep::PrimeExit)
            Serial.println(" — prime exit");
        else if (measureStep_ == MeasureStep::Clearance)
            Serial.println(" — clearance");
        else if (measureStep_ == MeasureStep::WaitEnter)
            Serial.println(" — enter magnet");
        else
            Serial.println(" — exit magnet");
        break;
    case Phase::SeekZero:
        Serial.print("INIT 3/3 [");
        Serial.print(how);
        Serial.print("]: motor to virtual 0 (");
        Serial.print(-axleAtIndex_, 1);
        Serial.println(" deg from index mid)");
        break;
    }
}

bool timedOut() {
    if (!useMotors_ || HOME_MOTOR_TIMEOUT_MS == 0)
        return false;
    return (millis() - phaseStartMs_ > HOME_MOTOR_TIMEOUT_MS);
}

void noteMotion(float axleDeg) {
    if (fabsf(axleDeg - lastMotionAxle_) >= HOME_MOTION_STALL_DEG) {
        lastMotionAxle_ = axleDeg;
        lastMotionMs_ = millis();
    }
}

bool motionStalled() {
    if (!useMotors_ || HOME_MOTION_STALL_MS == 0)
        return false;
    // Measure pass is intentional creep over the magnet — don't false-abort.
    if (phase_ == Phase::MeasureIndex)
        return false;
    // Holding / dithering near virtual zero is not a stall.
    // Also ignore stall through most of to0 — low residual error + stiction
    // used to false-abort ~20° out while PWM was still commanded.
    if (phase_ == Phase::SeekZero)
        return false;
    return (millis() - lastMotionMs_) > HOME_MOTION_STALL_MS;
}

void armMotionWatch(float axleDeg) {
    lastMotionAxle_ = axleDeg;
    lastMotionMs_ = millis();
}

bool seekTravelExhausted(float axleDeg) {
    return fabsf(axleDeg - startAxle_) >= HOME_SEEK_TRAVEL_DEG;
}

/** Motors commanded but encoder axle not advancing — MLX/gear/SPI issue. */
bool encoderNotAdvancing(float axleDeg) {
    if (!useMotors_ || HOME_ENCODER_PROGRESS_MS == 0)
        return false;
    if (phase_ != Phase::SeekIndex && phase_ != Phase::SeekZero)
        return false;
    if (phase_ == Phase::SeekZero && fabsf(axleDeg) <= HOME_ZERO_TOLERANCE_DEG * 4.0f)
        return false;
    const float ref = (phase_ == Phase::SeekZero) ? seekZeroStartAxle_ : startAxle_;
    if (fabsf(axleDeg - ref) >= HOME_ENCODER_PROGRESS_DEG)
        return false;
    return (millis() - phaseStartMs_) >= HOME_ENCODER_PROGRESS_MS;
}

void reverseIndexSeek(float axleDeg, const char* why) {
    seekDir_ = -seekDir_;
    startAxle_ = axleDeg;
    seekSweep_++;
    phaseStartMs_ = millis();
    Serial.print("INIT: ");
    Serial.print(why);
    Serial.print(" — reverse seek (sweep ");
    Serial.print(seekSweep_ + 1);
    Serial.println(")");
    printPrompt();
    drive(seekDir_ * HOME_MOTOR_DUTY);
}

int16_t adxlOffset() {
    const Settings::Data& d = Settings::cdata();
    return d.adxlCalValid ? d.adxlXOffset : (int16_t)0;
}

void beginMagnetPath(const char* why) {
    Serial.print("INIT: ");
    Serial.print(why);
    Serial.println(" — magnet index fallback");

    startAxle_ = WheelEncoder::axleDegrees();
    seekDir_ = HOME_MOTOR_DIR;
    seekSweep_ = 0;
    passIndex_ = 0;
    midSum_ = 0.0f;
    measureStep_ = MeasureStep::PrimeExit;
    indexWasActive_ = AxleIndex::active();
    backingOff_ = indexWasActive_;
    if (backingOff_) {
        seekDir_ = -HOME_MOTOR_DIR;
    }

    phase_ = Phase::SeekIndex;
    nearZeroSinceMs_ = 0;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;
    armMotionWatch(startAxle_);
    printPrompt();
    drive(seekDir_ * HOME_MOTOR_DUTY);
}

void beginGravitySeek() {
    phase_ = Phase::SeekGravity;
    nearZeroSinceMs_ = 0;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;
    lastAccelReqMs_ = 0;
    lastAbsCalX_ = 32767;
    lastCalX_ = 0;
    Serial.print("INIT: ADXL gravity zero (offset=");
    Serial.print(adxlOffset());
    Serial.print(Settings::cdata().adxlCalValid ? " cal" : " raw");
    Serial.println(")");
    printPrompt();
    AccessoryLink::requestAccel(FfbLink::AccelOnce, 0);
}

// First contact: keep going until magnet falls (exit), then reverse for a clean
// enter→exit window. Midpoint of that window is the index center.
void beginMeasureAfterContact(float axleDeg) {
    phase_ = Phase::MeasureIndex;
    measureStep_ = MeasureStep::PrimeExit;
    passIndex_ = 0;
    midSum_ = 0.0f;
    indexWasActive_ = true;
    axlePrev_ = axleDeg;
    startAxle_ = axleDeg;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;
    armMotionWatch(axleDeg);
    Serial.print("INIT: index contact axle=");
    Serial.print(axleDeg, 1);
    Serial.println(" — prime exit, then ± window average");
    printPrompt();
    drive(seekDir_ * HOME_MOTOR_DUTY);
}

void beginClearance(float axleDeg, const char* why) {
    measureStep_ = MeasureStep::Clearance;
    clearanceFrom_ = axleDeg;
    startAxle_ = axleDeg;
    phaseStartMs_ = millis();
    armMotionWatch(axleDeg);
    Serial.print("INIT: ");
    Serial.print(why);
    Serial.print(" — clearance ");
    Serial.print(HOME_INDEX_CLEARANCE_DEG, 0);
    Serial.println(" deg before reverse");
    printPrompt();
    drive(seekDir_ * HOME_MOTOR_DUTY);
}

void afterClearanceReverse(float axleDeg) {
    seekDir_ = -seekDir_;
    measureStep_ = MeasureStep::WaitEnter;
    startAxle_ = axleDeg;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;
    armMotionWatch(axleDeg);
    Serial.print("INIT: reverse for pass ");
    Serial.print(passIndex_ + 1);
    Serial.print("  dir=");
    Serial.println(seekDir_ > 0.0f ? "+" : "-");
    printPrompt();
    drive(seekDir_ * HOME_MOTOR_DUTY);
}

// Sync so magnet mid == AXLE_INDEX_ANGLE_DEG, then motor to virtual axle=0.
// Zero is encoder math only — no sensor at straight-ahead.
void finishMeasureAndSeekZero(float centerAxle) {
    stopDrive();
    WheelEncoder::syncMeasuredIndex(centerAxle, AXLE_INDEX_ANGLE_DEG);
    const float n = roundf((centerAxle - AXLE_INDEX_ANGLE_DEG) / 360.0f);
    axleAtIndex_ = AXLE_INDEX_ANGLE_DEG + n * 360.0f;

    // Pick an initial spin direction; reverse if |axle| grows (motor/encoder sign unknown).
    to0Cmd_ = (WheelEncoder::axleDegrees() > 0.0f) ? -1.0f : 1.0f;
    to0BestErr_ = fabsf(WheelEncoder::axleDegrees());
    to0ReverseMs_ = 0;
    to0ReverseCount_ = 0;
    phase_ = Phase::SeekZero;
    nearZeroSinceMs_ = 0;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;
    seekZeroStartAxle_ = WheelEncoder::axleDegrees();
    armMotionWatch(seekZeroStartAxle_);

    Serial.print("INIT: index mid  raw=");
    Serial.print(centerAxle, 2);
    Serial.print("  synced=");
    Serial.print(axleAtIndex_, 2);
    Serial.print("  (cfg offset=");
    Serial.print(AXLE_INDEX_ANGLE_DEG, 1);
    Serial.print(")  passes=");
    Serial.print(HOME_INDEX_WINDOW_PASSES);
    Serial.print("  motor ");
    Serial.print(-seekZeroStartAxle_, 1);
    Serial.println(" deg to virtual 0");
    printPrompt();

    if (fabsf(seekZeroStartAxle_) <= HOME_ZERO_TOLERANCE_DEG) {
        finishOk();
        return;
    }
    drive(to0Cmd_ * HOME_MOTOR_DUTY);
}

void finishGravityZero(int16_t calX) {
    stopDrive();
    const float before = WheelEncoder::axleDegrees();
    WheelEncoder::zeroHere();
    Serial.print("INIT: ADXL center calX=");
    Serial.print(calX);
    Serial.print("  axle ");
    Serial.print(before, 1);
    Serial.println(" → 0");
    finishOk();
}

} // namespace

void begin() {
    phase_ = Phase::Idle;
    nearZeroSinceMs_ = 0;
    useMotors_ = false;
}

void armMotorsForInit(bool preferMotors) {
    const bool wantMotors = preferMotors || HOME_BOOT_USE_MOTORS || MotorDriver::enabled();
    if (!wantMotors) {
        useMotors_ = false;
        return;
    }

    if (MotorDriver::faultActive())
        MotorDriver::clearFault();
    MotorDriver::setEnabled(true);
    if (MotorDriver::faultActive()) {
        MotorDriver::stop();
        useMotors_ = false;
        Serial.println("INIT: driver fault after arm — falling back to hand");
        return;
    }
    Safety::gMotorWatchdog.notifyMotorEnabled();
    useMotors_ = true;
}

void startInternal(bool preferMotors) {
    if (phase_ != Phase::Idle)
        return;

    savedFfb_ = Ffb::mode();
    Ffb::setMode(Ffb::Mode::Off);

    armMotorsForInit(preferMotors);

    startAxle_ = WheelEncoder::axleDegrees();
    nearZeroSinceMs_ = 0;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;
    lastAccelReqMs_ = 0;

    Serial.println();
    Serial.print(useMotors_ ? "INIT (motors)" : "INIT (hand)");
    Serial.print(" — saved axle=");
    Serial.print(startAxle_, 1);
    Serial.println("  n cancels");

    if (HOME_USE_ADXL) {
        phase_ = Phase::ProbeAdxl;
        printPrompt();
        AccessoryLink::requestAccel(FfbLink::AccelOnce, 0);
        return;
    }

    beginMagnetPath("ADXL disabled");
}

void start() {
    startInternal(false);
}

void startMotorized() {
    startInternal(true);
}

void startOrCancel() {
    if (phase_ != Phase::Idle) {
        abortHome("cancelled");
        return;
    }
    // Explicit `n` / `:home` — try motorized seek (falls back to hand on fault).
    startMotorized();
}

bool active() {
    return phase_ != Phase::Idle;
}
Phase phase() {
    return phase_;
}
bool usingMotors() {
    return useMotors_ && active();
}

const char* phaseName() {
    switch (phase_) {
    case Phase::Idle:
        return "idle";
    case Phase::ProbeAdxl:
        return "adxl";
    case Phase::SeekGravity:
        return "grav";
    case Phase::SeekIndex:
        return backingOff_ ? "boff" : "idx";
    case Phase::MeasureIndex:
        if (measureStep_ == MeasureStep::PrimeExit)
            return "pexit";
        if (measureStep_ == MeasureStep::Clearance)
            return "clr";
        return (measureStep_ == MeasureStep::WaitEnter) ? "ent" : "exit";
    case Phase::SeekZero:
        return "to0";
    }
    return "?";
}

void update(bool indexEdge, float axleDeg, bool axleFresh) {
    if (phase_ == Phase::Idle)
        return;

    if (useMotors_ && !MotorDriver::enabled()) {
        abortHome("driver disabled/fault during INIT");
        return;
    }

    if (phase_ != Phase::ProbeAdxl && timedOut()) {
        if (useMotors_) {
            stopDrive();
            if (MotorDriver::enabled()) {
                MotorDriver::stop();
                Safety::gMotorWatchdog.notifyMotorDisabled();
            }
            useMotors_ = false;
            phaseStartMs_ = millis();
            armMotionWatch(axleDeg);
            Serial.println("INIT: motor timeout — continue by hand (n cancels)");
            printPrompt();
            return;
        }
        abortHome("timeout");
        return;
    }

    const bool idxActive = AxleIndex::active();
    // Hall dropouts freeze the encoder reading — don't treat that as a mechanical stall,
    // but do abort if samples stay stale while motors are driving.
    if (axleFresh) {
        hallLostSinceMs_ = 0;
        noteMotion(axleDeg);
    } else if (useMotors_) {
        if (hallLostSinceMs_ == 0)
            hallLostSinceMs_ = millis();
        if ((millis() - hallLostSinceMs_) >= HOME_HALL_LOSS_MS) {
            abortHome("hall lost during INIT (check MLX SPI)");
            return;
        }
    } else {
        hallLostSinceMs_ = 0;
    }

    if (phase_ != Phase::ProbeAdxl && motionStalled()) {
        abortHome("no axle motion (check hall / gear_ratio)");
        return;
    }

    if (encoderNotAdvancing(axleDeg)) {
        abortHome("encoder not advancing (MLX stuck / gear_ratio / motor sign)");
        return;
    }

    // Live seek telemetry so a one-way spin is diagnosable without :log.
    if (useMotors_ && phase_ == Phase::SeekIndex && (millis() - lastHintMs_) >= 2000u) {
        lastHintMs_ = millis();
        Serial.print("INIT seek ");
        Serial.print(backingOff_ ? "boff" : "idx");
        Serial.print(" axle=");
        Serial.print(axleDeg, 1);
        Serial.print(" d=");
        Serial.print(axleDeg - startAxle_, 1);
        Serial.print(" dir=");
        Serial.print(seekDir_ > 0.0f ? "+" : "-");
        Serial.print(" idx=");
        Serial.print(idxActive ? 1 : 0);
        Serial.print(" hall=");
        Serial.println(axleFresh ? 1 : 0);
    }

    switch (phase_) {
    case Phase::ProbeAdxl: {
        const uint32_t elapsed = millis() - phaseStartMs_;
        const auto& rep = AccessoryLink::lastAccel();
        if (AccessoryLink::lastAccelMs() >= phaseStartMs_) {
            if (rep.present && rep.ok) {
                beginGravitySeek();
                break;
            }
            if (!rep.present) {
                beginMagnetPath("no ADXL on rim");
                break;
            }
        }
        if (AccessoryLink::adxlPresent() && elapsed > 50) {
            beginGravitySeek();
            break;
        }
        if (elapsed >= HOME_ADXL_PROBE_MS) {
            if (!AccessoryLink::linked()) {
                beginMagnetPath("rim unlink / no ADXL");
            } else {
                beginMagnetPath("no ADXL on rim");
            }
        } else if (elapsed > 100 && (millis() - lastAccelReqMs_) > 120) {
            lastAccelReqMs_ = millis();
            AccessoryLink::requestAccel(FfbLink::AccelOnce, 0);
        }
        break;
    }

    case Phase::SeekGravity: {
        if ((millis() - lastAccelReqMs_) >= HOME_ADXL_POLL_MS) {
            lastAccelReqMs_ = millis();
            AccessoryLink::requestAccel(FfbLink::AccelOnce, 0);
        }

        const auto& rep = AccessoryLink::lastAccel();
        if (!rep.present) {
            if ((millis() - phaseStartMs_) > 1000 && !AccessoryLink::adxlPresent()) {
                beginMagnetPath("ADXL lost");
            }
            break;
        }
        if (!rep.ok)
            break;

        const int16_t calX = (int16_t)(rep.ax - adxlOffset());
        lastCalX_ = calX;
        const int16_t absCal = (int16_t)(calX < 0 ? -calX : calX);

        if (absCal <= HOME_ADXL_TOLERANCE_RAW) {
            stopDrive();
            if (nearZeroSinceMs_ == 0)
                nearZeroSinceMs_ = millis();
            if (millis() - nearZeroSinceMs_ >= HOME_ADXL_HOLD_MS) {
                finishGravityZero(calX);
            }
            break;
        }

        nearZeroSinceMs_ = 0;

        float dir = (calX > 0) ? -HOME_ADXL_DIR : HOME_ADXL_DIR;
        if (absCal > lastAbsCalX_ + 8) {
            dir = -dir;
        }
        lastAbsCalX_ = absCal;
        seekDir_ = dir;

        if (useMotors_) {
            float mag = HOME_MOTOR_DUTY;
            if (absCal < 80) {
                mag *= (0.25f + 0.75f * (float)absCal / 80.0f);
            }
            if (mag < 0.05f)
                mag = 0.05f;
            drive(dir * mag);
        } else if (millis() - lastHintMs_ > 1500) {
            lastHintMs_ = millis();
            Serial.print("INIT gravity: calX=");
            Serial.print(calX);
            Serial.print("  turn ");
            Serial.println(dir > 0.0f ? "positive" : "negative");
        }
        break;
    }

    case Phase::SeekIndex:
        drive(seekDir_ * HOME_MOTOR_DUTY);

        if (backingOff_) {
            if (!idxActive) {
                backingOff_ = false;
                seekDir_ = HOME_MOTOR_DIR;
                startAxle_ = axleDeg;
                seekSweep_ = 0;
                indexWasActive_ = false;
                phaseStartMs_ = millis();
                Serial.println("INIT: clear of magnet — approaching");
                printPrompt();
                drive(seekDir_ * HOME_MOTOR_DUTY);
            } else if (useMotors_ && seekTravelExhausted(axleDeg)) {
                reverseIndexSeek(axleDeg, "still on magnet after travel");
            }
            break;
        }

        if (indexEdge) {
            beginMeasureAfterContact(axleDeg);
            break;
        }

        if (useMotors_ && seekTravelExhausted(axleDeg)) {
            if (seekSweep_ == 0) {
                reverseIndexSeek(axleDeg, "no index in first turn");
            } else {
                abortHome("no index after ±360 deg");
            }
        }
        break;

    case Phase::MeasureIndex: {
        drive(seekDir_ * HOME_MOTOR_DUTY);

        const bool rose = idxActive && !indexWasActive_;
        const bool fell = !idxActive && indexWasActive_;
        indexWasActive_ = idxActive;

        switch (measureStep_) {
        case MeasureStep::PrimeExit:
            if (fell || !idxActive) {
                beginClearance(axleDeg, "primed exit");
            } else if (useMotors_ && seekTravelExhausted(axleDeg)) {
                Serial.println("INIT: no magnet exit on prime — clearance anyway");
                beginClearance(axleDeg, "prime timeout");
            } else if (!useMotors_ && millis() - lastHintMs_ > 1500) {
                lastHintMs_ = millis();
                Serial.println("INIT measure: keep turning until magnet OFF");
            }
            break;

        case MeasureStep::Clearance:
            if (fabsf(axleDeg - clearanceFrom_) >= HOME_INDEX_CLEARANCE_DEG) {
                afterClearanceReverse(axleDeg);
            } else if (useMotors_ && seekTravelExhausted(axleDeg)) {
                afterClearanceReverse(axleDeg);
            }
            break;

        case MeasureStep::WaitEnter:
            if (rose) {
                // Interpolate edge between samples (reduces one-tick lag bias).
                enterAxle_ = 0.5f * (axlePrev_ + axleDeg);
                measureStep_ = MeasureStep::WaitExit;
                phaseStartMs_ = millis();
                Serial.print("INIT: pass ");
                Serial.print(passIndex_ + 1);
                Serial.print(" dir=");
                Serial.print(seekDir_ > 0.0f ? "+" : "-");
                Serial.print(" enter=");
                Serial.println(enterAxle_, 2);
                printPrompt();
            } else if (useMotors_ && seekTravelExhausted(axleDeg)) {
                abortHome("no magnet enter on measure pass");
            } else if (!useMotors_ && millis() - lastHintMs_ > 1500) {
                lastHintMs_ = millis();
                Serial.println("INIT measure: keep turning until magnet ON");
            }
            break;

        case MeasureStep::WaitExit:
            if (fell) {
                const float exitAxle = 0.5f * (axlePrev_ + axleDeg);
                const float width = fabsf(exitAxle - enterAxle_);
                if (width < HOME_INDEX_WIDTH_MIN_DEG) {
                    Serial.println("INIT: ignore tiny index blip");
                    measureStep_ = MeasureStep::WaitEnter;
                    phaseStartMs_ = millis();
                    break;
                }
                if (width > HOME_INDEX_WIDTH_MAX_DEG) {
                    Serial.print("INIT: ignore huge window width=");
                    Serial.println(width, 1);
                    measureStep_ = MeasureStep::WaitEnter;
                    phaseStartMs_ = millis();
                    break;
                }
                const float mid = 0.5f * (enterAxle_ + exitAxle);
                midSum_ += mid;
                passIndex_++;

                Serial.print("INIT: pass ");
                Serial.print(passIndex_);
                Serial.print(" dir=");
                Serial.print(seekDir_ > 0.0f ? "+" : "-");
                Serial.print(" exit=");
                Serial.print(exitAxle, 2);
                Serial.print("  mid=");
                Serial.print(mid, 2);
                Serial.print("  width=");
                Serial.println(width, 2);

                if (passIndex_ >= HOME_INDEX_WINDOW_PASSES) {
                    const float avg = midSum_ / (float)passIndex_;
                    Serial.print("INIT: window avg mid=");
                    Serial.print(avg, 2);
                    Serial.print("  (");
                    Serial.print(passIndex_);
                    Serial.println(" dirs)");
                    finishMeasureAndSeekZero(avg);
                    break;
                }

                // Same physical direction until cleared, then reverse for −dir pass.
                beginClearance(axleDeg, "pass exit");
            } else if (useMotors_ && seekTravelExhausted(axleDeg)) {
                abortHome("no magnet exit on measure pass");
            } else if (!useMotors_ && millis() - lastHintMs_ > 1500) {
                lastHintMs_ = millis();
                Serial.println("INIT measure: keep turning until magnet OFF");
            }
            break;
        }

        axlePrev_ = axleDeg;
        break;
    }

    case Phase::SeekZero: {
        // Target: encoder axle≈0 (virtual straight-ahead). Closed-loop on
        // axleDegrees — MOTOR_OUTPUT_SIGN already maps logical cmd → wheel.
        // Bang-bang "reverse if |err| grows" false-triggered on soft approach
        // / backlash and aborted INIT after a few flips.
        const float err = fabsf(axleDeg);

        if (err <= HOME_ZERO_TOLERANCE_DEG) {
            stopDrive();
            lastMotionMs_ = millis(); // holding in band is not a stall
            if (nearZeroSinceMs_ == 0)
                nearZeroSinceMs_ = millis();
            if (millis() - nearZeroSinceMs_ >= HOME_ZERO_HOLD_MS) {
                finishOk();
            }
            break;
        }

        nearZeroSinceMs_ = 0;

        if (useMotors_) {
            if (fabsf(axleDeg - seekZeroStartAxle_) >= HOME_SEEK_ZERO_TRAVEL_DEG) {
                abortHome("seek-zero travel exceeded (sync/index suspect)");
                break;
            }

            // Keep nearly full seek duty until inside a few degrees — proportional
            // /45 * duty was dying around ~20° (stiction > ~0.1 cmd on this gear).
            float mag = HOME_MOTOR_DUTY;
            if (err < 12.0f) {
                mag = HOME_MOTOR_DUTY * (0.60f + 0.40f * (err / 12.0f));
            }
            const float floor = HOME_MOTOR_DUTY * 0.60f;
            if (mag < floor)
                mag = floor;

            const float cmd = (axleDeg > 0.0f) ? -mag : mag;
            seekDir_ = (cmd >= 0.0f) ? 1.0f : -1.0f;
            to0Cmd_ = seekDir_;
            drive(cmd);

            if ((millis() - lastHintMs_) >= 1500u) {
                lastHintMs_ = millis();
                Serial.print("INIT to0: axle=");
                Serial.print(axleDeg, 1);
                Serial.print(" err=");
                Serial.print(err, 1);
                Serial.print(" cmd=");
                Serial.println(cmd, 3);
            }
        } else if (millis() - lastHintMs_ > 1500) {
            lastHintMs_ = millis();
            Serial.print("INIT to0: axle=");
            Serial.print(axleDeg, 1);
            Serial.print("  |err|=");
            Serial.print(err, 1);
            Serial.println("  turn toward 0");
        }
        break;
    }

    case Phase::Idle:
        break;
    }
}

} // namespace Homing
