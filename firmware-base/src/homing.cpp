#include "homing.h"

#include <Arduino.h>
#include <math.h>

#include "accessory_link.h"
#include "axle_index.h"
#include "config.h"
#include "ffb.h"
#include "motor_bts7960.h"
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
float axleAtIndex_ = 0.0f;  // synced axle at magnet mid (== offset ± 360n)
Ffb::Mode savedFfb_ = Ffb::Mode::Off;

// MeasureIndex state
bool backingOff_ = false;       // SeekIndex: leave magnet before approach
bool waitingEnter_ = true;      // MeasureIndex: true=wait rise, false=wait fall
uint8_t passIndex_ = 0;
float enterAxle_ = 0.0f;
float midSum_ = 0.0f;
bool indexWasActive_ = false;   // edge detect from AxleIndex::active()
int16_t lastCalX_ = 0;
int16_t lastAbsCalX_ = 32767;

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float dutyForPhase() {
    if (phase_ == Phase::MeasureIndex) return HOME_MEASURE_DUTY;
    return HOME_MOTOR_DUTY;
}

void drive(float cmd) {
    if (!useMotors_) return;
    if (!MotorBts7960::enabled()) return;
    const float cap = dutyForPhase();
    MotorBts7960::setBoth(clampf(cmd, -cap, cap));
}

void stopDrive() {
    if (useMotors_) MotorBts7960::coast();
}

void finishOk() {
    stopDrive();
    phase_ = Phase::Idle;
    nearZeroSinceMs_ = 0;
    Ffb::setMode(savedFfb_);
    Serial.print("INIT done — axle=");
    Serial.println(WheelEncoder::axleDegrees(), 1);
}

void abortHome(const char *why) {
    stopDrive();
    phase_ = Phase::Idle;
    nearZeroSinceMs_ = 0;
    Ffb::setMode(savedFfb_);
    Serial.print("INIT aborted: ");
    Serial.println(why);
}

void printPrompt() {
    const char *how = useMotors_ ? "motors" : "hand";
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
            Serial.print("]: slow window pass ");
            Serial.print(passIndex_ + 1);
            Serial.print('/');
            Serial.print(HOME_INDEX_WINDOW_PASSES);
            Serial.println(waitingEnter_ ? " — enter magnet" : " — exit magnet");
            break;
        case Phase::SeekZero:
            Serial.print("INIT 3/3 [");
            Serial.print(how);
            Serial.print("]: turn toward 0 (delta ");
            Serial.print(-axleAtIndex_, 1);
            Serial.println(" deg from index mid)");
            break;
    }
}

bool timedOut() {
    return useMotors_ && (millis() - phaseStartMs_ > HOME_MOTOR_TIMEOUT_MS);
}

int16_t adxlOffset() {
    const Settings::Data &d = Settings::cdata();
    return d.adxlCalValid ? d.adxlXOffset : (int16_t)0;
}

void beginMagnetPath(const char *why) {
    Serial.print("INIT: ");
    Serial.print(why);
    Serial.println(" — magnet index fallback");

    startAxle_ = WheelEncoder::axleDegrees();
    seekDir_ = HOME_MOTOR_DIR;
    passIndex_ = 0;
    midSum_ = 0.0f;
    waitingEnter_ = true;
    indexWasActive_ = AxleIndex::active();
    backingOff_ = indexWasActive_;
    if (backingOff_) {
        seekDir_ = -HOME_MOTOR_DIR;
    }

    phase_ = Phase::SeekIndex;
    nearZeroSinceMs_ = 0;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;
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

void beginMeasureFromEnter(float enterAxle) {
    phase_ = Phase::MeasureIndex;
    waitingEnter_ = false;
    enterAxle_ = enterAxle;
    passIndex_ = 0;
    midSum_ = 0.0f;
    indexWasActive_ = true;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;
    Serial.print("INIT: index enter  axle=");
    Serial.print(enterAxle_, 1);
    Serial.println(" — measuring window");
    printPrompt();
    drive(seekDir_ * HOME_MEASURE_DUTY);
}

void finishMeasureAndSeekZero(float centerAxle) {
    stopDrive();
    WheelEncoder::syncMeasuredIndex(centerAxle, AXLE_INDEX_ANGLE_DEG);
    const float n = roundf((centerAxle - AXLE_INDEX_ANGLE_DEG) / 360.0f);
    axleAtIndex_ = AXLE_INDEX_ANGLE_DEG + n * 360.0f;

    seekDir_ = (axleAtIndex_ > 0.0f) ? -1.0f : 1.0f;

    phase_ = Phase::SeekZero;
    nearZeroSinceMs_ = 0;
    phaseStartMs_ = millis();
    lastHintMs_ = 0;

    Serial.print("INIT: index mid  raw=");
    Serial.print(centerAxle, 2);
    Serial.print("  synced=");
    Serial.print(axleAtIndex_, 2);
    Serial.print("  (cfg=");
    Serial.print(AXLE_INDEX_ANGLE_DEG, 1);
    Serial.print(")  passes=");
    Serial.print(HOME_INDEX_WINDOW_PASSES);
    Serial.print("  true 0 is ");
    Serial.print(-axleAtIndex_, 1);
    Serial.println(" deg from mid — seeking 0");
    printPrompt();
    drive(seekDir_ * HOME_MOTOR_DUTY);
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

}  // namespace

void begin() {
    phase_ = Phase::Idle;
    nearZeroSinceMs_ = 0;
    useMotors_ = false;
}

void start() {
    if (phase_ != Phase::Idle) return;

    savedFfb_ = Ffb::mode();
    Ffb::setMode(Ffb::Mode::Off);

    if (HOME_BOOT_USE_MOTORS || MotorBts7960::enabled()) {
        MotorBts7960::setEnabled(true);
        useMotors_ = true;
    } else {
        useMotors_ = false;
    }

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

void startOrCancel() {
    if (phase_ != Phase::Idle) {
        abortHome("cancelled");
        return;
    }
    start();
}

bool active() { return phase_ != Phase::Idle; }
Phase phase() { return phase_; }
bool usingMotors() { return useMotors_ && active(); }

const char *phaseName() {
    switch (phase_) {
        case Phase::Idle: return "idle";
        case Phase::ProbeAdxl: return "adxl";
        case Phase::SeekGravity: return "grav";
        case Phase::SeekIndex: return backingOff_ ? "boff" : "idx";
        case Phase::MeasureIndex: return waitingEnter_ ? "ent" : "exit";
        case Phase::SeekZero: return "to0";
    }
    return "?";
}

void update(bool indexEdge, float axleDeg) {
    if (phase_ == Phase::Idle) return;

    if (phase_ != Phase::ProbeAdxl && timedOut()) {
        if (useMotors_) {
            stopDrive();
            useMotors_ = false;
            phaseStartMs_ = millis();
            Serial.println("INIT: motor timeout — continue by hand");
            printPrompt();
            return;
        }
        abortHome("timeout");
        return;
    }

    const bool idxActive = AxleIndex::active();

    switch (phase_) {
        case Phase::ProbeAdxl: {
            const uint32_t elapsed = millis() - phaseStartMs_;
            const auto &rep = AccessoryLink::lastAccel();
            // Fresh report after our probe request?
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
            // Input flag can arrive before AccelReport.
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

            const auto &rep = AccessoryLink::lastAccel();
            if (!rep.present) {
                // Chip disappeared mid-home → magnet fallback.
                if ((millis() - phaseStartMs_) > 1000 && !AccessoryLink::adxlPresent()) {
                    beginMagnetPath("ADXL lost");
                }
                break;
            }
            if (!rep.ok) break;

            const int16_t calX = (int16_t)(rep.ax - adxlOffset());
            lastCalX_ = calX;
            const int16_t absCal = (int16_t)(calX < 0 ? -calX : calX);

            if (absCal <= HOME_ADXL_TOLERANCE_RAW) {
                stopDrive();
                if (nearZeroSinceMs_ == 0) nearZeroSinceMs_ = millis();
                if (millis() - nearZeroSinceMs_ >= HOME_ADXL_HOLD_MS) {
                    finishGravityZero(calX);
                }
                break;
            }

            nearZeroSinceMs_ = 0;

            // If error grew while driving one way, reverse (mount-agnostic).
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
                if (mag < 0.05f) mag = 0.05f;
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
                    indexWasActive_ = false;
                    phaseStartMs_ = millis();
                    Serial.println("INIT: clear of magnet — approaching");
                    printPrompt();
                    drive(seekDir_ * HOME_MOTOR_DUTY);
                }
                break;
            }

            if (indexEdge) {
                beginMeasureFromEnter(axleDeg);
            }
            break;

        case Phase::MeasureIndex: {
            drive(seekDir_ * HOME_MEASURE_DUTY);

            const bool rose = idxActive && !indexWasActive_;
            const bool fell = !idxActive && indexWasActive_;
            indexWasActive_ = idxActive;

            if (waitingEnter_) {
                if (rose) {
                    enterAxle_ = axleDeg;
                    waitingEnter_ = false;
                    phaseStartMs_ = millis();
                    Serial.print("INIT: pass ");
                    Serial.print(passIndex_ + 1);
                    Serial.print(" enter=");
                    Serial.println(enterAxle_, 2);
                    printPrompt();
                } else if (!useMotors_ && millis() - lastHintMs_ > 1500) {
                    lastHintMs_ = millis();
                    Serial.println("INIT measure: keep turning until magnet ON");
                }
                break;
            }

            // Waiting for exit (falling edge)
            if (fell) {
                const float exitAxle = axleDeg;
                const float mid = 0.5f * (enterAxle_ + exitAxle);
                midSum_ += mid;
                passIndex_++;

                Serial.print("INIT: pass ");
                Serial.print(passIndex_);
                Serial.print(" exit=");
                Serial.print(exitAxle, 2);
                Serial.print("  mid=");
                Serial.print(mid, 2);
                Serial.print("  width=");
                Serial.println(fabsf(exitAxle - enterAxle_), 2);

                if (passIndex_ >= HOME_INDEX_WINDOW_PASSES) {
                    finishMeasureAndSeekZero(midSum_ / (float)HOME_INDEX_WINDOW_PASSES);
                    break;
                }

                // Reverse for next pass (approaches from other side)
                seekDir_ = -seekDir_;
                waitingEnter_ = true;
                phaseStartMs_ = millis();
                lastHintMs_ = 0;
                Serial.print("INIT: reverse for pass ");
                Serial.println(passIndex_ + 1);
                printPrompt();
                drive(seekDir_ * HOME_MEASURE_DUTY);
            } else if (!useMotors_ && millis() - lastHintMs_ > 1500) {
                lastHintMs_ = millis();
                Serial.println("INIT measure: keep turning until magnet OFF");
            }
            break;
        }

        case Phase::SeekZero: {
            if (fabsf(axleDeg) <= HOME_ZERO_TOLERANCE_DEG && !AxleIndex::active()) {
                stopDrive();
                if (nearZeroSinceMs_ == 0) nearZeroSinceMs_ = millis();
                if (millis() - nearZeroSinceMs_ >= HOME_ZERO_HOLD_MS) {
                    finishOk();
                }
                break;
            }

            nearZeroSinceMs_ = 0;

            const float dir = (axleDeg > 0.0f) ? -1.0f : 1.0f;
            seekDir_ = dir;

            if (useMotors_) {
                float mag = HOME_MOTOR_DUTY;
                if (fabsf(axleDeg) < 40.0f) {
                    mag *= (0.25f + 0.75f * fabsf(axleDeg) / 40.0f);
                }
                if (mag < 0.05f) mag = 0.05f;
                drive(dir * mag);
            } else if (millis() - lastHintMs_ > 1500) {
                lastHintMs_ = millis();
                Serial.print("INIT to0: axle=");
                Serial.print(axleDeg, 1);
                Serial.print("  remaining=");
                Serial.print(-axleDeg, 1);
                Serial.print(" deg  turn ");
                Serial.println(dir > 0.0f ? "positive" : "negative");
            }
            break;
        }

        case Phase::Idle:
            break;
    }
}

}  // namespace Homing
