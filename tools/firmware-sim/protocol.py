"""
UART frame protocol implementation for rp2040-ffb simulator.
Based on shared/ffb_link.h
"""

import struct
from typing import Optional, Tuple

# Frame constants
SYNC0 = 0xAA
SYNC1 = 0x55
VERSION = 1
MAX_PAYLOAD = 128

# Message types
MSG_PING = 0x01
MSG_PONG = 0x02
MSG_INPUT = 0x10
MSG_TELEMETRY = 0x20
MSG_SHIFT_LED = 0x21
MSG_BTN_LED = 0x22
MSG_DISPLAY = 0x23
MSG_ACCEL_GET = 0x24
MSG_ACCEL_REPORT = 0x25
MSG_CFG_SYNC = 0x30
MSG_CFG_ACK = 0x31
MSG_CFG_GET = 0x32
MSG_CFG_REPORT = 0x33
MSG_CFG_SAVE = 0x34
MSG_VERSION_GET = 0x51
MSG_VERSION_REPORT = 0x52


def crc16_ccitt_false(data: bytes) -> int:
    """
    CRC-16/CCITT-FALSE implementation.
    Polynomial: 0x1021, Init: 0xFFFF, XorOut: 0x0000
    """
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame(msg_type: int, payload: bytes = b"") -> bytes:
    """
    Build a complete UART frame with sync, header, payload, and CRC.
    
    Frame structure:
    | sync0 (0xAA) | sync1 (0x55) | ver | type | len | payload | crc16_le |
    """
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"Payload too large: {len(payload)} > {MAX_PAYLOAD}")
    
    # Build header + payload
    header = struct.pack("BBBB", VERSION, msg_type, len(payload), 0)  # 0 = padding
    data = header[:-1] + payload  # Exclude padding from CRC calculation
    
    # Calculate CRC over version + type + len + payload
    crc = crc16_ccitt_false(data)
    
    # Assemble complete frame
    frame = struct.pack("BB", SYNC0, SYNC1) + data + struct.pack("<H", crc)
    return frame


def parse_frame(frame: bytes) -> Tuple[int, bytes]:
    """
    Parse a received frame and validate CRC.
    
    Returns: (msg_type, payload)
    Raises: ValueError on invalid frame or CRC mismatch
    """
    if len(frame) < 7:  # Min frame: sync(2) + hdr(3) + crc(2)
        raise ValueError(f"Frame too short: {len(frame)} bytes")
    
    # Check sync bytes
    if frame[0] != SYNC0 or frame[1] != SYNC1:
        raise ValueError(f"Invalid sync: {frame[0]:02x} {frame[1]:02x}")
    
    # Parse header
    ver, msg_type, payload_len = struct.unpack("BBB", frame[2:5])
    
    if ver != VERSION:
        raise ValueError(f"Unsupported version: {ver}")
    
    if payload_len > MAX_PAYLOAD:
        raise ValueError(f"Invalid payload length: {payload_len}")
    
    expected_len = 2 + 3 + payload_len + 2  # sync + hdr + payload + crc
    if len(frame) < expected_len:
        raise ValueError(f"Incomplete frame: {len(frame)} < {expected_len}")
    
    # Extract payload
    payload_start = 5
    payload_end = payload_start + payload_len
    payload = frame[payload_start:payload_end]
    
    # Extract CRC
    crc_bytes = frame[payload_end:payload_end + 2]
    if len(crc_bytes) < 2:
        raise ValueError("Missing CRC")
    received_crc = struct.unpack("<H", crc_bytes)[0]
    
    # Verify CRC (over ver + type + len + payload)
    data_for_crc = frame[2:payload_end]
    calculated_crc = crc16_ccitt_false(data_for_crc)
    
    if received_crc != calculated_crc:
        raise ValueError(
            f"CRC mismatch: received {received_crc:04x}, calculated {calculated_crc:04x}"
        )
    
    return msg_type, payload


def frame_to_hex(frame: bytes) -> str:
    """Convert frame to readable hex string for debugging."""
    return " ".join(f"{b:02x}" for b in frame)


# Test vectors for CRC-16/CCITT-FALSE
def _test_crc():
    # Known test vector
    data = b"123456789"
    expected = 0x29B1
    result = crc16_ccitt_false(data)
    assert result == expected, f"CRC test failed: {result:04x} != {expected:04x}"
    print("CRC-16/CCITT-FALSE test passed")


if __name__ == "__main__":
    _test_crc()
    
    # Test frame building and parsing
    payload = b"\x01\x02\x03\x04"
    frame = build_frame(MSG_PING, payload)
    print(f"Built frame: {frame_to_hex(frame)}")
    
    msg_type, parsed_payload = parse_frame(frame)
    assert msg_type == MSG_PING
    assert parsed_payload == payload
    print("Frame parse test passed")
    
    # Test empty payload
    frame = build_frame(MSG_PONG)
    msg_type, parsed_payload = parse_frame(frame)
    assert msg_type == MSG_PONG
    assert len(parsed_payload) == 0
    print("Empty payload test passed")
    
    print("All protocol tests passed!")
