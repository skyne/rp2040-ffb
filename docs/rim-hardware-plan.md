# Rim hardware plan (locked)

Target architecture for the next rim panel build. **Not yet reflected in firmware** (`firmware-rim` still targets 3× PCA8574A). Confirm ADS1115 in parts stock before ordering substitutes.

**Decision date:** 2026-09-11

## Architecture

- **Base Pico** + **Rim Pico** only (no panel MCU / C3 / Nano).
- Front panel: **TSD-1166** illuminated tacts (integrated white LEDs, switches only on the face).
- Panel I/O via **2× slim MCP23017 modules** on I²C (not DIP bricks).
- Analogue paddles via **ADS1115** on the same I²C bus.
- Ambient light: **LDR** on a Pico ADC.
- Display: **ILI9341** on the rim (required).

## Rim Pico pin map

| GP | Function |
|----|----------|
| 0 | UART TX → base |
| 1 | UART RX ← base |
| 2 | spare |
| 3 | spare |
| 4 | I²C SDA |
| 5 | I²C SCL |
| 6 / 7 | Enc0 A / B |
| 8 / 9 | Enc1 A / B |
| 10 / 11 | Enc2 A / B |
| 12 / 13 | Enc3 A / B |
| 14 | WS2812 DIN (shift strip) |
| 15 | spare |
| 16 | TFT MISO |
| 17 | TFT CS |
| 18 | TFT SCLK |
| 19 | TFT MOSI |
| 20 | TFT DC |
| 21 | TFT RST |
| 22 | TFT BL |
| 26 | LDR voltage divider |
| 27 | Panel LED brightness PWM → FET (shared LED rail) |
| 28 | spare |

TFT pins match current `firmware-rim/src/config.h`.

## I²C bus (GP4 / GP5)

| Addr | Device | Role |
|------|--------|------|
| `0x20` | MCP23017 (A) | 10× TSD-1166 **switches** (GPA0–7, GPB0–1; pull-ups, active-low) |
| `0x21` | MCP23017 (B) | 10× TSD-1166 **LEDs** (GPA0–7, GPB0–1; ~100 Ω series each) |
| `0x48` | ADS1115 | 4× linear halls on A0–A3 (clutch L/R, shifter A/B) |
| `0x53` | ADXL345 | Optional accelerometer (soft-fail if absent) |

Set MCP `A0/A1/A2` for `0x20` / `0x21`. ADS1115 default is typically `0x48` (confirm on module pads).

### LED brightness

- Per-button **on/off**: yes (individual MCP pins).
- Per-button **brightness**: no.
- **Global** brightness / auto-dim from LDR: yes — common LED supply chopped by FET on **GP27**.

### Panel cable

Minimum hub ↔ panel: **3V3, GND, SDA, SCL**.  
If LED current should not come only from module 3V3, add a dedicated LED supply + FET high-side (still PWM’d from GP27).

## Encoders

- **4× EC12**, 3-pin (A / B / COM) — **no shaft switch / click**.
- Quadrature on GP6–13 as above.

## Explicitly out of scope (this plan)

- CD4051 / power-gated ADC timeshare
- ESP32-C3 or Arduino Nano panel MCU
- Charlieplexing / addressable LEDs under the TSD caps
- Encoder shaft buttons
- Per-LED PWM driver (e.g. PCA9685)

## Firmware follow-ups

When implementing:

1. Replace PCA8574 paths with dual MCP23017 (`0x20` inputs, `0x21` outputs).
2. Add ADS1115 sampling for four halls into the rim→base input payload (or extend protocol).
3. `analogRead` LDR on GP26 → drive GP27 PWM (+ optional map into `panelLedBright` / shift brightness).
4. Keep ILI9341 on reserved SPI1 pins; ADXL remains optional on shared I²C.
5. Update README BOM / rim wiring when firmware lands.
