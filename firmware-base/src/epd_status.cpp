#include "epd_status.h"

#include "config.h"

#if !ENABLE_BASE_EPD

namespace EpdStatus {
void begin() {}
void update(bool, float) {}
void requestRefresh() {}
void forceRefresh() {}
bool enabled() { return false; }
void setEnabled(bool) {}
void beginCore1() {}
void serviceCore1() {}
}  // namespace EpdStatus

#else

#include <Arduino.h>
#include <SPI.h>
#include <SoftwareSPI.h>
#include <math.h>
#include <string.h>
#include <pico/mutex.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

#include "accessory_link.h"
#include "ffb.h"
#include "ffb_version.h"
#include "homing.h"
#include "motor_bts7960.h"
#include "wheel_encoder.h"

#include <gdey/GxEPD2_370_GDEY037T03.h>

namespace EpdStatus {
namespace {

// Dedicated PIO SPI on core1 — HW SPI0 stays exclusive to the MLX90363 on core0.
SoftwareSPI epdSpi(PIN_EPD_SCK, PIN_EPD_MISO_UNUSED, PIN_EPD_MOSI);

GxEPD2_BW<GxEPD2_370_GDEY037T03, GxEPD2_370_GDEY037T03::HEIGHT> display(
    GxEPD2_370_GDEY037T03(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

mutex_t jobMu;
bool gEnabled = true;
bool gForce = false;
volatile bool gCore1Ready = false;

uint32_t lastPaintMs = 0;
uint32_t lastFullMs = 0;
uint16_t partialsSinceFull = 0;

static constexpr uint32_t kMinPaintIntervalMs = 1000;
static constexpr uint16_t kFullEveryNPartials = 20;
static constexpr uint32_t kFullAtLeastMs = 10UL * 60UL * 1000UL;

struct FrameSig {
    uint8_t hallOk;
    uint8_t rimOk;
    uint8_t motorsOn;
    uint8_t ffbMode;
    uint8_t homeActive;
    uint8_t homePhase;
    int32_t axleX10;
    int32_t gearX1000;
    int32_t capX100;
    int32_t torqX1000;
};

// Fully self-contained draw payload — core1 must not touch FFB/hall/homing.
struct PaintJob {
    FrameSig sig;
    bool forceFull;
    bool hallOk;
    bool rimOk;
    bool motorsOn;
    char fw[FfbVersion::kIdMax];
    char ffbName[8];
    char homeLine[24];
    float axleDeg;
    float gear;
    float cap;
    float torq;
};

FrameSig lastSig{};
bool lastSigValid = false;

PaintJob queuedJob{};
volatile bool hasJob = false;
volatile bool core1Painting = false;

const char *ffbModeName(Ffb::Mode m) {
    switch (m) {
        case Ffb::Mode::Off: return "off";
        case Ffb::Mode::Manual: return "manual";
        case Ffb::Mode::Spring: return "spring";
    }
    return "?";
}

FrameSig makeSig(bool hallOk, float axleDeg) {
    FrameSig s{};
    s.hallOk = hallOk ? 1 : 0;
    s.rimOk = AccessoryLink::linked() ? 1 : 0;
    s.motorsOn = MotorBts7960::enabled() ? 1 : 0;
    s.ffbMode = (uint8_t)Ffb::mode();
    s.homeActive = Homing::active() ? 1 : 0;
    s.homePhase = (uint8_t)Homing::phase();
    s.axleX10 = (int32_t)lroundf(axleDeg * 10.0f);
    s.gearX1000 = (int32_t)lroundf(WheelEncoder::gearRatio() * 1000.0f);
    s.capX100 = (int32_t)lroundf(MotorBts7960::dutyCap() * 100.0f);
    s.torqX1000 = (int32_t)lroundf(Ffb::commandedTorque() * 1000.0f);
    return s;
}

bool sigEqual(const FrameSig &a, const FrameSig &b) { return memcmp(&a, &b, sizeof(FrameSig)) == 0; }

bool wantFullRefresh(bool forceFull) {
    if (forceFull) return true;
    if (lastFullMs == 0) return true;
    if (partialsSinceFull >= kFullEveryNPartials) return true;
    if ((millis() - lastFullMs) >= kFullAtLeastMs) return true;
    return false;
}

bool motorsBlockingAuto() { return MotorBts7960::enabled(); }

void fillJob(PaintJob &job, bool hallOk, float axleDeg, bool forceFull, const FrameSig &sig) {
    job.sig = sig;
    job.forceFull = forceFull;
    job.hallOk = hallOk;
    job.rimOk = AccessoryLink::linked();
    job.motorsOn = MotorBts7960::enabled();
    FfbVersion::formatId(job.fw, sizeof(job.fw));
    strncpy(job.ffbName, ffbModeName(Ffb::mode()), sizeof(job.ffbName) - 1);
    job.ffbName[sizeof(job.ffbName) - 1] = '\0';
    if (Homing::active()) {
        snprintf(job.homeLine, sizeof(job.homeLine), "home %s", Homing::phaseName());
    } else {
        snprintf(job.homeLine, sizeof(job.homeLine), "home idle");
    }
    job.axleDeg = axleDeg;
    job.gear = WheelEncoder::gearRatio();
    job.cap = MotorBts7960::dutyCap();
    job.torq = Ffb::commandedTorque();
}

// Core0: enqueue latest frame (coalesces if core1 is still busy).
void queueJob(const PaintJob &job) {
    mutex_enter_blocking(&jobMu);
    queuedJob = job;
    hasJob = true;
    mutex_exit(&jobMu);
}

void drawLine(int16_t &y, const GFXfont *font, const char *text) {
    display.setFont(font);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
    display.setCursor(8, y);
    display.print(text);
    y += (int16_t)h + 6;
}

void paintJob(const PaintJob &job) {
    char line[48];

    display.setRotation(1);
    if (job.forceFull) {
        display.setFullWindow();
    } else {
        display.setPartialWindow(0, 0, display.width(), display.height());
    }
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);

        int16_t y = 28;
        drawLine(y, &FreeMonoBold12pt7b, "rp2040-ffb base");

        snprintf(line, sizeof(line), "fw %s", job.fw);
        drawLine(y, &FreeMono9pt7b, line);

        snprintf(line, sizeof(line), "FFB %s   mot %s", job.ffbName, job.motorsOn ? "ON" : "off");
        drawLine(y, &FreeMono9pt7b, line);

        snprintf(line, sizeof(line), "hall %s   rim %s", job.hallOk ? "OK" : "FAULT",
                 job.rimOk ? "ok" : "no");
        drawLine(y, &FreeMono9pt7b, line);

        drawLine(y, &FreeMono9pt7b, job.homeLine);

        snprintf(line, sizeof(line), "axle %+7.1f deg", (double)job.axleDeg);
        drawLine(y, &FreeMonoBold12pt7b, line);

        snprintf(line, sizeof(line), "gear %.3f  cap %.2f", (double)job.gear, (double)job.cap);
        drawLine(y, &FreeMono9pt7b, line);

        snprintf(line, sizeof(line), "torq %+.3f", (double)job.torq);
        drawLine(y, &FreeMono9pt7b, line);
    } while (display.nextPage());
}

}  // namespace

void begin() {
    mutex_init(&jobMu);
    gEnabled = true;
    gForce = false;
    lastSigValid = false;
    hasJob = false;
    // Mark boot full as done so the first queued frame can be partial once core1 is up.
    lastFullMs = millis();
    lastPaintMs = lastFullMs;
    partialsSinceFull = 0;
    // Hardware init happens on core1 — core0 never touches the panel bus.
}

void beginCore1() {
    pinMode(PIN_EPD_CS, OUTPUT);
    digitalWrite(PIN_EPD_CS, HIGH);
    pinMode(PIN_EPD_DC, OUTPUT);
    pinMode(PIN_EPD_RST, OUTPUT);
    pinMode(PIN_EPD_BUSY, INPUT);

    epdSpi.begin(/*hwCS=*/false);
    display.epd2.selectSPI(epdSpi, SPISettings(EPD_SPI_HZ, MSBFIRST, SPI_MODE0));
    // No busy callback — core0 keeps running the control loop independently.
    display.init(0, true, 2, false);

    // Boot splash (blocks core1 only). Core0 bookkeeping starts after gCore1Ready.
    PaintJob boot{};
    boot.forceFull = true;
    boot.hallOk = true;
    boot.rimOk = false;
    boot.motorsOn = false;
    FfbVersion::formatId(boot.fw, sizeof(boot.fw));
    strncpy(boot.ffbName, "off", sizeof(boot.ffbName) - 1);
    snprintf(boot.homeLine, sizeof(boot.homeLine), "home idle");
    boot.axleDeg = 0;
    boot.gear = DEFAULT_GEAR_RATIO;
    boot.cap = MOTOR_DUTY_CAP;
    boot.torq = 0;
    core1Painting = true;
    paintJob(boot);
    core1Painting = false;

    gCore1Ready = true;
}

void serviceCore1() {
    if (!gCore1Ready) return;

    PaintJob job{};
    bool doPaint = false;
    mutex_enter_blocking(&jobMu);
    if (hasJob) {
        job = queuedJob;
        hasJob = false;
        doPaint = true;
    }
    mutex_exit(&jobMu);

    if (!doPaint) return;

    core1Painting = true;
    paintJob(job);
    core1Painting = false;
}

void update(bool hallOk, float axleDeg) {
    if (!gEnabled || !gCore1Ready) return;

    const uint32_t now = millis();
    const FrameSig sig = makeSig(hallOk, axleDeg);
    const bool contentChanged = !lastSigValid || !sigEqual(sig, lastSig);
    const bool ghostDue = wantFullRefresh(/*forceFull=*/false);

    if (!gForce && !contentChanged && !ghostDue) return;
    if (!gForce && motorsBlockingAuto()) return;
    if (!gForce && (now - lastPaintMs < kMinPaintIntervalMs)) return;

    const bool forceFull = gForce || (!contentChanged && ghostDue) || wantFullRefresh(gForce);

    PaintJob job{};
    fillJob(job, hallOk, axleDeg, forceFull, sig);

    // Optimistic bookkeeping on core0 so we don't spam the same frame.
    lastSig = sig;
    lastSigValid = true;
    lastPaintMs = now;
    if (forceFull) {
        partialsSinceFull = 0;
        lastFullMs = now;
    } else {
        ++partialsSinceFull;
    }
    gForce = false;

    queueJob(job);
}

void requestRefresh() { lastSigValid = false; }

void forceRefresh() {
    gForce = true;
    lastSigValid = false;
}

bool enabled() { return gEnabled; }

void setEnabled(bool on) {
    gEnabled = on;
    if (on) lastSigValid = false;
}

}  // namespace EpdStatus

#endif  // ENABLE_BASE_EPD
