#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "ffb_link.h"
#include "ffb_version.h"
#include "inputs.h"
#include "link.h"
#include "rim_settings.h"
#include "shift_leds.h"
#include "updater.h"

namespace {

FfbLink::TelemetryPayload lastTel{};
uint32_t lastInputMs = 0;

void sendCfgReport() {
    Link::sendMsg(FfbLink::CfgReport, &RimSettings::cconfig(), sizeof(FfbLink::RimConfig));
}

void onFrame(uint8_t type, const uint8_t *payload, uint8_t len) {
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
    if (type == FfbLink::CfgSync && len >= sizeof(FfbLink::RimConfig)) {
        FfbLink::RimConfig cfg{};
        memcpy(&cfg, payload, sizeof(cfg));
        RimSettings::setConfig(cfg);  // live apply, not EEPROM
        sendCfgReport();
        return;
    }
    if (type == FfbLink::CfgSave) {
        const bool ok = RimSettings::save();
        Link::sendMsg(FfbLink::CfgAck, nullptr, 0);
        (void)ok;
        return;
    }
    if (type == FfbLink::Telemetry && len >= sizeof(FfbLink::TelemetryPayload)) {
        memcpy(&lastTel, payload, sizeof(lastTel));
        ShiftLeds::setTelemetry(lastTel);
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
}

}  // namespace

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    // Apply any staged OTA image before other init (may reboot).
    Updater::begin();
    Link::begin();
    Link::setHandler(onFrame);
    Inputs::begin();
    ShiftLeds::begin();
    RimSettings::begin();  // load EEPROM after strip init; re-applies LED config
}

void loop() {
    Link::update();
    Updater::update();
    if (!Updater::active()) {
        Inputs::update();
        ShiftLeds::update();

        const uint32_t now = millis();
        if (now - lastInputMs >= 10) {
            lastInputMs = now;
            FfbLink::InputPayload in{};
            Inputs::fillInput(in);
            Link::sendMsg(FfbLink::Input, &in, sizeof(in));
        }
        digitalWrite(LED_BUILTIN, ((millis() / 500) & 1) ? HIGH : LOW);
        delay(LOOP_PERIOD_MS);
    } else {
        digitalWrite(LED_BUILTIN, ((millis() / 100) & 1) ? HIGH : LOW);
    }
}
