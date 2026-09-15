#pragma once

/**
 * USB HID PID (Physical Interface Device) report layouts.
 * Adapted from the MIT-licensed ArduinoJoystickWithFFBLibrary report types
 * (Tero Loimuneva / Jaka Simonic / Hoan Tran / Yo Law).
 */

#include <stdint.h>

namespace HidPid {

constexpr uint8_t kMaxEffects = 12;
constexpr uint8_t kMaxAxes = 2;
constexpr uint16_t kEffectSlotBytes = 64; // approximate pool accounting
constexpr uint16_t kMemorySize = (uint16_t)(kMaxEffects * kEffectSlotBytes);

constexpr uint16_t kDurationInfinite = 0x7FFF;

constexpr uint8_t kEffectConstant = 0x01;
constexpr uint8_t kEffectRamp = 0x02;
constexpr uint8_t kEffectSquare = 0x03;
constexpr uint8_t kEffectSine = 0x04;
constexpr uint8_t kEffectTriangle = 0x05;
constexpr uint8_t kEffectSawtoothDown = 0x06;
constexpr uint8_t kEffectSawtoothUp = 0x07;
constexpr uint8_t kEffectSpring = 0x08;
constexpr uint8_t kEffectDamper = 0x09;
constexpr uint8_t kEffectInertia = 0x0A;
constexpr uint8_t kEffectFriction = 0x0B;
constexpr uint8_t kEffectCustom = 0x0C;

constexpr uint8_t kStateFree = 0x00;
constexpr uint8_t kStateAllocated = 0x01;
constexpr uint8_t kStatePlaying = 0x02;

constexpr uint8_t kXAxisEnable = 0x01;
constexpr uint8_t kYAxisEnable = 0x02;
constexpr uint8_t kDirectionEnable = 0x04;

// Joystick input report ID 1 (axes/buttons).
constexpr uint8_t kReportIdInput = 1;
// PID State input report ID 2.
constexpr uint8_t kReportIdPidState = 2;

#pragma pack(push, 1)

struct JoystickInputReport {
    uint8_t buttons[4];
    uint8_t hat; // bits 0..3 hat, 4..7 padding
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t rx;
    int16_t ry;
    int16_t rz;
};

struct PidStateInput {
    uint8_t status;           // bit0 paused, bit1 actuators, …
    uint8_t effectBlockIndex; // bit7 playing, bits0..6 id
};

struct SetEffectOut {
    uint8_t effectBlockIndex;
    uint8_t effectType;
    uint16_t duration;
    uint16_t triggerRepeatInterval;
    uint16_t samplePeriod;
    uint8_t gain;
    uint8_t triggerButton;
    uint8_t enableAxis;
    uint8_t directionX;
    uint8_t directionY;
};

struct SetEnvelopeOut {
    uint8_t effectBlockIndex;
    uint16_t attackLevel;
    uint16_t fadeLevel;
    uint32_t attackTime;
    uint32_t fadeTime;
};

struct SetConditionOut {
    uint8_t effectBlockIndex;
    uint8_t parameterBlockOffset; // low nibble = axis
    int16_t cpOffset;
    int16_t positiveCoefficient;
    int16_t negativeCoefficient;
    uint16_t positiveSaturation;
    uint16_t negativeSaturation;
    uint16_t deadBand;
};

struct SetPeriodicOut {
    uint8_t effectBlockIndex;
    uint16_t magnitude;
    int16_t offset;
    uint16_t phase;
    uint32_t period;
};

struct SetConstantForceOut {
    uint8_t effectBlockIndex;
    int16_t magnitude;
};

struct SetRampForceOut {
    uint8_t effectBlockIndex;
    int16_t startMagnitude;
    int16_t endMagnitude;
};

struct EffectOperationOut {
    uint8_t effectBlockIndex;
    uint8_t operation; // 1 start, 2 startSolo, 3 stop
    uint8_t loopCount;
};

struct BlockFreeOut {
    uint8_t effectBlockIndex;
};

struct DeviceControlOut {
    uint8_t control; // 1..6 enum
};

struct DeviceGainOut {
    uint8_t gain;
};

struct CreateNewEffectFeature {
    uint8_t effectType;
    uint16_t byteCount;
};

struct PidBlockLoadFeature {
    uint8_t effectBlockIndex;
    uint8_t loadStatus; // 1 success, 2 full, 3 error
    uint16_t ramPoolAvailable;
};

struct PidPoolFeature {
    uint16_t ramPoolSize;
    uint8_t maxSimultaneousEffects;
    uint8_t memoryManagement; // bits: device managed + shared
};

#pragma pack(pop)

struct EffectCondition {
    int16_t cpOffset = 0;
    int16_t positiveCoefficient = 0;
    int16_t negativeCoefficient = 0;
    uint16_t positiveSaturation = 10000;
    uint16_t negativeSaturation = 10000;
    uint16_t deadBand = 0;
};

struct EffectState {
    uint8_t state = kStateFree;
    uint8_t effectType = 0;
    int16_t offset = 0;
    uint8_t gain = 255;
    int16_t attackLevel = 0;
    int16_t fadeLevel = 0;
    uint32_t attackTime = 0;
    uint32_t fadeTime = 0;
    int16_t magnitude = 0;
    uint8_t enableAxis = kXAxisEnable;
    uint8_t directionX = 0;
    uint8_t directionY = 0;
    uint8_t conditionBlocksCount = 0;
    EffectCondition conditions[kMaxAxes];
    uint16_t phase = 0;
    int16_t startMagnitude = 0;
    int16_t endMagnitude = 0;
    uint32_t period = 0;
    uint16_t duration = 0;
    uint32_t elapsedTime = 0;
    uint32_t startMs = 0;
};

} // namespace HidPid
