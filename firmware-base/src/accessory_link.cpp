#include "accessory_link.h"

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "ffb_version.h"
#include "safety.h"
#include "settings.h"

namespace AccessoryLink {
namespace {

HardwareSerial& Uart = Serial1;
FfbLink::ByteRing<FfbLink::kRxRingSize> rxRing;

enum class RxState : uint8_t { Sync0, Sync1, Ver, Type, Len, Payload, Crc0, Crc1 };

RxState rxState = RxState::Sync0;
uint8_t rxVer = 0;
uint8_t rxType = 0;
uint8_t rxLen = 0;
uint8_t rxPayload[FfbLink::kMaxPayload];
uint8_t rxIdx = 0;
uint8_t rxCrcLo = 0;
uint8_t hdrBuf[3]; // ver, type, len for CRC

FfbLink::RimConfig rimCfg{};
FfbLink::DispMetaPayload dispMeta_{1, 0};
FfbLink::DisplayPage dispPages_[FfbLink::kDispPageMax]{};
char rimFwIdBuf[FfbVersion::kIdMax] = {};
uint32_t lastRx = 0;
bool haveLink = false;
bool adxlFlagPresent = false;
FfbLink::AccelReportPayload lastAccelReport{};
uint32_t accelReportMs = 0;

uint32_t panelBits = 0;
uint8_t encSwitchBits = 0;
int16_t panelAnalogRaw[FfbLink::kAnalogCount] = {};
bool adsFlagPresent = false;
bool mcpBtnFlagPresent = false;
bool mcpLedFlagPresent = false;
uint32_t pulseUntil[32] = {};

// Pending encoder deltas converted to HID pulses on base (rim may also pulse;
// we accept InputPayload.buttons for panel + switches, and encDelta for encoders).
int16_t encAccum[FfbLink::kEncoderCount] = {};
int16_t encAbs[FfbLink::kEncoderCount] = {}; // 0..100 for EncModeAbsolute
bool btnLedFollow = true;
uint16_t lastBtnLedSent = 0xFFFF;
uint16_t pendingBtnLed = 0xFFFF; // != last → send from update()
bool pendingBtnLedValid = false;

float adsRawToUnit(int16_t raw) {
    // ADS1115 single-ended vs GND, ±4.096 V → 0..+FS ≈ 0..32767.
    if (raw <= 0)
        return 0.0f;
    float n = (float)raw / 32767.0f;
    if (n > 1.0f)
        n = 1.0f;
    return n;
}

void forwardToHost(uint8_t type, const uint8_t* payload, uint8_t len);
void noteOtaTraffic(uint8_t type);

void releaseControlPin(int pin) {
    pinMode(pin, INPUT);
}

void assertControlPin(int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void pulseHidButton(uint8_t zeroBased, uint16_t ms) {
    if (zeroBased >= 32)
        return;
    const uint32_t until = millis() + ms;
    if (until > pulseUntil[zeroBased])
        pulseUntil[zeroBased] = until;
}

void holdHidButton(uint8_t zeroBased, uint16_t idleMs) {
    if (zeroBased >= 32)
        return;
    const uint16_t hold = idleMs < 30 ? 30 : idleMs;
    pulseUntil[zeroBased] = millis() + hold;
}

void applyEncoderDeltas(const FfbLink::InputPayload& in) {
    for (uint8_t i = 0; i < FfbLink::kEncoderCount; ++i) {
        int8_t d = in.encDelta[i];
        if (d == 0)
            continue;
        const FfbLink::EncoderConfig& ec = rimCfg.enc[i];
        if (ec.invert)
            d = (int8_t)(-d);

        // Quick menu: hold that encoder's shaft switch + turn.
        // enc0 → profile next/prev · enc1 → shift LED bright · enc2 → panel LED · enc3 → HID range
        // ±90°
        if (in.encSwitch & (1u << i)) {
            const int steps = ec.stepsPerClick < 1 ? 1 : ec.stepsPerClick;
            encAccum[i] = (int16_t)(encAccum[i] + d);
            while (encAccum[i] >= steps || encAccum[i] <= -steps) {
                const bool cw = encAccum[i] > 0;
                if (cw) {
                    encAccum[i] = (int16_t)(encAccum[i] - steps);
                } else {
                    encAccum[i] = (int16_t)(encAccum[i] + steps);
                }
                if (i == 0) {
                    Settings::quickProfileStep(cw);
                } else if (i == 1 || i == 2) {
                    Settings::quickBrightStep(i == 1, cw);
                } else if (i == 3) {
                    Settings::quickRangeStep(cw);
                }
            }
            continue;
        }

        encAccum[i] = (int16_t)(encAccum[i] + d);

        const int steps = ec.stepsPerClick < 1 ? 1 : ec.stepsPerClick;
        const uint8_t cwBtn = (uint8_t)(FfbLink::kHidEncCwFirst - 1 + i * 2);
        const uint8_t ccwBtn = (uint8_t)(FfbLink::kHidEncCcwFirst - 1 + i * 2);
        const uint8_t mode = ec.mode;

        while (encAccum[i] >= steps || encAccum[i] <= -steps) {
            const bool cw = encAccum[i] > 0;
            if (cw) {
                encAccum[i] = (int16_t)(encAccum[i] - steps);
            } else {
                encAccum[i] = (int16_t)(encAccum[i] + steps);
            }

            if (mode == FfbLink::EncModeAbsolute) {
                int16_t v = encAbs[i];
                v = (int16_t)(v + (cw ? 1 : -1));
                if (v < 0)
                    v = 0;
                if (v > 100)
                    v = 100;
                encAbs[i] = v;
                continue;
            }

            if (mode == FfbLink::EncModeHold) {
                const uint8_t idle = ec.pulseMs < 30 ? 80 : ec.pulseMs;
                pulseUntil[cw ? ccwBtn : cwBtn] = 0;
                holdHidButton(cw ? cwBtn : ccwBtn, idle);
                continue;
            }

            int mult = 1;
            if (ec.accelEnable && ec.accelMaxMult > 1) {
                const int ad = d < 0 ? -d : d;
                if (ad >= (int)ec.accelThreshold && ec.accelThreshold > 0) {
                    mult = ec.accelMaxMult;
                }
            }
            const uint8_t pulseMs = ec.pulseMs < 5 ? 5 : ec.pulseMs;
            for (int m = 0; m < mult; ++m) {
                pulseHidButton(cw ? cwBtn : ccwBtn, pulseMs);
            }
        }
    }
}

void handleFrame(uint8_t type, const uint8_t* payload, uint8_t len) {
    lastRx = millis();
    haveLink = true;

    // Notify watchdog of rim activity
    Safety::gCommWatchdog.notifyRimActivity();

    if (type == FfbLink::UpdaterReady || type == FfbLink::FwAck || type == FfbLink::FwNak ||
        type == FfbLink::FwDone || type == FfbLink::FwFail) {
        noteOtaTraffic(type);
        forwardToHost(type, payload, len);
    }

    if (type == FfbLink::Pong) {
        return;
    }
    // Core fields (buttons/enc/flags) are enough; analog[] is optional on older rims.
    if (type == FfbLink::Input && len >= FfbLink::kInputPayloadCoreSize) {
        FfbLink::InputPayload in{};
        const uint8_t n = len < sizeof(in) ? len : (uint8_t)sizeof(in);
        memcpy(&in, payload, n);
        panelBits = in.buttons & ((1u << FfbLink::kHidPanelBtnCount) - 1u);
        encSwitchBits = in.encSwitch;
        adxlFlagPresent = (in.flags & FfbLink::InputAdxlPresent) != 0;
        mcpBtnFlagPresent = (in.flags & FfbLink::InputMcpBtnPresent) != 0;
        mcpLedFlagPresent = (in.flags & FfbLink::InputMcpLedPresent) != 0;
        adsFlagPresent = (in.flags & FfbLink::InputAdsPresent) != 0;
        if (n >= sizeof(in)) {
            memcpy(panelAnalogRaw, in.analog, sizeof(panelAnalogRaw));
        } else {
            memset(panelAnalogRaw, 0, sizeof(panelAnalogRaw));
            adsFlagPresent = false;
        }
        applyEncoderDeltas(in);
        if (btnLedFollow) {
            const uint16_t mask = (uint16_t)(panelBits & 0x3FFu);
            if (mask != lastBtnLedSent) {
                pendingBtnLed = mask;
                pendingBtnLedValid = true;
            }
        }
        return;
    }
    if (type == FfbLink::AccelReport && len >= sizeof(FfbLink::AccelReportPayload)) {
        memcpy(&lastAccelReport, payload, sizeof(lastAccelReport));
        accelReportMs = millis();
        if (lastAccelReport.present)
            adxlFlagPresent = true;
        return;
    }
    if (type == FfbLink::CfgReport && len >= sizeof(FfbLink::RimConfig)) {
        memcpy(&rimCfg, payload, sizeof(rimCfg));
        return;
    }
    if (type == FfbLink::Display && len >= 1) {
        const uint8_t op = payload[0];
        if (op == FfbLink::DispOpSetMeta && len >= 1 + sizeof(FfbLink::DispMetaPayload)) {
            memcpy(&dispMeta_, payload + 1, sizeof(dispMeta_));
            if (dispMeta_.pageCount < 1)
                dispMeta_.pageCount = 1;
            if (dispMeta_.pageCount > FfbLink::kDispPageMax)
                dispMeta_.pageCount = FfbLink::kDispPageMax;
            if (dispMeta_.activePage >= dispMeta_.pageCount)
                dispMeta_.activePage = 0;
            return;
        }
        if (op == FfbLink::DispOpSetPageChunk &&
            len >= 1 + offsetof(FfbLink::DispPageChunkPayload, elements)) {
            FfbLink::DispPageChunkPayload chunk{};
            const uint8_t n = len - 1 < sizeof(chunk) ? (uint8_t)(len - 1) : (uint8_t)sizeof(chunk);
            memcpy(&chunk, payload + 1, n);
            if (chunk.pageIndex >= FfbLink::kDispPageMax)
                return;
            uint8_t total = chunk.layoutCount;
            if (total > FfbLink::kDispElementMax)
                total = FfbLink::kDispElementMax;
            uint8_t count = chunk.count;
            if (count > FfbLink::kDispChunkElements)
                count = FfbLink::kDispChunkElements;
            if (chunk.start >= FfbLink::kDispElementMax)
                return;
            if ((uint16_t)chunk.start + count > FfbLink::kDispElementMax) {
                count = (uint8_t)(FfbLink::kDispElementMax - chunk.start);
            }
            FfbLink::DisplayPage& dst = dispPages_[chunk.pageIndex];
            if (chunk.start == 0) {
                dst = FfbLink::DisplayPage{};
                dst.bgTheme = chunk.bgTheme;
                dst.layoutCount = total;
            }
            memcpy(dst.layout + chunk.start, chunk.elements,
                   count * sizeof(FfbLink::DisplayElement));
            return;
        }
        if (op == FfbLink::DispOpSetPage && len >= 1 + sizeof(FfbLink::DispPageSetPayload)) {
            FfbLink::DispPageSetPayload page{};
            memcpy(&page, payload + 1, sizeof(page));
            if (page.pageIndex >= FfbLink::kDispPageMax)
                return;
            dispPages_[page.pageIndex] = FfbLink::DisplayPage{};
            dispPages_[page.pageIndex].bgTheme = page.bgTheme;
            dispPages_[page.pageIndex].layoutCount = page.layoutCount > 8 ? 8 : page.layoutCount;
            memcpy(dispPages_[page.pageIndex].layout, page.layout,
                   dispPages_[page.pageIndex].layoutCount * sizeof(FfbLink::DisplayElement));
            return;
        }
    }
    if (type == FfbLink::VersionReport && len > 0) {
        const uint8_t n = len < sizeof(rimFwIdBuf) - 1 ? len : (uint8_t)(sizeof(rimFwIdBuf) - 1);
        memcpy(rimFwIdBuf, payload, n);
        rimFwIdBuf[n] = '\0';
        return;
    }
    if (type == FfbLink::CfgAck) {
        return;
    }
}

void resetRx() {
    rxState = RxState::Sync0;
}

void feedUartByte(uint8_t b) {
    switch (rxState) {
    case RxState::Sync0:
        if (b == FfbLink::kSync0)
            rxState = RxState::Sync1;
        break;
    case RxState::Sync1:
        rxState = (b == FfbLink::kSync1) ? RxState::Ver : RxState::Sync0;
        if (b == FfbLink::kSync0)
            rxState = RxState::Sync1;
        break;
    case RxState::Ver:
        rxVer = b;
        hdrBuf[0] = b;
        rxState = (b == FfbLink::kVersion) ? RxState::Type : RxState::Sync0;
        break;
    case RxState::Type:
        rxType = b;
        hdrBuf[1] = b;
        rxState = RxState::Len;
        break;
    case RxState::Len:
        rxLen = b;
        hdrBuf[2] = b;
        rxIdx = 0;
        if (rxLen > FfbLink::kMaxPayload) {
            resetRx();
            break;
        }
        rxState = rxLen ? RxState::Payload : RxState::Crc0;
        break;
    case RxState::Payload:
        rxPayload[rxIdx++] = b;
        if (rxIdx >= rxLen)
            rxState = RxState::Crc0;
        break;
    case RxState::Crc0:
        rxCrcLo = b;
        rxState = RxState::Crc1;
        break;
    case RxState::Crc1: {
        uint8_t crcbuf[3 + FfbLink::kMaxPayload];
        memcpy(crcbuf, hdrBuf, 3);
        if (rxLen)
            memcpy(crcbuf + 3, rxPayload, rxLen);
        const uint16_t expect = FfbLink::crc16(crcbuf, (uint16_t)(3 + rxLen));
        const uint16_t got = (uint16_t)rxCrcLo | ((uint16_t)b << 8);
        if (got == expect) {
            handleFrame(rxType, rxPayload, rxLen);
        }
        resetRx();
        break;
    }
    }
}

uint32_t lastPingMs = 0;
uint32_t otaQuietUntilMs = 0;
uint32_t otaLedUntilMs = 0; // status LED window (shorter than ping quiet)
bool wasLinked = false;

void noteOtaTraffic(uint8_t type) {
    const uint32_t now = millis();
    otaQuietUntilMs = now + 15000;
    // Hold LED through chunk gaps; clear soon after Done/Fail.
    if (type == FfbLink::FwDone || type == FfbLink::FwFail) {
        otaLedUntilMs = now + 800;
    } else {
        otaLedUntilMs = now + 3000;
    }
}

// Minimal CDC binary RX for FW forward (sync-framed). For now only detects
// frames and forwards identical bytes to UART once a full valid frame is in.
RxState cdcState = RxState::Sync0;
uint8_t cdcBuf[2 + 3 + FfbLink::kMaxPayload + 2];
uint8_t cdcLen = 0;
uint8_t cdcNeed = 0;
bool cdcActive = false;
uint32_t cdcLastMs = 0;

void forwardToHost(uint8_t type, const uint8_t* payload, uint8_t len) {
    // ASCII only — binary+ASCII twin caused the GUI to see duplicate ACKs
    // (begin's offset=0 leftover failed the first data chunk).
    Serial.print("OK FW ");
    Serial.print(type, HEX);
    for (uint8_t i = 0; i < len; ++i) {
        Serial.print(' ');
        if (payload[i] < 16)
            Serial.print('0');
        Serial.print(payload[i], HEX);
    }
    Serial.println();
    Serial.flush();
}

} // namespace

void seedDefaultDisplayPages();

void begin() {
    FfbLink::defaultRimConfig(rimCfg);
    seedDefaultDisplayPages();
    releaseControlPin(PIN_RIM_RESET);
    releaseControlPin(PIN_RIM_BOOTSEL);

    // Software RX FIFO + ring buffer absorb 460.8 kbaud bursts / OTA chunks.
    Serial1.setFIFOSize(FfbLink::kUartFifoSize);
    Uart.begin(FfbLink::kBaud);
    rxRing.clear();
}

bool sendMsg(uint8_t type, const void* payload, uint8_t len) {
    if (len > FfbLink::kMaxPayload)
        return false;
    uint8_t body[3 + FfbLink::kMaxPayload];
    body[0] = FfbLink::kVersion;
    body[1] = type;
    body[2] = len;
    if (len && payload)
        memcpy(body + 3, payload, len);
    const uint16_t crc = FfbLink::crc16(body, (uint16_t)(3 + len));
    Uart.write(FfbLink::kSync0);
    Uart.write(FfbLink::kSync1);
    Uart.write(body, 3 + len);
    Uart.write((uint8_t)(crc & 0xFF));
    Uart.write((uint8_t)(crc >> 8));
    return true;
}

void drainUartToRing() {
    while (Uart.available()) {
        const uint8_t b = (uint8_t)Uart.read();
        if (!rxRing.push(b)) {
            uint8_t discard = 0;
            (void)rxRing.pop(discard);
            (void)rxRing.push(b);
        }
    }
}

void update() {
    drainUartToRing();
    uint8_t b = 0;
    while (rxRing.pop(b)) {
        feedUartByte(b);
    }

    if (pendingBtnLedValid && haveLink) {
        FfbLink::BtnLedPayload led{pendingBtnLed};
        if (sendMsg(FfbLink::BtnLed, &led, sizeof(led))) {
            lastBtnLedSent = pendingBtnLed;
            pendingBtnLedValid = false;
        }
    }

    const uint32_t now = millis();
    if (haveLink && (now - lastRx) > FfbLink::kLinkTimeoutMs) {
        haveLink = false;
        panelBits = 0;
        encSwitchBits = 0;
        adxlFlagPresent = false;
        mcpBtnFlagPresent = false;
        mcpLedFlagPresent = false;
        adsFlagPresent = false;
        memset(panelAnalogRaw, 0, sizeof(panelAnalogRaw));
        lastAccelReport = FfbLink::AccelReportPayload{};
        accelReportMs = 0;
        memset(pulseUntil, 0, sizeof(pulseUntil));
        rimFwIdBuf[0] = '\0';
    }

    // Rim came back — pull EEPROM settings + firmware id (rim is source of truth).
    if (haveLink && !wasLinked) {
        requestRimConfig();
        requestRimVersion();
    }
    wasLinked = haveLink;

    if (now - lastPingMs >= 200) {
        lastPingMs = now;
        // Don't jabber on UART during rim OTA / CDC binary sessions.
        if (!(cdcActive || (int32_t)(now - otaQuietUntilMs) < 0)) {
            sendMsg(FfbLink::Ping, nullptr, 0);
        }
    }

    if (cdcActive && (now - cdcLastMs) > 2000) {
        cdcActive = false;
        cdcState = RxState::Sync0;
    }
}

bool linked() {
    return haveLink;
}
uint32_t lastRxMs() {
    return lastRx;
}

uint32_t hidButtons() {
    uint32_t mask = 0;
    // Panel 1..10 → bits 0..9
    mask |= panelBits & ((1u << FfbLink::kHidPanelBtnCount) - 1u);
    // Encoder switches → buttons 19..22 → bits 18..21
    for (uint8_t i = 0; i < FfbLink::kEncoderCount; ++i) {
        if (encSwitchBits & (1u << i)) {
            mask |= 1u << (FfbLink::kHidEncSwitchFirst - 1 + i);
        }
    }
    // Shifter paddles (ADS A2/A3) → buttons 23/24 when pressed past threshold.
    // No ADS → stay released (axis path also reports 0).
    if (adsFlagPresent) {
        constexpr float kShiftThresh = 0.35f;
        if (adsRawToUnit(panelAnalogRaw[FfbLink::kAnalogShifterA]) >= kShiftThresh) {
            mask |= 1u << (FfbLink::kHidShifterABtn - 1);
        }
        if (adsRawToUnit(panelAnalogRaw[FfbLink::kAnalogShifterB]) >= kShiftThresh) {
            mask |= 1u << (FfbLink::kHidShifterBBtn - 1);
        }
    }
    const uint32_t now = millis();
    for (uint8_t i = 0; i < 32; ++i) {
        if (pulseUntil[i] && (int32_t)(now - pulseUntil[i]) < 0) {
            mask |= 1u << i;
        } else {
            pulseUntil[i] = 0;
        }
    }
    return mask;
}

const FfbLink::RimConfig& rimConfig() {
    return rimCfg;
}

int16_t encoderAbs(uint8_t idx) {
    if (idx >= FfbLink::kEncoderCount)
        return 0;
    return encAbs[idx];
}

void setEncoderAbs(uint8_t idx, int16_t value) {
    if (idx >= FfbLink::kEncoderCount)
        return;
    if (value < 0)
        value = 0;
    if (value > 100)
        value = 100;
    encAbs[idx] = value;
}

void setRimConfig(const FfbLink::RimConfig& cfg) {
    rimCfg = cfg;
}

void pushRimConfig() {
    sendMsg(FfbLink::CfgSync, &rimCfg, sizeof(rimCfg));
}

void requestRimConfig() {
    sendMsg(FfbLink::CfgGet, nullptr, 0);
}

void saveRimConfig() {
    sendMsg(FfbLink::CfgSave, nullptr, 0);
}

void seedDefaultDisplayPages() {
    dispMeta_.pageCount = FfbLink::kDispPageMax;
    dispMeta_.activePage = 0;
    FfbLink::defaultDisplayPage0(dispPages_[0]);
    FfbLink::defaultDisplayPage1(dispPages_[1]);
    FfbLink::defaultDisplayPage2(dispPages_[2]);
}

void requestDisplayPages() {
    uint8_t op = FfbLink::DispOpGetAll;
    sendMsg(FfbLink::Display, &op, 1);
}

const FfbLink::DispMetaPayload& dispMeta() {
    return dispMeta_;
}

const FfbLink::DisplayPage& dispPage(uint8_t i) {
    if (i >= FfbLink::kDispPageMax)
        i = 0;
    return dispPages_[i];
}

bool pushDispMeta() {
    uint8_t buf[1 + sizeof(FfbLink::DispMetaPayload)];
    buf[0] = FfbLink::DispOpSetMeta;
    memcpy(buf + 1, &dispMeta_, sizeof(dispMeta_));
    return sendMsg(FfbLink::Display, buf, sizeof(buf));
}

bool pushDispPage(uint8_t i) {
    if (i >= FfbLink::kDispPageMax)
        return false;
    const FfbLink::DisplayPage& src = dispPages_[i];
    uint8_t start = 0;
    bool ok = true;
    do {
        FfbLink::DispPageChunkPayload chunk{};
        chunk.pageIndex = i;
        chunk.bgTheme = src.bgTheme;
        chunk.layoutCount = src.layoutCount;
        chunk.start = start;
        uint8_t remain = 0;
        if (src.layoutCount > start)
            remain = (uint8_t)(src.layoutCount - start);
        chunk.count = remain > FfbLink::kDispChunkElements ? FfbLink::kDispChunkElements : remain;
        if (chunk.count > 0) {
            memcpy(chunk.elements, src.layout + start,
                   chunk.count * sizeof(FfbLink::DisplayElement));
        }
        const uint8_t payloadLen = (uint8_t)(offsetof(FfbLink::DispPageChunkPayload, elements) +
                                             chunk.count * sizeof(FfbLink::DisplayElement));
        uint8_t buf[1 + sizeof(FfbLink::DispPageChunkPayload)];
        buf[0] = FfbLink::DispOpSetPageChunk;
        memcpy(buf + 1, &chunk, payloadLen);
        ok = sendMsg(FfbLink::Display, buf, (uint8_t)(1 + payloadLen)) && ok;
        if (src.layoutCount == 0)
            break;
        start = (uint8_t)(start + chunk.count);
    } while (start < src.layoutCount);
    return ok;
}

bool setDispPageLayout(uint8_t i, uint8_t bgTheme, uint8_t count,
                       const FfbLink::DisplayElement layout[FfbLink::kDispElementMax]) {
    if (i >= FfbLink::kDispPageMax)
        return false;
    dispPages_[i].bgTheme = bgTheme;
    dispPages_[i].layoutCount = count > FfbLink::kDispElementMax ? FfbLink::kDispElementMax : count;
    memcpy(dispPages_[i].layout, layout, sizeof(dispPages_[i].layout));
    return pushDispPage(i);
}

bool setDispActivePage(uint8_t page) {
    if (page >= dispMeta_.pageCount)
        return false;
    dispMeta_.activePage = page;
    return pushDispMeta();
}

bool setDispPageCount(uint8_t n) {
    if (n < 1)
        n = 1;
    if (n > FfbLink::kDispPageMax)
        n = FfbLink::kDispPageMax;
    dispMeta_.pageCount = n;
    if (dispMeta_.activePage >= n)
        dispMeta_.activePage = 0;
    return pushDispMeta();
}

void requestRimVersion() {
    sendMsg(FfbLink::VersionGet, nullptr, 0);
}

const char* rimFwId() {
    return rimFwIdBuf;
}

bool requestAccel(uint8_t mode, uint8_t count) {
    FfbLink::AccelGetPayload req{};
    req.mode = mode;
    req.count = count;
    return sendMsg(FfbLink::AccelGet, &req, sizeof(req));
}

bool pollAccel(uint32_t timeoutMs) {
    const uint32_t before = accelReportMs;
    if (!requestAccel(FfbLink::AccelOnce, 0))
        return false;
    const uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        update();
        if (accelReportMs != before)
            return lastAccelReport.present && lastAccelReport.ok;
        delay(1);
    }
    return false;
}

const FfbLink::AccelReportPayload& lastAccel() {
    return lastAccelReport;
}

uint32_t lastAccelMs() {
    return accelReportMs;
}

bool adxlPresent() {
    if (!haveLink)
        return false;
    if (adxlFlagPresent)
        return true;
    return lastAccelReport.present != 0 && (millis() - accelReportMs) < 2000;
}

int16_t adxlCalibratedX(int16_t offset) {
    if (!lastAccelReport.ok)
        return 0;
    return (int16_t)(lastAccelReport.ax - offset);
}

bool mcpBtnPresent() {
    return haveLink && mcpBtnFlagPresent;
}
bool mcpLedPresent() {
    return haveLink && mcpLedFlagPresent;
}
bool adsPresent() {
    return haveLink && adsFlagPresent;
}

void panelAnalog(int16_t out[FfbLink::kAnalogCount]) {
    if (!out)
        return;
    if (!haveLink || !adsFlagPresent) {
        memset(out, 0, sizeof(int16_t) * FfbLink::kAnalogCount);
        return;
    }
    memcpy(out, panelAnalogRaw, sizeof(panelAnalogRaw));
}

void panelAxes(float out[FfbLink::kAnalogCount]) {
    if (!out)
        return;
    for (uint8_t i = 0; i < FfbLink::kAnalogCount; ++i) {
        out[i] = 0.0f;
    }
    if (!haveLink || !adsFlagPresent)
        return;
    for (uint8_t i = 0; i < FfbLink::kAnalogCount; ++i) {
        out[i] = adsRawToUnit(panelAnalogRaw[i]);
    }
}

void rimResetPulse() {
    assertControlPin(PIN_RIM_RESET);
    delay(20);
    releaseControlPin(PIN_RIM_RESET);
}

void rimEnterUsbBootloader() {
    assertControlPin(PIN_RIM_BOOTSEL);
    delay(10);
    assertControlPin(PIN_RIM_RESET);
    delay(50);
    releaseControlPin(PIN_RIM_RESET);
    delay(100);
    releaseControlPin(PIN_RIM_BOOTSEL);
}

bool requestRimEnterUpdater() {
    // Pause pings so rim ACKs aren't racing UART traffic during OTA handshake.
    noteOtaTraffic(FfbLink::EnterBootloader);
    return sendMsg(FfbLink::EnterBootloader, nullptr, 0);
}

bool sendShiftLed(const FfbLink::ShiftLedPayload& cmd) {
    return sendMsg(FfbLink::ShiftLed, &cmd, sizeof(cmd));
}

bool sendTelemetry(const FfbLink::TelemetryPayload& tel) {
    return sendMsg(FfbLink::Telemetry, &tel, sizeof(tel));
}

bool sendBtnLed(uint16_t mask) {
    btnLedFollow = false;
    FfbLink::BtnLedPayload led{(uint16_t)(mask & 0x3FFu)};
    lastBtnLedSent = led.mask;
    return sendMsg(FfbLink::BtnLed, &led, sizeof(led));
}

void setBtnLedFollow(bool on) {
    btnLedFollow = on;
    lastBtnLedSent = 0xFFFF; // force refresh on next Input
}

bool btnLedFollowEnabled() {
    return btnLedFollow;
}

bool cdcForwardActive() {
    return cdcActive;
}

bool otaInProgress() {
    return (int32_t)(millis() - otaLedUntilMs) < 0;
}

void clearCdcSession() {
    cdcActive = false;
    cdcState = RxState::Sync0;
    cdcLen = 0;
    cdcNeed = 0;
}

bool feedCdcByte(uint8_t b) {
    // Re-sync hunt: if idle and not sync0, not ours.
    if (cdcState == RxState::Sync0) {
        if (b != FfbLink::kSync0)
            return false;
        cdcState = RxState::Sync1;
        cdcActive = true;
        cdcLastMs = millis();
        cdcBuf[0] = b;
        cdcLen = 1;
        return true;
    }

    cdcLastMs = millis();
    cdcActive = true;

    if (cdcState == RxState::Sync1) {
        if (b != FfbLink::kSync1) {
            cdcState = (b == FfbLink::kSync0) ? RxState::Sync1 : RxState::Sync0;
            if (cdcState == RxState::Sync0)
                cdcActive = false;
            return cdcState != RxState::Sync0;
        }
        cdcBuf[cdcLen++] = b;
        cdcState = RxState::Ver;
        return true;
    }

    // After sync, collect ver,type,len then payload+crc
    if (cdcState == RxState::Ver) {
        cdcBuf[cdcLen++] = b;
        cdcState = RxState::Type;
        return true;
    }
    if (cdcState == RxState::Type) {
        cdcBuf[cdcLen++] = b;
        cdcState = RxState::Len;
        return true;
    }
    if (cdcState == RxState::Len) {
        cdcBuf[cdcLen++] = b;
        const uint8_t plen = b;
        if (plen > FfbLink::kMaxPayload) {
            cdcState = RxState::Sync0;
            cdcActive = false;
            return true;
        }
        cdcNeed = (uint8_t)(plen + 2); // payload + crc16
        cdcState = RxState::Payload;
        if (cdcNeed == 0) {
            // unreachable
        }
        return true;
    }

    // Payload + CRC bytes
    cdcBuf[cdcLen++] = b;
    cdcNeed--;
    if (cdcNeed == 0) {
        // cdcBuf: sync0 sync1 | ver type len | payload | crc_lo crc_hi
        const uint8_t plen = cdcBuf[4];
        const uint8_t* body = &cdcBuf[2];
        const uint16_t expect = FfbLink::crc16(body, (uint16_t)(3 + plen));
        const uint16_t got = (uint16_t)cdcBuf[cdcLen - 2] | ((uint16_t)cdcBuf[cdcLen - 1] << 8);
        if (got == expect) {
            const uint8_t t = cdcBuf[3];
            const uint8_t* payload = plen ? &cdcBuf[5] : nullptr;
            // Rebuild via sendMsg so rim gets a clean frame (and we own TX pacing).
            sendMsg(t, payload, plen);
            if (t >= FfbLink::EnterBootloader && t <= FfbLink::FwFail) {
                noteOtaTraffic(t);
            }
        }
        cdcState = RxState::Sync0;
        cdcLen = 0;
        // Stay cdcActive briefly so inter-frame binary isn't eaten as ASCII.
    }
    return true;
}

} // namespace AccessoryLink
