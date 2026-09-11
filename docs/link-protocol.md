# Link protocol (base-mcu ↔ rim-mcu)

Binary UART frames (also used on USB CDC when the host streams framed telemetry or rim firmware).

## Frame

| Field   | Size | Value |
|---------|------|-------|
| sync0   | 1    | `0xAA` |
| sync1   | 1    | `0x55` |
| ver     | 1    | `1` |
| type    | 1    | message id |
| len     | 1    | payload 0..128 |
| payload | n    | |
| crc16   | 2    | CCITT-FALSE over `ver..payload`, little-endian |

Baud default: **115200**. Pins: base GP0 TX / GP1 RX ↔ rim GP0 TX / GP1 RX (cross TX/RX).

## Message IDs

| ID | Name | Direction | Payload |
|----|------|-----------|---------|
| `0x01` | Ping | base→rim | — |
| `0x02` | Pong | rim→base | — |
| `0x10` | Input | rim→base | `InputPayload` |
| `0x20` | Telemetry | base→rim | `TelemetryPayload` |
| `0x21` | ShiftLed | base→rim | raw LED map (optional) |
| `0x22` | BtnLed | base→rim | bitmask |
| `0x23` | Display | base→rim | UI hints (later) |
| `0x30` | CfgSync | base→rim | `RimConfig` (live apply, no EEPROM) |
| `0x31` | CfgAck | rim→base | — (after CfgSave) |
| `0x32` | CfgGet | base→rim | — |
| `0x33` | CfgReport | rim→base | `RimConfig` (authoritative) |
| `0x34` | CfgSave | base→rim | — (persist rim EEPROM) |
| `0x40` | EnterBootloader | base→rim | — |
| `0x41` | UpdaterReady | rim→base | — |
| `0x42`..`0x48` | Fw* | both | OTA chunks / ACK |
| `0x51` | VersionGet | base→rim | — |
| `0x52` | VersionReport | rim→base | ASCII `fw_id` (len = payload; no NUL) |

See [`shared/ffb_link.h`](../shared/ffb_link.h) for packed structs.

## Settings ownership

| Domain | Stored on | Notes |
|--------|-----------|-------|
| FFB / pedals / HID range | **base EEPROM** | CDC `:save` / `:load` |
| Encoders / LEDs / display / shift RPM | **rim EEPROM** | Survives rim OTA (app image only) |

Base keeps a **RAM cache** of `RimConfig` for HID encoder policy and GUI dump. On link-up base sends `CfgGet`; rim replies `CfgReport`. Live `:set` of rim keys → cache + `CfgSync`. `:save` → base EEPROM + `CfgSave` on rim.

## HID map (base gamepad)

| Buttons | Role |
|---------|------|
| 1–10 | Panel push buttons |
| 11/12, 13/14, 15/16, 17/18 | Encoder 0..3 CW / CCW (momentary pulses) |
| 19–22 | Encoder shaft switches |
| 23–32 | Reserved |

Axes: X=steering, Y=throttle, Z=brake, Rz=clutch.

## Encoder policy

Rim sends raw detent deltas in `Input.encDelta[]`. Base applies cached `RimConfig.enc[i]`:

- `steps` — detents per HID click  
- `accel` / `thresh` / `mult` — fast turns → multiple clicks  
- `pulse` — HID press duration (ms)  
- `invert` — swap CW/CCW  

Tunable from **ffb-config** via `:set enc0_steps 2` etc.

## Settings keys (CDC ASCII)

Read-only identity (also in `:dump`; refresh rim via `VersionGet`):

- `base_fw` — base MCU `fw_id` (`release` + `build_utc`)
- `rim_fw` — rim MCU `fw_id` (cached; `?` if unknown)

CDC `:version` / `:ver` prints both after a VersionGet round-trip.

Base: `duty_cap`, `spring_k`, `spring_dz`, `torque_cap`, `hid_range`, `gear_ratio`

Rim (EEPROM on rim; mirrored in base cache):

- `encN_steps`, `encN_accel`, `encN_thresh`, `encN_mult`, `encN_debounce`, `encN_pulse`, `encN_invert` (`N`=0..3)
- `panel_led_bright`, `shift_led_bright`, `shift_led_count`, `disp_bright`
- `shift_rpm_0` .. `shift_rpm_3` fill stages; `shift_rpm_4` overrev — last two red RPM LEDs blink
- `rim_link` (read-only 0/1)

Rim control: `:rim_reset`, `:rim_bootsel`, `:rim_updater`, `:rim_sync` (CfgGet), `:rim_save` (CfgSave)

## WS2812 (rim)

- **Data (DIN):** rim **GP14**
- Default count: **11** (`shift_led_count`)
- Layout: **LED 0–1** race flags · **LED 2–8** RPM (7) · **LED 9–10** TC / ABS
- Flag bits in telemetry / `:leds_flags`: `1=yellow`, `2=blue`, `4=TC`, `8=ABS`
- Power from 5 V (VBUS) with common GND; series ~330 Ω on DIN recommended

GUI Rim tab: zones / flags / RPM tests. Commands: `:leds_*` (see help).

## Firmware update

**Pack (recommended):** `./scripts/pack-firmware.sh` → `dist/ffb-firmware-*.zip` → GUI **Update both (pack)**  
(rim OTA, then base `:bootsel` + UF2 copy). See [`firmware-pack.md`](firmware-pack.md).

**Rim only:** pick `firmware-rim/.pio/build/pico/firmware.bin` → **Flash rim only**. Stages to upper flash; **rim EEPROM preserved**.

**Base only:** GUI pack path, or USB UF2 / `pio run -d firmware-base -t upload`. Base CDC command `:bootsel` enters UF2 mode.

Brick recovery: hold rim BOOTSEL + USB UF2 (or base `:rim_bootsel` if wired).

## Telemetry vs config

- **ffb-config** owns settings (this protocol’s ASCII keys).  
- **SimHub / companion** owns live race telemetry (`Telemetry` / `ShiftLed` frames) while driving.  
- One CDC port; do not run both tools at once.

## Build

```bash
pio run -d firmware-base
pio run -d firmware-rim
pio run -d firmware-base -t upload
pio run -d firmware-rim -t upload   # direct USB bring-up / recovery
```
