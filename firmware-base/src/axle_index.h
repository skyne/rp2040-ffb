#pragma once

#include <stdint.h>

namespace AxleIndex {

void begin();

// Poll pin; call every loop. Returns true on a fresh magnet edge (enter active).
bool update();

bool active();
bool rawHigh();
uint32_t edgeCount();

void armSyncOnNextEdge();
bool syncArmed();

} // namespace AxleIndex
