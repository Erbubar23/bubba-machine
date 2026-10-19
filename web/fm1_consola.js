// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// La consola de Bubba Machine por USB-MIDI, desde el navegador (hal/fm1/fm1_consola.c):
//   pedido F0 7D 42 4D cmd nibbles… F7 · respuesta F0 7D 42 4D (cmd|0x40) nibbles… F7
// La usa la plataforma de control (web/control.html; control_mock.js la imita con ?mock=1). Órdenes:
//   01 estado (palabras de 32 bits, FM1_STAT_*; tools/fm1/consola.py CAMPOS) · 07 off16 registro de texto
//   20 toma · 21 idioma (sub 3 = quitar) · 22 sonido (sub 4 idx = borrar uno del usuario)
//   23 sub idx: 0 informe (bm_app_report), 1 toma idx, 2 sonido del usuario idx, 3 borrar la toma idx
// Las que llevan datos van en trozos de 256 bytes: sub 0 = empezar, 1 = datos, 2 = terminar.
// Los errores llevan code: 'noport' (no hay puerto del FM-1), 'timeout' (no contestó), 'rejected'.

const PUERTO = /bubba|fm-?1/i;

export function puertos(access) {
  const pick = (it) => [...it.values()].find((p) => PUERTO.test(p.name || "") && p.state !== "disconnected");
  return { out: pick(access.outputs), inp: pick(access.inputs) };
}

function fallo(code, msg) { const e = new Error(msg); e.code = code; return e; }

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
    const t = setTimeout(() => { inp.onmidimessage = prev; bad(fallo("timeout", "the FM-1 did not answer")); }, ms);
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
    if (!out || !inp) throw fallo("noport", "no FM-1 MIDI port");
    return pedirPuerto(out, inp, cmd, data, ms);
  });
  cola = job.catch(() => {});
  return job;
}

const be32 = (v) => [v >>> 24, (v >> 16) & 255, (v >> 8) & 255, v & 255];

// manda bytes con una orden de trozos (20 toma, 21 idioma, 22 sonido); avance(0..1) opcional.
// fin = [a32, b16] del paso 2 (la toma lleva ahí su largo y su BPM). Un rechazo lleva e.step.
export async function enviar(access, cmd, bytes, avance, fin = [0, 0]) {
  const paso = async (sub, a, b, body) => {
    const r = await pedir(access, cmd, [sub, ...be32(a), (b >> 8) & 255, b & 255, ...body]);
    if (r[1] !== 1) { const e = fallo("rejected", "the FM-1 refused step " + sub); e.step = sub; throw e; }
  };
  await paso(0, 0, 0, []);
  if (cmd === 0x20) await new Promise((r) => setTimeout(r, 200));      /* el audio suelta la arena */
  for (let o = 0; o < bytes.length; o += 256) {
    await paso(1, o, 0, bytes.subarray(o, o + 256));
    if (avance) avance(o / bytes.length);
  }
  await paso(2, fin[0], fin[1], []);
  if (avance) avance(1);
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
