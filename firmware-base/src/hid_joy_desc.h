#pragma once

/**
 * Desktop Joystick input only (no USB HID PID FFB).
 * Same report ID 1 layout as the PID composite input block.
 */

#include <stddef.h>
#include <stdint.h>

namespace HidJoy {

static const uint8_t kHidReportDescriptor[] = {
    0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
    0x09, 0x04,       // USAGE (Joystick)
    0xA1, 0x01,       // COLLECTION (Application)
    0x85, 0x01,       //   REPORT_ID (1)
    0x05, 0x09,       //   USAGE_PAGE (Button)
    0x19, 0x01,       //   USAGE_MINIMUM (Button 1)
    0x29, 0x20,       //   USAGE_MAXIMUM (Button 32)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x25, 0x01,       //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,       //   REPORT_SIZE (1)
    0x95, 0x20,       //   REPORT_COUNT (32)
    0x81, 0x02,       //   INPUT (Data,Var,Abs)
    0x05, 0x01,       //   USAGE_PAGE (Generic Desktop)
    0x09, 0x39,       //   USAGE (Hat switch)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x25, 0x07,       //   LOGICAL_MAXIMUM (7)
    0x35, 0x00,       //   PHYSICAL_MINIMUM (0)
    0x46, 0x3B, 0x01, //   PHYSICAL_MAXIMUM (315)
    0x65, 0x14,       //   UNIT (Eng Rot:Angular Pos)
    0x75, 0x04,       //   REPORT_SIZE (4)
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x81, 0x42,       //   INPUT (Data,Var,Abs,Null) — value 8 = neutral
    0x75, 0x04,       //   REPORT_SIZE (4)
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x81, 0x03,       //   INPUT (Const,Var,Abs)
    0x09, 0x01,       //   USAGE (Pointer)
    0x16, 0x01, 0x80, //   LOGICAL_MINIMUM (-32767)
    0x26, 0xFF, 0x7F, //   LOGICAL_MAXIMUM (32767)
    0x75, 0x10,       //   REPORT_SIZE (16)
    0x95, 0x06,       //   REPORT_COUNT (6)
    0xA1, 0x00,       //   COLLECTION (Physical)
    0x09, 0x30,       //     USAGE (X)
    0x09, 0x31,       //     USAGE (Y)
    0x09, 0x32,       //     USAGE (Z)
    0x09, 0x33,       //     USAGE (Rx)
    0x09, 0x34,       //     USAGE (Ry)
    0x09, 0x35,       //     USAGE (Rz)
    0x81, 0x02,       //     INPUT (Data,Var,Abs)
    0xC0,             //   END_COLLECTION
    0xC0,             // END_COLLECTION (Application)
};

static constexpr size_t kHidReportDescriptorLen = sizeof(kHidReportDescriptor);

} // namespace HidJoy
