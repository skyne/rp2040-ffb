#pragma once

#include <stdint.h>

#include "ffb_link.h"

namespace Inputs {

void begin();
void update();  // sample encoders / buttons

void setConfig(const FfbLink::RimConfig &cfg);
const FfbLink::RimConfig &config();

// Fill INPUT payload (encDelta = raw detent steps this period)
void fillInput(FfbLink::InputPayload &out);

}  // namespace Inputs
