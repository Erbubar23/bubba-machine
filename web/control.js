// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// Plataforma de control (control.html): administra el FM-1 con Bubba Machine por la consola USB-MIDI
// (web/fm1_consola.js, hal/fm1/fm1_consola.c). Sin el FM-1 conectado no hace nada: no hay modo emulador.
//   - memoria: RAM libre del sistema, carga del audio, tomas (tope de 6 s), la zona de 100 KiB que
//     comparten tomas y sonidos, y el paquete de idioma; con avisos según lo que se está haciendo;
//   - tomas: subir un audio (se convierte aquí a 16 kHz y 8 bits, como lo graba el FM-1), borrar;
//   - sonidos: presets de Vital, bancos .syx de la DX7 y .bsnap; borrar los del usuario;
//   - idioma: cargar o quitar el paquete.
import { pedir, enviar, puertos } from "./fm1_consola.js?v=2";
import { vitalToBsnap, syxToBsnap } from "./bsnap.js";

const $ = (id) => document.getElementById(id);
const SMP = 16000;
const ENG = { 3: "B-MINI", 4: "BX-7", 5: "BITTAL", 6: "BUNO-6" };

let lang = new URLSearchParams(location.search).get("lang") ||
  (() => { try { return localStorage.getItem("bm-lang"); } catch (e) { return null; } })() ||
  ((navigator.language || "es").startsWith("es") ? "es" : "en");

const EN = {
  "nav.inst": "Install", "nav.emu": "Emulator", "nav.man": "Manual", h1: "FM-1 control",
  lead: "Connect your FM-1 with Bubba Machine over USB and manage it from here: sampler takes, sounds (Vital presets and DX7 banks), the screen language and the memory. Everything goes through the USB-MIDI console; without the FM-1 connected, this page does nothing.",
  "st.wait": "Looking for the FM-1…", "b.connect": "Connect", "h.mem": "Memory", "m.ram": "Free RAM (system heap)", "m.cpu": "Audio load",
  "m.takes": "Takes (6 s cap)", "m.zone": "Takes and sounds zone (100 KiB)", "m.lang": "Language pack (16 KiB)",
  "h.takes": "Sampler takes", "t.len": "Length", "t.pads": "Pads", "d.take": "Drop an audio file (WAV, MP3, FLAC…) or click",
  "t.from": "From (s)", "t.dur": "Length (s)", "b.listen": "Listen as on the FM-1", "b.sendtake": "Send to the FM-1",
  "h.snd": "Sounds", "s.mach": "Machine", "s.name": "Name", "s.auth": "Author", "d.snd": "Drop a Vital preset (.vital), a DX7 bank (.syx) or a .bsnap",
  "b.sendsnd": "Send to the FM-1", "h.lang": "Screen language",
  "l.txt": "The screen is in English. The language pack is a separate add-on: it lives in its own 16 KiB zone and takes no take or sound memory. It is changed here.",
  "b.es": "Load Spanish", "b.lfile": "Another pack (.bmlang)…", "b.loff": "Remove (English)",
};
const TX = {
  es: {
    nomidi: "Este navegador no tiene Web MIDI: usa Chrome o Edge.", deny: "Sin permiso de MIDI no se puede hablar con el FM-1.",
    nofm1: "Conecta el FM-1 por USB y préndelo (cierra otros programas que usen su MIDI).",
    old: (v) => `El FM-1 responde, pero no a la plataforma de control${v ? " (" + v + ")" : ""}: instala Bubba Machine 0.6.1-beta o más nueva.`,
    on: (v) => `Conectado: Bubba Machine ${v}`, playing: "Sonando: para el transporte (STOP) para escribir. Escribir la flash corta el audio.",
    ramBad: (k) => `RAM: quedan ${k} KiB del sistema. El FM-1 puede colgarse; no cargues más cosas y avisa este número.`,
    ramWarn: (k) => `RAM: quedan ${k} KiB del sistema; lo normal es más de 48 KiB.`,
    cpuBad: (p) => `Carga del audio ${p} %: cerca del límite, puede cortarse el sonido. Apaga voces o efectos.`,
    cpuWarn: (p) => `Carga del audio ${p} %.`,
    late: (n) => `${n} bloques de audio llegaron tarde desde que conectaste: hubo cortes.`,
    dual: (on, cpu) => on ? `Dos núcleos: encendido${cpu != null ? " (el segundo cálculo corrió en el núcleo " + cpu + ")" : ""}.` : "Dos núcleos: apagado (SISTEMA › TWO CORES). Se apaga solo después de un cuelgue.",
    takesFull: "Tomas: llegaste a los 6 s. Borra una para grabar o subir otra.",
    takesLow: (s) => `Tomas: quedan ${s} s de los 6.`,
    zoneLow: (k) => `Zona compartida: quedan ${k} KiB para tomas y sonidos.`,
    sndMax: (n) => `Sonidos: ${n} de 12, el máximo. Borra uno para cargar otro.`,
    tFit: (s, r) => `Caben ${r} s: la toma quedará de ${s} s.`,
    tNo: "No cabe ninguna toma: borra una toma (o un sonido, si la zona está llena).",
    tCut: (s) => `Se manda recortada a ${s} s (lo que cabe ahora).`,
    sFit: (kb, free) => `Ocupa ${kb} KiB; quedan ${free} KiB en la zona.`,
    sNo: (kb, free) => `No cabe: ocupa ${kb} KiB y quedan ${free} KiB. Borra una toma o un sonido.`,
    lFit: (kb) => `El paquete ocupa ${kb} KiB de 16.`,
    lNo: "Ese paquete no cabe en los 16 KiB.",
    sending: "Mandando…", tok: "Toma enviada: el FM-1 la guarda en TOMAS ahora que está parado.",
    sok: "Sonido guardado: PRESETS en el canal de su máquina lo elige.", lok: "Idioma cargado.", loff: "Pantalla en inglés.",
    del: "¿Borrar? No se puede deshacer.", delok: "Borrado.", fail: "Falló: ", bad: "No se pudo leer: ",
    info: (h, r) => `Montón libre según la RAM: ${r} KiB · RAM total del FM-1 ~511 KiB; el firmware fija ~384 KiB.`,
    pads: (n) => n ? n : "—", none: "—", conv: "Convirtiendo…", own: "propio",
  },
  en: {
    nomidi: "This browser has no Web MIDI: use Chrome or Edge.", deny: "Without MIDI permission the page cannot talk to the FM-1.",
    nofm1: "Connect the FM-1 over USB and switch it on (close other programs using its MIDI).",
    old: (v) => `The FM-1 answers, but not to the control platform${v ? " (" + v + ")" : ""}: install Bubba Machine 0.6.1-beta or newer.`,
    on: (v) => `Connected: Bubba Machine ${v}`, playing: "Playing: stop the transport (STOP) to write. Writing flash cuts the audio.",
    ramBad: (k) => `RAM: ${k} KiB of system memory left. The FM-1 may hang; don't load anything else and report this number.`,
    ramWarn: (k) => `RAM: ${k} KiB of system memory left; normal is above 48 KiB.`,
    cpuBad: (p) => `Audio load ${p} %: close to the limit, sound may drop out. Turn off voices or effects.`,
    cpuWarn: (p) => `Audio load ${p} %.`,
    late: (n) => `${n} audio blocks were late since you connected: there were dropouts.`,
    dual: (on, cpu) => on ? `Two cores: on${cpu != null ? " (the second job ran on core " + cpu + ")" : ""}.` : "Two cores: off (SYSTEM › TWO CORES). It switches off by itself after a hang.",
    takesFull: "Takes: you reached 6 s. Delete one to record or upload another.",
    takesLow: (s) => `Takes: ${s} s of 6 left.`,
    zoneLow: (k) => `Shared zone: ${k} KiB left for takes and sounds.`,
    sndMax: (n) => `Sounds: ${n} of 12, the maximum. Delete one to load another.`,
    tFit: (s, r) => `${r} s fit: the take will be ${s} s.`,
    tNo: "No take fits: delete a take (or a sound, if the zone is full).",
    tCut: (s) => `It is sent trimmed to ${s} s (what fits now).`,
    sFit: (kb, free) => `It takes ${kb} KiB; ${free} KiB left in the zone.`,
    sNo: (kb, free) => `It does not fit: it takes ${kb} KiB and ${free} KiB are left. Delete a take or a sound.`,
    lFit: (kb) => `The pack takes ${kb} KiB of 16.`,
    lNo: "That pack does not fit in 16 KiB.",
    sending: "Sending…", tok: "Take sent: the FM-1 stores it in TAKES now that it is stopped.",
    sok: "Sound saved: PRESETS on its machine's track picks it.", lok: "Language loaded.", loff: "Screen in English.",
    del: "Delete? It cannot be undone.", delok: "Deleted.", fail: "Failed: ", bad: "Could not read it: ",
    info: (h, r) => `Free heap by RAM: ${r} KiB · FM-1 RAM ~511 KiB in total; the firmware fixes ~384 KiB.`,
    pads: (n) => n ? n : "—", none: "—", conv: "Converting…", own: "own",
  },
};
const T = () => TX[lang];

function applyLang() {
  document.documentElement.lang = lang;
  document.querySelectorAll("[data-t]").forEach((e) => {
    const k = e.dataset.t;
    if (e.dataset.es === undefined) e.dataset.es = e.innerHTML;
    e.innerHTML = lang === "en" && EN[k] ? EN[k] : e.dataset.es;
  });
  $("langsw").textContent = lang === "en" ? "Español" : "English";
  if (estado) pintar();
}
$("langsw").addEventListener("click", (e) => {
  e.preventDefault(); lang = lang === "en" ? "es" : "en";
  try { localStorage.setItem("bm-lang", lang); } catch (_) {}
  applyLang();
});

/* ---------------------------------------------------------------- conexión */
let acc = null, estado = null, ocupado = false, lateBase = null, timer = null;
const le32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
const le16 = (b, o) => b[o] | b[o + 1] << 8;
const str = (b, o, n) => String.fromCharCode(...b.slice(o, o + n)).replace(/\0.*$/s, "").trim();
const kib = (n) => (n / 1024).toFixed(n < 10240 ? 1 : 0);
const seg = (n) => (n / SMP).toFixed(1);
function log(t, cls) { $("log").textContent = t; $("log").className = "log " + (cls || ""); }

async function conectar() {
  if (!navigator.requestMIDIAccess) { $("estado").textContent = T().nomidi; $("estado").className = "bad"; return; }
  try { acc = await navigator.requestMIDIAccess({ sysex: true }); } catch (_) { $("estado").textContent = T().deny; $("estado").className = "bad"; return; }
  $("conectar").hidden = true;
  acc.onstatechange = () => { setTimeout(leer, 600); };
  leer();
  clearInterval(timer);
  timer = setInterval(() => { if (!ocupado && !document.hidden) leer(); }, 2000);
}
$("conectar").addEventListener("click", conectar);

async function informe() {
  const r = await pedir(acc, 0x23, [0, 0], 1500);
  if (r[1] !== 1) throw new Error("informe");
  const b = r.slice(2), o = 8;
  return {
    heap: le32(b, 0), heapRam: le32(b, 4),
    playing: b[o + 1], cpu: b[o + 2], dual: b[o + 3], nTakes: b[o + 4], nSnd: b[o + 5], maxSnd: b[o + 6], langOn: b[o + 7],
    takesLen: le32(b, o + 8), takesCap: le32(b, o + 12), takesFree: le32(b, o + 16), room: le32(b, o + 20),
    zone: le32(b, o + 24), zoneFree: le32(b, o + 28), sndBytes: le32(b, o + 32), recMax: le32(b, o + 38),
    langName: str(b, o + 42, 12), langSize: le16(b, o + 54), langCap: le16(b, o + 56), version: str(b, o + 58, 14),
  };
}

async function stats() {
  const r = await pedir(acc, 0x01, [], 1500);
  const w = (i) => le32(r, i * 4);
  return { avg: w(8), budget: w(9), late: w(5), parCpu: w(26) };
}

async function listas(e) {
  e.takes = [];
  for (let i = 0; i < 8; i++) {
    const r = await pedir(acc, 0x23, [1, i]);
    const b = r.slice(2);
    if (r[1] === 1 && b[0]) e.takes.push({ id: b[1], len: le32(b, 2), bpm: le16(b, 6) / 10, pads: b[10] });
  }
  e.snds = [];
  for (let i = 0; i < e.nSnd; i++) {
    const r = await pedir(acc, 0x23, [2, i]);
    const b = r.slice(2);
    if (r[1] === 1) e.snds.push({ idx: i, eng: b[0], sect: b[2], size: le16(b, 4), name: str(b, 8, 12), author: str(b, 20, 12) });
  }
}

let firma = "";
async function leer() {
  if (ocupado || !acc) return;
  const { out, inp } = puertos(acc);
  if (!out || !inp) { estado = null; lateBase = null; pintarSin(T().nofm1); return; }
  let e;
  try { e = await informe(); } catch (_) {
    /* contesta el estado (01) pero no el informe (23): es un Bubba Machine anterior a 0.6.1 */
    try { await pedir(acc, 0x01, [], 1200); } catch (_) { estado = null; pintarSin(T().nofm1); return; }
    estado = null; pintarSin(T().old(""), true); return;
  }
  try { Object.assign(e, await stats()); } catch (_) { /* sin estadísticas */ }
  const f = [e.nTakes, e.nSnd, e.takesLen, e.sndBytes].join(",");
  if (f !== firma || !estado) { await listas(e); firma = f; } else { e.takes = estado.takes; e.snds = estado.snds; }
  if (lateBase === null && e.late != null) lateBase = e.late;
  estado = e;
  pintar();
}

function pintarSin(msg, viejo) {
  $("estado").innerHTML = msg + (viejo ? ` <a href="instalar.html">${lang === "en" ? "Installer" : "Instalador"}</a>` : "");
  $("estado").className = viejo ? "warn" : "bad";
  $("panel").classList.add("off");
  $("avisos").innerHTML = "";
}

/* ---------------------------------------------------------------- dibujo y avisos */
function barra(id, frac, color) { const el = $(id); el.style.width = Math.max(0, Math.min(100, frac * 100)) + "%"; el.style.background = color; }
const col = (lvl) => lvl === "bad" ? "var(--las)" : lvl === "warn" ? "var(--amb)" : "var(--mint)";

function avisos(e) {
  const a = [], t = T(), hk = Math.round(e.heap / 1024);
  if (e.playing) a.push(["warn", t.playing]);
  if (e.heap && hk < 24) a.push(["bad", t.ramBad(hk)]); else if (e.heap && hk < 48) a.push(["warn", t.ramWarn(hk)]);
  const cpu = e.budget ? Math.round(e.avg * 100 / e.budget) : e.cpu;
  if (cpu >= 85) a.push(["bad", t.cpuBad(cpu)]); else if (cpu >= 70) a.push(["warn", t.cpuWarn(cpu)]);
  if (e.late != null && lateBase != null && e.late > lateBase) a.push(["bad", t.late(e.late - lateBase)]);
  if (e.takesFree < 1600) a.push(["warn", t.takesFull]); else if (e.takesFree < SMP) a.push(["warn", t.takesLow(seg(e.takesFree))]);
  if (e.zoneFree < 8192) a.push(["warn", t.zoneLow(kib(e.zoneFree))]);
  if (e.nSnd >= e.maxSnd) a.push(["warn", t.sndMax(e.nSnd)]);
  a.push(["ok", t.dual(e.dual, e.dual && e.parCpu != null ? e.parCpu : null)]);
  return a;
}

function pintar() {
  const e = estado, t = T();
  if (!e) return;
  $("estado").textContent = t.on(e.version); $("estado").className = "ok";
  $("panel").classList.remove("off");
  $("avisos").innerHTML = avisos(e).map(([c, m]) => `<li class="${c}">${m}</li>`).join("");
  const RAM_TOT = 126 * 1024;                        /* el montón del 0.6.1 (tools/build.py: ram_check) */
  $("mram").textContent = kib(e.heap) + " KiB";
  barra("bram", e.heap / RAM_TOT, col(e.heap < 24576 ? "bad" : e.heap < 49152 ? "warn" : "ok"));
  const cpu = e.budget ? Math.round(e.avg * 100 / e.budget) : e.cpu;
  $("mcpu").textContent = cpu + " %"; barra("bcpu", cpu / 100, col(cpu >= 85 ? "bad" : cpu >= 70 ? "warn" : "ok"));
  $("mtak").textContent = `${seg(e.takesLen)} / ${seg(e.takesCap)} s`;
  barra("btak", e.takesLen / e.takesCap, col(e.takesFree < SMP ? "warn" : "ok"));
  const used = e.zone - e.zoneFree, tk = used - e.sndBytes;
  $("mzon").textContent = `${kib(used)} / ${kib(e.zone)} KiB`;
  $("bzon").innerHTML = `<i style="width:${tk / e.zone * 100}%;background:var(--amb)"></i><i style="width:${e.sndBytes / e.zone * 100}%;background:var(--cy)"></i>`;
  $("mlan").textContent = e.langOn ? `${e.langName} · ${kib(e.langSize)} KiB` : "ENGLISH";
  barra("blan", e.langOn ? e.langSize / e.langCap : 0, col("ok"));
  $("minfo").innerHTML = (lang === "en" ? "<span style='color:var(--amb)'>■</span> takes <span class='c'>■</span> sounds · " : "<span style='color:var(--amb)'>■</span> tomas <span class='c'>■</span> sonidos · ") + t.info(e.heap, Math.round(e.heapRam / 1024));
  $("ltak").innerHTML = e.takes.map((k) => `<tr><td>${String(k.id + 1).padStart(2, "0")}</td><td>${seg(k.len)} s</td><td>${k.bpm || t.none}</td><td>${t.pads(k.pads)}</td><td><button class="del" data-take="${k.id}">✕</button></td></tr>`).join("") || `<tr><td colspan="5" class="small">${t.none}</td></tr>`;
  $("lsnd").innerHTML = e.snds.map((s) => `<tr><td>${ENG[s.eng] || s.eng}</td><td>${s.name}</td><td>${s.author || t.own}</td><td>${kib(s.sect * 4096)}</td><td><button class="del" data-snd="${s.idx}">✕</button></td></tr>`).join("") || `<tr><td colspan="5" class="small">${t.none}</td></tr>`;
  const escribir = !e.playing;
  ["tsend", "ssend", "les", "lfile", "loff"].forEach((id) => { $(id).disabled = !escribir || ocupado; });
  document.querySelectorAll("button.del").forEach((b) => { b.disabled = !escribir || ocupado; });
  revisarToma(); revisarSonido();
}

async function trabajo(fn, okMsg) {
  if (ocupado) return;
  ocupado = true; pintar();
  try { await fn(); log(okMsg, "ok"); } catch (e) { log(T().fail + (e.message || e), "bad"); }
  $("bar").value = 0; ocupado = false; firma = ""; setTimeout(leer, 1200);
}

document.addEventListener("click", (ev) => {
  const b = ev.target.closest("button.del");
  if (!b || !estado || !confirm(T().del)) return;
  if (b.dataset.take !== undefined) {
    const id = +b.dataset.take;
    trabajo(async () => { const r = await pedir(acc, 0x23, [3, id]); if (r[1] !== 1) throw new Error("toma"); }, T().delok);
  } else {
    const i = +b.dataset.snd;
    trabajo(async () => { const r = await pedir(acc, 0x22, [4, 0, 0, 0, i, 0, 0]); if (r[1] !== 1) throw new Error("sonido"); }, T().delok);
  }
});

/* ---------------------------------------------------------------- tomas */
let audio = null, toma = null;
function caja(drop, file, fn) {
  $(drop).addEventListener("click", () => $(file).click());
  $(drop).addEventListener("keydown", (e) => { if (e.key === "Enter" || e.key === " ") $(file).click(); });
  $(file).addEventListener("change", () => { if ($(file).files[0]) fn($(file).files[0]); $(file).value = ""; });
  ["dragover", "dragenter"].forEach((t) => $(drop).addEventListener(t, (e) => { e.preventDefault(); $(drop).classList.add("on"); }));
  ["dragleave", "drop"].forEach((t) => $(drop).addEventListener(t, () => $(drop).classList.remove("on")));
  $(drop).addEventListener("drop", (e) => { e.preventDefault(); if (e.dataTransfer.files[0]) fn(e.dataTransfer.files[0]); });
}

caja("dtak", "ftak", async (f) => {
  try {
    log(T().conv);
    const ctx = new AudioContext();
    const buf = await ctx.decodeAudioData(await f.arrayBuffer());
    ctx.close();
    /* a 16 kHz y mono con el remuestreo del navegador (filtra antes de bajar la frecuencia) */
    const off = new OfflineAudioContext(1, Math.ceil(buf.duration * SMP), SMP);
    const src = off.createBufferSource(); src.buffer = buf; src.connect(off.destination); src.start();
    audio = (await off.startRendering()).getChannelData(0);
    const m = /(\d{2,3}(?:\.\d)?)\s*bpm/i.exec(f.name);
    if (m) $("tbpm").value = m[1];
    $("tfrom").value = 0;
    $("tdur").value = Math.min(audio.length / SMP, (estado ? estado.room : 49152) / SMP).toFixed(3);
    $("tprev").hidden = false;
    log("");
    revisarToma();
  } catch (e) { log(T().bad + (e.message || e), "bad"); }
});

function armarToma() {
  if (!audio) return null;
  const from = Math.max(0, Math.round(+$("tfrom").value * SMP));
  let n = Math.max(0, Math.min(audio.length - from, Math.round(+$("tdur").value * SMP)));
  const room = estado ? estado.room : 0;
  const recortada = n > room;
  if (recortada) n = room;
  const x = audio.subarray(from, from + n);
  let pico = 1e-6;
  for (const v of x) pico = Math.max(pico, Math.abs(v));
  const g = 0.95 * 127 / pico, out = new Uint8Array(n);
  let s = 1;
  const rnd = () => { s = (s * 1103515245 + 12345) >>> 0; return s / 4294967296; };
  for (let i = 0; i < n; i++) {                          /* 8 bits con ruido triangular (dither), como toma.py */
    const v = Math.max(-128, Math.min(127, Math.round(x[i] * g + rnd() - rnd())));
    out[i] = v & 255;
  }
  return { bytes: out, recortada };
}

function revisarToma() {
  if (!audio || !estado) return;
  const t = T(), dur = Math.min(+$("tdur").value, (audio.length / SMP) - +$("tfrom").value);
  if (estado.room < SMP / 10) { $("tinfo").textContent = t.tNo; $("tinfo").className = "small bad"; $("tsend").disabled = true; return; }
  const r = estado.room / SMP;
  $("tinfo").textContent = dur > r ? t.tCut(r.toFixed(2)) : t.tFit(Math.max(0, dur).toFixed(2), r.toFixed(2));
  $("tinfo").className = "small " + (dur > r ? "warn" : "ok");
}
["tfrom", "tdur"].forEach((id) => $(id).addEventListener("input", revisarToma));

$("tplay").addEventListener("click", () => {
  const k = armarToma();
  if (!k) return;
  const ctx = new AudioContext(), b = ctx.createBuffer(1, Math.max(1, k.bytes.length), SMP), d = b.getChannelData(0);
  for (let i = 0; i < k.bytes.length; i++) d[i] = ((k.bytes[i] << 24) >> 24) / 128;
  const s = ctx.createBufferSource(); s.buffer = b; s.connect(ctx.destination); s.start();
  s.onended = () => ctx.close();
});

$("tsend").addEventListener("click", () => {
  const k = armarToma();
  if (!k || !k.bytes.length) return;
  const bpm = Math.round(Math.max(40, Math.min(240, +$("tbpm").value || 120)) * 10);
  trabajo(() => enviar(acc, 0x20, k.bytes, (x) => { $("bar").value = 100 * x; log(T().sending); }, [k.bytes.length, bpm]), T().tok);
});

/* ---------------------------------------------------------------- sonidos */
let snd = null;
caja("dsnd", "fsnd", async (f) => {
  try {
    const raw = new Uint8Array(await f.arrayBuffer());
    const name = ($("sname").value || f.name.replace(/\.[^.]+$/, "")).toUpperCase().slice(0, 12);
    if (/\.bsnap$/i.test(f.name)) snd = { bytes: raw, report: { size: raw.length, kept: [], approx: [], dropped: [] } };
    else if (/\.syx$/i.test(f.name)) snd = syxToBsnap(raw, { name });
    else snd = vitalToBsnap(JSON.parse(new TextDecoder().decode(raw)), { machine: $("smach").value, name });
    $("sprev").hidden = false;
    revisarSonido();
  } catch (e) { snd = null; $("sprev").hidden = false; $("sinfo").textContent = T().bad + (e.message || e); $("sinfo").className = "small bad"; }
});

function revisarSonido() {
  if (!snd || !estado) return;
  const t = T(), need = Math.ceil((snd.bytes.length + 4) / 4096) * 4096, cabe = need <= estado.zoneFree && estado.nSnd < estado.maxSnd;
  const r = snd.report, extra = r.dropped && r.dropped.length ? ` · ${lang === "en" ? "left out" : "queda fuera"}: ${r.dropped.join(", ")}` : "";
  $("sinfo").textContent = (cabe ? t.sFit(kib(need), kib(estado.zoneFree - need)) : estado.nSnd >= estado.maxSnd ? t.sndMax(estado.nSnd) : t.sNo(kib(need), kib(estado.zoneFree))) + extra;
  $("sinfo").className = "small " + (cabe ? "ok" : "bad");
  $("ssend").disabled = !cabe || estado.playing || ocupado;
}

$("ssend").addEventListener("click", () => {
  if (!snd) return;
  trabajo(() => enviar(acc, 0x22, snd.bytes, (x) => { $("bar").value = 100 * x; log(T().sending); }), T().sok);
});

/* ---------------------------------------------------------------- idioma */
async function cargarIdioma(bytes) {
  if (bytes.length > (estado ? estado.langCap : 16384)) { log(T().lNo, "bad"); return; }
  log(T().lFit(kib(bytes.length)));
  trabajo(() => enviar(acc, 0x21, bytes, (x) => { $("bar").value = 100 * x; }), T().lok);
}
$("les").addEventListener("click", async () => {
  const r = await fetch("lang/es.bmlang", { cache: "no-cache" });
  cargarIdioma(new Uint8Array(await r.arrayBuffer()));
});
$("lfile").addEventListener("click", () => $("flan").click());
$("flan").addEventListener("change", async () => { const f = $("flan").files[0]; if (f) cargarIdioma(new Uint8Array(await f.arrayBuffer())); $("flan").value = ""; });
$("loff").addEventListener("click", () => {
  trabajo(async () => { const r = await pedir(acc, 0x21, [3, 0, 0, 0, 0, 0, 0]); if (r[1] !== 1) throw new Error("idioma"); }, T().loff);
});

applyLang();
$("estado").textContent = T().nofm1;
/* si el navegador ya dio permiso de MIDI antes, se conecta solo */
if (navigator.permissions && navigator.requestMIDIAccess) {
  navigator.permissions.query({ name: "midi", sysex: true }).then((p) => { if (p.state === "granted") conectar(); }).catch(() => {});
}
