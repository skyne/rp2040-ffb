#pragma once

#include <Adafruit_GFX.h>
#include <stdint.h>

#include "ffb_link.h"

// Built-in TFT backgrounds + 16×16 mono icon sprites.
namespace DispAssets {

void drawBackground(Adafruit_GFX &gfx, uint8_t theme);

// Draw icon at (x,y); scale 1..4; tint RGB565.
void drawIcon(Adafruit_GFX &gfx, uint8_t iconType, int16_t x, int16_t y, uint8_t scale,
              uint16_t color565);

bool isIconType(uint8_t type);
bool isButtonType(uint8_t type);

}  // namespace DispAssets
