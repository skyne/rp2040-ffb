#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "accessory_link.h"
#include "axle_index.h"
#include "config.h"
#include "control_tick.h"
#include "epd_status.h"
#include "ffb.h"
#include "ffb_pid.h"
#include "ffb_version.h"
#include "hid_wheel.h"
#include "homing.h"
#include "mlx90363.h"
#include "motor_driver.h"
#include "pedals.h"
#include "safety.h"
#include "settings.h"
#include "setup_msc.h"
#include "status_leds.h"
#include "wheel_encoder.h"

namespace {

uint32_t lastPrintMs = 0;
uint32_t lastStageMs = 0;
bool loggedHidReady_ = false;
bool loggedRimUp_ = false;

constexpr size_t kLineMax = 128;
char lineBuf[kLineMax];
size_t lineLen = 0;

const char* ffbModeName(Ffb::Mode m) {
    switch (m) {
    case Ffb::Mode::Off:
        return "off";
    case Ffb::Mode::Manual:
        return "manual";
    case Ffb::Mode::Spring:
        return "spring";
    case Ffb::Mode::Track:
        return "track";
    case Ffb::Mode::Pid:
        return "pid";
    }
    return "?";
}

bool tryEnableMotors() {
    if (!Safety::gMotorWatchdog.isMotorSafe() && Safety::gMotorWatchdog.driverFaultActive()) {
        Safety::gMotorWatchdog.clearDriverFault();
    }
    if (!Safety::gMotorWatchdog.isMotorSafe()) {
        if (Safety::gMotorWatchdog.driverFaultActive()) {
            Serial.println("ERR motor fault active — fix cause, then :motor_fault_clear or e");
        } else if (Safety::gMotorWatchdog.needsCooldown()) {
            Serial.println("ERR motor cooldown active — wait before enabling");
        } else {
            Serial.println("ERR motors not safe to enable");
        }
        return false;
    }
    MotorDriver::setEnabled(true);
    Safety::gMotorWatchdog.notifyMotorEnabled();
    Serial.println("OK motors_on");
    return true;
}

void printHelp() {
    Serial.println();
    Serial.println(
        "rp2040-ffb base-mcu: boot INIT  n=cancel/retry  z=set center NOW  i=index-sync  c=gear");
    Serial.println("            p=pedal-reset  e/d=motors  s/m/f/x=ffb  [/]=torque  h=help");
    Serial.println(
        "cfg: :get|:set <key> <v>  :dump  :save  :load  :defaults  :log 0|1  :bootsel  :version");
    Serial.println("profile: :profile list|load N|save N [name]|name N <str>|clear N");
    Serial.println("rim: :rim_reset  :rim_bootsel  :rim_updater  :rim_sync (pull)  :rim_save");
    Serial.println("leds: :leds_off|:leds_auto|:leds_chase|:leds_rainbow|:leds_boot");
    Serial.println("      :leds_solid <r> <g> <b>  :leds_fill <n> <r> <g> <b>");
    Serial.println("      :leds_rpm <rpm> [flags]  :leds_zones  :leds_flags <mask>");
    Serial.println("      :btnleds <mask>  :btnleds auto 0|1  (panel LED follow)");
    Serial.println("      flags bits: 1=yellow 2=blue 4=TC 8=ABS 16=red 32=pit");
    Serial.println(
        "motor: :e|:d  (enable/disable)  :m|:s|:f|:x|:track  (manual/spring/pid/off/track)");
    Serial.println(
        "      :ffb_target <deg>  (Track)  :pid_arm|:pid_spring|:pid_rumble|:pid_kick|:pid_end");
    Serial.println("      :pid_status  :n|:home  :motor_fault_clear");
    Serial.println("      (T lines are telemetry — use :track not :t)");
#if ENABLE_BASE_EPD
    Serial.println("epd:  :epd  (full redraw)  :epd 0|1  (disable/enable auto)");
#endif
    Serial.println("keys: duty_cap spring_k spring_dz torque_cap ffb_gain hid_range gear_ratio");
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
        tryEnableMotors();
    } else if (c == 'd' || c == 'D') {
        MotorDriver::stop();
        Ffb::setMode(Ffb::Mode::Off);
        Safety::gMotorWatchdog.notifyMotorDisabled();
        Serial.println("OK motors_off");
    } else if (c == 's' || c == 'S') {
        Ffb::setMode(Ffb::Mode::Spring);
        Serial.println("OK ffb=spring");
    } else if (c == 'm' || c == 'M') {
        Ffb::setMode(Ffb::Mode::Manual);
        Serial.println("OK ffb=manual");
    } else if (c == 'f' || c == 'F') {
        Ffb::setMode(Ffb::Mode::Pid);
        Serial.println("OK ffb=pid");
    } else if (c == 'x' || c == 'X') {
        Ffb::setMode(Ffb::Mode::Off);
        MotorDriver::coast();
        Serial.println("OK ffb=off");
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

void handleLine(char* line) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t')) {
        line[--n] = '\0';
    }
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '\0')
        return;

    // Any CDC colon-command means a host tool (ffb-config) is talking — keep/hide MSC.
    SetupMsc::notifyHostApp();

    if (*line == ':')
        ++line;
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '\0')
        return;

    char* save = nullptr;
    char* cmd = strtok_r(line, " \t", &save);
    if (!cmd)
        return;

    if (strcasecmp(cmd, "get") == 0) {
        char* key = strtok_r(nullptr, " \t", &save);
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
        char* key = strtok_r(nullptr, " \t", &save);
        char* val = strtok_r(nullptr, " \t", &save);
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
        char* end = nullptr;
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
        const auto& r = AccessoryLink::lastAccel();
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
        const auto& r = AccessoryLink::lastAccel();
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
        char* val = strtok_r(nullptr, " \t", &save);
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
#if ENABLE_BASE_EPD
    } else if (strcasecmp(cmd, "epd") == 0) {
        char* val = strtok_r(nullptr, " \t", &save);
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
#endif
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
        char* rs = strtok_r(nullptr, " \t", &save);
        char* gs = strtok_r(nullptr, " \t", &save);
        char* bs = strtok_r(nullptr, " \t", &save);
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
        char* ns = strtok_r(nullptr, " \t", &save);
        char* rs = strtok_r(nullptr, " \t", &save);
        char* gs = strtok_r(nullptr, " \t", &save);
        char* bs = strtok_r(nullptr, " \t", &save);
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
        char* rs = strtok_r(nullptr, " \t", &save);
        char* fs = strtok_r(nullptr, " \t", &save);
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
        char* fs = strtok_r(nullptr, " \t", &save);
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
        char* ms = strtok_r(nullptr, " \t", &save);
        if (!ms) {
            Serial.println("ERR usage: btnleds <mask>|auto 0|1");
            return;
        }
        if (strcasecmp(ms, "auto") == 0) {
            char* v = strtok_r(nullptr, " \t", &save);
            const bool on = !v || v[0] != '0';
            AccessoryLink::setBtnLedFollow(on);
            Serial.print("OK btnleds_auto=");
            Serial.println(on ? 1 : 0);
            return;
        }
        const uint16_t mask = (uint16_t)strtoul(ms, nullptr, 0);
        Serial.println(AccessoryLink::sendBtnLed(mask) ? "OK btnleds" : "ERR send");
    } else if (strcasecmp(cmd, "tel") == 0 || strcasecmp(cmd, "telemetry") == 0) {
        char* rs = strtok_r(nullptr, " \t", &save);
        char* gs = strtok_r(nullptr, " \t", &save);
        char* fs = strtok_r(nullptr, " \t", &save);
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
        char* v = strtok_r(nullptr, " \t", &save);
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
    } else if (strcasecmp(cmd, "e") == 0 || strcasecmp(cmd, "motors_on") == 0) {
        tryEnableMotors();
    } else if (strcasecmp(cmd, "d") == 0 || strcasecmp(cmd, "motors_off") == 0) {
        MotorDriver::stop();
        Ffb::setMode(Ffb::Mode::Off);
        Safety::gMotorWatchdog.notifyMotorDisabled();
        Serial.println("OK motors_off");
    } else if (strcasecmp(cmd, "m") == 0) {
        Ffb::setMode(Ffb::Mode::Manual);
        Serial.println("OK ffb=manual");
    } else if (strcasecmp(cmd, "s") == 0) {
        Ffb::setMode(Ffb::Mode::Spring);
        Serial.println("OK ffb=spring");
    } else if (strcasecmp(cmd, "pid") == 0 || strcasecmp(cmd, "f") == 0) {
        Ffb::setMode(Ffb::Mode::Pid);
        Serial.println("OK ffb=pid");
    } else if (strcasecmp(cmd, "pid_arm") == 0) {
        // Showcase / inject path: arm motors + PID demo slots together.
        if (!MotorDriver::enabled()) {
            if (!tryEnableMotors()) {
                Serial.println("ERR pid_arm: motors not enabled");
                return;
            }
        }
        FfbPid::demoReset();
        FfbPid::demoArmEffects();
        Ffb::setMode(Ffb::Mode::Pid);
        Serial.println("OK pid_arm spring=1 sine=2 const=3");
    } else if (strcasecmp(cmd, "pid_spring") == 0) {
        char* ds = strtok_r(nullptr, " \t", &save);
        if (!ds) {
            Serial.println("ERR usage: :pid_spring <deg>");
            return;
        }
        const float half = HidWheel::rangeDeg() * 0.5f;
        FfbPid::demoSetSpringDeg((float)atof(ds), half);
        if (Ffb::mode() != Ffb::Mode::Pid)
            Ffb::setMode(Ffb::Mode::Pid);
        // Silent OK — showcase streams this ~100 Hz.
    } else if (strcasecmp(cmd, "pid_rumble") == 0) {
        char* a = strtok_r(nullptr, " \t", &save);
        char* h = strtok_r(nullptr, " \t", &save);
        if (!a || !h) {
            Serial.println("ERR usage: :pid_rumble <amp0..1> <hz>");
            return;
        }
        FfbPid::demoSetRumble((float)atof(a), (float)atof(h));
    } else if (strcasecmp(cmd, "pid_kick") == 0) {
        char* m = strtok_r(nullptr, " \t", &save);
        if (!m) {
            Serial.println("ERR usage: :pid_kick <mag-1..1>");
            return;
        }
        FfbPid::demoSetConstant((float)atof(m));
    } else if (strcasecmp(cmd, "pid_end") == 0) {
        FfbPid::demoEnd();
        Ffb::setMode(Ffb::Mode::Off);
        MotorDriver::coast();
        Serial.println("OK pid_end ffb=off");
    } else if (strcasecmp(cmd, "pid_status") == 0) {
        Serial.print("OK pid actuators=");
        Serial.print(FfbPid::actuatorsEnabled() ? 1 : 0);
        Serial.print(" paused=");
        Serial.print(FfbPid::devicePaused() ? 1 : 0);
        Serial.print(" gain=");
        Serial.print(FfbPid::deviceGain());
        Serial.print(" playing=");
        Serial.print(FfbPid::playingCount());
        Serial.print(" alloc=");
        Serial.print(FfbPid::allocatedCount());
        Serial.print(" user_gain=");
        Serial.println(Ffb::ffbGain(), 3);
    } else if (strcasecmp(cmd, "track") == 0) {
        Ffb::setMode(Ffb::Mode::Track);
        Serial.print("OK ffb=track target=");
        Serial.println(Ffb::targetDeg(), 1);
    } else if (strcasecmp(cmd, "ffb_target") == 0) {
        char* ds = strtok_r(nullptr, " \t", &save);
        if (!ds) {
            Serial.print("OK ffb_target=");
            Serial.println(Ffb::targetDeg(), 2);
            return;
        }
        Ffb::setTargetDeg((float)atof(ds));
        Serial.print("OK ffb_target=");
        Serial.println(Ffb::targetDeg(), 2);
    } else if (strcasecmp(cmd, "spring_d") == 0) {
        char* ds = strtok_r(nullptr, " \t", &save);
        if (!ds) {
            Serial.print("OK spring_d=");
            Serial.println(Ffb::springD(), 4);
            return;
        }
        Ffb::setSpringD((float)atof(ds));
        Serial.print("OK spring_d=");
        Serial.println(Ffb::springD(), 4);
    } else if (strcasecmp(cmd, "x") == 0) {
        Ffb::setMode(Ffb::Mode::Off);
        MotorDriver::coast();
        Serial.println("OK ffb=off");
    } else if (strcasecmp(cmd, "n") == 0 || strcasecmp(cmd, "home") == 0 ||
               strcasecmp(cmd, "init") == 0) {
        Homing::startOrCancel();
    } else if (strcasecmp(cmd, "z") == 0) {
        WheelEncoder::zeroHere();
        Serial.println("Wheel zeroed");
    } else if (strcasecmp(cmd, "i") == 0) {
        AxleIndex::armSyncOnNextEdge();
        ControlTick::armIndexSyncLatch();
        Serial.print("Armed: next index → sync axle≈");
        Serial.println(AXLE_INDEX_ANGLE_DEG, 1);
    } else if (strcasecmp(cmd, "c") == 0) {
        if (!WheelEncoder::calibrating()) {
            WheelEncoder::startCal();
            Serial.println("Gear CAL: turn axle +360 deg, press c again");
        } else if (WheelEncoder::finishCal()) {
            Serial.print("Gear CAL ok, ratio=");
            Serial.println(WheelEncoder::gearRatio(), 4);
        } else {
            Serial.println("Gear CAL failed (move more)");
        }
    } else if (strcasecmp(cmd, "p") == 0) {
        Pedals::resetCalibration();
        Serial.println("Pedal CAL reset — HID=0 until each pedal is pressed once");
    } else if (strcasecmp(cmd, "h") == 0 || strcasecmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        printHelp();
    } else if (strcasecmp(cmd, "motor_fault_clear") == 0) {
        if (Safety::gMotorWatchdog.clearDriverFault()) {
            Serial.println("OK motor_fault cleared");
        } else {
            Serial.println("ERR motor_fault still active");
        }
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
        Serial.print("motor_driver=");
        Serial.println(MotorDriver::backendName());
        Serial.print("motor_fault=");
        Serial.println(MotorDriver::faultActive() ? "ACTIVE" : "ok");
        Serial.print("motors=");
        Serial.println(MotorDriver::enabled() ? "enabled" : "disabled");
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
            if (AccessoryLink::feedCdcByte((uint8_t)c))
                continue;
        }

        if (c == '\n' || c == '\r') {
            if (lineLen == 0)
                continue;
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
            if (c == ' ' || c == '\t')
                continue;
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

} // namespace

void setup() {
    // PID HID registered once at boot (no post-INIT USB re-enum — that killed CDC/rim).
#if ENABLE_USB_HID
    HidWheel::begin();
    delay(800);
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

    // Initialize safety watchdogs
    Safety::gMotorWatchdog.init();
    Safety::gCommWatchdog.init();

    Serial.println("init mlx...");
    Mlx90363::begin();
    Serial.println("mlx ok");

    AccessoryLink::begin();
    Settings::begin();
    WheelEncoder::begin(DEFAULT_GEAR_RATIO);
    AxleIndex::begin();
    Homing::begin();
    Pedals::begin();
    MotorDriver::begin();
    Serial.print("motor_driver=");
    Serial.println(MotorDriver::backendName());
#if HOME_BOOT_USE_MOTORS
    Serial.println("*** MOTORIZED BOOT INIT — keep clear; n cancels ***");
#endif
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

    Serial.println(ENABLE_USB_HID ? "base-mcu — PID HID at boot (reports after INIT), serial log"
                                  : "base-mcu — HID off, serial log");
    Serial.print("STAGE t=");
    Serial.print(millis());
    Serial.println(" boot: peripherals ready — starting INIT");
    printHelp();
    Homing::start();
    SetupMsc::beginGrace(SETUP_MSC_GRACE_MS);
}

#if ENABLE_BASE_EPD
void setup1() {
    EpdStatus::beginCore1();
}

void loop1() {
    EpdStatus::serviceCore1();
}
#endif

void loop() {
    const uint32_t loopStart = millis();

    ControlTick::service();
    // Extra UART drain outside the tick — HID/host chatter must not leave rim frames queued.
    AccessoryLink::update();
    // After INIT settle: enable HID reports / host FFB (USB already up — no re-enum).
    HidWheel::serviceAttach(Homing::active());
    SetupMsc::service();
    EpdStatus::update(ControlTick::lastHallOk(), ControlTick::lastAxleDeg());

    // Startup STAGE heartbeat until HID reports go live.
    {
        const uint32_t now = millis();
        if (AccessoryLink::linked() && !loggedRimUp_) {
            loggedRimUp_ = true;
            Serial.print("STAGE t=");
            Serial.print(now);
            Serial.println(" rim: link up");
        }
        if (HidWheel::ready() && !loggedHidReady_) {
            loggedHidReady_ = true;
            Serial.print("STAGE t=");
            Serial.print(now);
            Serial.println(" boot: READY (HID reports live + INIT done)");
        } else if (!HidWheel::ready() && now - lastStageMs >= 1000u) {
            lastStageMs = now;
            Serial.print("STAGE t=");
            Serial.print(now);
            Serial.print(" home=");
            Serial.print(Homing::active() ? Homing::phaseName() : "idle");
            Serial.print(" hid=");
            Serial.print(HidWheel::ready() ? 1 : 0);
            Serial.print(" rim=");
            Serial.print(AccessoryLink::linked() ? 1 : 0);
            Serial.print(" hall=");
            Serial.print(ControlTick::lastHallOk() ? 1 : 0);
            Serial.print(" axle=");
            Serial.println(ControlTick::lastAxleDeg(), 1);
        }
    }

    // Update safety watchdogs
    Safety::gMotorWatchdog.update();
    Safety::gCommWatchdog.update();

    const uint32_t now = millis();
    if (Settings::telemetryEnabled() && now - lastPrintMs >= 100) {
        lastPrintMs = now;

        const Pedals::State& ped = ControlTick::lastPedals();
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
            Serial.print(AccessoryLink::adxlCalibratedX(
                Settings::cdata().adxlCalValid ? Settings::cdata().adxlXOffset : (int16_t)0));
        }
        Serial.print(" gear=");
        Serial.print(WheelEncoder::gearRatio(), 4);
        Serial.print(" motors=");
        Serial.print(MotorDriver::enabled() ? 1 : 0);
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

    // USB host present (port open) counts as activity — not only inbound bytes.
    // Otherwise motorized boot INIT dies after 5s while the GUI only reads.
    if (Serial && Serial.dtr()) {
        Safety::gCommWatchdog.notifyUsbActivity();
    } else if (Serial.available()) {
        Safety::gCommWatchdog.notifyUsbActivity();
    }

    AccessoryLink::update();
    const uint32_t elapsed = millis() - loopStart;
    if (elapsed < LOOP_PERIOD_MS) {
        delay(LOOP_PERIOD_MS - elapsed);
    }
}
