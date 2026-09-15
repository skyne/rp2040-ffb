#pragma once

#include <stdint.h>

// Optional USB mass-storage "setup" volume with Win/Mac/Linux install scripts.
// Interface is registered once at USB bring-up (no later disconnect/re-enum).
// Media stays not-ready until grace expires; host app keeps it hidden.
namespace SetupMsc {

/** Call while USB is disconnected, after HID registration, before USB.connect(). */
void attachAtBoot();

void beginGrace(uint32_t graceMs = 2000);
void service();
void notifyHostApp();
bool visible();

} // namespace SetupMsc
