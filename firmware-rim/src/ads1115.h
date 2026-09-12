#pragma once

#include <stdint.h>

#include "ffb_link.h"

// Optional ADS1115 on shared I2C (addr 0x48). Soft-fails when absent.
namespace Ads1115 {

void begin(); // probe + config; safe if chip missing
bool present();

// Round-robin single-shot samples (call from Core0 @ ~500 Hz).
void update();

// Latest raw single-ended counts (0 if never sampled / absent).
void fillAnalog(int16_t out[FfbLink::kAnalogCount]);

} // namespace Ads1115
