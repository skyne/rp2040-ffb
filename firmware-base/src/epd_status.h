#pragma once

#include <stdint.h>

namespace EpdStatus {

// Core0: queue-only API (never blocks on EPD SPI / BUSY).
void begin();
void update(bool hallOk, float axleDeg);
void requestRefresh();
void forceRefresh();  // full redraw ASAP (still async on core1)
bool enabled();
void setEnabled(bool on);

// Core1: owns the panel + PIO SPI. Call from setup1 / loop1 only.
void beginCore1();
void serviceCore1();

}  // namespace EpdStatus
