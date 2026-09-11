#include "inputs.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "ads1115.h"
#include "adxl345.h"
#include "config.h"

namespace Inputs {
namespace {

FfbLink::RimConfig cfg{};
uint32_t panelBits = 0;
int8_t encDelta[FfbLink::kEncoderCount] = {};

const int encA[4] = {PIN_ENC0_A, PIN_ENC1_A, PIN_ENC2_A, PIN_ENC3_A};
const int encB[4] = {PIN_ENC0_B, PIN_ENC1_B, PIN_ENC2_B, PIN_ENC3_B};
uint8_t prevAb[4] = {};
int8_t accum[4] = {};

bool mcpBtnOk = false;
bool mcpLedOk = false;
uint16_t lastLedMask = 0xFFFF;  // force first write

bool ldrOk_ = false;
volatile uint8_t ambientScale_ = 255;  // Core1 may read for shift LEDs
uint32_t lastLdrMs_ = 0;
uint8_t lastPwmDuty_ = 0xFF;

// MCP23017 register map (BANK=0)
constexpr uint8_t kRegIodirA = 0x00;
constexpr uint8_t kRegGppuA = 0x0C;
constexpr uint8_t kRegGpioA = 0x12;
constexpr uint8_t kRegOlatA = 0x14;
constexpr uint8_t kRegIocon = 0x0A;

// Gray-code quadrature transitions → +1 / -1
int8_t quadStep(uint8_t prev, uint8_t now) {
    static const int8_t table[4][4] = {
        {0, 1, -1, 0},
        {-1, 0, 0, 1},
        {1, 0, 0, -1},
        {0, -1, 1, 0},
    };
    return table[prev & 3][now & 3];
}

uint8_t readAb(uint8_t i) {
    const uint8_t a = digitalRead(encA[i]) ? 1 : 0;
    const uint8_t b = digitalRead(encB[i]) ? 1 : 0;
    return (uint8_t)((a << 1) | b);
}

bool mcpProbe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

bool mcpWrite8(uint8_t addr, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool mcpWrite16(uint8_t addr, uint8_t regA, uint16_t value) {
    Wire.beginTransmission(addr);
    Wire.write(regA);
    Wire.write((uint8_t)(value & 0xFF));
    Wire.write((uint8_t)(value >> 8));
    return Wire.endTransmission() == 0;
}

bool mcpRead16(uint8_t addr, uint8_t regA, uint16_t &value) {
    Wire.beginTransmission(addr);
    Wire.write(regA);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr, 2) != 2) return false;
    const uint8_t lo = (uint8_t)Wire.read();
    const uint8_t hi = (uint8_t)Wire.read();
    value = (uint16_t)(lo | ((uint16_t)hi << 8));
    return true;
}

bool mcpForceBank0(uint8_t addr) {
    (void)mcpWrite8(addr, kRegIocon, 0x00);
    (void)mcpWrite8(addr, 0x05, 0x00);
    return true;
}

bool initMcpButtons() {
    if (!mcpProbe(MCP_ADDR_BTN)) return false;
    mcpForceBank0(MCP_ADDR_BTN);
    if (!mcpWrite16(MCP_ADDR_BTN, kRegIodirA, 0xFFFF)) return false;
    if (!mcpWrite16(MCP_ADDR_BTN, kRegGppuA, 0xFFFF)) return false;
    uint16_t iodir = 0;
    if (!mcpRead16(MCP_ADDR_BTN, kRegIodirA, iodir) || iodir != 0xFFFF) return false;
    return true;
}

bool initMcpLeds() {
    if (!mcpProbe(MCP_ADDR_LED)) return false;
    mcpForceBank0(MCP_ADDR_LED);
    if (!mcpWrite16(MCP_ADDR_LED, kRegIodirA, 0x0000)) return false;
    if (!mcpWrite16(MCP_ADDR_LED, kRegOlatA, 0xFFFF)) return false;
    uint16_t iodir = 0;
    if (!mcpRead16(MCP_ADDR_LED, kRegIodirA, iodir) || iodir != 0x0000) return false;
    return true;
}

uint8_t scaleFromLdr(uint16_t raw) {
    // Bright ambient → closer to 255; dark → LDR_SCALE_MIN.
    if (raw <= LDR_DARK) return LDR_SCALE_MIN;
    if (raw >= LDR_BRIGHT) return 255;
    const uint32_t span = (uint32_t)(LDR_BRIGHT - LDR_DARK);
    const uint32_t gain = (uint32_t)(255 - LDR_SCALE_MIN);
    return (uint8_t)(LDR_SCALE_MIN + ((uint32_t)(raw - LDR_DARK) * gain) / span);
}

void applyPanelBright() {
    const uint16_t duty =
        (uint16_t)(((uint16_t)cfg.panelLedBright * (uint16_t)ambientScale_) / 255u);
    const uint8_t out = (uint8_t)duty;
    if (out == lastPwmDuty_) return;
    lastPwmDuty_ = out;
    analogWrite(PIN_PANEL_LED_PWM, out);
}

void updateLdr() {
    const uint32_t now = millis();
    if (now - lastLdrMs_ < LDR_POLL_MS) return;
    lastLdrMs_ = now;

    const uint16_t raw = (uint16_t)analogRead(PIN_LDR);
    // Pulldown + open pin ≈ 0. Populated divider sits above LDR_PRESENT_MIN.
    if (raw < LDR_PRESENT_MIN) {
        ldrOk_ = false;
        ambientScale_ = 255;  // no LDR → full brightness
    } else {
        ldrOk_ = true;
        ambientScale_ = scaleFromLdr(raw);
    }
    applyPanelBright();
}

void sampleButtons() {
    if (!mcpBtnOk) {
        panelBits = 0;
        return;
    }
    uint16_t gpio = 0;
    if (!mcpRead16(MCP_ADDR_BTN, kRegGpioA, gpio)) {
        return;
    }
    panelBits = (uint16_t)((~gpio) & 0x03FFu);
}

}  // namespace

void begin() {
    FfbLink::defaultRimConfig(cfg);
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(encA[i], INPUT_PULLUP);
        pinMode(encB[i], INPUT_PULLUP);
        prevAb[i] = readAb(i);
    }

    analogReadResolution(12);
    // Pulldown so an unpopulated LDR pin reads ~0 → full brightness path.
    pinMode(PIN_LDR, INPUT_PULLDOWN);
    pinMode(PIN_PANEL_LED_PWM, OUTPUT);
    ldrOk_ = false;
    ambientScale_ = 255;
    lastPwmDuty_ = 0xFF;
    lastLdrMs_ = 0;
    applyPanelBright();

    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();

    // Independent probes — any missing chip must not block the others.
    mcpBtnOk = initMcpButtons();
    mcpLedOk = initMcpLeds();
    lastLedMask = 0xFFFF;

    Ads1115::begin();
    Adxl345::begin();
}

void setConfig(const FfbLink::RimConfig &c) {
    cfg = c;
    lastPwmDuty_ = 0xFF;  // force PWM refresh
    applyPanelBright();
}

const FfbLink::RimConfig &config() { return cfg; }

void setPanelLeds(uint16_t maskOn) {
    if (!mcpLedOk) return;
    maskOn = (uint16_t)(maskOn & 0x03FFu);
    if (maskOn == lastLedMask) return;
    const uint16_t olat = (uint16_t)((~maskOn) | 0xFC00u);
    if (!mcpWrite16(MCP_ADDR_LED, kRegOlatA, olat)) {
        return;
    }
    lastLedMask = maskOn;
}

void update() {
    memset(encDelta, 0, sizeof(encDelta));
    for (uint8_t i = 0; i < FfbLink::kEncoderCount; ++i) {
        const uint8_t now = readAb(i);
        const int8_t step = quadStep(prevAb[i], now);
        prevAb[i] = now;
        if (step == 0) continue;
        accum[i] = (int8_t)(accum[i] + step);
        while (accum[i] >= 4 || accum[i] <= -4) {
            if (accum[i] >= 4) {
                accum[i] = (int8_t)(accum[i] - 4);
                if (encDelta[i] < 127) encDelta[i]++;
            } else {
                accum[i] = (int8_t)(accum[i] + 4);
                if (encDelta[i] > -128) encDelta[i]--;
            }
        }
    }
    sampleButtons();
    Ads1115::update();
    updateLdr();
}

void fillInput(FfbLink::InputPayload &out) {
    out = FfbLink::InputPayload{};
    out.buttons = panelBits;
    out.encSwitch = 0;
    out.flags = FfbLink::InputAlive;
    if (mcpBtnOk) out.flags |= FfbLink::InputMcpBtnPresent;
    if (mcpLedOk) out.flags |= FfbLink::InputMcpLedPresent;
    if (Ads1115::present()) out.flags |= FfbLink::InputAdsPresent;
    if (Adxl345::present()) out.flags |= FfbLink::InputAdxlPresent;
    if (Adxl345::motionRecent()) out.flags |= FfbLink::InputAdxlMotion;
    for (uint8_t i = 0; i < FfbLink::kEncoderCount; ++i) {
        out.encDelta[i] = encDelta[i];
    }
    int16_t analog[FfbLink::kAnalogCount] = {};
    Ads1115::fillAnalog(analog);
    memcpy(out.analog, analog, sizeof(analog));
}

uint8_t ambientScale() { return ambientScale_; }
bool ldrPresent() { return ldrOk_; }

}  // namespace Inputs
