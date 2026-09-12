#include "pedals.h"

#include "config.h"

namespace Pedals {
namespace {

AxisCal thr, brk, clu;

uint16_t readFiltered(int pin) {
    long sum = 0;
    for (int i = 0; i < PEDAL_FILTER_SAMPLES; i++) {
        sum += analogRead(pin);
        delayMicroseconds(30);
    }
    return (uint16_t)(sum / PEDAL_FILTER_SAMPLES);
}

float normalize(int raw, const AxisCal& cal) {
    if (!cal.armed || cal.maxV <= cal.minV + PEDAL_MIN_SPAN)
        return 0.0f;

    float n = (float)(raw - cal.minV) / (float)(cal.maxV - cal.minV);
    if (cal.inverted)
        n = 1.0f - n;
    if (n < 0.0f)
        n = 0.0f;
    if (n > 1.0f)
        n = 1.0f;

    // Rest often sits slightly above 0 — small deadzone
    if (n < 0.02f)
        n = 0.0f;
    return n;
}

void expand(AxisCal& cal, int raw) {
    if (raw < cal.minV)
        cal.minV = raw;
    if (raw > cal.maxV)
        cal.maxV = raw;
}

// Unplugged → stay 0. Connected rest (high) tracked until first press arms min/max.
void learn(AxisCal& cal, int raw) {
    if (cal.armed) {
        expand(cal, raw);
        return;
    }

    // Floating ADC when unplugged — ignore until we see a high rest (plugged in).
    if (raw < PEDAL_CONNECTED_REST_MIN) {
        cal.haveRest = false;
        return;
    }

    if (!cal.haveRest) {
        cal.restV = raw;
        cal.haveRest = true;
        return;
    }

    // Still at rest — gently follow so plug-in / settle is OK.
    if (cal.restV - raw < PEDAL_PRESS_START_DELTA) {
        cal.restV = (cal.restV * 7 + raw) / 8;
        return;
    }

    // First press: seed min/max from rest + this sample, then keep expanding.
    cal.minV = cal.restV < raw ? cal.restV : raw;
    cal.maxV = cal.restV > raw ? cal.restV : raw;
    cal.armed = true;
}

} // namespace

void begin() {
    analogReadResolution(ADC_BITS);
    resetCalibration();
}

void resetCalibration() {
    thr = AxisCal{};
    brk = AxisCal{};
    clu = AxisCal{};
    // Logitech pots decrease when pressed (3V3→GND). inverted → rest=0, press=1.
    thr.inverted = 1;
    brk.inverted = 1;
    clu.inverted = 1;
}

void captureExtents(const State& s) {
    learn(thr, s.rawThrottle);
    learn(brk, s.rawBrake);
    learn(clu, s.rawClutch);
}

bool calibrationReady() {
    auto ok = [](const AxisCal& c) { return c.armed && (c.maxV - c.minV) > PEDAL_MIN_SPAN; };
    return ok(thr) && ok(brk) && ok(clu);
}

void getCalibration(AxisCal& t, AxisCal& b, AxisCal& c) {
    t = thr;
    b = brk;
    c = clu;
}

void setCalibration(const AxisCal& t, const AxisCal& b, const AxisCal& c) {
    thr = t;
    brk = b;
    clu = c;
}

State read() {
    State s;
    s.rawThrottle = readFiltered(PIN_PEDAL_THROTTLE);
    s.rawBrake = readFiltered(PIN_PEDAL_BRAKE);
    s.rawClutch = readFiltered(PIN_PEDAL_CLUTCH);

    learn(thr, s.rawThrottle);
    learn(brk, s.rawBrake);
    learn(clu, s.rawClutch);

    s.throttle = normalize(s.rawThrottle, thr);
    s.brake = normalize(s.rawBrake, brk);
    s.clutch = normalize(s.rawClutch, clu);
    return s;
}

} // namespace Pedals
