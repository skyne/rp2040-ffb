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

On connect you get: live steering/pedal gauges, cal min/max/rest markers, status chips, and a raw serial log with a send box.

## Protocol

Firmware listens for colon-prefixed lines (115200):

- `:dump` / `:get <key>` / `:set <key> <value>`
- `:save` / `:load` / `:defaults` / `:log 0|1`

Keys: `duty_cap`, `spring_k`, `spring_dz`, `torque_cap`, `hid_range`, `gear_ratio`
