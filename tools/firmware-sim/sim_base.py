#!/usr/bin/env python3
"""
Base MCU simulator for rp2040-ffb.
Emulates CDC serial interface and responds to commands.
"""

import argparse
import re
import serial
import sys
import time
from typing import Optional, Dict, Any

from protocol import build_frame, parse_frame, MSG_PONG, MSG_VERSION_REPORT


class VirtualEEPROM:
    """Simulated EEPROM storage for settings."""
    
    def __init__(self):
        self.load_defaults()
    
    def load_defaults(self):
        """Load factory default settings."""
        self.settings = {
            "duty_cap": 0.35,
            "spring_k": 0.004,
            "spring_dz": 2.0,
            "torque_cap": 0.35,
            "hid_range": 900.0,
            "gear_ratio": 18.0,
            "soft_limit_en": 0,
            "soft_limit_deg": 450.0,
            "soft_limit_k": 0.02,
            "adxl_cal": 0,
            "adxl_x_offset": 0,
        }
        self.pedal_cal = {
            "thr": {"rest": 2890, "min": 450, "max": 3200},
            "brk": {"rest": 2950, "min": 520, "max": 3150},
            "clu": {"rest": 3100, "min": 1200, "max": 3050},
        }
    
    def get(self, key: str) -> Any:
        return self.settings.get(key)
    
    def set(self, key: str, value: Any) -> bool:
        """Set value with validation."""
        if key not in self.settings:
            return False
        self.settings[key] = value
        return True
    
    def dump(self) -> Dict[str, Any]:
        """Return all settings."""
        result = dict(self.settings)
        # Add pedal calibration
        for axis in ["thr", "brk", "clu"]:
            for param in ["rest", "min", "max"]:
                result[f"{axis}_{param}"] = self.pedal_cal[axis][param]
        return result


class BaseSimulator:
    """Simulates base MCU behavior."""
    
    def __init__(self, port: Optional[str] = None, baudrate: int = 115200):
        self.eeprom = VirtualEEPROM()
        self.axle_deg = 0.0
        self.steer_deg = 0.0
        self.pedal_raw = {"thr": 2890, "brk": 2950, "clu": 3100}
        self.rim_linked = False
        self.rim_fw = "?"
        self.base_fw = "SIM 2024-09-12T00:00:00Z"
        self.running = True
        self.log_enabled = True
        
        self.port = None
        if port:
            try:
                self.port = serial.Serial(port, baudrate, timeout=0.1)
                print(f"Opened serial port {port} at {baudrate} baud")
            except Exception as e:
                print(f"Warning: Could not open port {port}: {e}")
                print("Running in offline mode (console only)")
    
    def print_boot(self):
        """Print boot sequence."""
        lines = [
            "",
            "rp2040-ffb base SIMULATOR v0.1.0",
            "Hall init...",
            "MLX90363 OK (simulated)",
            "Index init...",
            "Pedals init...",
            "Motor init...",
            "Rim UART 460800",
            "Status LEDs init...",
            "HID init...",
            "",
            "Boot INIT: Manual zero at axle=0.0",
            "Press 'h' for help",
            "",
            "OK selftest (simulated)",
            f"hall_ok=1 angle={self.axle_deg:.1f} index=0",
            "pedals: thr=1 brk=1 clu=1",
            "rim_link=0 (simulated)",
            "",
        ]
        for line in lines:
            self.output(line)
    
    def output(self, text: str):
        """Output a line to serial or console."""
        line = text + "\n"
        if self.port:
            try:
                self.port.write(line.encode("utf-8"))
            except Exception as e:
                print(f"Serial write error: {e}")
        print(line, end="")
    
    def handle_command(self, cmd: str) -> str:
        """Process a CDC command and return response."""
        cmd = cmd.strip()
        
        if not cmd:
            return ""
        
        # Single-character commands
        if len(cmd) == 1:
            return self.handle_char_command(cmd)
        
        # Colon-prefixed commands
        if cmd.startswith(":"):
            return self.handle_colon_command(cmd)
        
        return f"Unknown command: {cmd}"
    
    def handle_char_command(self, c: str) -> str:
        """Handle single-character commands."""
        if c in ["h", "H", "?"]:
            return self.print_help()
        elif c in ["z", "Z"]:
            self.axle_deg = 0.0
            self.steer_deg = 0.0
            return "Wheel zeroed"
        elif c in ["p", "P"]:
            # Reset pedal calibration
            for axis in ["thr", "brk", "clu"]:
                self.eeprom.pedal_cal[axis] = {"rest": 0, "min": 0, "max": 0}
            return "Pedal CAL reset — HID=0 until each pedal is pressed once"
        elif c in ["d", "D"]:
            return "Motors DISABLED (simulated)"
        elif c in ["e", "E"]:
            return "Motors ENABLED (simulated)"
        else:
            return ""
    
    def print_help(self) -> str:
        """Return help text."""
        lines = [
            "",
            "rp2040-ffb base-mcu SIMULATOR",
            "  Single keys: h=help  z=zero  p=pedal-reset  e/d=motors",
            "  Commands: :get|:set <key> <v>  :dump  :save  :load  :version",
            "  Simulator: :sim <cmd> [args]",
            "",
        ]
        return "\n".join(lines)
    
    def handle_colon_command(self, cmd: str) -> str:
        """Handle colon-prefixed commands."""
        parts = cmd[1:].split()
        if not parts:
            return "Empty command"
        
        action = parts[0].lower()
        
        if action == "version" or action == "ver":
            return self.cmd_version()
        elif action == "dump":
            return self.cmd_dump()
        elif action == "get" and len(parts) >= 2:
            return self.cmd_get(parts[1])
        elif action == "set" and len(parts) >= 3:
            return self.cmd_set(parts[1], " ".join(parts[2:]))
        elif action == "save":
            return "Settings saved to virtual EEPROM"
        elif action == "load":
            self.eeprom.load_defaults()
            return "Settings loaded from virtual EEPROM"
        elif action == "defaults":
            self.eeprom.load_defaults()
            return "Factory defaults loaded"
        elif action == "selftest":
            return self.cmd_selftest()
        elif action == "log" and len(parts) >= 2:
            self.log_enabled = (parts[1] == "1")
            return f"Logging {'enabled' if self.log_enabled else 'disabled'}"
        elif action == "sim":
            return self.cmd_sim(parts[1:])
        else:
            return f"Unknown command: :{action}"
    
    def cmd_version(self) -> str:
        """Return firmware versions."""
        return f"base_fw={self.base_fw}\nrim_fw={self.rim_fw}"
    
    def cmd_dump(self) -> str:
        """Dump all settings."""
        lines = [f"base_fw={self.base_fw}", f"rim_fw={self.rim_fw}"]
        
        settings = self.eeprom.dump()
        for key in sorted(settings.keys()):
            value = settings[key]
            if isinstance(value, float):
                lines.append(f"{key}={value:.6g}")
            else:
                lines.append(f"{key}={value}")
        
        lines.append(f"rim_link={1 if self.rim_linked else 0}")
        lines.append(f"adxl_present=0")
        
        return "\n".join(lines)
    
    def cmd_get(self, key: str) -> str:
        """Get a single setting."""
        if key == "base_fw":
            return f"base_fw={self.base_fw}"
        elif key == "rim_fw":
            return f"rim_fw={self.rim_fw}"
        elif key == "rim_link":
            return f"rim_link={1 if self.rim_linked else 0}"
        
        value = self.eeprom.get(key)
        if value is None:
            # Check pedal calibration
            match = re.match(r"(thr|brk|clu)_(rest|min|max)", key)
            if match:
                axis, param = match.groups()
                value = self.eeprom.pedal_cal[axis][param]
            else:
                return f"Unknown key: {key}"
        
        if isinstance(value, float):
            return f"{key}={value:.6g}"
        else:
            return f"{key}={value}"
    
    def cmd_set(self, key: str, value_str: str) -> str:
        """Set a setting with validation."""
        try:
            value = float(value_str)
        except ValueError:
            return f"Invalid value: {value_str}"
        
        # Validate ranges
        if key == "duty_cap":
            if value < 0.0 or value > 1.0:
                return f"ERROR: duty_cap out of range [0.0, 1.0]"
            if value > 0.50:
                self.output(f"WARNING: duty_cap > 0.50 - USE CAUTION!")
        elif key == "torque_cap":
            if value < 0.0 or value > 1.0:
                return f"ERROR: torque_cap out of range [0.0, 1.0]"
        elif key == "gear_ratio":
            if value < 5.0 or value > 50.0:
                return f"ERROR: gear_ratio out of range [5.0, 50.0]"
        
        if self.eeprom.set(key, value):
            return f"{key}={value:.6g}"
        else:
            return f"Unknown key: {key}"
    
    def cmd_selftest(self) -> str:
        """Run self-test."""
        lines = [
            "OK selftest (simulated)",
            f"hall_ok=1 angle={self.axle_deg:.1f} index=0",
            "pedals: thr=1 brk=1 clu=1",
            f"rim_link={1 if self.rim_linked else 0}",
            "OK end",
        ]
        return "\n".join(lines)
    
    def cmd_sim(self, args: list) -> str:
        """Simulator-specific commands."""
        if not args:
            return "Simulator commands: axle <deg>, steer <deg>, pedal <axis> <raw>, rimlink <0|1>"
        
        cmd = args[0].lower()
        
        if cmd == "axle" and len(args) >= 2:
            self.axle_deg = float(args[1])
            return f"Axle set to {self.axle_deg:.1f}°"
        elif cmd == "steer" and len(args) >= 2:
            self.steer_deg = float(args[1])
            return f"Steering set to {self.steer_deg:.1f}°"
        elif cmd == "pedal" and len(args) >= 3:
            axis = args[1].lower()
            if axis not in self.pedal_raw:
                return f"Unknown axis: {axis} (use thr/brk/clu)"
            self.pedal_raw[axis] = int(args[2])
            return f"{axis} raw={self.pedal_raw[axis]}"
        elif cmd == "rimlink" and len(args) >= 2:
            self.rim_linked = (args[1] == "1")
            if self.rim_linked:
                self.rim_fw = "SIM 2024-09-12T00:00:00Z"
            else:
                self.rim_fw = "?"
            return f"Rim link {'enabled' if self.rim_linked else 'disabled'}"
        else:
            return f"Unknown simulator command: {cmd}"
    
    def tick(self):
        """Main loop iteration - output telemetry."""
        if not self.log_enabled:
            return
        
        thr = self.pedal_raw["thr"]
        brk = self.pedal_raw["brk"]
        clu = self.pedal_raw["clu"]
        
        line = (
            f"T axle={self.axle_deg:.1f} steer={self.steer_deg:.1f} "
            f"rpm=0 gear=0 flags=0 thr={thr} brk={brk} clu={clu}"
        )
        self.output(line)
    
    def run(self):
        """Main simulator loop."""
        self.print_boot()
        
        last_tick = time.time()
        tick_interval = 1.0  # Telemetry every 1 second
        
        try:
            while self.running:
                # Check for input
                if self.port:
                    try:
                        if self.port.in_waiting:
                            line = self.port.readline().decode("utf-8").strip()
                            if line:
                                response = self.handle_command(line)
                                if response:
                                    self.output(response)
                    except Exception as e:
                        print(f"Serial error: {e}")
                else:
                    # Console mode - non-blocking input would need select/msvcrt
                    pass
                
                # Output telemetry periodically
                now = time.time()
                if now - last_tick >= tick_interval:
                    self.tick()
                    last_tick = now
                
                time.sleep(0.01)  # 10ms loop
                
        except KeyboardInterrupt:
            self.output("\nSimulator stopped")
        finally:
            if self.port:
                self.port.close()


def main():
    parser = argparse.ArgumentParser(description="rp2040-ffb base MCU simulator")
    parser.add_argument("--port", help="Serial port (e.g. /dev/pts/4 or COM10)")
    parser.add_argument("--baudrate", type=int, default=115200, help="Baud rate")
    args = parser.parse_args()
    
    sim = BaseSimulator(port=args.port, baudrate=args.baudrate)
    
    if not args.port:
        print("No serial port specified - running in console test mode")
        print("Try: python sim_base.py --port /dev/pts/4")
        print()
        print("Testing commands:")
        print()
        sim.print_boot()
        print(sim.handle_command(":version"))
        print(sim.handle_command(":get duty_cap"))
        print(sim.handle_command(":set duty_cap 0.40"))
        print(sim.handle_command(":dump"))
    else:
        sim.run()


if __name__ == "__main__":
    main()
