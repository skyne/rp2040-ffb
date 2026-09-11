# Rim hardware plan (locked)

Target architecture for the rim panel. **Reflected in `firmware-rim`** (MCP23017 + ADS1115 soft-fail; LDR auto-dim + paddle HID).

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
| 15 | spare (reserved future XPT2046 T_IRQ) |
| 16 | TFT MISO |
| 17 | TFT CS |
| 18 | TFT SCLK |
| 19 | TFT MOSI |
| 20 | TFT DC |
| 21 | TFT RST |
| 22 | TFT BL |
| 26 | LDR voltage divider |
| 27 | Panel LED brightness PWM → FET (shared LED rail) |
| 28 | spare (reserved future XPT2046 T_CS / SPI CS) |

TFT pins match current `firmware-rim/src/config.h`.

## I²C bus (GP4 / GP5)

| Addr | Device | Role |
|------|--------|------|
| `0x20` | MCP23017 (A) | 10× TSD-1166 **switches** (GPA0–7, GPB0–1; pull-ups, active-low) |
| `0x21` | MCP23017 (B) | 10× TSD-1166 **LEDs** (GPA0–7, GPB0–1; ~100 Ω series each) |
| `0x48` | ADS1115 | 4× linear halls on A0–A3 (clutch L/R, shifter A/B) |
| `0x53` | ADXL345 | Optional accelerometer (soft-fail if absent) |

Set MCP `A0/A1/A2` for `0x20` / `0x21`. ADS1115 default is typically `0x48` (confirm on module pads).

Each device is **probed independently** at boot. Missing MCP / ADS / ADXL must not stop encoders, WS2812, UART, or the other I²C chips.

### LED brightness

- Per-button **on/off**: yes (individual MCP pins).
- Per-button **brightness**: no.
- **Global** brightness: `panelLedBright` × LDR ambient scale → PWM on **GP27**. No LDR (open GP26 / pulldown ≈ 0) → scale 255 (full).

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

## Firmware status

Done:

1. Dual MCP23017 (`0x20` inputs, `0x21` outputs) with soft-fail.
2. ADS1115 round-robin sampling → `InputPayload.analog[4]` + `InputAdsPresent` flag.
3. `panelLedBright` → GP27 PWM; LDR on GP26 auto-dims panel + shift LEDs (absent LDR → full brightness).
4. HID: Rx/Ry = clutch L/R (0 if no ADS); buttons 23/24 = shifter A/B from hall threshold.
5. ADXL remains optional on shared I²C.

Still open:

1. Per-paddle min/max calibration (raw→unit is linear 0..+FS today).

Done recently: ILI9341 multi-page dash (DisplayStore + built-in bg themes / icons / nav buttons; ffb-config page tabs). Touch HW (XPT2046 on GP15/GP28) deferred — software page model + CDC `:disp` only.
