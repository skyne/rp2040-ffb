#include "shift_leds.h"

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <pico/mutex.h>
#include <string.h>

#include "config.h"
#include "inputs.h"
#include "link.h"

namespace ShiftLeds {
namespace {

Adafruit_NeoPixel* strip = nullptr;

mutex_t stateMu;
bool muReady = false;

// --- Shared state (Core0 writes, Core1 snapshots) ---
FfbLink::RimConfig cfg{};
FfbLink::TelemetryPayload tel{};
bool haveTel = false;
bool powerSave = false;

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
bool pendingStripReinit = false;  // force PIO NeoPixel recreate (OTA recovery)
bool otaCue = false;              // sticky orange cue while updater runs
volatile bool flashQuiet = false; // Core0 pauses bitbang during flash writes
uint8_t desiredLedCount = WS2812_DEFAULT_COUNT;

struct Snapshot {
    FfbLink::RimConfig cfg;
    FfbLink::TelemetryPayload tel;
    bool haveTel;
    bool powerSave;
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
    bool reinitStrip;
    uint8_t ledCount;
};

void lock() {
    if (muReady)
        mutex_enter_blocking(&stateMu);
}

void unlock() {
    if (muReady)
        mutex_exit(&stateMu);
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

// NeoPixel setBrightness scales RGB as (c * bright) / 255. Dim status colors
// (e.g. idle blue 40) floor to 0 below ~20 — keep a usable floor.
constexpr uint8_t kMinShiftLedBright = 20;

uint8_t stripBright() {
    uint8_t b = cfg.shiftLedBright ? cfg.shiftLedBright : 60;
    // Ambient auto-dim (255 = full / no LDR). Floor still applies after scale.
    b = (uint8_t)(((uint16_t)b * (uint16_t)Inputs::ambientScale()) / 255u);
    if (b < kMinShiftLedBright)
        b = kMinShiftLedBright;
    return b;
}

void ensureStrip(uint8_t count) {
    if (!count)
        count = WS2812_DEFAULT_COUNT;
    if (strip && strip->numPixels() == count)
        return;
    delete strip;
    strip = new Adafruit_NeoPixel(count, PIN_WS2812, NEO_GRB + NEO_KHZ800);
    strip->begin();
    strip->setBrightness(stripBright());
    strip->clear();
    strip->show();
}

uint32_t colYellow() {
    return strip->Color(180, 140, 0);
}
uint32_t colBlue() {
    return strip->Color(0, 50, 220);
}
uint32_t colRed() {
    return strip->Color(220, 0, 0);
}
uint32_t colTc() {
    return strip->Color(200, 90, 0);
}
uint32_t colAbs() {
    return strip->Color(0, 160, 200);
}

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
    if (!((millis() / 140) & 1))
        return;
    const uint32_t c = colYellow();
    for (uint8_t i = 0; i < FfbLink::kLedRpmCount; ++i) {
        strip->setPixelColor((uint16_t)(FfbLink::kLedRpmFirst + i), c);
    }
}

void drawRpmOnly(uint16_t rpm) {
    const uint8_t nRpm = FfbLink::kLedRpmCount;
    const uint8_t base = FfbLink::kLedRpmFirst;

    if (rpm == 0)
        return;

    uint8_t stage = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        if (rpm >= cfg.shiftRpm[i])
            stage = (uint8_t)(i + 1);
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
            if (frac > span)
                frac = span;
        }
        lit = (uint8_t)(seg + frac);
        if (lit > nRpm)
            lit = nRpm;
    }

    uint16_t blinkAt = cfg.shiftRpm[4];
    if (blinkAt == 0)
        blinkAt = 7800;
    const bool overrev = rpm >= blinkAt;
    const bool blinkOn = !overrev || ((millis() / 80) & 1);

    for (uint8_t i = 0; i < lit; ++i) {
        const bool isRed = i >= 5;
        if (overrev && isRed && !blinkOn)
            continue;
        uint32_t color = strip->Color(0, 50, 0);
        if (isRed)
            color = strip->Color(50, 0, 0);
        else if (i >= 3)
            color = strip->Color(50, 35, 0);
        strip->setPixelColor((uint16_t)(base + i), color);
    }
}

void drawDashboard(uint16_t rpm, uint8_t flags) {
    if (!strip)
        return;
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
    if (!strip)
        return;
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
    if (!strip)
        return;
    const uint8_t n = strip->numPixels();
    strip->setBrightness(stripBright());

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
        if (i >= 5)
            c = strip->Color(50, 0, 0);
        else if (i >= 3)
            c = strip->Color(50, 35, 0);
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
    s.powerSave = powerSave;
    s.linkUp = Link::linked();
    s.mode = mode;
    s.solidR = solidR;
    s.solidG = solidG;
    s.solidB = solidB;
    s.fillCount = fillCount;
    s.simRpm = simRpm;
    s.simFlags = simFlags;
    s.doBoot = pendingBoot;
    s.doOtaShow = pendingOtaShow || otaCue;
    s.doOtaClear = pendingOtaClear;
    s.doZones = pendingZones;
    s.doOffClear = pendingOffClear;
    s.rebuildStrip = stripNeedsRebuild;
    s.reinitStrip = pendingStripReinit;
    s.ledCount = desiredLedCount;
    pendingBoot = false;
    pendingOtaShow = false;
    pendingOtaClear = false;
    pendingZones = false;
    pendingOffClear = false;
    stripNeedsRebuild = false;
    pendingStripReinit = false;
    unlock();
    return s;
}

void applySnapshotMeta(const Snapshot& s) {
    cfg = s.cfg;
    const uint8_t count = s.ledCount ? s.ledCount : WS2812_DEFAULT_COUNT;
    if (!strip) {
        ensureStrip(count);
    } else if (s.reinitStrip || (s.rebuildStrip && strip->numPixels() != count)) {
        // Full recreate only when forced (OTA) or the configured length changed.
        // A plain setConfig rebuild after boot used to delete+new the same-length
        // strip and wedged RP2040 NeoPixel/PIO — chase worked, then status died.
        delete strip;
        strip = nullptr;
        ensureStrip(count);
    }
    // Brightness is applied in update() only when the value changes.

    if (s.doOtaShow && strip) {
        // Keep drawing orange while otaCue is set; do not sticky-change mode.
        strip->clear();
        const uint8_t mid = strip->numPixels() ? (uint8_t)(strip->numPixels() / 2) : 0;
        strip->setPixelColor(mid, strip->Color(80, 30, 0));
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

} // namespace

void begin() {
    mutex_init(&stateMu);
    muReady = true;
    FfbLink::defaultRimConfig(cfg);
    desiredLedCount = cfg.shiftLedCount ? cfg.shiftLedCount : WS2812_DEFAULT_COUNT;
}

void beginCore1() {
    lock();
    uint8_t count = desiredLedCount ? desiredLedCount : WS2812_DEFAULT_COUNT;
    if (count > 60)
        count = WS2812_DEFAULT_COUNT; // EEPROM garbage guard
    desiredLedCount = count;
    const FfbLink::RimConfig localCfg = cfg;
    otaCue = false;
    flashQuiet = false;
    pendingOtaShow = false;
    pendingOtaClear = false;
    stripNeedsRebuild = false;
    pendingStripReinit = false;
    unlock();
    cfg = localCfg;
    ensureStrip(count);
    runBootSequence();
    lock();
    mode = FfbLink::LedModeAuto;
    unlock();
}

void setConfig(const FfbLink::RimConfig& c) {
    lock();
    cfg = c;
    uint8_t count = cfg.shiftLedCount ? cfg.shiftLedCount : WS2812_DEFAULT_COUNT;
    if (count > 60)
        count = WS2812_DEFAULT_COUNT;
    desiredLedCount = count;
    stripNeedsRebuild = true;
    unlock();
}

void setTelemetry(const FfbLink::TelemetryPayload& t) {
    lock();
    tel = t;
    haveTel = true;
    unlock();
}

void clearTelemetry() {
    lock();
    haveTel = false;
    tel = FfbLink::TelemetryPayload{};
    pendingOffClear = true; // wipe last RPM/flags immediately on Core1
    unlock();
}

void setPowerSave(bool on) {
    lock();
    if (powerSave == on) {
        unlock();
        return;
    }
    powerSave = on;
    // No pendingOffClear — Auto mode keeps a dim idle pulse while asleep.
    unlock();
}

void setTest(const FfbLink::ShiftLedPayload& cmd) {
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
    otaCue = true;
    pendingOtaShow = true;
    unlock();
}

void clearOta() {
    lock();
    otaCue = false;
    pendingOtaClear = true;
    mode = FfbLink::LedModeAuto;
    unlock();
}

void setFlashQuiet(bool on) {
    flashQuiet = on;
}

bool flashQuietActive() {
    return flashQuiet;
}

void reinitStrip() {
    lock();
    pendingStripReinit = true;
    otaCue = false;
    mode = FfbLink::LedModeAuto;
    unlock();
}

void update() {
    Snapshot s = takeSnapshot();
    applySnapshotMeta(s);
    if (!strip)
        return;

    // Keep NeoPixel global brightness in sync with LDR auto-dim.
    static uint8_t lastBright = 0;
    const uint8_t bright = stripBright();
    if (bright != lastBright) {
        lastBright = bright;
        strip->setBrightness(bright);
    }

    // One-shot handlers above already drew.
    if (s.doBoot || s.doOtaShow || s.doZones)
        return;

    const uint8_t n = strip->numPixels();
    if (!n)
        return;
    const uint32_t now = millis();
    // Snapshot mode, but OTA-clear already restored Auto in applySnapshotMeta.
    uint8_t drawMode = s.doOtaClear ? FfbLink::LedModeAuto : s.mode;

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
        strip->setPixelColor(head, strip->Color(s.solidR ? s.solidR : 80, s.solidG ? s.solidG : 80,
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
    default: {
        const uint8_t mid = (uint8_t)(n / 2);
        // Link down / never linked — full-strip red blink (connection error).
        if (!s.linkUp) {
            // Force ON for the first ~1.5s so a one-shot update before a stalled
            // peripheral init cannot leave the strip stuck on the blink's off frame.
            const bool forceOn = now < 1500;
            const bool on = forceOn || ((now / 250) & 1);
            const uint32_t red = on ? strip->Color(120, 0, 0) : 0;
            for (uint8_t i = 0; i < n; ++i)
                strip->setPixelColor(i, red);
            strip->show();
            return;
        }
        // Live race data only when something is actually happening. rpm=0 + no
        // flags used to blank the strip (drawDashboard → drawRpmOnly early-out),
        // which looked like "idle effects gone" whenever telemetry was enabled.
        const bool liveTel = s.haveTel && !s.powerSave && (s.tel.rpm != 0 || s.tel.flags != 0);
        if (liveTel) {
            drawDashboard(s.tel.rpm, s.tel.flags);
            return;
        }
        // Linked standby / ADXL power-save — center blue pulse (idle).
        strip->clear();
        const uint16_t period = s.powerSave ? 1000 : 400;
        if ((now / period) & 1) {
            const uint32_t c = s.powerSave ? strip->Color(0, 20, 100) : strip->Color(0, 60, 255);
            strip->setPixelColor(mid, c);
        }
        strip->show();
        return;
    }
    }
}

} // namespace ShiftLeds
