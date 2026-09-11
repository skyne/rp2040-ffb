#pragma once

#include <stdint.h>

#include "ffb_link.h"

namespace ShiftLeds {

void begin();  // init strip + blocking startup sequence
void setConfig(const FfbLink::RimConfig &cfg);
void setTelemetry(const FfbLink::TelemetryPayload &tel);
void setTest(const FfbLink::ShiftLedPayload &cmd);
void showOta();  // middle RPM LED orange, others off (OTA in progress)
void update();

}  // namespace ShiftLeds
