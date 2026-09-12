#include "status_leds.h"

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <math.h>

#include "accessory_link.h"
#include "config.h"
#include "ffb.h"
#include "homing.h"
#include "motor_bts7960.h"

namespace StatusLeds {
namespace {

Adafruit_NeoPixel ring(STATUS_NEOPIXEL_COUNT, PIN_STATUS_NEOPIXEL, NEO_GRB + NEO_KHZ800);

uint32_t lastDrawMs = 0;
uint8_t spin = 0;

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ring.Color(r, g, b);
}

void clear() {
    for (int i = 0; i < STATUS_NEOPIXEL_COUNT; i++)
        ring.setPixelColor(i, 0);
}

void fill(uint32_t c) {
    for (int i = 0; i < STATUS_NEOPIXEL_COUNT; i++)
        ring.setPixelColor(i, c);
}

void chase(uint32_t c, uint8_t pos) {
    clear();
    ring.setPixelColor(pos % STATUS_NEOPIXEL_COUNT, c);
    ring.setPixelColor((pos + 1) % STATUS_NEOPIXEL_COUNT, c);
}

// Map axle [-half..+half] onto the ring as a single bright pip + dim trail.
void axlePip(float axleDeg) {
    clear();
    const float half = WHEEL_HID_RANGE_DEG * 0.5f;
    float n = axleDeg / half;
    if (n < -1.0f)
        n = -1.0f;
    if (n > 1.0f)
        n = 1.0f;
    // 0 = top-ish LED 0; sweep around ring
    float u = (n + 1.0f) * 0.5f; // 0..1
    int idx = (int)(u * (STATUS_NEOPIXEL_COUNT - 1) + 0.5f);
    if (idx < 0)
        idx = 0;
    if (idx >= STATUS_NEOPIXEL_COUNT)
        idx = STATUS_NEOPIXEL_COUNT - 1;

    // dim ring
    for (int i = 0; i < STATUS_NEOPIXEL_COUNT; i++) {
        ring.setPixelColor(i, rgb(0, 0, 8));
    }
    // center-ish marker at LED 0 when straight
    ring.setPixelColor(0, rgb(0, 20, 0));
    ring.setPixelColor(idx, rgb(0, 80, 40));
}

} // namespace

void begin() {
    ring.begin();
    ring.setBrightness(STATUS_NEOPIXEL_BRIGHTNESS);
    clear();
    ring.show();
}

void showUpdateBrief() {
    fill(rgb(80, 35, 0));
    ring.show();
}

void update(bool hallOk, bool indexActive, float axleDeg) {
    const uint32_t now = millis();
    if (now - lastDrawMs < 40)
        return; // ~25 fps
    lastDrawMs = now;
    spin++;

    if (!hallOk) {
        // Hall fault — red blink
        fill(((now / 200) & 1) ? rgb(80, 0, 0) : rgb(10, 0, 0));
        ring.show();
        return;
    }

    // Rim OTA / updater in progress — orange chase (matches rim OTA cue).
    if (AccessoryLink::otaInProgress()) {
        chase(rgb(90, 35, 0), spin / 2);
        // Center pip blink so it reads even if chase is missed
        if ((now / 150) & 1) {
            ring.setPixelColor(STATUS_NEOPIXEL_COUNT / 2, rgb(100, 40, 0));
        }
        ring.show();
        return;
    }

    if (Homing::active()) {
        switch (Homing::phase()) {
        case Homing::Phase::ProbeAdxl:
            // Dim amber pulse — ADXL probe
            chase(rgb(50, 30, 0), spin / 3);
            break;
        case Homing::Phase::SeekGravity:
            // Green chase — gravity zero
            chase(rgb(0, 60, 20), spin / 2);
            break;
        case Homing::Phase::SeekIndex:
            // Amber chase — looking for index
            chase(rgb(80, 40, 0), spin / 2);
            break;
        case Homing::Phase::MeasureIndex:
            // Orange chase — window enter/exit measure
            chase(rgb(90, 25, 0), spin / 2);
            break;
        case Homing::Phase::SeekZero:
            // Cyan chase — going to center
            chase(rgb(0, 50, 70), spin / 2);
            break;
        default:
            fill(rgb(20, 20, 0));
            break;
        }
        if (indexActive) {
            // brief white flash on magnet
            fill(rgb(60, 60, 60));
        }
        ring.show();
        return;
    }

    // Runtime status
    if (indexActive) {
        fill(rgb(50, 0, 50)); // magenta = on index
        ring.show();
        return;
    }

    switch (Ffb::mode()) {
    case Ffb::Mode::Spring:
        axlePip(axleDeg);
        // breathe green on LED 0
        {
            const uint8_t b = (uint8_t)(20 + (sinf(now / 400.0f) * 0.5f + 0.5f) * 40);
            ring.setPixelColor(0, rgb(0, b, 0));
        }
        break;
    case Ffb::Mode::Manual: {
        clear();
        const float t = fabsf(Ffb::manualTorque());
        const int lit = (int)(t * STATUS_NEOPIXEL_COUNT + 0.5f);
        for (int i = 0; i < lit && i < STATUS_NEOPIXEL_COUNT; i++) {
            ring.setPixelColor(i, rgb(70, 25, 0));
        }
        break;
    }
    case Ffb::Mode::Off:
    default:
        axlePip(axleDeg);
        if (MotorBts7960::enabled()) {
            ring.setPixelColor(STATUS_NEOPIXEL_COUNT - 1, rgb(40, 40, 0)); // motors armed tip
        }
        break;
    }

    ring.show();
}

} // namespace StatusLeds
