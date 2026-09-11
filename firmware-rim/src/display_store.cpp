#include "display_store.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

namespace DisplayStore {
namespace {

constexpr uint32_t kMagic = 0x32505344u;  // 'DSP2' — 16-widget pages
constexpr uint16_t kVersion = 2;
constexpr int kEepromSize = 1024;
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

// Per-page assemble state for chunked writes.
uint8_t chunkExpect_[FfbLink::kDispPageMax]{};
uint8_t chunkGot_[FfbLink::kDispPageMax]{};

void seedDefaults() {
    body_ = StoreBody{};
    body_.pageCount = FfbLink::kDispPageMax;
    body_.activePage = 0;
    FfbLink::defaultDisplayPage0(body_.pages[0]);
    FfbLink::defaultDisplayPage1(body_.pages[1]);
    FfbLink::defaultDisplayPage2(body_.pages[2]);
    memset(chunkExpect_, 0, sizeof(chunkExpect_));
    memset(chunkGot_, 0, sizeof(chunkGot_));
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
    for (uint8_t i = 0; i < FfbLink::kDispPageMax; ++i) {
        if (loaded.pages[i].layoutCount > FfbLink::kDispElementMax) {
            loaded.pages[i].layoutCount = FfbLink::kDispElementMax;
        }
    }
    body_ = loaded;
    hdr_ = hdr;
    memset(chunkExpect_, 0, sizeof(chunkExpect_));
    memset(chunkGot_, 0, sizeof(chunkGot_));
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

bool applyChunk(const FfbLink::DispPageChunkPayload &chunk) {
    if (chunk.pageIndex >= FfbLink::kDispPageMax) return false;
    uint8_t total = chunk.layoutCount;
    if (total > FfbLink::kDispElementMax) total = FfbLink::kDispElementMax;
    uint8_t count = chunk.count;
    if (count > FfbLink::kDispChunkElements) count = FfbLink::kDispChunkElements;
    if (chunk.start >= FfbLink::kDispElementMax) return false;
    if ((uint16_t)chunk.start + count > FfbLink::kDispElementMax) {
        count = (uint8_t)(FfbLink::kDispElementMax - chunk.start);
    }

    FfbLink::DisplayPage &dst = body_.pages[chunk.pageIndex];
    if (chunk.start == 0) {
        dst = FfbLink::DisplayPage{};
        dst.bgTheme = chunk.bgTheme;
        dst.layoutCount = total;
        chunkExpect_[chunk.pageIndex] = total;
        chunkGot_[chunk.pageIndex] = 0;
    } else if (chunkExpect_[chunk.pageIndex] == 0) {
        // Mid-chunk without start — adopt declared total.
        dst.layoutCount = total;
        chunkExpect_[chunk.pageIndex] = total;
    }

    memcpy(dst.layout + chunk.start, chunk.elements, count * sizeof(FfbLink::DisplayElement));
    const uint8_t end = (uint8_t)(chunk.start + count);
    if (end > chunkGot_[chunk.pageIndex]) chunkGot_[chunk.pageIndex] = end;

    const uint8_t expect = chunkExpect_[chunk.pageIndex];
    return expect == 0 || chunkGot_[chunk.pageIndex] >= expect;
}

void resetDefaults() { seedDefaults(); }

}  // namespace DisplayStore
