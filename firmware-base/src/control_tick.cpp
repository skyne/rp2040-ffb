#include "control_tick.h"

#include <Arduino.h>

#include "accessory_link.h"
#include "axle_index.h"
#include "config.h"
#include "ffb.h"
#include "hid_wheel.h"
#include "homing.h"
#include "mlx90363.h"
#include "settings.h"
#include "status_leds.h"
#include "wheel_encoder.h"

namespace ControlTick {
namespace {

SerialHook serialHook_ = nullptr;
uint8_t depth_ = 0;

bool hallOk_ = false;
float axleDeg_ = 0;
float sensorDeg_ = 0;
Pedals::State ped_{};
bool syncArmedLatched_ = false;

} // namespace

void begin(SerialHook serialHook) {
    serialHook_ = serialHook;
}

void service() {
    if (depth_ != 0)
        return;
    ++depth_;

    if (serialHook_)
        serialHook_();
    AccessoryLink::update();

    digitalWrite(LED_BUILTIN, ((millis() / 250) & 1) ? HIGH : LOW);

    float raw = 0;
    hallOk_ = Mlx90363::readAlphaDegrees(raw);
    if (hallOk_) {
        WheelEncoder::update(raw);
        sensorDeg_ = raw;
    }

    const bool wasArmed = AxleIndex::syncArmed() || syncArmedLatched_;
    const bool indexEdge = AxleIndex::update();

    if (indexEdge && !Homing::active()) {
        if (wasArmed || AXLE_INDEX_SYNC_ON_EDGE) {
            WheelEncoder::syncToIndexAngle(AXLE_INDEX_ANGLE_DEG);
            syncArmedLatched_ = false;
        }
        if (Settings::telemetryEnabled()) {
            Serial.print("T INDEX edges=");
            Serial.print(AxleIndex::edgeCount());
            Serial.print(" axle=");
            Serial.println(WheelEncoder::axleDegrees(), 2);
        }
    }

    Homing::update(indexEdge, WheelEncoder::axleDegrees(), hallOk_);

    ped_ = Pedals::read();
    axleDeg_ = WheelEncoder::axleDegrees();
    if (!Homing::active()) {
        Ffb::update(axleDeg_);
    }

    AccessoryLink::update();

    if (Homing::active()) {
        // Keep CDC/HID alive without joystick spam during INIT.
        HidWheel::serviceUsb();
    } else {
        float paddles[FfbLink::kAnalogCount] = {};
        AccessoryLink::panelAxes(paddles);
        HidWheel::update(axleDeg_, ped_.throttle, ped_.brake, ped_.clutch,
                         paddles[FfbLink::kAnalogClutchL], paddles[FfbLink::kAnalogClutchR],
                         AccessoryLink::hidButtons());
    }

    AccessoryLink::update();
    StatusLeds::update(hallOk_, AxleIndex::active(), axleDeg_);

    --depth_;
}

bool lastHallOk() {
    return hallOk_;
}
float lastAxleDeg() {
    return axleDeg_;
}
float lastSensorDeg() {
    return sensorDeg_;
}
const Pedals::State& lastPedals() {
    return ped_;
}

void armIndexSyncLatch() {
    syncArmedLatched_ = true;
}

} // namespace ControlTick
