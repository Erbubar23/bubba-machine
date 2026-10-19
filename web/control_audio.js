// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// Las tomas del sampler en la plataforma de control (control.html): el audio se decodifica y se lleva
// aquí a lo que graba el FM-1 (mono, a la frecuencia que da su informe, 8 bits con dither), como
// tools/fm1/toma.py. También el BPM (del nombre del archivo o adivinado), los compases como los cuenta
// el firmware (CMD_SMP_IMPORT en core/bm_audio.c) y la onda con el recorte.

// archivo -> Float32Array mono a 'rate' Hz (el remuestreo del navegador filtra antes de bajar)
export async function decodificar(file, rate) {
  const ctx = new (window.AudioContext || window.webkitAudioContext)();
  let buf;
  try { buf = await ctx.decodeAudioData(await file.arrayBuffer()); } finally { ctx.close(); }
  const off = new OfflineAudioContext(1, Math.max(1, Math.ceil(buf.duration * rate)), rate);
  const src = off.createBufferSource();
  src.buffer = buf; src.connect(off.destination); src.start();
  return (await off.startRendering()).getChannelData(0);
}

// BPM escrito en el nombre ("loop 128bpm.wav", "128 BPM")
export function bpmDelNombre(name) {
  const m = /(\d{2,3}(?:[.,]\d)?)\s*-?\s*bpm/i.exec(name || "");
  const v = m ? parseFloat(m[1].replace(",", ".")) : 0;
  return v >= 40 && v <= 240 ? v : 0;
}

// BPM adivinado: autocorrelación de los ataques entre 70 y 180 BPM; si el recorte dura compases
// enteros a un tempo cercano, se queda con ese (un loop bien cortado lo dice exacto)
export function adivinarBpm(x, rate) {
  const hop = 256, n = Math.floor(x.length / hop);
  if (n < 32) return 0;
  const env = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    let e = 0;
    for (let k = i * hop; k < (i + 1) * hop; k++) e += x[k] * x[k];
    env[i] = Math.log(1e-6 + e);
  }
  const on = new Float32Array(n);
  for (let i = 1; i < n; i++) on[i] = Math.max(0, env[i] - env[i - 1]);
  const fps = rate / hop;
  let best = 0, bestLag = 0;
  for (let bpm = 70; bpm <= 180; bpm += 0.5) {
    const lag = 60 * fps / bpm, l0 = Math.floor(lag), f = lag - l0;
    let s = 0;
    for (let i = 0; i + l0 + 1 < n; i++) s += on[i] * (on[i + l0] * (1 - f) + on[i + l0 + 1] * f);
    s /= n - l0;
    if (s > best) { best = s; bestLag = bpm; }
  }
  if (!bestLag) return 0;
  const secs = x.length / rate;
  for (const bars of [1, 2, 4, 8]) {
    const b = 240 * bars / secs;
    if (b >= 70 && b <= 180 && Math.abs(b - bestLag) / bestLag < 0.04) return Math.round(b * 10) / 10;
  }
  return bestLag;
}

// compases como los cuenta el firmware: un compás = rate * 2400 / bpm_x10 muestras, de 1 a 8
export function compases(len, bpmX10, rate) {
  if (bpmX10 < 400 || bpmX10 > 2400 || !len) return 0;
  const bar = Math.floor(rate * 2400 / bpmX10);
  return Math.max(1, Math.min(8, Math.floor((len + bar / 2) / bar)));
}

// el recorte a 8 bits con ruido triangular (dither), normalizado al 95 % del pico, como toma.py
export function a8bits(x) {
  let pico = 1e-6;
  for (let i = 0; i < x.length; i++) pico = Math.max(pico, Math.abs(x[i]));
  const g = 0.95 * 127 / pico, out = new Uint8Array(x.length);
  let s = 1;
  const rnd = () => { s = (s * 1103515245 + 12345) >>> 0; return s / 4294967296; };
  for (let i = 0; i < x.length; i++) out[i] = Math.max(-128, Math.min(127, Math.round(x[i] * g + rnd() - rnd()))) & 255;
  return out;
}

// suena tal cual lo va a tocar el FM-1: los bytes de 8 bits a su frecuencia
let sonando = null;
export function escuchar(bytes, rate, alTerminar) {
  parar();
  const ctx = new (window.AudioContext || window.webkitAudioContext)();
  const b = ctx.createBuffer(1, Math.max(1, bytes.length), rate), d = b.getChannelData(0);
  for (let i = 0; i < bytes.length; i++) d[i] = ((bytes[i] << 24) >> 24) / 128;
  const s = ctx.createBufferSource();
  s.buffer = b; s.connect(ctx.destination); s.start();
  sonando = { ctx, s };
  s.onended = () => { if (sonando && sonando.s === s) { sonando = null; ctx.close(); if (alTerminar) alTerminar(); } };
}
export function parar() {
  if (!sonando) return;
  const { ctx, s } = sonando;
  sonando = null;
  try { s.stop(); } catch (_) { /* ya terminó */ }
  ctx.close();
}

// La onda con el recorte: dos bordes que se arrastran (puntero) o se mueven con las flechas (teclado).
// onda.set(x, rate, a, b) · onda.cambio = (a, b) => … · a y b en muestras
export class Onda {
  constructor(canvas) {
    this.c = canvas; this.x = null; this.a = 0; this.b = 0; this.rate = 16000; this.drag = null; this.cambio = null;
    canvas.addEventListener("pointerdown", (e) => this.down(e));
    canvas.addEventListener("pointermove", (e) => { if (this.drag) this.move(e); });
    canvas.addEventListener("pointerup", () => { this.drag = null; });
    canvas.addEventListener("pointercancel", () => { this.drag = null; });
    canvas.addEventListener("keydown", (e) => this.key(e));
    new ResizeObserver(() => this.draw()).observe(canvas);
  }
  set(x, rate, a, b) { this.x = x; this.rate = rate; this.a = a; this.b = b; this.draw(); }
  pos(e) {
    const r = this.c.getBoundingClientRect();
    return Math.round(Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)) * this.x.length);
  }
  down(e) {
    if (!this.x) return;
    const p = this.pos(e);
    this.drag = Math.abs(p - this.a) <= Math.abs(p - this.b) ? "a" : "b";
    this.c.setPointerCapture(e.pointerId);
    this.move(e);
  }
  move(e) { this.mover(this.drag, this.pos(e)); }
  mover(which, p) {
    const min = Math.round(this.rate / 20);
    if (which === "a") this.a = Math.max(0, Math.min(p, this.b - min));
    else this.b = Math.min(this.x.length, Math.max(p, this.a + min));
    this.draw();
    if (this.cambio) this.cambio(this.a, this.b);
  }
  key(e) {
    if (!this.x || !["ArrowLeft", "ArrowRight"].includes(e.key)) return;
    e.preventDefault();
    const d = (e.key === "ArrowLeft" ? -1 : 1) * Math.round(this.rate * (e.altKey ? 0.001 : 0.01));
    this.mover(e.shiftKey ? "b" : "a", (e.shiftKey ? this.b : this.a) + d);
  }
  draw() {
    const c = this.c, w = c.clientWidth, h = c.clientHeight, dpr = window.devicePixelRatio || 1;
    if (!w || !h) return;
    c.width = Math.round(w * dpr); c.height = Math.round(h * dpr);
    const g = c.getContext("2d");
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, w, h);
    if (!this.x) return;
    const css = getComputedStyle(c), n = this.x.length, ax = this.a / n * w, bx = this.b / n * w;
    g.fillStyle = css.getPropertyValue("--sel") || "#2a2410";
    g.fillRect(ax, 0, bx - ax, h);
    for (let px = 0; px < w; px++) {
      const i0 = Math.floor(px / w * n), i1 = Math.max(i0 + 1, Math.floor((px + 1) / w * n));
      let lo = 1, hi = -1;
      for (let i = i0; i < i1; i++) { const v = this.x[i]; if (v < lo) lo = v; if (v > hi) hi = v; }
      g.fillStyle = px >= ax && px <= bx ? (css.getPropertyValue("--wave") || "#ffc618") : (css.getPropertyValue("--wave-off") || "#555");
      g.fillRect(px, h / 2 - hi * h / 2, 1, Math.max(1, (hi - lo) * h / 2));
    }
    g.fillStyle = css.getPropertyValue("--edge") || "#fff";
    g.fillRect(ax - 1, 0, 2, h); g.fillRect(bx - 1, 0, 2, h);
    g.fillRect(ax - 1, 0, 8, 6); g.fillRect(bx - 7, h - 6, 8, 6);
  }
}
