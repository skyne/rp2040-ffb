# Wiring Diagrams

**Visual connection references for rp2040-ffb build**

This document provides ASCII diagrams and pinout tables for each subsystem. Use these as a reference during assembly.

---

## Table of Contents

1. [Base MCU Complete](#base-mcu-complete)
2. [Rim MCU Complete](#rim-mcu-complete)
3. [Motor Driver Details](#motor-driver-details)
4. [I²C Bus Layout](#i2c-bus-layout)
5. [Power Distribution](#power-distribution)
6. [Cable Pinouts](#cable-pinouts)

---

## Base MCU Complete

### Raspberry Pi Pico (Base) - Full Pinout

```
                    ┌─────────────┐
                    │   Micro-USB │
                    └─────────────┘
                         ╭───╮
                         │RPi│
                    ┌────┤   ├────┐
                    │    │Pico│    │
                    │    ╰───╯    │
    ┌───────────────┴──────┬──────┴───────────────┐
    │                      │                      │
   GP0/TX ────────────────►  Rim RX (UART)
   GP1/RX ◄────────────────  Rim TX (UART)
   GP2    ────────────────►  Rim RUN (optional reset)
   GP3    ────────────────►  Rim BOOTSEL (optional)
   GP4/SDA ────────────────  E-paper (optional)
   GP5/SCL ────────────────  E-paper (optional)
   GP6    ────────────────►  FFB knob A (optional)
   GP7    ────────────────►  FFB knob B (optional)
   GP8    ────────────────►  Free / user input
   GP9    ────────────────►  Free / user input
   GP10   ────────────────►  Motor 1 RPWM (BTS7960)
   GP11   ────────────────►  Motor 1 LPWM (BTS7960)
   GP12   ────────────────►  Motor 1 EN (BTS7960)
   GP13   ────────────────►  Motor 2 RPWM (BTS7960)
   GP14   ────────────────►  Motor 2 LPWM (BTS7960)
   GP15   ────────────────►  Motor 2 EN (BTS7960)
   GP16/MISO ◄────────────  Angle sensor (MLX90363 on G29/G920/G923, encoder A on G25/G27/DFGT)
   GP17/CS   ────────────►  Angle sensor (MLX90363 CS on G29/G920/G923, encoder B on G25/G27/DFGT)
   GP18/SCLK ────────────►  MLX90363 SCLK (G920/G923 only)
   GP19/MOSI ────────────►  MLX90363 MOSI (G920/G923 only)
   GP20   ◄────────────────  Index hall sensor (A3144)
   GP21   ────────────────►  WS2812 ring (status LEDs)
   GP22   ────────────────►  ILI9341 TFT SCLK (optional)
   GP26/ADC0 ◄────────────  Throttle pedal
   GP27/ADC1 ◄────────────  Brake pedal
   GP28/ADC2 ◄────────────  Clutch pedal
   3V3OUT ────────────────►  Sensors, pedals, logic (ADC safe!)
   GND    ────────────────►  Common ground
   VBUS   ────────────────►  5V for MLX90363 if used (G920/G923 only)
    │                                              │
    └──────────────────────────────────────────────┘
```

### Base Connections Table

| Pico Pin | Function | Connected To | Signal Type |
|----------|----------|--------------|-------------|
| GP0 | UART TX | Rim GP0 (RX) | 3.3V digital |
| GP1 | UART RX | Rim GP1 (TX) | 3.3V digital |
| GP2 | Rim reset | Rim RUN | 3.3V digital (optional) |
| GP3 | Rim boot | Rim BOOTSEL | 3.3V digital (optional) |
| GP10 | PWM | Motor 1 BTS7960 RPWM | 3.3V PWM |
| GP11 | PWM | Motor 1 BTS7960 LPWM | 3.3V PWM |
| GP12 | Enable | Motor 1 BTS7960 R_EN + L_EN | 3.3V digital |
| GP13 | PWM | Motor 2 BTS7960 RPWM | 3.3V PWM |
| GP14 | PWM | Motor 2 BTS7960 LPWM | 3.3V PWM |
| GP15 | Enable | Motor 2 BTS7960 R_EN + L_EN | 3.3V digital |
| GP16 | SPI MISO / Encoder A | MLX90363 MISO (G29/G920/G923) or Encoder A (G25/G27/DFGT) | 3.3V |
| GP17 | SPI CS / Encoder B | MLX90363 SS (G29/G920/G923) or Encoder B (G25/G27/DFGT) | 3.3V |
| GP18 | SPI SCLK | MLX90363 SCLK (G920/G923 only) | 3.3V SPI |
| GP19 | SPI MOSI | MLX90363 MOSI (G920/G923 only) | 3.3V SPI |
| GP20 | Digital input | A3144 hall OUT | Active-low, pullup |
| GP21 | WS2812 data | LED ring DIN | 3.3V data (needs level shift to 5V) |
| GP26 | ADC | Throttle pot center tap | Analog 0-3.3V |
| GP27 | ADC | Brake pot center tap | Analog 0-3.3V |
| GP28 | ADC | Clutch pot center tap | Analog 0-3.3V |
| 3V3OUT | Power | All sensor VCC | 3.3V @ 300mA max |
| VBUS | Power | MLX90363 VCC (G920/G923 only), WS2812 VCC | 5V @ 500mA max |
| GND | Ground | Common ground plane | 0V |

---

## Rim MCU Complete

### Raspberry Pi Pico (Rim) - Full Pinout

```
                    ┌─────────────┐
                    │   Micro-USB │
                    └─────────────┘
                         ╭───╮
                         │RPi│
                    ┌────┤   ├────┐
                    │    │Pico│    │
                    │    ╰───╯    │
    ┌───────────────┴──────┬──────┴───────────────┐
    │                      │                      │
   GP0/RX ◄────────────────  Base TX (UART)
   GP1/TX ────────────────►  Base RX (UART)
   GP2    ────────────────►  MCP INT (optional)
   GP3    ────────────────►  ADS INT (optional)
   GP4/SDA ◄──────────────►  I²C bus (MCP + ADS)
   GP5/SCL ◄──────────────►  I²C bus (MCP + ADS)
   GP6    ◄────────────────  Encoder 0 A
   GP7    ◄────────────────  Encoder 0 B
   GP8    ◄────────────────  Encoder 1 A
   GP9    ◄────────────────  Encoder 1 B
   GP10   ◄────────────────  Encoder 2 A
   GP11   ◄────────────────  Encoder 2 B
   GP12   ◄────────────────  Encoder 3 A
   GP13   ◄────────────────  Encoder 3 B
   GP14   ────────────────►  WS2812B LED strip DIN
   GP15   ────────────────►  TFT DC (optional)
   GP16   ◄──────────────►  SPI1 MISO (TFT, optional)
   GP17   ────────────────►  TFT CS (optional)
   GP18   ────────────────►  SPI1 SCLK (TFT, optional)
   GP19   ────────────────►  SPI1 MOSI (TFT, optional)
   GP20   ────────────────►  TFT RST (optional)
   GP26/ADC0 ◄────────────  Free / user analog
   GP27/ADC1 ◄────────────  Free / user analog
   GP28/ADC2 ◄────────────  Free / user analog
   3V3OUT ────────────────►  I²C pullups, sensors
   GND    ────────────────►  Common ground
   VBUS   ────────────────►  5V for WS2812B
    │                                              │
    └──────────────────────────────────────────────┘
```

### Rim Connections Table

| Pico Pin | Function | Connected To | Signal Type |
|----------|----------|--------------|-------------|
| GP0 | UART RX | Base GP0 (TX) | 3.3V digital |
| GP1 | UART TX | Base GP1 (RX) | 3.3V digital |
| GP4 | I²C SDA | MCP 0x20, 0x21, ADS 0x48 | 3.3V I²C, 4.7kΩ pullup |
| GP5 | I²C SCL | MCP 0x20, 0x21, ADS 0x48 | 3.3V I²C, 4.7kΩ pullup |
| GP6 | Encoder A | Rotary encoder 0 pin A | Digital input, pullup |
| GP7 | Encoder B | Rotary encoder 0 pin B | Digital input, pullup |
| GP8 | Encoder A | Rotary encoder 1 pin A | Digital input, pullup |
| GP9 | Encoder B | Rotary encoder 1 pin B | Digital input, pullup |
| GP10 | Encoder A | Rotary encoder 2 pin A | Digital input, pullup |
| GP11 | Encoder B | Rotary encoder 2 pin B | Digital input, pullup |
| GP12 | Encoder A | Rotary encoder 3 pin A | Digital input, pullup |
| GP13 | Encoder B | Rotary encoder 3 pin B | Digital input, pullup |
| GP14 | WS2812 data | LED strip DIN | 3.3V data + 330Ω resistor |
| 3V3OUT | Power | I²C devices, encoder pullups | 3.3V @ 300mA max |
| VBUS | Power | WS2812 strip VCC | 5V @ 500mA max |
| GND | Ground | All device GND | 0V |

---

## Motor Driver Details

### Single BTS7960 Module Connections

```
┌─────────────────────────────────────────────┐
│            BTS7960 / IBT-2 Module           │
│                                             │
│  Logic Side (3.3V):          Motor Side:   │
│  ┌──────────────┐            ┌──────────┐  │
│  │ VCC  ─────────────────────  B+       │  │ ───► PSU + (24V stock)
│  │ GND  ─────────────────────  B-       │  │ ───► PSU -
│  │ RPWM ◄─── Pico GPx         M+       │  │ ───► Motor red wire
│  │ LPWM ◄─── Pico GPy         M-       │  │ ───► Motor black wire
│  │ R_EN ┐                      ╔══════╗   │
│  │ L_EN ┴◄─── Pico GPz         ║ HEAT ║   │  ← Heatsink required!
│  └──────────────┘              ╚══════╝   │
└─────────────────────────────────────────────┘
```

### Dual Motor Setup

```
        Pico Base                PSU (24V stock, 5A)
    ┌──────────────┐                 │
    │              │                 │
    │ GP10 ───────►│─── RPWM ─┐     │
    │ GP11 ───────►│─── LPWM  │     │
    │ GP12 ───────►│─── EN ────┤   ┌─▼──────────┐
    │              │            │   │ B+    M+   ├──► Motor 1 Red
    │              │      BTS7960 #1│     H      │
    │              │            │   │ B-    M-   ├──► Motor 1 Black
    │              │            │   └─┬──────────┘
    │ 3V3  ───────►│─── VCC ────┤     │
    │ GND  ───────►│─── GND ────┤     │
    │              │            │     │
    │ GP13 ───────►│─── RPWM ─┐ │     │
    │ GP14 ───────►│─── LPWM  │ │     │
    │ GP15 ───────►│─── EN ────┤ │   ┌─▼──────────┐
    │              │            │ │   │ B+    M+   ├──► Motor 2 Red
    └──────────────┘      BTS7960 #2│     H      │
                           │ │   │ B-    M-   ├──► Motor 2 Black
                     VCC ──┘ │   └─┬──────────┘
                     GND ────┘     │
                                   GND (common)
```

**Critical notes:**
- Logic VCC/GND connects to Pico
- Motor B+/B- connects ONLY to PSU
- Never connect motor voltage to Pico pins
- R_EN and L_EN can be tied together
- Heatsinks mandatory for >50W operation

---

## I²C Bus Layout

### Rim I²C Bus Overview

```
        Pico Rim
    ┌──────────────┐
    │              │
    │ GP4/SDA ─────┼────┬────┬────┐
    │              │    │    │    │
    │ GP5/SCL ─────┼────┼────┼────┼──┐
    │              │    │    │    │  │
    │ 3V3OUT ──────┼──┬─┼──┬─┼──┬─┼──┼─── Power for all
    │              │  │ │  │ │  │ │  │
    │ GND ─────────┼──┼─┼──┼─┼──┼─┼──┼─── Common GND
    └──────────────┘  │ │  │ │  │ │  │
                      │ │  │ │  │ │  │
    SDA pullup 4.7kΩ ─┘ │  │ │  │ │  │
    SCL pullup 4.7kΩ ───┘  │ │  │ │  │
                           │ │  │ │  │
    ┌──────────────────────▼─▼──▼─▼──▼────────┐
    │                                          │
    │  ┌──────────┐  ┌──────────┐  ┌───────┐ │
    │  │ MCP23017 │  │ MCP23017 │  │ADS1115│ │
    │  │ @ 0x20   │  │ @ 0x21   │  │@ 0x48 │ │
    │  │ (buttons)│  │  (LEDs)  │  │(halls)│ │
    │  └──────────┘  └──────────┘  └───────┘ │
    └──────────────────────────────────────────┘
```

### I²C Address Configuration

**MCP23017 Address Selection (A0/A1/A2 pads):**

```
Device   | A0 | A1 | A2 | Address
─────────┼────┼────┼────┼─────────
Buttons  | 0  | 0  | 0  | 0x20
LEDs     | 1  | 0  | 0  | 0x21
```

**0 = open/NC, 1 = solder bridge to GND**

**ADS1115 Address Selection (ADDR pin):**

```
ADDR Pin    | Address
────────────┼─────────
GND         | 0x48  ← Default
VDD         | 0x49
SDA         | 0x4A
SCL         | 0x4B
```

---

## Power Distribution

### System Power Architecture

```
                   ┌──────────────────────────────────┐
                   │       USB Power (from PC)        │
                   │         5V @ 500mA max           │
                   └────────┬─────────────────┬───────┘
                            │                 │
                            ▼                 ▼
                   ┌────────────────┐  ┌────────────────┐
                   │   Base Pico    │  │   Rim Pico     │
                   │   ┌────────┐   │  │   ┌────────┐   │
                   │   │ VBUS   │   │  │   │ VBUS   │   │
                   │   │  5V    ├───┼──┼───┤  5V    │   │
                   │   └───┬────┘   │  │   └───┬────┘   │
                   │       │        │  │       │        │
                   │   ┌───▼────┐   │  │   ┌───▼────┐   │
                   │   │ 3V3REG │   │  │   │ 3V3REG │   │
                   │   │ 300mA  │   │  │   │ 300mA  │   │
                   │   └───┬────┘   │  │   └───┬────┘   │
                   │       │        │  │       │        │
                   │   ┌───▼────┐   │  │   ┌───▼────┐   │
                   │   │ 3V3OUT │   │  │   │ 3V3OUT │   │
                   └───┴────┬───┴───┘  └───┴────┬───┴───┘
                            │                   │
              ┌─────────────┼───────┐ ┌─────────┼─────────────┐
              ▼             ▼       ▼ ▼         ▼             ▼
         MLX90363      BTS7960   Pedals  MCP23017 x2    ADS1115
         (5V VBUS)    (logic 3V3)(3V3)      (3V3)         (3V3)
              │             │       │
              └─────────────┴───────┴────► All GND common


                   ┌──────────────────────────────────┐
                   │      Motor PSU (external)        │
                   │     24V @ 5-10A (stock or aftermarket) │
                   │     (36V experimental with cooling)    │
                   └────────┬─────────────────────────┘
                            │
                            ▼
                   ┌────────────────┐
                   │  BTS7960 #1    │  Motor power ONLY
                   │  ┌────────┐    │  (never to Pico!)
                   │  │ B+  M+ ├────┼───► Motor 1
                   │  │ B-  M- ├────┼───► Motor 1
                   │  └────────┘    │
                   └────────┬───────┘
                            │
                   ┌────────▼───────┐
                   │  BTS7960 #2    │
                   │  ┌────────┐    │
                   │  │ B+  M+ ├────┼───► Motor 2
                   │  │ B-  M- ├────┼───► Motor 2
                   │  └────────┘    │
                   └────────┬───────┘
                            │
                            ▼
                         GND (common with USB GND)
```

### Power Budget

**USB 5V Rail (500mA total):**
- Base Pico: ~50mA
- Rim Pico: ~50mA
- MLX90363: ~20mA
- WS2812 LEDs (11): ~200mA @ full white
- Headroom: 180mA

**3.3V Rail (300mA per Pico):**
- MCP23017 x2: ~2mA each
- ADS1115: ~1mA
- Pedals: ~30mA total
- I²C pullups: ~2mA
- Sensors: ~50mA
- Headroom: 200mA

**Motor 24V Rail (stock voltage):**
- Stock motors rated: 24V
- Dual motors peak: 10A @ 24V (240W)
- Continuous: 3-5A typical
- Recommend: Use stock G920/G923 PSU or 5A+ aftermarket
- Experimental: 36V possible with cooling (under testing)

---

## Cable Pinouts

### Logitech Pedal DE-9 Connector

**Looking at male DE-9 plug (solder side):**

```
     5  4  3  2  1
      9  8  7  6

Pin | Signal    | Wire Color (typical) | Connect To
────┼───────────┼──────────────────────┼──────────────
 1  | GND       | Black                | Pico GND
 2  | Throttle  | Red                  | Pico GP26
 3  | Brake     | Green                | Pico GP27
 4  | Clutch    | Blue                 | Pico GP28
 5  | GND       | Black                | Pico GND (common)
 6  | VCC (+)   | Red/White            | Pico 3V3OUT
 7  | (unused)  | —                    | NC
 8  | (unused)  | —                    | NC
 9  | VCC (+)   | Red/White            | Pico 3V3OUT (common)
```

**⚠️ CRITICAL: VCC must be 3.3V, NOT 5V!**

### Base ↔ Rim UART Cable

**4-wire minimum (6-wire recommended):**

```
Wire  | Base Pin | Rim Pin  | Signal       | Notes
──────┼──────────┼──────────┼──────────────┼────────────────────
  1   | GP0      | GP0      | Base TX      | Cross RX/TX!
  2   | GP1      | GP1      | Rim TX       |
  3   | GND      | GND      | Ground       | Common reference
  4   | —        | —        | Shield/drain | If shielded cable
  5   | GP2      | RUN      | Rim reset    | Optional
  6   | GP3      | BOOTSEL  | Rim boot     | Optional
```

**Cable specs:**
- Twisted pair recommended
- Max length: 1 meter (for 460800 baud)
- Add ferrite beads if near motor wires

### WS2812B LED Strip

```
Strip Pad | Signal | Connect To       | Notes
──────────┼────────┼──────────────────┼─────────────────────
  VCC/+5V | Power  | Pico VBUS (5V)   | Check polarity!
  DIN/D   | Data   | Pico GPx + 330Ω  | Series resistor
  GND/-   | Ground | Pico GND         | Common GND
```

**Resistor placement:**
```
Pico GPx ─────[330Ω]───────► WS2812 DIN
```

### SPI Devices (MLX90363, ILI9341)

**Standard SPI 4-wire + power:**

```
Device Pin | Signal | Pico Pin | Notes
───────────┼────────┼──────────┼──────────────────
VCC        | Power  | 3V3/VBUS | Check voltage!
GND        | Ground | GND      | Common GND
SCLK       | Clock  | GP18     | Shared bus
MISO       | Data   | GP16     | Shared bus
MOSI       | Data   | GP19     | Shared bus
CS/SS      | Select | GP17     | Unique per device
```

---

## Wire Gauge Recommendations

**Current capacity vs. wire gauge:**

| Wire Gauge (AWG) | Max Current | Use Case |
|------------------|-------------|----------|
| 30 AWG | 0.5A | Signal wires (I²C, SPI, GPIO) |
| 28 AWG | 1A | LED power, sensor power |
| 26 AWG | 2A | Pedals, USB power distribution |
| 24 AWG | 3.5A | WS2812 strip power |
| 22 AWG | 5A | Short motor power runs |
| 20 AWG | 7.5A | Motor power (< 1m) |
| 18 AWG | 10A | Motor power (recommended) |

---

## Connector Standards

**Recommended connector types:**

| Signal Type | Connector | Pitch | Notes |
|-------------|-----------|-------|-------|
| Pico GPIO | Dupont female | 2.54mm | Standard header |
| Panel wiring | JST-XH | 2.5mm | Polarized, secure |
| Motor power | XT60 / XT30 | — | High current rated |
| Pedals | DE-9 male | — | Standard Logitech |
| I²C bus | 4-pin JST-XH | 2.5mm | VCC/GND/SDA/SCL |
| SPI devices | 6-pin JST-XH | 2.5mm | Full SPI + power |

---

## Testing Points

**Measure these voltages after assembly (no load):**

| Test Point | Expected Voltage | Tolerance |
|------------|------------------|-----------|
| Pico VBUS | 5.0V | ±0.2V |
| Pico 3V3OUT | 3.3V | ±0.1V |
| MLX90363 VCC | 5.0V | ±0.2V |
| Pedal VCC | 3.3V | ±0.1V | (Protects Pico ADC - see note below) |
| Motor PSU + | 24V (stock) | ±1V |
| BTS7960 B+ | Same as PSU | ±0.5V |
| BTS7960 logic VCC | 3.3V | ±0.1V |

**Why pedals use 3.3V (not 5V):**
- Logitech pedals are just potentiometers (can handle 5V just fine)
- BUT: Pico ADC maximum input is 3.3V
- At 0Ω pot resistance, full VCC voltage goes to ADC pin
- Using 5V would fry the ADC (>3.3V input)
- Using 3.3V ensures even at 0Ω, ADC sees safe voltage
- **This is about protecting the Pico, not the pedals!**

**Continuity checks (power OFF):**

| Test | Expected Result |
|------|-----------------|
| Any VCC to GND | Open circuit (∞Ω) |
| Motor B+ to logic | Open circuit (∞Ω) |
| Base GND to Rim GND | Short circuit (~0Ω) |
| Signal wire end-to-end | < 1Ω |

---

*Diagrams are ASCII representations. Refer to component datasheets for electrical specifications.*

*Last updated: 2024-09-12*
