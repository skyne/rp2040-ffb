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
constexpr uint16_t kVersion = 3;          // layout removed from RimConfig
constexpr uint16_t kVersionV2 = 2;
constexpr uint16_t kVersionV1 = 1;
constexpr int kEepromSize = 1024;

struct __attribute__((packed)) RimConfigV1 {
    FfbLink::EncoderConfig enc[FfbLink::kEncoderCount];
    uint8_t panelLedBright;
    uint8_t shiftLedBright;
    uint8_t shiftLedCount;
    uint8_t dispBright;
    uint16_t shiftRpm[5];
    uint8_t reserved[6];
};

// v2 had active-page layout mirror (8 widgets) inside RimConfig.
struct __attribute__((packed)) RimConfigV2 {
    FfbLink::EncoderConfig enc[FfbLink::kEncoderCount];
    uint8_t panelLedBright;
    uint8_t shiftLedBright;
    uint8_t shiftLedCount;
    uint8_t dispBright;
    uint16_t shiftRpm[5];
    uint8_t layoutCount;
    FfbLink::DisplayElement layout[8];
};

struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
};

FfbLink::RimConfig g{};

void copyCoreFields(const FfbLink::EncoderConfig *enc, uint8_t panel, uint8_t shiftBright,
                    uint8_t shiftCount, uint8_t disp, const uint16_t *rpm) {
    memcpy(g.enc, enc, sizeof(g.enc));
    g.panelLedBright = panel;
    g.shiftLedBright = shiftBright;
    g.shiftLedCount = shiftCount;
    g.dispBright = disp;
    memcpy(g.shiftRpm, rpm, sizeof(g.shiftRpm));
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
        return true;
    }

    if (hdr.version == kVersionV2 && hdr.size == sizeof(RimConfigV2)) {
        RimConfigV2 legacy{};
        EEPROM.get(sizeof(Header), legacy);
        FfbLink::defaultRimConfig(g);
        uint16_t rpm[5];
        memcpy(rpm, legacy.shiftRpm, sizeof(rpm));
        copyCoreFields(legacy.enc, legacy.panelLedBright, legacy.shiftLedBright, legacy.shiftLedCount,
                       legacy.dispBright, rpm);
        return true;
    }

    if (hdr.version == kVersionV1 && hdr.size == sizeof(RimConfigV1)) {
        RimConfigV1 legacy{};
        EEPROM.get(sizeof(Header), legacy);
        FfbLink::defaultRimConfig(g);
        uint16_t rpm[5];
        memcpy(rpm, legacy.shiftRpm, sizeof(rpm));
        copyCoreFields(legacy.enc, legacy.panelLedBright, legacy.shiftLedBright, legacy.shiftLedCount,
                       legacy.dispBright, rpm);
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
    apply();
}

}  // namespace RimSettings
