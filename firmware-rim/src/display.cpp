#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <math.h>
#include <pico/mutex.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "disp_assets.h"
#include "display_store.h"

namespace Display {
namespace {

mutex_t mu;
bool muReady = false;
bool telValid = false;
bool powerSave = false;
bool tftOk = false;
bool lastPowerSave = false;
bool lastValid = false;
bool needFullRedraw = true;
uint8_t bright = 180;
uint8_t bgTheme = FfbLink::DispBgBlack;
uint8_t layoutCount = 0;
FfbLink::DisplayElement layout[FfbLink::kDispElementMax]{};
FfbLink::TelemetryPayload tel{};
FfbLink::TelemetryPayload drawnTel{};
uint8_t drawnLayoutCount = 0xFF;
uint8_t drawnBright = 0xFF;
uint8_t drawnBg = 0xFF;
uint8_t drawnPage = 0xFF;

Adafruit_ILI9341* tft = nullptr;

constexpr float kPi = 3.14159265f;
constexpr uint16_t kRpmFull = 9000;
constexpr uint16_t kSpeedFullKph = 300;
constexpr uint16_t kTrackGray = 0x4208; // dark gray track
constexpr uint16_t kPanelDim = 0x2104;

void ensureMu() {
    if (muReady)
        return;
    mutex_init(&mu);
    muReady = true;
}

void loadActivePageLocked() {
    const FfbLink::DisplayPage& p = DisplayStore::cpage(DisplayStore::activePage());
    bgTheme = p.bgTheme;
    layoutCount = p.layoutCount;
    if (layoutCount > FfbLink::kDispElementMax)
        layoutCount = FfbLink::kDispElementMax;
    memcpy(layout, p.layout, sizeof(layout));
    needFullRedraw = true;
}

uint16_t btnW(uint8_t sz) {
    return (uint16_t)(36 + sz * 12);
}
uint16_t btnH(uint8_t sz) {
    return (uint16_t)(18 + sz * 6);
}

void drawButton(int16_t x, int16_t y, uint8_t sz, uint16_t color, const char* label) {
    const uint16_t w = btnW(sz);
    const uint16_t h = btnH(sz);
    tft->fillRoundRect(x, y, w, h, 4, kTrackGray);
    tft->drawRoundRect(x, y, w, h, 4, color);
    tft->setTextSize(sz > 2 ? 2 : 1);
    tft->setTextColor(color);
    const int16_t tw = (int16_t)(strlen(label) * 6 * (sz > 2 ? 2 : 1));
    tft->setCursor(x + (int16_t)(w - tw) / 2, y + (int16_t)(h / 2) - 4 * (sz > 2 ? 2 : 1));
    tft->print(label);
}

void drawPageDots() {
    const uint8_t n = DisplayStore::pageCount();
    const uint8_t cur = DisplayStore::activePage();
    if (n <= 1)
        return;
    const int16_t total = (int16_t)(n * 12 - 4);
    int16_t x = (int16_t)(FfbLink::kDispWidth - total) / 2;
    const int16_t y = (int16_t)FfbLink::kDispHeight - 10;
    for (uint8_t i = 0; i < n; ++i) {
        const uint16_t c = (i == cur) ? (uint16_t)0xFFFF : kTrackGray;
        tft->fillCircle(x + 3, y, 3, c);
        x += 12;
    }
}

const char* buttonLabel(uint8_t type) {
    switch (type) {
    case FfbLink::DispBtnPrev:
        return "<";
    case FfbLink::DispBtnNext:
        return ">";
    case FfbLink::DispBtnPage0:
        return "1";
    case FfbLink::DispBtnPage1:
        return "2";
    case FfbLink::DispBtnPage2:
        return "3";
    default:
        return "?";
    }
}

void applyBacklight(uint8_t b, bool off) {
    pinMode(PIN_TFT_BL, OUTPUT);
    if (off) {
        analogWrite(PIN_TFT_BL, 0);
        return;
    }
    analogWrite(PIN_TFT_BL, b);
}

uint8_t clampSize(uint8_t sz) {
    if (sz < 1)
        return 1;
    if (sz > 4)
        return 4;
    return sz;
}

float clampf01(float v) {
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

void formatGear(char* buf, size_t n, int8_t gear) {
    if (gear < 0) {
        snprintf(buf, n, "R");
    } else if (gear == 0) {
        snprintf(buf, n, "N");
    } else {
        snprintf(buf, n, "%d", (int)gear);
    }
}

void formatLap(char* buf, size_t n, uint16_t ms) {
    const unsigned sec = ms / 1000u;
    const unsigned rem = ms % 1000u;
    const unsigned min = sec / 60u;
    const unsigned s = sec % 60u;
    snprintf(buf, n, "%u:%02u.%03u", min, s, rem);
}

void formatFlags(char* buf, size_t n, uint8_t flags) {
    buf[0] = '\0';
    size_t used = 0;
    auto append = [&](const char* s) {
        if (used == 0) {
        } else if (used + 1 < n) {
            buf[used++] = ' ';
            buf[used] = '\0';
        } else {
            return;
        }
        const size_t len = strlen(s);
        if (used + len >= n)
            return;
        memcpy(buf + used, s, len + 1);
        used += len;
    };
    if (flags & FfbLink::TelRed)
        append("RED");
    if (flags & FfbLink::TelYellow)
        append("YEL");
    if (flags & FfbLink::TelBlue)
        append("BLU");
    if (flags & FfbLink::TelPit)
        append("PIT");
    if (flags & FfbLink::TelTc)
        append("TC");
    if (flags & FfbLink::TelAbs)
        append("ABS");
    if (used == 0)
        snprintf(buf, n, "-");
}

bool elementText(char* buf, size_t n, uint8_t type, const FfbLink::TelemetryPayload& t) {
    switch (type) {
    case FfbLink::DispGear:
        formatGear(buf, n, t.gear);
        return true;
    case FfbLink::DispSpeed:
        snprintf(buf, n, "%.0f kph", (double)t.speedKphx10 / 10.0);
        return true;
    case FfbLink::DispRpm:
        snprintf(buf, n, "%u rpm", (unsigned)t.rpm);
        return true;
    case FfbLink::DispFuel:
        snprintf(buf, n, "%.0f%%", (double)t.fuelPctx10 / 10.0);
        return true;
    case FfbLink::DispLapTime:
        formatLap(buf, n, t.lapTimeMs);
        return true;
    case FfbLink::DispLastLap: {
        char lap[16];
        if (t.lastLapMs == 0) {
            snprintf(buf, n, "L --:--.---");
        } else {
            formatLap(lap, sizeof(lap), t.lastLapMs);
            snprintf(buf, n, "L %s", lap);
        }
        return true;
    }
    case FfbLink::DispBestLap: {
        char lap[16];
        if (t.bestLapMs == 0) {
            snprintf(buf, n, "B --:--.---");
        } else {
            formatLap(lap, sizeof(lap), t.bestLapMs);
            snprintf(buf, n, "B %s", lap);
        }
        return true;
    }
    case FfbLink::DispFlags:
        formatFlags(buf, n, t.flags);
        return true;
    default:
        return false;
    }
}

void formatSignedGap(char* buf, size_t n, int16_t ms, const char* prefix) {
    if (ms == FfbLink::kTelemetryGapNa) {
        snprintf(buf, n, "%s--.-s", prefix ? prefix : "");
        return;
    }
    const int absMs = ms < 0 ? -ms : ms;
    const int whole = absMs / 1000;
    const int frac = (absMs % 1000) / 10; // hundredths
    snprintf(buf, n, "%s%c%d.%02ds", prefix ? prefix : "", ms < 0 ? '-' : '+', whole, frac);
}

uint16_t barWidth(uint8_t sz) {
    return (uint16_t)(72 + sz * 40);
}
uint16_t barHeight(uint8_t sz) {
    return (uint16_t)(4 + sz * 5);
}
uint16_t vBarWidth(uint8_t sz) {
    return (uint16_t)(10 + sz * 6);
}
uint16_t vBarHeight(uint8_t sz) {
    return (uint16_t)(60 + sz * 28);
}
uint16_t gaugeRadius(uint8_t sz) {
    return (uint16_t)(22 + sz * 10);
}
uint16_t panelW(uint8_t sz) {
    return (uint16_t)(48 + sz * 36);
}
uint16_t panelH(uint8_t sz) {
    return (uint16_t)(28 + sz * 20);
}
uint16_t bannerH(uint8_t sz) {
    return (uint16_t)(8 + sz * 6);
}
// Tall vertical tyre/brake cells (sidewall-style, not squares / horizontals).
uint16_t heatCellW(uint8_t sz) {
    return (uint16_t)(20 + sz * 8);
}
uint16_t heatCellH(uint8_t sz) {
    return (uint16_t)(34 + sz * 12);
}
uint16_t sectorBarW(uint8_t sz) {
    return (uint16_t)(120 + sz * 40);
}
uint16_t sectorBarH(uint8_t sz) {
    return (uint16_t)(10 + sz * 4);
}
uint16_t tyreCardW(uint8_t sz) {
    return (uint16_t)(34 + sz * 10);
}
uint16_t tyreCardH(uint8_t sz) {
    return (uint16_t)(40 + sz * 10);
}
uint16_t brakeBarW(uint8_t sz) {
    return (uint16_t)(16 + sz * 5);
}
uint16_t brakeBarH(uint8_t sz) {
    return (uint16_t)(24 + sz * 6);
}

float rpmNorm(uint16_t rpm) {
    return clampf01((float)rpm / (float)kRpmFull);
}
float speedNorm(int16_t speedX10) {
    return clampf01((float)speedX10 / 10.0f / (float)kSpeedFullKph);
}
float fuelNorm(uint16_t fuelX10) {
    return clampf01((float)fuelX10 / 1000.0f);
}

// RGB888 → RGB565
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint16_t lerpHeat(float t, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1) {
    t = clampf01(t);
    const uint8_t r = (uint8_t)lroundf(r0 + (r1 - r0) * t);
    const uint8_t g = (uint8_t)lroundf(g0 + (g1 - g0) * t);
    const uint8_t b = (uint8_t)lroundf(b0 + (b1 - b0) * t);
    return rgb565(r, g, b);
}

// Tyre °C: cold→blue, ~90 green, hot→red
uint16_t colourTyreTemp(uint8_t c) {
    if (c < 60)
        return lerpHeat(c / 60.0f, 20, 40, 180, 40, 160, 255);
    if (c < 85)
        return lerpHeat((c - 60) / 25.0f, 40, 160, 255, 40, 200, 60);
    if (c < 105)
        return lerpHeat((c - 85) / 20.0f, 40, 200, 60, 240, 200, 20);
    return lerpHeat(clampf01((c - 105) / 40.0f), 240, 200, 20, 255, 40, 20);
}

// Tyre PSI: low→blue, ~27–30 green, high→magenta/red
uint16_t colourTyrePress(uint8_t psi) {
    if (psi < 20)
        return lerpHeat(psi / 20.0f, 30, 60, 200, 80, 140, 255);
    if (psi < 27)
        return lerpHeat((psi - 20) / 7.0f, 80, 140, 255, 40, 200, 60);
    if (psi < 32)
        return lerpHeat((psi - 27) / 5.0f, 40, 200, 60, 240, 200, 20);
    return lerpHeat(clampf01((psi - 32) / 20.0f), 240, 200, 20, 255, 40, 80);
}

// Brake °C: cool dark → warm → glowing hot
uint16_t colourBrakeTemp(uint16_t c) {
    if (c < 100)
        return lerpHeat(c / 100.0f, 30, 30, 40, 40, 120, 200);
    if (c < 300)
        return lerpHeat((c - 100) / 200.0f, 40, 120, 200, 40, 200, 60);
    if (c < 500)
        return lerpHeat((c - 300) / 200.0f, 40, 200, 60, 255, 160, 20);
    if (c < 700)
        return lerpHeat((c - 500) / 200.0f, 255, 160, 20, 255, 40, 20);
    return lerpHeat(clampf01((c - 700) / 300.0f), 255, 40, 20, 255, 220, 200);
}

void drawHeatBox(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t fill, const char* label) {
    tft->fillRoundRect(x, y, w, h, 2, fill);
    tft->drawRoundRect(x, y, w, h, 2, kPanelDim);
    if (label && label[0]) {
        tft->setTextSize(1);
        tft->setTextColor(0xFFFF);
        const int16_t tw = (int16_t)(strlen(label) * 6);
        tft->setCursor(x + (int16_t)(w - tw) / 2, y + (int16_t)(h / 2) - 4);
        tft->print(label);
    }
}

void drawHeatQuad(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t gap, uint16_t c0,
                  uint16_t c1, uint16_t c2, uint16_t c3, const char* l0, const char* l1,
                  const char* l2, const char* l3) {
    drawHeatBox(x, y, w, h, c0, l0);
    drawHeatBox((int16_t)(x + w + gap), y, w, h, c1, l1);
    drawHeatBox(x, (int16_t)(y + h + gap), w, h, c2, l2);
    drawHeatBox((int16_t)(x + w + gap), (int16_t)(y + h + gap), w, h, c3, l3);
}

// One vertical tyre card: corner tag, °C (heat fill), PSI footer — TWF-style.
void drawTyreCard(int16_t x, int16_t y, uint16_t w, uint16_t h, const char* corner, uint8_t tempC,
                  uint8_t psi) {
    const uint16_t fill = colourTyreTemp(tempC);
    const uint16_t foot = (uint16_t)(h / 3);
    const uint16_t body = (uint16_t)(h - foot);
    tft->fillRoundRect(x, y, w, body, 3, fill);
    tft->fillRoundRect(x, (int16_t)(y + body - 2), w, (uint16_t)(foot + 2), 3,
                       colourTyrePress(psi));
    tft->drawRoundRect(x, y, w, h, 3, kPanelDim);
    tft->setTextSize(1);
    tft->setTextColor(0xFFFF);
    if (corner && corner[0]) {
        const int16_t tw = (int16_t)(strlen(corner) * 6);
        tft->setCursor(x + (int16_t)(w - tw) / 2, y + 3);
        tft->print(corner);
    }
    char lab[10];
    snprintf(lab, sizeof(lab), "%uC", tempC);
    {
        const int16_t tw = (int16_t)(strlen(lab) * 6);
        tft->setCursor(x + (int16_t)(w - tw) / 2, y + (int16_t)(body / 2) - 2);
        tft->print(lab);
    }
    snprintf(lab, sizeof(lab), "%upsi", psi);
    {
        const int16_t tw = (int16_t)(strlen(lab) * 6);
        tft->setCursor(x + (int16_t)(w - tw) / 2, (int16_t)(y + body + foot / 2 - 4));
        tft->print(lab);
    }
}

void drawTyreCar(int16_t x, int16_t y, uint8_t sz, const FfbLink::TelemetryPayload& t) {
    const uint16_t w = tyreCardW(sz);
    const uint16_t h = tyreCardH(sz);
    const uint16_t gap = 6;
    static const char* kCorners[4] = {"FL", "FR", "RL", "RR"};
    for (uint8_t i = 0; i < 4; ++i) {
        const uint8_t col = i & 1u;
        const uint8_t row = i >> 1;
        drawTyreCard((int16_t)(x + col * (w + gap)), (int16_t)(y + row * (h + gap)), w, h,
                     kCorners[i], t.tyreTempC[i], t.tyrePressPsi[i]);
    }
}

void drawBrakeCar(int16_t x, int16_t y, uint8_t sz, const FfbLink::TelemetryPayload& t) {
    // Four tall thin bars in a row; corner tag inside top, °C under bar (no overhead overlap).
    const uint16_t w = brakeBarW(sz);
    const uint16_t h = brakeBarH(sz);
    const uint16_t gap = (uint16_t)(14 + sz * 6);
    static const char* kCorners[4] = {"FL", "FR", "RL", "RR"};
    for (uint8_t i = 0; i < 4; ++i) {
        const int16_t bx = (int16_t)(x + i * (w + gap));
        const float frac = clampf01((float)t.brakeTempC[i] / 800.0f);
        tft->fillRoundRect(bx, y, w, h, 2, kTrackGray);
        tft->drawRoundRect(bx, y, w, h, 2, kPanelDim);
        const uint16_t fillH = (uint16_t)lroundf(frac * (float)(h - 2));
        if (fillH > 0) {
            tft->fillRect(bx + 1, (int16_t)(y + h - 1 - fillH), (uint16_t)(w - 2), fillH,
                          colourBrakeTemp(t.brakeTempC[i]));
        }
        tft->setTextSize(1);
        tft->setTextColor(0xFFFF);
        const int16_t tw = (int16_t)(strlen(kCorners[i]) * 6);
        tft->setCursor(bx + (int16_t)(w - tw) / 2, y + 2);
        tft->print(kCorners[i]);
        char lab[10];
        snprintf(lab, sizeof(lab), "%uC", t.brakeTempC[i]);
        const int16_t lw = (int16_t)(strlen(lab) * 6);
        tft->setCursor(bx + (int16_t)(w - lw) / 2, (int16_t)(y + h + 1));
        tft->print(lab);
    }
}

uint16_t deltaColor(int16_t ms) {
    if (ms == FfbLink::kTelemetryGapNa)
        return 0x8410;
    if (ms < 0)
        return 0x07E0; // ahead / faster — green
    if (ms > 0)
        return 0xF800; // behind — red
    return 0xFFFF;
}

void drawSectorSplit(int16_t x, int16_t y, uint8_t sz, int16_t deltaMs, uint16_t accent) {
    const uint16_t w = sectorBarW(sz);
    const uint16_t h = sectorBarH(sz);
    const int16_t mid = (int16_t)(x + w / 2);
    tft->fillRoundRect(x, y, w, h, 2, kTrackGray);
    tft->drawRoundRect(x, y, w, h, 2, kPanelDim);
    // Green left (faster), red right (slower)
    tft->fillRect(x + 1, y + 1, (uint16_t)(w / 2 - 1), (uint16_t)(h - 2), 0x0320);
    tft->fillRect(mid, y + 1, (uint16_t)(w / 2 - 1), (uint16_t)(h - 2), 0x4000);
    tft->drawFastVLine(mid, y, h, accent ? accent : (uint16_t)0xFFFF);

    if (deltaMs == FfbLink::kTelemetryGapNa)
        return;
    // Map ±2.0 s into bar; clamp.
    float t = (float)deltaMs / 2000.0f;
    if (t < -1.0f)
        t = -1.0f;
    if (t > 1.0f)
        t = 1.0f;
    const int16_t px = (int16_t)lroundf((float)mid + t * (float)(w / 2 - 3));
    const uint16_t mark = deltaColor(deltaMs);
    tft->fillTriangle(px, y - 1, px - 4, y + h + 2, px + 4, y + h + 2, mark);
    tft->fillRect(px - 1, y, 3, h, mark);
}

uint16_t flagColor(uint8_t flags) {
    if (flags & FfbLink::TelRed)
        return 0xF800;
    if (flags & FfbLink::TelYellow)
        return 0xFFE0;
    if (flags & FfbLink::TelBlue)
        return 0x001F;
    if (flags & FfbLink::TelPit)
        return 0xFE60;
    if (flags & FfbLink::TelTc)
        return 0xFD20;
    if (flags & FfbLink::TelAbs)
        return 0x07FF;
    return kTrackGray;
}

void drawHBar(int16_t x, int16_t y, uint16_t w, uint16_t h, float frac, uint16_t color) {
    tft->fillRect(x, y, w, h, kTrackGray);
    tft->drawRect(x, y, w, h, kPanelDim);
    const uint16_t fill = (uint16_t)lroundf(frac * (float)(w - 2));
    if (fill > 0) {
        tft->fillRect(x + 1, y + 1, fill, h - 2, color);
    }
}

void drawVBar(int16_t x, int16_t y, uint16_t w, uint16_t h, float frac, uint16_t color) {
    tft->fillRect(x, y, w, h, kTrackGray);
    tft->drawRect(x, y, w, h, kPanelDim);
    const uint16_t fill = (uint16_t)lroundf(frac * (float)(h - 2));
    if (fill > 0) {
        tft->fillRect(x + 1, (int16_t)(y + h - 1 - fill), w - 2, fill, color);
    }
}

void drawArcGauge(int16_t cx, int16_t cy, uint16_t r, float frac, uint16_t color) {
    // Bottom semicircle: 180° (left) → 0° (right), fill clockwise with value.
    const float start = kPi; // π
    const float span = kPi;  // 180°
    const int steps = 36;
    const int filled = (int)lroundf(frac * (float)steps);

    auto pt = [&](float a, int16_t& ox, int16_t& oy) {
        ox = (int16_t)lroundf((float)cx + cosf(a) * (float)r);
        oy = (int16_t)lroundf((float)cy - sinf(a) * (float)r);
    };

    int16_t px, py, qx, qy;
    pt(start, px, py);
    for (int i = 1; i <= steps; ++i) {
        const float a = start - span * ((float)i / (float)steps);
        pt(a, qx, qy);
        const uint16_t c = (i <= filled) ? color : kTrackGray;
        tft->drawLine(px, py, qx, qy, c);
        if (r > 8) {
            // thicken
            tft->drawLine(px, py + 1, qx, qy + 1, c);
        }
        px = qx;
        py = qy;
    }
    // Needle
    const float na = start - span * frac;
    int16_t nx, ny;
    pt(na, nx, ny);
    tft->drawLine(cx, cy, nx, ny, color);
    tft->fillCircle(cx, cy, 3, color);
}

void drawStandby() {
    if (!tft)
        return;
    tft->fillScreen(ILI9341_BLACK);
    tft->setTextColor(ILI9341_WHITE);
    tft->setTextSize(2);
    tft->setCursor(36, 100);
    tft->print("Waiting for");
    tft->setCursor(48, 128);
    tft->print("Telemetry");
}

void drawElement(const FfbLink::DisplayElement& el, const FfbLink::TelemetryPayload& t) {
    if (!tft || el.type == FfbLink::DispNone)
        return;
    const uint8_t sz = clampSize(el.fontSize);
    const int16_t x = (int16_t)el.x;
    const int16_t y = (int16_t)el.y;
    const uint16_t color = el.color565 ? el.color565 : (uint16_t)0xFFFF;

    if (DispAssets::isIconType(el.type)) {
        DispAssets::drawIcon(*tft, el.type, x, y, sz, color);
        return;
    }
    if (DispAssets::isButtonType(el.type)) {
        drawButton(x, y, sz, color, buttonLabel(el.type));
        return;
    }

    char text[24];
    if (elementText(text, sizeof(text), el.type, t)) {
        tft->setTextSize(sz);
        tft->setTextColor(color);
        tft->setCursor(x, y);
        tft->print(text);
        return;
    }

    switch (el.type) {
    case FfbLink::DispRpmBarH:
        drawHBar(x, y, barWidth(sz), barHeight(sz), rpmNorm(t.rpm), color);
        break;
    case FfbLink::DispFuelBarH:
        drawHBar(x, y, barWidth(sz), barHeight(sz), fuelNorm(t.fuelPctx10), color);
        break;
    case FfbLink::DispRpmBarV:
        drawVBar(x, y, vBarWidth(sz), vBarHeight(sz), rpmNorm(t.rpm), color);
        break;
    case FfbLink::DispFuelBarV:
        drawVBar(x, y, vBarWidth(sz), vBarHeight(sz), fuelNorm(t.fuelPctx10), color);
        break;
    case FfbLink::DispRpmGauge:
        drawArcGauge(x, y, gaugeRadius(sz), rpmNorm(t.rpm), color);
        break;
    case FfbLink::DispSpeedGauge:
        drawArcGauge(x, y, gaugeRadius(sz), speedNorm(t.speedKphx10), color);
        break;
    case FfbLink::DispFuelGauge:
        drawArcGauge(x, y, gaugeRadius(sz), fuelNorm(t.fuelPctx10), color);
        break;
    case FfbLink::DispFlagBanner: {
        const uint16_t h = bannerH(sz);
        const uint16_t w = (uint16_t)(FfbLink::kDispWidth - x);
        tft->fillRect(x, y, w > 8 ? w : 8, h, flagColor(t.flags));
        break;
    }
    case FfbLink::DispGearBadge: {
        const uint16_t tw = (uint16_t)(18 + sz * 14);
        const uint16_t th = (uint16_t)(18 + sz * 12);
        tft->fillRoundRect(x, y, tw, th, 4, kTrackGray);
        tft->drawRoundRect(x, y, tw, th, 4, color);
        formatGear(text, sizeof(text), t.gear);
        tft->setTextSize(sz);
        tft->setTextColor(color);
        tft->setCursor(x + 4 + sz, y + 4 + sz);
        tft->print(text);
        break;
    }
    case FfbLink::DispPanel:
        tft->fillRoundRect(x, y, panelW(sz), panelH(sz), 4, color);
        break;
    case FfbLink::DispTyreTempQuad: {
        const uint16_t cw = heatCellW(sz);
        const uint16_t ch = heatCellH(sz);
        char a[8], b[8], c[8], d[8];
        snprintf(a, sizeof(a), "%uC", t.tyreTempC[0]);
        snprintf(b, sizeof(b), "%uC", t.tyreTempC[1]);
        snprintf(c, sizeof(c), "%uC", t.tyreTempC[2]);
        snprintf(d, sizeof(d), "%uC", t.tyreTempC[3]);
        drawHeatQuad(x, y, cw, ch, 3, colourTyreTemp(t.tyreTempC[0]),
                     colourTyreTemp(t.tyreTempC[1]), colourTyreTemp(t.tyreTempC[2]),
                     colourTyreTemp(t.tyreTempC[3]), a, b, c, d);
        break;
    }
    case FfbLink::DispTyrePressQuad: {
        const uint16_t cw = heatCellW(sz);
        const uint16_t ch = heatCellH(sz);
        char a[8], b[8], c[8], d[8];
        snprintf(a, sizeof(a), "%upsi", t.tyrePressPsi[0]);
        snprintf(b, sizeof(b), "%upsi", t.tyrePressPsi[1]);
        snprintf(c, sizeof(c), "%upsi", t.tyrePressPsi[2]);
        snprintf(d, sizeof(d), "%upsi", t.tyrePressPsi[3]);
        drawHeatQuad(x, y, cw, ch, 3, colourTyrePress(t.tyrePressPsi[0]),
                     colourTyrePress(t.tyrePressPsi[1]), colourTyrePress(t.tyrePressPsi[2]),
                     colourTyrePress(t.tyrePressPsi[3]), a, b, c, d);
        break;
    }
    case FfbLink::DispBrakeTempQuad: {
        const uint16_t cw = heatCellW(sz);
        const uint16_t ch = heatCellH(sz);
        char a[8], b[8], c[8], d[8];
        snprintf(a, sizeof(a), "%uC", t.brakeTempC[0]);
        snprintf(b, sizeof(b), "%uC", t.brakeTempC[1]);
        snprintf(c, sizeof(c), "%uC", t.brakeTempC[2]);
        snprintf(d, sizeof(d), "%uC", t.brakeTempC[3]);
        drawHeatQuad(x, y, cw, ch, 3, colourBrakeTemp(t.brakeTempC[0]),
                     colourBrakeTemp(t.brakeTempC[1]), colourBrakeTemp(t.brakeTempC[2]),
                     colourBrakeTemp(t.brakeTempC[3]), a, b, c, d);
        break;
    }
    case FfbLink::DispTyreTempFL:
    case FfbLink::DispTyreTempFR:
    case FfbLink::DispTyreTempRL:
    case FfbLink::DispTyreTempRR: {
        const uint8_t i = (uint8_t)(el.type - FfbLink::DispTyreTempFL);
        char lab[8];
        snprintf(lab, sizeof(lab), "%uC", t.tyreTempC[i]);
        drawHeatBox(x, y, heatCellW(sz), heatCellH(sz), colourTyreTemp(t.tyreTempC[i]), lab);
        break;
    }
    case FfbLink::DispTyrePressFL:
    case FfbLink::DispTyrePressFR:
    case FfbLink::DispTyrePressRL:
    case FfbLink::DispTyrePressRR: {
        const uint8_t i = (uint8_t)(el.type - FfbLink::DispTyrePressFL);
        char lab[10];
        snprintf(lab, sizeof(lab), "%upsi", t.tyrePressPsi[i]);
        drawHeatBox(x, y, heatCellW(sz), heatCellH(sz), colourTyrePress(t.tyrePressPsi[i]), lab);
        break;
    }
    case FfbLink::DispBrakeTempFL:
    case FfbLink::DispBrakeTempFR:
    case FfbLink::DispBrakeTempRL:
    case FfbLink::DispBrakeTempRR: {
        const uint8_t i = (uint8_t)(el.type - FfbLink::DispBrakeTempFL);
        char lab[8];
        snprintf(lab, sizeof(lab), "%uC", t.brakeTempC[i]);
        drawHeatBox(x, y, heatCellW(sz), heatCellH(sz), colourBrakeTemp(t.brakeTempC[i]), lab);
        break;
    }
    case FfbLink::DispDeltaBest: {
        formatSignedGap(text, sizeof(text), t.deltaBestMs, "PB ");
        tft->setTextSize(sz);
        tft->setTextColor(deltaColor(t.deltaBestMs));
        tft->setCursor(x, y);
        tft->print(text);
        break;
    }
    case FfbLink::DispDeltaP1: {
        formatSignedGap(text, sizeof(text), t.deltaP1Ms, "P1 ");
        tft->setTextSize(sz);
        tft->setTextColor(deltaColor(t.deltaP1Ms));
        tft->setCursor(x, y);
        tft->print(text);
        break;
    }
    case FfbLink::DispGapAhead: {
        formatSignedGap(text, sizeof(text), t.gapAheadMs, "^ ");
        tft->setTextSize(sz);
        tft->setTextColor(color);
        tft->setCursor(x, y);
        tft->print(text);
        break;
    }
    case FfbLink::DispGapBehind: {
        formatSignedGap(text, sizeof(text), t.gapBehindMs, "v ");
        tft->setTextSize(sz);
        tft->setTextColor(color);
        tft->setCursor(x, y);
        tft->print(text);
        break;
    }
    case FfbLink::DispGapStack: {
        formatSignedGap(text, sizeof(text), t.gapAheadMs, "^ ");
        tft->setTextSize(sz);
        tft->setTextColor(color);
        tft->setCursor(x, y);
        tft->print(text);
        formatSignedGap(text, sizeof(text), t.gapBehindMs, "v ");
        tft->setCursor(x, (int16_t)(y + 8 * sz + 4));
        tft->print(text);
        break;
    }
    case FfbLink::DispSectorSplitBest:
        drawSectorSplit(x, y, sz, t.deltaBestMs, color);
        break;
    case FfbLink::DispSectorSplitP1:
        drawSectorSplit(x, y, sz, t.deltaP1Ms, color);
        break;
    case FfbLink::DispTyreCar:
        drawTyreCar(x, y, sz, t);
        break;
    case FfbLink::DispBrakeCar:
        drawBrakeCar(x, y, sz, t);
        break;
    default:
        break;
    }
}

void drawLayout(const FfbLink::TelemetryPayload& t, uint8_t count,
                const FfbLink::DisplayElement* els, uint8_t theme) {
    if (!tft)
        return;
    DispAssets::drawBackground(*tft, theme);
    for (uint8_t i = 0; i < count; ++i) {
        if (els[i].type == FfbLink::DispPanel)
            drawElement(els[i], t);
    }
    for (uint8_t i = 0; i < count; ++i) {
        if (els[i].type != FfbLink::DispPanel && !DispAssets::isButtonType(els[i].type)) {
            drawElement(els[i], t);
        }
    }
    for (uint8_t i = 0; i < count; ++i) {
        if (DispAssets::isButtonType(els[i].type))
            drawElement(els[i], t);
    }
    drawPageDots();
}

} // namespace

void begin() {
    ensureMu();
    DisplayStore::begin();
    mutex_enter_blocking(&mu);
    loadActivePageLocked();
    mutex_exit(&mu);
}

void beginCore1() {
    ensureMu();
    // Soft-fail: missing TFT must not hang Core1 (begin still runs; no probe ACK).
    SPI1.setRX(PIN_TFT_MISO);
    SPI1.setCS(PIN_TFT_CS);
    SPI1.setSCK(PIN_TFT_SCLK);
    SPI1.setTX(PIN_TFT_MOSI);
    SPI1.begin();
    tft = new Adafruit_ILI9341(&SPI1, PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_RST);
    tft->begin();
    tft->setRotation(1); // landscape 320×240
    tftOk = true;
    applyBacklight(bright, false);
    needFullRedraw = true;
    drawStandby();
}

void setTelemetryValid(bool valid) {
    ensureMu();
    mutex_enter_blocking(&mu);
    if (telValid != valid)
        needFullRedraw = true;
    telValid = valid;
    if (!valid) {
        tel = FfbLink::TelemetryPayload{};
    }
    mutex_exit(&mu);
}

void setTelemetry(const FfbLink::TelemetryPayload& t) {
    ensureMu();
    mutex_enter_blocking(&mu);
    tel = t;
    telValid = true;
    mutex_exit(&mu);
}

void setPowerSave(bool on) {
    ensureMu();
    mutex_enter_blocking(&mu);
    if (powerSave != on)
        needFullRedraw = true;
    powerSave = on;
    mutex_exit(&mu);
}

void setConfig(const FfbLink::RimConfig& cfg) {
    ensureMu();
    mutex_enter_blocking(&mu);
    bright = cfg.dispBright;
    needFullRedraw = true;
    mutex_exit(&mu);
}

void update() {
    if (!muReady || !tftOk || !tft)
        return;

    mutex_enter_blocking(&mu);
    const bool valid = telValid;
    const bool ps = powerSave;
    const bool redraw = needFullRedraw;
    const uint8_t b = bright;
    const uint8_t count = layoutCount;
    const uint8_t theme = bgTheme;
    const uint8_t page = DisplayStore::activePage();
    FfbLink::DisplayElement els[FfbLink::kDispElementMax];
    memcpy(els, layout, sizeof(els));
    FfbLink::TelemetryPayload t = tel;
    needFullRedraw = false;
    mutex_exit(&mu);

    if (ps) {
        if (!lastPowerSave || redraw) {
            applyBacklight(b, true);
            tft->fillScreen(ILI9341_BLACK);
            lastPowerSave = true;
            lastValid = false;
            drawnLayoutCount = 0xFF;
        }
        return;
    }

    if (lastPowerSave || drawnBright != b) {
        applyBacklight(b, false);
        drawnBright = b;
        lastPowerSave = false;
    }

    const bool layoutChanged =
        (drawnLayoutCount != count) || (drawnBg != theme) || (drawnPage != page) || redraw;
    const bool telChanged = memcmp(&t, &drawnTel, sizeof(t)) != 0;

    if (!valid) {
        if (lastValid || layoutChanged || redraw) {
            drawStandby();
            lastValid = false;
            drawnLayoutCount = 0xFF;
            drawnBg = 0xFF;
            drawnPage = 0xFF;
            drawnTel = FfbLink::TelemetryPayload{};
        }
        return;
    }

    if (!lastValid || layoutChanged || telChanged || redraw) {
        drawLayout(t, count, els, theme);
        drawnTel = t;
        drawnLayoutCount = count;
        drawnBg = theme;
        drawnPage = page;
        lastValid = true;
    }
}

bool telemetryValid() {
    if (!muReady)
        return false;
    mutex_enter_blocking(&mu);
    const bool v = telValid;
    mutex_exit(&mu);
    return v;
}

bool present() {
    return tftOk;
}

uint8_t activePage() {
    return DisplayStore::activePage();
}
uint8_t pageCount() {
    return DisplayStore::pageCount();
}

bool setActivePage(uint8_t page) {
    ensureMu();
    mutex_enter_blocking(&mu);
    const bool ok = DisplayStore::setActivePage(page);
    if (ok)
        loadActivePageLocked();
    mutex_exit(&mu);
    return ok;
}

void setPageCount(uint8_t n) {
    ensureMu();
    mutex_enter_blocking(&mu);
    DisplayStore::setPageCount(n);
    loadActivePageLocked();
    mutex_exit(&mu);
}

bool onTap(int16_t x, int16_t y) {
    ensureMu();
    mutex_enter_blocking(&mu);
    const uint8_t count = layoutCount;
    FfbLink::DisplayElement els[FfbLink::kDispElementMax];
    memcpy(els, layout, sizeof(els));
    const uint8_t cur = DisplayStore::activePage();
    const uint8_t pages = DisplayStore::pageCount();
    mutex_exit(&mu);

    for (int i = (int)count - 1; i >= 0; --i) {
        const auto& el = els[i];
        if (!DispAssets::isButtonType(el.type))
            continue;
        const uint8_t sz = clampSize(el.fontSize);
        const uint16_t w = btnW(sz);
        const uint16_t h = btnH(sz);
        if (x < (int16_t)el.x || y < (int16_t)el.y)
            continue;
        if (x >= (int16_t)(el.x + w) || y >= (int16_t)(el.y + h))
            continue;
        switch (el.type) {
        case FfbLink::DispBtnPrev:
            if (cur > 0)
                setActivePage((uint8_t)(cur - 1));
            return true;
        case FfbLink::DispBtnNext:
            if (cur + 1 < pages)
                setActivePage((uint8_t)(cur + 1));
            return true;
        case FfbLink::DispBtnPage0:
            setActivePage(0);
            return true;
        case FfbLink::DispBtnPage1:
            if (pages > 1)
                setActivePage(1);
            return true;
        case FfbLink::DispBtnPage2:
            if (pages > 2)
                setActivePage(2);
            return true;
        default:
            break;
        }
    }
    return false;
}

bool onSwipe(int16_t dx) {
    if (dx <= -40) {
        const uint8_t cur = activePage();
        if (cur + 1 < pageCount())
            return setActivePage((uint8_t)(cur + 1));
    } else if (dx >= 40) {
        const uint8_t cur = activePage();
        if (cur > 0)
            return setActivePage((uint8_t)(cur - 1));
    }
    return false;
}

void setStorePageChunk(const FfbLink::DispPageChunkPayload& chunk) {
    ensureMu();
    mutex_enter_blocking(&mu);
    const bool complete = DisplayStore::applyChunk(chunk);
    if (complete && chunk.pageIndex == DisplayStore::activePage()) {
        loadActivePageLocked();
    }
    mutex_exit(&mu);
}

void setStorePageLegacy(const FfbLink::DispPageSetPayload& page) {
    FfbLink::DispPageChunkPayload chunk{};
    chunk.pageIndex = page.pageIndex;
    chunk.bgTheme = page.bgTheme;
    chunk.layoutCount = page.layoutCount > 8 ? 8 : page.layoutCount;
    chunk.start = 0;
    chunk.count = chunk.layoutCount;
    memcpy(chunk.elements, page.layout, chunk.count * sizeof(FfbLink::DisplayElement));
    setStorePageChunk(chunk);
}

void setStoreMeta(const FfbLink::DispMetaPayload& meta) {
    ensureMu();
    mutex_enter_blocking(&mu);
    DisplayStore::setPageCount(meta.pageCount);
    DisplayStore::setActivePage(meta.activePage);
    loadActivePageLocked();
    mutex_exit(&mu);
}

void getStoreMeta(FfbLink::DispMetaPayload& out) {
    out.pageCount = DisplayStore::pageCount();
    out.activePage = DisplayStore::activePage();
}

bool fillStorePageChunk(uint8_t pageIndex, uint8_t start, FfbLink::DispPageChunkPayload& out) {
    out = FfbLink::DispPageChunkPayload{};
    if (pageIndex >= FfbLink::kDispPageMax)
        return false;
    const FfbLink::DisplayPage& p = DisplayStore::cpage(pageIndex);
    if (start >= p.layoutCount && !(start == 0 && p.layoutCount == 0))
        return false;
    out.pageIndex = pageIndex;
    out.bgTheme = p.bgTheme;
    out.layoutCount = p.layoutCount;
    out.start = start;
    uint8_t remain = 0;
    if (p.layoutCount > start)
        remain = (uint8_t)(p.layoutCount - start);
    out.count = remain > FfbLink::kDispChunkElements ? FfbLink::kDispChunkElements : remain;
    if (out.count > 0) {
        memcpy(out.elements, p.layout + start, out.count * sizeof(FfbLink::DisplayElement));
    }
    return true;
}

} // namespace Display
