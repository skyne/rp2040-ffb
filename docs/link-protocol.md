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

Baud default: **460800** (inter-MCU `Serial1`). Pins: base GP0 TX / GP1 RX ↔ rim GP0 TX / GP1 RX (cross TX/RX).

Host USB CDC (ffb-config / telemetry inject) remains **115200** on the base — only the base↔rim link uses 460.8 kbaud.

RX path: each MCU drains UART into a 1 KiB power-of-two `ByteRing` (`FfbLink::ByteRing`) before the frame FSM, so OTA / 500 Hz bursts do not stall Core0. Integrity is **CRC-16/CCITT-FALSE** (not CRC-8) over `ver..payload`.

## Message IDs

| ID | Name | Direction | Payload |
|----|------|-----------|---------|
| `0x01` | Ping | base→rim | — |
| `0x02` | Pong | rim→base | — |
| `0x10` | Input | rim→base | `InputPayload` (buttons + enc deltas + flags; optional `analog[4]`) |
| `0x20` | Telemetry | base→rim | `TelemetryPayload` |
| `0x21` | ShiftLed | base→rim | raw LED map (optional) |
| `0x22` | BtnLed | base→rim | `BtnLedPayload` (`uint16_t` mask, bit0 = panel LED 1) |
| `0x23` | Display | both | Multi-page bank: `DispOpSetMeta` / `DispOpSetPageChunk` (8 widgets per chunk, up to 16/page) / `DispOpGetAll`. Layout is **not** in `RimConfig`. |
| `0x24` | AccelGet | base→rim | `AccelGetPayload` (mode + count; empty → single sample) |
| `0x25` | AccelReport | rim→base | `AccelReportPayload` (present/ok/XYZ; present=0 if ADXL missing) |
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
| 19–22 | Encoder shaft switches (unused on MCP panel — always 0) |
| 23–24 | Shifter paddles A/B (ADS A2/A3 ≥ ~35%; off if ADS absent) |
| 25–32 | Reserved |

| Axis | Role |
|------|------|
| X | Steering |
| Y | Throttle |
| Z | Brake |
| Rz | Pedal clutch |
| Rx (`sliderLeft`) | Rim clutch L (ADS A0); **0 if ADS absent** |
| Ry (`sliderRight`) | Rim clutch R (ADS A1); **0 if ADS absent** |

### `InputPayload` (rim→base)

| Field | Notes |
|-------|--------|
| `buttons` | Bits 0..9 = panel 1..10 |
| `encDelta[4]` | Raw detent steps this frame |
| `encSwitch` | Bits 0..3 (legacy shaft switches; 0 on MCP panel) |
| `flags` | `InputAlive`, `InputAdxl*`, `InputMcpBtnPresent`, `InputMcpLedPresent`, `InputAdsPresent` |
| `analog[4]` | ADS1115 raw A0..A3 (clutch L/R, shifter A/B); zeros / omit flag if chip absent |

Base accepts frames that only carry the core fields (pre-ADS) via `kInputPayloadCoreSize`.

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

Base: `duty_cap`, `spring_k`, `spring_dz`, `torque_cap`, `hid_range`, `gear_ratio`, `soft_limit_en`, `soft_limit_deg` (0 = half of `hid_range`), `soft_limit_k`, `adxl_cal` (0/1), `adxl_x_offset` (raw X at visual center), `adxl_present` (read-only)

CDC helpers: `:adxl` (one-shot sample), `:adxl_cal` (average 100 samples → RAM offset; `:save` to persist). INIT prefers rim ADXL gravity zero when present; otherwise magnet index window (unchanged).

Rim (EEPROM on rim; mirrored in base cache):

- `encN_mode`, `encN_steps`, `encN_accel`, `encN_thresh`, `encN_mult`, `encN_debounce`, `encN_pulse`, `encN_invert`, `encN_value` (`N`=0..3)
- `panel_led_bright`, `shift_led_bright`, `shift_led_count`, `disp_bright`
- `shift_rpm_0` .. `shift_rpm_3` fill stages; `shift_rpm_4` overrev — last two red RPM LEDs blink
- `layout_hex` — **active page** TFT blob (`layoutCount` + `DisplayElement[16]`, 129 bytes hex; legacy 65-byte / 8-widget blobs still accepted). Live `:set` → DisplayStore via chunked `0x23`.
- `layout_pageN_hex` / `pageN_bg` (`N`=0..2) — full multi-page bank (bg theme + layout per page)
- `disp_page` / `disp_pages` — active page index (0..2) and page count (1..3)
- `:disp page [N]` / `:disp pages [N]` / `:disp swipe L|R` / `:disp sync` — page bring-up helpers
- `rim_link` (read-only 0/1)

### Rim TFT layout (multi-page)

Widgets live in rim **DisplayStore** EEPROM and sync over message `0x23` in **chunks** (`DispOpSetPageChunk`, 8 elements per frame → up to **16 widgets/page**). Live racing only streams `Telemetry`; layout is pushed only when config changes. `RimConfig` / CfgSync carries brightness and encoders/LEDs — **not** the widget list.

Built-in **background themes**: Black / Carbon / Navy / Grid (`DispBgTheme`). Built-in **icon sprites** (`DispIconRpm`…`DispIconLap`) and **nav buttons** (`DispBtnPrev`/`Next`/`Page0..2`). Touch controller deferred — buttons are hit-test ready; page changes via widgets, CDC, or `:disp swipe`.

| Field | Notes |
|-------|--------|
| `layoutCount` | 0..16 active widgets on the page |
| `layout[i].type` | Text, bars, gauges, chrome, tyre/brake heat (vertical rect cells + `TyreCar`/`BrakeCar`), icons (32–38), buttons (40–44), timing/gaps (45–53), — see `DispElementType` in `ffb_link.h`. Labels include units (`kph`/`rpm`/`C`/`psi`/`s`). |
| `layout[i].x/y` | Pixel origin (or gauge center); landscape 320×240 |
| `layout[i].fontSize` | 1..4 — text size, bar/gauge/panel/icon scale |
| `layout[i].color565` | RGB565 accent / icon tint |
| `bgTheme` | Per-page (`pageN_bg`) |

Defaults: page 0 drive (live PB delta), page 1 tyres/brakes car layout, page 2 timings; page dots chrome at bottom.

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

- **ffb-config** owns settings **and** live race inject (same CDC session).
- External SimHub/serial companions still conflict if they open the same port — prefer the in-app **Race** tab.
- Close PlatformIO / other monitors before connecting.

**Race mode (LMU):** UDP JSON from the [Telemetry Socket plugin](https://community.lemansultimate.com/index.php?threads/telemetry-socket-%E2%80%93-json-telemetry-plugin.8229/) (default port 5000) is mapped to framed `Telemetry` (`0x20`) at ~50 Hz. Start/stop from the Race panel; `:companion 1` is asserted while race mode runs so diagnostic `T …` lines stay quiet. Closing the window with race mode on hides to the system tray.

**Companion mode (ASCII):** `:companion 1` quiets verbose `T …` lines. `:companion 0` restores live diagnostics. ASCII inject: `:tel <rpm> [gear] [flags]`. Binary `Telemetry` frames (type `0x20`) still preferred at high rate.

**Self-test:** `:selftest` → hall / rim / pedals / motors snapshot (`OK selftest` … `OK end`).

**Recenter:** `:recenter` (same as serial `z`) zeros the wheel at the current angle.

### How the host drives the shift strip

**Preferred:** ffb-config **Race** tab (LMU today; other games later). Any other host that opens the **base CDC** port (115200) can still drive LEDs with framed binary or ASCII test commands — but not at the same time as ffb-config.

**Live path (while racing):**

1. Host builds a `Telemetry` frame (`type = 0x20`) with `TelemetryPayload`:
   - `rpm` — shift lights
   - `flags` — bit mask below
   - (optional) `speedKphx10`, `gear`, `fuelPctx10`, `lapTimeMs`
   - (optional extension) `tyreTempC[4]`, `tyrePressPsi[4]`, `brakeTempC[4]` — FL/FR/RL/RR for TFT heat boxes
   - (optional timing) `deltaBestMs`, `deltaP1Ms`, `gapAheadMs`, `gapBehindMs` — signed ms; use `-32768` (`kTelemetryGapNa`) when unavailable. Negative delta = faster than reference.
   - (optional laps) `lastLapMs`, `bestLapMs`
2. Host writes the framed packet on USB CDC to the **base**.
3. Base validates CRC and forwards the same message UART → **rim**.
4. Rim `LedModeAuto` redraws: RPM bar + flag/aid/pit patterns.

Rim accepts core-sized (10-byte) telemetry from older hosts; missing tyre/brake/gap/lap fields read as 0.

TFT timing widgets: `DispDeltaBest`/`DispDeltaP1` (text), `DispGapAhead`/`Behind`/`GapStack`, `DispSectorSplitBest`/`P1` (±2 s marker bar), `DispLastLap`/`DispBestLap`. Car-layout composites: `DispTyreCar` (vertical °C+PSI cards), `DispBrakeCar` (vertical fill bars). Labels include units (`kph`, `rpm`, `C`, `psi`, `s`).

**Telemetry watchdog (rim Core0):** if no valid `Telemetry` frame arrives for **500 ms** (`FfbLink::kTelemetryTimeoutUs`), the rim enters **hardware standby**: WS2812 RPM/flags clear (linked idle = blue center blink), and the future TFT shows a standby / “Waiting for Telemetry” screen. The next telemetry packet resumes live updates automatically. GUI LED test modes (`ShiftLed`) are unaffected.

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

**Other hosts:**

- Prefer ffb-config Race mode when possible (same process owns CDC + settings).
- External SimHub / serial plugins still work if ffb-config is closed.
- ASCII `:leds_flags` / `:leds_rpm` remain bring-up only.

Game property names differ (ACC / iRacing / rF2 / etc.); the host mapper owns that. Firmware only sees `TelemetryPayload`.

**Manual test (ffb-config closed or Race off):** `:leds_flags 0x20` (pit), `:leds_flags 0x03` (Y+B), etc.

## Build

```bash
pio run -d firmware-base
pio run -d firmware-rim
pio run -d firmware-base -t upload
pio run -d firmware-rim -t upload   # direct USB bring-up / recovery
```
