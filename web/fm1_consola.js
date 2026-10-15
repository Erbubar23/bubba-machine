// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// La consola de Bubba Machine por USB-MIDI, desde el navegador (hal/fm1/fm1_consola.c):
//   pedido F0 7D 42 4D cmd nibbles… F7 · respuesta F0 7D 42 4D (cmd|0x40) nibbles… F7
// La usan la plataforma de control (web/control.html) y el instalador. Órdenes:
//   01 estado (palabras de 32 bits, FM1_STAT_*) · 20 toma · 21 idioma · 22 sonido · 23 informe
// Las que llevan datos van en trozos de 256 bytes: sub 0 = empezar, 1 = datos, 2 = terminar.

const PUERTO = /bubba|fm-?1/i;

export function puertos(access) {
  const pick = (it) => [...it.values()].find((p) => PUERTO.test(p.name || "") && p.state !== "disconnected");
  return { out: pick(access.outputs), inp: pick(access.inputs) };
}

function sysex(cmd, data) {
  const m = [0xF0, 0x7D, 0x42, 0x4D, cmd];
  for (const b of data) m.push(b >> 4, b & 15);
  m.push(0xF7);
  return m;
}

// Un pedido a la vez: la respuesta se reconoce por la orden, así que dos pedidos cruzados se pisarían.
let cola = Promise.resolve();

function pedirPuerto(out, inp, cmd, data, ms) {
  return new Promise((ok, bad) => {
    const prev = inp.onmidimessage;
    const t = setTimeout(() => { inp.onmidimessage = prev; bad(new Error("el FM-1 no contestó")); }, ms);
    inp.onmidimessage = (ev) => {
      const d = ev.data;
      if (d.length > 5 && d[1] === 0x7D && d[2] === 0x42 && d[3] === 0x4D && d[4] === (cmd | 0x40)) {
        clearTimeout(t); inp.onmidimessage = prev;
        const r = [];
        for (let i = 5; i + 1 < d.length - 1; i += 2) r.push((d[i] << 4) | d[i + 1]);
        ok(r);
      }
    };
    out.send(sysex(cmd, data));
  });
}

// un pedido suelto: devuelve los bytes de la respuesta
export function pedir(access, cmd, data = [], ms = 3000) {
  const job = cola.then(() => {
    const { out, inp } = puertos(access);
    if (!out || !inp) throw new Error("no encuentro el FM-1 con Bubba Machine");
    return pedirPuerto(out, inp, cmd, data, ms);
  });
  cola = job.catch(() => {});
  return job;
}

const be32 = (v) => [v >>> 24, (v >> 16) & 255, (v >> 8) & 255, v & 255];

// manda bytes con una orden de trozos (20 toma, 21 idioma, 22 sonido); avance(0..1) opcional.
// fin = [a32, b16] del paso 2 (la toma lleva ahí su largo y su BPM).
export async function enviar(access, cmd, bytes, avance, fin = [0, 0]) {
  const paso = async (sub, a, b, body) => {
    const r = await pedir(access, cmd, [sub, ...be32(a), (b >> 8) & 255, b & 255, ...body]);
    if (r[1] !== 1) throw new Error("el FM-1 rechazó el paso " + sub + ": ¿está sonando? (STOP)");
  };
  await paso(0, 0, 0, []);
  if (cmd === 0x20) await new Promise((r) => setTimeout(r, 200));      /* el audio suelta la arena */
  for (let o = 0; o < bytes.length; o += 256) {
    await paso(1, o, 0, bytes.subarray(o, o + 256));
    if (avance) avance(o / bytes.length);
  }
  await paso(2, fin[0], fin[1], []);
}

// espera a que el FM-1 con Bubba Machine aparezca (después de instalar, arranca y enumera de nuevo)
export async function esperar(access, ms = 15000) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    try { await pedir(access, 0x01, [], 1500); return true; } catch (_) { /* todavía arrancando */ }
    await new Promise((r) => setTimeout(r, 700));
  }
  return false;
}
