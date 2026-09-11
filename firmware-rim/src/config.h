#pragma once

#include <stdint.h>

// Rim-mcu pin map (Pi Pico). PCB layout — WIP.

// UART to base-mcu (crossed with base GP0/GP1)
static const int PIN_LINK_UART_TX = 0;
static const int PIN_LINK_UART_RX = 1;

// I2C for PCA8574A expanders (buttons + button LEDs)
static const int PIN_I2C_SDA = 4;
static const int PIN_I2C_SCL = 5;
// Expected addresses (A variant): 0x38.. — three chips for 10 btn + 10 LED
static const uint8_t PCA_ADDR_BTN0 = 0x38;  // buttons 0..7
static const uint8_t PCA_ADDR_BTN1 = 0x39;  // buttons 8..9 (+ spare)
static const uint8_t PCA_ADDR_LED0 = 0x3A;  // LEDs 0..7 (active-low sinks typical)

// EC12 encoders: A/B (+ optional switch on panel expander or GPIO)
static const int PIN_ENC0_A = 6;
static const int PIN_ENC0_B = 7;
static const int PIN_ENC1_A = 8;
static const int PIN_ENC1_B = 9;
static const int PIN_ENC2_A = 10;
static const int PIN_ENC2_B = 11;
static const int PIN_ENC3_A = 12;
static const int PIN_ENC3_B = 13;

// WS2812B shift / ambient strip — DIN → GP14 (+ optional 330Ω series)
static const int PIN_WS2812 = 14;
static const int WS2812_DEFAULT_COUNT = 11;

// ILI9341 (SPI1) — optional; WIP
static const int PIN_TFT_SCLK = 18;
static const int PIN_TFT_MOSI = 19;
static const int PIN_TFT_MISO = 16;
static const int PIN_TFT_CS = 17;
static const int PIN_TFT_DC = 20;
static const int PIN_TFT_RST = 21;
static const int PIN_TFT_BL = 22;

static const uint32_t LOOP_PERIOD_MS = 2;  // Core0 target; see FfbLink::kRimIoPeriodUs

