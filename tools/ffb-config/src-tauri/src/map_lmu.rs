//! Map Le Mans Ultimate Telemetry Socket JSON (UDP) → TelemetryPayload bytes.
//! Plugin: https://community.lemansultimate.com/index.php?threads/telemetry-socket-–-json-telemetry-plugin.8229/

use serde_json::Value;

pub const MSG_TELEMETRY: u8 = 0x20;
pub const TEL_PAYLOAD_SIZE: usize = 34;
pub const GAP_NA: i16 = -32768;

pub const TEL_YELLOW: u8 = 1 << 0;
pub const TEL_BLUE: u8 = 1 << 1;
pub const TEL_RED: u8 = 1 << 4;
pub const TEL_PIT: u8 = 1 << 5;

#[derive(Debug, Clone)]
pub struct MappedTelemetry {
    pub rpm: u16,
    pub speed_kph_x10: i16,
    pub gear: i8,
    pub flags: u8,
    pub fuel_pct_x10: u16,
    pub lap_time_ms: u16,
    pub tyre_temp_c: [u8; 4],
    pub tyre_press_psi: [u8; 4],
    pub brake_temp_c: [u16; 4],
    pub delta_best_ms: i16,
    pub delta_p1_ms: i16,
    pub gap_ahead_ms: i16,
    pub gap_behind_ms: i16,
}

impl Default for MappedTelemetry {
    fn default() -> Self {
        Self {
            rpm: 0,
            speed_kph_x10: 0,
            gear: 0,
            flags: 0,
            fuel_pct_x10: 0,
            lap_time_ms: 0,
            tyre_temp_c: [0; 4],
            tyre_press_psi: [0; 4],
            brake_temp_c: [0; 4],
            delta_best_ms: GAP_NA,
            delta_p1_ms: GAP_NA,
            gap_ahead_ms: GAP_NA,
            gap_behind_ms: GAP_NA,
        }
    }
}

impl MappedTelemetry {
    pub fn to_bytes(&self) -> [u8; TEL_PAYLOAD_SIZE] {
        let mut out = [0u8; TEL_PAYLOAD_SIZE];
        out[0..2].copy_from_slice(&self.rpm.to_le_bytes());
        out[2..4].copy_from_slice(&self.speed_kph_x10.to_le_bytes());
        out[4] = self.gear as u8;
        out[5] = self.flags;
        out[6..8].copy_from_slice(&self.fuel_pct_x10.to_le_bytes());
        out[8..10].copy_from_slice(&self.lap_time_ms.to_le_bytes());
        out[10..14].copy_from_slice(&self.tyre_temp_c);
        out[14..18].copy_from_slice(&self.tyre_press_psi);
        for (i, t) in self.brake_temp_c.iter().enumerate() {
            out[18 + i * 2..20 + i * 2].copy_from_slice(&t.to_le_bytes());
        }
        out[26..28].copy_from_slice(&self.delta_best_ms.to_le_bytes());
        out[28..30].copy_from_slice(&self.delta_p1_ms.to_le_bytes());
        out[30..32].copy_from_slice(&self.gap_ahead_ms.to_le_bytes());
        out[32..34].copy_from_slice(&self.gap_behind_ms.to_le_bytes());
        out
    }
}

#[derive(Debug, Default, Clone)]
pub struct LmuState {
    pub mapped: MappedTelemetry,
    pub telem_packets: u64,
    pub scoring_packets: u64,
    pub last_error: String,
    /// Telem fields kept for scoring merge.
    fuel: f64,
    fuel_capacity: f64,
    lap_start_et: f64,
    elapsed_time: f64,
    speed_limiter: bool,
}

impl LmuState {
    pub fn ingest_json(&mut self, text: &str) {
        let v: Value = match serde_json::from_str(text.trim()) {
            Ok(v) => v,
            Err(e) => {
                self.last_error = format!("json: {e}");
                return;
            }
        };
        let ty = v
            .get("type")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        if ty.contains("Telem") || v.get("mEngineRPM").is_some() || v.get("mGear").is_some() {
            self.apply_telem(&v);
            self.telem_packets = self.telem_packets.wrapping_add(1);
            self.last_error.clear();
        } else if ty.contains("Scoring") || v.get("mVehicles").is_some() {
            self.apply_scoring(&v);
            self.scoring_packets = self.scoring_packets.wrapping_add(1);
            self.last_error.clear();
        } else {
            self.last_error = format!("unknown type '{ty}'");
        }
    }

    fn apply_telem(&mut self, v: &Value) {
        let rpm = num_f64(v, &["mEngineRPM", "engineRPM", "rpm"]).unwrap_or(0.0);
        self.mapped.rpm = rpm.clamp(0.0, 65535.0) as u16;

        let gear = num_f64(v, &["mGear", "gear"]).unwrap_or(0.0);
        self.mapped.gear = gear.round().clamp(-128.0, 127.0) as i8;

        if let Some(speed) = num_f64(v, &["speed", "mSpeed"]) {
            // Plugin sample used km/h.
            self.mapped.speed_kph_x10 = (speed * 10.0).round().clamp(-32768.0, 32767.0) as i16;
        } else if let Some(vel) = v.get("mLocalVel") {
            let x = num_f64(vel, &["x"]).unwrap_or(0.0);
            let y = num_f64(vel, &["y"]).unwrap_or(0.0);
            let z = num_f64(vel, &["z"]).unwrap_or(0.0);
            let ms = (x * x + y * y + z * z).sqrt();
            let kph = ms * 3.6;
            self.mapped.speed_kph_x10 = (kph * 10.0).round().clamp(-32768.0, 32767.0) as i16;
        }

        self.fuel = num_f64(v, &["mFuel", "fuel"]).unwrap_or(self.fuel);
        self.fuel_capacity =
            num_f64(v, &["mFuelCapacity", "fuelCapacity"]).unwrap_or(self.fuel_capacity);
        if self.fuel_capacity > 0.05 {
            let pct = (self.fuel / self.fuel_capacity) * 100.0;
            self.mapped.fuel_pct_x10 = (pct * 10.0).round().clamp(0.0, 65535.0) as u16;
        }

        self.lap_start_et = num_f64(v, &["mLapStartET"]).unwrap_or(self.lap_start_et);
        self.elapsed_time = num_f64(v, &["mElapsedTime"]).unwrap_or(self.elapsed_time);
        if self.elapsed_time > self.lap_start_et && self.mapped.lap_time_ms == 0 {
            let ms = ((self.elapsed_time - self.lap_start_et) * 1000.0).round();
            self.mapped.lap_time_ms = ms.clamp(0.0, 65535.0) as u16;
        }

        self.speed_limiter = num_f64(v, &["mSpeedLimiter", "speedLimiter"])
            .map(|x| x > 0.5)
            .unwrap_or(self.speed_limiter);

        if let Some(wheels) = v.get("mWheel").and_then(|w| w.as_array()) {
            for (i, w) in wheels.iter().take(4).enumerate() {
                self.mapped.tyre_temp_c[i] = wheel_temp_c(w);
                self.mapped.tyre_press_psi[i] = wheel_press_psi(w);
                let bt = num_f64(w, &["brakeTemp", "mBrakeTemp"]).unwrap_or(0.0);
                self.mapped.brake_temp_c[i] = bt.round().clamp(0.0, 65535.0) as u16;
            }
        }

        self.refresh_flags_pit();
    }

    fn apply_scoring(&mut self, v: &Value) {
        let yellow = num_f64(v, &["mYellowFlagState", "yellowFlagState"]).unwrap_or(0.0);
        let mut flags = 0u8;
        // rF2-style: non-zero yellow states; treat >0 as yellow unless clearly red.
        if yellow > 0.5 {
            flags |= TEL_YELLOW;
        }

        let vehicles = v
            .get("mVehicles")
            .and_then(|x| x.as_array())
            .cloned()
            .unwrap_or_default();

        let player = vehicles.iter().find(|car| {
            num_f64(car, &["mIsPlayer", "isPlayer"]).unwrap_or(0.0) > 0.5
        });

        if let Some(p) = player {
            let into = num_f64(p, &["mTimeIntoLap", "timeIntoLap"]).unwrap_or(0.0);
            if into > 0.0 {
                self.mapped.lap_time_ms = (into * 1000.0).round().clamp(0.0, 65535.0) as u16;
            }

            let best = num_f64(p, &["mBestLapTime", "bestLapTime"]).unwrap_or(0.0);
            if best > 0.5 && into > 0.0 {
                let delta = ((into - best) * 1000.0).round();
                self.mapped.delta_best_ms = delta.clamp(-32767.0, 32767.0) as i16;
            } else {
                self.mapped.delta_best_ms = GAP_NA;
            }

            let behind_next = num_f64(p, &["mTimeBehindNext", "timeBehindNext"]).unwrap_or(-1.0);
            let laps_behind_next =
                num_f64(p, &["mLapsBehindNext", "lapsBehindNext"]).unwrap_or(0.0);
            if laps_behind_next < 0.5 && behind_next >= 0.0 {
                self.mapped.gap_ahead_ms =
                    (behind_next * 1000.0).round().clamp(0.0, 32767.0) as i16;
            } else if behind_next < 0.0 {
                self.mapped.gap_ahead_ms = GAP_NA;
            } else {
                self.mapped.gap_ahead_ms = GAP_NA;
            }

            let place = num_f64(p, &["mPlace", "place"]).unwrap_or(0.0) as i32;
            let behind = vehicles.iter().find(|car| {
                let pl = num_f64(car, &["mPlace", "place"]).unwrap_or(0.0) as i32;
                pl == place + 1
            });
            if let Some(b) = behind {
                let t = num_f64(b, &["mTimeBehindNext", "timeBehindNext"]).unwrap_or(-1.0);
                let laps = num_f64(b, &["mLapsBehindNext", "lapsBehindNext"]).unwrap_or(0.0);
                if laps < 0.5 && t >= 0.0 {
                    self.mapped.gap_behind_ms = (t * 1000.0).round().clamp(0.0, 32767.0) as i16;
                } else {
                    self.mapped.gap_behind_ms = GAP_NA;
                }
            } else {
                self.mapped.gap_behind_ms = GAP_NA;
            }

            let flag = num_f64(p, &["mFlag", "flag"]).unwrap_or(0.0);
            // Common Internals flag enums: blue often 6; red often 1 — treat heuristically.
            if flag > 0.5 {
                // Prefer blue when under blue; yellow already from session.
                if (flag - 6.0).abs() < 0.1 {
                    flags |= TEL_BLUE;
                } else if (flag - 1.0).abs() < 0.1 {
                    flags |= TEL_RED;
                }
            }
            if num_f64(p, &["mUnderYellow", "underYellow"]).unwrap_or(0.0) > 0.5 {
                flags |= TEL_YELLOW;
            }
            if num_f64(p, &["mInPits", "inPits"]).unwrap_or(0.0) > 0.5 {
                self.speed_limiter = true;
            }

            // Delta vs P1: time behind leader when on same lap count.
            let behind_leader =
                num_f64(p, &["mTimeBehindLeader", "timeBehindLeader"]).unwrap_or(-1.0);
            let laps_leader = num_f64(p, &["mLapsBehindLeader", "lapsBehindLeader"]).unwrap_or(0.0);
            if place <= 1 {
                self.mapped.delta_p1_ms = 0;
            } else if laps_leader < 0.5 && behind_leader >= 0.0 {
                self.mapped.delta_p1_ms =
                    (behind_leader * 1000.0).round().clamp(-32767.0, 32767.0) as i16;
            } else {
                self.mapped.delta_p1_ms = GAP_NA;
            }
        }

        // Keep pit bit from telem; merge session flags.
        let pit = self.mapped.flags & TEL_PIT;
        self.mapped.flags = flags | pit;
        self.refresh_flags_pit();
    }

    fn refresh_flags_pit(&mut self) {
        if self.speed_limiter {
            self.mapped.flags |= TEL_PIT;
        } else {
            self.mapped.flags &= !TEL_PIT;
        }
    }
}

fn num_f64(v: &Value, keys: &[&str]) -> Option<f64> {
    for k in keys {
        if let Some(x) = v.get(*k) {
            if let Some(n) = x.as_f64() {
                return Some(n);
            }
            if let Some(n) = x.as_i64() {
                return Some(n as f64);
            }
            if let Some(n) = x.as_u64() {
                return Some(n as f64);
            }
            if let Some(s) = x.as_str() {
                if let Ok(n) = s.parse::<f64>() {
                    return Some(n);
                }
            }
        }
    }
    None
}

fn wheel_temp_c(w: &Value) -> u8 {
    if let Some(arr) = w.get("temperature").and_then(|t| t.as_array()) {
        let mut sum = 0.0;
        let mut n = 0.0;
        for t in arr {
            if let Some(v) = t.as_f64() {
                sum += v;
                n += 1.0;
            }
        }
        if n > 0.0 {
            return (sum / n).round().clamp(0.0, 255.0) as u8;
        }
    }
    num_f64(w, &["tireCarcassTemperature", "temperature"])
        .unwrap_or(0.0)
        .round()
        .clamp(0.0, 255.0) as u8
}

fn wheel_press_psi(w: &Value) -> u8 {
    let p = num_f64(w, &["pressure"]).unwrap_or(0.0);
    // Internals tyre pressure is typically kPa (~120–220). Convert when clearly not PSI.
    let psi = if p > 50.0 { p * 0.145_037_7 } else { p };
    psi.round().clamp(0.0, 255.0) as u8
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn packs_34_bytes() {
        let m = MappedTelemetry {
            rpm: 7000,
            gear: 3,
            ..Default::default()
        };
        assert_eq!(m.to_bytes().len(), 34);
        let b = m.to_bytes();
        assert_eq!(u16::from_le_bytes([b[0], b[1]]), 7000);
        assert_eq!(b[4] as i8, 3);
    }

    #[test]
    fn parses_telem_minimal() {
        let mut s = LmuState::default();
        s.ingest_json(
            r#"{"type":"TelemInfoV01","mEngineRPM":6500,"mGear":4,"mFuel":40,"mFuelCapacity":80,"mSpeedLimiter":0,"mLocalVel":{"x":20,"y":0,"z":0}}"#,
        );
        assert_eq!(s.mapped.rpm, 6500);
        assert_eq!(s.mapped.gear, 4);
        assert_eq!(s.mapped.fuel_pct_x10, 500);
        assert!(s.mapped.speed_kph_x10 > 700); // 20 m/s ≈ 72 km/h
    }
}
