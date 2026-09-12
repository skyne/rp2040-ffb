#include "updater.h"

#include <Arduino.h>
#include <hardware/flash.h>
#include <hardware/regs/psm.h>
#include <hardware/structs/psm.h>
#include <hardware/structs/watchdog.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <stdlib.h>
#include <string.h>

#include "ffb_link.h"
#include "link.h"
#include "shift_leds.h"

namespace Updater {
namespace {

// Stage new firmware in upper flash (app at 0 stays intact until apply-on-boot).
// Layout: [hdr 4K @ 1MB] [image ...] — EEPROM near 2MB is untouched.
static constexpr uint32_t kStageHdrOff = 1024u * 1024u;
static constexpr uint32_t kStageImgOff = kStageHdrOff + FLASH_SECTOR_SIZE;
static constexpr uint32_t kStageMagic = 0x3141544Fu; // 'OTA1'

struct StageHdr {
    uint32_t magic;
    uint32_t size;
    uint32_t crc32;
    uint32_t reserved;
};

bool inUpdater = false;
uint8_t* image = nullptr;
uint32_t imageSize = 0;
uint32_t expectCrc = 0;
uint32_t received = 0;
uint32_t lastActivityMs = 0;

// Longer than host per-chunk ACK wait (5s); shorter than a hung session.
// commit() blocks until reboot, so update() cannot fire mid-flash-write.
static constexpr uint32_t kIdleTimeoutMs = 15000;

// Shared 4K buffer in BSS (core stack is only ~2K).
alignas(4) uint8_t sectorBuf[FLASH_SECTOR_SIZE];

void noteActivity() {
    lastActivityMs = millis();
}

void freeImage() {
    if (image) {
        free(image);
        image = nullptr;
    }
    imageSize = 0;
    expectCrc = 0;
    received = 0;
}

void sendAck(uint32_t offset) {
    Link::sendMsg(FfbLink::FwAck, &offset, sizeof(offset));
}

void sendNak(uint32_t offset) {
    Link::sendMsg(FfbLink::FwNak, &offset, sizeof(offset));
}

void leaveUpdater() {
    freeImage();
    inUpdater = false;
    ShiftLeds::clearOta();
}

void sendFail(uint8_t reason) {
    Link::sendMsg(FfbLink::FwFail, &reason, sizeof(reason));
    leaveUpdater();
}

void fillFF(uint8_t* dst, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = 0xFF;
    }
}

// Safe while running from flash — only touches the staging region (not slot 0).
void writeSector(uint32_t flashOff, const uint8_t* data) {
    rp2040.idleOtherCore();
    noInterrupts();
    flash_range_erase(flashOff, FLASH_SECTOR_SIZE);
    flash_range_program(flashOff, data, FLASH_SECTOR_SIZE);
    interrupts();
    rp2040.resumeOtherCore();
}

bool writeStaging(const uint8_t* data, uint32_t len, uint32_t crc) {
    const uint32_t eraseLen =
        (len + FLASH_SECTOR_SIZE - 1u) / FLASH_SECTOR_SIZE * FLASH_SECTOR_SIZE;

    // Erase header first so a partial image can never look "ready".
    fillFF(sectorBuf, FLASH_SECTOR_SIZE);
    writeSector(kStageHdrOff, sectorBuf);

    for (uint32_t off = 0; off < eraseLen; off += FLASH_SECTOR_SIZE) {
        fillFF(sectorBuf, FLASH_SECTOR_SIZE);
        if (off < len) {
            const uint32_t n = (len - off) > FLASH_SECTOR_SIZE ? FLASH_SECTOR_SIZE : (len - off);
            memcpy(sectorBuf, data + off, n);
        }
        writeSector(kStageImgOff + off, sectorBuf);

        const uint8_t* flash = (const uint8_t*)(XIP_BASE + kStageImgOff + off);
        if (memcmp(flash, sectorBuf, FLASH_SECTOR_SIZE) != 0) {
            return false;
        }
    }

    // Header last — only now is the stage considered valid.
    StageHdr hdr{};
    hdr.magic = kStageMagic;
    hdr.size = len;
    hdr.crc32 = crc;
    hdr.reserved = 0;
    fillFF(sectorBuf, FLASH_SECTOR_SIZE);
    memcpy(sectorBuf, &hdr, sizeof(hdr));
    writeSector(kStageHdrOff, sectorBuf);

    const auto* rh = (const StageHdr*)(XIP_BASE + kStageHdrOff);
    return rh->magic == kStageMagic && rh->size == len && rh->crc32 == crc;
}

void __no_inline_not_in_flash_func(resetViaWatchdog)() {
    hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
    watchdog_hw->scratch[4] = 0;
    watchdog_hw->scratch[5] = 0;
    watchdog_hw->scratch[6] = 0;
    watchdog_hw->scratch[7] = 0;
    hw_set_bits(&psm_hw->wdsel, PSM_WDSEL_BITS & ~(PSM_WDSEL_ROSC_BITS | PSM_WDSEL_XOSC_BITS));
    hw_set_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_TRIGGER_BITS);
    while (true) {
        __asm volatile("wfi");
    }
}

// Copy staged image over slot 0. Header magic must already be cleared so a
// failed mid-write cannot re-trigger apply on the next boot (incl. after USB recovery).
void __no_inline_not_in_flash_func(installFromStaging)(uint32_t len) {
    const uint32_t eraseLen =
        (len + FLASH_SECTOR_SIZE - 1u) / FLASH_SECTOR_SIZE * FLASH_SECTOR_SIZE;

    // Core1 may already be running (Arduino starts it before setup). Any XIP
    // activity there during erase/program bricks the apply — park it for good;
    // we reboot instead of resumeOtherCore.
    rp2040.idleOtherCore();

    for (uint32_t off = 0; off < eraseLen; off += FLASH_SECTOR_SIZE) {
        const uint8_t* src = (const uint8_t*)(XIP_BASE + kStageImgOff + off);
        for (uint32_t i = 0; i < FLASH_SECTOR_SIZE; ++i) {
            sectorBuf[i] = src[i];
        }

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(off, FLASH_SECTOR_SIZE);
        flash_range_program(off, sectorBuf, FLASH_SECTOR_SIZE);
        restore_interrupts(ints);
    }

    resetViaWatchdog();
}

bool imageLooksBootable(const uint8_t* img, uint32_t len) {
    if (len < 0x200)
        return false;
    // SP in SRAM, Reset in flash XIP range (Thumb bit set).
    const uint32_t sp = (uint32_t)img[0x100] | ((uint32_t)img[0x101] << 8) |
                        ((uint32_t)img[0x102] << 16) | ((uint32_t)img[0x103] << 24);
    const uint32_t rs = (uint32_t)img[0x104] | ((uint32_t)img[0x105] << 8) |
                        ((uint32_t)img[0x106] << 16) | ((uint32_t)img[0x107] << 24);
    if (sp < 0x20000000u || sp > 0x20042000u)
        return false;
    if ((rs & 1u) == 0)
        return false;
    if ((rs & ~1u) < 0x10000100u || (rs & ~1u) >= 0x10200000u)
        return false;
    return true;
}

bool tryApplyStaged() {
    const auto* hdr = (const StageHdr*)(XIP_BASE + kStageHdrOff);
    if (hdr->magic != kStageMagic) {
        return false;
    }
    const uint32_t len = hdr->size;
    const uint32_t crc = hdr->crc32;
    if (len == 0 || len > FfbLink::kOtaMaxImageBytes) {
        fillFF(sectorBuf, FLASH_SECTOR_SIZE);
        writeSector(kStageHdrOff, sectorBuf);
        return false;
    }

    const uint8_t* img = (const uint8_t*)(XIP_BASE + kStageImgOff);
    if (FfbLink::crc32(img, len) != crc || !imageLooksBootable(img, len)) {
        fillFF(sectorBuf, FLASH_SECTOR_SIZE);
        writeSector(kStageHdrOff, sectorBuf);
        return false;
    }

    // Clear magic *before* touching slot 0 — image bytes remain in staging.
    fillFF(sectorBuf, FLASH_SECTOR_SIZE);
    writeSector(kStageHdrOff, sectorBuf);

    installFromStaging(len); // never returns
    return true;
}

void commit() {
    if (!image || received != imageSize) {
        sendFail(FfbLink::FwFailSeq);
        return;
    }
    const uint32_t got = FfbLink::crc32(image, imageSize);
    if (got != expectCrc) {
        sendFail(FfbLink::FwFailCrc);
        return;
    }

    if (!writeStaging(image, imageSize, expectCrc)) {
        sendFail(FfbLink::FwFailFlash);
        return;
    }

    freeImage();
    inUpdater = false;

    Link::sendMsg(FfbLink::FwDone, nullptr, 0);
    delay(80);
    Serial1.flush();

    // Park Core1 before reset — NeoPixel bitbang must not touch XIP mid-reboot.
    rp2040.idleOtherCore();
    resetViaWatchdog();
}

} // namespace

void begin() {
    inUpdater = false;
    freeImage();
    tryApplyStaged(); // may never return
}

bool active() {
    return inUpdater;
}

void enter() {
    freeImage();
    inUpdater = true;
    noteActivity();
    ShiftLeds::showOta();
    Link::sendMsg(FfbLink::UpdaterReady, nullptr, 0);
}

void update() {
    if (!inUpdater)
        return;
    if ((int32_t)(millis() - lastActivityMs) >= (int32_t)kIdleTimeoutMs) {
        sendFail(FfbLink::FwFailTimeout);
    }
}

void onFrame(uint8_t type, const uint8_t* payload, uint8_t len) {
    if (type == FfbLink::EnterBootloader) {
        enter();
        return;
    }
    if (!inUpdater)
        return;

    noteActivity();

    if (type == FfbLink::FwBegin) {
        if (len < sizeof(FfbLink::FwBeginPayload)) {
            sendFail(FfbLink::FwFailSize);
            return;
        }
        FfbLink::FwBeginPayload begin{};
        memcpy(&begin, payload, sizeof(begin));
        freeImage();
        if (begin.size == 0 || begin.size > FfbLink::kOtaMaxImageBytes) {
            sendFail(FfbLink::FwFailSize);
            return;
        }
        image = (uint8_t*)malloc(begin.size);
        if (!image) {
            sendFail(FfbLink::FwFailOom);
            return;
        }
        imageSize = begin.size;
        expectCrc = begin.crc32;
        received = 0;
        sendAck(0);
        return;
    }

    if (type == FfbLink::FwData) {
        if (!image || len < 4) {
            sendNak(received);
            return;
        }
        uint32_t offset = 0;
        memcpy(&offset, payload, 4);
        const uint8_t* chunk = payload + 4;
        const uint8_t chunkLen = (uint8_t)(len - 4);
        if (offset != received || offset + chunkLen > imageSize) {
            sendNak(received);
            return;
        }
        memcpy(image + offset, chunk, chunkLen);
        received += chunkLen;
        sendAck(received);
        return;
    }

    if (type == FfbLink::FwEnd) {
        commit();
        return;
    }
}

} // namespace Updater
