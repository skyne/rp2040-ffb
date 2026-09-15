#include "setup_msc.h"

#include "config.h"

#if ENABLE_SETUP_MSC

#include <Arduino.h>
#include <USB.h>
#include <class/msc/msc.h>
#include <device/usbd.h>
#include <string.h>
#include <tusb-msc.h>

#include "hid_wheel.h"
#include "homing.h"

namespace {

enum class State : uint8_t { Idle, PendingHome, Waiting, Visible, Hidden };

State state = State::Idle;
uint32_t graceMs_ = 2000;
uint32_t graceDeadlineMs = 0;
bool usbRegistered = false;
uint8_t ifaceId = 0;
uint8_t epIn = 0;
uint8_t epOut = 0;

// Tiny read-only FAT12: 1 boot + 1 FAT + 1 root + 8 data sectors.
constexpr uint16_t kSectorSize = 512;
constexpr uint16_t kSectorCount = 12;
constexpr uint16_t kReservedSectors = 1;
constexpr uint16_t kFatSectors = 1;
constexpr uint16_t kRootEntries = 16; // 1 root sector
constexpr uint16_t kRootSectors = (kRootEntries * 32) / kSectorSize;
constexpr uint16_t kDataStartSector = kReservedSectors + kFatSectors + kRootSectors;

uint8_t disk[kSectorCount * kSectorSize];

constexpr char kWinScript[] =
    "@echo off\r\n"
    "set \"URL=https://github.com/skyne/rp2040-ffb/releases/latest/download/"
    "ffb-config-windows-x86_64.exe\"\r\n"
    "set \"OUT=%TEMP%\\ffb-config.exe\"\r\n"
    "echo Downloading ffb-config...\r\n"
    "curl.exe -fsSL -o \"%OUT%\" \"%URL%\"\r\n"
    "if errorlevel 1 (\r\n"
    "  echo Download failed. Open: "
    "https://github.com/skyne/rp2040-ffb/releases/latest\r\n"
    "  pause\r\n"
    "  exit /b 1\r\n"
    ")\r\n"
    "start \"\" \"%OUT%\"\r\n";

constexpr char kMacScript[] = "#!/bin/sh\n"
                              "set -e\n"
                              "URL=\"https://github.com/skyne/rp2040-ffb/releases/latest/download/"
                              "ffb-config-macos-aarch64\"\n"
                              "OUT=\"${TMPDIR:-/tmp}/ffb-config\"\n"
                              "echo \"Downloading ffb-config...\"\n"
                              "curl -fsSL -o \"$OUT\" \"$URL\"\n"
                              "chmod +x \"$OUT\"\n"
                              "exec \"$OUT\"\n";

constexpr char kLinuxScript[] =
    "#!/bin/sh\n"
    "set -e\n"
    "URL=\"https://github.com/skyne/rp2040-ffb/releases/latest/download/"
    "ffb-config-linux-x86_64\"\n"
    "OUT=\"${TMPDIR:-/tmp}/ffb-config\"\n"
    "echo \"Downloading ffb-config...\"\n"
    "curl -fsSL -o \"$OUT\" \"$URL\"\n"
    "chmod +x \"$OUT\"\n"
    "exec \"$OUT\"\n";

struct FileSpec {
    const char name[11]; // 8.3, space-padded, no null required in FAT entry
    const char* data;
    uint16_t size;
    uint16_t startCluster;
};

// FAT12: cluster 2+ are data. One cluster per sector (spc=1).
FileSpec files[] = {
    {{'W', 'I', 'N', ' ', ' ', ' ', ' ', ' ', 'B', 'A', 'T'}, kWinScript, 0, 2},
    {{'M', 'A', 'C', ' ', ' ', ' ', ' ', ' ', 'S', 'H', ' '}, kMacScript, 0, 0},
    {{'L', 'I', 'N', 'U', 'X', ' ', ' ', ' ', 'S', 'H', ' '}, kLinuxScript, 0, 0},
};

void fat12Set(uint8_t* fat, uint16_t cluster, uint16_t value) {
    const uint32_t i = cluster + (cluster / 2);
    if (cluster & 1) {
        fat[i] = (uint8_t)((fat[i] & 0x0F) | ((value << 4) & 0xF0));
        fat[i + 1] = (uint8_t)((value >> 4) & 0xFF);
    } else {
        fat[i] = (uint8_t)(value & 0xFF);
        fat[i + 1] = (uint8_t)((fat[i + 1] & 0xF0) | ((value >> 8) & 0x0F));
    }
}

void writeDirEntry(uint8_t* ent, const char name[11], uint8_t attr, uint16_t cluster,
                   uint32_t size) {
    memset(ent, 0, 32);
    memcpy(ent, name, 11);
    ent[11] = attr;
    ent[26] = (uint8_t)(cluster & 0xFF);
    ent[27] = (uint8_t)((cluster >> 8) & 0xFF);
    ent[28] = (uint8_t)(size & 0xFF);
    ent[29] = (uint8_t)((size >> 8) & 0xFF);
    ent[30] = (uint8_t)((size >> 16) & 0xFF);
    ent[31] = (uint8_t)((size >> 24) & 0xFF);
}

void buildDisk() {
    memset(disk, 0, sizeof(disk));

    files[0].size = (uint16_t)strlen(kWinScript);
    files[1].size = (uint16_t)strlen(kMacScript);
    files[2].size = (uint16_t)strlen(kLinuxScript);

    uint16_t nextCluster = 2;
    for (FileSpec& f : files) {
        f.startCluster = nextCluster;
        const uint16_t clusters = (uint16_t)((f.size + kSectorSize - 1) / kSectorSize);
        nextCluster = (uint16_t)(nextCluster + (clusters ? clusters : 1));
    }

    // Boot / BPB
    uint8_t* bpb = disk;
    bpb[0] = 0xEB;
    bpb[1] = 0x3C;
    bpb[2] = 0x90;
    memcpy(bpb + 3, "MSDOS5.0", 8);
    bpb[11] = kSectorSize & 0xFF;
    bpb[12] = (kSectorSize >> 8) & 0xFF;
    bpb[13] = 1; // sectors per cluster
    bpb[14] = kReservedSectors & 0xFF;
    bpb[15] = (kReservedSectors >> 8) & 0xFF;
    bpb[16] = 1; // FATs
    bpb[17] = kRootEntries & 0xFF;
    bpb[18] = (kRootEntries >> 8) & 0xFF;
    bpb[19] = kSectorCount & 0xFF;
    bpb[20] = (kSectorCount >> 8) & 0xFF;
    bpb[21] = 0xF8;
    bpb[22] = kFatSectors & 0xFF;
    bpb[23] = (kFatSectors >> 8) & 0xFF;
    bpb[24] = 1;    // sectors/track (dummy)
    bpb[26] = 1;    // heads (dummy)
    bpb[36] = 0x29; // ext boot signature
    bpb[39] = 'F';
    bpb[40] = 'F';
    bpb[41] = 'B';
    bpb[42] = 'S';
    bpb[43] = 'E';
    bpb[44] = 'T';
    bpb[45] = 'U';
    bpb[46] = 'P';
    bpb[47] = ' ';
    bpb[48] = ' ';
    bpb[49] = ' ';
    memcpy(bpb + 54, "FAT12   ", 8);
    bpb[510] = 0x55;
    bpb[511] = 0xAA;

    // FAT
    uint8_t* fat = disk + kSectorSize;
    fat12Set(fat, 0, 0xFF8);
    fat12Set(fat, 1, 0xFFF);
    for (const FileSpec& f : files) {
        const uint16_t clusters = (uint16_t)((f.size + kSectorSize - 1) / kSectorSize);
        const uint16_t n = clusters ? clusters : 1;
        for (uint16_t i = 0; i < n; ++i) {
            const uint16_t c = (uint16_t)(f.startCluster + i);
            fat12Set(fat, c, (i + 1 < n) ? (uint16_t)(c + 1) : 0xFFF);
        }
    }

    // Root directory
    uint8_t* root = disk + (kReservedSectors + kFatSectors) * kSectorSize;
    const char vol[11] = {'F', 'F', 'B', 'S', 'E', 'T', 'U', 'P', ' ', ' ', ' '};
    writeDirEntry(root, vol, 0x08, 0, 0);
    uint8_t* ent = root + 32;
    for (const FileSpec& f : files) {
        writeDirEntry(ent, f.name, 0x20, f.startCluster, f.size);
        ent += 32;
    }

    // File payloads
    for (const FileSpec& f : files) {
        const uint16_t clusters = (uint16_t)((f.size + kSectorSize - 1) / kSectorSize);
        const uint16_t n = clusters ? clusters : 1;
        for (uint16_t i = 0; i < n; ++i) {
            const uint16_t sector = (uint16_t)(kDataStartSector + (f.startCluster - 2) + i);
            uint8_t* dst = disk + sector * kSectorSize;
            const uint16_t off = (uint16_t)(i * kSectorSize);
            uint16_t chunk = 0;
            if (f.size > off) {
                chunk = (uint16_t)(f.size - off);
                if (chunk > kSectorSize) {
                    chunk = kSectorSize;
                }
            }
            if (chunk) {
                memcpy(dst, f.data + off, chunk);
            }
        }
    }
}

void attachInterfaces() {
    if (usbRegistered) {
        return;
    }
    // Caller owns USB.disconnect()/connect() — never re-enum mid-session.
    epIn = USB.registerEndpointIn();
    epOut = USB.registerEndpointOut();
    static uint8_t msdDesc[] = {
        TUD_MSC_DESCRIPTOR(1 /* placeholder */, 0, epOut, epIn, CFG_TUD_MSC_EP_BUFSIZE)};
    ifaceId = USB.registerInterface(1, USBClass::simpleInterface, msdDesc, sizeof(msdDesc), 2, 0);
    usbRegistered = true;
}

void showVolume() {
    if (state == State::Visible || state == State::Hidden) {
        return;
    }
    buildDisk();
    if (!usbRegistered) {
        // Last-resort path if attachAtBoot() was skipped — still avoids mid-session churn
        // when already connected only if we refuse to tear down. Leave not-ready instead.
        Serial.println("setup_msc: not attached at boot; skipping volume");
        state = State::Hidden;
        return;
    }
    state = State::Visible;
    Serial.println("setup_msc: volume ready (WIN.BAT MAC.SH LINUX.SH)");
}

void hideVolume() {
    // Keep MSC interface registered; only clear media-ready so CDC/HID stay up.
    if (state != State::Visible) {
        state = State::Hidden;
        return;
    }
    state = State::Hidden;
    Serial.println("setup_msc: volume not-ready (configurator present)");
}

int32_t readSector(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (!usbRegistered || state != State::Visible || lba >= kSectorCount || offset >= kSectorSize) {
        return -1;
    }
    const uint32_t avail = kSectorSize - offset;
    const uint32_t n = (bufsize < avail) ? bufsize : avail;
    memcpy(buffer, disk + lba * kSectorSize + offset, n);
    return (int32_t)n;
}

void capacity(uint32_t* block_count, uint16_t* block_size) {
    *block_count = kSectorCount;
    *block_size = kSectorSize;
}

} // namespace

namespace SetupMsc {

void attachAtBoot() {
    attachInterfaces();
}

void beginGrace(uint32_t graceMs) {
    graceMs_ = graceMs;
    // Wait out motorized/hand INIT before starting the grace timer — otherwise the
    // setup volume (and any host remount noise) lands mid-homing.
    state = State::PendingHome;
}

void service() {
    if (state == State::PendingHome) {
        // Wait for INIT and for PID HID attach (MSC registers in the same re-enum).
        if (Homing::active() || !HidWheel::attached()) {
            return;
        }
        state = State::Waiting;
        graceDeadlineMs = millis() + graceMs_;
        return;
    }
    if (state != State::Waiting) {
        return;
    }
    if ((int32_t)(millis() - graceDeadlineMs) >= 0) {
        showVolume();
    }
}

void notifyHostApp() {
    if (state == State::PendingHome || state == State::Waiting) {
        state = State::Hidden;
        Serial.println("setup_msc: kept hidden (host app within grace)");
        return;
    }
    if (state == State::Visible) {
        hideVolume();
    }
}

bool visible() {
    return state == State::Visible;
}

} // namespace SetupMsc

// TinyUSB MSC callbacks (only compiled when this TU is linked).
extern "C" uint8_t tud_msc_get_maxlun_cb(void) {
    return 1;
}

extern "C" void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
                                   uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id, "rp2040ff", 8);
    memcpy(product_id, "FFB Setup      ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return SetupMsc::visible();
}

extern "C" void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void)lun;
    capacity(block_count, block_size);
}

extern "C" int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer,
                                     uint32_t bufsize) {
    (void)lun;
    return readSector(lba, offset, buffer, bufsize);
}

extern "C" bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return false;
}

extern "C" int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer,
                                      uint32_t bufsize) {
    (void)lun;
    (void)lba;
    (void)offset;
    (void)buffer;
    return (int32_t)bufsize; // ignore host writes
}

extern "C" bool tud_msc_set_sense(uint8_t lun, uint8_t sense_key, uint8_t add_sense_code,
                                  uint8_t add_sense_qualifier);

extern "C" int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                                   uint16_t bufsize) {
    (void)buffer;
    const int SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL = 0x1E;
    const int SCSI_CMD_START_STOP_UNIT = 0x1B;
    const int SCSI_SENSE_ILLEGAL_REQUEST = 0x05;

    int32_t resplen = 0;
    switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
    case SCSI_CMD_START_STOP_UNIT:
        resplen = 0;
        break;
    default:
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
        resplen = -1;
        break;
    }
    if (resplen > bufsize) {
        resplen = bufsize;
    }
    return resplen;
}

extern "C" bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                                      bool load_eject) {
    (void)lun;
    (void)power_condition;
    (void)start;
    (void)load_eject;
    return true;
}

#else // !ENABLE_SETUP_MSC

#include <string.h>

namespace SetupMsc {
void attachAtBoot() {}
void beginGrace(uint32_t) {}
void service() {}
void notifyHostApp() {}
bool visible() {
    return false;
}
} // namespace SetupMsc

// LDF may still pull tusb-msc from the inactive #if branch — provide inert callbacks.
extern "C" uint8_t tud_msc_get_maxlun_cb(void) {
    return 1;
}
extern "C" void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
                                   uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id, "rp2040ff", 8);
    memcpy(product_id, "FFB Setup      ", 16);
    memcpy(product_rev, "1.0 ", 4);
}
extern "C" bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return false;
}
extern "C" void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void)lun;
    *block_count = 0;
    *block_size = 512;
}
extern "C" int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer,
                                     uint32_t bufsize) {
    (void)lun;
    (void)lba;
    (void)offset;
    (void)buffer;
    (void)bufsize;
    return -1;
}
extern "C" int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer,
                                      uint32_t bufsize) {
    (void)lun;
    (void)lba;
    (void)offset;
    (void)buffer;
    (void)bufsize;
    return -1;
}
extern "C" int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                                   uint16_t bufsize) {
    (void)lun;
    (void)scsi_cmd;
    (void)buffer;
    (void)bufsize;
    return -1;
}
extern "C" bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                                      bool load_eject) {
    (void)lun;
    (void)power_condition;
    (void)start;
    (void)load_eject;
    return true;
}

#endif
