#pragma once

#include <stdint.h>

#include "pedals.h"

namespace Settings {

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

void dumpToSerial();

}  // namespace Settings
