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

constexpr uint32_t kMagic = 0x36424646u;    // 'FFB6'
constexpr uint32_t kMagicV5 = 0x35424646u;  // 'FFB5'
constexpr uint32_t kMagicV4 = 0x34424646u;  // 'FFB4'
constexpr uint32_t kMagicV3 = 0x33424646u;  // 'FFB3'
constexpr uint32_t kMagicV2 = 0x32424646u;  // 'FFB2' (included rim blob)
constexpr uint16_t kVersion = 6;
constexpr uint16_t kVersionV5 = 5;
constexpr uint16_t kVersionV4 = 4;
constexpr uint16_t kVersionV3 = 3;
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

struct DataV4 {
    float dutyCap;
    float springK;
    float springDz;
    float torqueCap;
    float hidRange;
    float gearRatio;
    Pedals::AxisCal thr;
    Pedals::AxisCal brk;
    Pedals::AxisCal clu;
};

struct StoreV4 {
    DataV4 data;
    uint8_t activeProfile;
    uint8_t _pad[3];
    Profile profiles[kProfileCount];
};

struct DataV5 {
    float dutyCap;
    float springK;
    float springDz;
    float torqueCap;
    float hidRange;
    float gearRatio;
    Pedals::AxisCal thr;
    Pedals::AxisCal brk;
    Pedals::AxisCal clu;
    float softLimitDeg;
    float softLimitK;
    uint8_t softLimitEn;
    uint8_t _padSL[3];
};

struct StoreV5 {
    DataV5 data;
    uint8_t activeProfile;
    uint8_t _pad[3];
    Profile profiles[kProfileCount];
};

struct StoreV6 {
    Data data;
    uint8_t activeProfile;
    uint8_t _pad[3];
    Profile profiles[kProfileCount];
};

Data g;
Profile profiles[kProfileCount];
uint8_t activeSlot = kProfileNone;
bool telemetry = true;

float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void setName(Profile &p, const char *name) {
    memset(p.name, 0, sizeof(p.name));
    if (!name || !name[0]) return;
    strncpy(p.name, name, kProfileNameLen - 1);
}

void fillProfileDefaults(Profile &p, uint8_t slot) {
    p = Profile{};
    p.used = 1;
    p.dutyCap = MOTOR_DUTY_CAP;
    p.springK = FFB_SPRING_K;
    p.springDz = FFB_SPRING_DEADZONE_DEG;
    p.torqueCap = FFB_TORQUE_CAP;
    p.hidRange = WHEEL_HID_RANGE_DEG;
    p.panelLedBright = 80;
    p.shiftLedBright = 60;
    p.shiftRpm[0] = 5000;
    p.shiftRpm[1] = 6000;
    p.shiftRpm[2] = 7000;
    p.shiftRpm[3] = 7500;
    p.shiftRpm[4] = 7800;
    switch (slot) {
        case 0:
            setName(p, "Road");
            p.springK = 0.003f;
            p.hidRange = 900.0f;
            break;
        case 1:
            setName(p, "GT");
            p.springK = 0.006f;
            p.torqueCap = 0.40f;
            p.hidRange = 540.0f;
            break;
        case 2:
            setName(p, "Rally");
            p.springK = 0.0045f;
            p.hidRange = 1080.0f;
            p.springDz = 3.0f;
            break;
        case 3:
            setName(p, "Night");
            p.panelLedBright = 30;
            p.shiftLedBright = 25;
            p.hidRange = 900.0f;
            break;
        default:
            setName(p, "User");
            break;
    }
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
    d.softLimitDeg = 0.0f;
    d.softLimitK = 0.012f;
    d.softLimitEn = 1;
    d.adxlCalValid = 0;
    d.adxlXOffset = 0;
}

float effectiveSoftLimitDeg(const Data &d) {
    if (d.softLimitDeg >= 1.0f) return d.softLimitDeg;
    return d.hidRange * 0.5f;
}

void seedFactoryProfiles() {
    for (uint8_t i = 0; i < kProfileCount; ++i) {
        fillProfileDefaults(profiles[i], i);
    }
    activeSlot = 0;
}

void pullFromLive(Data &d) {
    d.dutyCap = MotorBts7960::dutyCap();
    d.springK = Ffb::springK();
    d.springDz = Ffb::springDeadzone();
    d.torqueCap = Ffb::torqueCap();
    d.hidRange = HidWheel::rangeDeg();
    d.gearRatio = WheelEncoder::gearRatio();
    Pedals::getCalibration(d.thr, d.brk, d.clu);
    d.softLimitEn = Ffb::softLimitEnabled() ? 1 : 0;
    d.softLimitK = Ffb::softLimitK();
    // Keep stored softLimitDeg (0 = auto); live effective is applied separately.
}

void markCustom() { activeSlot = kProfileNone; }

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
    if (keyEq(field, "mode")) {
        out = ec.mode;
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
    if (keyEq(field, "mode")) {
        ec.mode = (uint8_t)clampf(value, 0.0f, 2.0f);
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

bool slotOk(uint8_t slot) { return slot < kProfileCount; }

void captureLiveToProfile(Profile &p) {
    pullFromLive(g);
    const FfbLink::RimConfig &rim = AccessoryLink::rimConfig();
    char keepName[kProfileNameLen];
    memcpy(keepName, p.name, kProfileNameLen);
    const bool hadName = keepName[0] != '\0';
    p.dutyCap = g.dutyCap;
    p.springK = g.springK;
    p.springDz = g.springDz;
    p.torqueCap = g.torqueCap;
    p.hidRange = g.hidRange;
    p.panelLedBright = rim.panelLedBright;
    p.shiftLedBright = rim.shiftLedBright;
    for (int i = 0; i < 5; ++i) p.shiftRpm[i] = rim.shiftRpm[i];
    p.used = 1;
    if (hadName) {
        memcpy(p.name, keepName, kProfileNameLen);
        p.name[kProfileNameLen - 1] = '\0';
    } else {
        setName(p, "User");
    }
}

bool applyProfileLive(const Profile &p) {
    if (!p.used) return false;
    g.dutyCap = clampf(p.dutyCap, 0.0f, 1.0f);
    g.springK = clampf(p.springK, 0.0f, 1.0f);
    g.springDz = clampf(p.springDz, 0.0f, 180.0f);
    g.torqueCap = clampf(p.torqueCap, 0.0f, 1.0f);
    g.hidRange = clampf(p.hidRange, 10.0f, 2880.0f);
    MotorBts7960::setDutyCap(g.dutyCap);
    Ffb::setSpringK(g.springK);
    Ffb::setSpringDeadzone(g.springDz);
    Ffb::setTorqueCap(g.torqueCap);
    HidWheel::setRangeDeg(g.hidRange);

    FfbLink::RimConfig rim = AccessoryLink::rimConfig();
    rim.panelLedBright = p.panelLedBright;
    rim.shiftLedBright = p.shiftLedBright;
    for (int i = 0; i < 5; ++i) rim.shiftRpm[i] = p.shiftRpm[i];
    applyRimLive(rim);
    return true;
}

}  // namespace

void begin() {
    EEPROM.begin(kEepromSize);
    fillDefaults(g);
    seedFactoryProfiles();
    FfbLink::RimConfig rim{};
    FfbLink::defaultRimConfig(rim);
    AccessoryLink::setRimConfig(rim);
}

bool load() {
    Header hdr{};
    EEPROM.get(0, hdr);

    if (hdr.magic == kMagic && hdr.version == kVersion && hdr.size == sizeof(StoreV6)) {
        StoreV6 store{};
        EEPROM.get(sizeof(Header), store);
        g = store.data;
        activeSlot = store.activeProfile;
        memcpy(profiles, store.profiles, sizeof(profiles));
        if (activeSlot != kProfileNone && activeSlot >= kProfileCount) {
            activeSlot = kProfileNone;
        }
        return true;
    }

    // Migrate v5 → v6 (ADXL cal defaults).
    if (hdr.magic == kMagicV5 && hdr.version == kVersionV5 && hdr.size == sizeof(StoreV5)) {
        StoreV5 legacy{};
        EEPROM.get(sizeof(Header), legacy);
        g = Data{};
        g.dutyCap = legacy.data.dutyCap;
        g.springK = legacy.data.springK;
        g.springDz = legacy.data.springDz;
        g.torqueCap = legacy.data.torqueCap;
        g.hidRange = legacy.data.hidRange;
        g.gearRatio = legacy.data.gearRatio;
        g.thr = legacy.data.thr;
        g.brk = legacy.data.brk;
        g.clu = legacy.data.clu;
        g.softLimitDeg = legacy.data.softLimitDeg;
        g.softLimitK = legacy.data.softLimitK;
        g.softLimitEn = legacy.data.softLimitEn;
        g.adxlCalValid = 0;
        g.adxlXOffset = 0;
        activeSlot = legacy.activeProfile;
        memcpy(profiles, legacy.profiles, sizeof(profiles));
        if (activeSlot != kProfileNone && activeSlot >= kProfileCount) {
            activeSlot = kProfileNone;
        }
        return true;
    }

    // Migrate v4 → v6 (profiles kept; soft-limit + ADXL defaults).
    if (hdr.magic == kMagicV4 && hdr.version == kVersionV4 && hdr.size == sizeof(StoreV4)) {
        StoreV4 legacy{};
        EEPROM.get(sizeof(Header), legacy);
        g = Data{};
        g.dutyCap = legacy.data.dutyCap;
        g.springK = legacy.data.springK;
        g.springDz = legacy.data.springDz;
        g.torqueCap = legacy.data.torqueCap;
        g.hidRange = legacy.data.hidRange;
        g.gearRatio = legacy.data.gearRatio;
        g.thr = legacy.data.thr;
        g.brk = legacy.data.brk;
        g.clu = legacy.data.clu;
        g.softLimitDeg = 0.0f;
        g.softLimitK = 0.012f;
        g.softLimitEn = 1;
        g.adxlCalValid = 0;
        g.adxlXOffset = 0;
        activeSlot = legacy.activeProfile;
        memcpy(profiles, legacy.profiles, sizeof(profiles));
        if (activeSlot != kProfileNone && activeSlot >= kProfileCount) {
            activeSlot = kProfileNone;
        }
        return true;
    }

    // Migrate v3 → v6 (base fields + factory profiles).
    if (hdr.magic == kMagicV3 && hdr.version == kVersionV3 && hdr.size == sizeof(DataV4)) {
        DataV4 loaded{};
        EEPROM.get(sizeof(Header), loaded);
        g = Data{};
        g.dutyCap = loaded.dutyCap;
        g.springK = loaded.springK;
        g.springDz = loaded.springDz;
        g.torqueCap = loaded.torqueCap;
        g.hidRange = loaded.hidRange;
        g.gearRatio = loaded.gearRatio;
        g.thr = loaded.thr;
        g.brk = loaded.brk;
        g.clu = loaded.clu;
        g.softLimitDeg = 0.0f;
        g.softLimitK = 0.012f;
        g.softLimitEn = 1;
        g.adxlCalValid = 0;
        g.adxlXOffset = 0;
        seedFactoryProfiles();
        activeSlot = kProfileNone;
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
        g.softLimitDeg = 0.0f;
        g.softLimitK = 0.012f;
        g.softLimitEn = 1;
        g.adxlCalValid = 0;
        g.adxlXOffset = 0;
        seedFactoryProfiles();
        activeSlot = kProfileNone;
        return true;
    }

    return false;
}

bool save() {
    pullFromLive(g);
    StoreV6 store{};
    store.data = g;
    store.activeProfile = activeSlot;
    memcpy(store.profiles, profiles, sizeof(profiles));
    Header hdr{kMagic, kVersion, (uint16_t)sizeof(StoreV6)};
    EEPROM.put(0, hdr);
    EEPROM.put(sizeof(Header), store);
    const bool ok = EEPROM.commit();
    if (ok && AccessoryLink::linked()) {
        AccessoryLink::saveRimConfig();
    }
    return ok;
}

void resetDefaults() {
    fillDefaults(g);
    markCustom();
}

void apply() {
    MotorBts7960::setDutyCap(g.dutyCap);
    Ffb::setSpringK(g.springK);
    Ffb::setSpringDeadzone(g.springDz);
    Ffb::setTorqueCap(g.torqueCap);
    HidWheel::setRangeDeg(g.hidRange);
    WheelEncoder::setGearRatio(g.gearRatio);
    Pedals::setCalibration(g.thr, g.brk, g.clu);
    Ffb::setSoftLimitEnabled(g.softLimitEn != 0);
    Ffb::setSoftLimitK(g.softLimitK);
    Ffb::setSoftLimitDeg(effectiveSoftLimitDeg(g));
}

Data &data() { return g; }
const Data &cdata() { return g; }

bool telemetryEnabled() { return telemetry; }
void setTelemetryEnabled(bool on) { telemetry = on; }

uint8_t activeProfile() { return activeSlot; }

const Profile &profile(uint8_t slot) {
    static const Profile empty{};
    if (!slotOk(slot)) return empty;
    return profiles[slot];
}

bool profileSave(uint8_t slot, const char *nameOrNull) {
    if (!slotOk(slot)) return false;
    captureLiveToProfile(profiles[slot]);
    if (nameOrNull && nameOrNull[0]) {
        setName(profiles[slot], nameOrNull);
    } else if (!profiles[slot].name[0]) {
        setName(profiles[slot], "User");
    }
    activeSlot = slot;
    return true;
}

bool profileLoad(uint8_t slot) {
    if (!slotOk(slot) || !profiles[slot].used) return false;
    if (!applyProfileLive(profiles[slot])) return false;
    activeSlot = slot;
    return true;
}

bool profileClear(uint8_t slot) {
    if (!slotOk(slot)) return false;
    profiles[slot] = Profile{};
    if (activeSlot == slot) activeSlot = kProfileNone;
    return true;
}

bool profileRename(uint8_t slot, const char *name) {
    if (!slotOk(slot) || !name || !name[0]) return false;
    if (!profiles[slot].used) {
        fillProfileDefaults(profiles[slot], slot);
    }
    setName(profiles[slot], name);
    profiles[slot].used = 1;
    return true;
}

void profilesDumpToSerial() {
    Serial.println("OK profiles");
    Serial.print("profile_active=");
    if (activeSlot == kProfileNone) {
        Serial.println("none");
    } else {
        Serial.println(activeSlot);
    }
    for (uint8_t i = 0; i < kProfileCount; ++i) {
        const Profile &p = profiles[i];
        Serial.print("profile");
        Serial.print(i);
        Serial.print("_used=");
        Serial.println(p.used ? 1 : 0);
        Serial.print("profile");
        Serial.print(i);
        Serial.print("_name=");
        Serial.println(p.used && p.name[0] ? p.name : "");
        if (!p.used) continue;
        Serial.print("profile");
        Serial.print(i);
        Serial.print("_hid_range=");
        Serial.println(p.hidRange, 1);
        Serial.print("profile");
        Serial.print(i);
        Serial.print("_spring_k=");
        Serial.println(p.springK, 6);
    }
    Serial.println("OK end");
}

bool handleProfileCmd(char *args) {
    while (args && (*args == ' ' || *args == '\t')) ++args;
    char listDefault[] = "list";
    char *work = (args && args[0]) ? args : listDefault;
    char *save = nullptr;
    char *sub = strtok_r(work, " \t", &save);
    if (!sub) {
        profilesDumpToSerial();
        return true;
    }

    if (keyEq(sub, "list") || keyEq(sub, "ls")) {
        profilesDumpToSerial();
        return true;
    }
    if (keyEq(sub, "next") || keyEq(sub, "prev")) {
        // Cycle used slots (for future on-rim quick menu).
        int start = activeSlot == kProfileNone ? -1 : (int)activeSlot;
        int dir = keyEq(sub, "next") ? 1 : -1;
        for (int n = 1; n <= (int)kProfileCount; ++n) {
            int cand = (start + dir * n) % (int)kProfileCount;
            if (cand < 0) cand += kProfileCount;
            if (profiles[(uint8_t)cand].used) {
                if (!profileLoad((uint8_t)cand)) break;
                Serial.print("OK profile=");
                Serial.println(cand);
                dumpToSerial();
                return true;
            }
        }
        Serial.println("ERR no used profiles");
        return true;
    }
    if (keyEq(sub, "active")) {
        Serial.print("OK profile_active=");
        if (activeSlot == kProfileNone) {
            Serial.println("none");
        } else {
            Serial.println(activeSlot);
        }
        return true;
    }

    char *slotStr = strtok_r(nullptr, " \t", &save);
    if (!slotStr) {
        Serial.println("ERR usage: profile list|load N|save N [name]|name N <str>|clear N");
        return true;
    }
    char *end = nullptr;
    long slotL = strtol(slotStr, &end, 10);
    if (end == slotStr || slotL < 0 || slotL >= kProfileCount) {
        Serial.println("ERR profile slot 0..3");
        return true;
    }
    const uint8_t slot = (uint8_t)slotL;

    if (keyEq(sub, "load")) {
        if (!profileLoad(slot)) {
            Serial.println("ERR profile empty or bad slot");
            return true;
        }
        Serial.print("OK profile=");
        Serial.println(slot);
        dumpToSerial();
        return true;
    }
    if (keyEq(sub, "save")) {
        char *name = strtok_r(nullptr, " \t", &save);
        if (!profileSave(slot, (name && name[0]) ? name : nullptr)) {
            Serial.println("ERR profile save");
            return true;
        }
        Serial.print("OK profile=");
        Serial.print(slot);
        Serial.print(" saved name=");
        Serial.println(profiles[slot].name);
        return true;
    }
    if (keyEq(sub, "name") || keyEq(sub, "rename")) {
        char *name = strtok_r(nullptr, " \t", &save);
        if (!profileRename(slot, name)) {
            Serial.println("ERR usage: profile name N <str>");
            return true;
        }
        Serial.print("OK profile=");
        Serial.print(slot);
        Serial.print(" name=");
        Serial.println(profiles[slot].name);
        return true;
    }
    if (keyEq(sub, "clear") || keyEq(sub, "del") || keyEq(sub, "delete")) {
        if (!profileClear(slot)) {
            Serial.println("ERR profile clear");
            return true;
        }
        Serial.print("OK profile=");
        Serial.print(slot);
        Serial.println(" cleared");
        return true;
    }

    Serial.println("ERR usage: profile list|load N|save N [name]|name N <str>|clear N");
    return true;
}

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
    if (keyEq(key, "soft_limit_en")) {
        out = g.softLimitEn ? 1.0f : 0.0f;
        return true;
    }
    if (keyEq(key, "soft_limit_deg")) {
        out = g.softLimitDeg;
        return true;
    }
    if (keyEq(key, "soft_limit_k")) {
        out = g.softLimitK;
        return true;
    }
    if (keyEq(key, "adxl_cal")) {
        out = g.adxlCalValid ? 1.0f : 0.0f;
        return true;
    }
    if (keyEq(key, "adxl_x_offset")) {
        out = (float)g.adxlXOffset;
        return true;
    }
    if (keyEq(key, "adxl_present")) {
        out = AccessoryLink::adxlPresent() ? 1.0f : 0.0f;
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
    if (keyEq(key, "profile_active")) {
        out = activeSlot == kProfileNone ? -1.0f : (float)activeSlot;
        return true;
    }

    uint8_t idx = 0;
    const char *field = nullptr;
    if (parseEncKey(key, idx, field)) {
        if (keyEq(field, "value")) {
            out = (float)AccessoryLink::encoderAbs(idx);
            return true;
        }
        return getEncField(rim.enc[idx], field, out);
    }
    return false;
}

bool setFloat(const char *key, float value) {
    if (keyEq(key, "duty_cap")) {
        g.dutyCap = clampf(value, 0.0f, 1.0f);
        MotorBts7960::setDutyCap(g.dutyCap);
        markCustom();
        return true;
    }
    if (keyEq(key, "spring_k")) {
        g.springK = clampf(value, 0.0f, 1.0f);
        Ffb::setSpringK(g.springK);
        markCustom();
        return true;
    }
    if (keyEq(key, "spring_dz")) {
        g.springDz = clampf(value, 0.0f, 180.0f);
        Ffb::setSpringDeadzone(g.springDz);
        markCustom();
        return true;
    }
    if (keyEq(key, "torque_cap")) {
        g.torqueCap = clampf(value, 0.0f, 1.0f);
        Ffb::setTorqueCap(g.torqueCap);
        markCustom();
        return true;
    }
    if (keyEq(key, "hid_range")) {
        g.hidRange = clampf(value, 10.0f, 2880.0f);
        HidWheel::setRangeDeg(g.hidRange);
        Ffb::setSoftLimitDeg(effectiveSoftLimitDeg(g));
        markCustom();
        return true;
    }
    if (keyEq(key, "gear_ratio")) {
        if (fabsf(value) < 0.1f) return false;
        g.gearRatio = value;
        WheelEncoder::setGearRatio(g.gearRatio);
        return true;
    }
    if (keyEq(key, "soft_limit_en")) {
        g.softLimitEn = value >= 0.5f ? 1 : 0;
        Ffb::setSoftLimitEnabled(g.softLimitEn != 0);
        return true;
    }
    if (keyEq(key, "soft_limit_deg")) {
        g.softLimitDeg = clampf(value, 0.0f, 1440.0f);
        Ffb::setSoftLimitDeg(effectiveSoftLimitDeg(g));
        return true;
    }
    if (keyEq(key, "soft_limit_k")) {
        g.softLimitK = clampf(value, 0.0f, 1.0f);
        Ffb::setSoftLimitK(g.softLimitK);
        return true;
    }
    if (keyEq(key, "adxl_cal")) {
        g.adxlCalValid = value >= 0.5f ? 1 : 0;
        return true;
    }
    if (keyEq(key, "adxl_x_offset")) {
        if (value < -32768.0f) value = -32768.0f;
        if (value > 32767.0f) value = 32767.0f;
        g.adxlXOffset = (int16_t)lroundf(value);
        return true;
    }

    FfbLink::RimConfig rim = AccessoryLink::rimConfig();
    bool rimChanged = false;
    bool profileKey = false;

    if (keyEq(key, "panel_led_bright")) {
        rim.panelLedBright = (uint8_t)clampf(value, 0.0f, 255.0f);
        rimChanged = true;
        profileKey = true;
    } else if (keyEq(key, "shift_led_bright")) {
        rim.shiftLedBright = (uint8_t)clampf(value, 0.0f, 255.0f);
        rimChanged = true;
        profileKey = true;
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
        profileKey = true;
    } else {
        uint8_t idx = 0;
        const char *field = nullptr;
        if (parseEncKey(key, idx, field)) {
            if (keyEq(field, "value")) {
                AccessoryLink::setEncoderAbs(idx, (int16_t)clampf(value, 0.0f, 100.0f));
                return true;
            }
            if (!setEncField(rim.enc[idx], field, value)) return false;
            rimChanged = true;
        }
    }

    if (rimChanged) {
        applyRimLive(rim);
        if (profileKey) markCustom();
        return true;
    }
    return false;
}

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool getLayoutHex(char *out, size_t outLen) {
    return getLayoutPageHex(AccessoryLink::dispMeta().activePage, out, outLen);
}

bool getLayoutPageHex(uint8_t page, char *out, size_t outLen) {
    if (!out || outLen < (size_t)(1 + 2 * FfbLink::kLayoutBlobSize)) return false;
    if (page >= FfbLink::kDispPageMax) return false;
    const FfbLink::DisplayPage &p = AccessoryLink::dispPage(page);
    uint8_t blob[FfbLink::kLayoutBlobSize];
    blob[0] = p.layoutCount;
    memcpy(blob + 1, p.layout, sizeof(p.layout));
    static const char *kHex = "0123456789abcdef";
    size_t o = 0;
    for (uint8_t i = 0; i < FfbLink::kLayoutBlobSize; ++i) {
        const uint8_t b = blob[i];
        out[o++] = kHex[b >> 4];
        out[o++] = kHex[b & 0x0F];
    }
    out[o] = '\0';
    return true;
}

bool setLayoutHex(const char *hex) {
    return setLayoutPageHex(AccessoryLink::dispMeta().activePage,
                            AccessoryLink::dispPage(AccessoryLink::dispMeta().activePage).bgTheme, hex);
}

bool setLayoutPageHex(uint8_t page, uint8_t bgTheme, const char *hex) {
    if (!hex || page >= FfbLink::kDispPageMax) return false;
    while (*hex == ' ' || *hex == '\t') ++hex;
    const size_t n = strlen(hex);
    if (n != (size_t)(2 * FfbLink::kLayoutBlobSize)) return false;

    uint8_t blob[FfbLink::kLayoutBlobSize];
    for (uint8_t i = 0; i < FfbLink::kLayoutBlobSize; ++i) {
        const int hi = hexNibble(hex[i * 2]);
        const int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        blob[i] = (uint8_t)((hi << 4) | lo);
    }
    uint8_t count = blob[0];
    if (count > FfbLink::kDispElementMax) count = FfbLink::kDispElementMax;
    FfbLink::DisplayElement layout[FfbLink::kDispElementMax]{};
    memcpy(layout, blob + 1, sizeof(layout));
    return AccessoryLink::setDispPageLayout(page, bgTheme, count, layout);
}

bool handleDispCmd(char *args) {
    char *save = nullptr;
    char *sub = strtok_r(args, " \t", &save);
    if (!sub) {
        Serial.print("OK disp_page=");
        Serial.println(AccessoryLink::dispMeta().activePage);
        return true;
    }
    if (strcasecmp(sub, "page") == 0) {
        char *v = strtok_r(nullptr, " \t", &save);
        if (!v) {
            Serial.print("OK disp_page=");
            Serial.println(AccessoryLink::dispMeta().activePage);
            return true;
        }
        const int page = atoi(v);
        if (page < 0 || !AccessoryLink::setDispActivePage((uint8_t)page)) {
            Serial.println("ERR disp page");
            return true;
        }
        Serial.print("OK disp_page=");
        Serial.println(AccessoryLink::dispMeta().activePage);
        return true;
    }
    if (strcasecmp(sub, "pages") == 0) {
        char *v = strtok_r(nullptr, " \t", &save);
        if (!v) {
            Serial.print("OK disp_pages=");
            Serial.println(AccessoryLink::dispMeta().pageCount);
            return true;
        }
        if (!AccessoryLink::setDispPageCount((uint8_t)atoi(v))) {
            Serial.println("ERR disp pages");
            return true;
        }
        Serial.print("OK disp_pages=");
        Serial.println(AccessoryLink::dispMeta().pageCount);
        return true;
    }
    if (strcasecmp(sub, "swipe") == 0) {
        char *dir = strtok_r(nullptr, " \t", &save);
        if (!dir) {
            Serial.println("ERR usage: disp swipe L|R");
            return true;
        }
        const uint8_t cur = AccessoryLink::dispMeta().activePage;
        const uint8_t n = AccessoryLink::dispMeta().pageCount;
        if (dir[0] == 'L' || dir[0] == 'l' || dir[0] == '-') {
            if (cur + 1 < n) AccessoryLink::setDispActivePage((uint8_t)(cur + 1));
        } else {
            if (cur > 0) AccessoryLink::setDispActivePage((uint8_t)(cur - 1));
        }
        Serial.print("OK disp_page=");
        Serial.println(AccessoryLink::dispMeta().activePage);
        return true;
    }
    if (strcasecmp(sub, "sync") == 0) {
        AccessoryLink::requestDisplayPages();
        for (int i = 0; i < 80; ++i) {
            AccessoryLink::update();
            delay(5);
        }
        Serial.println("OK disp sync");
        return true;
    }
    Serial.println("ERR disp usage: page|pages|swipe|sync");
    return true;
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
    Serial.print("soft_limit_en=");
    Serial.println(g.softLimitEn ? 1 : 0);
    Serial.print("soft_limit_deg=");
    Serial.println(g.softLimitDeg, 2);
    Serial.print("soft_limit_k=");
    Serial.println(g.softLimitK, 6);
    Serial.print("adxl_cal=");
    Serial.println(g.adxlCalValid ? 1 : 0);
    Serial.print("adxl_x_offset=");
    Serial.println(g.adxlXOffset);
    Serial.print("adxl_present=");
    Serial.println(AccessoryLink::adxlPresent() ? 1 : 0);
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
    {
        const auto &meta = AccessoryLink::dispMeta();
        Serial.print("disp_page=");
        Serial.println(meta.activePage);
        Serial.print("disp_pages=");
        Serial.println(meta.pageCount);
        for (uint8_t i = 0; i < meta.pageCount && i < FfbLink::kDispPageMax; ++i) {
            Serial.print("page");
            Serial.print(i);
            Serial.print("_bg=");
            Serial.println(AccessoryLink::dispPage(i).bgTheme);
            char layoutHex[1 + 2 * FfbLink::kLayoutBlobSize];
            if (getLayoutPageHex(i, layoutHex, sizeof(layoutHex))) {
                Serial.print("layout_page");
                Serial.print(i);
                Serial.print("_hex=");
                Serial.println(layoutHex);
            }
        }
        char layoutHex[1 + 2 * FfbLink::kLayoutBlobSize];
        if (getLayoutHex(layoutHex, sizeof(layoutHex))) {
            Serial.print("layout_hex=");
            Serial.println(layoutHex);
        }
    }
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
        snprintf(buf, sizeof(buf), "enc%u_mode=%u", i, ec.mode);
        Serial.println(buf);
        snprintf(buf, sizeof(buf), "enc%u_value=%d", i, (int)AccessoryLink::encoderAbs(i));
        Serial.println(buf);
    }
    Serial.print("profile_active=");
    if (activeSlot == kProfileNone) {
        Serial.println("none");
    } else {
        Serial.println(activeSlot);
    }
    for (uint8_t i = 0; i < kProfileCount; ++i) {
        Serial.print("profile");
        Serial.print(i);
        Serial.print("_used=");
        Serial.println(profiles[i].used ? 1 : 0);
        Serial.print("profile");
        Serial.print(i);
        Serial.print("_name=");
        Serial.println(profiles[i].used && profiles[i].name[0] ? profiles[i].name : "");
    }
    Serial.println("OK end");
}

void quickProfileStep(bool next) {
    int start = activeSlot == kProfileNone ? -1 : (int)activeSlot;
    int dir = next ? 1 : -1;
    for (int n = 1; n <= (int)kProfileCount; ++n) {
        int cand = (start + dir * n) % (int)kProfileCount;
        if (cand < 0) cand += kProfileCount;
        if (profiles[(uint8_t)cand].used) {
            profileLoad((uint8_t)cand);
            return;
        }
    }
}

void quickBrightStep(bool shiftNotPanel, bool up) {
    FfbLink::RimConfig rim = AccessoryLink::rimConfig();
    uint8_t &b = shiftNotPanel ? rim.shiftLedBright : rim.panelLedBright;
    const int delta = up ? 8 : -8;
    int v = (int)b + delta;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    b = (uint8_t)v;
    AccessoryLink::setRimConfig(rim);
    if (AccessoryLink::linked()) {
        AccessoryLink::pushRimConfig();
    }
}

void quickRangeStep(bool up) {
    float r = HidWheel::rangeDeg();
    r += up ? 90.0f : -90.0f;
    if (r < 180.0f) r = 180.0f;
    if (r > 1080.0f) r = 1080.0f;
    setFloat("hid_range", r);
}

}  // namespace Settings
