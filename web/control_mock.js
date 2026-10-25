// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// Un FM-1 de mentira para desarrollar la plataforma de control sin el aparato (control.html?mock=1).
// Contesta los mismos SysEx que hal/fm1/fm1_consola.c con respuestas como las de la 0.10.0-beta:
// 3 tomas, unos sonidos del usuario, el transporte parado. Las tomas y los borrados se guardan un
// rato después, como en el aparato (un trabajo de flash al parar). window.bmMock deja tocar el estado
// desde la consola del navegador o desde una prueba (bmMock.playing = 1, bmMock.busy = 1, …).
// control.js solo lo carga con ?mock=1.

const SECTOR = 4096, ZONE = 25, TAKE_HDR = 128, REC_MAX = 49152, TAKES_CAP = 96000, RATE = 16000, USER_MAX = 12;
const le32 = (v) => [v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >>> 24) & 255];
const le16 = (v) => [v & 255, (v >> 8) & 255];
const txt = (t, n) => { const a = new Array(n).fill(0); for (let i = 0; i < t.length && i < n; i++) a[i] = t.charCodeAt(i); return a; };
const takeSect = (len) => Math.ceil((TAKE_HDR + len) / SECTOR);
const sndSect = (size) => Math.ceil((size + 4) / SECTOR);

const st = {
  version: "0.10.0-beta", playing: 0, cpu: 38, dual: 0, heap: 96 * 1024, heapMin: 81 * 1024, busy: 0, late: 0,
  takes: [{ len: 23851, bpm: 1610, bars: 1, pads: 8 }, { len: 16000, bpm: 1200, bars: 1, pads: 0 }, { len: 12000, bpm: 1280, bars: 1, pads: 0 },
    null, null, null, null, null],
  snds: [
    { eng: 5, size: 1200, fp: 0xA4F9, name: "GLASS PAD", author: "ERICK" },
    { eng: 3, size: 88, fp: 0x31C2, name: "FAT LEAD", author: "" },
    { eng: 4, size: 88 + 32 * 156, fp: 0x7E10, name: "MY DX BANK", author: "" },
  ],
  lang: null, boot: Date.now() - 754000, log: "Bubba Machine 0.10.0-beta\nflash: zona de tomas leida, 3 tomas\nnucleo 1: listo\naudio: sonando\n",
  buf: [], arena: [], refused: [],
};

function used() {
  return st.takes.reduce((a, t) => a + (t ? takeSect(t.len) : 0), 0) + st.snds.reduce((a, s) => a + sndSect(s.size), 0);
}
function takesLen() { return st.takes.reduce((a, t) => a + (t ? t.len : 0), 0); }
function room() {
  if (st.takes.every(Boolean)) return 0;
  const free = (ZONE - used()) * SECTOR - TAKE_HDR, cap = TAKES_CAP - takesLen();
  return Math.max(0, Math.min(free, cap, REC_MAX));
}
function log(s) { st.log += s + "\n"; }

function report() {
  const free = Math.max(0, Math.min((ZONE - used()) * SECTOR - TAKE_HDR, TAKES_CAP - takesLen()));
  const sb = st.snds.reduce((a, s) => a + sndSect(s.size) * SECTOR, 0);
  return [1, st.playing, st.cpu, st.dual, st.takes.filter(Boolean).length, st.snds.length, USER_MAX, st.lang ? 1 : 0,
    ...le32(takesLen()), ...le32(TAKES_CAP), ...le32(free), ...le32(room()), ...le32(ZONE * SECTOR), ...le32((ZONE - used()) * SECTOR),
    ...le32(sb), ...le16(RATE), ...le32(REC_MAX), ...txt(st.lang ? st.lang.name : "", 12), ...le16(st.lang ? st.lang.size : 0),
    ...le16(16384), ...txt(st.version, 14)];
}

function stats() {
  const w = new Array(50).fill(0);
  w[0] = 0x000900; w[1] = Date.now() - st.boot; w[5] = st.late; w[8] = Math.round(st.cpu * 14.51); w[9] = 1451;
  w[16] = 0x80 | 7 | 16 | 1 << 8 | 1 << 16; w[24] = st.heap; w[25] = st.heapMin; w[28] = 2;
  w[35] = 0x01; w[37] = 3212 | 1388 << 16; w[38] = 3290 | 1402 << 16; w[39] = 3950 | 4010 << 16;
  return w.flatMap(le32);
}

// a la cola de trabajos: corre cuando el transporte está parado, como jobs_run
function trabajo(fn, ms = 600) {
  const go = () => { if (st.playing) { setTimeout(go, 300); return; } st.busy++; setTimeout(() => { fn(); st.busy--; }, ms); };
  setTimeout(go, 50);
}

function answer(cmd, d) {
  if (cmd === 0x01) return stats();
  if (cmd === 0x07) {
    const off = d[0] << 8 | d[1], b = new TextEncoder().encode(st.log).subarray(off, off + 64);
    return [d[0], d[1], b.length, ...b];
  }
  if (cmd === 0x23) {
    const sub = d[0], i = d[1];
    if (sub === 0) return [0, 1, ...le32(st.heap), ...le32(st.heapMin), ...report()];
    if (sub === 1) {
      const t = st.takes[i];
      return [1, 1, ...(t ? [1, i, ...le32(t.len), ...le16(t.bpm), t.bars, 64, t.pads, 0] : [0, i, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])];
    }
    if (sub === 2) {
      const x = st.snds[i];
      if (!x) return [2, 0];
      return [2, 1, x.eng, 0, sndSect(x.size), 0, ...le16(x.size), x.fp & 255, x.fp >> 8, ...txt(x.name, 12), ...txt(x.author, 12)];
    }
    if (sub === 3) {
      if (!st.takes[i]) return [3, 0];
      trabajo(() => { st.takes[i] = null; log("consola: borrar la toma " + (i + 1)); }, 400);
      return [3, 1];
    }
    return [sub, 0];
  }
  if (cmd === 0x20) {
    const sub = d[0], a = (d[1] << 24 | d[2] << 16 | d[3] << 8 | d[4]) >>> 0, b = d[5] << 8 | d[6];
    if (sub === 0) { st.arena = []; return [0, 1, 0, 0, 0]; }
    if (sub === 1) { for (let k = 7; k < d.length; k++) st.arena[a + k - 7] = d[k]; return [1, 1, 0, 0, 0]; }
    if (sub === 2) {
      const len = Math.min(a, REC_MAX), bpm = b;
      trabajo(() => {
        const id = st.takes.findIndex((t) => !t);
        if (id < 0 || len > room()) { log("NO ROOM IN TAKES: DISCARDED"); return; }
        const bar = Math.floor(RATE * 2400 / bpm);
        st.takes[id] = { len, bpm, bars: Math.max(1, Math.min(8, Math.floor((len + bar / 2) / bar))), pads: 0 };
        log("TAKE " + String(id + 1).padStart(2, "0") + " SAVED");
      }, 700);
      return [2, 1, 0, 0, 0];
    }
    return [sub, 0, 0, 0, 0];
  }
  if (cmd === 0x21 || cmd === 0x22) {
    const sub = d[0], off = (d[1] << 24 | d[2] << 16 | d[3] << 8 | d[4]) >>> 0, body = d.slice(7);
    const no = () => { st.refused.push([cmd, sub]); return [sub, 0, 0, 0, 0]; };
    if (st.playing || st.busy) return no();
    if (sub === 0) { st.buf = []; return [0, 1, 0, 0, 0]; }
    if (sub === 1) { body.forEach((v, k) => { st.buf[off + k] = v; }); return [1, 1, 0, 0, 0]; }
    if (cmd === 0x21) {
      if (sub === 3) { st.lang = null; return [3, 1, 0, 0, 0]; }
      if (sub !== 2) return no();
      const b = st.buf;
      if (b[0] !== 0x42 || b[1] !== 0x4D || b[2] !== 0x4C || b[3] !== 0x47) return no();
      st.lang = { name: String.fromCharCode(...b.slice(8, 20)).replace(/\0.*$/s, ""), size: b.length };
      return [2, 1, 0, 0, 0];
    }
    if (sub === 4) {
      if (off >= st.snds.length) return no();
      st.snds.splice(off, 1);
      return [4, 1, 0, 0, 0];
    }
    if (sub !== 2) return no();
    const b = st.buf, size = b[10] | b[11] << 8;
    if (b[0] !== 0x42 || b[1] !== 0x4E || b[2] !== 0x53 || b[3] !== 0x31 || size > b.length) return no();
    if (st.snds.length >= USER_MAX || used() + sndSect(size) > ZONE) return no();
    const name = (o) => String.fromCharCode(...b.slice(o, o + 12)).replace(/\0.*$/s, "").trim();
    st.snds.push({ eng: b[4], size, fp: b[8] | b[9] << 8, name: name(12), author: name(24) });
    return [2, 1, 0, 0, 0];
  }
  return null;
}

export function mockAccess() {
  window.bmMock = st;
  const inp = { name: "Bubba Machine (mock)", state: "connected", onmidimessage: null };
  const out = {
    name: "Bubba Machine (mock)", state: "connected",
    send(m) {
      const cmd = m[4], d = [];
      for (let i = 5; i + 1 < m.length - 1; i += 2) d.push((m[i] << 4) | m[i + 1]);
      const r = answer(cmd, d);
      if (!r) return;
      const msg = [0xF0, 0x7D, 0x42, 0x4D, cmd | 0x40];
      for (const b of r) msg.push(b >> 4, b & 15);
      msg.push(0xF7);
      setTimeout(() => { if (inp.onmidimessage) inp.onmidimessage({ data: new Uint8Array(msg) }); }, 2);
    },
  };
  return { inputs: new Map([["i", inp]]), outputs: new Map([["o", out]]), onstatechange: null };
}
