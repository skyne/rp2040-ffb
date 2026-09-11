#pragma once

#include <stdint.h>

#include "ffb_link.h"

namespace ShiftLeds {

// Core0: init mutex + defaults only (no WS2812 / PIO).
void begin();

// Core1: owns NeoPixel strip + boot sequence. Call from setup1().
void beginCore1();

// Core0 writers — queue state under mutex; never touch the strip.
void setConfig(const FfbLink::RimConfig &cfg);
void setTelemetry(const FfbLink::TelemetryPayload &tel);
void clearTelemetry();  // invalidate live tel → Auto standby (link OK, no race data)
void setPowerSave(bool on);  // ADXL idle: blank strip (no-op when ADXL absent / never set)
void setTest(const FfbLink::ShiftLedPayload &cmd);
void showOta();   // middle RPM LED orange (OTA in progress)
void clearOta();  // leave OTA cue; restore LedModeAuto

// Core1 render loop (~60 FPS). Call from loop1() only.
void update();

}  // namespace ShiftLeds
