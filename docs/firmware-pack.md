# Firmware pack (one-button update)

## Pack format (`ffb-fw-pack` v1)

Zip contents:

| File | Role |
|------|------|
| `manifest.json` | Product / hashes / release / **fw_id** |
| `base.uf2` | base-mcu image (USB BOOTSEL) |
| `rim.bin` | rim-mcu image (OTA via base) |

Example `manifest.json`:

```json
{
  "format": "ffb-fw-pack",
  "version": 1,
  "product": "rp2040-ffb",
  "release": "20260911",
  "build_utc": "2026-09-11T10:47:00Z",
  "fw_id": "20260911 2026-09-11T10:47:00Z",
  "base": {
    "file": "base.uf2",
    "sha256": "…",
    "size": 123456,
    "fw_id": "20260911 2026-09-11T10:47:00Z"
  },
  "rim": {
    "file": "rim.bin",
    "sha256": "…",
    "size": 49216,
    "fw_id": "20260911 2026-09-11T10:47:00Z"
  }
}
```

`fw_id` is `"{release} {build_utc}"`. The pack script stamps `shared/ffb_build_stamp.h` before building so **both MCUs report the same string** as the manifest.

## Build a pack

```bash
./scripts/pack-firmware.sh           # tag = UTC date
./scripts/pack-firmware.sh v0.2.0    # custom tag
```

Output: `dist/ffb-firmware-<tag>.zip`

Local `pio run` without the pack script uses tag `dev` and compile `__DATE__` / `__TIME__` (base and rim can differ).

## GUI update flow

1. Connect ffb-config to **base** CDC (rim linked over UART).
2. Rim tab → choose the zip → **Update both (pack)**.
3. App flashes **rim via OTA**, then sends `:bootsel` so base reboots into UF2 mode, then copies `base.uf2` onto the `RPI-RP2` volume.
4. Reconnect to the new base CDC when it reappears.
5. **Dump** (or connect auto-dump): compare **On-device firmware** `base_fw` / `rim_fw` to pack `fw_id`. Or CDC `:version`.

Rim-only: still available with a lone `.bin` / `.uf2`.

## Host / CI

Same script is CI-friendly (PlatformIO + `zip` + `sha256sum`). A GitHub Action can:

```yaml
- run: ./scripts/pack-firmware.sh ${{ github.ref_name }}
- uses: actions/upload-artifact@v4
  with:
    name: ffb-firmware
    path: dist/ffb-firmware-*.zip
```

## Notes

- Base cannot OTA itself over CDC yet; BOOTSEL + UF2 (or `picotool load -f`) is intentional.
- Pack update waits for `:bootsel` OK, then copies onto `RPI-RP2` (via `/proc/mounts` on Linux). If the volume never appears, GUI tries PlatformIO/`PATH` `picotool`.
- If BOOTSEL still fails: hold BOOTSEL on the base Pico and copy `base.uf2` from the unzipped pack manually.
- Rim EEPROM settings survive rim OTA; base settings stay in base EEPROM across base UF2.
