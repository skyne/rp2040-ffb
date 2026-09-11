#pragma once

#include <stdint.h>

#include "pedals.h"

namespace Settings {

static constexpr uint8_t kProfileCount = 4;
static constexpr uint8_t kProfileNameLen = 12;
static constexpr uint8_t kProfileNone = 0xFF;

// Named FFB / LED / rotation snapshot (not pedals or gear_ratio).
struct __attribute__((packed)) Profile {
    char name[kProfileNameLen];  // NUL-terminated
    float dutyCap;
    float springK;
    float springDz;
    float torqueCap;
    float hidRange;
    uint8_t panelLedBright;
    uint8_t shiftLedBright;
    uint16_t shiftRpm[5];
    uint8_t used;  // 0 = empty slot
    uint8_t _pad[3];
};

// Base-mcu only. RimConfig lives on the rim (EEPROM) and is cached in AccessoryLink.
struct Data {
    float dutyCap = 0.35f;
    float springK = 0.004f;
    float springDz = 2.0f;
    float torqueCap = 0.35f;
    float hidRange = 900.0f;
    float gearRatio = 18.0f;
    Pedals::AxisCal thr{};
    Pedals::AxisCal brk{};
    Pedals::AxisCal clu{};
    float softLimitDeg = 0.0f;   // 0 → use hidRange/2
    float softLimitK = 0.012f;
    uint8_t softLimitEn = 1;
    uint8_t adxlCalValid = 0;    // 1 = adxlXOffset captured at visual center
    int16_t adxlXOffset = 0;     // raw ADXL X at mechanical center
};

void begin();              // defaults in RAM; does not touch flash
bool load();               // false → kept defaults
bool save();               // base EEPROM + CfgSave to rim
void resetDefaults();      // base defaults only (rim unchanged)
void apply();              // push base RAM values into base subsystems

Data &data();
const Data &cdata();

bool telemetryEnabled();
void setTelemetryEnabled(bool on);

// Returns true if key known. get → writes value; set → parses and applies live (no flash).
bool getFloat(const char *key, float &out);
bool setFloat(const char *key, float value);

// Rim TFT layout blob (layoutCount + DisplayElement[8]) as lowercase hex.
bool getLayoutHex(char *out, size_t outLen);       // active page
bool setLayoutHex(const char *hex);                // active page + CfgSync
bool getLayoutPageHex(uint8_t page, char *out, size_t outLen);
bool setLayoutPageHex(uint8_t page, uint8_t bgTheme, const char *hex);
bool handleDispCmd(char *args);  // "page"/"swipe"/"pages" after :disp_

void dumpToSerial();

// Profiles (slots 0..kProfileCount-1). Load/save apply live; persist with Settings::save().
uint8_t activeProfile();  // kProfileNone if custom / none
const Profile &profile(uint8_t slot);
bool profileSave(uint8_t slot, const char *nameOrNull);  // capture live → slot
bool profileLoad(uint8_t slot);                          // apply slot live
bool profileClear(uint8_t slot);
bool profileRename(uint8_t slot, const char *name);
void profilesDumpToSerial();
bool handleProfileCmd(char *args);  // strtok-rest after "profile"; prints OK/ERR

// On-rim quick menu helpers (hold encoder shaft switch + turn).
void quickProfileStep(bool next);
void quickBrightStep(bool shiftNotPanel, bool up);
void quickRangeStep(bool up);

}  // namespace Settings
