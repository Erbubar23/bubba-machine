// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// Plataforma de control (control.html): administra el FM-1 con Bubba Machine por la consola USB-MIDI
// (fm1_consola.js, hal/fm1/fm1_consola.c). Todo cambio queda pendiente: la barra de arriba muestra
// cómo queda la memoria compartida de tomas y sonidos, y "Mandar al FM-1" escribe la lista en orden
// (con el transporte parado), lee de nuevo y marca lo comprobado. Sin el FM-1 se puede preparar todo.
//   TAKES: audio -> mono, a la frecuencia del aparato, 8 bits con dither (control_audio.js)
//   SOUNDS: .vital, .syx y .bsnap (bsnap.js) · SCREEN LANGUAGE: .bmlang · DEVICE: estado y registro
// Con ?mock=1 habla con un FM-1 de mentira (control_mock.js), para desarrollar sin el aparato.
import { pedir, enviar, puertos } from "./fm1_consola.js?v=3";
import { vitalToBsnap, syxToBsnap, readBsnap, renameBsnap } from "./bsnap.js?v=2";
import { decodificar, bpmDelNombre, adivinarBpm, compases, a8bits, escuchar, parar, Onda } from "./control_audio.js?v=1";
import { EN, TX } from "./control_i18n.js?v=1";

const $ = (id) => document.getElementById(id);
const Q = new URLSearchParams(location.search);
const MOCK = Q.get("mock") === "1";
const ENG = { 3: "B-MINI", 4: "BX-7", 5: "BITTAL", 6: "BUNO-6" };
const COLS = ["B-MINI", "BITTAL", "BUNO-6", "BX-7"];
const VITAL = ["B-MINI", "BITTAL", "BUNO-6"];
const SECTOR = 4096, TAKE_HDR = 128, NTAKES = 8;
const DEF = { rate: 16000, recMax: 49152, takesCap: 96000, zone: 102400, maxSnd: 12, langCap: 16384 };   /* core/bm_config.h */
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const takeSect = (len) => Math.ceil((TAKE_HDR + len) / SECTOR);        /* bm_takes.c sectors_for */
const sndSect = (size) => Math.ceil((size + 4) / SECTOR);              /* bm_snap.c sectors_for (+ CRC) */
const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const kb = (n) => Math.max(0, Math.round(n / 1024)) + " KB";
const sec = (n, rate) => (n / rate).toFixed(2) + " s";
const le32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
const le16 = (b, o) => b[o] | b[o + 1] << 8;
const str = (b, o, n) => String.fromCharCode(...b.slice(o, o + n)).replace(/\0.*$/s, "").trim();
const fallo = (msg) => Object.assign(new Error(msg), { code: "msg" });

/* ---------------------------------------------------------------- idioma de la página */
let lang = Q.get("lang");
if (lang !== "en" && lang !== "es") {
  try { lang = localStorage.getItem("bm-lang"); } catch (_) { lang = null; }
  if (lang !== "en" && lang !== "es") lang = (navigator.language || "es").toLowerCase().startsWith("es") ? "es" : "en";
}
const T = () => TX[lang];

function applyLang() {
  document.documentElement.lang = lang;
  document.querySelectorAll("[data-t]").forEach((e) => {
    if (e.dataset.es === undefined) e.dataset.es = e.innerHTML;
    e.innerHTML = lang === "en" && EN[e.dataset.t] != null ? EN[e.dataset.t] : e.dataset.es;
  });
  document.querySelectorAll("[data-t-aria]").forEach((e) => e.setAttribute("aria-label", T().aria[e.dataset.tAria]));
  document.querySelectorAll("[data-en]").forEach((a) => {
    if (a.dataset.hrefEs === undefined) a.dataset.hrefEs = a.getAttribute("href");
    a.setAttribute("href", lang === "en" ? a.dataset.en : a.dataset.hrefEs);
  });
  $("langsw").textContent = lang === "en" ? "Español" : "English";
  $("langsw").setAttribute("lang", lang === "en" ? "es" : "en");
  document.title = "Bubba Machine · " + T().title;
  render();
}
$("langsw").addEventListener("click", (e) => {
  e.preventDefault();
  lang = lang === "en" ? "es" : "en";
  try { localStorage.setItem("bm-lang", lang); } catch (_) { /* sin almacenamiento */ }
  const q = new URLSearchParams(location.search);
  q.set("lang", lang);
  history.replaceState(null, "", "?" + q + location.hash);
  applyLang();
});

/* ---------------------------------------------------------------- estado */
let acc = null, dev = null, estado = "off", lateBase = null, timer = null, leyendo = false, firma = "";
let pend = [], hechos = [], nid = 1, sending = false, esperaStop = false, cancelar = false, aviso = null, seguro = false;
const ver = { takes: new Set(), snds: new Set(), lang: false };
let ultima = null;

/* ---------------------------------------------------------------- conexión */
async function informe() {
  const r = await pedir(acc, 0x23, [0, 0], 1500);
  if (r[1] !== 1 || r.length < 2 + 8 + 72) throw Object.assign(new Error("report"), { code: "old" });
  const b = r.slice(2), o = 8;
  return {
    heap: le32(b, 0), playing: b[o + 1], cpu: b[o + 2], dual: b[o + 3], nTakes: b[o + 4], nSnd: b[o + 5], maxSnd: b[o + 6] || DEF.maxSnd,
    langOn: b[o + 7], takesLen: le32(b, o + 8), takesCap: le32(b, o + 12) || DEF.takesCap, room: le32(b, o + 20),
    zone: le32(b, o + 24) || DEF.zone, zoneFree: le32(b, o + 28), sndBytes: le32(b, o + 32), rate: le16(b, o + 36) || DEF.rate,
    recMax: le32(b, o + 38) || DEF.recMax, langName: str(b, o + 42, 12), langSize: le16(b, o + 54), langCap: le16(b, o + 56) || DEF.langCap,
    version: str(b, o + 58, 14),
  };
}

async function stats() {                       /* tools/fm1/consola.py CAMPOS */
  const r = await pedir(acc, 0x01, [], 1500);
  const n = r.length >> 2, w = (i) => (i < n ? le32(r, i * 4) : null);
  return { uptime: w(1), late: w(5), avg: w(8), budget: w(9), rst: w(35), railMin: w(37), vbat: w(39), c1: w(47) };
}

async function tomaEn(i) {
  const r = await pedir(acc, 0x23, [1, i]), b = r.slice(2);
  return r[1] === 1 && b[0] ? { slot: i, len: le32(b, 2), bpm: le16(b, 6) / 10, bars: b[8], pads: b[10] } : null;
}
async function leerTomas() { const t = []; for (let i = 0; i < NTAKES; i++) t.push(await tomaEn(i)); return t; }
async function leerSonidos(n) {
  const s = [];
  for (let i = 0; i < n; i++) {
    const r = await pedir(acc, 0x23, [2, i]), b = r.slice(2);
    if (r[1] === 1) s.push({ idx: i, eng: b[0], sect: b[2], size: le16(b, 4), fp: le16(b, 6), name: str(b, 8, 12), author: str(b, 20, 12) });
  }
  return s;
}

function perder(s) { dev = null; estado = s; lateBase = null; firma = ""; }

async function leer(full) {
  if (!acc || leyendo || sending) return;
  leyendo = true;
  try {
    const { out, inp } = puertos(acc);
    if (!out || !inp) { perder("noport"); return; }
    let e;
    try { e = await informe(); } catch (err) {
      if (err.code === "noport") { perder("noport"); return; }
      let viejo = false;                       /* contesta el estado (01) pero no el informe (23): anterior a 0.6.1 */
      try { await pedir(acc, 0x01, [], 1200); viejo = true; } catch (_) { /* nada */ }
      perder(viejo ? "old" : "noreply");
      return;
    }
    try { e.st = await stats(); } catch (_) { e.st = null; }
    const f = [e.nTakes, e.nSnd, e.takesLen, e.sndBytes, e.zoneFree].join(",");
    if (full || !dev || f !== firma) { e.takes = await leerTomas(); e.snds = await leerSonidos(e.nSnd); firma = f; }
    else { e.takes = dev.takes; e.snds = dev.snds; }
    if (lateBase === null && e.st && e.st.late != null) lateBase = e.st.late;
    dev = e; estado = "on";
  } catch (err) {
    perder(err.code === "noport" ? "noport" : "noreply");
  } finally { leyendo = false; render(); }
}

async function conectar() {
  if (acc) return;
  if (MOCK) {
    const m = await import("./control_mock.js");
    acc = m.mockAccess();
  } else {
    if (!navigator.requestMIDIAccess) { estado = "nomidi"; render(); return; }
    estado = "connecting"; render();
    try { acc = await navigator.requestMIDIAccess({ sysex: true }); } catch (_) { estado = "deny"; render(); return; }
    acc.onstatechange = () => setTimeout(() => leer(true), 600);
  }
  estado = "connecting"; render();
  await leer(true);
  clearInterval(timer);
  timer = setInterval(() => { if (!document.hidden) leer(false); }, 2000);
}
$("conectar").addEventListener("click", conectar);

/* la última versión publicada: la primera que no es el diagnóstico (fm1/versiones.json, la del instalador) */
const verDe = (s) => (/(\d+)\.(\d+)\.(\d+)(-[\w.]+)?/.exec(s || "") || []);
function cmpVer(a, b) {
  const x = verDe(a), y = verDe(b);
  if (!x[0] || !y[0]) return 0;
  for (let i = 1; i <= 3; i++) if (+x[i] !== +y[i]) return +x[i] - +y[i];
  return (x[4] ? 0 : 1) - (y[4] ? 0 : 1);
}
fetch("fm1/versiones.json", { cache: "no-cache" }).then((r) => r.json()).then((l) => {
  const e = l.find((v) => !/diag/i.test(v.archivo || ""));
  if (e) { ultima = verDe(e.archivo)[0] || null; render(); }
}).catch(() => {});

/* ---------------------------------------------------------------- lo pendiente */
function agregar(it) { it.id = nid++; it.st = "pend"; it.err = ""; pend.push(it); hechos = []; aviso = null; seguro = false; render(); }
function quitar(id) {
  const it = pend.find((x) => x.id === id);
  if (!it || it.st === "run") return;
  pend = pend.filter((x) => x.id !== id && x.link !== id);
  aviso = null; seguro = false;
  render();
}

// el orden en que se manda: primero los borrados (solo hacen lugar), después lo demás como se agregó
const orden = (l) => l.filter((x) => x.k === "tdel" || x.k === "sdel").concat(l.filter((x) => x.k !== "tdel" && x.k !== "sdel"));

// lo que queda después de mandar, paso a paso como lo hará el FM-1: la toma va al primer lugar libre,
// las tomas suman hasta 6 s, el usuario tiene hasta 12 sonidos y todo cabe en los sectores de la zona
function plan(extra) {
  const D = dev, lista = orden(extra ? pend.concat(extra) : pend);
  const total = Math.floor((D ? D.zone : DEF.zone) / SECTOR), cap = D ? D.takesCap : DEF.takesCap, maxSnd = D ? D.maxSnd : DEF.maxSnd;
  const valid = D ? D.takes.map(Boolean) : new Array(NTAKES).fill(false);
  const sn0 = D ? Math.round(D.sndBytes / SECTOR) : 0, tk0 = D ? Math.round((D.zone - D.zoneFree) / SECTOR) - sn0 : 0;
  let used = tk0 + sn0, tlen = D ? D.takesLen : 0, nUser = D ? D.nSnd : 0, frTk = 0, frSn = 0, added = 0;
  const bad = new Map(), slotOf = new Map(), t = T();
  for (const it of lista) {
    if (it.k === "tdel" && valid[it.slot]) { valid[it.slot] = false; used -= takeSect(it.len); frTk += takeSect(it.len); tlen -= it.len; }
    else if (it.k === "sdel") { used -= it.sect; frSn += it.sect; nUser--; }
    else if (it.k === "tadd") {
      const s = valid.indexOf(false), need = takeSect(it.len);
      if (s < 0) bad.set(it.id, t.noSlot);
      else if (tlen + it.len > cap) bad.set(it.id, t.noSecs(sec(Math.max(0, cap - tlen), D ? D.rate : DEF.rate)));
      else if (used + need > total) bad.set(it.id, t.noMem(kb(need * SECTOR), kb((total - used) * SECTOR)));
      else { valid[s] = true; slotOf.set(it.id, s); used += need; added += need; tlen += it.len; }
    } else if (it.k === "sadd") {
      const need = sndSect(it.size);
      if (nUser >= maxSnd) bad.set(it.id, t.noUser(maxSnd));
      else if (used + need > total) bad.set(it.id, t.noMem(kb(need * SECTOR), kb((total - used) * SECTOR)));
      else { used += need; added += need; nUser++; }
    } else if (it.k === "lang" && it.bytes && it.bytes.length > (D ? D.langCap : DEF.langCap)) bad.set(it.id, t.langBig);
  }
  const rate = D ? D.rate : DEF.rate, freeB = (total - used) * SECTOR;
  return { total, tk: tk0 - frTk, sn: sn0 - frSn, fr: frTk + frSn, added, used, tlen, cap, nUser, maxSnd, bad, slotOf, rate, freeB,
    freeS: Math.max(0, Math.min(freeB - TAKE_HDR, cap - tlen)) / rate };
}

/* ---------------------------------------------------------------- mandar */
const hasta = async (fn, ms, msg) => { const t0 = Date.now(); while (Date.now() - t0 < ms) { if (await fn()) return; await sleep(400); } throw fallo(msg); };

// paso 0 rechazado = el FM-1 está ocupado escribiendo la flash (o sonando): se espera y se repite
async function conReintento(fn) {
  for (let k = 0; k < 16; k++) {
    try { return await fn(); } catch (e) {
      if (e.code !== "rejected" || e.step !== 0) throw e;
      if ((await informe()).playing) throw fallo(T().fPlaying);
      await sleep(500);
    }
  }
  throw fallo(T().fBusy);
}
const cuenta = (l, fp) => l.filter((s) => s.fp === fp).length;

async function ejecutar(it, av) {
  const t = T();
  if (it.k === "tdel") {
    if (!(await tomaEn(it.slot))) return;
    const r = await pedir(acc, 0x23, [3, it.slot]);
    if (r[1] !== 1) throw fallo(t.fRefused);
    await hasta(async () => !(await tomaEn(it.slot)), 15000, t.fNotDone);
  } else if (it.k === "tadd") {
    const e = await informe();
    if (e.rate !== it.rate) throw fallo(t.fRate);
    if (e.room < it.len) throw fallo(e.nTakes >= NTAKES ? t.noSlot : t.fRoom);
    const antes = await leerTomas();
    await enviar(acc, 0x20, it.bytes, av, [it.len, it.bpm10]);
    let slot = -1;
    await hasta(async () => {
      const ahora = await leerTomas();
      slot = ahora.findIndex((x, k) => x && !antes[k] && Math.abs(x.len - it.len) <= 1);
      return slot >= 0;
    }, 20000, t.fNotSaved);
    it.done = slot; ver.takes.add(slot);
  } else if (it.k === "sdel") {
    const l0 = await leerSonidos((await informe()).nSnd), s = l0.find((x) => x.fp === it.fp && x.eng === it.eng);
    if (!s) return;
    await conReintento(async () => {
      const r = await pedir(acc, 0x22, [4, 0, 0, 0, s.idx, 0, 0]);
      if (r[1] !== 1) throw Object.assign(new Error("refused"), { code: "rejected", step: 0 });
    });
    const l1 = await leerSonidos((await informe()).nSnd);
    if (cuenta(l1, it.fp) >= cuenta(l0, it.fp)) throw fallo(t.fNotDone);
  } else if (it.k === "sadd") {
    const l0 = await leerSonidos((await informe()).nSnd);
    try { await conReintento(() => enviar(acc, 0x22, it.bytes, av)); } catch (e) {
      if (e.code === "rejected") throw fallo(t.fSndRefused);
      throw e;
    }
    const l1 = await leerSonidos((await informe()).nSnd);
    if (cuenta(l1, it.fp) <= cuenta(l0, it.fp)) throw fallo(t.fNotSaved);
    ver.snds.add(it.fp);
  } else if (it.k === "lang") {
    if (it.mode === "off") {
      await conReintento(async () => {
        const r = await pedir(acc, 0x21, [3, 0, 0, 0, 0, 0, 0]);
        if (r[1] !== 1) throw Object.assign(new Error("refused"), { code: "rejected", step: 0 });
      });
    } else {
      try { await conReintento(() => enviar(acc, 0x21, it.bytes, av)); } catch (e) {
        if (e.code === "rejected") throw fallo(t.fLangRefused);
        throw e;
      }
    }
    const e = await informe();
    if (it.mode === "off" ? e.langOn : !e.langOn || e.langName !== it.name) throw fallo(t.fNotDone);
    ver.lang = true;
  }
}

function porque(e) {
  const t = T();
  if (e.code === "timeout") return t.fTimeout;
  if (e.code === "noport") return t.fGone;
  if (e.code === "rejected") return t.fRefused;
  return e.message || String(e);
}

async function mandar() {
  if (sending || !pend.length || !dev || plan().bad.size) return;
  sending = true; cancelar = false; aviso = null; seguro = false;
  ver.takes.clear(); ver.snds.clear(); ver.lang = false; hechos = [];
  while (leyendo) await sleep(50);
  const prog = $("prog"), lista = orden(pend);
  prog.value = 0; prog.hidden = false;
  try {
    let e = await informe();
    while (e.playing) {                         /* escribir la flash corta el audio: solo parado */
      esperaStop = true; render();
      await sleep(1000);
      if (cancelar) return;
      e = await informe();
    }
    esperaStop = false;
    for (let i = 0; i < lista.length; i++) {
      const it = lista[i];
      it.st = "run"; it.err = ""; render();
      const av = (f) => { prog.value = (i + f) / lista.length; };
      av(0);
      try { await ejecutar(it, av); it.st = "ok"; } catch (err) {
        it.st = "fail"; it.err = porque(err);
        if (err.code === "noport" || err.code === "timeout") break;      /* se fue el FM-1: lo demás queda pendiente */
      }
      render();
    }
  } catch (_) {
    /* se perdió la conexión: lo que no se mandó queda pendiente */
  } finally {
    for (const it of lista) if (it.st === "run") { it.st = "fail"; it.err = it.err || T().fGone; }
    esperaStop = false; sending = false; prog.hidden = true;
    const ok = lista.filter((x) => x.st === "ok"), mal = lista.filter((x) => x.st === "fail");
    hechos = ok; pend = pend.filter((x) => x.st !== "ok");
    if (!cancelar) aviso = mal.length ? ["bad", T().sentBad(ok.length, mal.length)] : ["ok", T().sentOk(ok.length)];
    cancelar = false;
    await leer(true);
  }
}
$("send").addEventListener("click", mandar);
$("discard").addEventListener("click", () => {
  if (esperaStop) { cancelar = true; return; }
  if (!seguro) { seguro = true; render(); return; }
  pend = pend.filter((x) => x.st === "run"); seguro = false; aviso = null; render();
});
$("keep").addEventListener("click", () => { seguro = false; render(); });
window.addEventListener("beforeunload", (e) => { if (pend.length) { e.preventDefault(); e.returnValue = ""; } });

/* ---------------------------------------------------------------- dibujo */
const html = (el, s) => { if (el.dataset.html !== s) { el.innerHTML = s; el.dataset.html = s; } };
const badge = (cls, txt) => `<span class="badge ${cls}">${esc(txt)}</span>`;
function estadoItem(it, P) {
  const t = T();
  if (it.st === "run") return badge("run", t.bRun);
  if (it.st === "fail") return badge("bad", t.bFail) + `<div class="err">${esc(it.err)}</div>`;
  if (P.bad.has(it.id)) return badge("bad", t.bNoFit) + `<div class="err">${esc(P.bad.get(it.id))}</div>`;
  return "";
}

function render() {
  const P = plan();
  renderBar(P); renderTakes(P); renderSounds(P); renderLang(P); renderDevice();
  if (tCur) notaToma();
  if (sCur) notaSonido();
}

function renderBar(P) {
  const t = T(), dot = $("dot");
  const st = { off: [t.stOff, t.stOffH, ""], connecting: [t.stConn, "", "wait"], nomidi: [t.stNoMidi, t.stNoMidiH, "bad"], deny: [t.stDeny, t.stDenyH, "bad"],
    noport: [t.stNoPort, t.stNoPortH, "bad"], noreply: [t.stNoReply, t.stNoReplyH, "bad"], old: [t.stOld, t.stOldH, "bad"] }[estado];
  let s;
  if (estado === "on") { s = [t.stOn(dev.version), dev.playing ? t.stPlaying : t.stStopped, "on"]; }
  else s = st || [t.stOff, "", ""];
  dot.className = "dot " + s[2];
  html($("estado"), esc(s[0]) + (s[1] ? `<small>${s[1]}</small>` : ""));
  $("conectar").hidden = !!acc && estado !== "deny";
  if (estado === "deny") $("conectar").textContent = t.bRetry;
  const nueva = $("nueva"), mostrar = dev && ultima && cmpVer(ultima, dev.version) > 0;
  nueva.hidden = !mostrar;
  if (mostrar) { nueva.textContent = t.newer(ultima); nueva.href = "instalar.html" + (lang === "en" ? "?lang=en" : ""); }

  /* la memoria compartida de tomas y sonidos */
  const w = (n) => (Math.max(0, n) / P.total * 100).toFixed(2) + "%";
  const seg = $("meter").children;
  seg[0].style.width = w(P.tk); seg[1].style.width = w(P.sn); seg[2].style.width = w(P.fr); seg[3].style.width = w(P.added);
  const key = (c) => `<span class="key" style="background:${c}"></span>`;
  let m;
  if (dev) {
    const g = (x) => `<span class="g">${x}</span> `;
    m = g(`${key("var(--tk)")}${t.mTakes} <b>${(P.tlen / P.rate).toFixed(1)} / ${(P.cap / P.rate).toFixed(1)} s</b>`) +
      g(`${key("var(--sn)")}${t.mSounds} <b>${P.nUser} / ${P.maxSnd}</b>`) +
      (P.added || P.fr ? g(`${key("var(--pd)")}${t.mPend} <b>${P.added ? "+" + kb(P.added * SECTOR) : ""}${P.added && P.fr ? " " : ""}${P.fr ? "−" + kb(P.fr * SECTOR) : ""}</b>`) : "") +
      `<br>${g(`${pend.length ? t.mFreeAfter : t.mFree} <b>${kb(P.freeB)}</b> ${t.mOf(kb(P.total * SECTOR))}`)}${g("· " + t.mTakeS(P.freeS.toFixed(1)))}`;
  } else {
    m = pend.length ? `${key("var(--pd)")}${t.mOffPend(kb(P.added * SECTOR), kb(P.total * SECTOR))}` : t.mOff;
  }
  html($("mtxt"), m);
  $("meter").setAttribute("aria-label", $("mtxt").textContent);

  /* lo pendiente */
  const n = pend.length;
  $("pcount").textContent = esperaStop ? t.waitStop : sending ? t.sending : n ? t.nPend(n) : t.noPend;
  const send = $("send"), dis = $("discard");
  send.disabled = sending || !n || !dev || P.bad.size > 0;
  send.textContent = sending ? t.bSending : t.bSend;
  dis.disabled = (!n || sending) && !esperaStop;
  dis.hidden = !n && !esperaStop;
  send.parentElement.classList.toggle("none", !n && !sending);
  dis.textContent = esperaStop ? t.bStopWait : seguro ? t.bSure(n) : t.bDiscard;
  dis.classList.toggle("del", seguro);
  $("keep").hidden = !seguro;
  let why = "", cls = "warn";
  if (esperaStop) why = t.pressStop;
  else if (sending) why = "";
  else if (n && !dev) why = t.whyConnect;
  else if (n && P.bad.size) { why = t.whyBad; cls = "bad"; }
  else if (n && dev && dev.playing) why = t.whyPlaying;
  else if (aviso) { why = aviso[1]; cls = aviso[0]; }
  $("why").textContent = why; $("why").className = "why " + cls;
  const items = orden(pend).concat(hechos.filter((x) => !pend.includes(x)));
  $("plist").hidden = !items.length;
  html($("pitems"), items.map((it) => `<li>${esc(describir(it, P))} ` +
    (it.st !== "ok" && it.st !== "run" && !sending ? `<button type="button" class="small" data-undo="${it.id}">${t.bUndo}</button> ` : "") +
    `${it.st === "ok" ? badge("ok", t.bOk) : estadoItem(it, P)}</li>`).join(""));
}

function describir(it, P) {
  const t = T(), slot = (s) => "TAKE " + String(s + 1).padStart(2, "0");
  if (it.k === "tadd") {
    const s = it.st === "ok" ? it.done : P.slotOf.get(it.id);
    return (s != null && s >= 0 ? slot(s) + ": " : "") + t.dTadd(sec(it.len, it.rate), it.file);
  }
  if (it.k === "tdel") return slot(it.slot) + ": " + t.dTdel;
  if (it.k === "sadd") return ENG[it.eng] + ": " + t.dSadd(it.name);
  if (it.k === "sdel") return ENG[it.eng] + ": " + t.dSdel(it.name);
  return t.dLang(it.mode === "off" ? "English" : it.mode === "es" ? "Español" : it.name);
}

function renderTakes(P) {
  const t = T(), rate = P.rate;
  let h = "";
  for (let s = 0; s < NTAKES; s++) {
    const tk = dev ? dev.takes[s] : null;
    const del = pend.find((x) => x.k === "tdel" && x.slot === s);
    const add = pend.find((x) => x.k === "tadd" && P.slotOf.get(x.id) === s);
    const n = `<div class="n">TAKE ${String(s + 1).padStart(2, "0")}</div>`;
    const bars = (b, bpm) => (b ? t.bars(b) + " · " : "") + (bpm ? bpm + " BPM" : "");
    if (add) {
      h += `<div class="slot new" data-slot="${s}">${n}<div class="v">${sec(add.len, add.rate)}</div><div class="m">${esc(bars(add.bars, add.bpm10 / 10))}</div>` +
        `<div class="m">${esc(add.file)}</div>${add.st === "pend" ? badge("p", del && tk ? t.bReplace : t.bNew) : ""}${del && tk ? estadoItem(del, P) : ""}${estadoItem(add, P)}` +
        `<div class="acts">${add.st !== "run" && !sending ? `<button type="button" class="small" data-undo="${add.id}">${t.bUndo}</button>` : ""}</div></div>`;
    } else if (del && tk) {
      h += `<div class="slot gone" data-slot="${s}">${n}<div class="v">${sec(tk.len, rate)}</div><div class="m">${esc(bars(tk.bars, tk.bpm))}</div>` +
        `${del.st === "pend" ? badge("p", t.bDel) : ""}${estadoItem(del, P)}<div class="acts">${del.st !== "run" && !sending ? `<button type="button" class="small" data-undo="${del.id}">${t.bUndo}</button>` : ""}</div></div>`;
    } else if (tk) {
      h += `<div class="slot" data-slot="${s}">${n}<div class="v">${sec(tk.len, rate)}</div><div class="m">${esc(bars(tk.bars, tk.bpm))}</div>` +
        `<div class="m">${tk.pads ? esc(t.pads(tk.pads)) : ""}</div>${ver.takes.has(s) ? badge("ok", t.bOk) : ""}` +
        `<div class="acts"><button type="button" class="small" data-pick="${s}" aria-label="${esc(t.aReplace(s + 1))}">${t.bReplaceB}</button>` +
        `<button type="button" class="small del" data-tdel="${s}" aria-label="${esc(t.aDel("TAKE " + (s + 1)))}">${t.bDelete}</button></div></div>`;
    } else {
      h += `<div class="slot empty" data-slot="${s}">${n}<div class="v">${dev ? t.empty : "—"}</div>` +
        `<div class="acts"><button type="button" class="small" data-pick="${s}" aria-label="${esc(t.aAdd(s + 1))}">${t.bAddAudio}</button></div></div>`;
    }
  }
  const sinLugar = pend.filter((x) => x.k === "tadd" && !P.slotOf.has(x.id));
  h += sinLugar.map((x) => `<div class="slot new" style="grid-column:1/-1;min-height:0"><div class="n">${t.noPlace}</div><div class="m">${esc(x.file)} · ${sec(x.len, x.rate)}</div>${estadoItem(x, P)}` +
    `<div class="acts"><button type="button" class="small" data-undo="${x.id}">${t.bUndo}</button></div></div>`).join("");
  if (!dev) h += `<p class="mut" style="grid-column:1/-1;margin:0;font-size:14px">${t.takesOff}</p>`;
  html($("slots"), h);
}

function renderSounds(P) {
  const t = T();
  $("scount").textContent = dev ? t.sCount(P.nUser, P.maxSnd, dev.nSnd !== P.nUser) : t.sCountOff(P.maxSnd);
  let h = "";
  for (const m of COLS) {
    const eng = { "B-MINI": 3, "BX-7": 4, "BITTAL": 5, "BUNO-6": 6 }[m];
    const mine = dev ? dev.snds.filter((s) => s.eng === eng) : [];
    const adds = pend.filter((x) => x.k === "sadd" && x.eng === eng);
    let li = mine.map((s) => {
      const del = pend.find((x) => x.k === "sdel" && x.fp === s.fp && x.eng === eng);
      return `<li class="${del ? "gone" : ""}"><span class="nm">${esc(s.name || "?")}</span><span class="sz">${esc(s.author || t.yours)} · ${kb(s.sect * SECTOR)}</span>` +
        (del ? (del.st === "pend" ? badge("p", t.bDel) : "") + estadoItem(del, P) + (del.st !== "run" && !sending ? `<button type="button" class="small" data-undo="${del.id}">${t.bUndo}</button>` : "")
          : (ver.snds.has(s.fp) ? badge("ok", t.bOk) : "") + `<button type="button" class="small del" data-sdel="${s.idx}" aria-label="${esc(t.aDel(s.name))}">${t.bDelete}</button>`) + "</li>";
    }).join("");
    li += adds.map((x) => `<li class="new"><span class="nm">${esc(x.name)}</span><span class="sz">${kb(sndSect(x.size) * SECTOR)}</span>` +
      `${x.st === "pend" ? badge("p", t.bNew) : ""}${estadoItem(x, P)}${x.st !== "run" && !sending ? `<button type="button" class="small" data-undo="${x.id}">${t.bUndo}</button>` : ""}</li>`).join("");
    h += `<div class="mach" data-mach="${m}"><h3>${m}</h3><div class="k">${t.kind[m]}</div>` +
      `<ul>${li || `<li class="none" style="background:none;border:0;padding:0">${dev ? t.noneYet : t.noneOff}</li>`}</ul>` +
      `<button type="button" class="small add" data-spick="${m}">${t.bAddSound}</button></div>`;
  }
  h += `<div class="mach factory"><h3>BB-303</h3><div class="k">${t.kind["BB-303"]}</div><p class="none" style="margin:0">${t.factoryOnly}</p></div>`;
  html($("machines"), h);
}

function langActual() {
  if (!dev) return null;
  if (!dev.langOn) return "off";
  return /^ESPA/i.test(dev.langName) ? "es" : "file";
}
function renderLang(P) {
  const t = T(), cur = langActual(), it = pend.find((x) => x.k === "lang");
  const sel = it ? it.mode : cur;
  for (const r of document.querySelectorAll("input[name=lang]")) r.checked = r.value === sel;
  for (const m of ["off", "es", "file"]) {
    let b = "";
    if (it && it.mode === m) b = it.st === "pend" ? badge("p", P.bad.has(it.id) ? t.bNoFit : t.bPend) : estadoItem(it, P);
    else if (cur === m) b = ver.lang ? badge("ok", t.bOk) : badge("ok", t.bOnFm1);
    html($("lb-" + m), b);
  }
  const fname = it && it.mode === "file" ? it.name : cur === "file" ? dev.langName : "";
  $("lfname").textContent = fname;
}

function renderDevice() {
  const t = T(), dl = $("dinfo");
  if (!dev) { html(dl, `<dt>${t.dState}</dt><dd>${t.dNone}</dd>`); $("dwarn").textContent = ""; $("dlog").disabled = !acc || estado !== "on"; return; }
  const s = dev.st || {}, rows = [];
  rows.push([t.dFw, "Bubba Machine " + esc(dev.version)]);
  const cpu = s.budget ? Math.round(s.avg * 100 / s.budget) : dev.cpu;
  const late = s.late != null && lateBase != null ? s.late - lateBase : 0;
  const carga = cpu >= 85 ? ["bad", t.cpuBad(cpu)] : cpu >= 70 ? ["warn", t.cpuHigh(cpu)] : ["ok", t.cpuOk(cpu)];
  rows.push([t.dCpu, `<span class="${carga[0]}">${carga[1]}</span>` + (late > 0 ? ` <span class="bad">${t.late(late)}</span>` : "")]);
  const hk = dev.heap / 1024;
  rows.push([t.dRam, dev.heap ? (hk < 24 ? `<span class="bad">${t.ramBad}</span>` : hk < 48 ? `<span class="warn">${t.ramLow}</span>` : `<span class="ok">${t.ramOk}</span>`) : "—"]);
  if (s.uptime != null) rows.push([t.dUp, t.uptime(s.uptime)]);
  if (s.rst != null) rows.push([t.dBoot, t.boot(s.rst & 0xFF)]);
  if (s.railMin) {
    const v = (mv) => (mv / 1000).toFixed(2) + " V", bat = s.vbat ? s.vbat & 0xFFFF : 0;
    rows.push([t.dVolt, t.volts(v(s.railMin & 0xFFFF), v(s.railMin >>> 16), bat ? v(bat) : "")]);
  }
  const c1 = s.c1 || 0;
  if (dev.dual || c1 & 7) rows.push(["TWO CORES", c1 & 1 ? `<span class="warn">${t.c1Retired}</span>` : c1 & 4 ? `<span class="warn">${t.c1Off}</span>` : t.c1On]);
  rows.push([t.dLangPack, dev.langOn ? `${esc(dev.langName)} · ${kb(dev.langSize)} / ${kb(dev.langCap)}` : t.langNone]);
  html(dl, rows.map(([a, b]) => `<dt>${a}</dt><dd>${b}</dd>`).join(""));
  $("dwarn").textContent = cpu >= 85 || late > 0 ? "· " + t.wAudio : hk < 24 && dev.heap ? "· " + t.wRam : "";
  $("dlog").disabled = false;
}

/* ---------------------------------------------------------------- clics en las listas */
let destino = null;                                 /* el lugar de TAKES sobre el que se soltó o se eligió */
document.addEventListener("click", (ev) => {
  const b = ev.target.closest("button");
  if (!b) return;
  if (b.dataset.undo) quitar(+b.dataset.undo);
  else if (b.dataset.tdel !== undefined) {
    const s = +b.dataset.tdel, tk = dev && dev.takes[s];
    if (tk && !pend.some((x) => x.k === "tdel" && x.slot === s)) agregar({ k: "tdel", slot: s, len: tk.len });
  } else if (b.dataset.sdel !== undefined) {
    const s = dev && dev.snds.find((x) => x.idx === +b.dataset.sdel);
    if (s && !pend.some((x) => x.k === "sdel" && x.fp === s.fp)) agregar({ k: "sdel", eng: s.eng, fp: s.fp, name: s.name, sect: s.sect });
  } else if (b.dataset.pick !== undefined) { destino = +b.dataset.pick; $("ftake").click(); }
  else if (b.dataset.spick) { sMaq = b.dataset.spick; $("fsnd").click(); }
});

/* soltar archivos: sobre un lugar de TAKES, sobre la sección, o sobre la columna de una máquina */
function zona(sec, fn) {
  const el = $(sec);
  el.addEventListener("dragover", (e) => {
    e.preventDefault();
    el.querySelectorAll(".on").forEach((x) => x.classList.remove("on"));
    const t = e.target.closest(".slot[data-slot], .mach[data-mach], .drop");
    (t || el.querySelector(".drop") || el).classList.add("on");
  });
  el.addEventListener("dragleave", (e) => { if (!el.contains(e.relatedTarget)) el.querySelectorAll(".on").forEach((x) => x.classList.remove("on")); });
  el.addEventListener("drop", (e) => {
    e.preventDefault();
    el.querySelectorAll(".on").forEach((x) => x.classList.remove("on"));
    const f = e.dataTransfer.files[0];
    if (f) fn(f, e.target);
  });
}
document.addEventListener("dragover", (e) => e.preventDefault());
document.addEventListener("drop", (e) => e.preventDefault());

/* ---------------------------------------------------------------- TAKES: el editor */
let tCur = null;
const onda = new Onda($("wave"));
zona("takes", (f, el) => { const s = el.closest(".slot[data-slot]"); abrirToma(f, s ? +s.dataset.slot : null); });
$("tpick").addEventListener("click", () => { destino = null; $("ftake").click(); });
$("ftake").addEventListener("change", () => { const f = $("ftake").files[0]; $("ftake").value = ""; if (f) abrirToma(f, destino); });

async function abrirToma(file, target) {
  const t = T(), rate = dev ? dev.rate : DEF.rate, recMax = dev ? dev.recMax : DEF.recMax;
  parar();
  $("ted").hidden = false; $("tname").textContent = file.name; $("tmeta").textContent = t.converting;
  $("tok").disabled = true; $("tplay").disabled = true;
  try {
    const x = await decodificar(file, rate);
    const P = plan(), cabe = Math.max(Math.round(rate / 10), Math.floor(P.freeS * rate));
    const b = Math.min(x.length, recMax, cabe);
    const deNombre = bpmDelNombre(file.name);
    tCur = { file, x, rate, recMax, a: 0, b, target, bpmSrc: deNombre ? "name" : "guess" };
    $("tbpm").value = deNombre || adivinarBpm(x.subarray(0, b), rate) || 120;
    $("tmeta").textContent = t.audioLen(sec(x.length, rate)) + (x.length > recMax ? " · " + t.maxPer(sec(recMax, rate)) : "");
    onda.set(x, rate, 0, b);
    sincToma();
    $("tok").disabled = false; $("tplay").disabled = false;
    $("ted").scrollIntoView({ block: "nearest", behavior: "smooth" });
    $("wave").focus({ preventScroll: true });
  } catch (e) {
    tCur = null;
    $("tmeta").innerHTML = `<span class="bad">${esc(t.cantRead(e.message || e))}</span>`;
  }
}

function sincToma() {
  if (!tCur) return;
  $("tfrom").value = (tCur.a / tCur.rate).toFixed(2);
  $("tlen").value = ((tCur.b - tCur.a) / tCur.rate).toFixed(2);
  $("tlen").max = (tCur.recMax / tCur.rate).toFixed(2);
  notaToma();
}
let guess = null;
onda.cambio = (a, b) => {
  if (!tCur) return;
  if (b - a > tCur.recMax) { if (a !== tCur.a) a = b - tCur.recMax; else b = a + tCur.recMax; onda.a = a; onda.b = b; onda.draw(); }
  tCur.a = a; tCur.b = b; sincToma();
  if (tCur.bpmSrc === "guess") { clearTimeout(guess); guess = setTimeout(() => { if (tCur) { $("tbpm").value = adivinarBpm(tCur.x.subarray(tCur.a, tCur.b), tCur.rate) || $("tbpm").value; notaToma(); } }, 300); }
};
function desdeCampos() {
  if (!tCur) return;
  const r = tCur.rate, a = Math.max(0, Math.min(tCur.x.length - Math.round(r / 20), Math.round(+$("tfrom").value * r) || 0));
  const n = Math.max(Math.round(r / 20), Math.min(tCur.recMax, tCur.x.length - a, Math.round(+$("tlen").value * r) || 0));
  tCur.a = a; tCur.b = a + n; onda.set(tCur.x, r, tCur.a, tCur.b); notaToma();
}
$("tfrom").addEventListener("change", () => { desdeCampos(); sincToma(); });
$("tlen").addEventListener("change", () => { desdeCampos(); sincToma(); });
$("tbpm").addEventListener("input", () => { if (tCur) { tCur.bpmSrc = "user"; notaToma(); } });

function bpmX10() { return Math.round(Math.max(40, Math.min(240, +$("tbpm").value || 120)) * 10); }
function notaToma() {
  if (!tCur) return;
  const t = T(), len = tCur.b - tCur.a, bars = compases(len, bpmX10(), tCur.rate);
  $("tbars").textContent = (bars ? t.barsAt(bars, bpmX10() / 10) : "") + (tCur.bpmSrc === "guess" ? " · " + t.guessed : tCur.bpmSrc === "name" ? " · " + t.fromName : "");
  const extra = [{ id: -1, k: "tadd", len }];
  const tk = tCur.target != null && dev ? dev.takes[tCur.target] : null;
  if (tk && !pend.some((x) => x.k === "tdel" && x.slot === tCur.target)) extra.unshift({ id: -2, k: "tdel", slot: tCur.target, len: tk.len });
  const P = plan(extra), bad = P.bad.get(-1);
  const rep = tk ? t.replaces(tCur.target + 1, sec(tk.len, tCur.rate)) + " " : "";
  $("tnote").className = "note " + (bad ? "bad" : "ok");
  $("tnote").textContent = rep + (bad || t.fitsTake("TAKE " + String(P.slotOf.get(-1) + 1).padStart(2, "0"), kb(P.freeB)));
  $("tok").disabled = sending;
}
$("tplay").addEventListener("click", () => {
  if (!tCur) return;
  if ($("tplay").dataset.on) { parar(); $("tplay").removeAttribute("data-on"); applyTplay(); return; }
  escuchar(a8bits(tCur.x.subarray(tCur.a, tCur.b)), tCur.rate, () => { $("tplay").removeAttribute("data-on"); applyTplay(); });
  $("tplay").dataset.on = "1"; applyTplay();
});
function applyTplay() { $("tplay").textContent = $("tplay").dataset.on ? T().bStop : lang === "en" ? EN["t.play"] : $("tplay").dataset.es; }
$("tok").addEventListener("click", () => {
  if (!tCur) return;
  const bytes = a8bits(tCur.x.subarray(tCur.a, tCur.b)), b10 = bpmX10();
  const it = { k: "tadd", bytes, len: bytes.length, bpm10: b10, bars: compases(bytes.length, b10, tCur.rate), rate: tCur.rate, file: tCur.file.name };
  const tk = tCur.target != null && dev ? dev.takes[tCur.target] : null;
  if (tk && !pend.some((x) => x.k === "tdel" && x.slot === tCur.target)) {
    agregar({ k: "tdel", slot: tCur.target, len: tk.len });
    it.link = null;
    agregar(it);
    pend[pend.length - 2].link = it.id;                 /* quitar la toma nueva también quita el borrado */
  } else agregar(it);
  cerrarToma();
});
function cerrarToma() { parar(); $("tplay").removeAttribute("data-on"); applyTplay(); tCur = null; $("ted").hidden = true; }
$("tcancel").addEventListener("click", cerrarToma);

/* ---------------------------------------------------------------- SOUNDS: el editor */
let sCur = null, sMaq = null, sTimer = null;
zona("sounds", (f, el) => { const m = el.closest(".mach[data-mach]"); abrirSonido(f, m ? m.dataset.mach : null); });
$("spick").addEventListener("click", () => { sMaq = null; $("fsnd").click(); });
$("fsnd").addEventListener("change", () => { const f = $("fsnd").files[0]; $("fsnd").value = ""; if (f) abrirSonido(f, sMaq); });

async function abrirSonido(file, maq) {
  const t = T(), raw = new Uint8Array(await file.arrayBuffer());
  const ext = ((/\.(\w+)$/.exec(file.name) || [])[1] || "").toLowerCase();
  const base = file.name.replace(/\.[^.]+$/, "");
  let kind, json = null, name = base;
  if (ext === "syx" || raw[0] === 0xF0) kind = "syx";
  else if (ext === "bsnap" || (raw[0] === 0x42 && raw[1] === 0x4E && raw[2] === 0x53 && raw[3] === 0x31)) {
    kind = "bsnap";
    try { name = readBsnap(raw).name || base; } catch (_) { /* se dice al convertir */ }
  } else {
    kind = "vital";
    try { json = JSON.parse(new TextDecoder().decode(raw)); name = json.preset_name || base; } catch (_) { json = null; }
  }
  const machine = VITAL.includes(maq) ? maq : VITAL.includes($("smach").value) ? $("smach").value : "BITTAL";
  sCur = { file, raw, kind, json, machine, conv: null, info: null, err: "", maqPedida: maq };
  $("smach").value = machine;
  $("sname").value = name.normalize("NFD").replace(/[̀-ͯ]/g, "").toUpperCase().replace(/[^ -~]/g, "").slice(0, 12);
  $("sed").hidden = false;
  $("sfile").textContent = file.name;
  convertir();
  $("sed").scrollIntoView({ block: "nearest", behavior: "smooth" });
  $("sname").focus({ preventScroll: true });
}

function convertir() {
  if (!sCur) return;
  const s = sCur, name = $("sname").value.trim() || "SOUND";
  s.err = ""; s.conv = null; s.info = null;
  try {
    if (s.kind === "vital") {
      if (!s.json) throw new Error(T().notVital);
      s.conv = vitalToBsnap(s.json, { machine: s.machine, name });
    } else if (s.kind === "syx") s.conv = syxToBsnap(s.raw, { name });
    else {
      const h = readBsnap(s.raw);
      s.conv = { bytes: h.name === name ? s.raw.slice(0, h.size) : renameBsnap(s.raw, name), report: { kept: [], approx: [], dropped: [] } };
    }
    s.info = readBsnap(s.conv.bytes);
  } catch (e) { s.err = e.message || String(e); }
  notaSonido();
}

function notaSonido() {
  const s = sCur, t = T();
  if (!s) return;
  $("smachl").hidden = s.kind !== "vital";
  const li = (cls, label, xs) => (xs || []).map((x) => `<li class="${cls}">${esc(label)}: ${esc(x)}</li>`).join("");
  if (s.err) {
    $("snote").className = "note bad"; $("snote").textContent = t.cantRead(s.err);
    html($("srep"), ""); $("sok").disabled = true; $("sdl").disabled = true; return;
  }
  const r = s.conv.report, eng = s.info.eng;
  const P = plan([{ id: -1, k: "sadd", eng, size: s.info.size }]), bad = P.bad.get(-1);
  let lead = ENG[eng] + " · #" + s.info.id + " · " + kb(sndSect(s.info.size) * SECTOR);
  if (s.kind === "syx" && s.maqPedida && s.maqPedida !== "BX-7") lead += " · " + t.syxGoes;
  if (s.kind === "bsnap" && s.maqPedida && s.maqPedida !== ENG[eng]) lead += " · " + t.bsnapGoes(ENG[eng]);
  $("snote").className = "note " + (bad ? "bad" : "ok");
  $("snote").textContent = lead + " — " + (bad || t.fitsSound(kb(P.freeB)));
  html($("srep"), (r.names ? `<li class="c">${esc(r.names.join(" · "))}</li>` : "") + li("k", t.rKept, r.kept) + li("a", t.rApprox, r.approx) + li("d", t.rDropped, r.dropped) +
    (r.cpu ? `<li class="mut">${esc(r.cpu)}</li>` : ""));
  $("sok").disabled = sending; $("sdl").disabled = false;
}
$("smach").addEventListener("change", () => { if (sCur) { sCur.machine = $("smach").value; convertir(); } });
$("sname").addEventListener("input", () => {
  const el = $("sname"), p = el.selectionStart;
  el.value = el.value.toUpperCase().replace(/[^ -~]/g, "");
  el.setSelectionRange(p, p);
  clearTimeout(sTimer); sTimer = setTimeout(convertir, 250);
});
$("sok").addEventListener("click", () => {
  clearTimeout(sTimer); convertir();
  const s = sCur;
  if (!s || s.err) return;
  agregar({ k: "sadd", eng: s.info.eng, bytes: s.conv.bytes, name: s.info.name, fp: parseInt(s.info.id, 16), size: s.info.size, file: s.file.name });
  cerrarSonido();
});
$("sdl").addEventListener("click", () => {
  if (!sCur || !sCur.conv) return;
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([sCur.conv.bytes], { type: "application/octet-stream" }));
  a.download = (sCur.info.name || "sound").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") + ".bsnap";
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
});
function cerrarSonido() { sCur = null; $("sed").hidden = true; }
$("scancel").addEventListener("click", cerrarSonido);

/* ---------------------------------------------------------------- SCREEN LANGUAGE */
function ponerIdioma(it) {
  pend = pend.filter((x) => x.k !== "lang" || x.st === "run");
  if (it && !(dev && it.mode !== "file" && it.mode === langActual())) agregar(it);
  else { aviso = null; render(); }
}
$("lopts").addEventListener("change", async (e) => {
  const v = e.target.value;
  if (v === "off") ponerIdioma({ k: "lang", mode: "off", name: "" });
  else if (v === "es") {
    try {
      const r = await fetch("lang/es.bmlang", { cache: "no-cache" });
      if (!r.ok) throw new Error(r.status);
      const bytes = new Uint8Array(await r.arrayBuffer());
      ponerIdioma({ k: "lang", mode: "es", bytes, name: str(bytes, 8, 12) || "ESPANOL" });
    } catch (err) { aviso = ["bad", T().cantRead(err.message || err)]; render(); }
  } else { $("flang").click(); render(); }
});
$("lpick").addEventListener("click", (e) => { e.preventDefault(); $("flang").click(); });
$("flang").addEventListener("change", async () => {
  const f = $("flang").files[0];
  $("flang").value = "";
  if (!f) return;
  const bytes = new Uint8Array(await f.arrayBuffer());
  if (bytes.length < 20 || str(bytes, 0, 4) !== "BMLG") { aviso = ["bad", T().notLang]; render(); return; }
  ponerIdioma({ k: "lang", mode: "file", bytes, name: str(bytes, 8, 12) || f.name });
});

/* ---------------------------------------------------------------- DEVICE: el registro */
$("dlog").addEventListener("click", async () => {
  const pre = $("logtxt");
  if (!acc) return;
  $("dlog").disabled = true;
  try {
    const out = [];
    for (let off = 0, k = 0; k < 256; k++) {
      const r = await pedir(acc, 0x07, [off >> 8, off & 255]), n = r[2];
      out.push(...r.slice(3, 3 + n)); off += n;
      if (n < 64) break;
    }
    pre.textContent = new TextDecoder().decode(new Uint8Array(out)) || "—";
  } catch (e) { pre.textContent = porque(e); }
  pre.hidden = false; $("dlog").disabled = false;
});

/* ---------------------------------------------------------------- arranque */
applyLang();
if (MOCK) conectar();
else if (navigator.permissions && navigator.requestMIDIAccess) {        /* si ya dio permiso antes, se conecta solo */
  navigator.permissions.query({ name: "midi", sysex: true }).then((p) => { if (p.state === "granted") conectar(); }).catch(() => {});
}
