#pragma once

#include "ffb_link.h"

namespace RimSettings {

void begin();   // EEPROM + load/apply (defaults if empty)
bool load();    // false → defaults kept
bool save();    // persist current RAM config
void resetDefaults();
void apply();   // push RAM config into Inputs / ShiftLeds / Display

FfbLink::RimConfig &config();
const FfbLink::RimConfig &cconfig();
void setConfig(const FfbLink::RimConfig &cfg);  // RAM + apply (no flash)

}  // namespace RimSettings
