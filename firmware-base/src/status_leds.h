#pragma once

namespace StatusLeds {

void begin();

// Call every loop (throttles internally).
void update(bool hallOk, bool indexActive, float axleDeg);

// Solid orange flash (e.g. immediately before base BOOTSEL reboot).
void showUpdateBrief();

} // namespace StatusLeds
