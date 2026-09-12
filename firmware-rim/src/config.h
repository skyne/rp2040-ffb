#pragma once

#include <stdint.h>

// Rim-mcu pin map (Pi Pico). See docs/rim-hardware-plan.md.

// UART to base-mcu (crossed with base GP0/GP1)
static const int PIN_LINK_UART_TX = 0;
static const int PIN_LINK_UART_RX = 1;

// I2C: MCP23017 panel + ADS1115 halls + optional ADXL345 (each soft-fails independently)
static const int PIN_I2C_SDA = 4;
static const int PIN_I2C_SCL = 5;
static const uint8_t MCP_ADDR_BTN = 0x20; // 10× switches (GPA0–7, GPB0–1)
static const uint8_t MCP_ADDR_LED = 0x21; // 10× LEDs (GPA0–7, GPB0–1), active-low
static const uint8_t ADS_ADDR = 0x48;     // 4× linear halls A0–A3
static const uint8_t ADXL_ADDR = 0x53;
static const uint32_t ADXL_POLL_MS = 50;
static const uint32_t ADXL_IDLE_MS = 180000;         // 3 min stillness → power-save
static const uint32_t ADXL_MOTION_HOLD_MS = 500;     // InputAdxlMotion sticky window
static const int16_t ADXL_MOTION_THRESHOLD_RAW = 40; // ~156 mg @ 256 LSB/g

// Ambient light → global panel LED rail (FET PWM)
static const int PIN_LDR = 26;
static const int PIN_PANEL_LED_PWM = 27;
// Divider assumed: 3V3—LDR—ADC—R—GND. Pulldown ⇒ open pin ≈ 0 → treat as absent.
static const uint16_t LDR_PRESENT_MIN = 40; // 12-bit counts; below → no LDR → full bright
static const uint16_t LDR_DARK = 120;       // ambient scale floor region
static const uint16_t LDR_BRIGHT = 3200;    // ambient scale ceiling region
static const uint8_t LDR_SCALE_MIN = 64;    // never auto-dim below ~25% when LDR present
static const uint32_t LDR_POLL_MS = 50;

// EC12 encoders: A/B only (no shaft switch on this panel)
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

// GP15 / GP28 reserved for future XPT2046 touch (T_IRQ / T_CS) — not wired yet.

// ILI9341 (SPI1) — optional; soft-fail if absent
static const int PIN_TFT_SCLK = 18;
static const int PIN_TFT_MOSI = 19;
static const int PIN_TFT_MISO = 16;
static const int PIN_TFT_CS = 17;
static const int PIN_TFT_DC = 20;
static const int PIN_TFT_RST = 21;
static const int PIN_TFT_BL = 22;

static const uint32_t LOOP_PERIOD_MS = 2; // Core0 target; see FfbLink::kRimIoPeriodUs
