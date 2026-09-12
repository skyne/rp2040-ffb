#include "motor_bts7960.h"

#include <Arduino.h>

#include "config.h"

namespace MotorBts7960 {
namespace {

bool en = false;
float cmd1 = 0.0f;
float cmd2 = 0.0f;
float dutyCap_ = MOTOR_DUTY_CAP;

float clampf(float v, float lo, float hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

void writeChannel(int pinRpwm, int pinLpwm, float cmd) {
    cmd = clampf(cmd, -1.0f, 1.0f);
    cmd = clampf(cmd, -dutyCap_, dutyCap_);

    const int duty = (int)(fabsf(cmd) * (float)PWM_MAX + 0.5f);

    if (cmd > 0.001f) {
        analogWrite(pinRpwm, duty);
        analogWrite(pinLpwm, 0);
    } else if (cmd < -0.001f) {
        analogWrite(pinRpwm, 0);
        analogWrite(pinLpwm, duty);
    } else {
        analogWrite(pinRpwm, 0);
        analogWrite(pinLpwm, 0);
    }
}

} // namespace

void begin() {
    pinMode(PIN_M1_RPWM, OUTPUT);
    pinMode(PIN_M1_LPWM, OUTPUT);
    pinMode(PIN_M1_EN, OUTPUT);
    pinMode(PIN_M2_RPWM, OUTPUT);
    pinMode(PIN_M2_LPWM, OUTPUT);
    pinMode(PIN_M2_EN, OUTPUT);

    analogWrite(PIN_M1_RPWM, 0);
    analogWrite(PIN_M1_LPWM, 0);
    analogWrite(PIN_M2_RPWM, 0);
    analogWrite(PIN_M2_LPWM, 0);
    digitalWrite(PIN_M1_EN, LOW);
    digitalWrite(PIN_M2_EN, LOW);
    en = false;
    cmd1 = cmd2 = 0.0f;
    dutyCap_ = MOTOR_DUTY_CAP;
}

void setEnabled(bool on) {
    en = on;
    digitalWrite(PIN_M1_EN, on ? HIGH : LOW);
    digitalWrite(PIN_M2_EN, on ? HIGH : LOW);
    if (!on) {
        analogWrite(PIN_M1_RPWM, 0);
        analogWrite(PIN_M1_LPWM, 0);
        analogWrite(PIN_M2_RPWM, 0);
        analogWrite(PIN_M2_LPWM, 0);
        cmd1 = cmd2 = 0.0f;
    }
}

bool enabled() {
    return en;
}

void setMotor1(float cmd) {
    cmd1 = cmd;
    if (en)
        writeChannel(PIN_M1_RPWM, PIN_M1_LPWM, cmd);
}

void setMotor2(float cmd) {
    cmd2 = cmd;
    if (en)
        writeChannel(PIN_M2_RPWM, PIN_M2_LPWM, cmd);
}

void setBoth(float cmd) {
    setMotor1(cmd);
    setMotor2(cmd);
}

void coast() {
    analogWrite(PIN_M1_RPWM, 0);
    analogWrite(PIN_M1_LPWM, 0);
    analogWrite(PIN_M2_RPWM, 0);
    analogWrite(PIN_M2_LPWM, 0);
    cmd1 = cmd2 = 0.0f;
}

void stop() {
    coast();
    setEnabled(false);
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

} // namespace MotorBts7960
