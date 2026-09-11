#pragma once

#include <stdint.h>

// --- Steering hall (MLX90363 on G920 board) ---
// red=5V  black=GND  blue=MISO  yellow=/SS  orange=SCLK  green=MOSI
static const int PIN_HALL_MISO = 16;
static const int PIN_HALL_SS   = 17;
static const int PIN_HALL_SCLK = 18;
static const int PIN_HALL_MOSI = 19;

static const uint32_t HALL_SPI_HZ = 500000;

// Axle index (digital hall + magnet). Open-collector: use INPUT_PULLUP.
// VCC→3V3  GND→GND  OUT→GP20
static const int PIN_AXLE_INDEX = 20;
// true = OUT low when magnet present (common A3144-style modules)
static const bool AXLE_INDEX_ACTIVE_LOW = true;
// Axle degrees at the *center* of the index active window (not necessarily 0).
// Measure: z at straight ahead, slow-sweep magnet, take window mid axle=.
static const float AXLE_INDEX_ANGLE_DEG = 5.0f;
// Runtime snap on every magnet pass. Keep false — it fights steering near the
// index and feels like a huge deadzone. INIT / serial 'i' still sync.
static const bool AXLE_INDEX_SYNC_ON_EDGE = false;

// Manual / motorized INIT
static const float HOME_ZERO_TOLERANCE_DEG = 3.0f;
static const uint32_t HOME_ZERO_HOLD_MS = 300;
// Slow seek duty while motors are enabled (capped by MOTOR_DUTY_CAP too)
static const float HOME_MOTOR_DUTY = 0.12f;
// Even slower while measuring the index active window (enter/exit → mid)
static const float HOME_MEASURE_DUTY = 0.08f;
// Bidirectional window passes (1 = one direction only, 2 = + then −)
static const uint8_t HOME_INDEX_WINDOW_PASSES = 2;
// First search direction (+1 or -1). Measure reverse is automatic.
static const float HOME_MOTOR_DIR = 1.0f;
static const uint32_t HOME_MOTOR_TIMEOUT_MS = 15000;
// Power-up INIT always runs. If true, enable bridges and seek with motors.
static const bool HOME_BOOT_USE_MOTORS = false;  // motorized INIT — WIP
// Prefer rim ADXL345 gravity zero when present; else magnet index window.
static const bool HOME_USE_ADXL = true;
static const uint32_t HOME_ADXL_PROBE_MS = 500;     // wait for AccelReport before magnet fallback
static const uint32_t HOME_ADXL_POLL_MS = 40;       // AccelGet cadence while seeking
static const int16_t HOME_ADXL_TOLERANCE_RAW = 25;  // |ax - offset| at center (~100 mg)
static const uint32_t HOME_ADXL_HOLD_MS = 300;
// +1 means positive calibrated X → drive positive duty to reduce error (flip if mount inverted)
static const float HOME_ADXL_DIR = 1.0f;

// --- HW-159 7× WS2812 ring (status) ---
// VCC→5V (VBUS)  GND→GND  DIN→GP21 (+ optional 330–470Ω series)
static const int PIN_STATUS_NEOPIXEL = 21;
static const int STATUS_NEOPIXEL_COUNT = 7;
static const uint8_t STATUS_NEOPIXEL_BRIGHTNESS = 40;  // keep low on USB 5V

// Sensor turns per axle turn (G920 commonly cited 18:1)
static const float DEFAULT_GEAR_RATIO = 18.0f;

// --- Logitech G29/G920/G923 pedals (DE-9 male on pedal cable) ---
// DE-9: 1=GND  2=Throttle  3=Brake  4=Clutch  6&9=VCC
// Power pedals from Pico 3V3 so wipers stay within ADC range.
static const int PIN_PEDAL_THROTTLE = 26; // ADC0
static const int PIN_PEDAL_BRAKE    = 27; // ADC1
static const int PIN_PEDAL_CLUTCH   = 28; // ADC2

static const int ADC_BITS = 12;
static const int ADC_MAX  = (1 << ADC_BITS) - 1;
static const int PEDAL_FILTER_SAMPLES = 8;
// Logitech pots rest high (~3V3). Below this → treat as unplugged / not ready.
static const int PEDAL_CONNECTED_REST_MIN = 2800;
// First press must drop this far from rest before min/max learning starts.
static const int PEDAL_PRESS_START_DELTA = 400;
// Need at least this span before normalize leaves 0.
static const int PEDAL_MIN_SPAN = 200;

// --- Dual BTS7960 (IBT-2 style) H-bridge ---
// One channel per FFB motor. Logic GND common with Pico.
// Motor power (B+/B-) is a separate 12–24 V supply — never feed that into Pico pins.
//
// Channel A (motor 1):
static const int PIN_M1_RPWM = 10;
static const int PIN_M1_LPWM = 11;
static const int PIN_M1_EN   = 12;  // tie R_EN + L_EN together
// Channel B (motor 2):
static const int PIN_M2_RPWM = 13;
static const int PIN_M2_LPWM = 14;
static const int PIN_M2_EN   = 15;

// PWM full scale for analogWrite on this core
static const int PWM_MAX = 255;

// Hard cap while bringing up hardware (0..1). Raise later.
static const float MOTOR_DUTY_CAP = 0.35f;

// Simple software spring for bench test (N·m-ish units arbitrary)
static const float FFB_SPRING_K = 0.004f;      // torque per axle degree
static const float FFB_SPRING_DEADZONE_DEG = 2.0f;
static const float FFB_TORQUE_CAP = 0.35f;     // matches duty cap for now

// Control loop
static const uint32_t LOOP_PERIOD_MS = 10;

// --- Rim-mcu link (UART0) ---
// Base TX→rim RX, base RX←rim TX, common GND. 3V3 logic.
static const int PIN_RIM_UART_TX = 0;  // GP0 / UART0 TX
static const int PIN_RIM_UART_RX = 1;  // GP1 / UART0 RX
// Open-drain friendly: drive LOW to assert, INPUT (Hi-Z) to release.
static const int PIN_RIM_RESET = 2;    // → rim RUN
static const int PIN_RIM_BOOTSEL = 3;  // → rim BOOTSEL (USB recovery only)

// --- WeAct Studio 3.7" e-paper (GDEY037T03 / UC8253C, 240×416) ---
// Own PIO SoftwareSPI bus — does NOT share HW SPI0 with the MLX90363.
// VCC→Pico 3V3  GND→GND  (I/O is 3.3 V; do not use motor rail).
static const int PIN_EPD_SCK  = 4;   // SCL (SoftwareSPI / PIO)
static const int PIN_EPD_MOSI = 5;   // SDA / DIN
static const int PIN_EPD_CS   = 6;
static const int PIN_EPD_DC   = 7;
static const int PIN_EPD_RST  = 8;
static const int PIN_EPD_BUSY = 9;
// PIO SPI still needs a MISO GPIO; leave GP22 unconnected (not wired to the panel).
static const int PIN_EPD_MISO_UNUSED = 22;
static const uint32_t EPD_SPI_HZ = 4000000;
#ifndef ENABLE_BASE_EPD
#define ENABLE_BASE_EPD 1
#endif

// USB Joystick HID (0 = Serial-only debug; no gamepad)
#ifndef ENABLE_USB_HID
#define ENABLE_USB_HID 1
#endif

// HID: axle degrees that map to full stick travel (±half → joy 0..1023).
// 360 = sensitive for jstest. Use 900 for G920-style sims later.
static const float WHEEL_HID_RANGE_DEG = 900.0f;
