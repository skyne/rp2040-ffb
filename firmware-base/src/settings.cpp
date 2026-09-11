#include "settings.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "accessory_link.h"
#include "config.h"
#include "ffb.h"
#include "ffb_link.h"
#include "ffb_version.h"
#include "hid_wheel.h"
#include "motor_bts7960.h"
#include "pedals.h"
#include "wheel_encoder.h"

namespace Settings {
namespace {

constexpr uint32_t kMagic = 0x33424646u;  // 'FFB3'
constexpr uint32_t kMagicV2 = 0x32424646u;  // 'FFB2' (included rim blob)
constexpr uint16_t kVersion = 3;
constexpr uint16_t kVersionV2 = 2;
constexpr int kEepromSize = 512;

struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
};

// Legacy layout for migration (rim field ignored as source of truth).
struct DataV2 {
    float dutyCap;
    float springK;
    float springDz;
    float torqueCap;
    float hidRange;
    float gearRatio;
    Pedals::AxisCal thr;
    Pedals::AxisCal brk;
    Pedals::AxisCal clu;
    FfbLink::RimConfig rim;
};

Data g;
bool telemetry = true;

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void fillDefaults(Data &d) {
    d = Data{};
    d.dutyCap = MOTOR_DUTY_CAP;
    d.springK = FFB_SPRING_K;
    d.springDz = FFB_SPRING_DEADZONE_DEG;
    d.torqueCap = FFB_TORQUE_CAP;
    d.hidRange = WHEEL_HID_RANGE_DEG;
    d.gearRatio = DEFAULT_GEAR_RATIO;
    d.thr = Pedals::AxisCal{};
    d.brk = Pedals::AxisCal{};
    d.clu = Pedals::AxisCal{};
    d.thr.inverted = 1;
    d.brk.inverted = 1;
    d.clu.inverted = 1;
}

void pullFromLive(Data &d) {
    d.dutyCap = MotorBts7960::dutyCap();
    d.springK = Ffb::springK();
    d.springDz = Ffb::springDeadzone();
    d.torqueCap = Ffb::torqueCap();
    d.hidRange = HidWheel::rangeDeg();
    d.gearRatio = WheelEncoder::gearRatio();
    Pedals::getCalibration(d.thr, d.brk, d.clu);
}

bool keyEq(const char *a, const char *b) {
    return strcasecmp(a, b) == 0;
}

bool parseEncKey(const char *key, uint8_t &idx, const char *&field) {
    if (strncasecmp(key, "enc", 3) != 0) return false;
    if (key[3] < '0' || key[3] > '3') return false;
    if (key[4] != '_') return false;
    idx = (uint8_t)(key[3] - '0');
    field = key + 5;
    return true;
}

bool getEncField(const FfbLink::EncoderConfig &ec, const char *field, float &out) {
    if (keyEq(field, "steps")) {
        out = ec.stepsPerClick;
        return true;
    }
    if (keyEq(field, "accel")) {
        out = ec.accelEnable;
        return true;
    }
    if (keyEq(field, "thresh")) {
        out = ec.accelThreshold;
        return true;
    }
    if (keyEq(field, "mult")) {
        out = ec.accelMaxMult;
        return true;
    }
    if (keyEq(field, "debounce")) {
        out = ec.debounceMs;
        return true;
    }
    if (keyEq(field, "pulse")) {
        out = ec.pulseMs;
        return true;
    }
    if (keyEq(field, "invert")) {
        out = ec.invert;
        return true;
    }
    return false;
}

bool setEncField(FfbLink::EncoderConfig &ec, const char *field, float value) {
    if (keyEq(field, "steps")) {
        ec.stepsPerClick = (uint8_t)clampf(value, 1.0f, 32.0f);
        return true;
    }
    if (keyEq(field, "accel")) {
        ec.accelEnable = value >= 0.5f ? 1 : 0;
        return true;
    }
    if (keyEq(field, "thresh")) {
        ec.accelThreshold = (uint8_t)clampf(value, 1.0f, 64.0f);
        return true;
    }
    if (keyEq(field, "mult")) {
        ec.accelMaxMult = (uint8_t)clampf(value, 1.0f, 8.0f);
        return true;
    }
    if (keyEq(field, "debounce")) {
        ec.debounceMs = (uint8_t)clampf(value, 0.0f, 50.0f);
        return true;
    }
    if (keyEq(field, "pulse")) {
        ec.pulseMs = (uint8_t)clampf(value, 5.0f, 200.0f);
        return true;
    }
    if (keyEq(field, "invert")) {
        ec.invert = value >= 0.5f ? 1 : 0;
        return true;
    }
    return false;
}

void applyRimLive(FfbLink::RimConfig &rim) {
    AccessoryLink::setRimConfig(rim);
    if (AccessoryLink::linked()) {
        AccessoryLink::pushRimConfig();
    }
}

}  // namespace

void begin() {
    EEPROM.begin(kEepromSize);
    fillDefaults(g);
    FfbLink::RimConfig rim{};
    FfbLink::defaultRimConfig(rim);
    AccessoryLink::setRimConfig(rim);
}

bool load() {
    Header hdr{};
    EEPROM.get(0, hdr);

    if (hdr.magic == kMagic && hdr.version == kVersion && hdr.size == sizeof(Data)) {
        Data loaded{};
        EEPROM.get(sizeof(Header), loaded);
        g = loaded;
        return true;
    }

    // Migrate v2 (rim blob on base) → base fields only; rim stays on rim EEPROM.
    if (hdr.magic == kMagicV2 && hdr.version == kVersionV2 && hdr.size == sizeof(DataV2)) {
        DataV2 legacy{};
        EEPROM.get(sizeof(Header), legacy);
        g.dutyCap = legacy.dutyCap;
        g.springK = legacy.springK;
        g.springDz = legacy.springDz;
        g.torqueCap = legacy.torqueCap;
        g.hidRange = legacy.hidRange;
        g.gearRatio = legacy.gearRatio;
        g.thr = legacy.thr;
        g.brk = legacy.brk;
        g.clu = legacy.clu;
        return true;
    }

    return false;
}

bool save() {
    pullFromLive(g);
    Header hdr{kMagic, kVersion, (uint16_t)sizeof(Data)};
    EEPROM.put(0, hdr);
    EEPROM.put(sizeof(Header), g);
    const bool ok = EEPROM.commit();
    if (ok && AccessoryLink::linked()) {
        AccessoryLink::saveRimConfig();
    }
    return ok;
}

void resetDefaults() {
    fillDefaults(g);
}

void apply() {
    MotorBts7960::setDutyCap(g.dutyCap);
    Ffb::setSpringK(g.springK);
    Ffb::setSpringDeadzone(g.springDz);
    Ffb::setTorqueCap(g.torqueCap);
    HidWheel::setRangeDeg(g.hidRange);
    WheelEncoder::setGearRatio(g.gearRatio);
    Pedals::setCalibration(g.thr, g.brk, g.clu);
}

Data &data() { return g; }
const Data &cdata() { return g; }

bool telemetryEnabled() { return telemetry; }
void setTelemetryEnabled(bool on) { telemetry = on; }

bool getFloat(const char *key, float &out) {
    pullFromLive(g);
    const FfbLink::RimConfig &rim = AccessoryLink::rimConfig();

    if (keyEq(key, "duty_cap")) {
        out = g.dutyCap;
        return true;
    }
    if (keyEq(key, "spring_k")) {
        out = g.springK;
        return true;
    }
    if (keyEq(key, "spring_dz")) {
        out = g.springDz;
        return true;
    }
    if (keyEq(key, "torque_cap")) {
        out = g.torqueCap;
        return true;
    }
    if (keyEq(key, "hid_range")) {
        out = g.hidRange;
        return true;
    }
    if (keyEq(key, "gear_ratio")) {
        out = g.gearRatio;
        return true;
    }
    if (keyEq(key, "panel_led_bright")) {
        out = rim.panelLedBright;
        return true;
    }
    if (keyEq(key, "shift_led_bright")) {
        out = rim.shiftLedBright;
        return true;
    }
    if (keyEq(key, "shift_led_count")) {
        out = rim.shiftLedCount;
        return true;
    }
    if (keyEq(key, "disp_bright")) {
        out = rim.dispBright;
        return true;
    }
    if (keyEq(key, "shift_rpm_0")) {
        out = rim.shiftRpm[0];
        return true;
    }
    if (keyEq(key, "shift_rpm_1")) {
        out = rim.shiftRpm[1];
        return true;
    }
    if (keyEq(key, "shift_rpm_2")) {
        out = rim.shiftRpm[2];
        return true;
    }
    if (keyEq(key, "shift_rpm_3")) {
        out = rim.shiftRpm[3];
        return true;
    }
    if (keyEq(key, "shift_rpm_4")) {
        out = rim.shiftRpm[4] ? rim.shiftRpm[4] : 7800;
        return true;
    }
    if (keyEq(key, "rim_link")) {
        out = AccessoryLink::linked() ? 1.0f : 0.0f;
        return true;
    }

    uint8_t idx = 0;
    const char *field = nullptr;
    if (parseEncKey(key, idx, field)) {
        return getEncField(rim.enc[idx], field, out);
    }
    return false;
}

bool setFloat(const char *key, float value) {
    if (keyEq(key, "duty_cap")) {
        g.dutyCap = clampf(value, 0.0f, 1.0f);
        MotorBts7960::setDutyCap(g.dutyCap);
        return true;
    }
    if (keyEq(key, "spring_k")) {
        g.springK = clampf(value, 0.0f, 1.0f);
        Ffb::setSpringK(g.springK);
        return true;
    }
    if (keyEq(key, "spring_dz")) {
        g.springDz = clampf(value, 0.0f, 180.0f);
        Ffb::setSpringDeadzone(g.springDz);
        return true;
    }
    if (keyEq(key, "torque_cap")) {
        g.torqueCap = clampf(value, 0.0f, 1.0f);
        Ffb::setTorqueCap(g.torqueCap);
        return true;
    }
    if (keyEq(key, "hid_range")) {
        g.hidRange = clampf(value, 10.0f, 2880.0f);
        HidWheel::setRangeDeg(g.hidRange);
        return true;
    }
    if (keyEq(key, "gear_ratio")) {
        if (fabsf(value) < 0.1f) return false;
        g.gearRatio = value;
        WheelEncoder::setGearRatio(g.gearRatio);
        return true;
    }

    FfbLink::RimConfig rim = AccessoryLink::rimConfig();
    bool rimChanged = false;

    if (keyEq(key, "panel_led_bright")) {
        rim.panelLedBright = (uint8_t)clampf(value, 0.0f, 255.0f);
        rimChanged = true;
    } else if (keyEq(key, "shift_led_bright")) {
        rim.shiftLedBright = (uint8_t)clampf(value, 0.0f, 255.0f);
        rimChanged = true;
    } else if (keyEq(key, "shift_led_count")) {
        rim.shiftLedCount = (uint8_t)clampf(value, 1.0f, 64.0f);
        rimChanged = true;
    } else if (keyEq(key, "disp_bright")) {
        rim.dispBright = (uint8_t)clampf(value, 0.0f, 255.0f);
        rimChanged = true;
    } else if (keyEq(key, "shift_rpm_0") || keyEq(key, "shift_rpm_1") || keyEq(key, "shift_rpm_2") ||
               keyEq(key, "shift_rpm_3") || keyEq(key, "shift_rpm_4")) {
        const int i = key[10] - '0';
        if (i < 0 || i > 4) return false;
        rim.shiftRpm[i] = (uint16_t)clampf(value, 0.0f, 20000.0f);
        rimChanged = true;
    } else {
        uint8_t idx = 0;
        const char *field = nullptr;
        if (parseEncKey(key, idx, field)) {
            if (!setEncField(rim.enc[idx], field, value)) return false;
            rimChanged = true;
        }
    }

    if (rimChanged) {
        applyRimLive(rim);
        return true;
    }
    return false;
}

void dumpToSerial() {
    pullFromLive(g);
    const FfbLink::RimConfig &rim = AccessoryLink::rimConfig();
    char baseId[FfbVersion::kIdMax];
    FfbVersion::formatId(baseId, sizeof(baseId));

    // Refresh rim id before dump so GUI can verify both MCUs.
    AccessoryLink::requestRimVersion();
    for (int i = 0; i < 40; ++i) {
        AccessoryLink::update();
        delay(5);
    }

    Serial.println("OK dump");
    Serial.print("base_fw=");
    Serial.println(baseId);
    Serial.print("rim_fw=");
    Serial.println(AccessoryLink::rimFwId()[0] ? AccessoryLink::rimFwId() : "?");
    Serial.print("duty_cap=");
    Serial.println(g.dutyCap, 6);
    Serial.print("spring_k=");
    Serial.println(g.springK, 6);
    Serial.print("spring_dz=");
    Serial.println(g.springDz, 4);
    Serial.print("torque_cap=");
    Serial.println(g.torqueCap, 6);
    Serial.print("hid_range=");
    Serial.println(g.hidRange, 2);
    Serial.print("gear_ratio=");
    Serial.println(g.gearRatio, 6);
    Serial.print("rim_link=");
    Serial.println(AccessoryLink::linked() ? 1 : 0);
    Serial.print("panel_led_bright=");
    Serial.println(rim.panelLedBright);
    Serial.print("shift_led_bright=");
    Serial.println(rim.shiftLedBright);
    Serial.print("shift_led_count=");
    Serial.println(rim.shiftLedCount);
    Serial.print("disp_bright=");
    Serial.println(rim.dispBright);
    for (int i = 0; i < 5; ++i) {
        Serial.print("shift_rpm_");
        Serial.print(i);
        Serial.print('=');
        Serial.println(rim.shiftRpm[i]);
    }
    for (uint8_t i = 0; i < FfbLink::kEncoderCount; ++i) {
        const auto &ec = rim.enc[i];
        char buf[48];
        snprintf(buf, sizeof(buf), "enc%u_steps=%u", i, ec.stepsPerClick);
        Serial.println(buf);
        snprintf(buf, sizeof(buf), "enc%u_accel=%u", i, ec.accelEnable);
        Serial.println(buf);
        snprintf(buf, sizeof(buf), "enc%u_thresh=%u", i, ec.accelThreshold);
        Serial.println(buf);
        snprintf(buf, sizeof(buf), "enc%u_mult=%u", i, ec.accelMaxMult);
        Serial.println(buf);
        snprintf(buf, sizeof(buf), "enc%u_debounce=%u", i, ec.debounceMs);
        Serial.println(buf);
        snprintf(buf, sizeof(buf), "enc%u_pulse=%u", i, ec.pulseMs);
        Serial.println(buf);
        snprintf(buf, sizeof(buf), "enc%u_invert=%u", i, ec.invert);
        Serial.println(buf);
    }
    Serial.println("OK end");
}

}  // namespace Settings
