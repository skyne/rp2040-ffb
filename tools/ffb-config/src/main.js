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
const THEME_KEY = "ffb.theme";

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
let activeTab = "settings";
let ignoreScrollEvent = false;
let themePref = "auto";
let mediaDark = null;

function setStatus(msg, kind = "") {
  els.status.textContent = msg;
  els.status.className = "status" + (kind ? ` ${kind}` : "");
}

function setConnected(on) {
  connected = on;
  els.connect.disabled = on || connecting;
  els.disconnect.disabled = !on;
  els.port.disabled = on;
  els.tabs.hidden = !on;
  els.logEnable.disabled = !on;
  showTab(activeTab);
}

function showTab(name) {
  activeTab = name;
  localStorage.setItem(TAB_KEY, name);
  for (const btn of document.querySelectorAll(".tab")) {
    btn.classList.toggle("active", btn.dataset.tab === name);
  }
  for (const panel of document.querySelectorAll(".tab-panel")) {
    const show = connected && panel.dataset.panel === name;
    panel.hidden = !show;
  }
  if (name === "diag" && lastTelem) onTelemetry(lastTelem);
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
  refreshProfilesFromMap(map);
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
  try {
    setStatus(auto ? `Auto-connecting to ${path}…` : `Connecting to ${path}…`);
    await new Promise((r) => requestAnimationFrame(() => r()));
    await bindEvents();
    const map = await invoke("connect", { path });
    fillFields(map);
    setConnected(true);
    userDisconnected = false;
    localStorage.setItem(LAST_PORT_KEY, path);
    els.logEnable.checked = true;
    setStatus(`Connected — ${path}`, "ok");
  } catch (e) {
    setConnected(false);
    setStatus(String(e), "err");
  } finally {
    connecting = false;
    if (!connected) els.connect.disabled = false;
  }
}

async function disconnect() {
  userDisconnected = true;
  try {
    await invoke("disconnect");
  } catch (_) {}
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
  if (savedTab === "settings" || savedTab === "rim" || savedTab === "diag" || savedTab === "monitor") {
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
