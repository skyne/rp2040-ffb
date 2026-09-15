//! Built-in Spa-Francorchamps lap showcase: telemetry for rim LEDs/TFT + PID FFB.
//! Drives the base through the USB HID PID effect pool (spring / sine / constant)
//! via CDC inject commands — same mixer a game uses over HID.

use crate::map_lmu::{MappedTelemetry, GAP_NA, TEL_BLUE, TEL_PIT, TEL_YELLOW};

/// Approximate racing-line length (metres).
pub const TRACK_LEN_M: f64 = 7004.0;
/// Target lap time for the scripted GT3-ish pace (~2:00).
pub const LAP_TARGET_S: f64 = 120.0;

/// Clamp scripted wheel angle (showcase is intentionally theatrical).
pub const STEER_CLAMP_DEG: f64 = 100.0;

/// First-order lag when adding lock (corner entry).
pub const STEER_TAU_IN_S: f64 = 0.14;
/// Faster unwind on exit — still eased enough to avoid step jerks.
pub const STEER_TAU_OUT_S: f64 = 0.07;

pub const TEL_TC: u8 = 1 << 2;
pub const TEL_ABS: u8 = 1 << 3;

/// (distance_m, speed_kph, steer_deg, note)
/// Includes brief opposite-lock after several exits (counter-steer).
pub const WAYPOINTS: &[(f64, f64, f64, &str)] = &[
    (0.0, 90.0, 0.0, "Start/Finish"),
    (160.0, 75.0, 12.0, "To La Source"),
    (270.0, 52.0, 85.0, "La Source"),
    (330.0, 68.0, 40.0, "La Source unwind"),
    (370.0, 95.0, -22.0, "La Source counter"),
    (430.0, 120.0, 0.0, "La Source exit"),
    (700.0, 185.0, -8.0, "Eau Rouge approach"),
    (860.0, 205.0, -58.0, "Eau Rouge"),
    (960.0, 220.0, 22.0, "Eau Rouge crest"),
    (1060.0, 235.0, 72.0, "Raidillon"),
    (1140.0, 250.0, -18.0, "Raidillon counter"),
    (1220.0, 265.0, 0.0, "Raidillon exit"),
    (1300.0, 270.0, 0.0, "Kemmel climb"),
    (2000.0, 295.0, 0.0, "Kemmel straight"),
    (2320.0, 255.0, 10.0, "Les Combes brake"),
    (2480.0, 110.0, 80.0, "Les Combes"),
    (2560.0, 125.0, 28.0, "Les Combes unwind"),
    (2620.0, 145.0, -20.0, "Les Combes counter"),
    (2720.0, 165.0, 0.0, "Malmedy"),
    (3000.0, 205.0, -10.0, "To Rivage"),
    (3160.0, 100.0, -75.0, "Rivage"),
    (3240.0, 125.0, -22.0, "Rivage unwind"),
    (3300.0, 155.0, 18.0, "Rivage counter"),
    (3400.0, 180.0, 0.0, "Rivage exit"),
    (3700.0, 215.0, -10.0, "To Pouhon"),
    (3900.0, 150.0, -70.0, "Pouhon 1"),
    (4080.0, 140.0, -65.0, "Pouhon 2"),
    (4180.0, 165.0, 16.0, "Pouhon counter"),
    (4300.0, 200.0, 0.0, "Fagnes approach"),
    (4520.0, 120.0, 58.0, "Fagnes"),
    (4600.0, 145.0, -16.0, "Fagnes counter"),
    (4700.0, 175.0, 0.0, "Fagnes exit"),
    (4850.0, 190.0, 20.0, "Campus"),
    (5020.0, 125.0, 70.0, "Stavelot"),
    (5120.0, 155.0, -18.0, "Stavelot counter"),
    (5240.0, 215.0, 0.0, "Stavelot exit"),
    (5450.0, 255.0, -14.0, "Blanchimont approach"),
    (5660.0, 280.0, -55.0, "Blanchimont"),
    (5760.0, 265.0, 14.0, "Blanchimont counter"),
    (5880.0, 255.0, 0.0, "Blanchimont exit"),
    (6000.0, 250.0, 0.0, "To Bus Stop"),
    (6260.0, 145.0, 22.0, "Bus Stop brake"),
    (6380.0, 80.0, -70.0, "Bus Stop left"),
    (6460.0, 88.0, 12.0, "Bus Stop swap"),
    (6540.0, 92.0, 78.0, "Bus Stop right"),
    (6620.0, 120.0, -16.0, "Bus Stop counter"),
    (6720.0, 160.0, 0.0, "Bus Stop exit"),
    (6850.0, 190.0, 0.0, "Pit straight"),
    (TRACK_LEN_M, 100.0, 0.0, "Start/Finish"),
];

/// Kerb / rumble strips: (start_m, end_m, amp_deg, hz).
/// Amp must be large enough to survive spring follow on the weak MC33926.
const KERBS: &[(f64, f64, f64, f64)] = &[
    (860.0, 1180.0, 10.0, 14.0),  // Eau Rouge / Raidillon
    (2460.0, 2650.0, 12.0, 16.0), // Les Combes
    (3140.0, 3360.0, 11.0, 15.0), // Rivage
    (3880.0, 4220.0, 9.0, 13.0),  // Pouhon
    (4500.0, 4680.0, 10.0, 14.0), // Fagnes
    (5000.0, 5180.0, 11.0, 15.0), // Stavelot
    (5640.0, 5820.0, 8.0, 14.0),  // Blanchimont
    (6360.0, 6660.0, 14.0, 17.0), // Bus Stop sausage
];

/// Hard ABS zones (heavy brake).
const ABS_ZONES: &[(f64, f64)] = &[
    (220.0, 300.0),   // La Source
    (2320.0, 2500.0), // Les Combes
    (3120.0, 3200.0), // Rivage
    (6240.0, 6400.0), // Bus Stop
];

/// TC flash on greedy exits.
const TC_ZONES: &[(f64, f64)] = &[
    (360.0, 450.0),
    (1140.0, 1280.0),
    (2580.0, 2750.0),
    (3260.0, 3450.0),
    (4160.0, 4350.0),
    (5100.0, 5280.0),
    (6600.0, 6780.0),
];

#[derive(Debug, Clone)]
pub struct SpaFrame {
    pub mapped: MappedTelemetry,
    /// Racing-line steer only (smooth).
    pub line_steer_deg: f64,
    /// Kerb rumble + impact — retained for UI / debug (PID path uses rumble_* / kick_*).
    #[allow(dead_code)]
    pub shake_deg: f64,
    pub steer_deg: f64,
    pub note: String,
    pub lap: u32,
    pub sector: u8,
    #[allow(dead_code)]
    pub on_kerb: bool,
    /// PID periodic rumble amplitude 0..1 (kerb only, no impact).
    pub rumble_amp: f64,
    /// PID periodic rumble frequency (Hz).
    pub rumble_hz: f64,
    /// PID constant kick −1..1 from kerb impact (decays).
    pub kick_mag: f64,
}

#[derive(Debug)]
pub struct SpaLapSim {
    dist_m: f64,
    speed_kph: f64,
    /// Smoothed line steer (no rumble).
    line_steer_deg: f64,
    gear: i8,
    rpm: f64,
    fuel: f64,
    fuel_cap: f64,
    lap: u32,
    lap_start_et: f64,
    best_lap: f64,
    gap_ahead: f64,
    gap_behind: f64,
    tyre_c: [f64; 4],
    brake_c: [f64; 4],
    note: String,
    /// Decaying kick when first hitting a kerb.
    impact_amp: f64,
    impact_phase: f64,
    was_on_kerb: bool,
}

impl Default for SpaLapSim {
    fn default() -> Self {
        Self {
            // Skip the long Start/Finish coast — jump to La Source approach so
            // the first big spring hits within ~2–3 s (showcase feel).
            dist_m: 180.0,
            speed_kph: 78.0,
            line_steer_deg: 8.0,
            gear: 2,
            rpm: 3800.0,
            fuel: 68.0,
            fuel_cap: 80.0,
            lap: 1,
            lap_start_et: 0.0,
            best_lap: 118.4,
            gap_ahead: 1.2,
            gap_behind: 0.8,
            tyre_c: [82.0, 83.0, 80.0, 80.5],
            brake_c: [380.0, 375.0, 340.0, 338.0],
            note: "To La Source".into(),
            impact_amp: 0.0,
            impact_phase: 0.0,
            was_on_kerb: false,
        }
    }
}

impl SpaLapSim {
    pub fn step(&mut self, dt: f64, et: f64) -> SpaFrame {
        let (target_v, target_steer, note) = lerp_wp(self.dist_m);
        self.note = note.to_string();

        let tau = if target_v < self.speed_kph {
            0.45
        } else {
            0.95
        };
        self.speed_kph += (target_v - self.speed_kph) * (1.0 - (-dt / tau).exp());
        self.speed_kph = self.speed_kph.clamp(40.0, 320.0);

        let adding_lock = target_steer.abs() > self.line_steer_deg.abs() + 0.5;
        let steer_tau = if adding_lock {
            STEER_TAU_IN_S
        } else {
            STEER_TAU_OUT_S
        };
        self.line_steer_deg +=
            (target_steer - self.line_steer_deg) * (1.0 - (-dt / steer_tau).exp());
        self.line_steer_deg = self.line_steer_deg.clamp(-STEER_CLAMP_DEG, STEER_CLAMP_DEG);

        let d = self.dist_m.rem_euclid(TRACK_LEN_M);
        let (kerb_amp, kerb_hz) = kerb_at(d);
        let on_kerb = kerb_amp > 0.05;

        if on_kerb && !self.was_on_kerb {
            self.impact_amp = (kerb_amp * 1.4).min(16.0);
            self.impact_phase = 0.0;
            self.note = format!("{note} · KERB");
        } else if on_kerb {
            self.note = format!("{note} · rumble");
        }
        self.was_on_kerb = on_kerb;

        self.impact_phase += dt;
        self.impact_amp *= (-dt / 0.16).exp();

        let rumble = if on_kerb {
            let fund = (et * kerb_hz * std::f64::consts::TAU).sin();
            let harm = (et * kerb_hz * 2.0 * std::f64::consts::TAU).sin();
            kerb_amp * (0.75 * fund + 0.25 * harm)
        } else {
            0.0
        };
        let impact = if self.impact_amp > 0.2 {
            let kick = (self.impact_phase * 32.0).sin() * self.impact_amp;
            let sign = if self.line_steer_deg >= 0.0 {
                -1.0
            } else {
                1.0
            };
            kick * sign
        } else {
            0.0
        };
        let shake = rumble + impact;
        let steer_out = (self.line_steer_deg + shake).clamp(-STEER_CLAMP_DEG, STEER_CLAMP_DEG);

        // Map theatrical deg-amps into PID effect units (games-like).
        let rumble_amp = if on_kerb {
            (kerb_amp / 14.0).clamp(0.0, 1.0)
        } else {
            0.0
        };
        let rumble_hz = if on_kerb { kerb_hz } else { 0.0 };
        let kick_mag = if self.impact_amp > 0.2 {
            let sign = if self.line_steer_deg >= 0.0 {
                -1.0
            } else {
                1.0
            };
            (self.impact_amp / 16.0).clamp(0.0, 1.0) * sign
        } else {
            0.0
        };

        let (g, rpm) = gear_for_speed(self.speed_kph);
        self.gear = g;
        // Small flutter; kerb adds noise without blowing past a readable blink.
        let mut rpm_out = rpm + 40.0 * (et * 18.0).sin();
        if on_kerb {
            rpm_out += 120.0 * (et * kerb_hz * std::f64::consts::TAU).sin().abs();
        }
        self.rpm = rpm_out.clamp(800.0, 9000.0);

        let ms = self.speed_kph / 3.6;
        self.dist_m += ms * dt;
        if self.dist_m >= TRACK_LEN_M {
            let lap_t = et - self.lap_start_et;
            if lap_t > 40.0 && (self.best_lap <= 0.0 || lap_t < self.best_lap) {
                self.best_lap = lap_t;
            }
            self.dist_m -= TRACK_LEN_M;
            self.lap = self.lap.saturating_add(1);
            self.lap_start_et = et;
            self.gap_ahead = (1.2 + 0.15 * (self.lap as f64).sin()).max(0.3);
            self.gap_behind = (0.7 + 0.2 * (self.lap as f64 * 1.3).sin()).max(0.2);
            self.note = format!("LAP {} complete ({:.1}s)", self.lap - 1, lap_t);
        }

        self.fuel = (self.fuel - (2.1 / LAP_TARGET_S) * dt).max(4.0);

        let braking = target_v + 25.0 < self.speed_kph;
        for i in 0..4 {
            let heat = if braking { 0.35 } else { -0.12 };
            self.brake_c[i] = (self.brake_c[i] + heat * dt * 40.0).clamp(220.0, 780.0);
            let mut load = 0.08 + 0.04 * (self.speed_kph / 300.0);
            if i >= 2 {
                load *= 0.85;
            }
            load += (steer_out.abs() / STEER_CLAMP_DEG) * 0.04;
            if on_kerb {
                load += 0.12;
            }
            self.tyre_c[i] = (self.tyre_c[i] + (load - 0.05) * dt * 8.0).clamp(70.0, 110.0);
        }

        let sector = sector_of(self.dist_m);
        let into = (et - self.lap_start_et).max(0.0);
        let d = self.dist_m.rem_euclid(TRACK_LEN_M);

        let yellow = (850.0..1100.0).contains(&d) && self.lap % 3 == 0;
        let blue = (1500.0..1950.0).contains(&d)
            || (5300.0..5650.0).contains(&d)
            || (6700.0..6950.0).contains(&d);
        let pit = self.lap % 4 == 0 && d < 350.0;
        let abs_on = in_zones(d, ABS_ZONES) && braking;
        // Strobe TC while on kerb exits / power-on.
        let tc_on = in_zones(d, TC_ZONES) || (on_kerb && !braking && self.speed_kph > 100.0);

        let mut flags = 0u8;
        if yellow {
            flags |= TEL_YELLOW;
        }
        if blue {
            flags |= TEL_BLUE;
        }
        if pit {
            flags |= TEL_PIT;
        }
        if abs_on {
            flags |= TEL_ABS;
        }
        if tc_on {
            flags |= TEL_TC;
        }

        let fuel_pct = if self.fuel_cap > 0.0 {
            (self.fuel / self.fuel_cap * 100.0).clamp(0.0, 100.0)
        } else {
            0.0
        };

        let delta_best_ms = if self.best_lap > 0.0 {
            let frac = d / TRACK_LEN_M;
            let pred = into - self.best_lap * frac;
            (pred * 1000.0).round().clamp(-30000.0, 30000.0) as i16
        } else {
            GAP_NA
        };

        let mapped = MappedTelemetry {
            rpm: self.rpm.clamp(0.0, 65535.0) as u16,
            speed_kph_x10: (self.speed_kph * 10.0).round().clamp(-32768.0, 32767.0) as i16,
            gear: self.gear,
            flags,
            fuel_pct_x10: (fuel_pct * 10.0).round().clamp(0.0, 65535.0) as u16,
            lap_time_ms: (into * 1000.0).min(65535.0) as u16,
            tyre_temp_c: [
                self.tyre_c[0].round().clamp(0.0, 255.0) as u8,
                self.tyre_c[1].round().clamp(0.0, 255.0) as u8,
                self.tyre_c[2].round().clamp(0.0, 255.0) as u8,
                self.tyre_c[3].round().clamp(0.0, 255.0) as u8,
            ],
            tyre_press_psi: [186, 184, 180, 181],
            brake_temp_c: [
                self.brake_c[0].round().clamp(0.0, 65535.0) as u16,
                self.brake_c[1].round().clamp(0.0, 65535.0) as u16,
                self.brake_c[2].round().clamp(0.0, 65535.0) as u16,
                self.brake_c[3].round().clamp(0.0, 65535.0) as u16,
            ],
            delta_best_ms,
            delta_p1_ms: ((self.gap_ahead + 2.5) * 1000.0)
                .round()
                .clamp(-30000.0, 30000.0) as i16,
            gap_ahead_ms: (self.gap_ahead * 1000.0).round().clamp(-30000.0, 30000.0) as i16,
            gap_behind_ms: (self.gap_behind * 1000.0).round().clamp(-30000.0, 30000.0) as i16,
        };

        SpaFrame {
            mapped,
            line_steer_deg: self.line_steer_deg,
            shake_deg: shake,
            steer_deg: steer_out,
            note: self.note.clone(),
            lap: self.lap,
            sector,
            on_kerb,
            rumble_amp,
            rumble_hz,
            kick_mag,
        }
    }
}

fn kerb_at(dist: f64) -> (f64, f64) {
    let d = dist.rem_euclid(TRACK_LEN_M);
    for &(a, b, amp, hz) in KERBS {
        if d >= a && d <= b {
            // Ease amp in/out over ~15 m so it doesn't click on.
            let edge = 15.0;
            let into = ((d - a) / edge).clamp(0.0, 1.0);
            let out = ((b - d) / edge).clamp(0.0, 1.0);
            let gate = into.min(out);
            return (amp * gate, hz);
        }
    }
    (0.0, 20.0)
}

fn in_zones(dist: f64, zones: &[(f64, f64)]) -> bool {
    let d = dist.rem_euclid(TRACK_LEN_M);
    zones.iter().any(|&(a, b)| d >= a && d <= b)
}

fn lerp_wp(dist: f64) -> (f64, f64, &'static str) {
    let d = dist.rem_euclid(TRACK_LEN_M);
    for i in 0..WAYPOINTS.len() - 1 {
        let (d0, v0, s0, _) = WAYPOINTS[i];
        let (d1, v1, s1, name) = WAYPOINTS[i + 1];
        if d0 <= d && d <= d1 {
            let u = if (d1 - d0).abs() < 1e-6 {
                0.0
            } else {
                ((d - d0) / (d1 - d0)).clamp(0.0, 1.0)
            };
            let u_speed = u * u * (3.0 - 2.0 * u);
            let u_steer = if s1.abs() < s0.abs() {
                1.0 - (1.0 - u).powi(2)
            } else {
                u * u
            };
            return (v0 + (v1 - v0) * u_speed, s0 + (s1 - s0) * u_steer, name);
        }
    }
    let last = WAYPOINTS[WAYPOINTS.len() - 1];
    (last.1, last.2, last.3)
}

/// Rim default shift LED thresholds (see FfbLink::defaultRimConfig / shift_rpm_*).
pub const SHIFT_RPM: [u16; 5] = [5000, 6000, 7000, 7500, 7800];

fn gear_for_speed(kph: f64) -> (i8, f64) {
    // Keep RPM inside the rim LED window: dark below 5k → fill → blink at 7.8k.
    // Each gear climbs from just under stage0 to a brief overrev flash, then shifts.
    let bands: &[(f64, i8, f64, f64)] = &[
        (0.0, 1, 0.0, 70.0),
        (70.0, 2, 60.0, 110.0),
        (110.0, 3, 100.0, 150.0),
        (150.0, 4, 140.0, 190.0),
        (190.0, 5, 180.0, 240.0),
        (240.0, 6, 230.0, 290.0),
        (290.0, 7, 280.0, 340.0),
    ];
    let lo_rpm = (SHIFT_RPM[0] as f64) - 400.0; // ~4600 — bar still mostly dark
    let hi_rpm = (SHIFT_RPM[4] as f64) + 150.0; // ~7950 — clear overrev blink
    for &(lo, g, a, b) in bands {
        if kph < b || g == 7 {
            let span = (b - a).max(1.0);
            let u = ((kph - a) / span).clamp(0.0, 1.0);
            // Ease so more time is spent climbing the lit stages (5→7.5k).
            let u = u.powf(0.85);
            let mut rpm = lo_rpm + u * (hi_rpm - lo_rpm);
            if kph < lo + 5.0 {
                // Just after an upshift — drop below first threshold.
                rpm = (SHIFT_RPM[0] as f64) - 200.0;
            }
            return (g, rpm);
        }
    }
    (7, hi_rpm)
}

fn sector_of(dist: f64) -> u8 {
    let d = dist.rem_euclid(TRACK_LEN_M);
    if d < 2550.0 {
        1
    } else if d < 5100.0 {
        2
    } else {
        3
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lap_advances() {
        let mut sim = SpaLapSim::default();
        let mut et = 0.0;
        let mut saw_lap2 = false;
        for _ in 0..20_000 {
            et += 0.02;
            let f = sim.step(0.02, et);
            if f.lap >= 2 {
                saw_lap2 = true;
                break;
            }
        }
        assert!(saw_lap2);
    }

    #[test]
    fn steer_clamped() {
        let mut sim = SpaLapSim::default();
        let mut et = 0.0;
        for _ in 0..2000 {
            et += 0.02;
            let f = sim.step(0.02, et);
            assert!(f.steer_deg.abs() <= STEER_CLAMP_DEG + 0.01);
        }
    }

    #[test]
    fn rpm_sweeps_shift_thresholds() {
        let mut min_r = f64::MAX;
        let mut max_r = 0.0f64;
        let mut saw_overrev = false;
        let mut saw_mid = false;
        for kph in (40..320).step_by(2) {
            let (_, rpm) = gear_for_speed(kph as f64);
            min_r = min_r.min(rpm);
            max_r = max_r.max(rpm);
            if rpm >= SHIFT_RPM[2] as f64 {
                saw_mid = true;
            }
            if rpm >= SHIFT_RPM[4] as f64 {
                saw_overrev = true;
            }
        }
        assert!(
            min_r < SHIFT_RPM[0] as f64,
            "should dip below first LED stage"
        );
        assert!(saw_mid, "should reach yellow/red fill stages");
        assert!(saw_overrev, "should hit overrev blink threshold");
        assert!(max_r < 9200.0);
    }
}
