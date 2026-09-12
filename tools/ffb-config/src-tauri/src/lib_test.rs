// Unit tests for Tauri backend
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_crc16_ccitt() {
        let data = vec![0x01, 0x02, 0x03, 0x04];
        let crc1 = crc16_ccitt(&data);
        let crc2 = crc16_ccitt(&data);
        assert_eq!(crc1, crc2, "CRC16 should be deterministic");
        assert_ne!(crc1, 0xFFFF, "CRC16 should not be initial value");
    }
    
    #[test]
    fn test_crc16_changes_with_data() {
        let data1 = vec![0x01, 0x02, 0x03];
        let data2 = vec![0x01, 0x02, 0x04];
        let crc1 = crc16_ccitt(&data1);
        let crc2 = crc16_ccitt(&data2);
        assert_ne!(crc1, crc2, "Different data should produce different CRC");
    }
    
    #[test]
    fn test_crc32_ieee() {
        let data = b"123456789";
        let crc = crc32_ieee(data);
        assert_eq!(crc, 0xCBF43926, "CRC32 should match known test vector");
    }
    
    #[test]
    fn test_crc32_deterministic() {
        let data = vec![0xDE, 0xAD, 0xBE, 0xEF];
        let crc1 = crc32_ieee(&data);
        let crc2 = crc32_ieee(&data);
        assert_eq!(crc1, crc2, "CRC32 should be deterministic");
    }
    
    #[test]
    fn test_is_uf2_valid() {
        let mut data = vec![0u8; 512];
        data[0] = 0x55;
        data[1] = 0x46;
        data[2] = 0x32;
        data[3] = 0x0A;
        assert!(is_uf2(&data), "Should recognize UF2 magic");
    }
    
    #[test]
    fn test_is_uf2_invalid() {
        let data = vec![0u8; 512];
        assert!(!is_uf2(&data), "Should reject non-UF2 data");
    }
    
    #[test]
    fn test_build_frame() {
        let payload = vec![0xAB, 0xCD];
        let frame = build_frame(0x10, &payload);
        
        assert_eq!(frame[0], 0xAA, "Sync byte 1");
        assert_eq!(frame[1], 0x55, "Sync byte 2");
        assert_eq!(frame[2], 1, "Version");
        assert_eq!(frame[3], 0x10, "Message type");
        assert_eq!(frame[4], 2, "Payload length");
        assert_eq!(frame[5], 0xAB, "Payload byte 1");
        assert_eq!(frame[6], 0xCD, "Payload byte 2");
        
        // Frame should have: 2 sync + 3 header + 2 payload + 2 CRC = 9 bytes
        assert_eq!(frame.len(), 9);
    }
    
    #[test]
    fn test_try_pop_frame_valid() {
        let payload = vec![0x12, 0x34];
        let frame = build_frame(0x20, &payload);
        let mut buf = frame.clone();
        
        let result = try_pop_frame(&mut buf);
        assert!(result.is_some(), "Should parse valid frame");
        
        let (msg_type, parsed_payload) = result.unwrap();
        assert_eq!(msg_type, 0x20);
        assert_eq!(parsed_payload, payload);
        assert_eq!(buf.len(), 0, "Buffer should be empty after pop");
    }
    
    #[test]
    fn test_try_pop_frame_bad_crc() {
        let payload = vec![0x12, 0x34];
        let mut frame = build_frame(0x20, &payload);
        
        // Corrupt CRC
        let len = frame.len();
        frame[len - 1] ^= 0xFF;
        
        let result = try_pop_frame(&mut frame);
        assert!(result.is_none(), "Should reject frame with bad CRC");
    }
    
    #[test]
    fn test_try_pop_frame_incomplete() {
        let mut buf = vec![0xAA, 0x55, 0x01];
        let result = try_pop_frame(&mut buf);
        assert!(result.is_none(), "Should reject incomplete frame");
    }
    
    #[test]
    fn test_port_is_likely_pico() {
        // Mock port info with Raspberry Pi VID
        let info = serialport::SerialPortInfo {
            port_name: "/dev/ttyACM0".to_string(),
            port_type: serialport::SerialPortType::UsbPort(serialport::UsbPortInfo {
                vid: 0x2E8A,
                pid: 0x0001,
                serial_number: None,
                manufacturer: Some("Raspberry Pi".to_string()),
                product: Some("Pico".to_string()),
            }),
        };
        
        assert!(port_is_likely_pico(&info), "Should recognize Pico by VID");
    }
    
    #[test]
    fn test_parse_kv_map() {
        let lines = vec![
            "OK dump".to_string(),
            "duty_cap=0.350000".to_string(),
            "spring_k=0.004000".to_string(),
            "hid_range=900.00".to_string(),
            "OK end".to_string(),
        ];
        
        let map = parse_kv_map(&lines);
        assert_eq!(map.get("duty_cap"), Some(&"0.350000".to_string()));
        assert_eq!(map.get("spring_k"), Some(&"0.004000".to_string()));
        assert_eq!(map.get("hid_range"), Some(&"900.00".to_string()));
        assert!(!map.contains_key("OK"), "Should skip OK lines");
    }
    
    #[test]
    fn test_parse_telemetry() {
        let line = "T axle=45.2 hidX=100 sens=450.0 hall=1 idx=0 rim=1 motors=0 ffb=Spring torq=0.123";
        let telem = parse_telemetry(line);
        
        assert!(telem.is_some(), "Should parse valid telemetry");
        let t = telem.unwrap();
        assert_eq!(t.axle, 45.2);
        assert_eq!(t.hid_x, 100);
        assert_eq!(t.sens, 450.0);
        assert!(t.hall);
        assert!(!t.idx);
        assert!(t.rim);
        assert!(!t.motors);
        assert_eq!(t.ffb, "Spring");
    }
    
    #[test]
    fn test_parse_telemetry_invalid() {
        let line = "invalid telemetry line";
        let telem = parse_telemetry(line);
        assert!(telem.is_none(), "Should reject invalid telemetry");
    }
}

// Helper functions to make tests compile (extract from lib.rs)
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

fn is_uf2(data: &[u8]) -> bool {
    data.len() >= 8 && data[0..4] == [0x55, 0x46, 0x32, 0x0A]
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
        buf.remove(0);
        return None;
    }
    buf.drain(..total);
    let msg_type = body[1];
    let payload = body[3..].to_vec();
    Some((msg_type, payload))
}

fn parse_kv_map(lines: &[String]) -> std::collections::BTreeMap<String, String> {
    let mut map = std::collections::BTreeMap::new();
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

#[derive(Debug, PartialEq)]
struct Telemetry {
    axle: f64,
    hid_x: i32,
    sens: f64,
    hall: bool,
    idx: bool,
    rim: bool,
    motors: bool,
    ffb: String,
    torq: f64,
}

fn parse_telemetry(line: &str) -> Option<Telemetry> {
    let body = line.strip_prefix("T ")?;
    let mut map = std::collections::BTreeMap::new();
    for part in body.split_whitespace() {
        if let Some((k, v)) = part.split_once('=') {
            map.insert(k, v);
        }
    }
    Some(Telemetry {
        axle: map.get("axle")?.parse().ok()?,
        hid_x: map.get("hidX")?.parse().ok()?,
        sens: map.get("sens")?.parse().ok()?,
        hall: map.get("hall").map(|v| v != "0").unwrap_or(false),
        idx: map.get("idx").map(|v| v != "0").unwrap_or(false),
        rim: map.get("rim").map(|v| v != "0").unwrap_or(false),
        motors: map.get("motors").map(|v| v != "0").unwrap_or(false),
        ffb: map.get("ffb").unwrap_or(&"?").to_string(),
        torq: map.get("torq")?.parse().ok()?,
    })
}

fn port_is_likely_pico(port: &serialport::SerialPortInfo) -> bool {
    match &port.port_type {
        serialport::SerialPortType::UsbPort(usb) => {
            usb.vid == 0x2E8A
        }
        _ => false,
    }
}
