// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
//
// Actualización del FM-1 por USB-MIDI (el protocolo de M-UPGRADE), desde el navegador.
//
// Paso 1: el firmware que corre pide partes del paquete, prepara el cargador de
// actualización y reinicia en él (el FM-1 vuelve a aparecer como "ota-FM-1_015").
// Paso 2: el cargador pide la imagen completa y la escribe. En los dos pasos es el
// FM-1 el que pide (lecturas con dirección y largo) y aquí solo se contesta.
//
// Datos del protocolo: FM-1-RE (WTFPL), docs/io/11-ota-protocol.md y
// docs/io/12-v15-reflash-proof.md. El código es propio.

export const V15_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a";
// los primeros 16 KiB de flash.bin del V15 (cabecera del flash, SPL e isd_config): el cargador
// oficial los reescribe, así que un paquete que no los traiga idénticos no se instala
export const V15_HEAD_SHA256 = "d9f43191777656de01c7bed4f9e9ba2e34e94832c5d2ed02c3fc7ab7a6d9bd67";
// el cargador de actualización oficial de M-VAVE (ota.bin): cabecera de 32 bytes
const LOADER_HEAD = "aaeb815820000000e14d0000410000007573625f6869645f6f74612e62696e00";

const HANDSHAKE = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const ADDR_VERIFIED = 0xE0000000;      // paso 1 terminado: el FM-1 pasa al cargador
const ADDR_WRITTEN = 0xF0000000;       // paso 2 terminado: imagen escrita
const MAX_REQ = 512;
// nombres de puerto que pueden ser un FM-1: nunca se manda nada a otros aparatos
export const PORT_RE = /fm-?1|ota|usb-?midi|composite|bubba|x0x|felucca/i;

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export class FalloFM1 extends Error {
  constructor(code, msg) { super(msg); this.code = code; }
}

// ------------------------------------------------------------------ bytes ---
export function pack7(data) {
  const out = [];
  let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7F);
  return out;
}

export function unpack7(s) {
  const out = [];
  let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; }
  }
  return out;
}

const T16 = (() => {
  const t = new Uint16Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i << 8;
    for (let k = 0; k < 8; k++) c = c & 0x8000 ? (c << 1) ^ 0x1021 : c << 1;
    t[i] = c & 0xFFFF;
  }
  return t;
})();

export function crc16(d, crc = 0) {
  for (let i = 0; i < d.length; i++) crc = ((crc << 8) & 0xFFFF) ^ T16[((crc >> 8) ^ d[i]) & 0xFF];
  return crc;
}

function enc(buf, off, n, key = 0xFFFF) {
  for (let i = 0; i < n; i++) {
    buf[off + i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
}

const rd16 = (b, o) => b[o] | (b[o + 1] << 8);
const rd32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const hex = (b) => Array.from(b, (x) => x.toString(16).padStart(2, "0")).join("");

export async function sha256(bytes) {
  return hex(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)));
}

// --------------------------------------------------------------- paquetes ---
// identidad: un marcador después de cada uno de los primeros 20 bloques de 0x30 bytes
export function identidadPaquete(raw) {
  let s = "";
  for (let i = 0; i < 20; i++) {
    const m = raw[i * 0x30 + 0x2F];
    if (m !== 0x7D) s += String.fromCharCode((m - i - 1) & 0xFF);
  }
  return s;
}

// lo que el FM-1 pide: el archivo sin los 20 marcadores
export function imagenLogica(raw) {
  const out = new Uint8Array(raw.length - 20);
  for (let i = 0; i < 20; i++) out.set(raw.subarray(i * 0x30, i * 0x30 + 0x2F), i * 0x2F);
  out.set(raw.subarray(960), 940);
  return out;
}

// Revisa un .fwsc entero antes de ofrecerlo: CRC de la cabecera y de cada parte que el FM-1
// escribe, el cargador oficial y la cabecera del flash del V15. Devuelve {ident, image, sha,
// stock} o lanza FalloFM1("paquete", motivo).
export async function revisarPaquete(raw) {
  const bad = (m) => { throw new FalloFM1("paquete", m); };
  if (!(raw instanceof Uint8Array) || raw.length < 4096) bad("no es un paquete del FM-1");
  const sha = await sha256(raw);
  const ident = identidadPaquete(raw);
  const img = imagenLogica(raw);
  const h = img.slice(0, 0x40);
  enc(h, 0, 0x40);
  if (crc16(h.subarray(2, 0x40)) !== rd16(h, 0)) bad("la cabecera del paquete está dañada");
  const n = rd16(h, 8);
  if (n < 2 || n > 16 || rd32(h, 4) !== img.length) bad("la cabecera del paquete no cuadra con el archivo");
  if (crc16(img.subarray(0x40, 0x40 + n * 0x50)) !== rd16(h, 2)) bad("la lista del paquete está dañada");
  const ents = {};
  for (let k = 0; k < n; k++) {
    const e = img.slice(0x40 + k * 0x50, 0x90 + k * 0x50);
    enc(e, 0, 0x50);
    ents[rd16(e, 0)] = { dcrc: rd16(e, 4), off: rd32(e, 8), size: rd32(e, 12) };
  }
  const fl = ents[0], ota = ents[100];
  if (!fl || !ota) bad("al paquete le falta flash.bin o el cargador");
  for (const e of [fl, ota]) {
    if (e.off + e.size > img.length) bad("una parte del paquete se sale del archivo");
    if (crc16(img.subarray(e.off, e.off + e.size)) !== e.dcrc) bad("una parte del paquete está dañada");
  }
  if (hex(img.subarray(ota.off, ota.off + 32)) !== LOADER_HEAD) bad("el cargador no es el oficial de M-VAVE");
  if (await sha256(img.slice(fl.off, fl.off + 0x4000)) !== V15_HEAD_SHA256) bad("el paquete cambia el arranque del FM-1");
  if (!/^FM-1_\d+$/.test(ident)) bad(`identidad desconocida (${ident || "vacía"})`);
  return { ident, image: img, sha, stock: sha === V15_SHA256 };
}

// ------------------------------------------------------------------ SysEx ---
export function respuesta(addr, data, fl = 0) {
  const n = data.length;
  const body = [0x00, 0x59, 0x30, (n + 8) & 0xFF, ((n + 8) >> 8) & 0xFF, ((n + 8) >> 16) & 0xFF, fl,
    addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
    n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF];
  for (let i = 0; i < n; i++) body.push(data[i]);
  let s = 0;
  for (let i = 6; i < body.length; i++) s += body[i];
  body.push(~s & 0xFF);
  return [0xF0, ...pack7(body), 0xF7];
}

export function pedido(pkt) {
  if (pkt.length < 4 || pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length !== 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  if ((u[3] | (u[4] << 8) | (u[5] << 16)) !== 8) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  return { fl: u[6], addr: rd32(u, 7), len: u[11] | (u[12] << 8) | (u[13] << 16) };
}

// respuesta al saludo: "FM-1_015", "ota-FM-1_015", "FM-1_003"...
export function identidad(pkt) {
  if (pkt.length < 4 || pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const d = unpack7(pkt.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  const txt = String.fromCharCode(...d.slice(6, 33)).replace(/\0+$/, "");
  const m = /^([^_\0]+)_(\d+)$/.exec(txt);
  if (!m) return null;
  return { text: txt, model: m[1], version: parseInt(m[2], 10), ota: /^ota-/i.test(m[1]) };
}

// ------------------------------------------------------------------ enlace ---
class Enlace {
  constructor(input, output) {
    this.input = input; this.output = output; this.q = []; this.wake = null;
    input.onmidimessage = (ev) => {
      if (ev.data[0] !== 0xF0) return;
      this.q.push(Array.from(ev.data));
      if (this.wake) { const w = this.wake; this.wake = null; w(); }
    };
  }
  get perdido() { return this.input.state === "disconnected" || this.output.state === "disconnected"; }
  send(b) { try { this.output.send(b); return true; } catch (_) { return false; } }
  drain() { this.q.length = 0; }
  async read(ms) {
    if (!this.q.length) {
      let t;
      await Promise.race([new Promise((r) => { this.wake = r; }), new Promise((r) => { t = setTimeout(r, ms); })]);
      clearTimeout(t);
      this.wake = null;
    }
    return this.q.shift() || null;
  }
  close() { this.input.onmidimessage = null; }
}

async function saludar(link, tries = 3) {
  link.drain();
  for (let t = 0; t < tries; t++) {
    if (!link.send(HANDSHAKE)) return null;
    const end = Date.now() + 1000;
    while (Date.now() < end) {
      const p = await link.read(end - Date.now());
      if (!p) break;
      const id = identidad(p);
      if (id) return id;
    }
  }
  return null;
}

// ------------------------------------------------------------ actualizador ---
export class Actualizador {
  // access: MIDIAccess con sysex; log(texto) opcional; portRe: qué puertos se prueban
  constructor(access, log, portRe = PORT_RE) {
    this.access = access; this.log = log || (() => {}); this.portRe = portRe;
  }

  // el primer par entrada/salida (mismo nombre) que contesta el saludo y pasa el filtro
  // this.vistos queda con lo que se encontró en cada puerto, para explicar por qué no aparece
  async buscar(filtro) {
    const outs = [...this.access.outputs.values()];
    this.vistos = [];
    for (const input of this.access.inputs.values()) {
      if (input.state === "disconnected") continue;
      const name = input.name || "";
      if (!this.portRe.test(name)) { this.vistos.push({ name, estado: "otro" }); continue; }
      const output = outs.find((o) => o.name === name) || (outs.filter((o) => this.portRe.test(o.name || "")).length === 1 ? outs.find((o) => this.portRe.test(o.name || "")) : null);
      if (!output) { this.vistos.push({ name, estado: "sinsalida" }); continue; }
      try { await input.open(); await output.open(); } catch (_) { this.vistos.push({ name, estado: "ocupado" }); continue; }
      const link = new Enlace(input, output);
      const id = await saludar(link, 2);
      if (id && (!filtro || filtro(id))) { this.vistos.push({ name, estado: "ok", id }); return { link, id, name }; }
      this.vistos.push({ name, estado: id ? "filtrado" : "mudo", id });
      link.close();
    }
    return null;
  }

  async esperar(filtro, ms) {
    const end = Date.now() + ms;
    while (Date.now() < end) {
      const d = await this.buscar(filtro);
      if (d) return d;
      await sleep(1000);
    }
    return null;
  }

  // contesta pedidos hasta que el FM-1 pide `fin`; corta si se desconecta o se calla `idleMs`
  async servir(link, image, fin, idleMs, progreso) {
    let n = 0, last = Date.now();
    for (;;) {
      const pkt = await link.read(1000);
      if (!pkt) {
        if (link.perdido) return { n, ok: false, perdido: true };
        if (Date.now() - last > idleMs) return { n, ok: false };
        continue;
      }
      const r = pedido(pkt);
      if (!r) continue;
      last = Date.now();
      if (r.addr === ADDR_VERIFIED || r.addr === ADDR_WRITTEN) {
        link.send(respuesta(r.addr, [...new TextEncoder().encode("success"), 0]));
        if (r.addr === fin) return { n, ok: true };
        continue;
      }
      if (r.len > MAX_REQ || r.addr + r.len > image.length) {
        throw new FalloFM1("pedido", `el FM-1 pidió algo fuera del paquete (0x${r.addr.toString(16)} + ${r.len}); se detuvo sin contestar`);
      }
      if (!link.send(respuesta(r.addr, image.subarray(r.addr, r.addr + r.len), r.fl))) return { n, ok: false, perdido: true };
      n++;
      if (progreso) progreso(n, r.addr);
    }
  }

  // qué hay conectado ahora (sin tocar nada)
  async conectado() {
    const d = await this.buscar(null);
    if (!d) return null;
    d.link.close();
    return d.id;
  }

  // instalación completa: firmware actual -> cargador -> firmware nuevo
  async instalar(pkg, paso) {
    const step = paso || (() => {});
    const dev = await this.buscar((id) => !id.ota);
    if (!dev) throw new FalloFM1("noencontrado", "no se encontró el FM-1 (cable USB de datos, y ningún otro programa usándolo)");
    if (dev.id.model !== "FM-1") { dev.link.close(); throw new FalloFM1("modelo", `el aparato dice ser ${dev.id.text}, no un FM-1`); }
    step("inicio", dev.id.text);
    this.log(`conectado: ${dev.id.text} (${dev.name})`);
    dev.link.send(UPGRADE);
    await sleep(2000);
    const s1 = await this.servir(dev.link, pkg.image, ADDR_VERIFIED, 8000, (n) => step("revisa", n));
    dev.link.close();
    this.log(`paso 1: ${s1.n} pedidos, ${s1.ok ? "aceptado" : "sin terminar"}`);
    if (!s1.ok) {
      throw new FalloFM1(s1.perdido ? "perdido" : "rechazado",
        s1.perdido ? "el FM-1 se desconectó en el paso 1: no se escribió nada"
          : `el FM-1 dejó de pedir datos después de ${s1.n} pedidos: no aceptó el paquete y no se escribió nada`);
    }
    step("cargador");
    await sleep(3000);
    const ota = await this.esperar((id) => id.ota, 30000);
    if (!ota) throw new FalloFM1("sincargador", "el cargador de actualización no apareció. Desconecta y conecta el cable USB y usa «Continuar»: el FM-1 se queda en modo actualización hasta terminar.");
    return this.escribir(ota, pkg, step);
  }

  // paso 2 con el cargador ya conectado
  async escribir(ota, pkg, step) {
    this.log(`cargador: ${ota.id.text}`);
    ota.link.send(UPGRADE);
    await sleep(2000);
    const s2 = await this.servir(ota.link, pkg.image, ADDR_WRITTEN, 180000, (n) => step("escribe", n));
    ota.link.close();
    this.log(`paso 2: ${s2.n} pedidos, ${s2.ok ? "terminado" : "sin terminar"}`);
    if (!s2.ok) throw new FalloFM1(s2.perdido ? "perdido" : "detenido", `la escritura se cortó después de ${s2.n} pedidos. Conecta de nuevo y usa «Continuar» con el mismo archivo.`);
    step("reinicio");
    await sleep(3000);
    const back = await this.esperar((id) => !id.ota, 40000);
    if (!back) throw new FalloFM1("novuelve", "se escribió, pero el FM-1 no volvió a aparecer: apágalo y préndelo");
    back.link.close();
    step("listo", back.id.text);
    return back.id.text;
  }

  // el FM-1 ya está en modo actualización (cargador): terminar la escritura
  async continuar(pkg, paso) {
    const ota = await this.buscar((id) => id.ota);
    if (!ota) throw new FalloFM1("sincargador", "no hay un FM-1 en modo actualización conectado");
    return this.escribir(ota, pkg, paso || (() => {}));
  }
}
