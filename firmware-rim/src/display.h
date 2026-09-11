#pragma once

#include <stdint.h>

#include "ffb_link.h"

// Core1 ILI9341 / LVGL placeholder. Tracks telemetry validity for standby UI.
namespace Display {

void begin();       // Core0: mutex only
void beginCore1();  // Core1: panel init (no-op until TFT driver)

// Core0: mirror telemetry watchdog (true = live race data).
void setTelemetryValid(bool valid);
void setTelemetry(const FfbLink::TelemetryPayload &tel);
void setPowerSave(bool on);  // ADXL idle blank / wake (TFT stub ready)

// Core1: draw standby / live dashboard when the panel exists.
void update();

bool telemetryValid();

}  // namespace Display
