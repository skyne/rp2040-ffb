#include "rim_settings.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

#include "display.h"
#include "display_store.h"
#include "inputs.h"
#include "shift_leds.h"

namespace RimSettings {
namespace {

constexpr uint32_t kMagic = 0x314D4952u;  // 'RIM1'
constexpr uint16_t kVersion = 2;
constexpr uint16_t kVersionV1 = 1;
constexpr int kEepromSize = 512;

struct __attribute__((packed)) RimConfigV1 {
    FfbLink::EncoderConfig enc[FfbLink::kEncoderCount];
    uint8_t panelLedBright;
    uint8_t shiftLedBright;
    uint8_t shiftLedCount;
    uint8_t dispBright;
    uint16_t shiftRpm[5];
    uint8_t reserved[6];
};

struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
};

FfbLink::RimConfig g{};

void copyV1Fields(const RimConfigV1 &v1) {
    memcpy(g.enc, v1.enc, sizeof(g.enc));
    g.panelLedBright = v1.panelLedBright;
    g.shiftLedBright = v1.shiftLedBright;
    g.shiftLedCount = v1.shiftLedCount;
    g.dispBright = v1.dispBright;
    memcpy(g.shiftRpm, v1.shiftRpm, sizeof(g.shiftRpm));
    FfbLink::defaultDisplayLayout(g);
}

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
    if (hdr.magic != kMagic) return false;

    if (hdr.version == kVersion && hdr.size == sizeof(FfbLink::RimConfig)) {
        FfbLink::RimConfig loaded{};
        EEPROM.get(sizeof(Header), loaded);
        g = loaded;
        if (g.layoutCount > FfbLink::kDispElementMax) {
            g.layoutCount = FfbLink::kDispElementMax;
        }
        return true;
    }

    if (hdr.version == kVersionV1 && hdr.size == sizeof(RimConfigV1)) {
        RimConfigV1 legacy{};
        EEPROM.get(sizeof(Header), legacy);
        FfbLink::defaultRimConfig(g);
        copyV1Fields(legacy);
        return true;
    }

    return false;
}

bool save() {
    Header hdr{kMagic, kVersion, (uint16_t)sizeof(FfbLink::RimConfig)};
    EEPROM.put(0, hdr);
    EEPROM.put(sizeof(Header), g);
    const bool okRim = EEPROM.commit();
    const bool okDisp = DisplayStore::save();
    return okRim && okDisp;
}

void resetDefaults() {
    FfbLink::defaultRimConfig(g);
    DisplayStore::resetDefaults();
}

void apply() {
    Inputs::setConfig(g);
    ShiftLeds::setConfig(g);
    Display::setConfig(g);
}

FfbLink::RimConfig &config() { return g; }
const FfbLink::RimConfig &cconfig() { return g; }

void setConfig(const FfbLink::RimConfig &cfg) {
    g = cfg;
    if (g.layoutCount > FfbLink::kDispElementMax) {
        g.layoutCount = FfbLink::kDispElementMax;
    }
    apply();
}

}  // namespace RimSettings
