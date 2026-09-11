# rp2040-ffb config (Tauri)

Cross-platform GUI for runtime settings over USB CDC.

## Prerequisites

- Rust + Node.js
- Linux (Ubuntu/Debian):

```bash
sudo apt update
sudo apt install -y libwebkit2gtk-4.1-dev libgtk-3-dev librsvg2-dev patchelf pkg-config
```

See also [Tauri prerequisites](https://tauri.app/start/prerequisites/).
- Pico in `dialout` group (Linux) so `/dev/ttyACM*` is accessible

## Run

```bash
cd tools/ffb-config
npm install
npm run tauri dev
```

Flash the latest **firmware-base** first (telemetry is `T key=value …` lines).

See [docs/link-protocol.md](../../docs/link-protocol.md) for rim settings keys, encoder HID mapping, and OTA.

Close the PlatformIO serial monitor before connecting — only one process can open the CDC port.

App icons (window / taskbar / tray) are generated from [`assets/logo.png`](../../assets/logo.png) via `src-tauri/app-icon.png`. To refresh:

```bash
# from tools/ffb-config
# (re-export a transparent 1024² PNG to src-tauri/app-icon.png if the logo changed)
./node_modules/.bin/tauri icon src-tauri/app-icon.png
```

On connect you get: live steering/pedal gauges, cal min/max/rest markers, status chips, and a raw serial log with a send box.

**Display tab** works offline (no MCU required): drag widgets on the 320×240 preview, auto-saves a draft in the browser, and Export/Import JSON (`ffb-tft-layout.json`). Apply / Refresh / Save to flash unlock when connected.

**Race tab** works offline (UDP listen / mapping preview). Connect the base to inject rim `Telemetry` frames on the same CDC session. While race mode is active, closing the window minimizes to the system tray (Show / Quit from the tray menu).

Plugin quick setup:

1. Copy `LeMansUltimateTelemetryPlugin.dll` into LMU `Plugins`
2. Enable in `UserData/player/CustomPluginVariables.JSON`:
   `"LeMansUltimateTelemetryPlugin.dll": { "Enabled": 1, "scoring": 1, "telemetry": 1 }`
3. Launch LMU, connect ffb-config, start **Race mode**

Prefer binding UDP locally / on a trusted LAN — the plugin can broadcast.

## Protocol

Firmware listens for colon-prefixed lines (115200):

- `:dump` / `:get <key>` / `:set <key> <value>`
- `:save` / `:load` / `:defaults` / `:log 0|1`

Keys: `duty_cap`, `spring_k`, `spring_dz`, `torque_cap`, `hid_range`, `gear_ratio`
