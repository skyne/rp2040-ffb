#include "display.h"

#include <pico/mutex.h>
#include <string.h>

namespace Display {
namespace {

mutex_t mu;
bool muReady = false;
bool telValid = false;
bool powerSave = false;
FfbLink::TelemetryPayload tel{};

void ensureMu() {
    if (muReady) return;
    mutex_init(&mu);
    muReady = true;
}

}  // namespace

void begin() {
    ensureMu();
}

void beginCore1() {
    ensureMu();
    // ILI9341 init lands here (SPI1 pins in config.h).
}

void setTelemetryValid(bool valid) {
    ensureMu();
    mutex_enter_blocking(&mu);
    telValid = valid;
    if (!valid) {
        tel = FfbLink::TelemetryPayload{};
    }
    mutex_exit(&mu);
}

void setTelemetry(const FfbLink::TelemetryPayload &t) {
    ensureMu();
    mutex_enter_blocking(&mu);
    tel = t;
    telValid = true;
    mutex_exit(&mu);
}

void setPowerSave(bool on) {
    ensureMu();
    mutex_enter_blocking(&mu);
    powerSave = on;
    mutex_exit(&mu);
}

void update() {
    if (!muReady) return;
    mutex_enter_blocking(&mu);
    const bool valid = telValid;
    const bool ps = powerSave;
    mutex_exit(&mu);
    (void)valid;
    (void)ps;
    // When the TFT driver is wired:
    //   ps     → backlight off / blank
    //   valid  → live dashboard from `tel`
    //   !valid → "Waiting for Telemetry / Hardware Standby"
}

bool telemetryValid() {
    if (!muReady) return false;
    mutex_enter_blocking(&mu);
    const bool v = telValid;
    mutex_exit(&mu);
    return v;
}

}  // namespace Display
