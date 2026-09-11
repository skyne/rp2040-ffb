#include "display_store.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

namespace DisplayStore {
namespace {

constexpr uint32_t kMagic = 0x31505344u;  // 'DSP1'
constexpr uint16_t kVersion = 1;
constexpr int kEepromSize = 512;
constexpr int kStoreOffset = 256;

struct Header {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
};

struct __attribute__((packed)) StoreBody {
    uint8_t pageCount;
    uint8_t activePage;
    FfbLink::DisplayPage pages[FfbLink::kDispPageMax];
};

Header hdr_{};
StoreBody body_{};
bool ready_ = false;

void seedDefaults() {
    body_ = StoreBody{};
    body_.pageCount = FfbLink::kDispPageMax;
    body_.activePage = 0;
    FfbLink::defaultDisplayPage0(body_.pages[0]);
    FfbLink::defaultDisplayPage1(body_.pages[1]);
    FfbLink::defaultDisplayPage2(body_.pages[2]);
}

}  // namespace

void begin() {
    EEPROM.begin(kEepromSize);
    seedDefaults();
    if (!load()) {
        // keep seeded defaults
    }
    ready_ = true;
}

bool load() {
    Header hdr{};
    EEPROM.get(kStoreOffset, hdr);
    if (hdr.magic != kMagic || hdr.version != kVersion || hdr.size != sizeof(StoreBody)) {
        return false;
    }
    StoreBody loaded{};
    EEPROM.get(kStoreOffset + (int)sizeof(Header), loaded);
    if (loaded.pageCount < 1 || loaded.pageCount > FfbLink::kDispPageMax) return false;
    if (loaded.activePage >= loaded.pageCount) loaded.activePage = 0;
    body_ = loaded;
    hdr_ = hdr;
    return true;
}

bool save() {
    Header hdr{kMagic, kVersion, (uint16_t)sizeof(StoreBody)};
    EEPROM.put(kStoreOffset, hdr);
    EEPROM.put(kStoreOffset + (int)sizeof(Header), body_);
    hdr_ = hdr;
    return EEPROM.commit();
}

uint8_t pageCount() { return body_.pageCount ? body_.pageCount : 1; }

uint8_t activePage() { return body_.activePage; }

void setPageCount(uint8_t n) {
    if (n < 1) n = 1;
    if (n > FfbLink::kDispPageMax) n = FfbLink::kDispPageMax;
    body_.pageCount = n;
    if (body_.activePage >= body_.pageCount) body_.activePage = 0;
}

bool setActivePage(uint8_t page) {
    if (page >= pageCount()) return false;
    body_.activePage = page;
    return true;
}

FfbLink::DisplayPage &page(uint8_t i) {
    if (i >= FfbLink::kDispPageMax) i = 0;
    return body_.pages[i];
}

const FfbLink::DisplayPage &cpage(uint8_t i) {
    if (i >= FfbLink::kDispPageMax) i = 0;
    return body_.pages[i];
}

void captureFromRimConfig(const FfbLink::RimConfig &cfg, uint8_t bgTheme) {
    FfbLink::DisplayPage &p = page(activePage());
    p.bgTheme = bgTheme;
    p.layoutCount = cfg.layoutCount;
    if (p.layoutCount > FfbLink::kDispElementMax) p.layoutCount = FfbLink::kDispElementMax;
    memcpy(p.layout, cfg.layout, sizeof(p.layout));
}

void mirrorActiveToRimConfig(FfbLink::RimConfig &cfg) {
    const FfbLink::DisplayPage &p = cpage(activePage());
    cfg.layoutCount = p.layoutCount;
    if (cfg.layoutCount > FfbLink::kDispElementMax) cfg.layoutCount = FfbLink::kDispElementMax;
    memcpy(cfg.layout, p.layout, sizeof(cfg.layout));
}

void resetDefaults() { seedDefaults(); }

}  // namespace DisplayStore
