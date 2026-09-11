#pragma once

#include <stdint.h>

// Shared UART / CDC framing between base-mcu and rim-mcu.
// See docs/link-protocol.md

namespace FfbLink {

static constexpr uint8_t kSync0 = 0xAA;
static constexpr uint8_t kSync1 = 0x55;
static constexpr uint8_t kVersion = 1;
static constexpr uint8_t kMaxPayload = 128;
static constexpr uint32_t kBaud = 115200;
static constexpr uint32_t kLinkTimeoutMs = 500;
static constexpr uint32_t kOtaMaxImageBytes = 192u * 1024u;

// --- Message types ---
enum Msg : uint8_t {
    Ping = 0x01,
    Pong = 0x02,
    Input = 0x10,
    Telemetry = 0x20,
    ShiftLed = 0x21,
    BtnLed = 0x22,
    Display = 0x23,
    CfgSync = 0x30,      // base→rim | RimConfig (apply live, do not persist)
    CfgAck = 0x31,       // rim→base | — (after CfgSave)
    CfgGet = 0x32,       // base→rim | — (request CfgReport)
    CfgReport = 0x33,    // rim→base | RimConfig (authoritative)
    CfgSave = 0x34,      // base→rim | — (persist rim EEPROM)
    VersionGet = 0x51,   // base→rim | —
    VersionReport = 0x52, // rim→base | ASCII id string (no NUL required; len = payload)
    EnterBootloader = 0x40,
    UpdaterReady = 0x41,
    FwBegin = 0x42,
    FwData = 0x43,
    FwAck = 0x44,
    FwNak = 0x45,
    FwEnd = 0x46,
    FwDone = 0x47,
    FwFail = 0x48,
    Log = 0x50,
};

struct __attribute__((packed)) FwBeginPayload {
    uint32_t size;
    uint32_t crc32;
};

// FwData payload: uint32_t offset + bytes...
// FwAck / FwNak payload: uint32_t offset (bytes received / fail offset)
// FwFail payload: uint8_t reason
enum FwFailReason : uint8_t {
    FwFailSize = 1,
    FwFailCrc = 2,
    FwFailOom = 3,
    FwFailSeq = 4,
    FwFailFlash = 5,
    FwFailTimeout = 6,  // idle in updater with no host traffic
};

inline uint32_t crc32(const uint8_t *data, uint32_t len) {
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; ++i) {
        c ^= data[i];
        for (int b = 0; b < 8; ++b) {
            const uint32_t mask = (uint32_t)-(int32_t)(c & 1u);
            c = (c >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~c;
}

// HID button allocation on the base gamepad (1-based Joystick.button IDs).
static constexpr uint8_t kHidPanelBtnFirst = 1;   // 1..10
static constexpr uint8_t kHidPanelBtnCount = 10;
static constexpr uint8_t kHidEncCwFirst = 11;     // 11,13,15,17
static constexpr uint8_t kHidEncCcwFirst = 12;    // 12,14,16,18
static constexpr uint8_t kHidEncSwitchFirst = 19; // 19..22
static constexpr uint8_t kHidReservedFirst = 23;  // 23..32
static constexpr uint8_t kEncoderCount = 4;

enum EncMode : uint8_t {
    EncModeRelative = 0,  // momentary CW/CCW pulses (default)
    EncModeHold = 1,      // hold CW or CCW while turning; release after idle
    EncModeAbsolute = 2,  // accumulate 0..100 value (telemetry / :get encN_value)
};

struct __attribute__((packed)) EncoderConfig {
    uint8_t invert;          // 0/1
    uint8_t stepsPerClick;   // detents per HID pulse (1..)
    uint8_t accelEnable;     // 0/1 (relative mode)
    uint8_t accelThreshold;  // detents/s before accel
    uint8_t accelMaxMult;    // 1..8
    uint8_t debounceMs;
    uint8_t pulseMs;         // HID press duration (relative) / hold idle ms (hold)
    uint8_t mode;            // EncMode
};

struct __attribute__((packed)) RimConfig {
    EncoderConfig enc[kEncoderCount];
    uint8_t panelLedBright;   // 0..255
    uint8_t shiftLedBright;   // 0..255
    uint8_t shiftLedCount;    // WS2812 count
    uint8_t dispBright;       // 0..255
    uint16_t shiftRpm[5];     // 0..3 fill stages, [4] = red-blink overrev
    uint8_t reserved[6];
};

struct __attribute__((packed)) InputPayload {
    uint32_t buttons;        // bit0 = panel btn 1 ... bit9 = btn 10
    int8_t encDelta[kEncoderCount];  // signed steps this frame (pre-policy raw)
    uint8_t encSwitch;       // bits 0..3
    uint8_t flags;           // bit0 = link app alive
};

struct __attribute__((packed)) TelemetryPayload {
    uint16_t rpm;
    int16_t speedKphx10;
    int8_t gear;             // -1 = R, 0 = N, 1..
    uint8_t flags;           // TelFlag bits
    uint16_t fuelPctx10;
    uint16_t lapTimeMs;
};

// 11× WS2812 layout: [0..1]=flags  [2..8]=RPM  [9..10]=TC/ABS
static constexpr uint8_t kLedFlagCount = 2;
static constexpr uint8_t kLedRpmCount = 7;
static constexpr uint8_t kLedAidCount = 2;
static constexpr uint8_t kLedFlagFirst = 0;
static constexpr uint8_t kLedRpmFirst = 2;
static constexpr uint8_t kLedAidFirst = 9;

enum TelFlag : uint8_t {
    TelYellow = 1u << 0,  // flag zone (LEDs 0–1), shared blink
    TelBlue = 1u << 1,    // flag zone (LEDs 0–1), shared blink
    TelTc = 1u << 2,      // aid zone (LEDs 9–10), shared blink
    TelAbs = 1u << 3,     // aid zone (LEDs 9–10), shared blink
    TelRed = 1u << 4,     // flag zone: both LEDs urgent red blink
    TelPit = 1u << 5,     // RPM zone: all 7 LEDs yellow blink (pit limiter)
};

// Shift / strip LED control (base → rim), also used by GUI tests.
enum ShiftLedMode : uint8_t {
    LedModeAuto = 0,      // follow telemetry RPM + flags
    LedModeOff = 1,       // clear strip, stay off until Auto
    LedModeSolid = 2,     // solid RGB
    LedModeFill = 3,      // light first `param` LEDs with RGB
    LedModeChase = 4,     // moving chase
    LedModeRainbow = 5,   // rainbow cycle
    LedModeRpm = 6,       // simulate RPM bar + optional flags
    LedModeBoot = 7,      // replay startup sequence
    LedModeZones = 8,     // show zone colors (flags / rpm / aids)
};

struct __attribute__((packed)) ShiftLedPayload {
    uint8_t mode;   // ShiftLedMode
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t param;  // fill count, etc.
    uint16_t rpm;   // for LedModeRpm
    uint8_t flags;  // TelFlag bits for LedModeRpm / tests
};

// Panel button LEDs (base → rim). bit0 = LED for panel btn 1 …
struct __attribute__((packed)) BtnLedPayload {
    uint16_t mask;
};

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF)
inline uint16_t crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

inline void defaultRimConfig(RimConfig &c) {
    c = RimConfig{};
    for (uint8_t i = 0; i < kEncoderCount; ++i) {
        c.enc[i].invert = 0;
        c.enc[i].stepsPerClick = 1;
        c.enc[i].accelEnable = 0;
        c.enc[i].accelThreshold = 8;
        c.enc[i].accelMaxMult = 4;
        c.enc[i].debounceMs = 2;
        c.enc[i].pulseMs = 35;
        c.enc[i].mode = EncModeRelative;
    }
    c.panelLedBright = 80;
    c.shiftLedBright = 60;
    c.shiftLedCount = 11;
    c.dispBright = 180;
    c.shiftRpm[0] = 5000;
    c.shiftRpm[1] = 6000;
    c.shiftRpm[2] = 7000;
    c.shiftRpm[3] = 7500;
    c.shiftRpm[4] = 7800;  // last two red LEDs blink at/above this
}

}  // namespace FfbLink
