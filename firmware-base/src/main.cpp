#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "accessory_link.h"
#include "axle_index.h"
#include "config.h"
#include "control_tick.h"
#include "epd_status.h"
#include "ffb.h"
#include "ffb_version.h"
#include "hid_wheel.h"
#include "homing.h"
#include "mlx90363.h"
#include "motor_bts7960.h"
#include "pedals.h"
#include "settings.h"
#include "status_leds.h"
#include "wheel_encoder.h"

namespace {

uint32_t lastPrintMs = 0;

constexpr size_t kLineMax = 128;
char lineBuf[kLineMax];
size_t lineLen = 0;

const char *ffbModeName(Ffb::Mode m) {
    switch (m) {
        case Ffb::Mode::Off: return "off";
        case Ffb::Mode::Manual: return "manual";
        case Ffb::Mode::Spring: return "spring";
    }
    return "?";
}

void printHelp() {
    Serial.println();
    Serial.println("rp2040-ffb base-mcu: boot INIT  n=cancel/retry  z=set center NOW  i=index-sync  c=gear");
    Serial.println("            p=pedal-reset  e/d=motors  s/m/x=ffb  [/]=torque  h=help");
    Serial.println("cfg: :get|:set <key> <v>  :dump  :save  :load  :defaults  :log 0|1  :bootsel  :version");
    Serial.println("profile: :profile list|load N|save N [name]|name N <str>|clear N");
    Serial.println("rim: :rim_reset  :rim_bootsel  :rim_updater  :rim_sync (pull)  :rim_save");
    Serial.println("leds: :leds_off|:leds_auto|:leds_chase|:leds_rainbow|:leds_boot");
    Serial.println("      :leds_solid <r> <g> <b>  :leds_fill <n> <r> <g> <b>");
    Serial.println("      :leds_rpm <rpm> [flags]  :leds_zones  :leds_flags <mask>");
    Serial.println("      :btnleds <mask>  :btnleds auto 0|1  (panel LED follow)");
    Serial.println("      flags bits: 1=yellow 2=blue 4=TC 8=ABS 16=red 32=pit");
    Serial.println("epd:  :epd  (full redraw)  :epd 0|1  (disable/enable auto)");
    Serial.println("keys: duty_cap spring_k spring_dz torque_cap hid_range gear_ratio");
    Serial.println("      encN_* panel/shift/disp/shift_rpm_*/layout_hex on rim EEPROM; rim_link");
}

void handleSingleChar(char c) {
    if (c == 'h' || c == 'H' || c == '?') {
        printHelp();
    } else if (c == 'n' || c == 'N') {
        Homing::startOrCancel();
    } else if (c == 'z' || c == 'Z') {
        WheelEncoder::zeroHere();
        Serial.println("Wheel zeroed");
    } else if (c == 'i' || c == 'I') {
        AxleIndex::armSyncOnNextEdge();
        ControlTick::armIndexSyncLatch();
        Serial.print("Armed: next index → sync axle≈");
        Serial.println(AXLE_INDEX_ANGLE_DEG, 1);
    } else if (c == 'c' || c == 'C') {
        if (!WheelEncoder::calibrating()) {
            WheelEncoder::startCal();
            Serial.println("Gear CAL: turn axle +360 deg, press c again");
        } else if (WheelEncoder::finishCal()) {
            Serial.print("Gear CAL ok, ratio=");
            Serial.println(WheelEncoder::gearRatio(), 4);
        } else {
            Serial.println("Gear CAL failed (move more)");
        }
    } else if (c == 'p' || c == 'P') {
        Pedals::resetCalibration();
        Serial.println("Pedal CAL reset — HID=0 until each pedal is pressed once");
    } else if (c == 'e' || c == 'E') {
        MotorBts7960::setEnabled(true);
        Serial.println("Motors ENABLED");
    } else if (c == 'd' || c == 'D') {
        MotorBts7960::stop();
        Ffb::setMode(Ffb::Mode::Off);
        Serial.println("Motors DISABLED");
    } else if (c == 's' || c == 'S') {
        Ffb::setMode(Ffb::Mode::Spring);
        Serial.println("FFB spring");
    } else if (c == 'm' || c == 'M') {
        Ffb::setMode(Ffb::Mode::Manual);
        Serial.println("FFB manual");
    } else if (c == 'x' || c == 'X') {
        Ffb::setMode(Ffb::Mode::Off);
        MotorBts7960::coast();
        Serial.println("FFB off");
    } else if (c == '[') {
        Ffb::setManualTorque(Ffb::manualTorque() - 0.05f);
        Ffb::setMode(Ffb::Mode::Manual);
        Serial.print("manual torq=");
        Serial.println(Ffb::manualTorque(), 2);
    } else if (c == ']') {
        Ffb::setManualTorque(Ffb::manualTorque() + 0.05f);
        Ffb::setMode(Ffb::Mode::Manual);
        Serial.print("manual torq=");
        Serial.println(Ffb::manualTorque(), 2);
    }
}

void handleLine(char *line) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t')) {
        line[--n] = '\0';
    }
    while (*line == ' ' || *line == '\t') ++line;
    if (*line == '\0') return;

    if (*line == ':') ++line;
    while (*line == ' ' || *line == '\t') ++line;
    if (*line == '\0') return;

    char *save = nullptr;
    char *cmd = strtok_r(line, " \t", &save);
    if (!cmd) return;

    if (strcasecmp(cmd, "get") == 0) {
        char *key = strtok_r(nullptr, " \t", &save);
        if (!key) {
            Serial.println("ERR unknown key");
            return;
        }
        if (strcasecmp(key, "layout_hex") == 0) {
            char out[1 + 2 * FfbLink::kLayoutBlobSize];
            if (!Settings::getLayoutHex(out, sizeof(out))) {
                Serial.println("ERR layout_hex");
                return;
            }
            Serial.print("OK layout_hex=");
            Serial.println(out);
            return;
        }
        float v = 0;
        if (!Settings::getFloat(key, v)) {
            Serial.println("ERR unknown key");
            return;
        }
        Serial.print("OK ");
        Serial.print(key);
        Serial.print('=');
        Serial.println(v, 6);
    } else if (strcasecmp(cmd, "set") == 0) {
        char *key = strtok_r(nullptr, " \t", &save);
        char *val = strtok_r(nullptr, " \t", &save);
        if (!key || !val) {
            Serial.println("ERR usage: set <key> <value>");
            return;
        }
        if (strcasecmp(key, "layout_hex") == 0) {
            if (!Settings::setLayoutHex(val)) {
                Serial.println("ERR bad layout_hex");
                return;
            }
            char out[1 + 2 * FfbLink::kLayoutBlobSize];
            Settings::getLayoutHex(out, sizeof(out));
            Serial.print("OK layout_hex=");
            Serial.println(out);
            return;
        }
        if (strncasecmp(key, "layout_page", 11) == 0 && key[11] >= '0' && key[11] <= '2' &&
            strcmp(key + 12, "_hex") == 0) {
            const uint8_t page = (uint8_t)(key[11] - '0');
            const uint8_t bg = AccessoryLink::dispPage(page).bgTheme;
            if (!Settings::setLayoutPageHex(page, bg, val)) {
                Serial.println("ERR bad layout_page_hex");
                return;
            }
            Serial.print("OK ");
            Serial.print(key);
            Serial.print('=');
            Serial.println(val);
            return;
        }
        if (strncasecmp(key, "page", 4) == 0 && key[4] >= '0' && key[4] <= '2' &&
            strcmp(key + 5, "_bg") == 0) {
            const uint8_t page = (uint8_t)(key[4] - '0');
            const uint8_t bg = (uint8_t)atoi(val);
            char hex[1 + 2 * FfbLink::kLayoutBlobSize];
            if (!Settings::getLayoutPageHex(page, hex, sizeof(hex)) ||
                !Settings::setLayoutPageHex(page, bg, hex)) {
                Serial.println("ERR page_bg");
                return;
            }
            Serial.print("OK page");
            Serial.print(page);
            Serial.print("_bg=");
            Serial.println(bg);
            return;
        }
        if (strcasecmp(key, "disp_page") == 0) {
            if (!AccessoryLink::setDispActivePage((uint8_t)atoi(val))) {
                Serial.println("ERR disp_page");
                return;
            }
            Serial.print("OK disp_page=");
            Serial.println(AccessoryLink::dispMeta().activePage);
            return;
        }
        if (strcasecmp(key, "disp_pages") == 0) {
            if (!AccessoryLink::setDispPageCount((uint8_t)atoi(val))) {
                Serial.println("ERR disp_pages");
                return;
            }
            Serial.print("OK disp_pages=");
            Serial.println(AccessoryLink::dispMeta().pageCount);
            return;
        }
        char *end = nullptr;
        float v = strtof(val, &end);
        if (end == val) {
            Serial.println("ERR bad value");
            return;
        }
        if (!Settings::setFloat(key, v)) {
            Serial.println("ERR unknown key or out of range");
            return;
        }
        float out = 0;
        Settings::getFloat(key, out);
        Serial.print("OK ");
        Serial.print(key);
        Serial.print('=');
        Serial.println(out, 6);
    } else if (strcasecmp(cmd, "dump") == 0) {
        Settings::dumpToSerial();
    } else if (strcasecmp(cmd, "save") == 0) {
        Serial.println(Settings::save() ? "OK saved" : "ERR save failed");
    } else if (strcasecmp(cmd, "load") == 0) {
        if (!Settings::load()) {
            Serial.println("ERR no settings in flash");
            return;
        }
        Settings::apply();
        Settings::dumpToSerial();
    } else if (strcasecmp(cmd, "defaults") == 0) {
        Settings::resetDefaults();
        Settings::apply();
        Settings::dumpToSerial();
    } else if (strcasecmp(cmd, "profile") == 0 || strcasecmp(cmd, "profiles") == 0) {
        Settings::handleProfileCmd(save);
    } else if (strcasecmp(cmd, "disp") == 0 || strcasecmp(cmd, "display") == 0) {
        Settings::handleDispCmd(save);
    } else if (strcasecmp(cmd, "recenter") == 0 || strcasecmp(cmd, "zero") == 0) {
        WheelEncoder::zeroHere();
        Serial.println("OK recenter");
    } else if (strcasecmp(cmd, "adxl_cal") == 0 || strcasecmp(cmd, "calibrate_adxl") == 0) {
        if (!AccessoryLink::linked()) {
            Serial.println("ERR adxl_cal: rim not linked");
            return;
        }
        const uint32_t beforeMs = AccessoryLink::lastAccelMs();
        if (!AccessoryLink::requestAccel(FfbLink::AccelAverage, 100)) {
            Serial.println("ERR adxl_cal: send failed");
            return;
        }
        bool got = false;
        const uint32_t t0 = millis();
        while ((millis() - t0) < 800) {
            AccessoryLink::update();
            if (AccessoryLink::lastAccelMs() != beforeMs) {
                got = true;
                break;
            }
            delay(5);
        }
        const auto &r = AccessoryLink::lastAccel();
        if (!got || !r.present) {
            Serial.println("ERR adxl_cal: no ADXL on rim");
            return;
        }
        if (!r.ok || r.samples < 10) {
            Serial.println("ERR adxl_cal: read failed");
            return;
        }
        Settings::data().adxlXOffset = r.ax;
        Settings::data().adxlCalValid = 1;
        Serial.print("OK adxl_cal ax=");
        Serial.print(r.ax);
        Serial.print(" ay=");
        Serial.print(r.ay);
        Serial.print(" az=");
        Serial.print(r.az);
        Serial.print(" n=");
        Serial.print(r.samples);
        Serial.println(" (use :save to persist)");
    } else if (strcasecmp(cmd, "adxl") == 0) {
        AccessoryLink::requestAccel(FfbLink::AccelOnce, 0);
        for (int i = 0; i < 40; ++i) {
            AccessoryLink::update();
            delay(5);
        }
        const auto &r = AccessoryLink::lastAccel();
        Serial.print("OK adxl present=");
        Serial.print(r.present ? 1 : 0);
        Serial.print(" ok=");
        Serial.print(r.ok ? 1 : 0);
        Serial.print(" ax=");
        Serial.print(r.ax);
        Serial.print(" ay=");
        Serial.print(r.ay);
        Serial.print(" az=");
        Serial.print(r.az);
        Serial.print(" cal=");
        Serial.print(Settings::cdata().adxlCalValid ? 1 : 0);
        Serial.print(" off=");
        Serial.println(Settings::cdata().adxlXOffset);
    } else if (strcasecmp(cmd, "log") == 0) {
        char *val = strtok_r(nullptr, " \t", &save);
        if (!val) {
            Serial.print("OK log=");
            Serial.println(Settings::telemetryEnabled() ? 1 : 0);
            return;
        }
        const bool on = (val[0] != '0');
        Settings::setTelemetryEnabled(on);
        Serial.print("OK log=");
        Serial.println(on ? 1 : 0);
    } else if (strcasecmp(cmd, "version") == 0 || strcasecmp(cmd, "ver") == 0) {
        char baseId[FfbVersion::kIdMax];
        FfbVersion::formatId(baseId, sizeof(baseId));
        AccessoryLink::requestRimVersion();
        for (int i = 0; i < 40; ++i) {
            AccessoryLink::update();
            delay(5);
        }
        Serial.print("OK base_fw=");
        Serial.println(baseId);
        Serial.print("OK rim_fw=");
        Serial.println(AccessoryLink::rimFwId()[0] ? AccessoryLink::rimFwId() : "?");
    } else if (strcasecmp(cmd, "rim_reset") == 0) {
        AccessoryLink::rimResetPulse();
        Serial.println("OK rim_reset");
    } else if (strcasecmp(cmd, "rim_bootsel") == 0) {
        AccessoryLink::rimEnterUsbBootloader();
        Serial.println("OK rim_bootsel");
    } else if (strcasecmp(cmd, "bootsel") == 0) {
        // Reboot base into USB UF2 bootloader (for GUI / host updates).
        AccessoryLink::clearCdcSession();
        Serial.println("OK bootsel");
        Serial.flush();
        StatusLeds::showUpdateBrief();
        delay(250);
        rp2040.rebootToBootloader();
    } else if (strcasecmp(cmd, "rim_updater") == 0) {
        Serial.println(AccessoryLink::requestRimEnterUpdater() ? "OK rim_updater" : "ERR send");
    } else if (strcasecmp(cmd, "rim_sync") == 0) {
        AccessoryLink::requestRimConfig();
        Serial.println("OK rim_sync");
    } else if (strcasecmp(cmd, "rim_save") == 0) {
        AccessoryLink::saveRimConfig();
        Serial.println("OK rim_save");
    } else if (strcasecmp(cmd, "epd") == 0) {
        char *val = strtok_r(nullptr, " \t", &save);
        if (!val) {
            EpdStatus::forceRefresh();
            Serial.println("OK epd refresh");
            return;
        }
        if (val[0] == '0' || strcasecmp(val, "off") == 0) {
            EpdStatus::setEnabled(false);
            Serial.println("OK epd=0");
        } else if (val[0] == '1' || strcasecmp(val, "on") == 0) {
            EpdStatus::setEnabled(true);
            Serial.println("OK epd=1");
        } else {
            Serial.println("ERR usage: epd | epd 0|1");
        }
    } else if (strcasecmp(cmd, "leds_off") == 0) {
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeOff;
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_off" : "ERR send");
    } else if (strcasecmp(cmd, "leds_auto") == 0) {
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeAuto;
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_auto" : "ERR send");
    } else if (strcasecmp(cmd, "leds_chase") == 0) {
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeChase;
        p.r = p.g = p.b = 80;
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_chase" : "ERR send");
    } else if (strcasecmp(cmd, "leds_rainbow") == 0) {
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeRainbow;
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_rainbow" : "ERR send");
    } else if (strcasecmp(cmd, "leds_boot") == 0) {
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeBoot;
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_boot" : "ERR send");
    } else if (strcasecmp(cmd, "leds_solid") == 0) {
        char *rs = strtok_r(nullptr, " \t", &save);
        char *gs = strtok_r(nullptr, " \t", &save);
        char *bs = strtok_r(nullptr, " \t", &save);
        if (!rs || !gs || !bs) {
            Serial.println("ERR usage: leds_solid <r> <g> <b>");
            return;
        }
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeSolid;
        p.r = (uint8_t)atoi(rs);
        p.g = (uint8_t)atoi(gs);
        p.b = (uint8_t)atoi(bs);
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_solid" : "ERR send");
    } else if (strcasecmp(cmd, "leds_fill") == 0) {
        char *ns = strtok_r(nullptr, " \t", &save);
        char *rs = strtok_r(nullptr, " \t", &save);
        char *gs = strtok_r(nullptr, " \t", &save);
        char *bs = strtok_r(nullptr, " \t", &save);
        if (!ns || !rs || !gs || !bs) {
            Serial.println("ERR usage: leds_fill <n> <r> <g> <b>");
            return;
        }
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeFill;
        p.param = (uint8_t)atoi(ns);
        p.r = (uint8_t)atoi(rs);
        p.g = (uint8_t)atoi(gs);
        p.b = (uint8_t)atoi(bs);
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_fill" : "ERR send");
    } else if (strcasecmp(cmd, "leds_rpm") == 0) {
        char *rs = strtok_r(nullptr, " \t", &save);
        char *fs = strtok_r(nullptr, " \t", &save);
        if (!rs) {
            Serial.println("ERR usage: leds_rpm <rpm> [flags]");
            return;
        }
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeRpm;
        p.rpm = (uint16_t)atoi(rs);
        p.flags = fs ? (uint8_t)strtoul(fs, nullptr, 0) : 0;
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_rpm" : "ERR send");
    } else if (strcasecmp(cmd, "leds_zones") == 0) {
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeZones;
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_zones" : "ERR send");
    } else if (strcasecmp(cmd, "leds_flags") == 0) {
        char *fs = strtok_r(nullptr, " \t", &save);
        if (!fs) {
            Serial.println("ERR usage: leds_flags <mask>");
            return;
        }
        FfbLink::ShiftLedPayload p{};
        p.mode = FfbLink::LedModeRpm;
        p.rpm = 0;
        p.flags = (uint8_t)strtoul(fs, nullptr, 0);
        Serial.println(AccessoryLink::sendShiftLed(p) ? "OK leds_flags" : "ERR send");
    } else if (strcasecmp(cmd, "btnleds") == 0) {
        char *ms = strtok_r(nullptr, " \t", &save);
        if (!ms) {
            Serial.println("ERR usage: btnleds <mask>|auto 0|1");
            return;
        }
        if (strcasecmp(ms, "auto") == 0) {
            char *v = strtok_r(nullptr, " \t", &save);
            const bool on = !v || v[0] != '0';
            AccessoryLink::setBtnLedFollow(on);
            Serial.print("OK btnleds_auto=");
            Serial.println(on ? 1 : 0);
            return;
        }
        const uint16_t mask = (uint16_t)strtoul(ms, nullptr, 0);
        Serial.println(AccessoryLink::sendBtnLed(mask) ? "OK btnleds" : "ERR send");
    } else if (strcasecmp(cmd, "tel") == 0 || strcasecmp(cmd, "telemetry") == 0) {
        char *rs = strtok_r(nullptr, " \t", &save);
        char *gs = strtok_r(nullptr, " \t", &save);
        char *fs = strtok_r(nullptr, " \t", &save);
        if (!rs) {
            Serial.println("ERR usage: tel <rpm> [gear] [flags]");
            return;
        }
        FfbLink::TelemetryPayload t{};
        t.rpm = (uint16_t)atoi(rs);
        t.gear = gs ? (int8_t)atoi(gs) : 0;
        t.flags = fs ? (uint8_t)strtoul(fs, nullptr, 0) : 0;
        Serial.println(AccessoryLink::sendTelemetry(t) ? "OK tel" : "ERR send");
    } else if (strcasecmp(cmd, "companion") == 0) {
        char *v = strtok_r(nullptr, " \t", &save);
        if (!v) {
            Serial.print("OK companion=");
            Serial.println(Settings::telemetryEnabled() ? 0 : 1);
            return;
        }
        const bool on = v[0] != '0';
        // Companion mode: quiet verbose T lines so SimHub/serial plugins own the port.
        Settings::setTelemetryEnabled(!on);
        Serial.print("OK companion=");
        Serial.println(on ? 1 : 0);
    } else if (strcasecmp(cmd, "selftest") == 0) {
        Serial.println("OK selftest");
        Serial.print("hall=");
        Serial.println(ControlTick::lastHallOk() ? "ok" : "FAIL");
        Serial.print("axle=");
        Serial.println(ControlTick::lastAxleDeg(), 2);
        Serial.print("index=");
        Serial.println(AxleIndex::active() ? "active" : "idle");
        Serial.print("edges=");
        Serial.println(AxleIndex::edgeCount());
        Serial.print("rim=");
        Serial.println(AccessoryLink::linked() ? "ok" : "FAIL");
        Serial.print("pedals=");
        Serial.println(Pedals::calibrationReady() ? "cal_ok" : "need_press");
        Serial.print("motors=");
        Serial.println(MotorBts7960::enabled() ? "enabled" : "disabled");
        Serial.print("hid_range=");
        Serial.println(HidWheel::rangeDeg(), 1);
        Serial.print("soft_limit=");
        Serial.println(Ffb::softLimitEnabled() ? "on" : "off");
        Serial.println("OK end");
    } else {
        Serial.println("ERR unknown cmd (try h)");
    }
}

void handleSerial() {
    while (Serial.available()) {
        const char c = (char)Serial.read();
        if (lineLen == 0 && AccessoryLink::feedCdcByte((uint8_t)c)) {
            continue;
        }
        if (AccessoryLink::cdcForwardActive() && lineLen == 0) {
            if (AccessoryLink::feedCdcByte((uint8_t)c)) continue;
        }

        if (c == '\n' || c == '\r') {
            if (lineLen == 0) continue;
            lineBuf[lineLen] = '\0';
            handleLine(lineBuf);
            lineLen = 0;
            continue;
        }
        if (lineLen == 0) {
            if (c == ':') {
                lineBuf[lineLen++] = c;
                continue;
            }
            if (c == ' ' || c == '\t') continue;
            handleSingleChar(c);
            continue;
        }
        if (lineLen + 1 < kLineMax) {
            lineBuf[lineLen++] = c;
        } else {
            lineLen = 0;
            Serial.println("ERR line too long");
        }
    }
}

}  // namespace

void setup() {
#if ENABLE_USB_HID
    HidWheel::begin();
    delay(1500);
#endif

    Serial.begin(115200);
    Serial.ignoreFlowControl(true);
    delay(500);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.println();
    Serial.println("boot");
    {
        char id[FfbVersion::kIdMax];
        FfbVersion::formatId(id, sizeof(id));
        Serial.print("base_fw=");
        Serial.println(id);
    }

    Serial.println("init mlx...");
    Mlx90363::begin();
    Serial.println("mlx ok");

    AccessoryLink::begin();
    Settings::begin();
    WheelEncoder::begin(DEFAULT_GEAR_RATIO);
    AxleIndex::begin();
    Homing::begin();
    Pedals::begin();
    MotorBts7960::begin();
    Ffb::begin();
    StatusLeds::begin();
    ControlTick::begin(handleSerial);
    EpdStatus::begin();

    if (Settings::load()) {
        Settings::apply();
        Serial.println("settings: loaded from flash");
    } else {
        Serial.println("settings: using defaults");
        Settings::apply();
    }

    Serial.println(ENABLE_USB_HID ? "base-mcu — HID on, serial log" : "base-mcu — HID off, serial log");
    printHelp();
    Homing::start();
}

#if ENABLE_BASE_EPD
void setup1() {
    // Panel + PIO SPI live entirely on core1 so core0 never blocks on EPD.
    EpdStatus::beginCore1();
}

void loop1() {
    EpdStatus::serviceCore1();
}
#endif

void loop() {
    ControlTick::service();
    EpdStatus::update(ControlTick::lastHallOk(), ControlTick::lastAxleDeg());

    const uint32_t now = millis();
    if (Settings::telemetryEnabled() && now - lastPrintMs >= 100) {
        lastPrintMs = now;

        const Pedals::State &ped = ControlTick::lastPedals();
        Pedals::AxisCal ct, cb, cc;
        Pedals::getCalibration(ct, cb, cc);

        Serial.print("T axle=");
        Serial.print(ControlTick::lastAxleDeg(), 2);
        Serial.print(" hidX=");
        Serial.print(HidWheel::lastSteeringHid());
        Serial.print(" sens=");
        Serial.print(ControlTick::lastSensorDeg(), 2);
        Serial.print(" hall=");
        Serial.print(ControlTick::lastHallOk() ? 1 : 0);
        Serial.print(" idx=");
        Serial.print(AxleIndex::active() ? 1 : 0);
        Serial.print(" idxH=");
        Serial.print(AxleIndex::rawHigh() ? 1 : 0);
        Serial.print(" edges=");
        Serial.print(AxleIndex::edgeCount());
        Serial.print(" home=");
        Serial.print(Homing::active() ? Homing::phaseName() : "-");
        Serial.print(" homeM=");
        Serial.print(Homing::usingMotors() ? 1 : 0);
        Serial.print(" adxl=");
        Serial.print(AccessoryLink::adxlPresent() ? 1 : 0);
        if (AccessoryLink::lastAccel().ok) {
            Serial.print(" ax=");
            Serial.print(AccessoryLink::adxlCalibratedX(Settings::cdata().adxlCalValid
                                                            ? Settings::cdata().adxlXOffset
                                                            : (int16_t)0));
        }
        Serial.print(" gear=");
        Serial.print(WheelEncoder::gearRatio(), 4);
        Serial.print(" motors=");
        Serial.print(MotorBts7960::enabled() ? 1 : 0);
        Serial.print(" ffb=");
        Serial.print(ffbModeName(Ffb::mode()));
        Serial.print(" torq=");
        Serial.print(Ffb::commandedTorque(), 3);
        Serial.print(" hidRange=");
        Serial.print(HidWheel::rangeDeg(), 1);
        Serial.print(" rim=");
        Serial.print(AccessoryLink::linked() ? 1 : 0);
        Serial.print(" btns=");
        Serial.print(AccessoryLink::hidButtons(), HEX);
        for (uint8_t ei = 0; ei < FfbLink::kEncoderCount; ++ei) {
            Serial.print(" enc");
            Serial.print(ei);
            Serial.print('=');
            Serial.print(AccessoryLink::encoderAbs(ei));
        }
        Serial.print(" adcT=");
        Serial.print(ped.rawThrottle);
        Serial.print(" adcB=");
        Serial.print(ped.rawBrake);
        Serial.print(" adcC=");
        Serial.print(ped.rawClutch);
        Serial.print(" nT=");
        Serial.print(ped.throttle, 3);
        Serial.print(" nB=");
        Serial.print(ped.brake, 3);
        Serial.print(" nC=");
        Serial.print(ped.clutch, 3);
        Serial.print(" tMin=");
        Serial.print(ct.minV);
        Serial.print(" tMax=");
        Serial.print(ct.maxV);
        Serial.print(" tRest=");
        Serial.print(ct.restV);
        Serial.print(" tArm=");
        Serial.print(ct.armed);
        Serial.print(" bMin=");
        Serial.print(cb.minV);
        Serial.print(" bMax=");
        Serial.print(cb.maxV);
        Serial.print(" bRest=");
        Serial.print(cb.restV);
        Serial.print(" bArm=");
        Serial.print(cb.armed);
        Serial.print(" cMin=");
        Serial.print(cc.minV);
        Serial.print(" cMax=");
        Serial.print(cc.maxV);
        Serial.print(" cRest=");
        Serial.print(cc.restV);
        Serial.print(" cArm=");
        Serial.print(cc.armed);
        Serial.println();
    }

    delay(LOOP_PERIOD_MS);
}
