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
void setConfig(const FfbLink::RimConfig &cfg);  // brightness + capture layout into active page

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

// Apply a full page blob into the store (link DispOpSetPage).
void setStorePage(const FfbLink::DispPageSetPayload &page);
void setStoreMeta(const FfbLink::DispMetaPayload &meta);
void getStoreMeta(FfbLink::DispMetaPayload &out);
void getStorePage(uint8_t index, FfbLink::DispPageSetPayload &out);

}  // namespace Display
