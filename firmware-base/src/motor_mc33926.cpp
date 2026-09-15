#include "motor_driver.h"

#if defined(MOTOR_DRIVER_MC33926)

#include <Arduino.h>

#include "config.h"

namespace MotorDriver {
namespace {

bool en = false;
bool d2High_ = false;
float cmd1 = 0.0f;
float cmd2 = 0.0f;
float dutyCap_ = MOTOR_DUTY_CAP;

constexpr uint32_t kPwmHz = 20000;

float clampf(float v, float lo, float hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

void pwmOff() {
    analogWrite(PIN_M1_PWM, 0);
    analogWrite(PIN_M2_PWM, 0);
}

void setD2(bool high) {
    digitalWrite(PIN_MC33926_ND2, high ? HIGH : LOW);
    d2High_ = high;
}

void outputsDisabled() {
    pwmOff();
    setD2(false); // D2 low = coast / tri-state; SF is also forced low (not a real fault)
}

void writeChannel(int pinDir, int pinPwm, float cmd) {
    cmd = clampf(cmd, -1.0f, 1.0f);
    cmd = clampf(cmd, -dutyCap_, dutyCap_);

    if (fabsf(cmd) < 0.001f) {
        analogWrite(pinPwm, 0); // D2 high + PWM 0 = brake-low
        return;
    }

    digitalWrite(pinDir, cmd > 0.0f ? HIGH : LOW);
    const int duty = (int)(fabsf(cmd) * (float)PWM_MAX + 0.5f);
    analogWrite(pinPwm, duty);
}

void applyOutputs() {
    if (!en) {
        outputsDisabled();
        return;
    }

    // Keep D2 high while armed so motor indicator LEDs and nSF are meaningful.
    // (Pololu: nSF is forced low whenever D2 is low — ignore that as a "fault".)
    setD2(true);
    writeChannel(PIN_M1_DIR, PIN_M1_PWM, cmd1);
    writeChannel(PIN_M2_DIR, PIN_M2_PWM, cmd2);
}

float readFbAmps(int pin) {
    if (pin < 0)
        return 0.0f;
    const int raw = analogRead(pin);
    const float volts = (float)raw * (3.3f / (float)ADC_MAX);
    return volts / MC33926_FB_VOLTS_PER_AMP;
}

bool sfPinFaultLevel() {
    if (PIN_MC33926_NSF < 0)
        return false;
    const bool pinLow = digitalRead(PIN_MC33926_NSF) == LOW;
    return MC33926_SF_ACTIVE_LOW ? pinLow : !pinLow;
}

} // namespace

const char* backendName() {
    return "mc33926";
}

void begin() {
    pinMode(PIN_M1_DIR, OUTPUT);
    pinMode(PIN_M1_PWM, OUTPUT);
    pinMode(PIN_M2_DIR, OUTPUT);
    pinMode(PIN_M2_PWM, OUTPUT);
    pinMode(PIN_MC33926_ND2, OUTPUT);
    if (PIN_MC33926_NSF >= 0)
        pinMode(PIN_MC33926_NSF, INPUT_PULLUP);

    if (PIN_M1_FB >= 0)
        pinMode(PIN_M1_FB, INPUT);
    if (PIN_M2_FB >= 0)
        pinMode(PIN_M2_FB, INPUT);

    analogWriteFreq(kPwmHz);
    analogWriteRange(PWM_MAX);

    digitalWrite(PIN_M1_DIR, LOW);
    digitalWrite(PIN_M2_DIR, LOW);
    outputsDisabled();

    en = false;
    cmd1 = cmd2 = 0.0f;
    dutyCap_ = MOTOR_DUTY_CAP;
}

void setEnabled(bool on) {
    en = on;
    if (!on) {
        cmd1 = cmd2 = 0.0f;
        outputsDisabled();
        return;
    }
    applyOutputs();
}

bool enabled() {
    return en;
}

void setMotor1(float cmd) {
    cmd1 = cmd * MOTOR_OUTPUT_SIGN;
    if (en)
        applyOutputs();
}

void setMotor2(float cmd) {
    cmd2 = cmd * MOTOR_OUTPUT_SIGN;
    if (en)
        applyOutputs();
}

void setBoth(float cmd) {
    cmd1 = cmd2 = cmd * MOTOR_OUTPUT_SIGN;
    if (en)
        applyOutputs();
}

void coast() {
    cmd1 = cmd2 = 0.0f;
    // Stay armed if enabled: brake at zero (D2 high). Full coast only when disabled.
    if (en) {
        setD2(true);
        pwmOff();
    } else {
        outputsDisabled();
    }
}

void stop() {
    cmd1 = cmd2 = 0.0f;
    en = false;
    outputsDisabled();
}

float lastCmd1() {
    return cmd1;
}

float lastCmd2() {
    return cmd2;
}

void setDutyCap(float cap) {
    dutyCap_ = clampf(cap, 0.0f, 1.0f);
}

float dutyCap() {
    return dutyCap_;
}

bool faultActive() {
    // Pololu: "SF will also be low whenever D2 is low" — not a latched fault.
    if (!d2High_)
        return false;
    return sfPinFaultLevel();
}

void clearFault() {
    // Pololu: toggle D2 to clear latched overcurrent/thermal faults.
    const bool restore = en;
    setD2(false);
    pwmOff();
    delayMicroseconds(100);
    setD2(true);
    delayMicroseconds(100);
    if (!restore)
        setD2(false);
    else
        applyOutputs();
}

float motor1CurrentA() {
    return readFbAmps(PIN_M1_FB);
}

float motor2CurrentA() {
    return readFbAmps(PIN_M2_FB);
}

} // namespace MotorDriver

#endif // MOTOR_DRIVER_MC33926
