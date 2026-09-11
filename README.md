# rp2040-ffb

**Open DIY force-feedback steering wheel** built around two Raspberry Pi Pico (RP2040) boards — a **base MCU** that owns sensing, motors, pedals, and USB HID, and a **rim MCU** for buttons, encoders, and shift lights, linked over a framed UART protocol.

Mechanics are a **Logitech G920 or G923** base with the plastic rotation endstop limiter removed, plus an axle-mounted magnet for the index hall. The rest is Pico firmware, Logitech pedals, a Tauri desktop configurator, and a one-button firmware pack. Software FFB today is **spring + manual torque** for bench validation — not full USB PID effects from the game yet.

---

## Architecture

```
┌─────────────────────────────┐       UART 460800 CRC16      ┌─────────────────────────────┐
│         base-mcu            │◄────────────────────────────►│          rim-mcu            │
│  Raspberry Pi Pico          │   GP0/GP1 + RUN/BOOTSEL      │  Raspberry Pi Pico          │
│                             │                              │                             │
│  • MLX90363 steering hall   │                              │  • Core0: inputs + UART 500Hz│
│  • Axle index magnet        │                              │  • Core1: WS2812 (+ TFT WIP) │
│  • Dual BTS7960 (IBT-2)     │                              │  • PCA8574A buttons / LEDs  │
│  • Logitech pedal ADC       │                              │  • 4× EC12 encoders         │
│  • USB HID gamepad + CDC    │                              │  • WS2812 shift strip       │
│  • Status NeoPixel ring     │                              │  • Optional ADXL345 (I2C)  │
│                             │                              │  • OTA staged flash         │
└──────────────┬──────────────┘                              └─────────────────────────────┘
               │ USB CDC 115200
               ▼
        ┌──────────────┐
        │  ffb-config  │  Tauri GUI — settings, diagnostics, pack flash
        └──────────────┘
```

| Board | Role |
|-------|------|
| **base-mcu** | USB HID joystick, FFB motor drive, hall angle + homing, pedals, UART host for the rim |
| **rim-mcu** | Dual-core: Core0 panel/encoder I/O + link @ 500 Hz; Core1 WS2812 (TFT reserved); config EEPROM; OTA |

Shared protocol: [`shared/ffb_link.h`](shared/ffb_link.h) · full spec: [`docs/link-protocol.md`](docs/link-protocol.md)

---

## Status

| Area | State |
|------|--------|
| Steering (MLX90363 + gear ratio) | Working |
| Axle index / manual INIT | Working |
| Motorized INIT / seek | WIP |
| Pedals (G29/G920/G923 DE-9) | Working with auto-cal |
| USB HID (steer + 3 pedals + 32 buttons) | Working |
| Dual BTS7960 motor drive | WIP |
| FFB modes | **Off / Manual / Spring** — not game PID yet |
| Rim buttons + encoders | Working (Core0 @ 500 Hz) |
| Rim WS2812 shift lights | Working (Core1 ~60 FPS) |
| Rim display (ILI9341) | Optional — WIP (Core1 reserved) |
| Base↔rim UART | 460800 + CRC16 + RX ring buffer |
| Firmware pack + GUI OTA | Working |
| PCB / CAD | Coming soon — pin maps in `config.h` are source of truth |

---

## Repository layout

| Path | Description |
|------|-------------|
| [`firmware-base/`](firmware-base/) | Base Pico firmware (PlatformIO + Arduino-Pico) |
| [`firmware-rim/`](firmware-rim/) | Rim Pico firmware |
| [`shared/`](shared/) | Link protocol headers + build stamp |
| [`tools/ffb-config/`](tools/ffb-config/) | Cross-platform Tauri configurator |
| [`scripts/pack-firmware.sh`](scripts/pack-firmware.sh) | Build both MCUs → release zip |
| [`docs/link-protocol.md`](docs/link-protocol.md) | Frames, HID map, settings keys, LEDs, OTA |
| [`docs/firmware-pack.md`](docs/firmware-pack.md) | Pack format and GUI update flow |

---

## Bill of materials

No formal PCB BOM lives in the tree yet. The following is what the firmware targets today.

### Electronics

| Qty | Part | Notes |
|-----|------|--------|
| 2 | **Raspberry Pi Pico** (RP2040) | Base + rim; `board = rpipico` |
| 1 | **MLX90363** magnetic rotary sensor | Absolute angle on the G920/G923 sensor board |
| 1 | Digital hall (e.g. **A3144**-style OC) | Reads the axle index magnet; active-low into GP20 |
| 2 | **BTS7960 / IBT-2** H-bridge modules | One per FFB motor; logic @ 3V3, motor rail **12–24 V separate** — WIP |
| 1 | **Logitech G29 / G920 / G923 pedals** | DE-9: GND / Thr / Brk / Clu / VCC from Pico **3V3** |
| 1 | **HW-159** 7× WS2812 ring | Base status LEDs (GP21) |
| 1 | **WeAct Studio 3.7"** e-paper (GDEY037T03 / UC8253C) | Base status panel; **3V3** VCC; SCL/SDA on GP4/GP5 (PIO SPI, not hall) |
| 1 | **WS2812B** strip (default 11 LEDs) | Rim shift / flags (GP14); 5 V + ~330 Ω series on DIN |
| 3 | **PCA8574A** I²C expanders | `0x38` / `0x39` / `0x3A` — buttons + panel LEDs |
| 4 | **EC12** rotary encoders | Quadrature A/B on GP6–13 |
| — | Wiring, commons GND, optional series resistors | Motor B+/B− **never** onto Pico pins |
| 0–1 | **ILI9341** TFT | Optional — SPI1 pins reserved on rim; not required for the base |

### Mechanical

| Item | Notes |
|------|--------|
| **Logitech G920 or G923** wheel base | Stock dual-motor gearbox (~18:1; `gear_ratio`) |
| Plastic endstop limiter | **Removed** so the axle can turn freely past the stock stops |
| Index magnet on axle | Only hardware add to the donor gearbox; align `AXLE_INDEX_ANGLE_DEG` after measuring |
| Wheel rim / button panel | Hosts PCA8574A + encoders + LED strip |
| PSU for motors | Sized for dual BTS7960 load; isolated from USB logic |

Pin assignments are defined in:

- [`firmware-base/src/config.h`](firmware-base/src/config.h)
- [`firmware-rim/src/config.h`](firmware-rim/src/config.h)

**Next rim panel (planned):** [`docs/rim-hardware-plan.md`](docs/rim-hardware-plan.md) — dual MCP23017 + ADS1115 + ILI on rim (not in firmware yet).

---

## Wiring summary

### Base ↔ rim link

| Signal | Base | Rim |
|--------|------|-----|
| UART TX / RX | GP0 → rim RX, GP1 ← rim TX | GP0 / GP1 (crossed) |
| Rim RESET (RUN) | GP2 | Pico RUN |
| Rim BOOTSEL | GP3 | Pico BOOTSEL |
| GND | common | common |

Baud: **460800** (base↔rim UART), 3V3 logic. Host CDC stays **115200**.

### Base highlights

| Function | Pins |
|----------|------|
| Hall SPI (MLX90363) | MISO 16, SS 17, SCLK 18, MOSI 19 @ 500 kHz |
| Hall wire colors | red 5V · black GND · blue MISO · yellow /SS · orange SCLK · green MOSI |
| E-paper (WeAct 3.7") | SCL 4 · SDA 5 · CS 6 · DC 7 · RST 8 · BUSY 9 · VCC **3V3** (own PIO SPI; not hall bus) |
| Axle index | GP20 (`INPUT_PULLUP`, active-low) |
| Status NeoPixel | GP21 (×7) |
| Pedals ADC | Thr 26 · Brk 27 · Clu 28 |
| Pedal DE-9 | 1 GND · 2 Thr · 3 Brk · 4 Clu · 6&9 VCC (Pico 3V3) |
| Motor 1 | RPWM 10 · LPWM 11 · EN 12 |
| Motor 2 | RPWM 13 · LPWM 14 · EN 15 |

### Rim highlights

| Function | Pins |
|----------|------|
| I²C PCA8574A | SDA 4 · SCL 5 |
| Encoders 0–3 A/B | 6/7 · 8/9 · 10/11 · 12/13 |
| WS2812 DIN | GP14 (default 11 LEDs) |
| TFT (optional) | SCLK 18 · MOSI 19 · MISO 16 · CS 17 · DC 20 · RST 21 · BL 22 |

**Rim LED layout:** LEDs 0–1 flags · 2–8 RPM (or pit yellow blink) · 9–10 TC / ABS. Flag/aid pairs blink together (single signal = both LEDs that color; two signals alternate; red = both urgent).

---

## Prerequisites

- [PlatformIO Core](https://platformio.org/install/cli) (`pio`)
- USB access to the Picos (Linux: user in `dialout` for `/dev/ttyACM*`)
- For the GUI: **Node.js**, **Rust**, and [Tauri Linux deps](https://tauri.app/start/prerequisites/) (webkit2gtk, gtk, etc.)

Firmware platform: [`maxgerhardt/platform-raspberrypi`](https://github.com/maxgerhardt/platform-raspberrypi) with **Earle Philhower** Arduino-Pico core. Library: Adafruit NeoPixel.

---

## Build firmware

```bash
# Individual images
pio run -d firmware-base
pio run -d firmware-rim

# Release pack for the GUI (stamps both MCUs with the same fw_id)
./scripts/pack-firmware.sh           # tag = UTC date
./scripts/pack-firmware.sh v0.2.0    # custom tag
```

| Artifact | Path |
|----------|------|
| Base UF2 | `firmware-base/.pio/build/pico/firmware.uf2` |
| Rim BIN | `firmware-rim/.pio/build/pico/firmware.bin` |
| Pack zip | `dist/ffb-firmware-<tag>.zip` |

Pack contents: `manifest.json` + `base.uf2` + `rim.bin` (SHA-256 verified). Details: [`docs/firmware-pack.md`](docs/firmware-pack.md).

---

## Flash / update

| Method | When to use |
|--------|-------------|
| **ffb-config → Update both (pack)** | Recommended day-to-day: rim OTA, then base BOOTSEL UF2 |
| `pio run -d firmware-base -t upload` | Direct base USB |
| `pio run -d firmware-rim -t upload` | Direct rim USB / recovery |
| Hold **BOOTSEL** + copy UF2 to `RPI-RP2` | Manual / brick recovery |
| CDC `:bootsel` / `:rim_bootsel` | Enter UF2 from software (if wired) |

Rim OTA updates the app image only — **rim EEPROM settings are preserved**. Base settings live in base EEPROM across UF2 flashes.

If the BOOTSEL volume does not appear within ~45 s during a pack update, unzip the pack and copy `base.uf2` manually.

---

## Configurator (ffb-config)

```bash
cd tools/ffb-config
npm install
npm run tauri dev
```

Connect to the **base** CDC port (115200). Close any PlatformIO serial monitor first — only one process can own the port.

GUI covers base/FFB tuning, **profiles** (4 on-device slots), rim encoders & LEDs, diagnostics, serial monitor, and firmware pack flash. See [`tools/ffb-config/README.md`](tools/ffb-config/README.md).

**CDC examples:** `:dump` · `:get` / `:set` · `:save` / `:load` · `:version` · `:profile list|load N|save N` · `:leds_*` · `:rim_sync`

Do not run **ffb-config** and a SimHub/companion telemetry client on the same CDC port at once.

---

## HID map

| Control | Mapping |
|---------|---------|
| Axes | X steering · Y throttle · Z brake · Rz clutch |
| Buttons 1–10 | Panel |
| 11/12 … 17/18 | Encoder 0–3 CW/CCW (momentary pulses) |
| 19–22 | Encoder shaft switches |
| 23–32 | Reserved |

Default HID range: **±450°** (`hid_range` / `WHEEL_HID_RANGE_DEG = 900`).

---

## Safety

- Motor supply (**12–24 V**) is independent of the Pico. **Never** feed B+/B− into GPIO or VBUS logic rails.
- Stock plastic rotation stops are removed — soft limits are software-only. Keep motor duty capped (`MOTOR_DUTY_CAP = 0.35`) until FFB is trusted.
- Boot INIT prefers **ADXL345 gravity zero** when the rim reports the sensor; otherwise the existing **magnet index** window seek is used (500 ms probe timeout). Motorized seek remains WIP (`HOME_BOOT_USE_MOTORS = false`).
- Keep NeoPixel brightness modest on USB 5 V.

---

## Roadmap (high level)

- Motor drive + motorized INIT
- Full USB HID force-feedback (PID effects from the host)
- Optional rim ILI9341 UI
- First-party PCB + published BOM / gerbers
- Companion telemetry path (SimHub-friendly) without fighting the configurator

---

## Documentation

- [Link protocol, settings, LEDs, OTA](docs/link-protocol.md)
- [Firmware pack format](docs/firmware-pack.md)
- [ffb-config GUI](tools/ffb-config/README.md)

---

## License

No project license file is published yet. Third-party components retain their own licenses (e.g. Adafruit NeoPixel, Tauri). Choose and add an SPDX license before a public release if you fork or redistribute.
