#include "hid_wheel.h"

#include <Arduino.h>
#include <USB.h>
#include <math.h>
#include <pico/mutex.h>
#include <string.h>
#include <tusb-hid.h>

#include "class/hid/hid_device.h"
#include "config.h"
#include "ffb_pid.h"
#include "hid_pid_desc.h"
#include "hid_pid_reports.h"
#include "setup_msc.h"
#include "tusb.h"

namespace HidWheel {
namespace {

int lastHidX = 0;
float rangeDeg_ = WHEEL_HID_RANGE_DEG;
uint8_t hidLocalId_ = 0;
bool inited_ = false;
bool usbUp_ = false;       // HID iface registered (once at boot)
bool reportsLive_ = false; // after INIT: send joy reports / accept FFB lifecycle
bool settleArmed_ = false;
uint32_t settleDeadlineMs_ = 0;
uint32_t lastReportMs_ = 0;

HidPid::JoystickInputReport report_{};

constexpr uint32_t kMinReportPeriodMs = 10; // 100 Hz max

int clampf(int v, int lo, int hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

int16_t axleToAxis(float axleDeg) {
    const float half = rangeDeg_ * 0.5f;
    float n = (half > 0.0f) ? (axleDeg / half) : 0.0f;
    if (n < -1.0f)
        n = -1.0f;
    if (n > 1.0f)
        n = 1.0f;
    return (int16_t)clampf((int)lroundf(n * 32767.0f), -32767, 32767);
}

int16_t pedalToAxis(float p) {
    if (p < 0.0f)
        p = 0.0f;
    if (p > 1.0f)
        p = 1.0f;
    return (int16_t)clampf((int)lroundf((p * 2.0f - 1.0f) * 32767.0f), -32767, 32767);
}

bool tryUsb(bool sendReport) {
#if ENABLE_USB_HID
    uint32_t owner = 0;
    if (!mutex_try_enter(&USB.mutex, &owner))
        return false;
    tud_task();
    if (sendReport && usbUp_ && reportsLive_ && USB.HIDReady()) {
        tud_hid_n_report(0, HidPid::kReportIdInput, &report_, sizeof(report_));
    }
    tud_task();
    mutex_exit(&USB.mutex);
    return true;
#else
    (void)sendReport;
    return false;
#endif
}

} // namespace

bool begin() {
    rangeDeg_ = WHEEL_HID_RANGE_DEG;
#if ENABLE_USB_HID
    FfbPid::begin();
    memset(&report_, 0, sizeof(report_));
    report_.hat = 8;
    report_.x = 0;
    report_.y = -32767;
    report_.z = -32767;
    report_.rz = -32767;
    report_.rx = -32767;
    report_.ry = -32767;
    reportsLive_ = false;
    settleArmed_ = false;
    settleDeadlineMs_ = 0;

    // One-shot USB compose at boot. Never disconnect again after INIT — that
    // killed CDC/rim (host re-enum mid-session).
    USB.disconnect();
    hidLocalId_ = USB.registerHIDDevice(HidPid::kHidReportDescriptor,
                                        HidPid::kHidReportDescriptorLen, 30, 0x0010);
    SetupMsc::attachAtBoot();
    USB.connect();
    usbUp_ = true;
    inited_ = true;

    Serial.print("STAGE t=");
    Serial.print(millis());
    Serial.print(" usb: PID HID registered at boot desc=");
    Serial.print((unsigned)HidPid::kHidReportDescriptorLen);
    Serial.println(" B (reports gated until INIT)");
    return true;
#else
    (void)hidLocalId_;
    return false;
#endif
}

void serviceAttach(bool homingActive) {
#if ENABLE_USB_HID
    if (!inited_ || reportsLive_)
        return;
    if (homingActive) {
        settleArmed_ = false;
        return;
    }
    if (!settleArmed_) {
        settleArmed_ = true;
        settleDeadlineMs_ = millis() + HID_ATTACH_SETTLE_MS;
        Serial.print("STAGE t=");
        Serial.print(millis());
        Serial.print(" usb: INIT idle — enable HID reports in ");
        Serial.print(HID_ATTACH_SETTLE_MS);
        Serial.println(" ms (no re-enum)");
        return;
    }
    if ((int32_t)(millis() - settleDeadlineMs_) < 0)
        return;
    reportsLive_ = true;
    Serial.print("STAGE t=");
    Serial.print(millis());
    Serial.println(" usb: HID reports + FFB lifecycle live");
    tryUsb(true);
#else
    (void)homingActive;
#endif
}

bool ready() {
    return ENABLE_USB_HID != 0 && usbUp_ && reportsLive_;
}

bool attached() {
    // "Attached" for MSC/grace = USB iface up (registered at boot).
    return usbUp_;
}

int lastSteeringHid() {
    return lastHidX;
}

void setRangeDeg(float deg) {
    if (deg < 10.0f)
        deg = 10.0f;
    rangeDeg_ = deg;
}

float rangeDeg() {
    return rangeDeg_;
}

void serviceUsb() {
#if ENABLE_USB_HID
    if (!inited_)
        return;
    tryUsb(false);
#endif
}

void update(float axleDegrees, float throttle, float brake, float clutch, float paddleClutchL,
            float paddleClutchR, uint32_t buttons) {
    const int16_t x = axleToAxis(axleDegrees);
    lastHidX = (int)x;
#if ENABLE_USB_HID
    if (!inited_)
        return;

    report_.x = x;
    report_.y = pedalToAxis(throttle);
    report_.z = pedalToAxis(brake);
    report_.rz = pedalToAxis(clutch);
    report_.rx = pedalToAxis(paddleClutchL);
    report_.ry = pedalToAxis(paddleClutchR);
    report_.buttons[0] = (uint8_t)(buttons & 0xFFu);
    report_.buttons[1] = (uint8_t)((buttons >> 8) & 0xFFu);
    report_.buttons[2] = (uint8_t)((buttons >> 16) & 0xFFu);
    report_.buttons[3] = (uint8_t)((buttons >> 24) & 0xFFu);
    report_.hat = 8;

    if (!reportsLive_) {
        tryUsb(false);
        return;
    }

    const uint32_t now = millis();
    if ((now - lastReportMs_) < kMinReportPeriodMs) {
        tryUsb(false);
        return;
    }
    if (tryUsb(true))
        lastReportMs_ = now;
#else
    (void)throttle;
    (void)brake;
    (void)clutch;
    (void)paddleClutchL;
    (void)paddleClutchR;
    (void)buttons;
#endif
}

} // namespace HidWheel

#if ENABLE_USB_HID
extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                          hid_report_type_t report_type, uint8_t* buffer,
                                          uint16_t reqlen) {
    (void)instance;
    // Ignore host FFB traffic until INIT settle — still ACK GET with zeros via FfbPid.
    return FfbPid::onGetReport(report_id, (uint8_t)report_type, buffer, reqlen);
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                      hid_report_type_t report_type, uint8_t const* buffer,
                                      uint16_t bufsize) {
    (void)instance;
    if (!HidWheel::ready()) {
        // Drop SET_REPORT during INIT so Create/Start effects don't queue a surprise.
        return;
    }
    FfbPid::onSetReport(report_id, (uint8_t)report_type, buffer, bufsize);
}
#endif
