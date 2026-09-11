#pragma once

#include <stdint.h>

#include "ffb_link.h"

// Core1 ILI9341 multi-page dashboard. Soft-fails when the panel is absent.
namespace Display {

void begin();       // Core0: mutex + DisplayStore
void beginCore1();  // Core1: panel init

void setTelemetryValid(bool valid);
void setTelemetry(const FfbLink::TelemetryPayload &tel);
void setPowerSave(bool on);
void setConfig(const FfbLink::RimConfig &cfg);  // brightness only (layouts in DisplayStore)

void update();

bool telemetryValid();
bool present();

// Page nav (CDC / future touch).
uint8_t activePage();
uint8_t pageCount();
bool setActivePage(uint8_t page);
void setPageCount(uint8_t n);
bool onTap(int16_t x, int16_t y);  // hit-test buttons
bool onSwipe(int16_t dx);          // |dx|>=40 → prev/next

// DisplayStore sync (0x23).
void setStorePageChunk(const FfbLink::DispPageChunkPayload &chunk);
void setStorePageLegacy(const FfbLink::DispPageSetPayload &page);  // first 8 widgets
void setStoreMeta(const FfbLink::DispMetaPayload &meta);
void getStoreMeta(FfbLink::DispMetaPayload &out);
// Fill one chunk for GetAll / push. Returns false when start >= layoutCount.
bool fillStorePageChunk(uint8_t pageIndex, uint8_t start, FfbLink::DispPageChunkPayload &out);

}  // namespace Display
