#include "inputs.h"

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

#include "config.h"

namespace Inputs {
namespace {

FfbLink::RimConfig cfg{};
uint32_t panelBits = 0;
uint8_t encSwitch = 0;
int8_t encDelta[FfbLink::kEncoderCount] = {};

const int encA[4] = {PIN_ENC0_A, PIN_ENC1_A, PIN_ENC2_A, PIN_ENC3_A};
const int encB[4] = {PIN_ENC0_B, PIN_ENC1_B, PIN_ENC2_B, PIN_ENC3_B};
uint8_t prevAb[4] = {};
int8_t accum[4] = {};

// Gray-code quadrature transitions → +1 / -1
int8_t quadStep(uint8_t prev, uint8_t now) {
    // prev/now are 2-bit AB
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

bool i2cOk = false;

uint8_t readPca(uint8_t addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission(false) != 0) return 0xFF;
    if (Wire.requestFrom((int)addr, 1) != 1) return 0xFF;
    return (uint8_t)Wire.read();
}

void writePca(uint8_t addr, uint8_t value) {
    Wire.beginTransmission(addr);
    Wire.write(value);
    Wire.endTransmission();
}

void sampleButtons() {
    if (!i2cOk) {
        panelBits = 0;
        return;
    }
    // Active-low buttons assumed
    const uint8_t b0 = ~readPca(PCA_ADDR_BTN0);
    const uint8_t b1 = ~readPca(PCA_ADDR_BTN1);
    panelBits = ((uint32_t)b0) | (((uint32_t)(b1 & 0x03)) << 8);
    // PCA 0x39 bits 2..5 → encoder shaft switches 0..3 (active-low, inverted above)
    encSwitch = (uint8_t)((b1 >> 2) & 0x0F);
}

}  // namespace

void begin() {
    FfbLink::defaultRimConfig(cfg);
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(encA[i], INPUT_PULLUP);
        pinMode(encB[i], INPUT_PULLUP);
        prevAb[i] = readAb(i);
    }

    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();
    Wire.beginTransmission(PCA_ADDR_BTN0);
    i2cOk = (Wire.endTransmission() == 0);
    if (i2cOk) {
        // LED expander: all off (1 = off if open-drain active-low)
        writePca(PCA_ADDR_LED0, 0xFF);
    }
}

void setConfig(const FfbLink::RimConfig &c) { cfg = c; }
const FfbLink::RimConfig &config() { return cfg; }

void setPanelLeds(uint16_t maskOn) {
    if (!i2cOk) return;
    // Active-low sinks: 0 = LED on
    const uint8_t out = (uint8_t)(~(maskOn & 0xFFu));
    writePca(PCA_ADDR_LED0, out);
}

void update() {
    memset(encDelta, 0, sizeof(encDelta));
    for (uint8_t i = 0; i < FfbLink::kEncoderCount; ++i) {
        const uint8_t now = readAb(i);
        const int8_t step = quadStep(prevAb[i], now);
        prevAb[i] = now;
        if (step == 0) continue;
        accum[i] = (int8_t)(accum[i] + step);
        // Emit whole detents (4 transitions per detent typical for EC12)
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
}

void fillInput(FfbLink::InputPayload &out) {
    out = FfbLink::InputPayload{};
    out.buttons = panelBits;
    out.encSwitch = encSwitch;
    out.flags = 1;
    for (uint8_t i = 0; i < FfbLink::kEncoderCount; ++i) {
        out.encDelta[i] = encDelta[i];
    }
}

}  // namespace Inputs
