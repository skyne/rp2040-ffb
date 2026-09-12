#!/usr/bin/env python3
"""Unit tests for LMU telemetry simulator"""

import unittest
import json
import socket
import time
from unittest.mock import patch, MagicMock
import sys
import os

# Add tools directory to path to import the simulator
sys.path.insert(0, os.path.dirname(__file__))


class TestTelemSimulator(unittest.TestCase):
    """Test suite for LMU telemetry simulator"""

    def test_track_waypoints_ordering(self):
        """Verify waypoints are in ascending distance order"""
        from lmu_telem_sim import WAYPOINTS
        
        for i in range(len(WAYPOINTS) - 1):
            self.assertLess(
                WAYPOINTS[i][0], 
                WAYPOINTS[i + 1][0],
                f"Waypoint {i} distance ({WAYPOINTS[i][0]}) >= waypoint {i+1} ({WAYPOINTS[i+1][0]})"
            )

    def test_track_length_consistency(self):
        """Verify track length matches last waypoint"""
        from lmu_telem_sim import WAYPOINTS, TRACK_LEN_M
        
        # Last waypoint should be close to track length
        last_distance = WAYPOINTS[-1][0]
        self.assertLess(
            abs(last_distance - TRACK_LEN_M),
            500,  # Allow 500m tolerance
            f"Last waypoint ({last_distance}m) too far from track length ({TRACK_LEN_M}m)"
        )

    def test_speed_values_reasonable(self):
        """Verify speed values are within reasonable GT3 range"""
        from lmu_telem_sim import WAYPOINTS
        
        for distance, speed, note in WAYPOINTS:
            self.assertGreater(speed, 0, f"Speed at {distance}m is not positive")
            self.assertLess(speed, 350, f"Speed at {distance}m ({speed} km/h) exceeds GT3 max")

    def test_lap_time_calculation(self):
        """Verify lap time calculation is reasonable"""
        from lmu_telem_sim import LAP_TARGET_S, TRACK_LEN_M
        
        # Average speed check
        avg_speed_ms = TRACK_LEN_M / LAP_TARGET_S
        avg_speed_kph = avg_speed_ms * 3.6
        
        # GT3 at Le Mans should average 180-220 km/h
        self.assertGreater(avg_speed_kph, 150, "Average speed too low for GT3")
        self.assertLess(avg_speed_kph, 250, "Average speed too high for GT3")

    def test_interpolate_speed(self):
        """Test speed interpolation between waypoints"""
        from lmu_telem_sim import interpolate_speed
        
        # Simple linear interpolation test
        # Waypoints: [(0, 100), (100, 200)]
        # At distance 50, should be 150 km/h
        waypoints = [(0, 100, "start"), (100, 200, "end")]
        speed = interpolate_speed(50, waypoints)
        self.assertAlmostEqual(speed, 150, delta=5)

    def test_interpolate_speed_edge_cases(self):
        """Test speed interpolation at edges"""
        from lmu_telem_sim import interpolate_speed
        
        waypoints = [(0, 100, "start"), (100, 200, "end")]
        
        # At start
        speed = interpolate_speed(0, waypoints)
        self.assertAlmostEqual(speed, 100, delta=1)
        
        # At end
        speed = interpolate_speed(100, waypoints)
        self.assertAlmostEqual(speed, 200, delta=1)
        
        # Before start (should clamp to first)
        speed = interpolate_speed(-10, waypoints)
        self.assertAlmostEqual(speed, 100, delta=1)

    def test_calculate_gear(self):
        """Test gear calculation from speed"""
        from lmu_telem_sim import calculate_gear
        
        # Typical GT3 gear ratios
        test_cases = [
            (0, 0),      # Stopped = neutral
            (30, 1),     # Low speed = 1st
            (80, 2),     # 80 km/h = 2nd
            (120, 3),    # 120 km/h = 3rd
            (160, 4),    # 160 km/h = 4th
            (200, 5),    # 200 km/h = 5th
            (280, 6),    # 280 km/h = 6th
        ]
        
        for speed, expected_gear in test_cases:
            gear = calculate_gear(speed)
            self.assertEqual(
                gear, 
                expected_gear,
                f"Speed {speed} km/h should be gear {expected_gear}, got {gear}"
            )

    def test_calculate_rpm(self):
        """Test RPM calculation from speed and gear"""
        from lmu_telem_sim import calculate_rpm
        
        # Test cases: (speed_kph, gear, expected_min_rpm, expected_max_rpm)
        test_cases = [
            (0, 0, 0, 1500),      # Neutral, idle
            (100, 3, 4000, 7000), # Mid-range in 3rd
            (280, 6, 5000, 8500), # High speed in 6th
        ]
        
        for speed, gear, min_rpm, max_rpm in test_cases:
            rpm = calculate_rpm(speed, gear)
            self.assertGreaterEqual(rpm, min_rpm)
            self.assertLessEqual(rpm, max_rpm)

    def test_json_structure(self):
        """Verify telemetry JSON has required fields"""
        from lmu_telem_sim import build_telemetry_packet
        
        telem = build_telemetry_packet(0, 0, 0)
        
        # Check top-level structure
        self.assertIn("TelemInfoV01", telem)
        self.assertIn("ScoringInfoV01", telem)
        
        # Check TelemInfoV01 fields
        telem_info = telem["TelemInfoV01"]
        required_fields = [
            "mSpeed", "mRPM", "mGear", "mFuel", 
            "mLapStartET", "mLapNumber"
        ]
        for field in required_fields:
            self.assertIn(field, telem_info, f"Missing field: {field}")
        
        # Check ScoringInfoV01 fields
        scoring_info = telem["ScoringInfoV01"]
        self.assertIn("mVehicles", scoring_info)
        self.assertIsInstance(scoring_info["mVehicles"], list)
        self.assertGreater(len(scoring_info["mVehicles"]), 0)

    def test_fuel_consumption(self):
        """Test fuel consumption over distance"""
        from lmu_telem_sim import calculate_fuel_pct, TRACK_LEN_M
        
        # At start, should be full
        fuel_start = calculate_fuel_pct(0, 0)
        self.assertAlmostEqual(fuel_start, 100.0, delta=1)
        
        # After one lap, should have consumed some fuel
        fuel_lap1 = calculate_fuel_pct(TRACK_LEN_M, 1)
        self.assertLess(fuel_lap1, fuel_start)
        self.assertGreater(fuel_lap1, 50)  # Should still have >50% after 1 lap
        
        # After many laps, should be low
        fuel_lap5 = calculate_fuel_pct(TRACK_LEN_M * 5, 5)
        self.assertLess(fuel_lap5, 30)

    def test_lap_counter(self):
        """Test lap counting logic"""
        from lmu_telem_sim import calculate_lap_number, TRACK_LEN_M
        
        # First lap
        lap = calculate_lap_number(TRACK_LEN_M * 0.5)
        self.assertEqual(lap, 1)
        
        # Second lap
        lap = calculate_lap_number(TRACK_LEN_M * 1.5)
        self.assertEqual(lap, 2)
        
        # Fifth lap
        lap = calculate_lap_number(TRACK_LEN_M * 4.8)
        self.assertEqual(lap, 5)

    def test_position_wraparound(self):
        """Test position wraps correctly at track length"""
        from lmu_telem_sim import wrap_distance, TRACK_LEN_M
        
        # Before end
        self.assertAlmostEqual(wrap_distance(TRACK_LEN_M - 100), TRACK_LEN_M - 100)
        
        # Past end (should wrap to start)
        self.assertAlmostEqual(wrap_distance(TRACK_LEN_M + 50), 50, delta=1)
        
        # Two laps
        self.assertAlmostEqual(wrap_distance(TRACK_LEN_M * 2 + 100), 100, delta=1)

    @patch('socket.socket')
    def test_udp_send(self, mock_socket):
        """Test UDP packet sending"""
        from lmu_telem_sim import send_telemetry
        
        # Mock socket
        mock_sock_instance = MagicMock()
        mock_socket.return_value = mock_sock_instance
        
        # Send telemetry
        telem = {"test": "data"}
        send_telemetry(telem, "127.0.0.1", 5000, mock_sock_instance)
        
        # Verify sendto was called
        mock_sock_instance.sendto.assert_called_once()
        
        # Verify data was JSON
        call_args = mock_sock_instance.sendto.call_args[0]
        sent_data = call_args[0]
        self.assertIsInstance(sent_data, bytes)
        
        # Verify it can be decoded
        decoded = json.loads(sent_data.decode('utf-8'))
        self.assertEqual(decoded["test"], "data")

    def test_flags_yellow(self):
        """Test yellow flag logic"""
        from lmu_telem_sim import get_flags, TRACK_LEN_M
        
        # Yellow flag zone (if defined)
        # Assuming yellow flag zone is around Mulsanne (example)
        flags = get_flags(5000)  # Mid-Mulsanne
        
        # Flags should be valid bitmask
        self.assertGreaterEqual(flags, 0)
        self.assertLessEqual(flags, 0xFF)

    def test_time_progression(self):
        """Test session time progresses correctly"""
        from lmu_telem_sim import calculate_session_time, LAP_TARGET_S
        
        # At start
        time_start = calculate_session_time(0)
        self.assertAlmostEqual(time_start, 0, delta=1)
        
        # After one lap
        time_lap1 = calculate_session_time(1)
        self.assertAlmostEqual(time_lap1, LAP_TARGET_S, delta=5)
        
        # Session time should increase linearly
        time_lap2 = calculate_session_time(2)
        self.assertAlmostEqual(time_lap2, LAP_TARGET_S * 2, delta=10)


class TestSimulatorIntegration(unittest.TestCase):
    """Integration tests for simulator with actual packet generation"""

    def test_full_lap_simulation(self):
        """Simulate a full lap and verify data consistency"""
        from lmu_telem_sim import build_telemetry_packet, TRACK_LEN_M
        
        # Sample points throughout lap
        num_samples = 100
        for i in range(num_samples):
            distance = (TRACK_LEN_M / num_samples) * i
            lap = int(distance / TRACK_LEN_M) + 1
            
            telem = build_telemetry_packet(distance, 0, lap)
            
            # Verify all required fields exist
            self.assertIn("TelemInfoV01", telem)
            self.assertIn("ScoringInfoV01", telem)
            
            # Verify values are reasonable
            telem_info = telem["TelemInfoV01"]
            self.assertGreater(telem_info["mSpeed"], 0)
            self.assertGreater(telem_info["mRPM"], 0)
            self.assertGreaterEqual(telem_info["mGear"], 0)
            self.assertLessEqual(telem_info["mGear"], 6)
            self.assertGreater(telem_info["mFuel"], 0)

    def test_packet_rate(self):
        """Verify simulator can maintain target packet rate"""
        from lmu_telem_sim import build_telemetry_packet
        
        num_packets = 100
        start_time = time.time()
        
        for i in range(num_packets):
            telem = build_telemetry_packet(i * 100, 0, 1)
            # Simulate encoding
            json.dumps(telem)
        
        elapsed = time.time() - start_time
        packets_per_sec = num_packets / elapsed
        
        # Should be able to generate at least 60 Hz
        self.assertGreater(packets_per_sec, 60, 
                          f"Packet generation too slow: {packets_per_sec:.1f} Hz")


def run_tests():
    """Run all tests"""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    suite.addTests(loader.loadTestsFromTestCase(TestTelemSimulator))
    suite.addTests(loader.loadTestsFromTestCase(TestSimulatorIntegration))
    
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(run_tests())
