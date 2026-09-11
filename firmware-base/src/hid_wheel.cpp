#include "hid_wheel.h"

#include <Arduino.h>

#include "config.h"

#if ENABLE_USB_HID
#include <Joystick.h>
#endif

namespace HidWheel {
namespace {

int lastHidX = 0;
float rangeDeg_ = WHEEL_HID_RANGE_DEG;

int clampf(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Adafruit/earle Joystick default is 10-bit: 0..1023 with center ~512.
// Feeding raw int16 with use16bit was unreliable vs jstest on Linux.
int axleToJoy(float axleDeg) {
    const float half = rangeDeg_ * 0.5f;
    float n = axleDeg / half;
    if (n < -1.0f) n = -1.0f;
    if (n > 1.0f) n = 1.0f;
    return clampf((int)((n + 1.0f) * 0.5f * 1023.0f + 0.5f), 0, 1023);
}

int pedalToJoy(float p) {
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return (int)(p * 1023.0f + 0.5f);
}

}  // namespace

bool begin() {
    rangeDeg_ = WHEEL_HID_RANGE_DEG;
#if ENABLE_USB_HID
    Joystick.use10bit();
    Joystick.useManualSend(true);
    Joystick.begin();
    Joystick.hat(-1);
    Joystick.X(512);
    Joystick.Y(0);
    Joystick.Z(0);
    Joystick.Zrotate(0);
    Joystick.send_now();
    return true;
#else
    return false;
#endif
}

bool ready() { return ENABLE_USB_HID != 0; }

int lastSteeringHid() { return lastHidX; }

void setRangeDeg(float deg) {
    if (deg < 10.0f) deg = 10.0f;
    rangeDeg_ = deg;
}

float rangeDeg() { return rangeDeg_; }

void update(float axleDegrees, float throttle, float brake, float clutch,
            uint32_t buttons) {
    lastHidX = axleToJoy(axleDegrees);
#if ENABLE_USB_HID
    Joystick.X(lastHidX);
    Joystick.Y(pedalToJoy(throttle));
    Joystick.Z(pedalToJoy(brake));
    Joystick.Zrotate(pedalToJoy(clutch));
    for (int i = 0; i < 32; ++i) {
        Joystick.button(i + 1, (buttons >> i) & 1u);
    }
    Joystick.send_now();
#else
    (void)throttle;
    (void)brake;
    (void)clutch;
    (void)buttons;
#endif
}

}  // namespace HidWheel
