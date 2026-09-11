#pragma once

#include <stdint.h>

#include "ffb_link.h"

namespace AccessoryLink {

void begin();
void update();  // poll UART, maintain link state, expire pulses

bool linked();
uint32_t lastRxMs();

// Combined HID button mask bit0 = Joystick button 1 ... bit31 = button 32
uint32_t hidButtons();

const FfbLink::RimConfig &rimConfig();
void setRimConfig(const FfbLink::RimConfig &cfg);  // update base cache only
void pushRimConfig();     // CfgSync → rim (live apply)
void requestRimConfig();  // CfgGet → expect CfgReport
void saveRimConfig();     // CfgSave → rim EEPROM
void requestRimVersion(); // VersionGet → expect VersionReport
const char *rimFwId();    // cached; empty if unknown

// Multi-page TFT bank (mirrored from rim DisplayStore).
void requestDisplayPages();  // DispOpGetAll
const FfbLink::DispMetaPayload &dispMeta();
const FfbLink::DisplayPage &dispPage(uint8_t i);
bool pushDispMeta();
bool pushDispPage(uint8_t i);
bool setDispPageLayout(uint8_t i, uint8_t bgTheme, uint8_t count,
                       const FfbLink::DisplayElement layout[FfbLink::kDispElementMax]);
bool setDispActivePage(uint8_t page);
bool setDispPageCount(uint8_t n);
void seedDefaultDisplayPages();

// ADXL345 on rim (optional). Soft-fail when absent / unlink.
bool requestAccel(uint8_t mode = FfbLink::AccelOnce, uint8_t count = 0);
bool pollAccel(uint32_t timeoutMs = 80);  // request + wait for AccelReport
const FfbLink::AccelReportPayload &lastAccel();
uint32_t lastAccelMs();  // millis() stamp of last AccelReport (0 = never)
bool adxlPresent();   // from Input flags and/or last AccelReport
int16_t adxlCalibratedX(int16_t offset);  // last ax - offset (0 if no sample)

// Optional rim panel I2C (MCP23017 / ADS1115). Soft-fail when unpopulated.
bool mcpBtnPresent();
bool mcpLedPresent();
bool adsPresent();
void panelAnalog(int16_t out[FfbLink::kAnalogCount]);  // raw; zeros if absent / unlink
// Normalized 0..1 paddle axes (clutch L/R, shifter A/B). Zeros if ADS absent.
void panelAxes(float out[FfbLink::kAnalogCount]);

// Absolute encoder values (0..100) when enc mode is EncModeAbsolute.
int16_t encoderAbs(uint8_t idx);
void setEncoderAbs(uint8_t idx, int16_t value);

void rimResetPulse();
void rimEnterUsbBootloader();  // BOOTSEL + reset (recovery)
bool requestRimEnterUpdater(); // soft ENTER_BOOTLOADER
bool sendShiftLed(const FfbLink::ShiftLedPayload &cmd);
bool sendTelemetry(const FfbLink::TelemetryPayload &tel);
bool sendBtnLed(uint16_t mask);  // manual; disables follow until re-enabled
void setBtnLedFollow(bool on);   // when on, panel LEDs mirror pressed buttons
bool btnLedFollowEnabled();

// Forward a raw framed message already validated, or build helpers:
bool sendMsg(uint8_t type, const void *payload, uint8_t len);

// CDC binary frame demux: feed bytes that are not part of ASCII CLI.
// Returns true if byte was consumed by binary RX (caller should not treat as CLI).
bool feedCdcByte(uint8_t b);
bool cdcForwardActive();  // true while a binary CDC session is in progress
bool otaInProgress();     // rim OTA / updater traffic recently forwarded
void clearCdcSession();   // drop mid-frame CDC state so ASCII CLI works

}  // namespace AccessoryLink
