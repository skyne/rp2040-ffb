#include "rim_settings.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

#include "inputs.h"
#include "shift_leds.h"

namespace RimSettings {
namespace {

constexpr uint32_t kMagic = 0x314D4952u;  // 'RIM1'
constexpr uint16_t kVersion = 1;
constexpr int kEepromSize = 256;

struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
};

FfbLink::RimConfig g{};

}  // namespace

void begin() {
    EEPROM.begin(kEepromSize);
    FfbLink::defaultRimConfig(g);
    if (!load()) {
        // keep defaults
    }
    apply();
}

bool load() {
    Header hdr{};
    EEPROM.get(0, hdr);
    if (hdr.magic != kMagic || hdr.version != kVersion || hdr.size != sizeof(FfbLink::RimConfig)) {
        return false;
    }
    FfbLink::RimConfig loaded{};
    EEPROM.get(sizeof(Header), loaded);
    g = loaded;
    return true;
}

bool save() {
    Header hdr{kMagic, kVersion, (uint16_t)sizeof(FfbLink::RimConfig)};
    EEPROM.put(0, hdr);
    EEPROM.put(sizeof(Header), g);
    return EEPROM.commit();
}

void resetDefaults() {
    FfbLink::defaultRimConfig(g);
}

void apply() {
    Inputs::setConfig(g);
    ShiftLeds::setConfig(g);
}

FfbLink::RimConfig &config() { return g; }
const FfbLink::RimConfig &cconfig() { return g; }

void setConfig(const FfbLink::RimConfig &cfg) {
    g = cfg;
    apply();
}

}  // namespace RimSettings
