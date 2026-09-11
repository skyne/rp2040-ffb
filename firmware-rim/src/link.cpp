#include "link.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"

namespace Link {
namespace {

HardwareSerial &Uart = Serial1;
FrameHandler handler = nullptr;
volatile uint32_t lastRx = 0;

FfbLink::ByteRing<FfbLink::kRxRingSize> rxRing;

enum class RxState : uint8_t { Sync0, Sync1, Ver, Type, Len, Payload, Crc0, Crc1 };
RxState rxState = RxState::Sync0;
uint8_t rxType = 0;
uint8_t rxLen = 0;
uint8_t rxPayload[FfbLink::kMaxPayload];
uint8_t rxIdx = 0;
uint8_t rxCrcLo = 0;
uint8_t hdrBuf[3];

void resetRx() { rxState = RxState::Sync0; }

void feed(uint8_t b) {
    switch (rxState) {
        case RxState::Sync0:
            if (b == FfbLink::kSync0) rxState = RxState::Sync1;
            break;
        case RxState::Sync1:
            if (b == FfbLink::kSync1) {
                rxState = RxState::Ver;
            } else if (b != FfbLink::kSync0) {
                rxState = RxState::Sync0;
            }
            break;
        case RxState::Ver:
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
            if (rxIdx >= rxLen) rxState = RxState::Crc0;
            break;
        case RxState::Crc0:
            rxCrcLo = b;
            rxState = RxState::Crc1;
            break;
        case RxState::Crc1: {
            uint8_t crcbuf[3 + FfbLink::kMaxPayload];
            memcpy(crcbuf, hdrBuf, 3);
            if (rxLen) memcpy(crcbuf + 3, rxPayload, rxLen);
            const uint16_t expect = FfbLink::crc16(crcbuf, (uint16_t)(3 + rxLen));
            const uint16_t got = (uint16_t)rxCrcLo | ((uint16_t)b << 8);
            if (got == expect) {
                lastRx = millis();
                if (handler) handler(rxType, rxPayload, rxLen);
            }
            // Drop bad CRC without stalling — resync on next frame.
            resetRx();
            break;
        }
    }
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

}  // namespace

void begin() {
    // UART0 defaults to GP0 TX / GP1 RX on earlephilhower core (matches config.h).
    Serial1.setFIFOSize(FfbLink::kUartFifoSize);
    Uart.begin(FfbLink::kBaud);
    rxRing.clear();
    lastRx = 0;
}

void setHandler(FrameHandler h) { handler = h; }

void update() {
    drainUartToRing();
    uint8_t b = 0;
    while (rxRing.pop(b)) {
        feed(b);
    }
}

void noteRx() { lastRx = millis(); }

uint32_t lastRxMs() { return lastRx; }

bool linked() {
    if (lastRx == 0) return false;
    return (millis() - lastRx) < FfbLink::kLinkTimeoutMs;
}

bool sendMsg(uint8_t type, const void *payload, uint8_t len) {
    if (len > FfbLink::kMaxPayload) return false;
    uint8_t body[3 + FfbLink::kMaxPayload];
    body[0] = FfbLink::kVersion;
    body[1] = type;
    body[2] = len;
    if (len && payload) memcpy(body + 3, payload, len);
    const uint16_t crc = FfbLink::crc16(body, (uint16_t)(3 + len));
    Uart.write(FfbLink::kSync0);
    Uart.write(FfbLink::kSync1);
    Uart.write(body, 3 + len);
    Uart.write((uint8_t)(crc & 0xFF));
    Uart.write((uint8_t)(crc >> 8));
    return true;
}

}  // namespace Link
