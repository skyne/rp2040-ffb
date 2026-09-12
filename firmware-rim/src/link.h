#pragma once

#include <stdint.h>

#include "ffb_link.h"

namespace Link {

void begin();
void update();

bool sendMsg(uint8_t type, const void* payload, uint8_t len);

bool linked(); // true if a valid frame from base arrived recently
uint32_t lastRxMs();
void noteRx(); // call from frame handler if needed; also set on parse

using FrameHandler = void (*)(uint8_t type, const uint8_t* payload, uint8_t len);
void setHandler(FrameHandler h);

} // namespace Link
