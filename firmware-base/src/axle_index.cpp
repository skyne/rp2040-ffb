#include "axle_index.h"

#include <Arduino.h>

#include "config.h"

namespace AxleIndex {
namespace {

bool lastActive = false;
bool haveSample = false;
uint32_t edges = 0;
bool armSync = false;

bool readActive() {
    const bool high = digitalRead(PIN_AXLE_INDEX) == HIGH;
    return AXLE_INDEX_ACTIVE_LOW ? !high : high;
}

}  // namespace

void begin() {
    pinMode(PIN_AXLE_INDEX, INPUT_PULLUP);
    haveSample = false;
    edges = 0;
    armSync = false;
    lastActive = readActive();
    haveSample = true;
}

bool update() {
    const bool now = readActive();
    bool edge = false;

    if (haveSample && now && !lastActive) {
        edge = true;
        edges++;
        if (armSync) armSync = false;
    }

    lastActive = now;
    return edge;
}

bool active() { return lastActive; }
bool rawHigh() { return digitalRead(PIN_AXLE_INDEX) == HIGH; }
uint32_t edgeCount() { return edges; }

void armSyncOnNextEdge() { armSync = true; }
bool syncArmed() { return armSync; }

}  // namespace AxleIndex
