#!/usr/bin/env python3
"""Fake LMU Telemetry Socket — one emulated lap of Circuit de la Sarthe (Le Mans).

Sends TelemInfoV01 + ScoringInfoV01 JSON to UDP for ffb-config Race mode testing.

Usage:
  # Start Race mode in ffb-config first, then:
  python3 tools/lmu-telem-sim.py
  python3 tools/lmu-telem-sim.py --host 127.0.0.1 --port 5000 --laps 0
"""

from __future__ import annotations

import argparse
import json
import math
import socket
import time
from dataclasses import dataclass


# Approximate GT3-ish lap of Circuit de la Sarthe (~13.6 km), ~3:50 target.
# Distances in metres along the racing line; speeds are target km/h at that point.
# Waypoints are (distance_m, speed_kph, note).
TRACK_LEN_M = 13626.0
LAP_TARGET_S = 230.0  # ~3:50

WAYPOINTS: list[tuple[float, float, str]] = [
    (0, 80, "Start/Finish"),
    (200, 160, "Dunlop rise"),
    (450, 120, "Dunlop chicane"),
    (700, 200, "Toward Esses"),
    (1100, 140, "Esses"),
    (1600, 220, "Tertre Rouge exit"),
    (2500, 300, "Mulsanne early"),
    (4000, 320, "Mulsanne mid"),
    (5200, 280, "First chicane brake"),
    (5450, 120, "Mulsanne chicane 1"),
    (5800, 300, "Mulsanne resume"),
    (7200, 320, "Toward second chicane"),
    (7800, 270, "Second chicane brake"),
    (8050, 110, "Mulsanne chicane 2"),
    (8500, 300, "To Mulsanne corner"),
    (9200, 90, "Mulsanne corner"),
    (9800, 200, "Indianapolis approach"),
    (10200, 100, "Indianapolis"),
    (10600, 180, "Arnage approach"),
    (10900, 85, "Arnage"),
    (11400, 210, "Porsche curves entry"),
    (11800, 150, "Porsche mid"),
    (12200, 170, "Porsche exit"),
    (12600, 220, "Ford chicanes"),
    (12950, 130, "Ford chicane"),
    (13300, 180, "To pit straight"),
    (TRACK_LEN_M, 90, "Start/Finish"),
]


@dataclass
class CarState:
    dist_m: float = 0.0
    speed_kph: float = 80.0
    gear: int = 2
    rpm: float = 3500.0
    fuel: float = 72.0
    fuel_cap: float = 80.0
    lap: int = 1
    lap_start_et: float = 0.0
    best_lap: float = 228.5
    place: int = 3
    gap_ahead: float = 1.4
    gap_behind: float = 0.9
    sector: int = 1
    tyre_c: list[float] | None = None
    brake_c: list[float] | None = None

    def __post_init__(self) -> None:
        if self.tyre_c is None:
            self.tyre_c = [82.0, 83.0, 80.0, 80.5]
        if self.brake_c is None:
            self.brake_c = [380.0, 375.0, 340.0, 338.0]


def lerp_speed(dist: float) -> tuple[float, str]:
    d = dist % TRACK_LEN_M
    for i in range(len(WAYPOINTS) - 1):
        d0, v0, _ = WAYPOINTS[i]
        d1, v1, name = WAYPOINTS[i + 1]
        if d0 <= d <= d1:
            u = 0.0 if d1 == d0 else (d - d0) / (d1 - d0)
            # Ease in/out a bit so braking isn't a knife edge.
            u = u * u * (3 - 2 * u)
            return v0 + (v1 - v0) * u, name
    return WAYPOINTS[-1][1], WAYPOINTS[-1][2]


def gear_for_speed(kph: float) -> tuple[int, float]:
    # Crude GT3 ratios → RPM in ~3–8.5k band.
    bands = [
        (0, 1, 0, 70),
        (70, 2, 60, 110),
        (110, 3, 100, 150),
        (150, 4, 140, 190),
        (190, 5, 180, 240),
        (240, 6, 230, 290),
        (290, 7, 280, 340),
    ]
    for lo, g, a, b in bands:
        if kph < b or g == 7:
            span = max(b - a, 1.0)
            u = max(0.0, min(1.0, (kph - a) / span))
            rpm = 3200 + u * 5300
            if kph < lo + 5:
                rpm = max(2800, rpm - 400)
            return g, rpm
    return 7, 7500.0


def sector_of(dist: float) -> int:
    d = dist % TRACK_LEN_M
    if d < 5800:
        return 1
    if d < 10900:
        return 2
    return 3


def step(car: CarState, dt: float, et: float) -> str:
    target, note = lerp_speed(car.dist_m)
    # First-order chase so accel/brake feel less teleporty.
    tau = 0.55 if target < car.speed_kph else 1.1
    car.speed_kph += (target - car.speed_kph) * (1.0 - math.exp(-dt / tau))
    car.speed_kph = max(40.0, car.speed_kph)

    car.gear, car.rpm = gear_for_speed(car.speed_kph)
    # Shift blip / overrev flicker near gear changes
    car.rpm += 80 * math.sin(et * 18.0)

    ms = car.speed_kph / 3.6
    car.dist_m += ms * dt
    if car.dist_m >= TRACK_LEN_M:
        lap_t = et - car.lap_start_et
        if lap_t > 60 and (car.best_lap <= 0 or lap_t < car.best_lap):
            car.best_lap = lap_t
        car.dist_m -= TRACK_LEN_M
        car.lap += 1
        car.lap_start_et = et
        car.gap_ahead = max(0.3, car.gap_ahead + 0.05 * math.sin(car.lap))
        car.gap_behind = max(0.2, 0.7 + 0.2 * math.sin(car.lap * 1.3))
        note = f"LAP {car.lap} complete ({lap_t:.1f}s)"

    car.sector = sector_of(car.dist_m)
    # Fuel ~2.4 L / lap
    car.fuel = max(4.0, car.fuel - (2.4 / LAP_TARGET_S) * dt)

    braking = target + 25 < car.speed_kph
    for i in range(4):
        heat = 0.35 if braking else -0.12
        car.brake_c[i] = max(220.0, min(780.0, car.brake_c[i] + heat * dt * 40))
        load = 0.08 + 0.04 * (car.speed_kph / 320.0)
        if i >= 2:
            load *= 0.85
        car.tyre_c[i] = max(70.0, min(110.0, car.tyre_c[i] + (load - 0.05) * dt * 8))

    return note


def wheel(temp: float, brake: float, press: float = 185.0) -> dict:
    return {
        "brakeTemp": brake,
        "pressure": press,
        "temperature": [temp - 2.5, temp, temp + 2.0],
        "tireCarcassTemperature": temp,
    }


def telem(car: CarState, et: float, pit: bool) -> dict:
    ms = car.speed_kph / 3.6
    return {
        "type": "TelemInfoV01",
        "mEngineRPM": car.rpm,
        "mGear": car.gear,
        "mFuel": car.fuel,
        "mFuelCapacity": car.fuel_cap,
        "mSpeedLimiter": 1 if pit else 0,
        "mElapsedTime": et,
        "mLapStartET": car.lap_start_et,
        "mLapNumber": car.lap,
        "mCurrentSector": car.sector,
        "mTrackName": "Le Mans Circuit de la Sarthe",
        "mVehicleName": "Sim GT3",
        "mLocalVel": {"x": ms, "y": 0.0, "z": 0.0},
        "speed": car.speed_kph,
        "mWheel": [
            wheel(car.tyre_c[0], car.brake_c[0], 186),
            wheel(car.tyre_c[1], car.brake_c[1], 184),
            wheel(car.tyre_c[2], car.brake_c[2], 180),
            wheel(car.tyre_c[3], car.brake_c[3], 181),
        ],
    }


def scoring(car: CarState, et: float, yellow: bool, blue: bool) -> dict:
    into = max(0.0, et - car.lap_start_et)
    return {
        "type": "ScoringInfoV01",
        "mTrackName": "Le Mans Circuit de la Sarthe",
        "mYellowFlagState": 1 if yellow else 0,
        "mVehicles": [
            {
                "mIsPlayer": 1,
                "mDriverName": "Player",
                "mVehicleName": "Sim GT3",
                "mPlace": car.place,
                "mTotalLaps": car.lap - 1,
                "mSector": car.sector,
                "mTimeIntoLap": into,
                "mBestLapTime": car.best_lap,
                "mTimeBehindNext": car.gap_ahead,
                "mLapsBehindNext": 0,
                "mTimeBehindLeader": car.gap_ahead + 2.8,
                "mLapsBehindLeader": 0,
                "mFlag": 6 if blue else 0,
                "mUnderYellow": 1 if yellow else 0,
                "mInPits": 0,
            },
            {
                "mIsPlayer": 0,
                "mPlace": car.place + 1,
                "mTimeBehindNext": car.gap_behind,
                "mLapsBehindNext": 0,
            },
        ],
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5000)
    ap.add_argument("--hz", type=float, default=50.0)
    ap.add_argument("--laps", type=int, default=0, help="0 = forever")
    ap.add_argument("--broadcast", action="store_true")
    ap.add_argument("--pit-every", type=int, default=0, help="Enter pit limiter every N laps (0=off)")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if args.broadcast:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    dest = (args.host, args.port)
    period = 1.0 / max(args.hz, 1.0)

    car = CarState()
    t0 = time.monotonic()
    last = t0
    n = 0
    note = "Start/Finish"
    print(
        f"Le Mans sim → {dest[0]}:{dest[1]} @ {args.hz:.0f} Hz  "
        f"({TRACK_LEN_M/1000:.1f} km / ~{LAP_TARGET_S:.0f}s lap)  Ctrl+C stop"
    )

    try:
        while True:
            now = time.monotonic()
            et = now - t0
            dt = min(0.1, now - last)
            last = now

            note = step(car, dt, et)
            if args.laps and car.lap > args.laps + 1:
                print(f"\nfinished {args.laps} lap(s)")
                break

            pit = bool(args.pit_every and (car.lap % args.pit_every == 0) and car.dist_m < 400)
            # Brief yellow at Tertre / Porsche, blue near end of lap occasionally
            d = car.dist_m % TRACK_LEN_M
            yellow = 1400 < d < 1700 and (car.lap % 3 == 0)
            blue = d > 12800 and (car.lap % 2 == 0)

            sock.sendto(
                (json.dumps(telem(car, et, pit), separators=(",", ":")) + "\n").encode(),
                dest,
            )
            if n % 5 == 0:
                sock.sendto(
                    (json.dumps(scoring(car, et, yellow, blue), separators=(",", ":")) + "\n").encode(),
                    dest,
                )

            if n % max(1, int(args.hz)) == 0:
                into = et - car.lap_start_et
                flags = []
                if yellow:
                    flags.append("YEL")
                if blue:
                    flags.append("BLU")
                if pit:
                    flags.append("PIT")
                fl = (" " + " ".join(flags)) if flags else ""
                print(
                    f"\r L{car.lap} S{car.sector} {into:6.1f}s  "
                    f"{car.speed_kph:5.0f} km/h  G{car.gear}  {car.rpm:5.0f} rpm  "
                    f"{car.dist_m/1000:5.2f} km  {note[:28]:<28}{fl}   ",
                    end="",
                    flush=True,
                )

            n += 1
            time.sleep(period)
    except KeyboardInterrupt:
        print("\nstopped")


if __name__ == "__main__":
    main()
