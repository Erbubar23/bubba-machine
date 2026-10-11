// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
//
// Arma el paquete de Bubba Machine en el navegador: el FM-1.fwsc V15 que cada quien descarga de
// M-VAVE + el app.bin de Bubba Machine. Así lo de M-VAVE (arranque y cargador) nunca se publica.
// Es el mismo procedimiento que tools/fm1/fwsc.py (y tests/test_pack.mjs comprueba que den los
// mismos bytes): solo cambia app.bin dentro de la zona de la app, que se vuelve a cifrar con la
// llave del chip; se recalculan los CRC que cubren lo que cambió.

import { crc16, imagenLogica, identidadPaquete } from "./fm1_update.js";

class FalloPaquete extends Error {}
const bad = (m) => { throw new FalloPaquete(m); };

function enc(buf, off, n, key = 0xFFFF) {
  for (let i = 0; i < n; i++) {
    buf[off + i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
}

function sfc(buf, off, n, base, key) {
  for (let i = 0; i < n; i += 32) enc(buf, off + i, Math.min(n - i, 32), key ^ ((off + i - base) >>> 2));
}

const rd16 = (b, o) => b[o] | (b[o + 1] << 8);
const rd32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const wr16 = (b, o, v) => { b[o] = v & 0xFF; b[o + 1] = (v >> 8) & 0xFF; };
const wr32 = (b, o, v) => { wr16(b, o, v & 0xFFFF); wr16(b, o + 2, (v >>> 16) & 0xFFFF); };

// entrada JLFS de 32 bytes: hcrc dcrc offset size flags resvd index name[16]
function jlfsLeer(b, o) {
  if (crc16(b.subarray(o + 2, o + 32)) !== rd16(b, o)) bad(`entrada en 0x${o.toString(16)} dañada`);
  const raw = b.slice(o + 16, o + 32);
  let n = 0;
  while (n < 16 && raw[n]) n++;
  return { dcrc: rd16(b, o + 2), off: rd32(b, o + 4), size: rd32(b, o + 8), flags: b[o + 12], resvd: b[o + 13],
           idx: rd16(b, o + 14), raw, name: String.fromCharCode(...raw.subarray(0, n)) };
}

function jlfsArmar(e) {
  const b = new Uint8Array(32);
  wr16(b, 2, e.dcrc); wr32(b, 4, e.off); wr32(b, 8, e.size);
  b[12] = e.flags; b[13] = e.resvd; wr16(b, 14, e.idx); b.set(e.raw, 16);
  wr16(b, 0, crc16(b.subarray(2, 32)));
  return b;
}

function llaveIsd(d) {
  if (crc16(d.subarray(0, 32)) !== rd16(d, 32)) bad("isd_config: llave dañada");
  let s = 0;
  for (let i = 0; i < 16; i++) s += d[i];
  s &= 0xFF;
  s = s >= 0xE0 ? 0xAA : s <= 0x10 ? 0x55 : s;
  let k = 0;
  for (let i = 0; i < 16; i++) if ((d[16 + i] ^ d[15 - i]) < s) k |= 1 << i;
  return k;
}

function marcar(log, ident) {
  if (ident.length > 20) bad("identidad demasiado larga");
  const raw = new Uint8Array(log.length + 20);
  for (let i = 0; i < 20; i++) {
    raw.set(log.subarray(i * 0x2F, (i + 1) * 0x2F), i * 0x30);
    const m = i < ident.length ? (ident.charCodeAt(i) + i + 1) & 0xFF : 0x7D;
    if (i < ident.length && m === 0x7D) bad("identidad imposible de marcar");
    raw[i * 0x30 + 0x2F] = m;
  }
  raw.set(log.subarray(940), 960);
  return raw;
}

// v15: el .fwsc oficial (bytes); app: app.bin; devuelve el .fwsc nuevo (bytes)
export function armarPaquete(v15, app, ident = "FM-1_003", vm = [0xE5000, 0x8000]) {
  const log = imagenLogica(v15);
  const h = log.slice(0, 0x40);
  enc(h, 0, 0x40);
  const n = rd16(h, 8);
  const ents = [];
  for (let k = 0; k < n; k++) {
    const e = log.slice(0x40 + k * 0x50, 0x90 + k * 0x50);
    enc(e, 0, 0x50);
    ents.push(e);
  }
  const e0 = ents.find((e) => rd16(e, 0) === 0) || bad("el V15 no trae flash.bin");
  const flOff = rd32(e0, 8), flLen = rd32(e0, 12);
  const fl = log.slice(flOff, flOff + flLen);
  if (crc16(fl) !== rd16(e0, 4)) bad("flash.bin del V15 dañado");
  // cabecera del flash y entradas de arriba (ENC 0xFFFF)
  const top = [];
  for (let o = 32; o < 32 * 17; o += 32) {
    const e = fl.slice(o, o + 32);
    enc(e, 0, 32);
    const t = jlfsLeer(e, 0);
    t.at = o;
    top.push(t);
    if (t.idx) break;
  }
  const appdir = top.find((t) => t.flags === 0x81 && t.name === "app_dir_head") || bad("falta app_dir_head");
  const isd = top.find((t) => t.name === "isd_config.ini") || bad("falta isd_config");
  const base = appdir.off, key = llaveIsd(fl.subarray(isd.off, isd.off + 34));
  const z = fl.slice();
  sfc(z, base, z.length - base, base, key);
  const head = jlfsLeer(z, base);
  if (head.flags !== 0x83 || crc16(z.subarray(base + 32, base + head.size)) !== head.dcrc) bad("zona de la app dañada");
  const subs = [];
  let o = base + 32;
  for (;;) {
    const e = jlfsLeer(z, o);
    subs.push(e);
    o += 32;
    if (e.idx) break;
  }
  if (subs[0].name !== "app.bin") bad("la primera entrada no es app.bin");
  const files = {};
  for (const e of subs) if (e.flags === 0x82) files[e.name] = z.slice(base + e.off, base + e.off + e.size);
  const res = [];
  o = base + head.size;
  let last = head.idx;
  while (!last) {
    const e = jlfsLeer(z, o);
    res.push(z.slice(o, o + e.size));
    o += e.size;
    last = e.idx;
  }
  // armar la zona nueva
  files["app.bin"] = app;
  let pos = subs[0].off;
  const datos = [];
  const nuevas = subs.map((s) => {
    const e = { ...s };
    if (vm && s.raw[0] === 0x56 && s.raw[1] === 0x4D && s.raw[2] === 0) {   // "VM\0"
      if (vm[0] % 0x1000 || vm[1] % 0x1000 || vm[0] < 0x93000 || vm[0] + vm[1] > 0xED000) bad("zona VM fuera de lugar");
      e.off = vm[0] - base; e.size = vm[1];
    }
    if (s.flags === 0x82) {
      const d = files[s.name];
      e.off = pos; e.size = d.length; e.dcrc = crc16(d);
      datos.push(d);
      pos += d.length;
    }
    return e;
  });
  const blk = pos;
  const resLen = res.reduce((a, r) => a + r.length, 0);
  if (base + blk + resLen > flLen) bad(`app.bin no cabe (${app.length} bytes)`);
  const area = new Uint8Array(blk + resLen).fill(0xFF);
  nuevas.forEach((e, i) => area.set(jlfsArmar(e), 32 + i * 32));
  let p = subs[0].off;
  for (const d of datos) { area.set(d, p); p += d.length; }
  area.set(jlfsArmar({ ...head, size: blk, dcrc: crc16(area.subarray(32, blk)) }), 0);
  p = blk;
  for (const r of res) { area.set(r, p); p += r.length; }
  const nf = new Uint8Array(flLen).fill(0xFF);
  nf.set(fl.subarray(0, base));
  nf.set(area, base);
  sfc(nf, base, area.length, base, key);
  // paquete: flash.bin nuevo, CRC de su entrada, de la lista y de la cabecera
  const out = log.slice();
  out.set(nf, flOff);
  wr16(e0, 4, crc16(nf));
  const lst = new Uint8Array(n * 0x50);
  ents.forEach((e, k) => { const c = e.slice(); enc(c, 0, 0x50); lst.set(c, k * 0x50); });
  wr16(h, 2, crc16(lst));
  wr16(h, 0, crc16(h.subarray(2, 0x40)));
  enc(h, 0, 0x40);
  out.set(h, 0);
  out.set(lst, 0x40);
  return marcar(out, ident);
}

export { FalloPaquete, identidadPaquete };
