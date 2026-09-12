#include <Arduino.h>
#include <stddef.h>
#include <string.h>

#include "adxl345.h"
#include "config.h"
#include "display.h"
#include "ffb_link.h"
#include "ffb_version.h"
#include "inputs.h"
#include "link.h"
#include "rim_settings.h"
#include "shift_leds.h"
#include "updater.h"

namespace {

FfbLink::TelemetryPayload lastTel{};
uint64_t nextIoUs = 0;
uint64_t lastTelemetryUs = 0;
bool telemetryActive = false;
bool powerSaveLatched = false;

// Arduino-Pico launches Core1 before setup(). Hold it until Updater::begin()
// finishes apply-from-staging (flash writes need Core1 fully idle / not in XIP).
volatile bool gCore1Go = false;

void sendCfgReport() {
    Link::sendMsg(FfbLink::CfgReport, &RimSettings::cconfig(), sizeof(FfbLink::RimConfig));
}

void applyPowerSave(bool on) {
    if (powerSaveLatched == on)
        return;
    powerSaveLatched = on;
    ShiftLeds::setPowerSave(on);
    Display::setPowerSave(on);
}

void enterTelemetryStandby() {
    if (!telemetryActive)
        return;
    telemetryActive = false;
    lastTel = FfbLink::TelemetryPayload{};
    ShiftLeds::clearTelemetry();
    Display::setTelemetryValid(false);
}

void checkTelemetryWatchdog() {
    if (!telemetryActive)
        return;
    if ((time_us_64() - lastTelemetryUs) <= FfbLink::kTelemetryTimeoutUs)
        return;
    enterTelemetryStandby();
}

void handleAccelGet(const uint8_t* payload, uint8_t len) {
    FfbLink::AccelGetPayload req{};
    if (len >= sizeof(req)) {
        memcpy(&req, payload, sizeof(req));
    }
    FfbLink::AccelReportPayload report{};
    if (req.mode == FfbLink::AccelAverage) {
        Adxl345::readAverage(report, req.count);
    } else {
        Adxl345::read(report);
    }
    // Always reply so base can fall back when present=0.
    Link::sendMsg(FfbLink::AccelReport, &report, sizeof(report));
}

void onFrame(uint8_t type, const uint8_t* payload, uint8_t len) {
    if (type == FfbLink::EnterBootloader || type == FfbLink::FwBegin || type == FfbLink::FwData ||
        type == FfbLink::FwEnd) {
        Updater::onFrame(type, payload, len);
        return;
    }
    if (Updater::active()) {
        // Keep link alive so base doesn't look "rim dead" during OTA / idle wait.
        if (type == FfbLink::Ping) {
            Link::sendMsg(FfbLink::Pong, nullptr, 0);
        }
        return;
    }

    if (type == FfbLink::Ping) {
        Link::sendMsg(FfbLink::Pong, nullptr, 0);
        return;
    }
    if (type == FfbLink::CfgGet) {
        sendCfgReport();
        return;
    }
    if (type == FfbLink::VersionGet) {
        char id[FfbVersion::kIdMax];
        FfbVersion::formatId(id, sizeof(id));
        Link::sendMsg(FfbLink::VersionReport, id, (uint8_t)strlen(id));
        return;
    }
    if (type == FfbLink::AccelGet) {
        handleAccelGet(payload, len);
        return;
    }
    if (type == FfbLink::CfgSync && len >= sizeof(FfbLink::RimConfig)) {
        FfbLink::RimConfig cfg{};
        memcpy(&cfg, payload, sizeof(cfg));
        RimSettings::setConfig(cfg); // live apply, not EEPROM
        sendCfgReport();
        return;
    }
    if (type == FfbLink::CfgSave) {
        const bool ok = RimSettings::save();
        Link::sendMsg(FfbLink::CfgAck, nullptr, 0);
        (void)ok;
        return;
    }
    if (type == FfbLink::Telemetry && len >= FfbLink::kTelemetryPayloadCoreSize) {
        lastTel = FfbLink::TelemetryPayload{};
        const uint8_t n = len < sizeof(lastTel) ? len : (uint8_t)sizeof(lastTel);
        memcpy(&lastTel, payload, n);
        lastTelemetryUs = time_us_64();
        telemetryActive = true;
        // Live race data wakes idle power-save.
        if (powerSaveLatched) {
            Adxl345::clearMotion();
            applyPowerSave(false);
        }
        ShiftLeds::setTelemetry(lastTel);
        Display::setTelemetry(lastTel);
        return;
    }
    if (type == FfbLink::ShiftLed && len >= sizeof(FfbLink::ShiftLedPayload)) {
        FfbLink::ShiftLedPayload cmd{};
        memcpy(&cmd, payload, sizeof(cmd));
        ShiftLeds::setTest(cmd);
        return;
    }
    if (type == FfbLink::BtnLed && len >= sizeof(FfbLink::BtnLedPayload)) {
        FfbLink::BtnLedPayload cmd{};
        memcpy(&cmd, payload, sizeof(cmd));
        Inputs::setPanelLeds(cmd.mask);
        return;
    }
    if (type == FfbLink::Display && len >= 1) {
        const uint8_t op = payload[0];
        if (op == FfbLink::DispOpSetMeta && len >= 1 + sizeof(FfbLink::DispMetaPayload)) {
            FfbLink::DispMetaPayload meta{};
            memcpy(&meta, payload + 1, sizeof(meta));
            Display::setStoreMeta(meta);
            return;
        }
        if (op == FfbLink::DispOpSetPageChunk &&
            len >= 1 + offsetof(FfbLink::DispPageChunkPayload, elements)) {
            FfbLink::DispPageChunkPayload chunk{};
            const uint8_t n = len - 1 < sizeof(chunk) ? (uint8_t)(len - 1) : (uint8_t)sizeof(chunk);
            memcpy(&chunk, payload + 1, n);
            Display::setStorePageChunk(chunk);
            return;
        }
        if (op == FfbLink::DispOpSetPage && len >= 1 + sizeof(FfbLink::DispPageSetPayload)) {
            FfbLink::DispPageSetPayload page{};
            memcpy(&page, payload + 1, sizeof(page));
            Display::setStorePageLegacy(page);
            return;
        }
        if (op == FfbLink::DispOpGetAll) {
            FfbLink::DispMetaPayload meta{};
            Display::getStoreMeta(meta);
            uint8_t buf[1 + sizeof(FfbLink::DispMetaPayload)];
            buf[0] = FfbLink::DispOpSetMeta;
            memcpy(buf + 1, &meta, sizeof(meta));
            Link::sendMsg(FfbLink::Display, buf, sizeof(buf));
            for (uint8_t i = 0; i < meta.pageCount && i < FfbLink::kDispPageMax; ++i) {
                uint8_t start = 0;
                for (;;) {
                    FfbLink::DispPageChunkPayload chunk{};
                    if (!Display::fillStorePageChunk(i, start, chunk))
                        break;
                    const uint8_t payloadLen =
                        (uint8_t)(offsetof(FfbLink::DispPageChunkPayload, elements) +
                                  chunk.count * sizeof(FfbLink::DisplayElement));
                    uint8_t pbuf[1 + sizeof(FfbLink::DispPageChunkPayload)];
                    pbuf[0] = FfbLink::DispOpSetPageChunk;
                    memcpy(pbuf + 1, &chunk, payloadLen);
                    Link::sendMsg(FfbLink::Display, pbuf, (uint8_t)(1 + payloadLen));
                    if (chunk.layoutCount == 0)
                        break;
                    start = (uint8_t)(chunk.start + chunk.count);
                    if (start >= chunk.layoutCount)
                        break;
                }
            }
            return;
        }
    }
}

} // namespace

// ============================================================================
// CORE 0 — deterministic I/O + UART @ 500 Hz
// ============================================================================
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    // Apply any staged OTA image before other init (may reboot).
    // Must run before Core1 is released — see gCore1Go.
    Updater::begin();
    ShiftLeds::begin(); // mutex + defaults only; strip lives on Core1
    Display::begin();
    Link::begin();
    Link::setHandler(onFrame);
    Inputs::begin();
    RimSettings::begin(); // load EEPROM; queues LED config for Core1
    nextIoUs = time_us_64();
    gCore1Go = true;
}

void loop() {
    Link::update();
    Updater::update();

    if (!Updater::active()) {
        checkTelemetryWatchdog();
        Inputs::update();
        Adxl345::update();
        if (Adxl345::present()) {
            if (Adxl345::consumeWakeEdge())
                applyPowerSave(false);
            applyPowerSave(Adxl345::powerSaveActive());
        } else {
            applyPowerSave(false);
        }

        FfbLink::InputPayload in{};
        Inputs::fillInput(in);
        Link::sendMsg(FfbLink::Input, &in, sizeof(in));

        digitalWrite(LED_BUILTIN, ((millis() / 500) & 1) ? HIGH : LOW);

        // Precise 500 Hz guard (2 ms). Overrun → resync to now.
        nextIoUs += FfbLink::kRimIoPeriodUs;
        const int64_t sleepUs = (int64_t)nextIoUs - (int64_t)time_us_64();
        if (sleepUs > 0) {
            delayMicroseconds((uint32_t)sleepUs);
        } else {
            nextIoUs = time_us_64();
        }
    } else {
        digitalWrite(LED_BUILTIN, ((millis() / 100) & 1) ? HIGH : LOW);
        // OTA: drain UART as fast as possible (no 2 ms pacing).
    }
}

// ============================================================================
// CORE 1 — WS2812 + ILI9341 dashboard (~60 FPS)
// ============================================================================
void setup1() {
    while (!gCore1Go) {
        tight_loop_contents();
    }
    ShiftLeds::beginCore1();
    Display::beginCore1();
}

void loop1() {
    ShiftLeds::update();
    Display::update();
    delay(16); // ~60 FPS target
}
