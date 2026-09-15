#pragma once

/**
 * @file ffb_link.h
 * @brief Inter-MCU Communication Protocol for rp2040-ffb
 *
 * This file defines the complete binary framed protocol for communication between:
 *  - Base MCU (Pico #1): Motor control, wheel angle, pedals, USB HID
 *  - Rim MCU (Pico #2): Buttons, encoders, shift lights, TFT display
 *
 * UART Physical Layer:
 *  - Baud rate: 460,800 baud (kBaud)
 *  - Hardware: Serial1 on both Picos
 *  - Flow control: None (RX FIFO buffering + ByteRing)
 *
 * Frame Structure:
 *  ┌─────┬─────┬─────┬──────┬─────────────┬───────┐
 *  │ AA  │ 55  │ Ver │ Type │ Payload ... │ CRC16 │
 *  └─────┴─────┴─────┴──────┴─────────────┴───────┘
 *   Sync0 Sync1   1    Msg     0-128 bytes   2 bytes
 *
 * Message Types (Msg enum):
 *  - Ping/Pong: Keepalive and latency measurement
 *  - Input: Rim → Base (buttons, encoders, analog inputs)
 *  - Telemetry: Base → Rim (RPM, speed, gear, flags for display/LEDs)
 *  - ShiftLed/BtnLed/Display: Base → Rim (LED patterns, TFT widgets)
 *  - CfgSync/CfgGet/CfgSave: Configuration synchronization
 *  - FwBegin/FwData/FwEnd: Over-the-air firmware update (OTA)
 *  - VersionGet/VersionReport: Firmware version query
 *  - AccelGet/AccelReport: ADXL345 accelerometer readings (homing)
 *
 * Key Features:
 *  - CRC-16/CCITT-FALSE frame integrity (poly 0x1021, init 0xFFFF)
 *  - Packed structs (no padding) for wire compatibility
 *  - Variable-length payloads (0-128 bytes, kMaxPayload)
 *  - Firmware update protocol (up to 192KB images, CRC-32 validation)
 *  - Display layout synchronization (3 pages, 16 widgets per page)
 *  - Encoder configuration (4 rotary encoders with acceleration/debounce)
 *  - Shift light sequencing (WS2812 LED strip control)
 *
 * Thread Safety:
 *  - ByteRing: Single-producer/single-consumer lock-free ring buffer
 *  - Parser runs in Core0 input loop (both MCUs)
 *  - Transmitter runs from main control loop
 *
 * See also:
 *  - docs/link-protocol.md: Full protocol specification
 *  - firmware-base/src/uart_link.cpp: Base-side implementation
 *  - firmware-rim/src/uart_link.cpp: Rim-side implementation
 */

#include <stdint.h>

namespace FfbLink {

static constexpr uint8_t kSync0 = 0xAA;
static constexpr uint8_t kSync1 = 0x55;
static constexpr uint8_t kVersion = 1;
static constexpr uint8_t kMaxPayload = 128;
// Inter-MCU UART (base Serial1 ↔ rim Serial1). CDC to the host stays 115200.
static constexpr uint32_t kBaud = 460800;
static constexpr uint16_t kUartFifoSize = 1024;
static constexpr uint16_t kRxRingSize = 1024;
static constexpr uint32_t kLinkTimeoutMs = 2500;
static constexpr uint32_t kOtaMaxImageBytes = 192u * 1024u;
// Rim Core0 input / UART cadence (µs). 100 Hz — 500 Hz flooded the link under motor EMI / USB load.
static constexpr uint32_t kRimIoPeriodUs = 10000;
// Core0: drop stale live telemetry (LEDs / future TFT → hardware standby).
static constexpr uint32_t kTelemetryTimeoutUs = 500000;

// --- Message types ---
enum Msg : uint8_t {
    Ping = 0x01,
    Pong = 0x02,
    Input = 0x10,
    Telemetry = 0x20,
    ShiftLed = 0x21,
    BtnLed = 0x22,
    Display = 0x23,
    AccelGet = 0x24,      // base→rim | AccelGetPayload (optional; empty = single sample)
    AccelReport = 0x25,   // rim→base | AccelReportPayload
    CfgSync = 0x30,       // base→rim | RimConfig (apply live, do not persist)
    CfgAck = 0x31,        // rim→base | — (after CfgSave)
    CfgGet = 0x32,        // base→rim | — (request CfgReport)
    CfgReport = 0x33,     // rim→base | RimConfig (authoritative)
    CfgSave = 0x34,       // base→rim | — (persist rim EEPROM)
    VersionGet = 0x51,    // base→rim | —
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
    FwFailTimeout = 6, // idle in updater with no host traffic
};

inline uint32_t crc32(const uint8_t* data, uint32_t len) {
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; ++i) {
        c ^= data[i];
        for (int b = 0; b < 8; ++b) {
            const uint32_t mask = (uint32_t) - (int32_t)(c & 1u);
            c = (c >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~c;
}

// HID button allocation on the base gamepad (1-based Joystick.button IDs).
static constexpr uint8_t kHidPanelBtnFirst = 1; // 1..10
static constexpr uint8_t kHidPanelBtnCount = 10;
static constexpr uint8_t kHidEncCwFirst = 11;     // 11,13,15,17
static constexpr uint8_t kHidEncCcwFirst = 12;    // 12,14,16,18
static constexpr uint8_t kHidEncSwitchFirst = 19; // 19..22
static constexpr uint8_t kHidShifterABtn = 23;    // sequential / shifter paddle A (from ADS)
static constexpr uint8_t kHidShifterBBtn = 24;    // sequential / shifter paddle B (from ADS)
static constexpr uint8_t kHidReservedFirst = 25;  // 25..32
static constexpr uint8_t kEncoderCount = 4;

enum EncMode : uint8_t {
    EncModeRelative = 0, // momentary CW/CCW pulses (default)
    EncModeHold = 1,     // hold CW or CCW while turning; release after idle
    EncModeAbsolute = 2, // accumulate 0..100 value (telemetry / :get encN_value)
};

struct __attribute__((packed)) EncoderConfig {
    uint8_t invert;         // 0/1
    uint8_t stepsPerClick;  // detents per HID pulse (1..)
    uint8_t accelEnable;    // 0/1 (relative mode)
    uint8_t accelThreshold; // detents/s before accel
    uint8_t accelMaxMult;   // 1..8
    uint8_t debounceMs;
    uint8_t pulseMs; // HID press duration (relative) / hold idle ms (hold)
    uint8_t mode;    // EncMode
};

// Rim ILI9341 dashboard widgets (landscape 320×240).
// Page layouts live in DisplayStore (rim EEPROM) and sync over Display 0x23 — NOT in RimConfig.
static constexpr uint8_t kDispElementMax = 16;
static constexpr uint8_t kDispChunkElements = 8; // elements per SetPageChunk frame
static constexpr uint16_t kDispWidth = 320;
static constexpr uint16_t kDispHeight = 240;

enum DispElementType : uint8_t {
    DispNone = 0,
    // Text values
    DispGear = 1,
    DispSpeed = 2,
    DispRpm = 3,
    DispFuel = 4,
    DispLapTime = 5, // TelemetryPayload.lapTimeMs
    DispFlags = 6,   // compact flag text from TelFlag bits
    // Bars / gauges (fontSize 1..4 = widget scale; color565 = accent)
    DispRpmBarH = 7,     // horizontal RPM progress (0..9000)
    DispFuelBarH = 8,    // horizontal fuel % bar
    DispRpmBarV = 9,     // vertical RPM bar
    DispFuelBarV = 10,   // vertical fuel % bar
    DispRpmGauge = 11,   // arc gauge RPM
    DispSpeedGauge = 12, // arc gauge speed (0..300 km/h)
    DispFuelGauge = 13,  // arc gauge fuel %
    // Chrome
    DispFlagBanner = 14, // colored strip from active TelFlag bits
    DispGearBadge = 15,  // gear in a framed badge
    DispPanel = 16,      // decorative filled panel (backdrop)
    // Tyre / brake heat maps (auto color from telemetry; fontSize = box scale)
    DispTyreTempQuad = 17,  // 2×2 FL FR / RL RR °C
    DispTyrePressQuad = 18, // 2×2 pressure PSI
    DispBrakeTempQuad = 19, // 2×2 brake °C
    DispTyreTempFL = 20,
    DispTyreTempFR = 21,
    DispTyreTempRL = 22,
    DispTyreTempRR = 23,
    DispTyrePressFL = 24,
    DispTyrePressFR = 25,
    DispTyrePressRL = 26,
    DispTyrePressRR = 27,
    DispBrakeTempFL = 28,
    DispBrakeTempFR = 29,
    DispBrakeTempRL = 30,
    DispBrakeTempRR = 31,
    // Built-in icons (fontSize = scale 1..4, color565 = tint)
    DispIconRpm = 32,
    DispIconFuel = 33,
    DispIconSpeed = 34,
    DispIconFlag = 35,
    DispIconTyre = 36,
    DispIconBrake = 37,
    DispIconLap = 38,
    // On-screen page nav (touch later; CDC/hit-test now)
    DispBtnPrev = 40,
    DispBtnNext = 41,
    DispBtnPage0 = 42,
    DispBtnPage1 = 43,
    DispBtnPage2 = 44,
    // Timing / gaps (signed ms from TelemetryPayload; INT16_MIN = n/a)
    DispDeltaBest = 45,       // sector/lap delta vs personal best
    DispDeltaP1 = 46,         // sector/lap delta vs P1 / overall best
    DispGapAhead = 47,        // race gap to car ahead (ms)
    DispGapBehind = 48,       // race gap to car behind (ms)
    DispGapStack = 49,        // ahead + behind stacked
    DispSectorSplitBest = 50, // visual ± marker vs PB
    DispSectorSplitP1 = 51,   // visual ± marker vs P1
    // Lap history + car-layout composites (TWF / Slick Modern style)
    DispLastLap = 52,
    DispBestLap = 53,
    DispTyreCar = 54,  // 2×2 vertical tyre cards (°C + PSI)
    DispBrakeCar = 55, // 2×2 vertical brake bars
};

static constexpr uint8_t kDispPageMax = 3;

enum DispBgTheme : uint8_t {
    DispBgBlack = 0,
    DispBgCarbon = 1,
    DispBgNavy = 2,
    DispBgGrid = 3,
};

struct __attribute__((packed)) DisplayElement {
    uint8_t type;      // DispElementType
    uint16_t x;        // pixels from left (origin / gauge center-ish)
    uint16_t y;        // pixels from top
    uint8_t fontSize;  // 1..4 → text size, or bar/gauge/panel/icon scale
    uint16_t color565; // RGB565 accent / fill
};

// layoutCount + layout[kDispElementMax] — CDC layout_hex / layout_pageN_hex (zero-padded).
static constexpr uint16_t kLayoutBlobSize =
    (uint16_t)(sizeof(uint8_t) + kDispElementMax * sizeof(DisplayElement));

struct __attribute__((packed)) DisplayPage {
    uint8_t bgTheme; // DispBgTheme
    uint8_t layoutCount;
    DisplayElement layout[kDispElementMax];
};
static constexpr uint16_t kDisplayPageBlobSize = (uint16_t)sizeof(DisplayPage);

// base↔rim page bank sync (message Display 0x23)
enum DispOp : uint8_t {
    DispOpSetPage = 1,      // legacy full page (≤8); prefer SetPageChunk
    DispOpSetMeta = 2,      // DispMetaPayload
    DispOpGetAll = 3,       // rim replies with meta + chunked pages
    DispOpSetPageChunk = 4, // DispPageChunkPayload (variable count ≤ kDispChunkElements)
};

struct __attribute__((packed)) DispMetaPayload {
    uint8_t pageCount;  // 1..kDispPageMax
    uint8_t activePage; // 0..pageCount-1
};

// Legacy single-frame page (kept for older tooling; truncated to first 8 widgets).
struct __attribute__((packed)) DispPageSetPayload {
    uint8_t pageIndex;
    uint8_t bgTheme;
    uint8_t layoutCount;
    DisplayElement layout[8];
};

// Chunked page write — sender loops until start+count >= layoutCount.
struct __attribute__((packed)) DispPageChunkPayload {
    uint8_t pageIndex;
    uint8_t bgTheme;     // authoritative on start==0; ignored otherwise
    uint8_t layoutCount; // total widgets on page (0..kDispElementMax)
    uint8_t start;       // first element index in this chunk
    uint8_t count;       // elements present in `elements` (1..kDispChunkElements)
    DisplayElement elements[kDispChunkElements];
};

struct __attribute__((packed)) RimConfig {
    EncoderConfig enc[kEncoderCount];
    uint8_t panelLedBright; // 0..255
    uint8_t shiftLedBright; // 0..255
    uint8_t shiftLedCount;  // WS2812 count
    uint8_t dispBright;     // 0..255
    uint16_t shiftRpm[5];   // 0..3 fill stages, [4] = red-blink overrev
    // Layout removed — pages live in DisplayStore / 0x23 only (keeps CfgSync small).
};
static_assert(sizeof(RimConfig) <= kMaxPayload, "RimConfig must fit one UART frame");
static_assert(kLayoutBlobSize == 1 + kDispElementMax * sizeof(DisplayElement), "layout blob size");
static_assert(sizeof(DispPageSetPayload) <= kMaxPayload,
              "DispPageSetPayload must fit one UART frame");
static_assert(1 + sizeof(DispPageChunkPayload) <= kMaxPayload,
              "DispPageChunkPayload must fit one UART frame");
static_assert(sizeof(DisplayElement) == 8, "DisplayElement packed size");

enum InputFlag : uint8_t {
    InputAlive = 1u << 0,         // rim app loop running
    InputAdxlPresent = 1u << 1,   // ADXL345 probed OK at 0x53
    InputAdxlMotion = 1u << 2,    // recent motion (for sleep/wake telemetry)
    InputMcpBtnPresent = 1u << 3, // MCP23017 @ 0x20 (panel switches)
    InputMcpLedPresent = 1u << 4, // MCP23017 @ 0x21 (panel LEDs)
    InputAdsPresent = 1u << 5,    // ADS1115 @ 0x48 (paddle halls)
};

// ADS1115 single-ended channels on the rim (InputPayload.analog[]).
static constexpr uint8_t kAnalogClutchL = 0;
static constexpr uint8_t kAnalogClutchR = 1;
static constexpr uint8_t kAnalogShifterA = 2;
static constexpr uint8_t kAnalogShifterB = 3;
static constexpr uint8_t kAnalogCount = 4;
// Minimum Input payload size (pre-ADS). Newer rims append analog[4].
static constexpr uint8_t kInputPayloadCoreSize =
    (uint8_t)(sizeof(uint32_t) + kEncoderCount * sizeof(int8_t) + sizeof(uint8_t) +
              sizeof(uint8_t));

enum AccelGetMode : uint8_t {
    AccelOnce = 0,    // single XYZ sample
    AccelAverage = 1, // average N samples (N in AccelGetPayload::count, default 100)
};

struct __attribute__((packed)) AccelGetPayload {
    uint8_t mode;  // AccelGetMode
    uint8_t count; // samples for AccelAverage (0 → 100)
};

struct __attribute__((packed)) AccelReportPayload {
    uint8_t present; // 0/1 chip ACK at 0x53
    uint8_t ok;      // last read / average succeeded
    int16_t ax;      // raw X (ADXL345 DATAX, little-endian full-res)
    int16_t ay;
    int16_t az;
    uint8_t samples; // how many samples went into ax/ay/az
    uint8_t reserved;
};

struct __attribute__((packed)) InputPayload {
    uint32_t buttons;               // bit0 = panel btn 1 ... bit9 = btn 10
    int8_t encDelta[kEncoderCount]; // signed steps this frame (pre-policy raw)
    uint8_t encSwitch;              // bits 0..3 (unused on MCP panel — always 0)
    uint8_t flags;                  // InputFlag bits
    int16_t analog[kAnalogCount];   // ADS1115 raw (0 if chip absent / not yet sampled)
};

struct __attribute__((packed)) TelemetryPayload {
    uint16_t rpm;
    int16_t speedKphx10;
    int8_t gear;   // -1 = R, 0 = N, 1..
    uint8_t flags; // TelFlag bits
    uint16_t fuelPctx10;
    uint16_t lapTimeMs;
    // Optional extension (older hosts may omit — rim accepts core-sized frames).
    // Corner order: FL, FR, RL, RR.
    uint8_t tyreTempC[4];    // °C 0..255
    uint8_t tyrePressPsi[4]; // PSI 0..255
    uint16_t brakeTempC[4];  // °C (brakes can exceed 255)
    // Timing gaps (optional; omit → 0). Use INT16_MIN (-32768) for n/a.
    // Negative delta = ahead of reference (faster); positive = behind.
    int16_t deltaBestMs; // sector/lap split vs personal best
    int16_t deltaP1Ms;   // sector/lap split vs P1 / session best
    int16_t gapAheadMs;  // race interval to car ahead (usually ≥0)
    int16_t gapBehindMs; // race interval to car behind (usually ≥0)
    // Lap history (optional). 0 = unknown / not provided.
    uint16_t lastLapMs;
    uint16_t bestLapMs;
};
// Pre-tyre/brake telemetry size (rpm..lapTimeMs).
static constexpr uint8_t kTelemetryPayloadCoreSize = 10;
static constexpr uint8_t kTelemetryPayloadTyreSize = 26;
static constexpr int16_t kTelemetryGapNa = (int16_t)-32768;
static_assert(sizeof(TelemetryPayload) == 38, "TelemetryPayload size");
static_assert(sizeof(TelemetryPayload) <= kMaxPayload, "TelemetryPayload must fit one UART frame");

// 11× WS2812 layout: [0..1]=flags  [2..8]=RPM  [9..10]=TC/ABS
static constexpr uint8_t kLedFlagCount = 2;
static constexpr uint8_t kLedRpmCount = 7;
static constexpr uint8_t kLedAidCount = 2;
static constexpr uint8_t kLedFlagFirst = 0;
static constexpr uint8_t kLedRpmFirst = 2;
static constexpr uint8_t kLedAidFirst = 9;

enum TelFlag : uint8_t {
    TelYellow = 1u << 0, // flag zone (LEDs 0–1), shared blink
    TelBlue = 1u << 1,   // flag zone (LEDs 0–1), shared blink
    TelTc = 1u << 2,     // aid zone (LEDs 9–10), shared blink
    TelAbs = 1u << 3,    // aid zone (LEDs 9–10), shared blink
    TelRed = 1u << 4,    // flag zone: both LEDs urgent red blink
    TelPit = 1u << 5,    // RPM zone: all 7 LEDs yellow blink (pit limiter)
};

// Shift / strip LED control (base → rim), also used by GUI tests.
enum ShiftLedMode : uint8_t {
    LedModeAuto = 0,    // follow telemetry RPM + flags
    LedModeOff = 1,     // clear strip, stay off until Auto
    LedModeSolid = 2,   // solid RGB
    LedModeFill = 3,    // light first `param` LEDs with RGB
    LedModeChase = 4,   // moving chase
    LedModeRainbow = 5, // rainbow cycle
    LedModeRpm = 6,     // simulate RPM bar + optional flags
    LedModeBoot = 7,    // replay startup sequence
    LedModeZones = 8,   // show zone colors (flags / rpm / aids)
};

struct __attribute__((packed)) ShiftLedPayload {
    uint8_t mode; // ShiftLedMode
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t param; // fill count, etc.
    uint16_t rpm;  // for LedModeRpm
    uint8_t flags; // TelFlag bits for LedModeRpm / tests
};

// Panel button LEDs (base → rim). bit0 = LED for panel btn 1 …
struct __attribute__((packed)) BtnLedPayload {
    uint16_t mask;
};

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) — frame integrity (kept over CRC-8
// so OTA / config frames stay strong at 460.8 kbaud).
inline uint16_t crc16(const uint8_t* data, uint16_t len) {
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

// Single-producer / single-consumer byte ring for UART RX burst absorption.
// Capacity is N-1 usable bytes (one slot left empty to distinguish full/empty).
template <uint16_t N> struct ByteRing {
    static_assert(N >= 4 && (N & (N - 1)) == 0, "ByteRing size must be power of 2");

    uint8_t buf[N];
    volatile uint16_t head = 0; // write index
    volatile uint16_t tail = 0; // read index

    static constexpr uint16_t mask() { return (uint16_t)(N - 1); }

    void clear() {
        head = 0;
        tail = 0;
    }

    uint16_t count() const { return (uint16_t)((head - tail) & mask()); }

    uint16_t freeSpace() const { return (uint16_t)(N - 1 - count()); }

    bool empty() const { return head == tail; }

    bool push(uint8_t b) {
        const uint16_t h = head;
        const uint16_t next = (uint16_t)((h + 1) & mask());
        if (next == tail)
            return false;
        buf[h] = b;
        head = next;
        return true;
    }

    bool pop(uint8_t& b) {
        const uint16_t t = tail;
        if (t == head)
            return false;
        b = buf[t];
        tail = (uint16_t)((t + 1) & mask());
        return true;
    }
};

inline void clearDisplayPage(DisplayPage& p) {
    p = DisplayPage{};
    p.bgTheme = DispBgBlack;
    p.layoutCount = 0;
}

inline void defaultDisplayPage0(DisplayPage& p) {
    // Drive (Slick/TWF style): gear cluster + bars + always-on live delta.
    clearDisplayPage(p);
    p.bgTheme = DispBgCarbon;
    p.layoutCount = 8;
    p.layout[0] = {DispFlagBanner, 0, 0, 2, 0xFFE0};
    p.layout[1] = {DispSpeed, 12, 28, 2, 0x07E0};
    p.layout[2] = {DispGearBadge, 118, 28, 4, 0xFFFF};
    p.layout[3] = {DispRpm, 228, 28, 2, 0xFFE0};
    p.layout[4] = {DispDeltaBest, 12, 72, 2, 0x07E0}; // live PB delta
    p.layout[5] = {DispRpmBarH, 36, 112, 3, 0xF800};
    p.layout[6] = {DispFuelBarH, 36, 148, 2, 0x07FF};
    p.layout[7] = {DispBtnNext, 252, 200, 2, 0xFFFF};
}

inline void defaultDisplayPage1(DisplayPage& p) {
    // Tyres MFD: gear/delta header, tyre cards, brake row, nav — no overlaps on 320×240.
    clearDisplayPage(p);
    p.bgTheme = DispBgNavy;
    p.layoutCount = 7;
    p.layout[0] = {DispFlagBanner, 0, 0, 1, 0xFFE0};
    p.layout[1] = {DispGearBadge, 8, 14, 2, 0xFFFF}; // left — clear of tyre gap
    p.layout[2] = {DispDeltaBest, 200, 18, 1, 0x07E0};
    p.layout[3] = {DispTyreCar, 86, 44, 1, 0xFFFF};   // size 1: ~102×102 block
    p.layout[4] = {DispBrakeCar, 52, 156, 1, 0xFD20}; // below tyres; labels inside bars
    p.layout[5] = {DispBtnPrev, 8, 216, 1, 0xFFFF};
    p.layout[6] = {DispBtnNext, 280, 216, 1, 0xFFFF};
}

inline void defaultDisplayPage2(DisplayPage& p) {
    // Timing MFD: current / last / best, PB+P1 deltas, sector split, race gaps.
    clearDisplayPage(p);
    p.bgTheme = DispBgGrid;
    p.layoutCount = 8;
    p.layout[0] = {DispLapTime, 12, 16, 3, 0xFFFF};
    p.layout[1] = {DispLastLap, 12, 52, 2, 0xC618};
    p.layout[2] = {DispBestLap, 160, 52, 2, 0x07E0};
    p.layout[3] = {DispDeltaBest, 12, 84, 2, 0x07E0};
    p.layout[4] = {DispDeltaP1, 160, 84, 2, 0xFFE0};
    p.layout[5] = {DispSectorSplitBest, 28, 118, 3, 0xFFFF};
    p.layout[6] = {DispGapStack, 28, 152, 2, 0x07FF};
    p.layout[7] = {DispBtnPrev, 40, 200, 2, 0xFFFF};
}

inline void defaultRimConfig(RimConfig& c) {
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
    c.shiftRpm[4] = 7800; // last two red LEDs blink at/above this
}

} // namespace FfbLink
