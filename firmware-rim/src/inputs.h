#pragma once

#include <stdint.h>

#include "ffb_link.h"

namespace Inputs {

void begin();
void update(); // sample encoders / buttons / ADS1115 / LDR

void setConfig(const FfbLink::RimConfig& cfg);
const FfbLink::RimConfig& config();

// Panel LEDs on MCP23017 @ 0x21 (active-low). Bits 0..9 → LEDs 1..10. No-op if absent.
void setPanelLeds(uint16_t maskOn);

// Fill INPUT payload (encDelta = raw detent steps this period)
void fillInput(FfbLink::InputPayload& out);

// Ambient dim scale 0..255 for panel PWM + shift LEDs.
// 255 when LDR absent / not readable (full brightness).
uint8_t ambientScale();
bool ldrPresent();

} // namespace Inputs
