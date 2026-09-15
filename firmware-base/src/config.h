#pragma once

#include <stdint.h>

// --- Steering hall (MLX90363 on G920 board) ---
// red=5V  black=GND  blue=MISO  yellow=/SS  orange=SCLK  green=MOSI
static const int PIN_HALL_MISO = 16;
static const int PIN_HALL_SS = 17;
static const int PIN_HALL_SCLK = 18;
static const int PIN_HALL_MOSI = 19;

static const uint32_t HALL_SPI_HZ = 500000;

// Axle index (digital hall + magnet). Open-collector: use INPUT_PULLUP.
// VCC→3V3  GND→GND  OUT→GP20
static const int PIN_AXLE_INDEX = 20;
// true = OUT low when magnet present (common A3144-style modules)
static const bool AXLE_INDEX_ACTIVE_LOW = true;
// Axle degrees at the *center* of the index active window (not necessarily 0).
// Trim from GUI at *manual* visual center (not post-INIT settle): last read −19° → +19.
static const float AXLE_INDEX_ANGLE_DEG = 156.0f;
// Runtime snap on every magnet pass. Keep false — it fights steering near the
// index and feels like a huge deadzone. INIT / serial 'i' still sync.
static const bool AXLE_INDEX_SYNC_ON_EDGE = false;

// Manual / motorized INIT
#ifndef HOME_ZERO_TOLERANCE_DEG
#define HOME_ZERO_TOLERANCE_DEG 2.5f
#endif
static const uint32_t HOME_ZERO_HOLD_MS = 300;
// Seek + soft to0 use this duty (capped by MOTOR_DUTY_CAP too).
#ifndef HOME_MOTOR_DUTY
#define HOME_MOTOR_DUTY 0.12f
#endif
// Magnet window measure is slower so enter/exit edges are not skipped.
#ifndef HOME_MEASURE_DUTY
#define HOME_MEASURE_DUTY 0.08f
#endif
// Bidirectional window passes after prime exit. Keep ≥2 — one-way mid shifts
// with approach direction (hall hysteresis); average of + and − cancels it.
#ifndef HOME_INDEX_WINDOW_PASSES
#define HOME_INDEX_WINDOW_PASSES 2
#endif
// After leaving the magnet, travel this far before reversing for the next pass.
#ifndef HOME_INDEX_CLEARANCE_DEG
#define HOME_INDEX_CLEARANCE_DEG 15.0f
#endif
// Reject absurd window widths (deg of axle travel while idx active).
#ifndef HOME_INDEX_WIDTH_MIN_DEG
#define HOME_INDEX_WIDTH_MIN_DEG 1.0f
#endif
#ifndef HOME_INDEX_WIDTH_MAX_DEG
#define HOME_INDEX_WIDTH_MAX_DEG 90.0f
#endif
// First search direction (+1 or -1). Measure reverse is automatic.
static const float HOME_MOTOR_DIR = 1.0f;
// Power-up INIT always runs. If true, enable bridges and seek with motors.
// Override at build time: -DHOME_BOOT_USE_MOTORS=1 (see pico-mc33926-home env).
#ifndef HOME_BOOT_USE_MOTORS
#define HOME_BOOT_USE_MOTORS 0
#endif
// Prefer rim ADXL345 gravity zero when present; else magnet index window.
// Override: -DHOME_USE_ADXL=0 for magnet-only INIT (bench / no rim ADXL).
#ifndef HOME_USE_ADXL
#define HOME_USE_ADXL 1
#endif
static const uint32_t HOME_ADXL_PROBE_MS = 500;    // wait for AccelReport before magnet fallback
static const uint32_t HOME_ADXL_POLL_MS = 40;      // AccelGet cadence while seeking
static const int16_t HOME_ADXL_TOLERANCE_RAW = 25; // |ax - offset| at center (~100 mg)
static const uint32_t HOME_ADXL_HOLD_MS = 300;
// +1 means positive calibrated X → drive positive duty to reduce error (flip if mount inverted)
static const float HOME_ADXL_DIR = 1.0f;
// Motorized index search: travel this far before reversing (wheel may boot anywhere).
#ifndef HOME_SEEK_TRAVEL_DEG
#define HOME_SEEK_TRAVEL_DEG 380.0f
#endif
// Per motorized-seek time budget (ms). Raised so a slow 360° pass can finish.
#ifndef HOME_MOTOR_TIMEOUT_MS
#define HOME_MOTOR_TIMEOUT_MS 90000u
#endif
// Abort motorized phase if axle angle doesn't change (hall stuck / wrong gear).
// Disabled during MeasureIndex (slow magnet window on purpose).
#ifndef HOME_MOTION_STALL_MS
#define HOME_MOTION_STALL_MS 5000u
#endif
#ifndef HOME_MOTION_STALL_DEG
#define HOME_MOTION_STALL_DEG 1.0f
#endif
// If motors are on but axle barely moves for this long → hall/SPI/gear is wrong.
// 0 = disabled (preferred — false aborts at low INIT duty).
#ifndef HOME_ENCODER_PROGRESS_MS
#define HOME_ENCODER_PROGRESS_MS 0
#endif
#ifndef HOME_ENCODER_PROGRESS_DEG
#define HOME_ENCODER_PROGRESS_DEG 3.0f
#endif
// Max time without a fresh MLX sample during motorized INIT.
#ifndef HOME_HALL_LOSS_MS
#define HOME_HALL_LOSS_MS 800u
#endif
// Max axle travel while driving to virtual 0 after index sync.
#ifndef HOME_SEEK_ZERO_TRAVEL_DEG
#define HOME_SEEK_ZERO_TRAVEL_DEG 400.0f
#endif

// --- HW-159 7× WS2812 ring (status) ---
// VCC→5V (VBUS)  GND→GND  DIN→GP21 (+ optional 330–470Ω series)
static const int PIN_STATUS_NEOPIXEL = 21;
static const int STATUS_NEOPIXEL_COUNT = 7;
static const uint8_t STATUS_NEOPIXEL_BRIGHTNESS = 40; // keep low on USB 5V

// Sensor turns per axle turn (G920 commonly cited 18:1)
static const float DEFAULT_GEAR_RATIO = 18.0f;

// --- Logitech G29/G920/G923 pedals (DE-9 male on pedal cable) ---
// DE-9: 1=GND  2=Throttle  3=Brake  4=Clutch  6&9=VCC
// Power pedals from Pico 3V3 so wipers stay within ADC range.
static const int PIN_PEDAL_THROTTLE = 26; // ADC0
static const int PIN_PEDAL_BRAKE = 27;    // ADC1
static const int PIN_PEDAL_CLUTCH = 28;   // ADC2

static const int ADC_BITS = 12;
static const int ADC_MAX = (1 << ADC_BITS) - 1;
static const int PEDAL_FILTER_SAMPLES = 8;
// Logitech pots rest high (~3V3). Below this → treat as unplugged / not ready.
static const int PEDAL_CONNECTED_REST_MIN = 2800;
// First press must drop this far from rest before min/max learning starts.
static const int PEDAL_PRESS_START_DELTA = 400;
// Need at least this span before normalize leaves 0.
static const int PEDAL_MIN_SPAN = 200;

// --- Motor driver (BTS7960 default, or Pololu Dual MC33926 when MOTOR_DRIVER_MC33926) ---
// Logic GND common with Pico. Motor power is a separate 12–24 V supply — never into Pico pins.
#if defined(MOTOR_DRIVER_MC33926)
// Pololu Dual MC33926 shield (wire breakout to Pico; not an Arduino stack).
// Matches default shield signals; remap here if needed.
static const int PIN_M1_DIR = 10;
static const int PIN_M1_PWM = 11;
static const int PIN_M2_DIR = 13;
static const int PIN_M2_PWM = 14;
static const int PIN_MC33926_ND2 = 12; // D2: HIGH = armed; LOW = coast / disable
// Pololu silk "SF" with overbar = nSF (active LOW). LOW = fault when D2 is HIGH.
// When D2 is LOW, nSF is forced low too — firmware ignores that (not a real fault).
// Leave SF wired to GP15; set to -1 only if the pin is unused.
static const int PIN_MC33926_NSF = 15;
// If fault detection feels backwards on your wiring, set this to false.
static const bool MC33926_SF_ACTIVE_LOW = true;
// Optional current sense (ADC). -1 = unwired (pedals use GP26–28 by default).
static const int PIN_M1_FB = -1;
static const int PIN_M2_FB = -1;
static const float MC33926_FB_VOLTS_PER_AMP = 0.525f;
#else
// BTS7960 / IBT-2 — sign-magnitude RPWM/LPWM + EN per channel.
static const int PIN_M1_RPWM = 10;
static const int PIN_M1_LPWM = 11;
static const int PIN_M1_EN = 12; // tie R_EN + L_EN together
static const int PIN_M2_RPWM = 13;
static const int PIN_M2_LPWM = 14;
static const int PIN_M2_EN = 15;
#endif

// PWM full scale for analogWrite on this core
static const int PWM_MAX = 255;

// Hard cap while bringing up hardware (0..1). Raise later.
// Override: -DMOTOR_DUTY_CAP=0.15f
#ifndef MOTOR_DUTY_CAP
#define MOTOR_DUTY_CAP 0.35f
#endif
// +1 = positive torque/duty increases axle angle; -1 if motor wiring vs hall is flipped.
// Wrong sign → spring runs away from center instead of recentering.
#ifndef MOTOR_OUTPUT_SIGN
#define MOTOR_OUTPUT_SIGN 1.0f
#endif

// Simple software spring for bench test (N·m-ish units arbitrary)
static const float FFB_SPRING_K = 0.004f; // torque per axle degree
static const float FFB_SPRING_DEADZONE_DEG = 2.0f;
#ifndef FFB_TORQUE_CAP
#define FFB_TORQUE_CAP 0.35f // matches duty cap for production default
#endif

// Control loop
static const uint32_t LOOP_PERIOD_MS = 10;

// --- Rim-mcu link (UART0) ---
// Base TX→rim RX, base RX←rim TX, common GND. 3V3 logic.
static const int PIN_RIM_UART_TX = 0; // GP0 / UART0 TX
static const int PIN_RIM_UART_RX = 1; // GP1 / UART0 RX
// Open-drain friendly: drive LOW to assert, INPUT (Hi-Z) to release.
static const int PIN_RIM_RESET = 2;   // → rim RUN
static const int PIN_RIM_BOOTSEL = 3; // → rim BOOTSEL (USB recovery only)

// --- WeAct Studio 3.7" e-paper (GDEY037T03 / UC8253C, 240×416) ---
// Own PIO SoftwareSPI bus — does NOT share HW SPI0 with the MLX90363.
// VCC→Pico 3V3  GND→GND  (I/O is 3.3 V; do not use motor rail).
// Compiled only when ENABLE_BASE_EPD=1 (default 0 — stubs in epd_status.cpp).
static const int PIN_EPD_SCK = 4;  // SCL (SoftwareSPI / PIO)
static const int PIN_EPD_MOSI = 5; // SDA / DIN
static const int PIN_EPD_CS = 6;
static const int PIN_EPD_DC = 7;
static const int PIN_EPD_RST = 8;
static const int PIN_EPD_BUSY = 9;
// PIO SPI still needs a MISO GPIO; leave GP22 unconnected (not wired to the panel).
static const int PIN_EPD_MISO_UNUSED = 22;
static const uint32_t EPD_SPI_HZ = 4000000;
// Base e-ink status panel. 0 = keep module in tree but no SPI/core1/draw (default).
// Re-enable: -DENABLE_BASE_EPD=1 (and ensure GxEPD2 is in lib_deps).
#ifndef ENABLE_BASE_EPD
#define ENABLE_BASE_EPD 0
#endif

// USB Joystick HID (0 = Serial-only debug; no gamepad)
#ifndef ENABLE_USB_HID
#define ENABLE_USB_HID 1
#endif

// USB HID PID force-feedback (host effects). Always on with ENABLE_USB_HID —
// bring-up is staged (CDC through INIT, then attach PID HID) so host probing
// does not fight motorized home / rim UART.
#ifndef ENABLE_USB_HID_PID
#define ENABLE_USB_HID_PID 1
#endif

// Run TinyUSB + HID on core1 (experimental). Default OFF.
#ifndef ENABLE_USB_HID_CORE1
#define ENABLE_USB_HID_CORE1 0
#endif

// After INIT goes idle, wait this long before USB re-enum with PID HID.
#ifndef HID_ATTACH_SETTLE_MS
#define HID_ATTACH_SETTLE_MS 1500u
#endif

// USB MSC setup volume (WIN.BAT / MAC.SH / LINUX.SH → GitHub Releases).
// Registered with HID attach (post-INIT). Default OFF — enable with
// -DENABLE_SETUP_MSC=1 when you want the install volume.
#ifndef ENABLE_SETUP_MSC
#define ENABLE_SETUP_MSC 0
#endif
static const uint32_t SETUP_MSC_GRACE_MS = 2000;

// HID: axle degrees that map to full stick travel (±half → joy 0..1023).
// 360 = sensitive for jstest. Use 900 for G920-style sims later.
static const float WHEEL_HID_RANGE_DEG = 900.0f;
