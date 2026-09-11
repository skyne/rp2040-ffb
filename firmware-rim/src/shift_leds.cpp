#include "shift_leds.h"

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "config.h"
#include "link.h"

namespace ShiftLeds {
namespace {

Adafruit_NeoPixel *strip = nullptr;
FfbLink::RimConfig cfg{};
FfbLink::TelemetryPayload tel{};
bool haveTel = false;

uint8_t mode = FfbLink::LedModeAuto;
uint8_t solidR = 0, solidG = 0, solidB = 0;
uint8_t fillCount = 0;
uint16_t simRpm = 0;
uint8_t simFlags = 0;

uint32_t wheelColor(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return strip->Color(255 - pos * 3, 0, pos * 3);
    }
    if (pos < 170) {
        pos = (uint8_t)(pos - 85);
        return strip->Color(0, pos * 3, 255 - pos * 3);
    }
    pos = (uint8_t)(pos - 170);
    return strip->Color(pos * 3, 255 - pos * 3, 0);
}

void ensureStrip(uint8_t count) {
    if (!count) count = WS2812_DEFAULT_COUNT;
    if (strip && strip->numPixels() == count) return;
    delete strip;
    strip = new Adafruit_NeoPixel(count, PIN_WS2812, NEO_GRB + NEO_KHZ800);
    strip->begin();
    strip->setBrightness(cfg.shiftLedBright ? cfg.shiftLedBright : 60);
    strip->clear();
    strip->show();
}

// Brighter indicator colors (strip brightness still caps overall level).
uint32_t colYellow() { return strip->Color(180, 140, 0); }
uint32_t colBlue() { return strip->Color(0, 50, 220); }
uint32_t colRed() { return strip->Color(220, 0, 0); }
uint32_t colTc() { return strip->Color(200, 90, 0); }
uint32_t colAbs() { return strip->Color(0, 160, 200); }

void setPair(uint8_t first, uint32_t c) {
    strip->setPixelColor(first, c);
    strip->setPixelColor((uint16_t)(first + 1), c);
}

// Both LEDs same color, ~50% duty blink.
void blinkPair(uint8_t first, uint32_t color, uint16_t halfPeriodMs) {
    if ((millis() / halfPeriodMs) & 1) {
        setPair(first, color);
    }
}

// Alternate color A / color B on both LEDs (with a short off gap).
void altPair(uint8_t first, uint32_t a, uint32_t b, uint16_t slotMs) {
    const uint32_t t = millis() % (uint32_t)(slotMs * 2);
    const uint16_t onMs = (uint16_t)((slotMs * 4) / 5);  // ~80% on per slot
    if (t < onMs) {
        setPair(first, a);
    } else if (t >= slotMs && t < (uint32_t)(slotMs + onMs)) {
        setPair(first, b);
    }
}

void drawIndicators(uint8_t flags) {
    // Flag zone (LED 0–1): share both pixels so a single flag is obvious.
    // Red overrides; yellow+blue alternate; single color blinks on both.
    const bool red = (flags & FfbLink::TelRed) != 0;
    const bool yellow = (flags & FfbLink::TelYellow) != 0;
    const bool blue = (flags & FfbLink::TelBlue) != 0;

    if (red) {
        blinkPair(FfbLink::kLedFlagFirst, colRed(), 90);  // urgent
    } else if (yellow && blue) {
        altPair(FfbLink::kLedFlagFirst, colYellow(), colBlue(), 280);
    } else if (yellow) {
        blinkPair(FfbLink::kLedFlagFirst, colYellow(), 160);
    } else if (blue) {
        blinkPair(FfbLink::kLedFlagFirst, colBlue(), 160);
    }

    // Aid zone (LED 9–10): same pairing rules for TC / ABS.
    const bool tc = (flags & FfbLink::TelTc) != 0;
    const bool absOn = (flags & FfbLink::TelAbs) != 0;

    if (tc && absOn) {
        altPair(FfbLink::kLedAidFirst, colTc(), colAbs(), 320);
    } else if (tc) {
        blinkPair(FfbLink::kLedAidFirst, colTc(), 180);
    } else if (absOn) {
        blinkPair(FfbLink::kLedAidFirst, colAbs(), 180);
    }
}

void drawPitLimiter() {
    // Middle 7 LEDs: full-bar yellow blink (classic pit limiter look).
    if (!((millis() / 140) & 1)) return;
    const uint32_t c = colYellow();
    for (uint8_t i = 0; i < FfbLink::kLedRpmCount; ++i) {
        strip->setPixelColor((uint16_t)(FfbLink::kLedRpmFirst + i), c);
    }
}

void drawRpmOnly(uint16_t rpm) {
    const uint8_t nRpm = FfbLink::kLedRpmCount;
    const uint8_t base = FfbLink::kLedRpmFirst;

    if (rpm == 0) return;

    // Map rpm across 7 LEDs using fill thresholds [0..3]
    uint8_t stage = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        if (rpm >= cfg.shiftRpm[i]) stage = (uint8_t)(i + 1);
    }

    uint8_t lit = 0;
    if (stage >= 4) {
        lit = nRpm;
    } else if (stage > 0) {
        const uint16_t lo = stage == 1 ? 0 : cfg.shiftRpm[stage - 2];
        const uint16_t hi = cfg.shiftRpm[stage - 1];
        const uint8_t seg = (uint8_t)(((stage - 1) * nRpm) / 4);
        const uint8_t span = (uint8_t)((nRpm + 3) / 4);
        uint8_t frac = span;
        if (hi > lo) {
            frac = (uint8_t)(((uint32_t)(rpm - lo) * span) / (hi - lo));
            if (frac > span) frac = span;
        }
        lit = (uint8_t)(seg + frac);
        if (lit > nRpm) lit = nRpm;
    }

    uint16_t blinkAt = cfg.shiftRpm[4];
    if (blinkAt == 0) blinkAt = 7800;
    const bool overrev = rpm >= blinkAt;
    const bool blinkOn = !overrev || ((millis() / 80) & 1);

    for (uint8_t i = 0; i < lit; ++i) {
        const bool isRed = i >= 5;  // last two of the 7
        if (overrev && isRed && !blinkOn) continue;
        uint32_t color = strip->Color(0, 50, 0);
        if (isRed) color = strip->Color(50, 0, 0);
        else if (i >= 3) color = strip->Color(50, 35, 0);
        strip->setPixelColor((uint16_t)(base + i), color);
    }
}

void drawDashboard(uint16_t rpm, uint8_t flags) {
    if (!strip) return;
    strip->clear();
    if (flags & FfbLink::TelPit) {
        drawPitLimiter();  // overrides RPM bar while limiter is active
    } else {
        drawRpmOnly(rpm);
    }
    drawIndicators(flags);
    strip->show();
}

void drawZonesDemo() {
    if (!strip) return;
    strip->clear();
    strip->setPixelColor(0, colYellow());
    strip->setPixelColor(1, colBlue());
    for (uint8_t i = 0; i < FfbLink::kLedRpmCount; ++i) {
        strip->setPixelColor((uint16_t)(FfbLink::kLedRpmFirst + i), strip->Color(0, 25, 0));
    }
    strip->setPixelColor(FfbLink::kLedAidFirst, colTc());
    strip->setPixelColor(FfbLink::kLedAidFirst + 1, colAbs());
    strip->show();
}

void runBootSequence() {
    if (!strip) return;
    const uint8_t n = strip->numPixels();
    strip->setBrightness(cfg.shiftLedBright ? cfg.shiftLedBright : 60);

    // 1) Flag pair blink
    for (int k = 0; k < 2; ++k) {
        strip->clear();
        strip->setPixelColor(0, strip->Color(50, 40, 0));
        strip->setPixelColor(1, strip->Color(0, 20, 60));
        strip->show();
        delay(120);
        strip->clear();
        strip->show();
        delay(80);
    }

    // 2) RPM bar fill green→amber→red
    strip->clear();
    for (uint8_t i = 0; i < FfbLink::kLedRpmCount; ++i) {
        uint32_t c = strip->Color(0, 50, 0);
        if (i >= 5) c = strip->Color(50, 0, 0);
        else if (i >= 3) c = strip->Color(50, 35, 0);
        strip->setPixelColor((uint16_t)(FfbLink::kLedRpmFirst + i), c);
        strip->show();
        delay(40);
    }
    delay(150);

    // 3) TC / ABS blink
    for (int k = 0; k < 2; ++k) {
        strip->setPixelColor(FfbLink::kLedAidFirst, strip->Color(50, 25, 0));
        strip->setPixelColor(FfbLink::kLedAidFirst + 1, strip->Color(0, 40, 50));
        strip->show();
        delay(120);
        strip->setPixelColor(FfbLink::kLedAidFirst, 0);
        strip->setPixelColor(FfbLink::kLedAidFirst + 1, 0);
        strip->show();
        delay(80);
    }

    // 4) Quick full chase
    for (int head = 0; head < n + 2; ++head) {
        strip->clear();
        if (head >= 0 && head < n) {
            strip->setPixelColor((uint16_t)head, strip->Color(40, 40, 40));
        }
        strip->show();
        delay(30);
    }
    strip->clear();
    strip->show();
    mode = FfbLink::LedModeAuto;
}

}  // namespace

void begin() {
    FfbLink::defaultRimConfig(cfg);
    ensureStrip(cfg.shiftLedCount ? cfg.shiftLedCount : WS2812_DEFAULT_COUNT);
    runBootSequence();
}

void setConfig(const FfbLink::RimConfig &c) {
    cfg = c;
    ensureStrip(cfg.shiftLedCount ? cfg.shiftLedCount : WS2812_DEFAULT_COUNT);
    if (strip) strip->setBrightness(cfg.shiftLedBright ? cfg.shiftLedBright : 60);
}

void setTelemetry(const FfbLink::TelemetryPayload &t) {
    tel = t;
    haveTel = true;
}

void setTest(const FfbLink::ShiftLedPayload &cmd) {
    mode = cmd.mode;
    solidR = cmd.r;
    solidG = cmd.g;
    solidB = cmd.b;
    fillCount = cmd.param;
    simRpm = cmd.rpm;
    simFlags = cmd.flags;

    if (mode == FfbLink::LedModeBoot) {
        runBootSequence();
        return;
    }
    if (mode == FfbLink::LedModeOff && strip) {
        strip->clear();
        strip->show();
    }
    if (mode == FfbLink::LedModeZones) {
        drawZonesDemo();
    }
}

void showOta() {
    if (!strip) return;
    mode = FfbLink::LedModeOff;  // freeze auto/test redraws
    strip->clear();
    const uint8_t mid = (uint8_t)(FfbLink::kLedRpmFirst + FfbLink::kLedRpmCount / 2);
    strip->setPixelColor(mid, strip->Color(50, 20, 0));  // orange
    strip->show();
}

void clearOta() {
    if (!strip) return;
    mode = FfbLink::LedModeAuto;
    strip->clear();
    strip->show();
}

void update() {
    if (!strip) return;
    const uint8_t n = strip->numPixels();
    const uint32_t now = millis();

    switch (mode) {
        case FfbLink::LedModeOff:
            return;

        case FfbLink::LedModeZones:
            return;  // static until next command

        case FfbLink::LedModeSolid:
            for (uint8_t i = 0; i < n; ++i) {
                strip->setPixelColor(i, strip->Color(solidR, solidG, solidB));
            }
            strip->show();
            return;

        case FfbLink::LedModeFill: {
            strip->clear();
            const uint8_t cnt = fillCount > n ? n : fillCount;
            for (uint8_t i = 0; i < cnt; ++i) {
                strip->setPixelColor(i, strip->Color(solidR, solidG, solidB));
            }
            strip->show();
            return;
        }

        case FfbLink::LedModeChase: {
            strip->clear();
            const uint8_t head = (uint8_t)((now / 60) % n);
            strip->setPixelColor(head, strip->Color(solidR ? solidR : 80,
                                                    solidG ? solidG : 80,
                                                    solidB ? solidB : 80));
            if (n > 1) {
                const uint8_t t = (uint8_t)((head + n - 1) % n);
                strip->setPixelColor(t, strip->Color(20, 20, 20));
            }
            strip->show();
            return;
        }

        case FfbLink::LedModeRainbow: {
            const uint8_t base = (uint8_t)(now / 10);
            for (uint8_t i = 0; i < n; ++i) {
                strip->setPixelColor(i, wheelColor((uint8_t)(base + i * 256 / n)));
            }
            strip->show();
            return;
        }

        case FfbLink::LedModeRpm:
            drawDashboard(simRpm, simFlags);
            return;

        case FfbLink::LedModeAuto:
        default:
            // No base link → all red pulse
            if (!Link::linked()) {
                const bool on = (now / 300) & 1;
                const uint32_t red = on ? strip->Color(40, 0, 0) : 0;
                for (uint8_t i = 0; i < n; ++i) strip->setPixelColor(i, red);
                strip->show();
                return;
            }
            if (haveTel) {
                drawDashboard(tel.rpm, tel.flags);
            } else {
                // Linked, waiting for telemetry — blue center idle blink
                strip->clear();
                if ((now / 500) & 1) {
                    strip->setPixelColor(FfbLink::kLedRpmFirst + FfbLink::kLedRpmCount / 2,
                                         strip->Color(0, 0, 40));
                }
                strip->show();
            }
            return;
    }
}

}  // namespace ShiftLeds
