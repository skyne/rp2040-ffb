#pragma once

#include <stdint.h>

namespace FfbPid {

void begin();

// Host HID callbacks (buffer excludes report ID when TinyUSB stripped it).
void onSetReport(uint8_t reportId, uint8_t reportType, const uint8_t* data, uint16_t len);
uint16_t onGetReport(uint8_t reportId, uint8_t reportType, uint8_t* data, uint16_t len);

// Normalized axle state for condition effects. position/velocity in roughly ±1.
void setAxisState(float positionNorm, float velocityNorm, float accelNorm);

// Sum active effects → torque in roughly [-1, +1] before device/user gain.
float computeTorque();

bool actuatorsEnabled();
bool devicePaused();
uint8_t deviceGain(); // 0..255
uint8_t playingCount();
uint8_t allocatedCount();

// Auto mode hints for Ffb::
bool autoEnterRequested();
void clearAutoEnter();
bool autoExitRequested();
void clearAutoExit();

// ---------------------------------------------------------------------------
// Host-script / showcase inject API — same effect pool as USB HID SET_REPORT.
// Slot conventions for :pid_arm: 1=spring, 2=sine rumble, 3=constant kick.
// ---------------------------------------------------------------------------
static constexpr uint8_t kDemoSpringId = 1;
static constexpr uint8_t kDemoSineId = 2;
static constexpr uint8_t kDemoConstId = 3;

/** Free all effects, enable actuators, gain=255, request auto-enter. */
void demoReset();

/**
 * Ensure demo slots exist and spring is playing (sine/const allocated, stopped).
 * Spring uses a strong condition coefficient so cpOffset steers the wheel.
 */
void demoArmEffects();

/** Spring condition toward axle degrees (using halfRangeDeg for ±10000 normalize). */
void demoSetSpringDeg(float targetDeg, float halfRangeDeg);

/** Last demo spring target in degrees (0 if not armed). Used by runaway vs-target check. */
float demoSpringTargetDeg();

/**
 * Periodic sine rumble. amp01 in [0,1] → magnitude; hz → period.
 * amp<=0 stops the effect; amp>0 starts/updates it.
 */
void demoSetRumble(float amp01, float hz);

/**
 * Constant kick/force. mag01 in [-1,1] → magnitude ±10000.
 * |mag| small → stop.
 */
void demoSetConstant(float mag01);

/** Stop + free all effects and request auto-exit (caller should Mode::Off). */
void demoEnd();

void setDeviceGain(uint8_t gain);

} // namespace FfbPid
