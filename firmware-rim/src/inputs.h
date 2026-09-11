#pragma once

#include <stdint.h>

#include "ffb_link.h"

namespace Inputs {

void begin();
void update();  // sample encoders / buttons

void setConfig(const FfbLink::RimConfig &cfg);
const FfbLink::RimConfig &config();

// Panel LEDs on PCA_ADDR_LED0 (active-low). Bits 0..7 → LEDs 1..8; 8..9 ignored if no expander.
void setPanelLeds(uint16_t maskOn);

// Fill INPUT payload (encDelta = raw detent steps this period)
void fillInput(FfbLink::InputPayload &out);

}  // namespace Inputs
