#pragma once

#include "ffb_link.h"

// Multi-page TFT bank in rim EEPROM (offset 256; RimConfig occupies low EEPROM).
namespace DisplayStore {

void begin();  // load or seed defaults
bool load();
bool save();

uint8_t pageCount();
uint8_t activePage();
void setPageCount(uint8_t n);
bool setActivePage(uint8_t page);  // clamps to pageCount

FfbLink::DisplayPage &page(uint8_t i);
const FfbLink::DisplayPage &cpage(uint8_t i);

// Apply one chunk of a page (DispOpSetPageChunk). Returns true when page is complete.
bool applyChunk(const FfbLink::DispPageChunkPayload &chunk);

void resetDefaults();  // 3 pages seeded

}  // namespace DisplayStore
