#pragma once

#include "ffb_link.h"

// Multi-page TFT bank in rim EEPROM (offset 256; RimConfig occupies 0..).
namespace DisplayStore {

void begin();  // load or seed defaults
bool load();
bool save();

uint8_t pageCount();
uint8_t activePage();
void setPageCount(uint8_t n);
bool setActivePage(uint8_t page);  // clamps; mirrors into RimConfig layout via Display

FfbLink::DisplayPage &page(uint8_t i);
const FfbLink::DisplayPage &cpage(uint8_t i);

// Copy RimConfig.layout (+ optional bg) into active page slot.
void captureFromRimConfig(const FfbLink::RimConfig &cfg, uint8_t bgTheme);
// Write active page layout into RimConfig fields (brightness untouched).
void mirrorActiveToRimConfig(FfbLink::RimConfig &cfg);

void resetDefaults();  // 3 pages seeded

}  // namespace DisplayStore
