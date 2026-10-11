// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
//
// Prueba del instalador web contra un FM-1 simulado (sin aparato, sin MIDI real).
//
//   node tests/test_instalador.mjs FM-1.fwsc PAQUETE.fwsc
//
// FM-1.fwsc es el V15 de M-VAVE (no está en el repositorio) y PAQUETE.fwsc uno armado con
// tools/fm1/fwsc.py. El FM-1 simulado se porta como lo documentado: en el paso 1 el firmware
// que corre lee la cabecera, el cargador y los primeros 16 KiB de flash.bin, y se niega si
// no son iguales a los suyos; en el paso 2 el cargador pide la imagen como en la captura de
// FM-1-RE (docs/io/12-v15-reflash-proof.md) y la escribe.
import { readFileSync } from "node:fs";
import * as U from "../web/fm1_update.js";

let fallos = 0;
const ok = (c, m) => { console.log(`${c ? "ok  " : "FALLA"} ${m}`); if (!c) fallos++; };

// ------------------------------------------------------------- MIDI falso ---
class Puerto {
  constructor(name, dev, dir) { this.name = name; this.dev = dev; this.dir = dir; this.state = "connected"; this.onmidimessage = null; }
  async open() { if (this.state !== "connected") throw new Error("desconectado"); }
  send(b) { if (this.state !== "connected") throw new Error("desconectado"); this.dev.recibir(Array.from(b)); }
}

class Acceso {
  constructor() { this.inputs = new Map(); this.outputs = new Map(); this.n = 0; }
  conectar(dev, name) {
    const id = String(++this.n);
    const i = new Puerto(name, dev, "in"), o = new Puerto(name, dev, "out");
    dev.puertos = [i, o]; dev.emitir = (b) => setTimeout(() => i.onmidimessage && i.state === "connected" && i.onmidimessage({ data: Uint8Array.from(b) }), 1);
    this.inputs.set(id, i); this.outputs.set(id, o);
    return id;
  }
  desconectar(dev) {
    for (const p of dev.puertos) p.state = "disconnected";
    for (const m of [this.inputs, this.outputs]) for (const [k, p] of m) if (p.dev === dev) m.delete(k);
  }
}

// ------------------------------------------------------- FM-1 simulado ---
const enc = (b, o, n) => { let k = 0xFFFF; for (let i = 0; i < n; i++) { b[o + i] ^= k & 0xFF; k = ((k << 1) ^ (k & 0x8000 ? 0x1021 : 0)) & 0xFFFF; } };
const rd16 = (b, o) => b[o] | (b[o + 1] << 8);
const rd32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;

function idMsg(text) {
  const body = [0x00, 0x59, 0x11, 27, 0, 0];
  const id = new Array(27).fill(0);
  for (let i = 0; i < text.length; i++) id[i] = text.charCodeAt(i);
  let s = 0; for (const b of id) s += b;
  return [0xF0, ...U.pack7([...body, ...id, ~s & 0xFF]), 0xF7];
}

function reqMsg(addr, len) {
  const b = [0x00, 0x59, 0x30, 8, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, (len >> 8) & 0xFF, 0];
  let s = 0; for (let i = 6; i < 14; i++) s += b[i];
  return [0xF0, ...U.pack7([...b, ~s & 0xFF]), 0xF7];
}

class FM1 {
  constructor(acc, flash) {
    this.acc = acc; this.flash = flash; this.modo = null; this.ident = null; this.cola = null; this.escrito = 0;
  }
  arrancar(modo, ident) {
    this.modo = modo; this.ident = ident;
    this.acc.conectar(this, modo === "ota" ? "ota-FM-1" : "FM-1 Midi");
  }
  apagar() { this.acc.desconectar(this); }
  recibir(b) {
    if (b.length === 10 && b[1] === 0x00 && b[2] === 0x32 && b[3] === 0x45) return this.emitir(idMsg(this.ident));
    if (b.length === 6 && b[1] === 0x22 && b[2] === 0x24) { this.sesion(); return; }
    if (this.cola) { const r = this.cola; this.cola = null; r(b); }
  }
  async leer(addr, len) {
    for (let t = 0; t < 3; t++) {
      const p = new Promise((r) => { this.cola = r; setTimeout(() => r(null), 5000); });
      this.emitir(reqMsg(addr, len));
      const b = await p;
      if (!b) continue;
      const u = U.unpack7(b.slice(1, -1));
      if (u.length !== len + 15 || u[2] !== 0x30 || rd32(u, 7) !== addr || (u[11] | (u[12] << 8)) !== len) continue;
      let s = 0; for (let i = 6; i < u.length - 1; i++) s += u[i];
      if ((~s & 0xFF) !== u[u.length - 1]) continue;
      return Uint8Array.from(u.slice(14, 14 + len));
    }
    return null;
  }
  async cabecera() {
    const a = await this.leer(0, 512), b = await this.leer(512, 512);
    if (!a || !b) return null;
    const h = new Uint8Array(1024); h.set(a); h.set(b, 512);
    enc(h, 0, 0x40);
    const n = rd16(h, 8);
    if (U.crc16(h.subarray(2, 0x40)) !== rd16(h, 0) || U.crc16(h.subarray(0x40, 0x40 + n * 0x50)) !== rd16(h, 2)) return null;
    const e = {};
    for (let k = 0; k < n; k++) { enc(h, 0x40 + k * 0x50, 0x50); e[rd16(h, 0x40 + k * 0x50)] = { off: rd32(h, 0x48 + k * 0x50), size: rd32(h, 0x4C + k * 0x50) }; }
    return e;
  }
  async sesion() {
    await new Promise((r) => setTimeout(r, 50));
    if (this.modo === "app") return this.paso1();
    return this.paso2();
  }
  async paso1() {                                   // firmware que corre: revisa y pasa al cargador
    const e = await this.cabecera();
    if (!e || !e[0] || !e[100]) return;
    const l = await this.leer(e[100].off, 512);
    if (!l || rd16(l, 0) !== 0xEBAA || rd16(l, 2) !== 0x5881) return;
    for (let i = 0; i < 0x4000; i += 512) {
      const b = await this.leer(e[0].off + i, 512);
      if (!b || b.some((x, k) => x !== this.flash[i + k])) { this.rechazo = true; return; }
    }
    const len = rd32(l, 8);
    for (let i = 0; i < len; i += 512) if (!(await this.leer(e[100].off + 0x20 + i, Math.min(512, len - i)))) return;
    const ok = await this.leer(0xE0000000, 8);
    if (!ok || String.fromCharCode(...ok) !== "success\0") return;
    this.apagar();
    setTimeout(() => this.arrancar("ota", "ota-FM-1_015"), 300);
  }
  async paso2() {                                   // cargador: escribe flash.bin de arriba abajo
    const e = await this.cabecera();
    if (!e || !e[0]) return;
    const n = e[0].size;
    for (let a = n - 512; a >= 0; a -= 512) {
      const b = await this.leer(e[0].off + a, 512);
      if (!b) return;
      this.flash.set(b, a);
      this.escrito += 512;
      if (this.cortarEn && this.escrito >= this.cortarEn) { this.cortarEn = 0; this.apagar(); setTimeout(() => this.arrancar("ota", "ota-FM-1_015"), 300); return; }
    }
    if (!(await this.leer(0xF0000000, 8))) return;
    this.apagar();
    setTimeout(() => this.arrancar("app", this.alVolver), 300);
  }
}

// ----------------------------------------------------------------- pruebas ---
const [v15path, pkgpath] = process.argv.slice(2);
if (!v15path || !pkgpath) { console.log("uso: node tests/test_instalador.mjs FM-1.fwsc PAQUETE.fwsc"); process.exit(2); }
const v15 = new Uint8Array(readFileSync(v15path));
const nuestro = new Uint8Array(readFileSync(pkgpath));

const sv15 = await U.revisarPaquete(v15);
ok(sv15.stock && sv15.ident === "FM-1_015", `el V15 se reconoce (${sv15.ident})`);
const pk = await U.revisarPaquete(nuestro);
ok(!pk.stock && /^FM-1_\d+$/.test(pk.ident), `nuestro paquete pasa la revisión (${pk.ident})`);

// paquetes dañados o con otro arranque no se ofrecen
const roto = nuestro.slice(); roto[5000] ^= 1;
await U.revisarPaquete(roto).then(() => ok(false, "un byte cambiado se detecta"), (e) => ok(e.code === "paquete", `un byte cambiado se detecta: ${e.message}`));
const flashOff = (() => { const h = U.imagenLogica(nuestro).slice(0, 0x90); enc(h, 0x40, 0x50); return rd32(h, 0x48); })();
const otroArranque = U.imagenLogica(nuestro); otroArranque[flashOff + 0x100] ^= 0xFF;
{
  // rearmar el paquete con el arranque cambiado y los CRC buenos: tiene que negarse por la cabecera del flash
  const img = otroArranque;
  const h = img.slice(0, 0x40); enc(h, 0, 0x40);
  const n = rd16(h, 8);
  const lst = img.slice(0x40, 0x40 + n * 0x50);
  for (let k = 0; k < n; k++) {
    enc(lst, k * 0x50, 0x50);
    if (rd16(lst, k * 0x50) === 0) { const c = U.crc16(img.subarray(flashOff, flashOff + rd32(lst, k * 0x50 + 12))); lst[k * 0x50 + 4] = c & 0xFF; lst[k * 0x50 + 5] = c >> 8; }
    enc(lst, k * 0x50, 0x50);
  }
  const c = U.crc16(lst); h[2] = c & 0xFF; h[3] = c >> 8;
  const c2 = U.crc16(h.subarray(2, 0x40)); h[0] = c2 & 0xFF; h[1] = c2 >> 8;
  enc(h, 0, 0x40); img.set(h, 0); img.set(lst, 0x40);
  const raw = new Uint8Array(img.length + 20);
  for (let i = 0; i < 20; i++) { raw.set(img.subarray(i * 0x2F, (i + 1) * 0x2F), i * 0x30); raw[i * 0x30 + 0x2F] = nuestro[i * 0x30 + 0x2F]; }
  raw.set(img.subarray(940), 960);
  await U.revisarPaquete(raw).then(() => ok(false, "otro arranque se rechaza"), (e) => ok(/arranque/.test(e.message), `otro arranque se rechaza: ${e.message}`));
}

const flashDe = (p) => { const img = p.image; const h = img.slice(0, 0x90); enc(h, 0x40, 0x50); const o = rd32(h, 0x48), n = rd32(h, 0x4C); return img.slice(o, o + n); };
const silencio = () => {};

// 1. instalación completa desde X0X (el arranque del aparato es el del V15)
{
  const acc = new Acceso();
  const flash = flashDe(sv15).slice(); flash.fill(0x5A, 0x4000);   // "X0X" en la zona de la app
  const dev = new FM1(acc, flash); dev.alVolver = pk.ident; dev.arrancar("app", "FM-1_912");
  const up = new U.Actualizador(acc, silencio);
  const pasos = [];
  const fin = await up.instalar(pk, (k) => { if (pasos[pasos.length - 1] !== k) pasos.push(k); });
  const igual = flashDe(pk).every((b, i) => b === dev.flash[i]);
  ok(fin === pk.ident && igual, `X0X -> ${fin}: flash.bin escrito igual al del paquete (${pasos.join(" > ")})`);
}

// 2. un aparato con otro arranque se niega en el paso 1 y no se escribe nada
{
  const acc = new Acceso();
  const flash = flashDe(sv15).slice(); flash[0x200] ^= 1;
  const antes = flash.slice();
  const dev = new FM1(acc, flash); dev.arrancar("app", "FM-1_912");
  const up = new U.Actualizador(acc, silencio);
  await up.instalar(pk).then(() => ok(false, "arranque distinto: rechazado"), (e) =>
    ok(e.code === "rechazado" && dev.rechazo && dev.flash.every((b, i) => b === antes[i]), `arranque distinto: rechazado sin escribir (${e.message})`));
}

// 3. corte a la mitad del paso 2 y «Continuar» con el mismo archivo
{
  const acc = new Acceso();
  const flash = flashDe(sv15).slice(); flash.fill(0x5A, 0x4000);
  const dev = new FM1(acc, flash); dev.alVolver = pk.ident; dev.cortarEn = 200 * 512; dev.arrancar("app", "FM-1_912");
  const up = new U.Actualizador(acc, silencio);
  let e1 = null;
  await up.instalar(pk).catch((e) => { e1 = e; });
  ok(e1 && e1.code === "perdido", `corte en el paso 2: ${e1 && e1.message}`);
  await new Promise((r) => setTimeout(r, 500));
  const fin = await up.continuar(pk);
  ok(fin === pk.ident && flashDe(pk).every((b, i) => b === dev.flash[i]), `«Continuar» termina la escritura (${fin})`);
}

// 4. volver al V15 desde nuestro firmware
{
  const acc = new Acceso();
  const flash = flashDe(pk).slice();
  const dev = new FM1(acc, flash); dev.alVolver = "FM-1_015"; dev.arrancar("app", pk.ident);
  const up = new U.Actualizador(acc, silencio);
  const fin = await up.instalar(sv15);
  ok(fin === "FM-1_015" && flashDe(sv15).every((b, i) => b === dev.flash[i]), `de vuelta al V15 (${fin})`);
}

// 5. otros aparatos MIDI no se tocan
{
  const acc = new Acceso();
  let tocado = false;
  acc.conectar({ recibir: () => { tocado = true; } }, "Teclado Roland");
  const up = new U.Actualizador(acc, silencio);
  ok((await up.conectado()) === null && !tocado, "un teclado MIDI conectado no recibe nada");
}

console.log(fallos ? `${fallos} FALLAS` : "todo bien");
process.exit(fallos ? 1 : 0);
