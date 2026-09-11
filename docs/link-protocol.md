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
| `0x22` | BtnLed | base→rim | `BtnLedPayload` (`uint16_t` mask, bit0 = panel LED 1) |
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

- `mode` — `0` relative pulses (default) · `1` hold while turning · `2` absolute 0..100
- `steps` — detents per HID click / abs step  
- `accel` / `thresh` / `mult` — fast turns → multiple clicks (relative only)  
- `pulse` — HID press duration ms (relative) or hold-idle refresh ms (hold)  
- `invert` — swap CW/CCW  
- `value` — absolute position 0..100 (`:get` / `:set`; live in dump)

Tunable from **ffb-config** via `:set enc0_mode 1` etc.

## Settings keys (CDC ASCII)

Read-only identity (also in `:dump`; refresh rim via `VersionGet`):

- `base_fw` — base MCU `fw_id` (`release` + `build_utc`)
- `rim_fw` — rim MCU `fw_id` (cached; `?` if unknown)

CDC `:version` / `:ver` prints both after a VersionGet round-trip.

Base: `duty_cap`, `spring_k`, `spring_dz`, `torque_cap`, `hid_range`, `gear_ratio`, `soft_limit_en`, `soft_limit_deg` (0 = half of `hid_range`), `soft_limit_k`

Rim (EEPROM on rim; mirrored in base cache):

- `encN_mode`, `encN_steps`, `encN_accel`, `encN_thresh`, `encN_mult`, `encN_debounce`, `encN_pulse`, `encN_invert`, `encN_value` (`N`=0..3)
- `panel_led_bright`, `shift_led_bright`, `shift_led_count`, `disp_bright`
- `shift_rpm_0` .. `shift_rpm_3` fill stages; `shift_rpm_4` overrev — last two red RPM LEDs blink
- `rim_link` (read-only 0/1)

### Profiles (base EEPROM)

Four named slots (`0..3`) store FFB spring/torque/duty, `hid_range`, panel/shift LED brightness, and shift RPM thresholds. Pedals and `gear_ratio` are not part of a profile.

| Command | Action |
|---------|--------|
| `:profile` / `:profile list` | List slots + active |
| `:profile load N` | Apply slot live (then dump) |
| `:profile save N [name]` | Capture live settings into slot |
| `:profile name N <str>` | Rename slot (max 11 chars) |
| `:profile clear N` | Empty slot |

Active slot is marked in dump as `profile_active` (`none` if you `:set` a profile field manually). Factory defaults: Road / GT / Rally / Night. Persist with `:save`.

Rim control: `:rim_reset`, `:rim_bootsel`, `:rim_updater`, `:rim_sync` (CfgGet), `:rim_save` (CfgSave)

## WS2812 (rim)

- **Data (DIN):** rim **GP14**
- Default count: **11** (`shift_led_count`)
- Layout: **LED 0–1** race flags · **LED 2–8** RPM (7) · **LED 9–10** TC / ABS
- Flag bits in telemetry / `:leds_flags`: `1=yellow`, `2=blue`, `4=TC`, `8=ABS`, `16=red`, `32=pit`
- Flag / aid zones: both LEDs share one active signal (blink); two signals alternate; red = urgent both-red blink
- Pit limiter (`32`): middle 7 RPM LEDs yellow blink (overrides RPM bar while set)
- Power from 5 V (VBUS) with common GND; series ~330 Ω on DIN recommended

GUI Rim tab: zones / flags / RPM tests. Commands: `:leds_*` (see help).

## Firmware update

**Pack (recommended):** `./scripts/pack-firmware.sh` → `dist/ffb-firmware-*.zip` → GUI **Update both (pack)**  
(rim OTA, then base `:bootsel` + UF2 copy). See [`firmware-pack.md`](firmware-pack.md).

**Rim only:** pick `firmware-rim/.pio/build/pico/firmware.bin` → **Flash rim only**. Stages to upper flash; **rim EEPROM preserved**.

**Base only:** GUI pack path, or USB UF2 / `pio run -d firmware-base -t upload`. Base CDC command `:bootsel` enters UF2 mode.

Soft updater: orange middle RPM LED; inputs paused. Rim exits on `FwFail`, successful stage+reboot, or **15 s idle** (`FwFailTimeout`). GUI sends `:rim_reset` after a failed OTA. Manual recovery: `:rim_reset` (or power-cycle rim).

Brick recovery: hold rim BOOTSEL + USB UF2 (or base `:rim_bootsel` if wired).

## Telemetry vs config

- **ffb-config** owns settings (this protocol’s ASCII keys).  
- **SimHub / companion** owns live race telemetry while driving.  
- One CDC port; do not run both tools at once.

**Companion mode:** `:companion 1` quiets verbose `T …` lines so a serial plugin can own the port. `:companion 0` (or reconnect with ffb-config) restores live diagnostics. ASCII inject: `:tel <rpm> [gear] [flags]`. Binary `Telemetry` frames (type `0x20`) still preferred at high rate.

**Self-test:** `:selftest` → hall / rim / pedals / motors snapshot (`OK selftest` … `OK end`).

**Recenter:** `:recenter` (same as serial `z`) zeros the wheel at the current angle.

### How the host drives the shift strip

There is **no built-in SimHub plugin** in this repo yet. Any host that can open the **base CDC** port (115200) can drive LEDs by sending framed binary (preferred) or ASCII test commands.

**Live path (while racing):**

1. Host builds a `Telemetry` frame (`type = 0x20`) with `TelemetryPayload`:
   - `rpm` — shift lights
   - `flags` — bit mask below
   - (optional) `speedKphx10`, `gear`, `fuelPctx10`, `lapTimeMs` for later UI
2. Host writes the framed packet on USB CDC to the **base**.
3. Base validates CRC and forwards the same message UART → **rim**.
4. Rim `LedModeAuto` redraws: RPM bar + flag/aid/pit patterns.

**`flags` bit map (set from game properties):**

| Bit | Value | Typical SimHub / game property | Strip effect |
|-----|-------|--------------------------------|--------------|
| 0 | `0x01` | Yellow flag | Both flag LEDs yellow blink |
| 1 | `0x02` | Blue flag | Both flag LEDs blue blink |
| 2 | `0x04` | TC active | Both aid LEDs amber blink |
| 3 | `0x08` | ABS active | Both aid LEDs cyan blink |
| 4 | `0x10` | Red flag | Both flag LEDs urgent red blink |
| 5 | `0x20` | Pit limiter | Middle 7 LEDs yellow blink (overrides RPM bar) |

Yellow+blue together → alternate; TC+ABS together → alternate. Red overrides yellow/blue on the flag pair.

**SimHub options (when you wire it):**

- **Custom serial plugin** / small companion that maps game properties → `Telemetry` frames (recommended).
- Or map properties → ASCII `:leds_flags <mask>` / `:leds_rpm <rpm> [flags]` for bring-up only (less efficient; not ideal at 60 Hz).

Game property names differ (ACC / iRacing / rF2 / etc.); the companion owns that mapping. Firmware only sees `rpm` + `flags`.

**Manual test (ffb-config closed):** `:leds_flags 0x20` (pit), `:leds_flags 0x03` (Y+B), etc.

## Build

```bash
pio run -d firmware-base
pio run -d firmware-rim
pio run -d firmware-base -t upload
pio run -d firmware-rim -t upload   # direct USB bring-up / recovery
```
