#pragma once

#include <stdint.h>

namespace Updater {

void begin();
bool active();
void enter();
void onFrame(uint8_t type, const uint8_t *payload, uint8_t len);
void update();  // idle timeout while in soft updater

}  // namespace Updater
