const { invoke } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;

const KEYS = [
  { key: "base_fw", label: "Base firmware", group: "version", readonly: true, text: true },
  { key: "rim_fw", label: "Rim firmware", group: "version", readonly: true, text: true },
  { key: "duty_cap", label: "Motor duty cap", step: "0.01", group: "ffb" },
  { key: "spring_k", label: "Spring K", step: "0.0001", group: "ffb" },
  { key: "spring_dz", label: "Spring deadzone (°)", step: "0.1", group: "ffb" },
  { key: "torque_cap", label: "Torque cap", step: "0.01", group: "ffb" },
  { key: "hid_range", label: "HID range (°)", step: "1", group: "ffb" },
  { key: "gear_ratio", label: "Gear ratio", step: "0.01", group: "ffb" },
  { key: "soft_limit_en", label: "Soft limit on", step: "1", group: "ffb" },
  { key: "soft_limit_deg", label: "Soft limit ° (0=½ HID)", step: "1", group: "ffb" },
  { key: "soft_limit_k", label: "Soft limit K", step: "0.001", group: "ffb" },
  { key: "adxl_present", label: "ADXL present (ro)", step: "1", group: "ffb", readonly: true },
  { key: "adxl_cal", label: "ADXL cal valid", step: "1", group: "ffb" },
  { key: "adxl_x_offset", label: "ADXL X offset (raw)", step: "1", group: "ffb" },
  { key: "rim_link", label: "Rim link (ro)", step: "1", group: "rim", readonly: true },
  { key: "panel_led_bright", label: "Panel LED brightness", step: "1", group: "leds" },
  { key: "shift_led_bright", label: "Shift LED brightness", step: "1", group: "leds" },
  { key: "shift_led_count", label: "Shift LED count", step: "1", group: "leds" },
  { key: "disp_bright", label: "Display brightness", step: "1", group: "display" },
  { key: "shift_rpm_0", label: "Shift RPM stage 0", step: "50", group: "leds" },
  { key: "shift_rpm_1", label: "Shift RPM stage 1", step: "50", group: "leds" },
  { key: "shift_rpm_2", label: "Shift RPM stage 2", step: "50", group: "leds" },
  { key: "shift_rpm_3", label: "Shift RPM stage 3 (full bar)", step: "50", group: "leds" },
  { key: "shift_rpm_4", label: "Shift RPM blink (red pair)", step: "50", group: "leds" },
  ...[0, 1, 2, 3].flatMap((n) => [
    { key: `enc${n}_mode`, label: `Enc ${n} mode (0=rel 1=hold 2=abs)`, step: "1", group: "encoders" },
    { key: `enc${n}_steps`, label: `Enc ${n} steps/click`, step: "1", group: "encoders" },
    { key: `enc${n}_accel`, label: `Enc ${n} accel on`, step: "1", group: "encoders" },
    { key: `enc${n}_thresh`, label: `Enc ${n} accel thresh`, step: "1", group: "encoders" },
    { key: `enc${n}_mult`, label: `Enc ${n} accel mult`, step: "1", group: "encoders" },
    { key: `enc${n}_pulse`, label: `Enc ${n} pulse/hold ms`, step: "1", group: "encoders" },
    { key: `enc${n}_invert`, label: `Enc ${n} invert`, step: "1", group: "encoders" },
    { key: `enc${n}_value`, label: `Enc ${n} abs value 0..100`, step: "1", group: "encoders" },
  ]),
];

const ADC_MAX = 4095;
const SPARK_LEN = 120;
const LAST_PORT_KEY = "ffb.lastPort";
const AUTO_CONNECT_KEY = "ffb.autoConnect";
const TAB_KEY = "ffb.tab";
let raceEnabled = false;
const THEME_KEY = "ffb.theme";
const DISP_DRAFT_KEY = "ffb.dispLayout";

const els = {};
const inputs = {};
const axleHistory = [];
let logPaused = false;
let unlistenLine = null;
let unlistenTelem = null;
let lastTelem = null;
let connected = false;
let connecting = false;
let userDisconnected = false;
let latestPorts = [];
let autoTimer = null;
let activeTab = "display";
let ignoreScrollEvent = false;
let themePref = "auto";
let mediaDark = null;
/** Last rim link seen in telemetry / dump — used to refresh rim_fw on late link-up. */
let lastRimLinked = false;
let rimRefreshBusy = false;

// --- TFT layout editor (320×240 landscape) ---
const DISP_W = 320;
const DISP_H = 240;
const DISP_MAX = 16;
const DISP_TYPES = {
  1: "Gear",
  2: "Speed",
  3: "RPM",
  4: "Fuel",
  5: "Lap",
  6: "Flags",
  7: "RPM bar",
  8: "Fuel bar",
  9: "RPM vert",
  10: "Fuel vert",
  11: "RPM gauge",
  12: "Speed gauge",
  13: "Fuel gauge",
  14: "Flag banner",
  15: "Gear badge",
  16: "Panel",
  17: "Tyre temp 2×2",
  18: "Tyre PSI 2×2",
  19: "Brake temp 2×2",
  20: "Tyre °C FL",
  21: "Tyre °C FR",
  22: "Tyre °C RL",
  23: "Tyre °C RR",
  24: "Tyre PSI FL",
  25: "Tyre PSI FR",
  26: "Tyre PSI RL",
  27: "Tyre PSI RR",
  28: "Brake °C FL",
  29: "Brake °C FR",
  30: "Brake °C RL",
  31: "Brake °C RR",
  32: "Icon RPM",
  33: "Icon fuel",
  34: "Icon speed",
  35: "Icon flag",
  36: "Icon tyre",
  37: "Icon brake",
  38: "Icon lap",
  40: "Btn prev",
  41: "Btn next",
  42: "Btn page 1",
  43: "Btn page 2",
  44: "Btn page 3",
  45: "Delta PB",
  46: "Delta P1",
  47: "Gap ahead",
  48: "Gap behind",
  49: "Gaps stack",
  50: "Split PB",
  51: "Split P1",
  52: "Last lap",
  53: "Best lap",
  54: "Tyre car",
  55: "Brake car",
};
const DISP_TEXT_TYPES = new Set([1, 2, 3, 4, 5, 6, 45, 46, 47, 48, 52, 53]);
const DISP_ICON_TYPES = new Set([32, 33, 34, 35, 36, 37, 38]);
const DISP_BTN_TYPES = new Set([40, 41, 42, 43, 44]);
const DISP_TIMING_TYPES = new Set([45, 46, 47, 48, 49, 50, 51, 52, 53]);
const DISP_BG_NAMES = { 0: "Black", 1: "Carbon", 2: "Navy", 3: "Grid" };
const DISP_SAMPLE = {
  rpm: 7120,
  speed: 182,
  fuel: 64,
  gear: 3,
  flags: 0x01,
  tyreTemp: [88, 92, 84, 86],
  tyrePsi: [27, 27, 26, 26],
  brakeTemp: [420, 390, 310, 280],
  deltaBestMs: -120,
  deltaP1Ms: 340,
  gapAheadMs: 420,
  gapBehindMs: 1150,
  lastLapMs: 83456,
  bestLapMs: 82100,
};

/** @type {{ type: number, x: number, y: number, fontSize: number, color565: number }[]} */
let dispLayout = [];
/** @type {{ bgTheme: number, elements: typeof dispLayout }[]} */
let dispPages = [];
let dispActivePage = 0;
let dispPageCount = 3;
let dispSelected = -1;
let dispDrag = null;

function isValidDispType(type) {
  return (
    (type >= 1 && type <= 31) ||
    (type >= 32 && type <= 38) ||
    (type >= 40 && type <= 55)
  );
}

function defaultDispPage0() {
  // Mirrors firmware defaultDisplayPage0() — drive + live delta.
  return {
    bgTheme: 1,
    elements: [
      { type: 14, x: 0, y: 0, fontSize: 2, color565: 0xffe0 },
      { type: 2, x: 12, y: 28, fontSize: 2, color565: 0x07e0 },
      { type: 15, x: 118, y: 28, fontSize: 4, color565: 0xffff },
      { type: 3, x: 228, y: 28, fontSize: 2, color565: 0xffe0 },
      { type: 45, x: 12, y: 72, fontSize: 2, color565: 0x07e0 },
      { type: 7, x: 36, y: 112, fontSize: 3, color565: 0xf800 },
      { type: 8, x: 36, y: 148, fontSize: 2, color565: 0x07ff },
      { type: 41, x: 252, y: 200, fontSize: 2, color565: 0xffff },
    ],
  };
}

function defaultDispPage1() {
  return {
    bgTheme: 2,
    elements: [
      { type: 14, x: 0, y: 0, fontSize: 1, color565: 0xffe0 },
      { type: 15, x: 8, y: 14, fontSize: 2, color565: 0xffff },
      { type: 45, x: 200, y: 18, fontSize: 1, color565: 0x07e0 },
      { type: 54, x: 86, y: 44, fontSize: 1, color565: 0xffff },
      { type: 55, x: 52, y: 156, fontSize: 1, color565: 0xfd20 },
      { type: 40, x: 8, y: 216, fontSize: 1, color565: 0xffff },
      { type: 41, x: 280, y: 216, fontSize: 1, color565: 0xffff },
    ],
  };
}

function defaultDispPage2() {
  return {
    bgTheme: 3,
    elements: [
      { type: 5, x: 12, y: 16, fontSize: 3, color565: 0xffff },
      { type: 52, x: 12, y: 52, fontSize: 2, color565: 0xc618 },
      { type: 53, x: 160, y: 52, fontSize: 2, color565: 0x07e0 },
      { type: 45, x: 12, y: 84, fontSize: 2, color565: 0x07e0 },
      { type: 46, x: 160, y: 84, fontSize: 2, color565: 0xffe0 },
      { type: 50, x: 28, y: 118, fontSize: 3, color565: 0xffff },
      { type: 49, x: 28, y: 152, fontSize: 2, color565: 0x07ff },
      { type: 40, x: 40, y: 200, fontSize: 2, color565: 0xffff },
    ],
  };
}

function defaultDispPages() {
  return [defaultDispPage0(), defaultDispPage1(), defaultDispPage2()];
}

function clonePage(page) {
  return {
    bgTheme: Math.max(0, Math.min(3, Number(page?.bgTheme) || 0)),
    elements: (page?.elements || []).map((el) => ({ ...el })).slice(0, DISP_MAX),
  };
}

function syncLayoutIntoPages() {
  if (!dispPages[dispActivePage]) {
    dispPages[dispActivePage] = { bgTheme: 0, elements: [] };
  }
  dispPages[dispActivePage].elements = dispLayout.map((el) => ({ ...el }));
  if (els.dispBg) {
    dispPages[dispActivePage].bgTheme = Math.max(
      0,
      Math.min(3, Number(els.dispBg.value) || 0),
    );
  }
}

function loadActivePageLayout() {
  const page = dispPages[dispActivePage] || { bgTheme: 0, elements: [] };
  dispLayout = (page.elements || []).map((el) => ({ ...el }));
  if (els.dispBg) els.dispBg.value = String(page.bgTheme ?? 0);
  dispSelected = -1;
}

function switchDispPage(next) {
  const n = Math.max(0, Math.min(2, Number(next) || 0));
  if (n === dispActivePage) return;
  syncLayoutIntoPages();
  dispActivePage = n;
  loadActivePageLayout();
  updateDispPageTabs();
  syncDispInspector();
  drawDispPreview();
  persistDispDraft();
}

function updateDispPageTabs() {
  for (const btn of document.querySelectorAll("#disp-page-tabs [data-disp-page]")) {
    const i = Number(btn.dataset.dispPage);
    btn.classList.toggle("primary", i === dispActivePage);
    btn.disabled = i >= dispPageCount;
    btn.hidden = i >= dispPageCount;
  }
  if (els.dispPages) els.dispPages.value = String(dispPageCount);
}

function rgb565ToHex(c) {
  const r = ((c >> 11) & 0x1f) * 255 / 31;
  const g = ((c >> 5) & 0x3f) * 255 / 63;
  const b = (c & 0x1f) * 255 / 31;
  const to2 = (n) => Math.round(n).toString(16).padStart(2, "0");
  return `#${to2(r)}${to2(g)}${to2(b)}`;
}

function hexToRgb565(hex) {
  const m = /^#?([0-9a-f]{6})$/i.exec(String(hex).trim());
  if (!m) return 0xffff;
  const n = parseInt(m[1], 16);
  const r = (n >> 16) & 0xff;
  const g = (n >> 8) & 0xff;
  const b = n & 0xff;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

function u16le(n) {
  return [n & 0xff, (n >> 8) & 0xff];
}

function readU16le(bytes, i) {
  return bytes[i] | (bytes[i + 1] << 8);
}

function encodeLayoutHex(elements) {
  const count = Math.min(DISP_MAX, elements.length);
  const bytes = new Uint8Array(1 + DISP_MAX * 8);
  bytes[0] = count;
  for (let i = 0; i < DISP_MAX; i++) {
    const el = elements[i] || { type: 0, x: 0, y: 0, fontSize: 1, color565: 0 };
    const o = 1 + i * 8;
    bytes[o] = el.type & 0xff;
    const xb = u16le(el.x & 0xffff);
    const yb = u16le(el.y & 0xffff);
    bytes[o + 1] = xb[0];
    bytes[o + 2] = xb[1];
    bytes[o + 3] = yb[0];
    bytes[o + 4] = yb[1];
    bytes[o + 5] = Math.max(1, Math.min(4, el.fontSize || 1));
    const cb = u16le(el.color565 & 0xffff);
    bytes[o + 6] = cb[0];
    bytes[o + 7] = cb[1];
  }
  return [...bytes].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function decodeLayoutHex(hex) {
  const clean = String(hex || "").trim().toLowerCase();
  // 16-widget blob = 258 hex chars; legacy 8-widget = 130.
  if (!/^[0-9a-f]+$/.test(clean) || (clean.length !== 258 && clean.length !== 130)) {
    return null;
  }
  const maxSlots = clean.length === 130 ? 8 : DISP_MAX;
  const bytes = new Uint8Array(1 + maxSlots * 8);
  for (let i = 0; i < bytes.length; i++) {
    bytes[i] = parseInt(clean.slice(i * 2, i * 2 + 2), 16);
  }
  const count = Math.min(DISP_MAX, maxSlots, bytes[0]);
  const out = [];
  for (let i = 0; i < count; i++) {
    const o = 1 + i * 8;
    const type = bytes[o];
    if (!isValidDispType(type)) continue;
    out.push({
      type,
      x: readU16le(bytes, o + 1),
      y: readU16le(bytes, o + 3),
      fontSize: Math.max(1, Math.min(4, bytes[o + 5] || 1)),
      color565: readU16le(bytes, o + 6),
    });
  }
  return out;
}

function previewSampleText(type) {
  switch (type) {
    case 1:
      return String(DISP_SAMPLE.gear);
    case 2:
      return `${DISP_SAMPLE.speed} kph`;
    case 3:
      return `${DISP_SAMPLE.rpm} rpm`;
    case 4:
      return `${DISP_SAMPLE.fuel}%`;
    case 5:
      return "1:23.456";
    case 6:
      return "YEL";
    case 15:
      return String(DISP_SAMPLE.gear);
    case 45:
      return formatGapPreview(DISP_SAMPLE.deltaBestMs, "PB ");
    case 46:
      return formatGapPreview(DISP_SAMPLE.deltaP1Ms, "P1 ");
    case 47:
      return formatGapPreview(DISP_SAMPLE.gapAheadMs, "^ ");
    case 48:
      return formatGapPreview(DISP_SAMPLE.gapBehindMs, "v ");
    case 52:
      return `L ${formatLapPreview(DISP_SAMPLE.lastLapMs)}`;
    case 53:
      return `B ${formatLapPreview(DISP_SAMPLE.bestLapMs)}`;
    default:
      return "";
  }
}

function formatLapPreview(ms) {
  if (!ms) return "--:--.---";
  const sec = Math.floor(ms / 1000);
  const rem = ms % 1000;
  const min = Math.floor(sec / 60);
  const s = sec % 60;
  return `${min}:${String(s).padStart(2, "0")}.${String(rem).padStart(3, "0")}`;
}

function formatGapPreview(ms, prefix) {
  if (ms == null || ms === -32768) return `${prefix || ""}--.-s`;
  const abs = Math.abs(ms);
  const whole = Math.floor(abs / 1000);
  const frac = Math.floor((abs % 1000) / 10);
  return `${prefix || ""}${ms < 0 ? "-" : "+"}${whole}.${String(frac).padStart(2, "0")}s`;
}

function deltaColorCss(ms) {
  if (ms == null || ms === -32768) return "#888";
  if (ms < 0) return "#00c853";
  if (ms > 0) return "#ff1744";
  return "#fff";
}

function widgetScale(sz) {
  return Math.max(1, Math.min(4, sz || 1));
}

function barSize(sz) {
  const s = widgetScale(sz);
  return { w: 72 + s * 40, h: 4 + s * 5 };
}

function vBarSize(sz) {
  const s = widgetScale(sz);
  return { w: 10 + s * 6, h: 60 + s * 28 };
}

function gaugeR(sz) {
  return 22 + widgetScale(sz) * 10;
}

function panelSize(sz) {
  const s = widgetScale(sz);
  return { w: 48 + s * 36, h: 28 + s * 20 };
}

function badgeSize(sz) {
  const s = widgetScale(sz);
  return { w: 18 + s * 14, h: 18 + s * 12 };
}

function bannerSize(sz, x) {
  return { w: Math.max(8, DISP_W - x), h: 8 + widgetScale(sz) * 6 };
}

function heatBoxSize(sz) {
  // Tall vertical cells (sidewall-style) — mirrors firmware heatCellW/H.
  return {
    w: 20 + widgetScale(sz) * 8,
    h: 34 + widgetScale(sz) * 12,
  };
}

function heatQuadSize(sz) {
  const box = heatBoxSize(sz);
  const gap = 3;
  return { w: box.w * 2 + gap, h: box.h * 2 + gap, box, gap };
}

function sectorBarSize(sz) {
  const s = widgetScale(sz);
  return { w: 120 + s * 40, h: 10 + s * 4 };
}

function tyreCardSize(sz) {
  const s = widgetScale(sz);
  const cell = { w: 34 + s * 10, h: 40 + s * 10 };
  const gap = 6;
  return { w: cell.w * 2 + gap, h: cell.h * 2 + gap, cell, gap };
}

function brakeCarSize(sz) {
  const s = widgetScale(sz);
  const barW = 16 + s * 5;
  const barH = 24 + s * 6;
  const gap = 14 + s * 6;
  return { w: barW * 4 + gap * 3, h: barH + 12, barW, barH, gap };
}

function iconPx(sz) {
  // fontSize 1..4 → ~1× / 1.5× / 2× / 3× of 16px sprite
  const scale = [1, 1.5, 2, 3][Math.max(0, Math.min(3, (sz || 1) - 1))];
  return Math.round(16 * scale);
}

function btnSize(sz) {
  const s = widgetScale(sz);
  return { w: 28 + s * 10, h: 14 + s * 6 };
}

function lerpByte(a, b, t) {
  return Math.round(a + (b - a) * Math.max(0, Math.min(1, t)));
}

function rgbCss(r, g, b) {
  return `rgb(${r},${g},${b})`;
}

function colourTyreTemp(c) {
  if (c < 60) return rgbCss(lerpByte(20, 40, c / 60), lerpByte(40, 160, c / 60), lerpByte(180, 255, c / 60));
  if (c < 85) return rgbCss(lerpByte(40, 40, (c - 60) / 25), lerpByte(160, 200, (c - 60) / 25), lerpByte(255, 60, (c - 60) / 25));
  if (c < 105) return rgbCss(lerpByte(40, 240, (c - 85) / 20), lerpByte(200, 200, (c - 85) / 20), lerpByte(60, 20, (c - 85) / 20));
  const t = Math.min(1, (c - 105) / 40);
  return rgbCss(lerpByte(240, 255, t), lerpByte(200, 40, t), lerpByte(20, 20, t));
}

function colourTyrePress(psi) {
  if (psi < 20) return rgbCss(lerpByte(30, 80, psi / 20), lerpByte(60, 140, psi / 20), lerpByte(200, 255, psi / 20));
  if (psi < 27) return rgbCss(lerpByte(80, 40, (psi - 20) / 7), lerpByte(140, 200, (psi - 20) / 7), lerpByte(255, 60, (psi - 20) / 7));
  if (psi < 32) return rgbCss(lerpByte(40, 240, (psi - 27) / 5), lerpByte(200, 200, (psi - 27) / 5), lerpByte(60, 20, (psi - 27) / 5));
  const t = Math.min(1, (psi - 32) / 20);
  return rgbCss(lerpByte(240, 255, t), lerpByte(200, 40, t), lerpByte(20, 80, t));
}

function colourBrakeTemp(c) {
  if (c < 100) return rgbCss(lerpByte(30, 40, c / 100), lerpByte(30, 120, c / 100), lerpByte(40, 200, c / 100));
  if (c < 300) return rgbCss(lerpByte(40, 40, (c - 100) / 200), lerpByte(120, 200, (c - 100) / 200), lerpByte(200, 60, (c - 100) / 200));
  if (c < 500) return rgbCss(lerpByte(40, 255, (c - 300) / 200), lerpByte(200, 160, (c - 300) / 200), lerpByte(60, 20, (c - 300) / 200));
  if (c < 700) return rgbCss(255, lerpByte(160, 40, (c - 500) / 200), 20);
  const t = Math.min(1, (c - 700) / 300);
  return rgbCss(255, lerpByte(40, 220, t), lerpByte(20, 200, t));
}

function widgetBounds(el) {
  const s = widgetScale(el.fontSize);
  switch (el.type) {
    case 7:
    case 8: {
      const b = barSize(s);
      return { x: el.x, y: el.y, w: b.w, h: b.h };
    }
    case 9:
    case 10: {
      const b = vBarSize(s);
      return { x: el.x, y: el.y, w: b.w, h: b.h };
    }
    case 11:
    case 12:
    case 13: {
      const r = gaugeR(s);
      return { x: el.x - r, y: el.y - r, w: r * 2, h: r + 8 };
    }
    case 14: {
      const b = bannerSize(s, el.x);
      return { x: el.x, y: el.y, w: b.w, h: b.h };
    }
    case 15: {
      const b = badgeSize(s);
      return { x: el.x, y: el.y, w: b.w, h: b.h };
    }
    case 16: {
      const b = panelSize(s);
      return { x: el.x, y: el.y, w: b.w, h: b.h };
    }
    case 17:
    case 18:
    case 19: {
      const q = heatQuadSize(s);
      return { x: el.x, y: el.y, w: q.w, h: q.h };
    }
    case 49: {
      const line = 8 * s + 4;
      const label = formatGapPreview(DISP_SAMPLE.gapAheadMs, "^ ");
      const approxW = Math.max(48, label.length * 6 * s + 8);
      return { x: el.x - 4, y: el.y - 4, w: approxW, h: line * 2 + 8 };
    }
    case 50:
    case 51: {
      const b = sectorBarSize(el.fontSize);
      return { x: el.x, y: el.y - 4, w: b.w, h: b.h + 8 };
    }
    case 54: {
      const t = tyreCardSize(el.fontSize);
      return { x: el.x, y: el.y, w: t.w, h: t.h };
    }
    case 55: {
      const b = brakeCarSize(el.fontSize);
      return { x: el.x, y: el.y, w: b.w, h: b.h };
    }
    default: {
      if (DISP_ICON_TYPES.has(el.type)) {
        const px = iconPx(el.fontSize);
        return { x: el.x, y: el.y, w: px, h: px };
      }
      if (DISP_BTN_TYPES.has(el.type)) {
        const b = btnSize(el.fontSize);
        return { x: el.x, y: el.y, w: b.w, h: b.h };
      }
      if (el.type >= 20 && el.type <= 31) {
        const box = heatBoxSize(s);
        return { x: el.x, y: el.y, w: box.w, h: box.h };
      }
      const label = previewSampleText(el.type) || DISP_TYPES[el.type] || "?";
      const approxW = Math.max(24, label.length * 6 * s + 8);
      const approxH = 8 * s + 8;
      return { x: el.x - 4, y: el.y - 4, w: approxW, h: approxH };
    }
  }
}

function fracRpm() {
  return Math.max(0, Math.min(1, DISP_SAMPLE.rpm / 9000));
}
function fracFuel() {
  return Math.max(0, Math.min(1, DISP_SAMPLE.fuel / 100));
}
function fracSpeed() {
  return Math.max(0, Math.min(1, DISP_SAMPLE.speed / 300));
}

function drawPreviewHBar(ctx, x, y, w, h, frac, color) {
  ctx.fillStyle = "#333";
  ctx.fillRect(x, y, w, h);
  ctx.strokeStyle = "#555";
  ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
  ctx.fillStyle = color;
  ctx.fillRect(x + 1, y + 1, Math.max(0, (w - 2) * frac), h - 2);
}

function drawPreviewVBar(ctx, x, y, w, h, frac, color) {
  ctx.fillStyle = "#333";
  ctx.fillRect(x, y, w, h);
  ctx.strokeStyle = "#555";
  ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
  const fill = Math.max(0, (h - 2) * frac);
  ctx.fillStyle = color;
  ctx.fillRect(x + 1, y + h - 1 - fill, w - 2, fill);
}

function drawPreviewGauge(ctx, cx, cy, r, frac, color) {
  const start = Math.PI;
  const span = Math.PI;
  const steps = 36;
  const filled = Math.round(frac * steps);
  let px = cx + Math.cos(start) * r;
  let py = cy - Math.sin(start) * r;
  for (let i = 1; i <= steps; i++) {
    const a = start - span * (i / steps);
    const qx = cx + Math.cos(a) * r;
    const qy = cy - Math.sin(a) * r;
    ctx.strokeStyle = i <= filled ? color : "#333";
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(px, py);
    ctx.lineTo(qx, qy);
    ctx.stroke();
    px = qx;
    py = qy;
  }
  const na = start - span * frac;
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(cx, cy);
  ctx.lineTo(cx + Math.cos(na) * r, cy - Math.sin(na) * r);
  ctx.stroke();
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.arc(cx, cy, 3, 0, Math.PI * 2);
  ctx.fill();
}

function canvasToDisp(ev) {
  const canvas = els.dispCanvas;
  const rect = canvas.getBoundingClientRect();
  const x = ((ev.clientX - rect.left) / rect.width) * DISP_W;
  const y = ((ev.clientY - rect.top) / rect.height) * DISP_H;
  return {
    x: Math.max(0, Math.min(DISP_W - 1, Math.round(x))),
    y: Math.max(0, Math.min(DISP_H - 1, Math.round(y))),
  };
}

function hitTest(x, y) {
  for (let i = dispLayout.length - 1; i >= 0; i--) {
    const b = widgetBounds(dispLayout[i]);
    if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) return i;
  }
  return -1;
}

function syncDispInspector() {
  const font = els.dispFont;
  const color = els.dispColor;
  const label = document.querySelector("#disp-font-label");
  if (!font || !color) return;
  if (dispSelected < 0 || dispSelected >= dispLayout.length) {
    font.disabled = true;
    color.disabled = true;
    return;
  }
  font.disabled = false;
  color.disabled = false;
  const el = dispLayout[dispSelected];
  font.value = String(el.fontSize);
  color.value = rgb565ToHex(el.color565);
  if (label) {
    label.textContent = DISP_TEXT_TYPES.has(el.type)
      ? "Font size (1–4)"
      : "Size scale (1–4)";
  }
}

function drawDispWidget(ctx, el, selected) {
  const color = rgb565ToHex(el.color565);
  const s = widgetScale(el.fontSize);
  const drawSel = (b) => {
    if (!selected) return;
    ctx.strokeStyle = getCss("--papaya") || "#ff8000";
    ctx.lineWidth = 1;
    ctx.strokeRect(b.x - 1, b.y - 1, b.w + 2, b.h + 2);
  };

  if (DISP_TEXT_TYPES.has(el.type) || el.type === 15) {
    if (el.type === 15) {
      const b = badgeSize(s);
      ctx.fillStyle = "#333";
      roundRect(ctx, el.x, el.y, b.w, b.h, 4, true, false);
      ctx.strokeStyle = color;
      roundRect(ctx, el.x, el.y, b.w, b.h, 4, false, true);
      ctx.fillStyle = color;
      ctx.font = `${s * 8}px monospace`;
      ctx.textBaseline = "top";
      ctx.fillText(previewSampleText(15), el.x + 4 + s, el.y + 4 + s);
      drawSel({ x: el.x, y: el.y, w: b.w, h: b.h });
      return;
    }
    const text = previewSampleText(el.type);
    const size = s * 8;
    ctx.font = `${size}px monospace`;
    let fill = color;
    if (el.type === 45) fill = deltaColorCss(DISP_SAMPLE.deltaBestMs);
    else if (el.type === 46) fill = deltaColorCss(DISP_SAMPLE.deltaP1Ms);
    ctx.fillStyle = fill;
    ctx.textBaseline = "top";
    ctx.fillText(text, el.x, el.y);
    if (selected) {
      const w = Math.max(24, ctx.measureText(text).width + 6);
      drawSel({ x: el.x, y: el.y, w, h: size + 4 });
    }
    return;
  }

  if (el.type === 7 || el.type === 8) {
    const b = barSize(s);
    drawPreviewHBar(ctx, el.x, el.y, b.w, b.h, el.type === 7 ? fracRpm() : fracFuel(), color);
    drawSel({ x: el.x, y: el.y, w: b.w, h: b.h });
    return;
  }
  if (el.type === 9 || el.type === 10) {
    const b = vBarSize(s);
    drawPreviewVBar(ctx, el.x, el.y, b.w, b.h, el.type === 9 ? fracRpm() : fracFuel(), color);
    drawSel({ x: el.x, y: el.y, w: b.w, h: b.h });
    return;
  }
  if (el.type === 11 || el.type === 12 || el.type === 13) {
    const r = gaugeR(s);
    const frac = el.type === 11 ? fracRpm() : el.type === 12 ? fracSpeed() : fracFuel();
    drawPreviewGauge(ctx, el.x, el.y, r, frac, color);
    drawSel({ x: el.x - r, y: el.y - r, w: r * 2, h: r + 8 });
    return;
  }
  if (el.type === 14) {
    const b = bannerSize(s, el.x);
    ctx.fillStyle = "#ffe000";
    ctx.fillRect(el.x, el.y, b.w, b.h);
    drawSel({ x: el.x, y: el.y, w: b.w, h: b.h });
    return;
  }
  if (el.type === 16) {
    const b = panelSize(s);
    ctx.fillStyle = color;
    roundRect(ctx, el.x, el.y, b.w, b.h, 4, true, false);
    drawSel({ x: el.x, y: el.y, w: b.w, h: b.h });
    return;
  }

  if (DISP_ICON_TYPES.has(el.type)) {
    const px = iconPx(el.fontSize);
    drawPreviewIcon(ctx, el.type, el.x, el.y, px, color);
    drawSel({ x: el.x, y: el.y, w: px, h: px });
    return;
  }

  if (DISP_BTN_TYPES.has(el.type)) {
    const b = btnSize(el.fontSize);
    drawPreviewBtn(ctx, el.type, el.x, el.y, b.w, b.h, color);
    drawSel({ x: el.x, y: el.y, w: b.w, h: b.h });
    return;
  }

  const drawHeat = (x, y, cell, fill, label) => {
    ctx.fillStyle = fill;
    roundRect(ctx, x, y, cell.w, cell.h, 2, true, false);
    ctx.strokeStyle = "#444";
    roundRect(ctx, x, y, cell.w, cell.h, 2, false, true);
    ctx.fillStyle = "#fff";
    ctx.font = "10px monospace";
    ctx.textBaseline = "middle";
    ctx.textAlign = "center";
    ctx.fillText(String(label), x + cell.w / 2, y + cell.h / 2);
    ctx.textAlign = "start";
  };

  if (el.type === 17 || el.type === 18 || el.type === 19) {
    const q = heatQuadSize(s);
    const vals =
      el.type === 17
        ? DISP_SAMPLE.tyreTemp
        : el.type === 18
          ? DISP_SAMPLE.tyrePsi
          : DISP_SAMPLE.brakeTemp;
    const cols =
      el.type === 17
        ? vals.map(colourTyreTemp)
        : el.type === 18
          ? vals.map(colourTyrePress)
          : vals.map(colourBrakeTemp);
    const unit = el.type === 18 ? "psi" : "C";
    drawHeat(el.x, el.y, q.box, cols[0], `${vals[0]}${unit}`);
    drawHeat(el.x + q.box.w + q.gap, el.y, q.box, cols[1], `${vals[1]}${unit}`);
    drawHeat(el.x, el.y + q.box.h + q.gap, q.box, cols[2], `${vals[2]}${unit}`);
    drawHeat(el.x + q.box.w + q.gap, el.y + q.box.h + q.gap, q.box, cols[3], `${vals[3]}${unit}`);
    drawSel({ x: el.x, y: el.y, w: q.w, h: q.h });
    return;
  }

  if (el.type >= 20 && el.type <= 31) {
    const box = heatBoxSize(s);
    let val;
    let fill;
    let unit = "C";
    if (el.type <= 23) {
      val = DISP_SAMPLE.tyreTemp[el.type - 20];
      fill = colourTyreTemp(val);
    } else if (el.type <= 27) {
      val = DISP_SAMPLE.tyrePsi[el.type - 24];
      fill = colourTyrePress(val);
      unit = "psi";
    } else {
      val = DISP_SAMPLE.brakeTemp[el.type - 28];
      fill = colourBrakeTemp(val);
    }
    drawHeat(el.x, el.y, box, fill, `${val}${unit}`);
    drawSel({ x: el.x, y: el.y, w: box.w, h: box.h });
    return;
  }

  if (el.type === 54) {
    const t = tyreCardSize(el.fontSize);
    const corners = ["FL", "FR", "RL", "RR"];
    for (let i = 0; i < 4; i++) {
      const col = i & 1;
      const row = i >> 1;
      const x = el.x + col * (t.cell.w + t.gap);
      const y = el.y + row * (t.cell.h + t.gap);
      const foot = Math.round(t.cell.h / 3);
      const body = t.cell.h - foot;
      ctx.fillStyle = colourTyreTemp(DISP_SAMPLE.tyreTemp[i]);
      roundRect(ctx, x, y, t.cell.w, body, 3, true, false);
      ctx.fillStyle = colourTyrePress(DISP_SAMPLE.tyrePsi[i]);
      roundRect(ctx, x, y + body - 2, t.cell.w, foot + 2, 3, true, false);
      ctx.strokeStyle = "#444";
      roundRect(ctx, x, y, t.cell.w, t.cell.h, 3, false, true);
      ctx.fillStyle = "#fff";
      ctx.font = "10px monospace";
      ctx.textAlign = "center";
      ctx.textBaseline = "top";
      ctx.fillText(corners[i], x + t.cell.w / 2, y + 3);
      ctx.fillText(`${DISP_SAMPLE.tyreTemp[i]}C`, x + t.cell.w / 2, y + body / 2 - 4);
      ctx.fillText(`${DISP_SAMPLE.tyrePsi[i]}psi`, x + t.cell.w / 2, y + body + foot / 2 - 5);
    }
    ctx.textAlign = "start";
    drawSel({ x: el.x, y: el.y, w: t.w, h: t.h });
    return;
  }

  if (el.type === 55) {
    const b = brakeCarSize(el.fontSize);
    const corners = ["FL", "FR", "RL", "RR"];
    for (let i = 0; i < 4; i++) {
      const bx = el.x + i * (b.barW + b.gap);
      const frac = Math.max(0, Math.min(1, DISP_SAMPLE.brakeTemp[i] / 800));
      ctx.fillStyle = "#333";
      roundRect(ctx, bx, el.y, b.barW, b.barH, 2, true, false);
      ctx.strokeStyle = "#444";
      roundRect(ctx, bx, el.y, b.barW, b.barH, 2, false, true);
      const fillH = Math.round(frac * (b.barH - 2));
      ctx.fillStyle = colourBrakeTemp(DISP_SAMPLE.brakeTemp[i]);
      ctx.fillRect(bx + 1, el.y + b.barH - 1 - fillH, b.barW - 2, fillH);
      ctx.fillStyle = "#fff";
      ctx.font = "10px monospace";
      ctx.textAlign = "center";
      ctx.textBaseline = "top";
      ctx.fillText(corners[i], bx + b.barW / 2, el.y + 2);
      ctx.fillText(`${DISP_SAMPLE.brakeTemp[i]}C`, bx + b.barW / 2, el.y + b.barH + 1);
    }
    ctx.textAlign = "start";
    drawSel({ x: el.x, y: el.y, w: b.w, h: b.h });
    return;
  }

  if (el.type === 49) {
    const a = formatGapPreview(DISP_SAMPLE.gapAheadMs, "^ ");
    const b = formatGapPreview(DISP_SAMPLE.gapBehindMs, "v ");
    const size = s * 8;
    ctx.font = `${size}px monospace`;
    ctx.fillStyle = color;
    ctx.textBaseline = "top";
    ctx.fillText(a, el.x, el.y);
    ctx.fillText(b, el.x, el.y + size + 4);
    drawSel({
      x: el.x - 4,
      y: el.y - 4,
      w: Math.max(ctx.measureText(a).width, ctx.measureText(b).width) + 8,
      h: size * 2 + 12,
    });
    return;
  }

  if (el.type === 50 || el.type === 51) {
    const b = sectorBarSize(el.fontSize);
    const ms = el.type === 50 ? DISP_SAMPLE.deltaBestMs : DISP_SAMPLE.deltaP1Ms;
    const mid = el.x + b.w / 2;
    roundRect(ctx, el.x, el.y, b.w, b.h, 2, false, false);
    ctx.fillStyle = "#222";
    roundRect(ctx, el.x, el.y, b.w, b.h, 2, true, false);
    ctx.fillStyle = "#0a3d14";
    ctx.fillRect(el.x + 1, el.y + 1, b.w / 2 - 1, b.h - 2);
    ctx.fillStyle = "#3d0a0a";
    ctx.fillRect(mid, el.y + 1, b.w / 2 - 1, b.h - 2);
    ctx.strokeStyle = color;
    ctx.beginPath();
    ctx.moveTo(mid + 0.5, el.y);
    ctx.lineTo(mid + 0.5, el.y + b.h);
    ctx.stroke();
    let t = ms / 2000;
    t = Math.max(-1, Math.min(1, t));
    const px = mid + t * (b.w / 2 - 3);
    ctx.fillStyle = deltaColorCss(ms);
    ctx.beginPath();
    ctx.moveTo(px, el.y - 1);
    ctx.lineTo(px - 4, el.y + b.h + 2);
    ctx.lineTo(px + 4, el.y + b.h + 2);
    ctx.closePath();
    ctx.fill();
    ctx.fillRect(px - 1, el.y, 3, b.h);
    drawSel({ x: el.x, y: el.y - 4, w: b.w, h: b.h + 8 });
  }
}

function roundRect(ctx, x, y, w, h, r, fill, stroke) {
  const rr = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + rr, y);
  ctx.arcTo(x + w, y, x + w, y + h, rr);
  ctx.arcTo(x + w, y + h, x, y + h, rr);
  ctx.arcTo(x, y + h, x, y, rr);
  ctx.arcTo(x, y, x + w, y, rr);
  ctx.closePath();
  if (fill) ctx.fill();
  if (stroke) ctx.stroke();
}

function drawPreviewIcon(ctx, type, x, y, px, color) {
  ctx.save();
  ctx.translate(x, y);
  const s = px / 16;
  ctx.scale(s, s);
  ctx.strokeStyle = color;
  ctx.fillStyle = color;
  ctx.lineWidth = 1.2;
  ctx.lineJoin = "round";
  ctx.beginPath();
  switch (type) {
    case 32: // RPM tach
      ctx.arc(8, 9, 6, Math.PI * 0.85, Math.PI * 0.15, false);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(8, 9);
      ctx.lineTo(12, 5);
      ctx.stroke();
      break;
    case 33: // fuel
      ctx.strokeRect(4, 3, 7, 11);
      ctx.fillRect(11, 5, 2, 4);
      ctx.beginPath();
      ctx.moveTo(5, 14);
      ctx.lineTo(3, 14);
      ctx.lineTo(3, 10);
      ctx.stroke();
      break;
    case 34: // speed
      ctx.beginPath();
      ctx.moveTo(2, 12);
      ctx.lineTo(8, 3);
      ctx.lineTo(14, 12);
      ctx.closePath();
      ctx.stroke();
      ctx.fillRect(7, 8, 2, 5);
      break;
    case 35: // flag
      ctx.beginPath();
      ctx.moveTo(4, 2);
      ctx.lineTo(4, 14);
      ctx.moveTo(4, 2);
      ctx.lineTo(13, 5);
      ctx.lineTo(4, 8);
      ctx.stroke();
      break;
    case 36: // tyre
      ctx.beginPath();
      ctx.arc(8, 8, 6, 0, Math.PI * 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(8, 8, 2.5, 0, Math.PI * 2);
      ctx.stroke();
      break;
    case 37: // brake
      ctx.strokeRect(3, 5, 10, 7);
      ctx.beginPath();
      ctx.moveTo(5, 5);
      ctx.lineTo(5, 3);
      ctx.lineTo(11, 3);
      ctx.lineTo(11, 5);
      ctx.stroke();
      break;
    case 38: // lap
      ctx.beginPath();
      ctx.arc(8, 8, 5.5, -Math.PI * 0.2, Math.PI * 1.4);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(11, 2);
      ctx.lineTo(14, 5);
      ctx.lineTo(10, 6);
      ctx.closePath();
      ctx.fill();
      break;
    default:
      ctx.strokeRect(2, 2, 12, 12);
  }
  ctx.restore();
}

function drawPreviewBtn(ctx, type, x, y, w, h, color) {
  ctx.fillStyle = "#1a1a1a";
  roundRect(ctx, x, y, w, h, 6, true, false);
  ctx.strokeStyle = color;
  ctx.lineWidth = 1;
  roundRect(ctx, x, y, w, h, 6, false, true);
  ctx.fillStyle = color;
  ctx.font = `${Math.max(10, Math.round(h * 0.55))}px monospace`;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  let label = "?";
  if (type === 40) label = "‹";
  else if (type === 41) label = "›";
  else if (type === 42) label = "1";
  else if (type === 43) label = "2";
  else if (type === 44) label = "3";
  ctx.fillText(label, x + w / 2, y + h / 2 + 0.5);
  ctx.textAlign = "start";
  ctx.textBaseline = "alphabetic";
}

function fillDispBackground(ctx, theme) {
  if (theme === 2) {
    ctx.fillStyle = "#0a1628";
    ctx.fillRect(0, 0, DISP_W, DISP_H);
  } else if (theme === 1) {
    ctx.fillStyle = "#0c0c0c";
    ctx.fillRect(0, 0, DISP_W, DISP_H);
    ctx.fillStyle = "#161616";
    for (let x = 0; x < DISP_W; x += 6) {
      ctx.fillRect(x, 0, 2, DISP_H);
    }
  } else if (theme === 3) {
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, DISP_W, DISP_H);
    ctx.strokeStyle = "#1a1a28";
    ctx.lineWidth = 1;
    for (let x = 0; x < DISP_W; x += 16) {
      ctx.beginPath();
      ctx.moveTo(x + 0.5, 0);
      ctx.lineTo(x + 0.5, DISP_H);
      ctx.stroke();
    }
    for (let y = 0; y < DISP_H; y += 16) {
      ctx.beginPath();
      ctx.moveTo(0, y + 0.5);
      ctx.lineTo(DISP_W, y + 0.5);
      ctx.stroke();
    }
  } else {
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, DISP_W, DISP_H);
  }
}

function drawPageDots(ctx) {
  const n = Math.max(1, Math.min(3, dispPageCount));
  const gap = 10;
  const r = 3;
  const total = (n - 1) * gap;
  const cx0 = DISP_W / 2 - total / 2;
  const cy = DISP_H - 10;
  for (let i = 0; i < n; i++) {
    ctx.beginPath();
    ctx.arc(cx0 + i * gap, cy, r, 0, Math.PI * 2);
    ctx.fillStyle = i === dispActivePage ? "#ff8000" : "#444";
    ctx.fill();
  }
}

function drawDispPreview() {
  const canvas = els.dispCanvas;
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  const theme = Number(els.dispBg?.value ?? dispPages[dispActivePage]?.bgTheme ?? 0);
  fillDispBackground(ctx, theme);
  ctx.strokeStyle = "#222";
  ctx.strokeRect(0.5, 0.5, DISP_W - 1, DISP_H - 1);

  // Draw order: panels → icons → rest → buttons (on top).
  const layers = [
    (el) => el.type === 16,
    (el) => DISP_ICON_TYPES.has(el.type),
    (el) => el.type !== 16 && !DISP_ICON_TYPES.has(el.type) && !DISP_BTN_TYPES.has(el.type),
    (el) => DISP_BTN_TYPES.has(el.type),
  ];
  for (const pred of layers) {
    dispLayout.forEach((el, i) => {
      if (pred(el)) drawDispWidget(ctx, el, i === dispSelected);
    });
  }
  drawPageDots(ctx);

  if (els.dispHint) {
    const mode = connected ? "device" : "offline draft";
    const bg = DISP_BG_NAMES[theme] || "Black";
    els.dispHint.textContent =
      `page ${dispActivePage + 1}/${dispPageCount} · ${bg} · ${dispLayout.length}/${DISP_MAX} · ${mode}` +
      (dispSelected >= 0 ? ` · ${DISP_TYPES[dispLayout[dispSelected].type] || "?"}` : "") +
      (connected ? "" : " · swipe via :disp swipe when linked");
  }
}

function normalizeDispElement(raw) {
  if (!raw || typeof raw !== "object") return null;
  const type = Number(raw.type);
  if (!Number.isFinite(type) || !isValidDispType(type)) return null;
  return {
    type,
    x: Math.max(0, Math.min(DISP_W - 1, Math.round(Number(raw.x) || 0))),
    y: Math.max(0, Math.min(DISP_H - 1, Math.round(Number(raw.y) || 0))),
    fontSize: Math.max(1, Math.min(4, Math.round(Number(raw.fontSize) || 1))),
    color565: (Number(raw.color565) || 0xffff) & 0xffff,
  };
}

function normalizeDispPage(raw) {
  if (!raw || typeof raw !== "object") return null;
  let elements = null;
  if (Array.isArray(raw.elements)) {
    elements = raw.elements.map(normalizeDispElement).filter(Boolean).slice(0, DISP_MAX);
  } else if (raw.layout_hex) {
    elements = decodeLayoutHex(raw.layout_hex);
  }
  if (!elements) return null;
  return {
    bgTheme: Math.max(0, Math.min(3, Number(raw.bgTheme) || 0)),
    elements,
  };
}

function layoutToJsonDoc() {
  syncLayoutIntoPages();
  return {
    version: 2,
    width: DISP_W,
    height: DISP_H,
    dispBright: Number(els.dispBright?.value) || 180,
    activePage: dispActivePage,
    pageCount: dispPageCount,
    pages: dispPages.slice(0, 3).map((p) => ({
      bgTheme: p.bgTheme,
      layout_hex: encodeLayoutHex(p.elements),
      elements: p.elements.map((el) => ({
        type: el.type,
        name: DISP_TYPES[el.type] || "?",
        x: el.x,
        y: el.y,
        fontSize: el.fontSize,
        color565: el.color565,
        color: rgb565ToHex(el.color565),
      })),
    })),
    // Backward-compatible single-page fields (active page).
    layout_hex: encodeLayoutHex(dispLayout),
    elements: dispLayout.map((el) => ({
      type: el.type,
      name: DISP_TYPES[el.type] || "?",
      x: el.x,
      y: el.y,
      fontSize: el.fontSize,
      color565: el.color565,
      color: rgb565ToHex(el.color565),
    })),
  };
}

function applyDispJsonDoc(doc) {
  if (!doc || typeof doc !== "object") throw new Error("Invalid layout JSON");
  let pages = null;
  if (Array.isArray(doc.pages) && doc.pages.length > 0) {
    pages = doc.pages.map(normalizeDispPage).filter(Boolean);
  }
  if (!pages || pages.length === 0) {
    let elements = null;
    if (Array.isArray(doc.elements)) {
      elements = doc.elements.map(normalizeDispElement).filter(Boolean).slice(0, DISP_MAX);
    } else if (doc.layout_hex) {
      elements = decodeLayoutHex(doc.layout_hex);
    }
    if (!elements) throw new Error("JSON missing pages / elements / layout_hex");
    pages = [{ bgTheme: Number(doc.bgTheme) || 0, elements }];
  }
  while (pages.length < 3) {
    const defs = defaultDispPages();
    pages.push(clonePage(defs[pages.length]));
  }
  dispPages = pages.slice(0, 3).map(clonePage);
  dispPageCount = Math.max(1, Math.min(3, Number(doc.pageCount) || pages.length || 3));
  dispActivePage = Math.max(0, Math.min(dispPageCount - 1, Number(doc.activePage) || 0));
  if (els.dispBright && doc.dispBright != null) {
    els.dispBright.value = String(Math.max(0, Math.min(255, Number(doc.dispBright) || 180)));
  }
  loadActivePageLayout();
  updateDispPageTabs();
  syncDispInspector();
  drawDispPreview();
  persistDispDraft();
}

function persistDispDraft() {
  try {
    localStorage.setItem(DISP_DRAFT_KEY, JSON.stringify(layoutToJsonDoc()));
  } catch {
    // ignore quota / private mode
  }
}

function loadDispDraft() {
  try {
    const raw = localStorage.getItem(DISP_DRAFT_KEY);
    if (!raw) return false;
    applyDispJsonDoc(JSON.parse(raw));
    return true;
  } catch {
    return false;
  }
}

async function exportDispJson() {
  const doc = layoutToJsonDoc();
  const text = JSON.stringify(doc, null, 2);
  persistDispDraft();
  try {
    const path = await invoke("save_text_file", {
      defaultName: "ffb-tft-layout.json",
      contents: text,
    });
    if (path) setStatus(`Exported ${path}`, "ok");
    else setStatus("Export cancelled", "");
  } catch (e) {
    // Browser / non-Tauri fallback.
    try {
      const blob = new Blob([text], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = "ffb-tft-layout.json";
      a.click();
      URL.revokeObjectURL(url);
      setStatus("Exported ffb-tft-layout.json (also auto-saved in browser)", "ok");
    } catch (e2) {
      setStatus(`Export failed: ${e} / ${e2}`, "err");
    }
  }
}

function importDispJsonFile(file) {
  const reader = new FileReader();
  reader.onload = () => {
    try {
      applyDispJsonDoc(JSON.parse(String(reader.result)));
      setStatus(`Imported ${file.name}`, "ok");
    } catch (e) {
      setStatus(String(e), "err");
    }
  };
  reader.onerror = () => setStatus("Failed to read JSON file", "err");
  reader.readAsText(file);
}

function loadDispFromMap(map) {
  if (!map) return;
  if (map.disp_bright != null && els.dispBright) {
    els.dispBright.value = String(map.disp_bright);
  }
  const pages = defaultDispPages().map(clonePage);
  let pageCount = 3;
  let active = 0;
  if (map.disp_pages != null) {
    pageCount = Math.max(1, Math.min(3, Number(map.disp_pages) || 3));
  }
  if (map.disp_page != null) {
    active = Math.max(0, Math.min(pageCount - 1, Number(map.disp_page) || 0));
  }
  let any = false;
  for (let i = 0; i < 3; i++) {
    const hex = map[`layout_page${i}_hex`];
    const bg = map[`page${i}_bg`];
    if (hex) {
      const decoded = decodeLayoutHex(hex);
      if (decoded) {
        pages[i].elements = decoded;
        any = true;
      }
    }
    if (bg != null) {
      pages[i].bgTheme = Math.max(0, Math.min(3, Number(bg) || 0));
      any = true;
    }
  }
  if (!any && map.layout_hex) {
    const decoded = decodeLayoutHex(map.layout_hex);
    if (decoded) {
      pages[active].elements = decoded;
      any = true;
    }
  }
  if (!any) return;
  dispPages = pages;
  dispPageCount = pageCount;
  dispActivePage = active;
  loadActivePageLayout();
  updateDispPageTabs();
  syncDispInspector();
  drawDispPreview();
  persistDispDraft();
}

async function applyDispLayout() {
  if (!connected) {
    setStatus("Connect to apply layout to the device (export JSON meanwhile)", "err");
    return;
  }
  try {
    syncLayoutIntoPages();
    await invoke("set_setting", { key: "disp_pages", value: dispPageCount });
    for (let i = 0; i < dispPageCount; i++) {
      const page = dispPages[i] || { bgTheme: 0, elements: [] };
      await invoke("set_setting", { key: `page${i}_bg`, value: page.bgTheme });
      await invoke("set_setting_str", {
        key: `layout_page${i}_hex`,
        value: encodeLayoutHex(page.elements || []),
      });
    }
    await invoke("set_setting", { key: "disp_page", value: dispActivePage });
    if (els.dispBright) {
      const bright = Number(els.dispBright.value);
      if (!Number.isNaN(bright)) {
        await invoke("set_setting", { key: "disp_bright", value: bright });
        if (inputs.disp_bright) inputs.disp_bright.value = String(bright);
      }
    }
    persistDispDraft();
    setStatus(`OK applied ${dispPageCount} page(s)`, "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function refreshDispLayout() {
  if (!connected) {
    setStatus("Connect to refresh from device", "err");
    return;
  }
  try {
    const map = await invoke("dump_settings");
    fillFields(map);
    loadDispFromMap(map);
    setStatus("Display layout refreshed", "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

function initDispEditor() {
  els.dispCanvas = document.querySelector("#disp-canvas");
  els.dispBright = document.querySelector("#disp-bright");
  els.dispFont = document.querySelector("#disp-font");
  els.dispColor = document.querySelector("#disp-color");
  els.dispHint = document.querySelector("#disp-hint");
  els.dispBg = document.querySelector("#disp-bg");
  els.dispPages = document.querySelector("#disp-pages");
  if (!els.dispCanvas) return;

  if (!loadDispDraft()) {
    dispPages = defaultDispPages().map(clonePage);
    dispPageCount = 3;
    dispActivePage = 0;
    loadActivePageLayout();
  }
  updateDispPageTabs();
  syncDispInspector();
  drawDispPreview();
  updateDispDeviceButtons();

  const touch = () => {
    syncLayoutIntoPages();
    persistDispDraft();
    drawDispPreview();
  };

  document.querySelector("#disp-page-tabs")?.addEventListener("click", (ev) => {
    const btn = ev.target.closest("[data-disp-page]");
    if (!btn || btn.disabled) return;
    switchDispPage(Number(btn.dataset.dispPage));
  });

  els.dispPages?.addEventListener("change", () => {
    syncLayoutIntoPages();
    dispPageCount = Math.max(1, Math.min(3, Number(els.dispPages.value) || 3));
    if (dispActivePage >= dispPageCount) {
      dispActivePage = dispPageCount - 1;
      loadActivePageLayout();
    }
    updateDispPageTabs();
    touch();
  });

  els.dispBg?.addEventListener("change", () => {
    if (dispPages[dispActivePage]) {
      dispPages[dispActivePage].bgTheme = Math.max(
        0,
        Math.min(3, Number(els.dispBg.value) || 0),
      );
    }
    touch();
  });

  document.querySelector(".disp-palette-groups")?.addEventListener("click", (ev) => {
    const btn = ev.target.closest("[data-disp-type]");
    if (!btn) return;
    if (dispLayout.length >= DISP_MAX) {
      setStatus("Max 16 widgets", "err");
      return;
    }
    const type = Number(btn.dataset.dispType);
    const defaults = {
      7: { fontSize: 2, color565: 0xffe0, x: 40, y: 200 },
      8: { fontSize: 2, color565: 0x07ff, x: 40, y: 210 },
      9: { fontSize: 2, color565: 0xffe0, x: 280, y: 40 },
      10: { fontSize: 2, color565: 0x07ff, x: 300, y: 40 },
      11: { fontSize: 3, color565: 0xf800, x: 80, y: 150 },
      12: { fontSize: 3, color565: 0x07e0, x: 240, y: 150 },
      13: { fontSize: 2, color565: 0x07ff, x: 160, y: 180 },
      14: { fontSize: 2, color565: 0xffe0, x: 0, y: 0 },
      15: { fontSize: 3, color565: 0xffff, x: 130, y: 80 },
      16: { fontSize: 3, color565: 0x2104, x: 20, y: 20 },
      17: { fontSize: 2, color565: 0xffff, x: 12, y: 120 },
      18: { fontSize: 2, color565: 0xffff, x: 120, y: 120 },
      19: { fontSize: 2, color565: 0xffff, x: 228, y: 120 },
      20: { fontSize: 2, color565: 0xffff, x: 16, y: 40 },
      21: { fontSize: 2, color565: 0xffff, x: 56, y: 40 },
      22: { fontSize: 2, color565: 0xffff, x: 16, y: 80 },
      23: { fontSize: 2, color565: 0xffff, x: 56, y: 80 },
      24: { fontSize: 2, color565: 0xffff, x: 100, y: 40 },
      25: { fontSize: 2, color565: 0xffff, x: 140, y: 40 },
      26: { fontSize: 2, color565: 0xffff, x: 100, y: 80 },
      27: { fontSize: 2, color565: 0xffff, x: 140, y: 80 },
      28: { fontSize: 2, color565: 0xffff, x: 200, y: 40 },
      29: { fontSize: 2, color565: 0xffff, x: 240, y: 40 },
      30: { fontSize: 2, color565: 0xffff, x: 200, y: 80 },
      31: { fontSize: 2, color565: 0xffff, x: 240, y: 80 },
      32: { fontSize: 2, color565: 0xf800, x: 12, y: 100 },
      33: { fontSize: 2, color565: 0x07ff, x: 12, y: 130 },
      34: { fontSize: 2, color565: 0x07e0, x: 12, y: 40 },
      35: { fontSize: 2, color565: 0xffe0, x: 12, y: 70 },
      36: { fontSize: 2, color565: 0x07e0, x: 12, y: 48 },
      37: { fontSize: 2, color565: 0xfd20, x: 12, y: 140 },
      38: { fontSize: 2, color565: 0x07ff, x: 12, y: 20 },
      40: { fontSize: 2, color565: 0xffff, x: 40, y: 200 },
      41: { fontSize: 2, color565: 0xffff, x: 252, y: 200 },
      42: { fontSize: 2, color565: 0x07e0, x: 120, y: 200 },
      43: { fontSize: 2, color565: 0x07e0, x: 160, y: 200 },
      44: { fontSize: 2, color565: 0x07e0, x: 200, y: 200 },
      45: { fontSize: 2, color565: 0x07e0, x: 12, y: 64 },
      46: { fontSize: 2, color565: 0xffe0, x: 160, y: 64 },
      47: { fontSize: 2, color565: 0x07ff, x: 40, y: 140 },
      48: { fontSize: 2, color565: 0x07ff, x: 40, y: 164 },
      49: { fontSize: 2, color565: 0x07ff, x: 40, y: 140 },
      50: { fontSize: 3, color565: 0xffff, x: 40, y: 100 },
      51: { fontSize: 3, color565: 0xffff, x: 40, y: 100 },
      52: { fontSize: 2, color565: 0xc618, x: 12, y: 52 },
      53: { fontSize: 2, color565: 0x07e0, x: 160, y: 52 },
      54: { fontSize: 1, color565: 0xffff, x: 86, y: 44 },
      55: { fontSize: 1, color565: 0xfd20, x: 52, y: 156 },
    };
    const d = defaults[type] || {
      fontSize: type === 1 ? 4 : 2,
      color565: 0xffff,
      x: 40 + dispLayout.length * 12,
      y: 40 + dispLayout.length * 10,
    };
    dispLayout.push({
      type,
      x: d.x,
      y: d.y,
      fontSize: d.fontSize,
      color565: d.color565,
    });
    dispSelected = dispLayout.length - 1;
    syncDispInspector();
    touch();
  });

  els.dispCanvas.addEventListener("pointerdown", (ev) => {
    const pt = canvasToDisp(ev);
    const hit = hitTest(pt.x, pt.y);
    dispSelected = hit;
    syncDispInspector();
    if (hit >= 0) {
      dispDrag = {
        index: hit,
        ox: pt.x - dispLayout[hit].x,
        oy: pt.y - dispLayout[hit].y,
      };
      els.dispCanvas.setPointerCapture(ev.pointerId);
    }
    drawDispPreview();
  });

  els.dispCanvas.addEventListener("pointermove", (ev) => {
    if (!dispDrag) return;
    const pt = canvasToDisp(ev);
    const el = dispLayout[dispDrag.index];
    el.x = Math.max(0, Math.min(DISP_W - 1, pt.x - dispDrag.ox));
    el.y = Math.max(0, Math.min(DISP_H - 1, pt.y - dispDrag.oy));
    drawDispPreview();
  });

  const endDrag = () => {
    if (dispDrag) {
      syncLayoutIntoPages();
      persistDispDraft();
    }
    dispDrag = null;
  };
  els.dispCanvas.addEventListener("pointerup", endDrag);
  els.dispCanvas.addEventListener("pointercancel", endDrag);

  els.dispFont?.addEventListener("change", () => {
    if (dispSelected < 0) return;
    dispLayout[dispSelected].fontSize = Math.max(1, Math.min(4, Number(els.dispFont.value) || 1));
    touch();
  });
  els.dispColor?.addEventListener("input", () => {
    if (dispSelected < 0) return;
    dispLayout[dispSelected].color565 = hexToRgb565(els.dispColor.value);
    touch();
  });
  els.dispBright?.addEventListener("change", () => persistDispDraft());

  document.querySelector("#disp-delete")?.addEventListener("click", () => {
    if (dispSelected < 0) return;
    dispLayout.splice(dispSelected, 1);
    dispSelected = -1;
    syncDispInspector();
    touch();
  });
  document.querySelector("#disp-clear")?.addEventListener("click", () => {
    dispLayout = [];
    syncDispInspector();
    touch();
  });
  document.querySelector("#disp-defaults")?.addEventListener("click", () => {
    const defs = defaultDispPages();
    dispPages = defs.map(clonePage);
    dispPageCount = 3;
    dispActivePage = 0;
    loadActivePageLayout();
    updateDispPageTabs();
    syncDispInspector();
    touch();
  });
  document.querySelector("#disp-export")?.addEventListener("click", () => {
    exportDispJson().catch((e) => setStatus(String(e), "err"));
  });
  document.querySelector("#disp-import")?.addEventListener("click", () => {
    document.querySelector("#disp-import-file")?.click();
  });
  document.querySelector("#disp-import-file")?.addEventListener("change", (ev) => {
    const file = ev.target.files?.[0];
    ev.target.value = "";
    if (file) importDispJsonFile(file);
  });
  document.querySelector("#disp-apply")?.addEventListener("click", () => applyDispLayout());
  document.querySelector("#disp-refresh")?.addEventListener("click", () => refreshDispLayout());
  document.querySelector("#disp-save")?.addEventListener("click", async () => {
    if (!connected) {
      setStatus("Connect to save to flash (use Export JSON offline)", "err");
      return;
    }
    await applyDispLayout();
    await save();
  });
}

function flagLabels(flags) {
  const bits = [];
  if (flags & 0x01) bits.push("YEL");
  if (flags & 0x02) bits.push("BLU");
  if (flags & 0x10) bits.push("RED");
  if (flags & 0x20) bits.push("PIT");
  if (flags & 0x04) bits.push("TC");
  if (flags & 0x08) bits.push("ABS");
  return bits.length ? bits.join(" ") : "—";
}

function renderRaceStatus(st) {
  raceEnabled = !!st?.enabled;
  const chips = document.querySelector("#race-chips");
  const err = document.querySelector("#race-error");
  if (chips) {
    const gear =
      st.gear < 0 ? "R" : st.gear === 0 ? "N" : String(st.gear);
    const inject = st.enabled && st.connected ? "CDC" : st.enabled ? "listen only" : "—";
    chips.innerHTML = [
      ["Mode", st.enabled ? "ON" : "off"],
      ["Inject", inject],
      ["UDP", String(st.udpPort ?? 5000)],
      ["Telem Hz", (st.telemHz ?? 0).toFixed(0)],
      ["Score Hz", (st.scoringHz ?? 0).toFixed(0)],
      ["RPM", String(st.rpm ?? 0)],
      ["Gear", gear],
      ["Speed", `${(st.speedKph ?? 0).toFixed(0)} km/h`],
      ["Fuel", `${(st.fuelPct ?? 0).toFixed(0)}%`],
      ["Flags", flagLabels(st.flags ?? 0)],
    ]
      .map(
        ([k, v]) =>
          `<span class="chip${st.enabled && k === "Mode" ? " on" : ""}${
            k === "Inject" && inject === "CDC" ? " on" : ""
          }"><b>${k}</b> ${v}</span>`
      )
      .join("");
  }
  if (err) {
    err.textContent = st.lastError || "";
    err.className = "status" + (st.lastError ? " err" : "");
  }
  updateRaceControls();
}

function updateRaceControls() {
  const start = document.querySelector("#race-start");
  const stop = document.querySelector("#race-stop");
  const port = document.querySelector("#race-udp-port");
  if (start) start.disabled = raceEnabled;
  if (stop) stop.disabled = !raceEnabled;
  if (port) port.disabled = raceEnabled;
  if (els.logEnable) els.logEnable.disabled = !connected || raceEnabled;
}

async function raceStart() {
  const portEl = document.querySelector("#race-udp-port");
  const udpPort = Number(portEl?.value || 5000);
  try {
    await invoke("race_set_udp_port", { udpPort });
    const st = await invoke("race_start", { udpPort });
    if (els.logEnable && connected) els.logEnable.checked = false;
    renderRaceStatus(st);
    setStatus(
      connected
        ? "Race mode on — injecting to wheel; close window to hide in tray"
        : "Race mode on (listen only) — connect base to inject; close window to hide in tray",
      "ok"
    );
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function raceStop() {
  try {
    const st = await invoke("race_stop");
    renderRaceStatus(st);
    if (els.logEnable) els.logEnable.checked = true;
    setStatus("Race mode stopped", "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

function setStatus(msg, kind = "") {
  if (els.status) {
    els.status.textContent = msg;
    els.status.className = "status" + (kind ? ` ${kind}` : "");
  }
}

function updateConnPill() {
  const pill = document.querySelector("#conn-pill");
  const label = document.querySelector("#conn-pill-label");
  if (!pill || !label) return;
  if (connecting) {
    pill.dataset.state = "busy";
    label.textContent = "Connecting…";
  } else if (connected) {
    pill.dataset.state = "on";
    label.textContent = "Connected";
  } else {
    pill.dataset.state = "off";
    label.textContent = "Offline";
  }
}

/** Disable device-only controls; keep tabs and offline features usable. */
function updateLinkGates() {
  const on = connected;
  for (const root of document.querySelectorAll("[data-needs-link]")) {
    for (const el of root.querySelectorAll("button, input, select, textarea")) {
      if (el.id === "log-enable") continue;
      el.disabled = !on;
    }
  }
  for (const el of document.querySelectorAll("[data-needs-link-btn]")) {
    el.disabled = !on;
  }
  for (const banner of document.querySelectorAll("[data-offline-banner]")) {
    banner.hidden = on;
  }
  updateDispDeviceButtons();
  updateRaceControls();
  updateConnPill();
}

function setConnected(on) {
  connected = on;
  els.connect.disabled = on || connecting;
  els.disconnect.disabled = !on;
  els.port.disabled = on;
  els.logEnable.disabled = !on || raceEnabled;
  updateLinkGates();
  showTab(activeTab);
}

function showTab(name) {
  const allowed = new Set(["settings", "rim", "display", "race", "diag", "monitor"]);
  if (!allowed.has(name)) name = "display";
  activeTab = name;
  localStorage.setItem(TAB_KEY, name);
  for (const btn of document.querySelectorAll(".tab")) {
    btn.classList.toggle("active", btn.dataset.tab === name);
  }
  for (const panel of document.querySelectorAll(".tab-panel")) {
    panel.hidden = panel.dataset.panel !== name;
  }
  if (name === "diag" && lastTelem) onTelemetry(lastTelem);
  if (name === "display") drawDispPreview();
}

function updateDispDeviceButtons() {
  for (const id of ["disp-apply", "disp-refresh", "disp-save"]) {
    const btn = document.querySelector(`#${id}`);
    if (btn) btn.disabled = !connected;
  }
}

function resolveTheme(pref) {
  if (pref === "light" || pref === "dark") return pref;
  return window.matchMedia("(prefers-color-scheme: dark)").matches
    ? "dark"
    : "light";
}

function applyTheme() {
  const resolved = resolveTheme(themePref);
  document.documentElement.dataset.theme = resolved;
  for (const btn of document.querySelectorAll("[data-theme-pref]")) {
    btn.classList.toggle("active", btn.dataset.themePref === themePref);
  }
  if (lastTelem) onTelemetry(lastTelem);
}

function setThemePref(pref) {
  themePref = pref === "light" || pref === "dark" ? pref : "auto";
  localStorage.setItem(THEME_KEY, themePref);
  applyTheme();
}

function nearBottom(el, slack = 40) {
  return el.scrollTop + el.clientHeight >= el.scrollHeight - slack;
}

function appendLog(line) {
  if (logPaused) return;
  const shouldStick = els.autoscroll.checked;
  els.log.textContent += line + "\n";
  const lines = els.log.textContent.split("\n");
  if (lines.length > 800) {
    els.log.textContent = lines.slice(-600).join("\n");
  }
  if (shouldStick) {
    ignoreScrollEvent = true;
    els.log.scrollTop = els.log.scrollHeight;
    requestAnimationFrame(() => {
      ignoreScrollEvent = false;
    });
  }
}

function onLogScroll() {
  if (ignoreScrollEvent || !els.autoscroll) return;
  if (!nearBottom(els.log)) {
    if (els.autoscroll.checked) els.autoscroll.checked = false;
  } else if (!els.autoscroll.checked) {
    // Re-enable when user scrolls back to the bottom.
    els.autoscroll.checked = true;
  }
}

function fillFields(map) {
  for (const { key } of KEYS) {
    if (map[key] != null && inputs[key]) {
      inputs[key].value = String(map[key]);
    }
  }
  if (map?.rim_link != null) {
    lastRimLinked = String(map.rim_link) !== "0";
  }
  refreshProfilesFromMap(map);
  loadDispFromMap(map);
}

/** When rim links after connect (or reconnects), re-dump so rim_fw / rim settings update. */
async function refreshAfterRimLink() {
  if (!connected || rimRefreshBusy) return;
  rimRefreshBusy = true;
  try {
    // Let firmware finish link-up VersionGet / CfgGet; dump also re-requests.
    await new Promise((r) => setTimeout(r, 200));
    if (!connected) return;
    let map = await invoke("dump_settings");
    fillFields(map);
    if ((map.rim_fw == null || map.rim_fw === "?" || map.rim_fw === "") && connected) {
      await new Promise((r) => setTimeout(r, 300));
      if (!connected) return;
      map = await invoke("dump_settings");
      fillFields(map);
    }
    setStatus("Rim linked — firmware / settings refreshed", "ok");
  } catch (e) {
    if (connected) setStatus(String(e), "err");
  } finally {
    rimRefreshBusy = false;
  }
}

function refreshProfilesFromMap(map) {
  const sel = els.profileSlot;
  const nameIn = els.profileName;
  const activeLbl = els.profileActiveLabel;
  if (!sel || !map) return;

  const active = map.profile_active;
  if (activeLbl) {
    activeLbl.textContent =
      active == null || active === "none" || active === "-1"
        ? "Active: custom / none"
        : `Active: slot ${active}`;
  }

  for (let i = 0; i < 4; i++) {
    const opt = sel.options[i];
    if (!opt) continue;
    const used = String(map[`profile${i}_used`] ?? "0");
    const name = map[`profile${i}_name`] || "";
    const label = used === "1" ? name || `Slot ${i}` : "(empty)";
    opt.textContent = `${i} — ${label}`;
  }

  const slot = Number(sel.value);
  if (nameIn && Number.isFinite(slot) && document.activeElement !== nameIn) {
    nameIn.value = map[`profile${slot}_name`] || "";
  }
}

async function profileLoad() {
  const slot = Number(els.profileSlot?.value ?? 0);
  try {
    const map = await invoke("run_dump_command", { line: `:profile load ${slot}` });
    fillFields(map);
    setStatus(`Loaded profile slot ${slot} (RAM) — Save to flash to persist`, "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function profileSaveSlot() {
  const slot = Number(els.profileSlot?.value ?? 0);
  const name = (els.profileName?.value || "").trim().replace(/\s+/g, "_").slice(0, 11);
  try {
    const line = name ? `:profile save ${slot} ${name}` : `:profile save ${slot}`;
    const reply = await invoke("send_raw", { line });
    await new Promise((r) => setTimeout(r, 80));
    const map = await invoke("dump_settings");
    fillFields(map);
    setStatus(`Saved into slot ${slot} (RAM) — Save to flash to persist`, "ok");
    void reply;
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function profileRename() {
  const slot = Number(els.profileSlot?.value ?? 0);
  const name = (els.profileName?.value || "").trim().replace(/\s+/g, "_").slice(0, 11);
  if (!name) {
    setStatus("Enter a name first", "err");
    return;
  }
  try {
    await invoke("send_raw", { line: `:profile name ${slot} ${name}` });
    await new Promise((r) => setTimeout(r, 80));
    const map = await invoke("dump_settings");
    fillFields(map);
    setStatus(`Renamed slot ${slot}`, "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function profileClear() {
  const slot = Number(els.profileSlot?.value ?? 0);
  try {
    await invoke("send_raw", { line: `:profile clear ${slot}` });
    await new Promise((r) => setTimeout(r, 80));
    const map = await invoke("dump_settings");
    fillFields(map);
    setStatus(`Cleared slot ${slot}`, "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

function chip(label, cls = "") {
  const span = document.createElement("span");
  span.className = "chip" + (cls ? ` ${cls}` : "");
  span.textContent = label;
  return span;
}

function renderChips(t) {
  els.chips.replaceChildren(
    chip(t.hall ? "HALL ok" : "HALL fail", t.hall ? "on" : "err"),
    chip(t.idx ? "INDEX active" : "INDEX idle", t.idx ? "on" : ""),
    chip(`edges ${t.edges}`),
    chip(t.home && t.home !== "-" ? `home ${t.home}` : "home idle", t.home && t.home !== "-" ? "warn" : ""),
    chip(t.adxl ? (t.ax != null ? `adxl ax=${t.ax}` : "adxl ok") : "adxl —", t.adxl ? "on" : ""),
    chip(t.motors ? "motors ON" : "motors off", t.motors ? "warn" : ""),
    chip(`ffb ${t.ffb}`),
    chip(`torq ${t.torq.toFixed(3)}`),
    chip(`gear ${t.gear.toFixed(3)}`),
    chip(`hidX ${t.hidX}`),
    chip(`sens ${t.sens.toFixed(1)}°`),
    chip(t.rim ? "rim link" : "rim down", t.rim ? "on" : "err")
  );
}

function drawAxle(t) {
  const { ctx, w, h } = fitCanvasToElement(els.axleCanvas, 640, 72);
  ctx.clearRect(0, 0, w, h);

  const half = Math.max(10, (t.hidRange || 900) * 0.5);
  const n = Math.max(-1, Math.min(1, t.axle / half));
  const padX = 4;
  const x = (n + 1) * 0.5 * (w - padX * 2) + padX;
  const cy = h / 2;

  ctx.fillStyle = getCss("--input-bg") || "#0c0c0c";
  if (ctx.roundRect) {
    ctx.beginPath();
    ctx.roundRect(padX, cy - 10, w - padX * 2, 20, 6);
    ctx.fill();
    ctx.strokeStyle = getCss("--border") || "#2c2c2c";
    ctx.lineWidth = 1;
    ctx.stroke();
  } else {
    ctx.fillRect(padX, cy - 10, w - padX * 2, 20);
    ctx.strokeStyle = getCss("--border") || "#2c2c2c";
    ctx.strokeRect(padX + 0.5, cy - 9.5, w - padX * 2 - 1, 19);
  }

  ctx.strokeStyle = getCss("--muted") || "#9b9890";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(w / 2, cy - 16);
  ctx.lineTo(w / 2, cy + 16);
  ctx.stroke();

  // Rhombus on a HiDPI backing store (full-width bar via CSS)
  const papaya = getCss("--papaya") || "#ff8000";
  const rw = 8;
  const rh = 14;
  ctx.beginPath();
  ctx.moveTo(x, cy - rh);
  ctx.lineTo(x + rw, cy);
  ctx.lineTo(x, cy + rh);
  ctx.lineTo(x - rw, cy);
  ctx.closePath();
  ctx.fillStyle = papaya;
  ctx.fill();
  ctx.strokeStyle = papaya;
  ctx.lineWidth = 1.5;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.stroke();

  els.axleLabel.textContent = `${t.axle.toFixed(2)}°  (±${half.toFixed(0)}° HID)`;

  axleHistory.push(t.axle);
  if (axleHistory.length > SPARK_LEN) axleHistory.shift();
  drawSpark(half);
}

function drawSpark(half) {
  const { ctx, w, h } = fitCanvasToElement(els.axleSpark, 640, 56);
  ctx.fillStyle = getCss("--canvas-bg") || "#0c0c0c";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = getCss("--border") || "#2c2c2c";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, h / 2);
  ctx.lineTo(w, h / 2);
  ctx.stroke();

  if (axleHistory.length < 2) return;
  ctx.strokeStyle = getCss("--papaya") || "#ff8000";
  ctx.lineWidth = 1.75;
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.beginPath();
  axleHistory.forEach((v, i) => {
    const x = (i / (SPARK_LEN - 1)) * (w - 2) + 1;
    const nn = Math.max(-1, Math.min(1, v / half));
    const y = (1 - (nn + 1) * 0.5) * (h - 8) + 4;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();
}

function drawPedal(el, raw, n, minV, maxV, restV, armed, color) {
  const canvas = el.querySelector(".pedal-canvas");
  const { ctx, w, h } = fitCanvasToElement(canvas, 200, 160);
  ctx.clearRect(0, 0, w, h);

  const pad = 18;
  const barX = w / 2 - 18;
  const barW = 36;
  const barTop = pad;
  const barH = h - pad * 2;

  ctx.fillStyle = getCss("--input-bg") || "#0c0c0c";
  if (ctx.roundRect) {
    ctx.beginPath();
    ctx.roundRect(barX, barTop, barW, barH, 4);
    ctx.fill();
    ctx.strokeStyle = getCss("--border") || "#2c2c2c";
    ctx.lineWidth = 1;
    ctx.stroke();
  } else {
    ctx.fillRect(barX, barTop, barW, barH);
    ctx.strokeStyle = getCss("--border") || "#2c2c2c";
    ctx.strokeRect(barX + 0.5, barTop + 0.5, barW - 1, barH - 1);
  }

  const yOf = (adc) => barTop + (1 - adc / ADC_MAX) * barH;

  const mark = (adc, stroke, label) => {
    const y = yOf(adc);
    ctx.strokeStyle = stroke;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(barX - 8, y);
    ctx.lineTo(barX + barW + 8, y);
    ctx.stroke();
    ctx.fillStyle = stroke;
    ctx.font = "10px sans-serif";
    ctx.fillText(label, barX + barW + 10, y + 3);
  };
  const markColor = getCss("--warn") || "#d4a017";
  const restColor = getCss("--muted") || "#9b9890";
  if (armed) {
    mark(minV, markColor, "min");
    mark(maxV, markColor, "max");
  }
  if (restV > 0) mark(restV, restColor, "rest");

  const yRaw = yOf(raw);
  ctx.fillStyle = color;
  if (ctx.roundRect) {
    ctx.beginPath();
    ctx.roundRect(barX + 4, yRaw - 3, barW - 8, 6, 2);
    ctx.fill();
  } else {
    ctx.fillRect(barX + 4, yRaw - 3, barW - 8, 6);
  }

  const fillH = Math.max(0, Math.min(1, n)) * barH;
  ctx.globalAlpha = 0.35;
  ctx.fillStyle = color;
  ctx.fillRect(barX + 2, barTop + barH - fillH, barW - 4, fillH);
  ctx.globalAlpha = 1;

  el.querySelector(".pedal-n").textContent = `${(n * 100).toFixed(0)}%  ADC ${raw}`;
  el.querySelector(".pedal-meta").textContent = armed
    ? `armed  min ${minV}  max ${maxV}  rest ${restV}`
    : `learning  rest ${restV || "—"}  (press once to arm)`;
}

function onTelemetry(t) {
  lastTelem = t;
  renderChips(t);
  drawAxle(t);
  drawPedal(
    els.pedalT,
    t.adcT,
    t.nT,
    t.tMin,
    t.tMax,
    t.tRest,
    t.tArm,
    getCss("--throttle")
  );
  drawPedal(
    els.pedalB,
    t.adcB,
    t.nB,
    t.bMin,
    t.bMax,
    t.bRest,
    t.bArm,
    getCss("--brake")
  );
  drawPedal(
    els.pedalC,
    t.adcC,
    t.nC,
    t.cMin,
    t.cMax,
    t.cRest,
    t.cArm,
    getCss("--clutch")
  );

  if (connected) {
    if (inputs.rim_link) {
      inputs.rim_link.value = t.rim ? "1" : "0";
    }
    if (t.rim && !lastRimLinked) {
      lastRimLinked = true;
      void refreshAfterRimLink();
    } else if (!t.rim) {
      lastRimLinked = false;
      if (inputs.rim_fw && inputs.rim_fw.value !== "?") {
        inputs.rim_fw.value = "?";
      }
    }
  }
}

function getCss(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

/** Backing-store sized for devicePixelRatio; CSS keeps the bar full-width. */
function fitCanvas(canvas, cssW, cssH) {
  const dpr = Math.max(1, window.devicePixelRatio || 1);
  const w = Math.max(1, Math.round(cssW));
  const h = Math.max(1, Math.round(cssH));
  if (
    canvas._cssW === w &&
    canvas._cssH === h &&
    canvas._dpr === dpr &&
    canvas.width === Math.round(w * dpr)
  ) {
    const ctx = canvas.getContext("2d");
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return { ctx, w, h, dpr };
  }
  canvas._cssW = w;
  canvas._cssH = h;
  canvas._dpr = dpr;
  canvas.width = Math.round(w * dpr);
  canvas.height = Math.round(h * dpr);
  // Full-width via CSS; only pin height so the bitmap aspect doesn't stretch.
  canvas.style.width = "100%";
  canvas.style.height = `${h}px`;
  const ctx = canvas.getContext("2d");
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.imageSmoothingEnabled = true;
  return { ctx, w, h, dpr };
}

function fitCanvasToElement(canvas, fallbackW, fallbackH) {
  const rect = canvas.getBoundingClientRect();
  // Prefer laid-out width (full card); fall back before first layout.
  const w = rect.width > 2 ? rect.width : fallbackW;
  const h = fallbackH;
  return fitCanvas(canvas, w, h);
}

async function bindEvents() {
  if (unlistenLine) await unlistenLine();
  if (unlistenTelem) await unlistenTelem();
  unlistenLine = await listen("serial-line", (e) => appendLog(String(e.payload)));
  unlistenTelem = await listen("telemetry", (e) => onTelemetry(e.payload));
}

function portLabel(p) {
  const bits = [];
  if (p.product) bits.push(p.product);
  else if (p.manufacturer) bits.push(p.manufacturer);
  if (p.vid != null && p.pid != null) {
    bits.push(
      `${p.vid.toString(16).padStart(4, "0")}:${p.pid.toString(16).padStart(4, "0")}`
    );
  }
  if (p.likelyPico) bits.push("pico?");
  return bits.length ? `${p.name}  (${bits.join(" · ")})` : p.name;
}

function pickBestPort(ports) {
  if (!ports.length) return null;
  const last = localStorage.getItem(LAST_PORT_KEY);
  const picos = ports.filter((p) => p.likelyPico);
  if (last && picos.some((p) => p.name === last)) {
    return last;
  }
  if (picos.length === 1) return picos[0].name;
  if (picos.length > 1) {
    // Prefer a remembered path among picos, else first.
    return picos[0].name;
  }
  // No USB match — only auto-pick if a single ACM/modem-looking name exists.
  const acm = ports.filter(
    (p) =>
      /ttyACM|usbmodem|CU\.usbmodem/i.test(p.name) ||
      /Pico Serial/i.test(p.portType || "")
  );
  if (acm.length === 1) return acm[0].name;
  if (last && ports.some((p) => p.name === last)) return last;
  return null;
}

async function refreshPorts({ quiet = false } = {}) {
  const ports = await invoke("list_ports");
  latestPorts = ports;
  const prev = els.port.value;
  const best = pickBestPort(ports);
  els.port.innerHTML = "";
  for (const p of ports) {
    const opt = document.createElement("option");
    opt.value = p.name;
    opt.textContent = portLabel(p);
    els.port.appendChild(opt);
  }
  if (ports.some((p) => p.name === prev)) {
    els.port.value = prev;
  } else if (best) {
    els.port.value = best;
  }
  if (!ports.length && !quiet) setStatus("No serial ports found", "err");
  return ports;
}

async function connect({ auto = false } = {}) {
  if (connecting || connected) return;
  const path = els.port.value;
  if (!path) {
    if (!auto) setStatus("Pick a port", "err");
    return;
  }
  connecting = true;
  els.connect.disabled = true;
  updateConnPill();
  try {
    setStatus(auto ? `Auto-connecting to ${path}…` : `Connecting to ${path}…`);
    await new Promise((r) => requestAnimationFrame(() => r()));
    await bindEvents();
    const map = await invoke("connect", { path });
    fillFields(map);
    connecting = false;
    setConnected(true);
    userDisconnected = false;
    localStorage.setItem(LAST_PORT_KEY, path);
    els.logEnable.checked = true;
    setStatus(`Connected — ${path}`, "ok");
  } catch (e) {
    lastRimLinked = false;
    rimRefreshBusy = false;
    setConnected(false);
    setStatus(String(e), "err");
  } finally {
    connecting = false;
    if (!connected) els.connect.disabled = false;
    updateConnPill();
  }
}

async function disconnect() {
  userDisconnected = true;
  try {
    await invoke("disconnect");
  } catch (_) {}
  lastRimLinked = false;
  rimRefreshBusy = false;
  setConnected(false);
  setStatus("Disconnected");
}

async function tryAutoConnect() {
  if (!els.autoConnect?.checked || connected || connecting) {
    return;
  }
  await refreshPorts({ quiet: true });

  const last = localStorage.getItem(LAST_PORT_KEY);
  // After a manual disconnect, allow auto again once that port disappears (unplug).
  if (userDisconnected && last && !latestPorts.some((p) => p.name === last)) {
    userDisconnected = false;
  }
  if (userDisconnected) return;

  const best = pickBestPort(latestPorts);
  if (!best) return;
  els.port.value = best;
  await connect({ auto: true });
}

async function apply() {
  try {
    for (const entry of KEYS) {
      if (entry.readonly) continue;
      const { key } = entry;
      const v = Number(inputs[key].value);
      if (Number.isNaN(v)) throw new Error(`bad value for ${key}`);
      const reply = await invoke("set_setting", { key, value: v });
      if (String(reply).startsWith("ERR")) throw new Error(reply);
    }
    const map = await invoke("dump_settings");
    fillFields(map);
    setStatus("Applied (RAM only — use Save to persist)", "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function sendCmd(line) {
  try {
    await invoke("send_raw", { line });
    setStatus(`Sent ${line}`, "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function save() {
  try {
    const reply = await invoke("save_settings");
    setStatus(String(reply), String(reply).startsWith("OK") ? "ok" : "err");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function reload() {
  try {
    const map = await invoke("load_settings");
    fillFields(map);
    setStatus("Loaded from flash", "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function defaults() {
  try {
    const map = await invoke("reset_defaults");
    fillFields(map);
    setStatus("Defaults applied (RAM) — Save to persist", "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function dump() {
  try {
    const map = await invoke("dump_settings");
    fillFields(map);
    setStatus("Dump refreshed", "ok");
  } catch (e) {
    setStatus(String(e), "err");
  }
}

async function sendRaw() {
  const line = els.rawInput.value;
  if (!line.trim()) return;
  try {
    await invoke("send_raw", { line });
    els.rawInput.value = "";
  } catch (e) {
    setStatus(String(e), "err");
  }
}

window.addEventListener("DOMContentLoaded", () => {
  els.port = document.querySelector("#port");
  els.refresh = document.querySelector("#refresh");
  els.connect = document.querySelector("#connect");
  els.disconnect = document.querySelector("#disconnect");
  els.tabs = document.querySelector("#tabs");
  els.status = document.querySelector("#status");
  els.fields = document.querySelector("#fields");
  els.fieldsRim = document.querySelector("#fields-rim");
  els.fieldsFw = document.querySelector("#fields-fw");
  els.profileSlot = document.querySelector("#profile-slot");
  els.profileName = document.querySelector("#profile-name");
  els.profileActiveLabel = document.querySelector("#profile-active-label");
  els.chips = document.querySelector("#chips");
  els.axleCanvas = document.querySelector("#axle-canvas");
  els.axleSpark = document.querySelector("#axle-spark");
  els.axleLabel = document.querySelector("#axle-label");
  els.log = document.querySelector("#log");
  els.rawInput = document.querySelector("#raw-input");
  els.logEnable = document.querySelector("#log-enable");
  els.autoConnect = document.querySelector("#auto-connect");
  els.autoscroll = document.querySelector("#autoscroll");
  els.pedalT = document.querySelector('.pedal[data-axis="t"]');
  els.pedalB = document.querySelector('.pedal[data-axis="b"]');
  els.pedalC = document.querySelector('.pedal[data-axis="c"]');

  const savedAuto = localStorage.getItem(AUTO_CONNECT_KEY);
  if (savedAuto != null) els.autoConnect.checked = savedAuto !== "0";
  const savedTab = localStorage.getItem(TAB_KEY);
  if (savedTab === "settings" || savedTab === "race" || savedTab === "rim" || savedTab === "display" || savedTab === "diag" || savedTab === "monitor") {
    activeTab = savedTab;
  }
  const savedTheme = localStorage.getItem(THEME_KEY);
  themePref =
    savedTheme === "light" || savedTheme === "dark" || savedTheme === "auto"
      ? savedTheme
      : "auto";
  mediaDark = window.matchMedia("(prefers-color-scheme: dark)");
  mediaDark.addEventListener("change", () => {
    if (themePref === "auto") applyTheme();
  });
  for (const btn of document.querySelectorAll("[data-theme-pref]")) {
    btn.addEventListener("click", () => setThemePref(btn.dataset.themePref));
  }
  applyTheme();

  for (const { key, label, step, group, readonly, text } of KEYS) {
    const wrap = document.createElement("div");
    wrap.className = "field";
    const lab = document.createElement("label");
    lab.htmlFor = key;
    lab.textContent = label;
    const input = document.createElement("input");
    input.type = text ? "text" : "number";
    input.id = key;
    if (!text) input.step = step;
    if (text) {
      input.spellcheck = false;
      input.classList.add("fw-id");
    }
    if (readonly) input.readOnly = true;
    inputs[key] = input;
    wrap.append(lab, input);
    const parent =
      group === "ffb" || !group
        ? els.fields
        : group === "version"
          ? els.fieldsFw || els.fieldsRim || els.fields
          : els.fieldsRim || els.fields;
    parent.appendChild(wrap);
  }

  for (const btn of document.querySelectorAll(".tab")) {
    btn.addEventListener("click", () => showTab(btn.dataset.tab));
  }

  initDispEditor();

  els.refresh.addEventListener("click", () =>
    refreshPorts().catch((e) => setStatus(String(e), "err"))
  );
  els.connect.addEventListener("click", () => {
    userDisconnected = false;
    connect({ auto: false });
  });
  els.disconnect.addEventListener("click", disconnect);
  document.querySelector("#apply").addEventListener("click", apply);
  document.querySelector("#apply-rim")?.addEventListener("click", apply);
  document.querySelector("#rim-sync")?.addEventListener("click", async () => {
    try {
      await sendCmd(":rim_sync");
      await new Promise((r) => setTimeout(r, 200));
      await dump();
      setStatus("Rim settings refreshed", "ok");
    } catch (e) {
      setStatus(String(e), "err");
    }
  });
  document.querySelector("#rim-reset")?.addEventListener("click", () => sendCmd(":rim_reset"));
  document.querySelector("#rim-updater")?.addEventListener("click", () => sendCmd(":rim_updater"));
  document.querySelector("#rim-bootsel")?.addEventListener("click", () =>
    sendCmd(":rim_bootsel")
  );
  document.querySelector("#leds-off")?.addEventListener("click", () => sendCmd(":leds_off"));
  document.querySelector("#leds-auto")?.addEventListener("click", () => sendCmd(":leds_auto"));
  document.querySelector("#leds-zones")?.addEventListener("click", () => sendCmd(":leds_zones"));
  document.querySelector("#leds-chase")?.addEventListener("click", () => sendCmd(":leds_chase"));
  document.querySelector("#leds-rainbow")?.addEventListener("click", () =>
    sendCmd(":leds_rainbow")
  );
  document.querySelector("#leds-boot")?.addEventListener("click", () => sendCmd(":leds_boot"));
  document.querySelector("#leds-solid-red")?.addEventListener("click", () =>
    sendCmd(":leds_solid 80 0 0")
  );
  document.querySelector("#leds-solid-green")?.addEventListener("click", () =>
    sendCmd(":leds_solid 0 80 0")
  );
  document.querySelector("#leds-fill-all")?.addEventListener("click", () =>
    sendCmd(":leds_fill 11 40 40 40")
  );
  document.querySelector("#leds-flags-all")?.addEventListener("click", () =>
    sendCmd(":leds_flags 0x0F")
  );
  document.querySelector("#leds-flag-yellow")?.addEventListener("click", () =>
    sendCmd(":leds_flags 0x01")
  );
  document.querySelector("#leds-flag-blue")?.addEventListener("click", () =>
    sendCmd(":leds_flags 0x02")
  );
  document.querySelector("#leds-flag-yb")?.addEventListener("click", () =>
    sendCmd(":leds_flags 0x03")
  );
  document.querySelector("#leds-flag-red")?.addEventListener("click", () =>
    sendCmd(":leds_flags 0x10")
  );
  document.querySelector("#leds-pit")?.addEventListener("click", () =>
    sendCmd(":leds_flags 0x20")
  );
  document.querySelector("#leds-tc")?.addEventListener("click", () => sendCmd(":leds_flags 0x04"));
  document.querySelector("#leds-abs")?.addEventListener("click", () => sendCmd(":leds_flags 0x08"));
  document.querySelector("#leds-tc-abs")?.addEventListener("click", () =>
    sendCmd(":leds_flags 0x0C")
  );
  document.querySelector("#leds-rpm-go")?.addEventListener("click", () => {
    const rpm = Number(document.querySelector("#leds-rpm")?.value ?? 0);
    const withFlags = document.querySelector("#leds-with-flags")?.checked;
    const flags = withFlags ? "0x0F" : "0";
    sendCmd(`:leds_rpm ${Number.isFinite(rpm) ? Math.round(rpm) : 0} ${flags}`);
  });
  document.querySelector("#btnleds-all")?.addEventListener("click", () => sendCmd(":btnleds 0x3FF"));
  document.querySelector("#btnleds-off")?.addEventListener("click", () => sendCmd(":btnleds 0"));
  document.querySelector("#btnleds-auto")?.addEventListener("click", () => sendCmd(":btnleds auto 1"));

  async function refreshLastGoodLabel() {
    const el = document.querySelector("#fw-pack-last-good");
    if (!el) return;
    try {
      const info = await invoke("last_good_pack_info");
      if (!info) {
        el.textContent = "Last-good: none yet";
        return;
      }
      const [filename, fwId] = info;
      el.textContent = `Last-good: ${filename}${fwId ? ` (${fwId})` : ""}`;
    } catch (_) {
      el.textContent = "Last-good: none yet";
    }
  }
  refreshLastGoodLabel();

  document.querySelector("#fw-pack-rollback")?.addEventListener("click", async () => {
    const status = document.querySelector("#rim-flash-status");
    if (!connected) {
      setStatus("Connect to base-mcu first", "err");
      return;
    }
    try {
      const [buf, filename, fwId] = await invoke("load_last_good_pack");
      status.textContent = `Restoring ${filename}${fwId ? ` (${fwId})` : ""}…`;
      setStatus("Rollback pack update in progress…", "");
      connected = false;
      const unlisten = await listen("flash-progress", () => {});
      const reply = await invoke("flash_firmware_pack", {
        data: buf,
        filename,
      });
      unlisten();
      status.textContent = String(reply);
      setStatus(String(reply), "ok");
      setConnected(false);
      refreshLastGoodLabel();
    } catch (e) {
      if (status) status.textContent = String(e);
      setStatus(String(e), "err");
      setConnected(false);
    }
  });

  document.querySelector("#fw-pack-flash")?.addEventListener("click", async () => {
    const input = document.querySelector("#fw-pack-file");
    const status = document.querySelector("#rim-flash-status");
    const file = input?.files?.[0];
    if (!file) {
      setStatus("Choose an ffb-firmware-*.zip pack first", "err");
      return;
    }
    if (!connected) {
      setStatus("Connect to base-mcu first", "err");
      return;
    }
    try {
      status.textContent = `Reading pack ${file.name}…`;
      const buf = new Uint8Array(await file.arrayBuffer());
      setStatus("Firmware pack update in progress…", "");
      connected = false;
      const unlisten = await listen("flash-progress", (e) => {
        const p = e.payload || {};
        if (p.event === "pack") {
          if (p.phase === "rim") {
            status.textContent = `Pack ${p.release || ""}: flashing rim (${p.rimBytes} bytes)…`;
          } else if (p.phase === "rim-done") {
            status.textContent = "Rim done — preparing base BOOTSEL…";
          } else if (p.phase === "base-bootsel") {
            status.textContent = "Base rebooting to BOOTSEL…";
          } else if (p.phase === "base-uf2") {
            status.textContent = "Copying base.uf2 to RPI-RP2…";
          } else if (p.phase === "done") {
            status.textContent = "Pack update finished — reconnect";
          }
        } else if (typeof p.received === "number" && typeof p.total === "number") {
          const pct = Math.floor((100 * p.received) / p.total);
          status.textContent = `Rim OTA… ${p.received}/${p.total} (${pct}%)`;
        } else if (p.phase) {
          status.textContent = `OTA ${p.phase}…`;
        }
      });
      const reply = await invoke("flash_firmware_pack", {
        data: Array.from(buf),
        filename: file.name,
      });
      unlisten();
      status.textContent = String(reply);
      setStatus(String(reply), "ok");
      setConnected(false);
      refreshLastGoodLabel();
    } catch (e) {
      if (status) status.textContent = String(e);
      setStatus(String(e), "err");
      setConnected(false);
    }
  });
  document.querySelector("#rim-flash")?.addEventListener("click", async () => {
    const input = document.querySelector("#rim-fw-file");
    const status = document.querySelector("#rim-flash-status");
    const file = input?.files?.[0];
    if (!file) {
      setStatus("Choose a .bin or .uf2 first", "err");
      return;
    }
    if (!connected) {
      setStatus("Connect to base-mcu first", "err");
      return;
    }
    try {
      status.textContent = `Reading ${file.name}…`;
      const buf = new Uint8Array(await file.arrayBuffer());
      status.textContent = `Flashing ${file.name} (${buf.length} bytes)…`;
      setStatus("Rim OTA in progress…", "");
      const unlisten = await listen("flash-progress", (e) => {
        const p = e.payload || {};
        if (p.event === "done") {
          status.textContent = "Flash done — rim rebooting";
        } else if (typeof p.received === "number" && typeof p.total === "number") {
          const pct = Math.floor((100 * p.received) / p.total);
          status.textContent = `Flashing… ${p.received}/${p.total} (${pct}%)`;
        } else if (typeof p.total === "number") {
          status.textContent = `Starting flash (${p.total} bytes)…`;
        } else if (p.phase) {
          status.textContent = `OTA ${p.phase}…`;
        }
      });
      const reply = await invoke("flash_rim", {
        data: Array.from(buf),
        filename: file.name,
      });
      unlisten();
      status.textContent = String(reply);
      setStatus(String(reply), "ok");
    } catch (e) {
      if (status) status.textContent = String(e);
      setStatus(String(e), "err");
    }
  });
  document.querySelector("#save").addEventListener("click", save);
  document.querySelector("#reload").addEventListener("click", reload);
  document.querySelector("#defaults").addEventListener("click", defaults);
  document.querySelector("#dump").addEventListener("click", dump);
  document.querySelector("#recenter")?.addEventListener("click", () => sendCmd(":recenter"));
  document.querySelector("#adxl-cal")?.addEventListener("click", async () => {
    try {
      await invoke("send_raw", { line: ":adxl_cal" });
      setStatus("ADXL calibrate sent — align wheel first; :save to persist offset", "ok");
      // Refresh dump so adxl_cal / adxl_x_offset appear if present.
      try {
        await dump();
      } catch (_) {}
    } catch (e) {
      setStatus(String(e), "err");
    }
  });
  document.querySelector("#selftest")?.addEventListener("click", async () => {
    try {
      const map = await invoke("run_dump_command", { line: ":selftest" });
      const bits = ["hall", "rim", "pedals", "motors", "index", "edges", "axle"]
        .map((k) => (map[k] != null ? `${k}=${map[k]}` : null))
        .filter(Boolean);
      setStatus(bits.length ? `Self-test: ${bits.join(" · ")}` : "Self-test done", "ok");
      appendLog(
        Object.entries(map)
          .map(([k, v]) => `${k}=${v}`)
          .join("\n")
      );
    } catch (e) {
      setStatus(String(e), "err");
    }
  });
  document.querySelector("#profile-load")?.addEventListener("click", profileLoad);
  document.querySelector("#profile-save-slot")?.addEventListener("click", profileSaveSlot);
  document.querySelector("#profile-rename")?.addEventListener("click", profileRename);
  document.querySelector("#profile-clear")?.addEventListener("click", profileClear);
  els.profileSlot?.addEventListener("change", async () => {
    try {
      const map = await invoke("dump_settings");
      refreshProfilesFromMap(map);
    } catch (_) {}
  });
  document.querySelector("#clear-log").addEventListener("click", () => {
    els.log.textContent = "";
  });
  document.querySelector("#pause-log").addEventListener("click", (e) => {
    logPaused = !logPaused;
    e.target.textContent = logPaused ? "Resume" : "Pause";
  });
  document.querySelector("#raw-send").addEventListener("click", sendRaw);
  els.rawInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") sendRaw();
  });
  els.log.addEventListener("scroll", onLogScroll, { passive: true });
  els.autoscroll.addEventListener("change", () => {
    if (els.autoscroll.checked) {
      ignoreScrollEvent = true;
      els.log.scrollTop = els.log.scrollHeight;
      requestAnimationFrame(() => {
        ignoreScrollEvent = false;
      });
    }
  });
  els.logEnable.addEventListener("change", async () => {
    try {
      await invoke("set_telemetry_log", { enabled: els.logEnable.checked });
    } catch (e) {
      setStatus(String(e), "err");
    }
  });
  document.querySelector("#race-start")?.addEventListener("click", () => {
    raceStart().catch(() => {});
  });
  document.querySelector("#race-stop")?.addEventListener("click", () => {
    raceStop().catch(() => {});
  });
  listen("race-status", (e) => renderRaceStatus(e.payload)).catch(() => {});
  invoke("race_status")
    .then((st) => renderRaceStatus(st))
    .catch(() => {});

  els.autoConnect.addEventListener("change", () => {
    localStorage.setItem(AUTO_CONNECT_KEY, els.autoConnect.checked ? "1" : "0");
    if (els.autoConnect.checked) {
      userDisconnected = false;
      tryAutoConnect().catch(() => {});
    }
  });

  setConnected(false);
  refreshPorts()
    .then(() => tryAutoConnect())
    .catch((e) => setStatus(String(e), "err"));

  autoTimer = setInterval(() => {
    tryAutoConnect().catch(() => {});
  }, 2000);
});
