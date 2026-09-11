#include "shift_leds.h"

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <pico/mutex.h>
#include <string.h>

#include "config.h"
#include "link.h"

namespace ShiftLeds {
namespace {

Adafruit_NeoPixel *strip = nullptr;

mutex_t stateMu;
bool muReady = false;

// --- Shared state (Core0 writes, Core1 snapshots) ---
FfbLink::RimConfig cfg{};
FfbLink::TelemetryPayload tel{};
bool haveTel = false;

uint8_t mode = FfbLink::LedModeAuto;
uint8_t solidR = 0, solidG = 0, solidB = 0;
uint8_t fillCount = 0;
uint16_t simRpm = 0;
uint8_t simFlags = 0;

bool pendingBoot = false;
bool pendingOtaShow = false;
bool pendingOtaClear = false;
bool pendingZones = false;
bool pendingOffClear = false;
bool stripNeedsRebuild = false;
uint8_t desiredLedCount = WS2812_DEFAULT_COUNT;

struct Snapshot {
    FfbLink::RimConfig cfg;
    FfbLink::TelemetryPayload tel;
    bool haveTel;
    bool linkUp;
    uint8_t mode;
    uint8_t solidR, solidG, solidB;
    uint8_t fillCount;
    uint16_t simRpm;
    uint8_t simFlags;
    bool doBoot;
    bool doOtaShow;
    bool doOtaClear;
    bool doZones;
    bool doOffClear;
    bool rebuildStrip;
    uint8_t ledCount;
};

void lock() {
    if (muReady) mutex_enter_blocking(&stateMu);
}

void unlock() {
    if (muReady) mutex_exit(&stateMu);
}

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

uint32_t colYellow() { return strip->Color(180, 140, 0); }
uint32_t colBlue() { return strip->Color(0, 50, 220); }
uint32_t colRed() { return strip->Color(220, 0, 0); }
uint32_t colTc() { return strip->Color(200, 90, 0); }
uint32_t colAbs() { return strip->Color(0, 160, 200); }

void setPair(uint8_t first, uint32_t c) {
    strip->setPixelColor(first, c);
    strip->setPixelColor((uint16_t)(first + 1), c);
}

void blinkPair(uint8_t first, uint32_t color, uint16_t halfPeriodMs) {
    if ((millis() / halfPeriodMs) & 1) {
        setPair(first, color);
    }
}

void altPair(uint8_t first, uint32_t a, uint32_t b, uint16_t slotMs) {
    const uint32_t t = millis() % (uint32_t)(slotMs * 2);
    const uint16_t onMs = (uint16_t)((slotMs * 4) / 5);
    if (t < onMs) {
        setPair(first, a);
    } else if (t >= slotMs && t < (uint32_t)(slotMs + onMs)) {
        setPair(first, b);
    }
}

void drawIndicators(uint8_t flags) {
    const bool red = (flags & FfbLink::TelRed) != 0;
    const bool yellow = (flags & FfbLink::TelYellow) != 0;
    const bool blue = (flags & FfbLink::TelBlue) != 0;

    if (red) {
        blinkPair(FfbLink::kLedFlagFirst, colRed(), 90);
    } else if (yellow && blue) {
        altPair(FfbLink::kLedFlagFirst, colYellow(), colBlue(), 280);
    } else if (yellow) {
        blinkPair(FfbLink::kLedFlagFirst, colYellow(), 160);
    } else if (blue) {
        blinkPair(FfbLink::kLedFlagFirst, colBlue(), 160);
    }

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
        const bool isRed = i >= 5;
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
        drawPitLimiter();
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
}

Snapshot takeSnapshot() {
    Snapshot s{};
    lock();
    s.cfg = cfg;
    s.tel = tel;
    s.haveTel = haveTel;
    s.linkUp = Link::linked();
    s.mode = mode;
    s.solidR = solidR;
    s.solidG = solidG;
    s.solidB = solidB;
    s.fillCount = fillCount;
    s.simRpm = simRpm;
    s.simFlags = simFlags;
    s.doBoot = pendingBoot;
    s.doOtaShow = pendingOtaShow;
    s.doOtaClear = pendingOtaClear;
    s.doZones = pendingZones;
    s.doOffClear = pendingOffClear;
    s.rebuildStrip = stripNeedsRebuild;
    s.ledCount = desiredLedCount;
    pendingBoot = false;
    pendingOtaShow = false;
    pendingOtaClear = false;
    pendingZones = false;
    pendingOffClear = false;
    stripNeedsRebuild = false;
    unlock();
    return s;
}

void applySnapshotMeta(const Snapshot &s) {
    cfg = s.cfg;
    if (s.rebuildStrip || !strip) {
        ensureStrip(s.ledCount ? s.ledCount : WS2812_DEFAULT_COUNT);
    }
    if (strip) {
        strip->setBrightness(cfg.shiftLedBright ? cfg.shiftLedBright : 60);
    }

    if (s.doOtaShow && strip) {
        mode = FfbLink::LedModeOff;
        strip->clear();
        const uint8_t mid = (uint8_t)(FfbLink::kLedRpmFirst + FfbLink::kLedRpmCount / 2);
        strip->setPixelColor(mid, strip->Color(50, 20, 0));
        strip->show();
        return;
    }
    if (s.doOtaClear && strip) {
        mode = FfbLink::LedModeAuto;
        strip->clear();
        strip->show();
    }
    if (s.doBoot) {
        runBootSequence();
        lock();
        mode = FfbLink::LedModeAuto;
        unlock();
        return;
    }
    if (s.doZones) {
        drawZonesDemo();
        return;
    }
    if (s.doOffClear && strip) {
        strip->clear();
        strip->show();
    }
}

}  // namespace

void begin() {
    mutex_init(&stateMu);
    muReady = true;
    FfbLink::defaultRimConfig(cfg);
    desiredLedCount = cfg.shiftLedCount ? cfg.shiftLedCount : WS2812_DEFAULT_COUNT;
}

void beginCore1() {
    lock();
    const uint8_t count = desiredLedCount ? desiredLedCount : WS2812_DEFAULT_COUNT;
    const FfbLink::RimConfig localCfg = cfg;
    unlock();
    cfg = localCfg;
    ensureStrip(count);
    runBootSequence();
    lock();
    mode = FfbLink::LedModeAuto;
    unlock();
}

void setConfig(const FfbLink::RimConfig &c) {
    lock();
    cfg = c;
    desiredLedCount = cfg.shiftLedCount ? cfg.shiftLedCount : WS2812_DEFAULT_COUNT;
    stripNeedsRebuild = true;
    unlock();
}

void setTelemetry(const FfbLink::TelemetryPayload &t) {
    lock();
    tel = t;
    haveTel = true;
    unlock();
}

void clearTelemetry() {
    lock();
    haveTel = false;
    tel = FfbLink::TelemetryPayload{};
    pendingOffClear = true;  // wipe last RPM/flags immediately on Core1
    unlock();
}

void setTest(const FfbLink::ShiftLedPayload &cmd) {
    lock();
    mode = cmd.mode;
    solidR = cmd.r;
    solidG = cmd.g;
    solidB = cmd.b;
    fillCount = cmd.param;
    simRpm = cmd.rpm;
    simFlags = cmd.flags;
    if (mode == FfbLink::LedModeBoot) {
        pendingBoot = true;
    } else if (mode == FfbLink::LedModeOff) {
        pendingOffClear = true;
    } else if (mode == FfbLink::LedModeZones) {
        pendingZones = true;
    }
    unlock();
}

void showOta() {
    lock();
    pendingOtaShow = true;
    mode = FfbLink::LedModeOff;
    unlock();
}

void clearOta() {
    lock();
    pendingOtaClear = true;
    unlock();
}

void update() {
    Snapshot s = takeSnapshot();
    applySnapshotMeta(s);
    if (!strip) return;

    // One-shot handlers above already drew.
    if (s.doBoot || s.doOtaShow || s.doZones) return;

    const uint8_t n = strip->numPixels();
    const uint32_t now = millis();
    const uint8_t drawMode = s.mode;

    switch (drawMode) {
        case FfbLink::LedModeOff:
            return;

        case FfbLink::LedModeZones:
            return;

        case FfbLink::LedModeSolid:
            for (uint8_t i = 0; i < n; ++i) {
                strip->setPixelColor(i, strip->Color(s.solidR, s.solidG, s.solidB));
            }
            strip->show();
            return;

        case FfbLink::LedModeFill: {
            strip->clear();
            const uint8_t cnt = s.fillCount > n ? n : s.fillCount;
            for (uint8_t i = 0; i < cnt; ++i) {
                strip->setPixelColor(i, strip->Color(s.solidR, s.solidG, s.solidB));
            }
            strip->show();
            return;
        }

        case FfbLink::LedModeChase: {
            strip->clear();
            const uint8_t head = (uint8_t)((now / 60) % n);
            strip->setPixelColor(head, strip->Color(s.solidR ? s.solidR : 80,
                                                    s.solidG ? s.solidG : 80,
                                                    s.solidB ? s.solidB : 80));
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
            drawDashboard(s.simRpm, s.simFlags);
            return;

        case FfbLink::LedModeAuto:
        default:
            if (!s.linkUp) {
                const bool on = (now / 300) & 1;
                const uint32_t red = on ? strip->Color(40, 0, 0) : 0;
                for (uint8_t i = 0; i < n; ++i) strip->setPixelColor(i, red);
                strip->show();
                return;
            }
            if (s.haveTel) {
                drawDashboard(s.tel.rpm, s.tel.flags);
            } else {
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
