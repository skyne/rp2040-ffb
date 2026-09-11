mod map_lmu;

use map_lmu::{LmuState, MSG_TELEMETRY};
use parking_lot::Mutex;
use serde::Serialize;
use serialport::SerialPort;
use std::collections::BTreeMap;
use std::io::{Read, Write};
use std::net::UdpSocket;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc;
use std::sync::Arc;
use std::time::{Duration, Instant};
use tauri::menu::{Menu, MenuItem};
use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
use tauri::{AppHandle, Emitter, Manager, State, WindowEvent};

enum ReplyKind {
    Simple,
    Dump,
}

struct PendingReply {
    kind: ReplyKind,
    buf: Vec<String>,
    saw_dump: bool,
    tx: mpsc::Sender<Result<Vec<String>, String>>,
}

enum IoCmd {
    Write {
        line: String,
        reply: Option<(ReplyKind, mpsc::Sender<Result<Vec<String>, String>>)>,
    },
    WriteBytes {
        data: Vec<u8>,
    },
    Flash {
        image: Vec<u8>,
        reply: mpsc::Sender<Result<(), String>>,
    },
    Shutdown,
}

struct RaceControl {
    enabled: bool,
    udp_port: u16,
    stop_tx: Option<mpsc::Sender<()>>,
    state: Arc<Mutex<LmuState>>,
}

impl Default for RaceControl {
    fn default() -> Self {
        Self {
            enabled: false,
            udp_port: 5000,
            stop_tx: None,
            state: Arc::new(Mutex::new(LmuState::default())),
        }
    }
}

struct AppState {
    cmd_tx: Mutex<Option<mpsc::Sender<IoCmd>>>,
    race: Mutex<RaceControl>,
    flash_busy: Arc<AtomicBool>,
}

#[derive(Serialize, Clone, Default)]
#[serde(rename_all = "camelCase")]
struct RaceStatus {
    enabled: bool,
    udp_port: u16,
    telem_hz: f64,
    scoring_hz: f64,
    rpm: u16,
    gear: i8,
    speed_kph: f64,
    fuel_pct: f64,
    flags: u8,
    last_error: String,
    connected: bool,
}

#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct PortInfo {
    name: String,
    port_type: String,
    vid: Option<u16>,
    pid: Option<u16>,
    manufacturer: Option<String>,
    product: Option<String>,
    /// Heuristic: Raspberry Pi Pico / rp2040 CDC likely used by this firmware.
    likely_pico: bool,
}

#[derive(Serialize, Clone, Default)]
#[serde(rename_all = "camelCase")]
struct Telemetry {
    axle: f64,
    hid_x: i32,
    sens: f64,
    hall: bool,
    idx: bool,
    idx_h: bool,
    edges: u32,
    home: String,
    home_m: bool,
    adxl: bool,
    ax: i32,
    gear: f64,
    motors: bool,
    ffb: String,
    torq: f64,
    hid_range: f64,
    rim: bool,
    adc_t: u32,
    adc_b: u32,
    adc_c: u32,
    n_t: f64,
    n_b: f64,
    n_c: f64,
    t_min: i32,
    t_max: i32,
    t_rest: i32,
    t_arm: bool,
    b_min: i32,
    b_max: i32,
    b_rest: i32,
    b_arm: bool,
    c_min: i32,
    c_max: i32,
    c_rest: i32,
    c_arm: bool,
}

fn port_is_likely_pico(port: &serialport::SerialPortInfo) -> bool {
    match &port.port_type {
        serialport::SerialPortType::UsbPort(usb) => {
            // Raspberry Pi Ltd USB VID
            if usb.vid == 0x2E8A {
                return true;
            }
            let blob = format!(
                "{} {} {}",
                usb.manufacturer.as_deref().unwrap_or(""),
                usb.product.as_deref().unwrap_or(""),
                port.port_name
            )
            .to_ascii_lowercase();
            blob.contains("pico")
                || blob.contains("rp2040")
                || blob.contains("raspberry")
        }
        _ => {
            let n = port.port_name.to_ascii_lowercase();
            n.contains("ttyacm") || n.contains("usbmodem")
        }
    }
}

#[tauri::command]
fn list_ports() -> Result<Vec<PortInfo>, String> {
    let ports = serialport::available_ports().map_err(|e| e.to_string())?;
    Ok(ports
        .into_iter()
        .map(|p| {
            let (vid, pid, manufacturer, product) = match &p.port_type {
                serialport::SerialPortType::UsbPort(usb) => (
                    Some(usb.vid),
                    Some(usb.pid),
                    usb.manufacturer.clone(),
                    usb.product.clone(),
                ),
                _ => (None, None, None, None),
            };
            let likely_pico = port_is_likely_pico(&p);
            PortInfo {
                name: p.port_name,
                port_type: format!("{:?}", p.port_type),
                vid,
                pid,
                manufacturer,
                product,
                likely_pico,
            }
        })
        .collect())
}

fn open_port(path: &str) -> Result<Box<dyn SerialPort>, String> {
    let mut port = serialport::new(path, 115_200)
        .timeout(Duration::from_millis(30))
        .flow_control(serialport::FlowControl::None)
        .dtr_on_open(false)
        .open()
        .map_err(|e| format!("open {path}: {e}"))?;
    let _ = port.write_data_terminal_ready(false);
    let _ = port.write_request_to_send(false);
    Ok(port)
}

fn parse_kv_map(lines: &[String]) -> BTreeMap<String, String> {
    let mut map = BTreeMap::new();
    for line in lines {
        if line.starts_with("OK") || line.starts_with("ERR") {
            continue;
        }
        if let Some((k, v)) = line.split_once('=') {
            map.insert(k.trim().to_string(), v.trim().to_string());
        }
    }
    map
}

fn parse_f64(map: &BTreeMap<&str, &str>, key: &str) -> f64 {
    map.get(key)
        .and_then(|v| v.parse().ok())
        .unwrap_or(0.0)
}

fn parse_i32(map: &BTreeMap<&str, &str>, key: &str) -> i32 {
    map.get(key)
        .and_then(|v| v.parse().ok())
        .unwrap_or(0)
}

fn parse_u32(map: &BTreeMap<&str, &str>, key: &str) -> u32 {
    map.get(key)
        .and_then(|v| v.parse().ok())
        .unwrap_or(0)
}

fn parse_bool01(map: &BTreeMap<&str, &str>, key: &str) -> bool {
    map.get(key).map(|v| *v != "0").unwrap_or(false)
}

fn parse_telemetry(line: &str) -> Option<Telemetry> {
    let body = line.strip_prefix("T ")?;
    let mut map = BTreeMap::new();
    for part in body.split_whitespace() {
        if let Some((k, v)) = part.split_once('=') {
            map.insert(k, v);
        }
    }
    Some(Telemetry {
        axle: parse_f64(&map, "axle"),
        hid_x: parse_i32(&map, "hidX"),
        sens: parse_f64(&map, "sens"),
        hall: parse_bool01(&map, "hall"),
        idx: parse_bool01(&map, "idx"),
        idx_h: parse_bool01(&map, "idxH"),
        edges: parse_u32(&map, "edges"),
        home: map.get("home").unwrap_or(&"-").to_string(),
        home_m: parse_bool01(&map, "homeM"),
        adxl: parse_bool01(&map, "adxl"),
        ax: parse_i32(&map, "ax"),
        gear: parse_f64(&map, "gear"),
        motors: parse_bool01(&map, "motors"),
        ffb: map.get("ffb").unwrap_or(&"?").to_string(),
        torq: parse_f64(&map, "torq"),
        hid_range: parse_f64(&map, "hidRange"),
        rim: parse_bool01(&map, "rim"),
        adc_t: parse_u32(&map, "adcT"),
        adc_b: parse_u32(&map, "adcB"),
        adc_c: parse_u32(&map, "adcC"),
        n_t: parse_f64(&map, "nT"),
        n_b: parse_f64(&map, "nB"),
        n_c: parse_f64(&map, "nC"),
        t_min: parse_i32(&map, "tMin"),
        t_max: parse_i32(&map, "tMax"),
        t_rest: parse_i32(&map, "tRest"),
        t_arm: parse_bool01(&map, "tArm"),
        b_min: parse_i32(&map, "bMin"),
        b_max: parse_i32(&map, "bMax"),
        b_rest: parse_i32(&map, "bRest"),
        b_arm: parse_bool01(&map, "bArm"),
        c_min: parse_i32(&map, "cMin"),
        c_max: parse_i32(&map, "cMax"),
        c_rest: parse_i32(&map, "cRest"),
        c_arm: parse_bool01(&map, "cArm"),
    })
}

fn is_noise_for_reply(line: &str) -> bool {
    line.starts_with("T ")
        || line.starts_with("axle=")
        || line.starts_with("INDEX ")
        || line.starts_with("boot")
        || line.starts_with("init ")
        || line.starts_with("rp2040-ffb")
        || line.starts_with("settings:")
        || line.starts_with("cfg:")
        || line.starts_with("keys:")
        || line.starts_with("If jstest")
        || line.is_empty()
}

fn feed_pending(pending: &mut Option<PendingReply>, line: &str) -> bool {
    let Some(p) = pending.as_mut() else {
        return false;
    };
    if is_noise_for_reply(line) {
        return false;
    }
    p.buf.push(line.to_string());
    match p.kind {
        ReplyKind::Simple => {
            if line.starts_with("OK") || line.starts_with("ERR") {
                let done = pending.take().unwrap();
                let _ = done.tx.send(Ok(done.buf));
                return true;
            }
        }
        ReplyKind::Dump => {
            if line.starts_with("ERR") {
                let done = pending.take().unwrap();
                let _ = done.tx.send(Ok(done.buf));
                return true;
            }
            if line == "OK dump" || line == "OK selftest" || line == "OK profiles" {
                p.saw_dump = true;
            } else if p.saw_dump && line == "OK end" {
                let done = pending.take().unwrap();
                let _ = done.tx.send(Ok(done.buf));
                return true;
            }
        }
    }
    false
}

fn crc16_ccitt(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    for &b in data {
        crc ^= (b as u16) << 8;
        for _ in 0..8 {
            if (crc & 0x8000) != 0 {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    crc
}

fn crc32_ieee(data: &[u8]) -> u32 {
    let mut c: u32 = 0xFFFF_FFFF;
    for &b in data {
        c ^= b as u32;
        for _ in 0..8 {
            if (c & 1) != 0 {
                c = (c >> 1) ^ 0xEDB8_8320;
            } else {
                c >>= 1;
            }
        }
    }
    !c
}

fn build_frame(msg_type: u8, payload: &[u8]) -> Vec<u8> {
    assert!(payload.len() <= 128);
    let mut body = Vec::with_capacity(3 + payload.len());
    body.push(1); // ver
    body.push(msg_type);
    body.push(payload.len() as u8);
    body.extend_from_slice(payload);
    let crc = crc16_ccitt(&body);
    let mut out = Vec::with_capacity(2 + body.len() + 2);
    out.push(0xAA);
    out.push(0x55);
    out.extend_from_slice(&body);
    out.push((crc & 0xFF) as u8);
    out.push((crc >> 8) as u8);
    out
}

fn try_pop_frame(buf: &mut Vec<u8>) -> Option<(u8, Vec<u8>)> {
    // Hunt for sync
    while buf.len() >= 2 {
        if buf[0] == 0xAA && buf[1] == 0x55 {
            break;
        }
        buf.remove(0);
    }
    if buf.len() < 5 {
        return None;
    }
    let plen = buf[4] as usize;
    if plen > 128 {
        buf.remove(0);
        return None;
    }
    let total = 2 + 3 + plen + 2;
    if buf.len() < total {
        return None;
    }
    let frame: Vec<u8> = buf[..total].to_vec();
    let body = &frame[2..2 + 3 + plen];
    let got = u16::from_le_bytes([frame[total - 2], frame[total - 1]]);
    let expect = crc16_ccitt(body);
    if got != expect {
        // Bad CRC — drop one sync byte and resync (don't discard the rest)
        buf.remove(0);
        return None;
    }
    buf.drain(..total);
    let msg_type = body[1];
    let payload = body[3..].to_vec();
    Some((msg_type, payload))
}

fn is_uf2(data: &[u8]) -> bool {
    data.len() >= 8 && data[0..4] == [0x55, 0x46, 0x32, 0x0A]
}

fn uf2_to_bin(data: &[u8]) -> Result<Vec<u8>, String> {
    if data.len() % 512 != 0 {
        return Err("UF2 size not multiple of 512".into());
    }
    let mut max_end = 0usize;
    let mut chunks: Vec<(usize, &[u8])> = Vec::new();
    for block in data.chunks_exact(512) {
        let magic0 = u32::from_le_bytes(block[0..4].try_into().unwrap());
        let magic1 = u32::from_le_bytes(block[4..8].try_into().unwrap());
        let magic_end = u32::from_le_bytes(block[508..512].try_into().unwrap());
        if magic0 != 0x0A32_4655 || magic1 != 0x9E5D_5157 || magic_end != 0x0AB1_6F30 {
            return Err("bad UF2 magic".into());
        }
        let flags = u32::from_le_bytes(block[8..12].try_into().unwrap());
        if (flags & 0x1) != 0 {
            continue; // not main flash
        }
        let addr = u32::from_le_bytes(block[12..16].try_into().unwrap());
        let payload_size = u32::from_le_bytes(block[16..20].try_into().unwrap()) as usize;
        if !(0x1000_0000..0x1020_0000).contains(&addr) {
            continue;
        }
        let off = (addr - 0x1000_0000) as usize;
        let payload = &block[32..32 + payload_size];
        max_end = max_end.max(off + payload_size);
        chunks.push((off, payload));
    }
    if max_end == 0 {
        return Err("no RP2040 flash blocks in UF2".into());
    }
    let mut out = vec![0xFFu8; max_end];
    for (off, payload) in chunks {
        out[off..off + payload.len()].copy_from_slice(payload);
    }
    Ok(out)
}

fn try_pop_ascii_fw(buf: &mut Vec<u8>) -> Option<(u8, Vec<u8>)> {
    // Find a full line containing "OK FW " (tolerate leading junk on the line)
    let newline = buf.iter().position(|&b| b == b'\n')?;
    let raw = buf.drain(..=newline).collect::<Vec<u8>>();
    let line = String::from_utf8_lossy(&raw)
        .trim_end_matches(['\r', '\n'])
        .to_string();
    let idx = line.find("OK FW ")?;
    let rest = &line[idx + "OK FW ".len()..];
    let mut parts = rest.split_whitespace();
    let type_str = parts.next()?;
    let msg_type = u8::from_str_radix(type_str, 16).ok()?;
    let mut payload = Vec::new();
    for p in parts {
        payload.push(u8::from_str_radix(p, 16).ok()?);
    }
    Some((msg_type, payload))
}

fn try_pop_any_fw(buf: &mut Vec<u8>) -> Option<(u8, Vec<u8>)> {
    // Prefer binary frames if sync is at front; else try ASCII line.
    if buf.len() >= 2 && buf[0] == 0xAA && buf[1] == 0x55 {
        return try_pop_frame(buf);
    }
    if let Some(pos) = buf.iter().position(|&b| b == b'\n') {
        // Only consume if this line looks like OK FW; else drop the line as noise
        let line = String::from_utf8_lossy(&buf[..=pos]);
        if line.contains("OK FW ") {
            return try_pop_ascii_fw(buf);
        }
        buf.drain(..=pos);
        return None;
    }
    // Discard leading junk until sync or 'O' of OK
    while !buf.is_empty() && buf[0] != 0xAA && buf[0] != b'O' {
        buf.remove(0);
    }
    None
}

fn wait_frame(
    app: &AppHandle,
    port: &mut Box<dyn SerialPort>,
    buf: &mut Vec<u8>,
    chunk: &mut [u8; 512],
    want: u8,
    timeout: Duration,
    phase: &str,
    // For FwAck (0x44): ignore stale ACKs with offset < min_ack_offset
    min_ack_offset: Option<u32>,
) -> Result<Vec<u8>, String> {
    let deadline = Instant::now() + timeout;
    loop {
        if Instant::now() > deadline {
            return Err(format!(
                "timeout waiting for frame 0x{want:02X} ({phase})"
            ));
        }
        while let Some((t, payload)) = try_pop_any_fw(buf) {
            let _ = app.emit(
                "flash-progress",
                serde_json::json!({ "event": "frame", "type": t, "phase": phase }),
            );
            if t == 0x45 {
                return Err(format!("NAK during {phase}"));
            }
            if t == 0x48 {
                let reason = payload.first().copied().unwrap_or(0);
                return Err(format!("FwFail reason={reason} during {phase}"));
            }
            if t != want {
                continue;
            }
            if want == 0x44 {
                if let Some(min) = min_ack_offset {
                    if payload.len() < 4 {
                        return Err(
                            "rim sent empty FwAck — rim firmware is too old for OTA. \
                             USB-flash firmware-rim once, then retry OTA"
                                .into(),
                        );
                    }
                    let got = u32::from_le_bytes(payload[0..4].try_into().unwrap());
                    if got < min {
                        // Stale ACK — keep waiting
                        continue;
                    }
                }
            }
            return Ok(payload);
        }
        match port.read(chunk) {
            Ok(n) if n > 0 => buf.extend_from_slice(&chunk[..n]),
            _ => std::thread::sleep(Duration::from_millis(2)),
        }
    }
}

fn run_flash(
    app: &AppHandle,
    port: &mut Box<dyn SerialPort>,
    buf: &mut Vec<u8>,
    chunk: &mut [u8; 512],
    image: &[u8],
) -> Result<(), String> {
    const READY: u8 = 0x41;
    const BEGIN: u8 = 0x42;
    const DATA: u8 = 0x43;
    const ACK: u8 = 0x44;
    const END: u8 = 0x46;
    const DONE: u8 = 0x47;
    const DATA_CHUNK: usize = 56;

    if image.is_empty() || image.len() > 192 * 1024 {
        return Err(format!("image size {} out of range", image.len()));
    }

    let _ = app.emit(
        "flash-progress",
        serde_json::json!({ "event": "start", "total": image.len() }),
    );

    let _ = port.write_all(b":log 0\n");
    let _ = port.flush();
    std::thread::sleep(Duration::from_millis(80));
    buf.clear();

    // Prefer ASCII :rim_updater — base builds CRC and writes UART directly.
    port.write_all(b":rim_updater\n")
        .and_then(|_| port.flush())
        .map_err(|e| format!("enter: {e}"))?;

    let _ = wait_frame(
        app,
        port,
        buf,
        chunk,
        READY,
        Duration::from_secs(8),
        "updater-ready",
        None,
    )
    .map_err(|e| {
        format!(
            "{e}. Rim did not enter updater — check UART (rim LEDs not all-red), \
             USB-flash firmware-rim once if never done, USB-flash firmware-base, \
             then retry. Serial: click Rim updater and look for 'OK FW 41'."
        )
    })?;
    // Late duplicate Ready is ignored by later wait_frame (wrong type).
    std::thread::sleep(Duration::from_millis(20));

    let mut begin_pl = Vec::with_capacity(8);
    begin_pl.extend_from_slice(&(image.len() as u32).to_le_bytes());
    begin_pl.extend_from_slice(&crc32_ieee(image).to_le_bytes());
    port.write_all(&build_frame(BEGIN, &begin_pl))
        .and_then(|_| port.flush())
        .map_err(|e| format!("begin: {e}"))?;
    let _ = wait_frame(
        app,
        port,
        buf,
        chunk,
        ACK,
        Duration::from_secs(5),
        "fw-begin-ack",
        Some(0),
    )?;

    let mut offset = 0usize;
    while offset < image.len() {
        let n = (image.len() - offset).min(DATA_CHUNK);
        let mut pl = Vec::with_capacity(4 + n);
        pl.extend_from_slice(&(offset as u32).to_le_bytes());
        pl.extend_from_slice(&image[offset..offset + n]);
        port.write_all(&build_frame(DATA, &pl))
            .and_then(|_| port.flush())
            .map_err(|e| format!("data @{offset}: {e}"))?;
        let expect = (offset + n) as u32;
        let ack = wait_frame(
            app,
            port,
            buf,
            chunk,
            ACK,
            Duration::from_secs(5),
            &format!("fw-data @{offset}"),
            Some(expect),
        )?;
        if ack.len() >= 4 {
            let got = u32::from_le_bytes(ack[0..4].try_into().unwrap());
            if got != expect {
                return Err(format!("ACK offset mismatch: got {got} want {expect}"));
            }
        }
        offset += n;
        let _ = app.emit(
            "flash-progress",
            serde_json::json!({ "received": offset, "total": image.len() }),
        );
    }

    port.write_all(&build_frame(END, &[]))
        .and_then(|_| port.flush())
        .map_err(|e| format!("end: {e}"))?;
    let _ = wait_frame(
        app,
        port,
        buf,
        chunk,
        DONE,
        Duration::from_secs(30),
        "fw-done",
        None,
    )?;
    // Soft reboot → apply slot 0 (~1–2s). Wait it out, then pulse RUN in case the
    // rim parked after FwDone without resetting (dual-core / XIP races).
    std::thread::sleep(Duration::from_millis(3500));
    let _ = port.write_all(b":rim_reset\n");
    let _ = port.flush();
    std::thread::sleep(Duration::from_millis(500));
    let _ = app.emit("flash-progress", serde_json::json!({ "event": "done" }));
    Ok(())
}

/// Soft-updater has no host abort; hardware reset clears a parked rim OTA session.
fn recover_rim_after_ota_fail(port: &mut Box<dyn SerialPort>) {
    let _ = port.write_all(b":rim_reset\n");
    let _ = port.flush();
    std::thread::sleep(Duration::from_millis(400));
}

#[derive(serde::Deserialize)]
struct PackFileInfo {
    file: String,
    sha256: String,
    #[allow(dead_code)]
    size: u64,
    #[allow(dead_code)]
    fw_id: Option<String>,
}

#[derive(serde::Deserialize)]
struct PackManifest {
    format: String,
    version: u32,
    release: Option<String>,
    #[allow(dead_code)]
    build_utc: Option<String>,
    fw_id: Option<String>,
    base: PackFileInfo,
    rim: PackFileInfo,
}

struct FirmwarePack {
    release: String,
    fw_id: String,
    base_uf2: Vec<u8>,
    rim_bin: Vec<u8>,
}

fn sha256_hex(data: &[u8]) -> String {
    use sha2::{Digest, Sha256};
    let mut h = Sha256::new();
    h.update(data);
    hex::encode(h.finalize())
}

fn parse_firmware_pack(zip_bytes: &[u8]) -> Result<FirmwarePack, String> {
    use std::io::Cursor;
    use zip::ZipArchive;

    let cursor = Cursor::new(zip_bytes);
    let mut archive = ZipArchive::new(cursor).map_err(|e| format!("zip open: {e}"))?;

    let mut files: BTreeMap<String, Vec<u8>> = BTreeMap::new();
    for i in 0..archive.len() {
        let mut f = archive
            .by_index(i)
            .map_err(|e| format!("zip entry: {e}"))?;
        let name = f.name().replace('\\', "/");
        let base = name.rsplit('/').next().unwrap_or(&name).to_string();
        if base.is_empty() || base == "." {
            continue;
        }
        let mut data = Vec::new();
        f.read_to_end(&mut data)
            .map_err(|e| format!("zip read {base}: {e}"))?;
        files.insert(base, data);
    }

    let manifest_raw = files
        .get("manifest.json")
        .ok_or_else(|| "pack missing manifest.json".to_string())?;
    let manifest: PackManifest = serde_json::from_slice(manifest_raw)
        .map_err(|e| format!("manifest.json: {e}"))?;
    if manifest.format != "ffb-fw-pack" {
        return Err(format!("unknown pack format '{}'", manifest.format));
    }
    if manifest.version != 1 {
        return Err(format!("unsupported pack version {}", manifest.version));
    }

    let rim = files
        .get(&manifest.rim.file)
        .ok_or_else(|| format!("pack missing {}", manifest.rim.file))?
        .clone();
    let base = files
        .get(&manifest.base.file)
        .ok_or_else(|| format!("pack missing {}", manifest.base.file))?
        .clone();

    let rim_hash = sha256_hex(&rim);
    let base_hash = sha256_hex(&base);
    if !rim_hash.eq_ignore_ascii_case(&manifest.rim.sha256) {
        return Err(format!(
            "rim sha256 mismatch (got {rim_hash}, want {})",
            manifest.rim.sha256
        ));
    }
    if !base_hash.eq_ignore_ascii_case(&manifest.base.sha256) {
        return Err(format!(
            "base sha256 mismatch (got {base_hash}, want {})",
            manifest.base.sha256
        ));
    }
    if !manifest.base.file.to_ascii_lowercase().ends_with(".uf2") && !is_uf2(&base) {
        return Err("base image must be UF2 for BOOTSEL copy".into());
    }

    let release = manifest.release.clone().unwrap_or_else(|| "unknown".into());
    let fw_id = manifest
        .fw_id
        .or(manifest.base.fw_id)
        .or(manifest.rim.fw_id)
        .unwrap_or_else(|| release.clone());

    Ok(FirmwarePack {
        release,
        fw_id,
        base_uf2: base,
        rim_bin: if manifest.rim.file.to_ascii_lowercase().ends_with(".uf2") || is_uf2(&rim) {
            uf2_to_bin(&rim)?
        } else {
            rim
        },
    })
}

fn find_uf2_boot_drive() -> Option<std::path::PathBuf> {
    use std::path::PathBuf;

    // Linux: /proc/mounts is authoritative (USER/$HOME mounts vary).
    if cfg!(target_os = "linux") {
        if let Ok(mounts) = std::fs::read_to_string("/proc/mounts") {
            for line in mounts.lines() {
                let mut parts = line.split_whitespace();
                let _src = parts.next();
                let Some(mnt) = parts.next() else { continue };
                let mnt = mnt.replace("\\040", " ");
                let p = PathBuf::from(&mnt);
                let name = p.file_name().and_then(|n| n.to_str()).unwrap_or("");
                if name.eq_ignore_ascii_case("RPI-RP2") || p.join("INFO_UF2.TXT").is_file() {
                    return Some(p);
                }
            }
        }
    }

    let mut roots: Vec<PathBuf> = Vec::new();

    if cfg!(target_os = "macos") {
        roots.push(PathBuf::from("/Volumes"));
    } else if cfg!(target_os = "windows") {
        for c in b'D'..=b'Z' {
            roots.push(PathBuf::from(format!("{}:\\", c as char)));
        }
    } else {
        if let Ok(user) = std::env::var("USER") {
            roots.push(PathBuf::from(format!("/media/{user}")));
            roots.push(PathBuf::from(format!("/run/media/{user}")));
        }
        if let Ok(home) = std::env::var("HOME") {
            // Some desktops mount under ~/media
            roots.push(PathBuf::from(home).join("media"));
        }
        roots.push(PathBuf::from("/mnt"));
        roots.push(PathBuf::from("/media"));
        roots.push(PathBuf::from("/run/media"));
    }

    for root in roots {
        if cfg!(target_os = "windows") {
            let info = root.join("INFO_UF2.TXT");
            if info.is_file() {
                return Some(root);
            }
            continue;
        }
        let Ok(entries) = std::fs::read_dir(&root) else {
            continue;
        };
        for ent in entries.flatten() {
            let p = ent.path();
            if !p.is_dir() {
                continue;
            }
            if p.join("INFO_UF2.TXT").is_file() {
                return Some(p);
            }
            // Nested: /run/media/user/RPI-RP2 when scanning /run/media
            if let Ok(sub) = std::fs::read_dir(&p) {
                for s in sub.flatten() {
                    let sp = s.path();
                    if sp.join("INFO_UF2.TXT").is_file() {
                        return Some(sp);
                    }
                }
            }
        }
    }
    None
}

fn write_uf2_to_drive(drive: &std::path::Path, uf2: &[u8]) -> Result<String, String> {
    use std::io::Write;
    let dest = drive.join("firmware.uf2");
    let mut f = std::fs::File::create(&dest).map_err(|e| format!("create {}: {e}", dest.display()))?;
    f.write_all(uf2)
        .map_err(|e| format!("write {}: {e}", dest.display()))?;
    let _ = f.sync_all();
    drop(f);
    #[cfg(unix)]
    {
        let _ = std::process::Command::new("sync").status();
    }
    Ok(format!("wrote {} ({} bytes)", dest.display(), uf2.len()))
}

/// Linux UF2: after a successful program the Pico resets and the mount vanishes;
/// a mid-write I/O error often still means success if the drive is gone.
fn copy_uf2_to_bootsel(uf2: &[u8], timeout: Duration) -> Result<String, String> {
    let deadline = Instant::now() + timeout;
    let mut last_err = "waiting for RPI-RP2 (BOOTSEL) drive…".to_string();
    while Instant::now() < deadline {
        if let Some(drive) = find_uf2_boot_drive() {
            match write_uf2_to_drive(&drive, uf2) {
                Ok(msg) => return Ok(msg),
                Err(e) => {
                    // Drive disappeared mid-write → usually flashed OK
                    std::thread::sleep(Duration::from_millis(300));
                    if find_uf2_boot_drive().is_none() {
                        return Ok(format!(
                            "UF2 accepted (drive unmounted after write; {e})"
                        ));
                    }
                    last_err = e;
                }
            }
        }
        std::thread::sleep(Duration::from_millis(300));
    }
    Err(format!(
        "timeout waiting for BOOTSEL drive ({last_err}). Hold BOOTSEL on base Pico, \
         then retry; or copy base.uf2 from the pack manually onto RPI-RP2."
    ))
}

fn try_picotool_load(uf2: &[u8]) -> Result<String, String> {
    let picotool = which_picotool().ok_or_else(|| "picotool not found in PATH".to_string())?;
    let tmp = std::env::temp_dir().join(format!(
        "ffb-base-{}.uf2",
        std::process::id()
    ));
    std::fs::write(&tmp, uf2).map_err(|e| format!("temp uf2: {e}"))?;
    let out = std::process::Command::new(&picotool)
        .args(["load", "-f", "-x"])
        .arg(&tmp)
        .output()
        .map_err(|e| format!("picotool: {e}"))?;
    let _ = std::fs::remove_file(&tmp);
    let stderr = String::from_utf8_lossy(&out.stderr);
    let stdout = String::from_utf8_lossy(&out.stdout);
    if out.status.success() {
        Ok(format!(
            "picotool load ok ({})",
            stdout.lines().last().unwrap_or("ok").trim()
        ))
    } else {
        Err(format!(
            "picotool failed: {} {}",
            stdout.trim(),
            stderr.trim()
        ))
    }
}

fn which_picotool() -> Option<std::path::PathBuf> {
    let path = std::env::var_os("PATH")?;
    for dir in std::env::split_paths(&path) {
        let cand = dir.join("picotool");
        if cand.is_file() {
            return Some(cand);
        }
    }
    // PlatformIO bundled tool
    if let Ok(home) = std::env::var("HOME") {
        let glob_root = std::path::PathBuf::from(home).join(".platformio/packages");
        if let Ok(walker) = std::fs::read_dir(glob_root) {
            for ent in walker.flatten() {
                let name = ent.file_name().to_string_lossy().to_string();
                if name.starts_with("tool-picotool") {
                    let bin = ent.path().join("picotool");
                    if bin.is_file() {
                        return Some(bin);
                    }
                }
            }
        }
    }
    None
}

fn flash_base_uf2(uf2: &[u8]) -> Result<String, String> {
    // Soft :bootsel → RPI-RP2 mount can take a few seconds; picotool -f works
    // even if soft reboot never happened (forces BOOTSEL over USB).
    if let Ok(msg) = copy_uf2_to_bootsel(uf2, Duration::from_secs(12)) {
        return Ok(msg);
    }
    match try_picotool_load(uf2) {
        Ok(msg) => Ok(msg),
        Err(pt_err) => copy_uf2_to_bootsel(uf2, Duration::from_secs(50)).map_err(|mount_err| {
            format!("{mount_err} | {pt_err}")
        }),
    }
}

fn io_thread(
    app: AppHandle,
    mut port: Box<dyn SerialPort>,
    cmd_rx: mpsc::Receiver<IoCmd>,
    flash_busy: Arc<AtomicBool>,
) {
    let mut pending: Option<PendingReply> = None;
    let mut pending_deadline: Option<Instant> = None;
    let mut buf: Vec<u8> = Vec::new();
    let mut chunk = [0u8; 512];

    loop {
        match cmd_rx.try_recv() {
            Ok(IoCmd::Shutdown) => break,
            Ok(IoCmd::Flash { image, reply }) => {
                flash_busy.store(true, Ordering::SeqCst);
                // Drain any pending text reply.
                if let Some(done) = pending.take() {
                    let _ = done.tx.send(Err("interrupted by flash".into()));
                }
                pending_deadline = None;
                let res = run_flash(&app, &mut port, &mut buf, &mut chunk, &image);
                if res.is_err() {
                    // Enter was sent; rim may still be in soft updater (orange LED).
                    recover_rim_after_ota_fail(&mut port);
                }
                let _ = reply.send(res);
                // Re-enable telemetry best-effort
                let _ = port.write_all(b":log 1\n");
                let _ = port.flush();
                flash_busy.store(false, Ordering::SeqCst);
            }
            Ok(IoCmd::WriteBytes { data }) => {
                if !flash_busy.load(Ordering::SeqCst) {
                    let _ = port.write_all(&data);
                }
            }
            Ok(IoCmd::Write { line, reply }) => {
                let mut msg = line;
                if !msg.ends_with('\n') {
                    msg.push('\n');
                }
                if let Err(e) = port.write_all(msg.as_bytes()).and_then(|_| port.flush()) {
                    if let Some((_, tx)) = reply {
                        let _ = tx.send(Err(format!("write: {e}")));
                    }
                } else if let Some((kind, tx)) = reply {
                    pending = Some(PendingReply {
                        kind,
                        buf: Vec::new(),
                        saw_dump: false,
                        tx,
                    });
                    pending_deadline = Some(Instant::now() + Duration::from_millis(2500));
                }
            }
            Err(mpsc::TryRecvError::Empty) => {}
            Err(mpsc::TryRecvError::Disconnected) => break,
        }

        if let Some(deadline) = pending_deadline {
            if Instant::now() > deadline {
                if let Some(done) = pending.take() {
                    pending_deadline = None;
                    let _ = done.tx.send(Err("timeout waiting for device reply".into()));
                }
            }
        }

        match port.read(&mut chunk) {
            Ok(n) if n > 0 => {
                buf.extend_from_slice(&chunk[..n]);
                // Drop leading binary noise for text mode (keep if looks like frame)
                while let Some(pos) = buf.iter().position(|&b| b == b'\n') {
                    // If buffer starts with binary sync before newline, don't treat as text
                    if !buf.is_empty() && buf[0] == 0xAA {
                        break;
                    }
                    let raw = buf.drain(..=pos).collect::<Vec<u8>>();
                    let line = String::from_utf8_lossy(&raw)
                        .trim_end_matches(['\r', '\n'])
                        .to_string();
                    if line.is_empty() {
                        continue;
                    }
                    // Skip lines that are mostly non-printable
                    if line.bytes().filter(|b| *b < 9 || (*b > 13 && *b < 32)).count() > 0 {
                        continue;
                    }
                    let _ = app.emit("serial-line", &line);
                    if let Some(t) = parse_telemetry(&line) {
                        let _ = app.emit("telemetry", &t);
                    }
                    if feed_pending(&mut pending, &line) {
                        pending_deadline = None;
                    }
                }
                // Opportunistically discard orphan sync if stuck without completing frame
                if buf.len() > 4096 {
                    buf.drain(..buf.len() - 512);
                }
            }
            Ok(_) | Err(_) => {
                std::thread::sleep(Duration::from_millis(5));
            }
        }
    }
}

fn with_cmd_tx<F, T>(state: &AppState, f: F) -> Result<T, String>
where
    F: FnOnce(&mpsc::Sender<IoCmd>) -> Result<T, String>,
{
    let guard = state.cmd_tx.lock();
    let tx = guard.as_ref().ok_or_else(|| "not connected".to_string())?;
    f(tx)
}

fn request(
    tx: &mpsc::Sender<IoCmd>,
    line: &str,
    kind: ReplyKind,
) -> Result<Vec<String>, String> {
    let (rtx, rrx) = mpsc::channel();
    tx.send(IoCmd::Write {
        line: line.to_string(),
        reply: Some((kind, rtx)),
    })
    .map_err(|_| "io thread gone".to_string())?;
    rrx.recv_timeout(Duration::from_secs(6))
        .map_err(|_| "timeout waiting for device reply".to_string())?
}

fn request_ok_line(tx: &mpsc::Sender<IoCmd>, line: &str) -> Result<String, String> {
    let lines = request(tx, line, ReplyKind::Simple)?;
    lines
        .last()
        .cloned()
        .ok_or_else(|| "no reply".to_string())
}

fn request_dump_map(tx: &mpsc::Sender<IoCmd>, line: &str) -> Result<BTreeMap<String, String>, String> {
    let lines = request(tx, line, ReplyKind::Dump)?;
    if let Some(err) = lines.iter().find(|l| l.starts_with("ERR")) {
        return Err(err.clone());
    }
    let map = parse_kv_map(&lines);
    if map.is_empty() {
        return Err("empty dump".into());
    }
    Ok(map)
}

#[tauri::command]
async fn connect(
    app: AppHandle,
    state: State<'_, Arc<AppState>>,
    path: String,
) -> Result<BTreeMap<String, String>, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        // Stop previous session.
        if let Some(old) = state.cmd_tx.lock().take() {
            let _ = old.send(IoCmd::Shutdown);
        }

        let port = open_port(&path)?;
        let (cmd_tx, cmd_rx) = mpsc::channel::<IoCmd>();
        let app2 = app.clone();
        let flash_busy = Arc::clone(&state.flash_busy);
        state.flash_busy.store(false, Ordering::SeqCst);
        std::thread::Builder::new()
            .name("ffb-serial".into())
            .spawn(move || io_thread(app2, port, cmd_rx, flash_busy))
            .map_err(|e| format!("spawn io: {e}"))?;

        *state.cmd_tx.lock() = Some(cmd_tx.clone());

        // Brief settle, then dump settings, then enable live telemetry.
        std::thread::sleep(Duration::from_millis(200));
        let _ = request_ok_line(&cmd_tx, ":log 0");
        let map = request_dump_map(&cmd_tx, ":dump").map_err(|e| {
            let _ = cmd_tx.send(IoCmd::Shutdown);
            *state.cmd_tx.lock() = None;
            e
        })?;
        if state.race.lock().enabled {
            let _ = request_ok_line(&cmd_tx, ":companion 1");
        } else {
            let _ = request_ok_line(&cmd_tx, ":log 1");
        }
        Ok(map)
    })
    .await
    .map_err(|e| format!("connect task: {e}"))?
}

#[tauri::command]
async fn disconnect(state: State<'_, Arc<AppState>>) -> Result<(), String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        // Keep race UDP listener running; inject resumes on next connect.
        if let Some(tx) = state.cmd_tx.lock().take() {
            let _ = tx.send(IoCmd::Shutdown);
        }
        Ok(())
    })
    .await
    .map_err(|e| format!("disconnect task: {e}"))?
}

#[tauri::command]
fn is_connected(state: State<'_, Arc<AppState>>) -> bool {
    state.cmd_tx.lock().is_some()
}

#[tauri::command]
async fn dump_settings(
    state: State<'_, Arc<AppState>>,
) -> Result<BTreeMap<String, String>, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| request_dump_map(tx, ":dump"))
    })
    .await
    .map_err(|e| format!("dump task: {e}"))?
}

/// Run a CDC command that ends with an `OK dump` … `OK end` block (e.g. `:profile load N`).
#[tauri::command]
async fn run_dump_command(
    state: State<'_, Arc<AppState>>,
    line: String,
) -> Result<BTreeMap<String, String>, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| request_dump_map(tx, &line))
    })
    .await
    .map_err(|e| format!("dump cmd task: {e}"))?
}

#[tauri::command]
async fn set_setting(
    state: State<'_, Arc<AppState>>,
    key: String,
    value: f64,
) -> Result<String, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| {
            request_ok_line(tx, &format!(":set {key} {value}"))
        })
    })
    .await
    .map_err(|e| format!("set task: {e}"))?
}

#[tauri::command]
async fn set_setting_str(
    state: State<'_, Arc<AppState>>,
    key: String,
    value: String,
) -> Result<String, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| {
            request_ok_line(tx, &format!(":set {key} {value}"))
        })
    })
    .await
    .map_err(|e| format!("set str task: {e}"))?
}

#[tauri::command]
async fn save_settings(state: State<'_, Arc<AppState>>) -> Result<String, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| request_ok_line(tx, ":save"))
    })
    .await
    .map_err(|e| format!("save task: {e}"))?
}

#[tauri::command]
async fn load_settings(
    state: State<'_, Arc<AppState>>,
) -> Result<BTreeMap<String, String>, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| request_dump_map(tx, ":load"))
    })
    .await
    .map_err(|e| format!("load task: {e}"))?
}

#[tauri::command]
async fn reset_defaults(
    state: State<'_, Arc<AppState>>,
) -> Result<BTreeMap<String, String>, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| request_dump_map(tx, ":defaults"))
    })
    .await
    .map_err(|e| format!("defaults task: {e}"))?
}

#[tauri::command]
async fn send_raw(state: State<'_, Arc<AppState>>, line: String) -> Result<(), String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| {
            tx.send(IoCmd::Write {
                line,
                reply: None,
            })
            .map_err(|_| "io thread gone".to_string())
        })
    })
    .await
    .map_err(|e| format!("send task: {e}"))?
}

#[tauri::command]
async fn flash_rim(
    state: State<'_, Arc<AppState>>,
    data: Vec<u8>,
    filename: String,
) -> Result<String, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        let image = if filename.to_ascii_lowercase().ends_with(".uf2") || is_uf2(&data) {
            uf2_to_bin(&data)?
        } else {
            data
        };
        let nbytes = image.len();
        with_cmd_tx(&state, |tx| {
            let (rtx, rrx) = mpsc::channel();
            tx.send(IoCmd::Flash {
                image,
                reply: rtx,
            })
            .map_err(|_| "io thread gone".to_string())?;
            rrx.recv_timeout(Duration::from_secs(180))
                .map_err(|_| "flash timeout".to_string())?
        })?;
        Ok(format!(
            "OK rim flash complete ({nbytes} bytes) — rim reboots; settings stay in rim EEPROM"
        ))
    })
    .await
    .map_err(|e| format!("flash task: {e}"))?
}

#[tauri::command]
async fn flash_firmware_pack(
    app: AppHandle,
    state: State<'_, Arc<AppState>>,
    data: Vec<u8>,
    filename: String,
) -> Result<String, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        let pack = parse_firmware_pack(&data)?;
        let rim_n = pack.rim_bin.len();
        let base_n = pack.base_uf2.len();

        let _ = app.emit(
            "flash-progress",
            serde_json::json!({
                "event": "pack",
                "phase": "rim",
                "release": pack.release,
                "rimBytes": rim_n,
                "baseBytes": base_n,
            }),
        );

        // 1) Rim OTA while base CDC is still alive
        with_cmd_tx(&state, |tx| {
            let (rtx, rrx) = mpsc::channel();
            tx.send(IoCmd::Flash {
                image: pack.rim_bin,
                reply: rtx,
            })
            .map_err(|_| "io thread gone".to_string())?;
            rrx.recv_timeout(Duration::from_secs(180))
                .map_err(|_| "rim flash timeout".to_string())?
        })?;

        let _ = app.emit(
            "flash-progress",
            serde_json::json!({ "event": "pack", "phase": "rim-done" }),
        );

        // Pause for rim soft-reboot + slot-0 apply (+ GUI :rim_reset after FwDone)
        // and let CDC leave binary/OTA quiet mode before :bootsel.
        std::thread::sleep(Duration::from_millis(4000));

        // 2) Reboot base into BOOTSEL — wait for OK so the command actually landed
        let _ = app.emit(
            "flash-progress",
            serde_json::json!({ "event": "pack", "phase": "base-bootsel" }),
        );
        let bootsel_reply = with_cmd_tx(&state, |tx| request_ok_line(tx, ":bootsel"));
        match &bootsel_reply {
            Ok(s) if s.starts_with("OK") => {}
            Ok(s) => {
                return Err(format!(
                    "rim OTA ok, but base :bootsel replied '{s}' — hold BOOTSEL and copy base.uf2 manually"
                ));
            }
            Err(e) => {
                // Device may reboot before OK is fully observed — continue and hunt for RPI-RP2
                let _ = app.emit(
                    "flash-progress",
                    serde_json::json!({
                        "event": "pack",
                        "phase": "base-bootsel",
                        "note": format!("no OK ({e}); waiting for RPI-RP2 anyway"),
                    }),
                );
            }
        }
        // Drop serial — device is (or will be) gone
        if let Some(tx) = state.cmd_tx.lock().take() {
            let _ = tx.send(IoCmd::Shutdown);
        }
        std::thread::sleep(Duration::from_millis(1500));

        let _ = app.emit(
            "flash-progress",
            serde_json::json!({ "event": "pack", "phase": "base-uf2" }),
        );

        // 3) Copy base UF2 (mount) or picotool -f
        let copied = flash_base_uf2(&pack.base_uf2).map_err(|e| {
            format!(
                "rim OTA ok ({} bytes), but base UF2 failed: {e}. \
                 Unzip the pack and copy base.uf2 onto RPI-RP2 (hold BOOTSEL).",
                rim_n
            )
        })?;

        let _ = save_last_good_pack(&app, &data, &filename, &pack.fw_id);

        let _ = app.emit(
            "flash-progress",
            serde_json::json!({ "event": "pack", "phase": "done" }),
        );

        Ok(format!(
            "OK pack {} (fw_id={}) — rim OTA {} bytes, base UF2 {} — reconnect, Dump, compare base_fw/rim_fw",
            pack.release, pack.fw_id, rim_n, copied
        ))
    })
    .await
    .map_err(|e| format!("pack flash task: {e}"))?
}

fn last_good_pack_dir(app: &AppHandle) -> Result<std::path::PathBuf, String> {
    let dir = app
        .path()
        .app_data_dir()
        .map_err(|e| format!("app data dir: {e}"))?
        .join("last-good-pack");
    std::fs::create_dir_all(&dir).map_err(|e| format!("mkdir: {e}"))?;
    Ok(dir)
}

fn save_last_good_pack(app: &AppHandle, data: &[u8], filename: &str, fw_id: &str) -> Result<(), String> {
    let dir = last_good_pack_dir(app)?;
    let zip_path = dir.join("ffb-firmware-last-good.zip");
    let meta_path = dir.join("meta.txt");
    std::fs::write(&zip_path, data).map_err(|e| format!("write pack: {e}"))?;
    let meta = format!("filename={filename}\nfw_id={fw_id}\n");
    std::fs::write(&meta_path, meta).map_err(|e| format!("write meta: {e}"))?;
    Ok(())
}

#[tauri::command]
async fn save_last_good_pack_cmd(
    app: AppHandle,
    data: Vec<u8>,
    filename: String,
    fw_id: String,
) -> Result<String, String> {
    tauri::async_runtime::spawn_blocking(move || {
        save_last_good_pack(&app, &data, &filename, &fw_id)?;
        Ok(format!("OK saved last-good ({fw_id})"))
    })
    .await
    .map_err(|e| format!("save pack task: {e}"))?
}

#[tauri::command]
async fn load_last_good_pack(app: AppHandle) -> Result<(Vec<u8>, String, String), String> {
    tauri::async_runtime::spawn_blocking(move || {
        let dir = last_good_pack_dir(&app)?;
        let zip_path = dir.join("ffb-firmware-last-good.zip");
        let meta_path = dir.join("meta.txt");
        if !zip_path.exists() {
            return Err("no last-good pack saved yet".into());
        }
        let data = std::fs::read(&zip_path).map_err(|e| format!("read pack: {e}"))?;
        let mut filename = "ffb-firmware-last-good.zip".to_string();
        let mut fw_id = String::new();
        if let Ok(meta) = std::fs::read_to_string(&meta_path) {
            for line in meta.lines() {
                if let Some(v) = line.strip_prefix("filename=") {
                    filename = v.to_string();
                } else if let Some(v) = line.strip_prefix("fw_id=") {
                    fw_id = v.to_string();
                }
            }
        }
        Ok((data, filename, fw_id))
    })
    .await
    .map_err(|e| format!("load pack task: {e}"))?
}

#[tauri::command]
async fn last_good_pack_info(app: AppHandle) -> Result<Option<(String, String)>, String> {
    tauri::async_runtime::spawn_blocking(move || {
        let dir = last_good_pack_dir(&app)?;
        let zip_path = dir.join("ffb-firmware-last-good.zip");
        let meta_path = dir.join("meta.txt");
        if !zip_path.exists() {
            return Ok(None);
        }
        let mut filename = "ffb-firmware-last-good.zip".to_string();
        let mut fw_id = String::new();
        if let Ok(meta) = std::fs::read_to_string(&meta_path) {
            for line in meta.lines() {
                if let Some(v) = line.strip_prefix("filename=") {
                    filename = v.to_string();
                } else if let Some(v) = line.strip_prefix("fw_id=") {
                    fw_id = v.to_string();
                }
            }
        }
        Ok(Some((filename, fw_id)))
    })
    .await
    .map_err(|e| format!("pack info task: {e}"))?
}

#[tauri::command]
async fn set_telemetry_log(
    state: State<'_, Arc<AppState>>,
    enabled: bool,
) -> Result<String, String> {
    let state = state.inner().clone();
    if state.race.lock().enabled && enabled {
        return Err("disable Race mode before enabling live diagnostic telemetry".into());
    }
    let cmd = if enabled { ":log 1" } else { ":log 0" };
    tauri::async_runtime::spawn_blocking(move || {
        with_cmd_tx(&state, |tx| request_ok_line(tx, cmd))
    })
    .await
    .map_err(|e| format!("log task: {e}"))?
}

fn stop_race_inner(state: &AppState) {
    let mut race = state.race.lock();
    if let Some(tx) = race.stop_tx.take() {
        let _ = tx.send(());
    }
    let was = race.enabled;
    race.enabled = false;
    drop(race);
    if was {
        if let Ok(tx) = with_cmd_tx(state, |tx| Ok(tx.clone())) {
            let _ = request_ok_line(&tx, ":companion 0");
            let _ = request_ok_line(&tx, ":log 1");
        }
    }
}

fn start_race_worker(
    app: AppHandle,
    state: Arc<AppState>,
    udp_port: u16,
) -> Result<(), String> {
    {
        let race = state.race.lock();
        if race.enabled {
            return Err("race mode already running".into());
        }
    }

    // Quiet diagnostic spam when hardware is already connected.
    let _ = with_cmd_tx(&state, |tx| {
        let _ = request_ok_line(tx, ":companion 1");
        Ok(())
    });

    let (stop_tx, stop_rx) = mpsc::channel::<()>();
    {
        let mut race = state.race.lock();
        race.udp_port = udp_port;
        race.enabled = true;
        race.stop_tx.replace(stop_tx);
        *race.state.lock() = LmuState::default();
    }

    let flash_busy = Arc::clone(&state.flash_busy);
    let state_for_thread = Arc::clone(&state);

    std::thread::Builder::new()
        .name("ffb-lmu-race".into())
        .spawn(move || {
            let bind = format!("0.0.0.0:{udp_port}");
            let sock = match UdpSocket::bind(&bind) {
                Ok(s) => s,
                Err(e) => {
                    let msg = format!("UDP bind {bind}: {e}");
                    state_for_thread.race.lock().state.lock().last_error = msg.clone();
                    {
                        let mut race = state_for_thread.race.lock();
                        race.enabled = false;
                        race.stop_tx = None;
                    }
                    if let Some(tx) = state_for_thread.cmd_tx.lock().as_ref() {
                        let _ = request_ok_line(tx, ":companion 0");
                        let _ = request_ok_line(tx, ":log 1");
                    }
                    let _ = app.emit(
                        "race-status",
                        &RaceStatus {
                            enabled: false,
                            udp_port,
                            last_error: msg,
                            connected: state_for_thread.cmd_tx.lock().is_some(),
                            ..Default::default()
                        },
                    );
                    return;
                }
            };
            let _ = sock.set_read_timeout(Some(Duration::from_millis(50)));
            let mut buf = vec![0u8; 65535];
            let mut last_emit = Instant::now() - Duration::from_secs(1);
            let mut last_status = Instant::now() - Duration::from_secs(1);
            let mut telem_window = Instant::now();
            let mut telem_count = 0u32;
            let mut scoring_count = 0u32;
            let mut telem_hz = 0.0f64;
            let mut scoring_hz = 0.0f64;
            let lmu = Arc::clone(&state_for_thread.race.lock().state);

            loop {
                if stop_rx.try_recv().is_ok() {
                    break;
                }

                match sock.recv_from(&mut buf) {
                    Ok((n, _)) if n > 0 => {
                        let text = String::from_utf8_lossy(&buf[..n]);
                        for part in text.split('\n') {
                            let part = part.trim();
                            if part.is_empty() {
                                continue;
                            }
                            let mut g = lmu.lock();
                            let before_t = g.telem_packets;
                            let before_s = g.scoring_packets;
                            g.ingest_json(part);
                            if g.telem_packets != before_t {
                                telem_count += 1;
                            }
                            if g.scoring_packets != before_s {
                                scoring_count += 1;
                            }
                        }
                    }
                    Ok(_) => {}
                    Err(e)
                        if e.kind() == std::io::ErrorKind::WouldBlock
                            || e.kind() == std::io::ErrorKind::TimedOut => {}
                    Err(e) => {
                        lmu.lock().last_error = format!("UDP recv: {e}");
                    }
                }

                if telem_window.elapsed() >= Duration::from_secs(1) {
                    let dt = telem_window.elapsed().as_secs_f64().max(0.001);
                    telem_hz = telem_count as f64 / dt;
                    scoring_hz = scoring_count as f64 / dt;
                    telem_count = 0;
                    scoring_count = 0;
                    telem_window = Instant::now();
                }

                // Inject only while CDC is open (reconnect mid-race is fine).
                if last_emit.elapsed() >= Duration::from_millis(20)
                    && !flash_busy.load(Ordering::SeqCst)
                {
                    if let Some(tx) = state_for_thread.cmd_tx.lock().as_ref() {
                        let mapped = lmu.lock().mapped.clone();
                        let frame = build_frame(MSG_TELEMETRY, &mapped.to_bytes());
                        let _ = tx.send(IoCmd::WriteBytes { data: frame });
                        last_emit = Instant::now();
                    }
                }

                if last_status.elapsed() >= Duration::from_millis(500) {
                    let snap = lmu.lock().clone();
                    let connected = state_for_thread.cmd_tx.lock().is_some();
                    let st = RaceStatus {
                        enabled: true,
                        udp_port,
                        telem_hz,
                        scoring_hz,
                        rpm: snap.mapped.rpm,
                        gear: snap.mapped.gear,
                        speed_kph: snap.mapped.speed_kph_x10 as f64 / 10.0,
                        fuel_pct: snap.mapped.fuel_pct_x10 as f64 / 10.0,
                        flags: snap.mapped.flags,
                        last_error: snap.last_error.clone(),
                        connected,
                    };
                    let _ = app.emit("race-status", &st);
                    if let Some(tray) = app.tray_by_id("main") {
                        let tip = if connected {
                            format!(
                                "rp2040-ffb · race · {} rpm · G{}",
                                st.rpm, st.gear
                            )
                        } else {
                            format!(
                                "rp2040-ffb · race (listen) · {} rpm · G{}",
                                st.rpm, st.gear
                            )
                        };
                        let _ = tray.set_tooltip(Some(tip));
                    }
                    last_status = Instant::now();
                }
            }

            if let Some(tray) = app.tray_by_id("main") {
                let _ = tray.set_tooltip(Some("rp2040-ffb"));
            }
        })
        .map_err(|e| {
            stop_race_inner(&state);
            format!("spawn race: {e}")
        })?;

    Ok(())
}

#[tauri::command]
async fn race_start(
    app: AppHandle,
    state: State<'_, Arc<AppState>>,
    udp_port: Option<u16>,
) -> Result<RaceStatus, String> {
    let state = state.inner().clone();
    let port = udp_port.unwrap_or(5000).max(1);
    tauri::async_runtime::spawn_blocking(move || {
        start_race_worker(app, state.clone(), port)?;
        Ok(race_status_snapshot(&state))
    })
    .await
    .map_err(|e| format!("race start: {e}"))?
}

#[tauri::command]
async fn race_stop(state: State<'_, Arc<AppState>>) -> Result<RaceStatus, String> {
    let state = state.inner().clone();
    tauri::async_runtime::spawn_blocking(move || {
        stop_race_inner(&state);
        Ok(race_status_snapshot(&state))
    })
    .await
    .map_err(|e| format!("race stop: {e}"))?
}

#[tauri::command]
fn race_status(state: State<'_, Arc<AppState>>) -> RaceStatus {
    race_status_snapshot(&state)
}

#[tauri::command]
fn race_set_udp_port(state: State<'_, Arc<AppState>>, udp_port: u16) -> Result<u16, String> {
    let mut race = state.race.lock();
    if race.enabled {
        return Err("stop race mode before changing UDP port".into());
    }
    race.udp_port = udp_port.max(1);
    Ok(race.udp_port)
}

fn race_status_snapshot(state: &AppState) -> RaceStatus {
    let race = state.race.lock();
    let snap = race.state.lock();
    RaceStatus {
        enabled: race.enabled,
        udp_port: race.udp_port,
        telem_hz: 0.0,
        scoring_hz: 0.0,
        rpm: snap.mapped.rpm,
        gear: snap.mapped.gear,
        speed_kph: snap.mapped.speed_kph_x10 as f64 / 10.0,
        fuel_pct: snap.mapped.fuel_pct_x10 as f64 / 10.0,
        flags: snap.mapped.flags,
        last_error: snap.last_error.clone(),
        connected: state.cmd_tx.lock().is_some(),
    }
}

fn setup_tray(app: &AppHandle) -> tauri::Result<()> {
    let show = MenuItem::with_id(app, "show", "Show", true, None::<&str>)?;
    let quit = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
    let menu = Menu::with_items(app, &[&show, &quit])?;

    let icon = app
        .default_window_icon()
        .cloned()
        .expect("bundle icon from assets/logo");

    let _tray = TrayIconBuilder::with_id("main")
        .tooltip("rp2040-ffb")
        .icon(icon)
        .menu(&menu)
        .show_menu_on_left_click(false)
        .on_menu_event(|app, event| match event.id.as_ref() {
            "show" => {
                if let Some(w) = app.get_webview_window("main") {
                    let _ = w.show();
                    let _ = w.unminimize();
                    let _ = w.set_focus();
                }
            }
            "quit" => {
                app.exit(0);
            }
            _ => {}
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click {
                button: MouseButton::Left,
                button_state: MouseButtonState::Up,
                ..
            } = event
            {
                let app = tray.app_handle();
                if let Some(w) = app.get_webview_window("main") {
                    let _ = w.show();
                    let _ = w.unminimize();
                    let _ = w.set_focus();
                }
            }
        })
        .build(app)?;
    Ok(())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let state = Arc::new(AppState {
        cmd_tx: Mutex::new(None),
        race: Mutex::new(RaceControl::default()),
        flash_busy: Arc::new(AtomicBool::new(false)),
    });

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .manage(state)
        .setup(|app| {
            setup_tray(app.handle())?;
            Ok(())
        })
        .on_window_event(|window, event| {
            if let WindowEvent::CloseRequested { api, .. } = event {
                let race_on = window
                    .state::<Arc<AppState>>()
                    .race
                    .lock()
                    .enabled;
                if race_on {
                    let _ = window.hide();
                    api.prevent_close();
                }
            }
        })
        .invoke_handler(tauri::generate_handler![
            list_ports,
            connect,
            disconnect,
            is_connected,
            dump_settings,
            run_dump_command,
            set_setting,
            set_setting_str,
            save_settings,
            load_settings,
            reset_defaults,
            send_raw,
            flash_rim,
            flash_firmware_pack,
            save_last_good_pack_cmd,
            load_last_good_pack,
            last_good_pack_info,
            set_telemetry_log,
            race_start,
            race_stop,
            race_status,
            race_set_udp_port,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
