// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// Pruebas del conversor de sonidos (web/bsnap.js): presets de Vital y bancos .syx de la DX7.
//   node tests/test_bsnap.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { vitalToBsnap, syxToBsnap, packTable, ENG } from '../web/bsnap.js';

const ROOT = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
let checks = 0, fails = 0;
const CHECK = (c, msg) => { checks++; if (!c) { fails++; console.log('  FALLO ' + msg); } };

// 1. una tabla de una sierra: 256 armónicos con caída 1/n y un solo frame
const saw = new Float32Array(2048);
for (let i = 0; i < 2048; i++) saw[i] = 1 - 2 * i / 2048;
const t = packTable([saw]);
CHECK(t[0] === 1 && (t[1] === 0 || t[1] > 200), 'sierra: 1 frame y muchos armónicos');
CHECK(t[2] === 255 && Math.abs((255 - t[4]) * 0.375 - 6.02) < 0.5, 'sierra: el segundo armónico a -6 dB');

// 2. los presets propios de BUNO-6: sus ajustes viajan en los comentarios
for (const f of fs.readdirSync(path.join(ROOT, 'assets/presets/buno'))) {
  const j = JSON.parse(fs.readFileSync(path.join(ROOT, 'assets/presets/buno', f), 'utf8'));
  const { bytes, report } = vitalToBsnap(j, { machine: 'BUNO-6' });
  CHECK(bytes.length === 88 && bytes[4] === ENG['BUNO-6'] && report.kept.includes('BUNO-6 settings'), 'BUNO-6 ' + f);
}

// 3. un banco .syx: se empaqueta un banco a mano y se desempaqueta con el conversor
const voices = [];
const packed = new Uint8Array(4096);
for (let v = 0; v < 32; v++) {
  const u = new Uint8Array(156);
  for (let op = 0; op < 6; op++) {
    const q = op * 21;
    for (let i = 0; i < 4; i++) { u[q + i] = (v * 3 + op * 7 + i) % 100; u[q + 4 + i] = (v + op + i * 11) % 100; }
    u[q + 8] = 39; u[q + 9] = v % 100; u[q + 10] = op * 5; u[q + 11] = v & 3; u[q + 12] = op & 3; u[q + 13] = (v + op) & 7;
    u[q + 14] = op & 3; u[q + 15] = v & 7; u[q + 16] = 99 - v; u[q + 17] = op & 1; u[q + 18] = (v + op) & 31; u[q + 19] = v % 100; u[q + 20] = (op + v) % 15;
    const s = packed.subarray(v * 128 + op * 17, v * 128 + op * 17 + 17);
    for (let i = 0; i < 11; i++) s[i] = u[q + i];
    s[11] = u[q + 11] | (u[q + 12] << 2); s[12] = u[q + 13] | (u[q + 20] << 3); s[13] = u[q + 14] | (u[q + 15] << 2);
    s[14] = u[q + 16]; s[15] = u[q + 17] | (u[q + 18] << 1); s[16] = u[q + 19];
  }
  const p = packed.subarray(v * 128, v * 128 + 128);
  for (let i = 0; i < 8; i++) { u[126 + i] = 50 + i; p[102 + i] = 50 + i; }
  u[134] = v; p[110] = v;
  u[135] = v & 7; u[136] = 1; p[111] = (v & 7) | 8;
  for (let i = 0; i < 4; i++) { u[137 + i] = 10 + i; p[112 + i] = 10 + i; }
  u[141] = 1; u[142] = 3; u[143] = 5; p[116] = 1 | (3 << 1) | (5 << 4);
  u[144] = 24; p[117] = 24;
  const nm = ('VOICE ' + v).padEnd(10);
  for (let i = 0; i < 10; i++) { u[145 + i] = nm.charCodeAt(i); p[118 + i] = nm.charCodeAt(i); }
  u[155] = 0x3F;
  voices.push(u);
}
let sum = 0;
for (const b of packed) sum = (sum + b) & 0x7F;
const syx = new Uint8Array(4104);
syx.set([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]); syx.set(packed, 6); syx[4102] = (128 - sum) & 0x7F; syx[4103] = 0xF7;
const { bytes, report } = syxToBsnap(syx);
CHECK(bytes.length === 88 + 32 * 156 && bytes[4] === 4 && !report.approx.length, 'banco: tamaño, máquina y suma');
let same = true;
for (let v = 0; v < 32; v++) for (let i = 0; i < 156; i++) if (bytes[88 + v * 156 + i] !== voices[v][i]) { same = false; if (fails < 3) console.log('  voz', v, 'byte', i, bytes[88 + v * 156 + i], voices[v][i]); }
CHECK(same, 'banco: las 32 voces sin empaquetar coinciden');
CHECK(report.names[7] === 'VOICE 7', 'banco: los nombres');

console.log(`${checks} comprobaciones, ${fails} fallos`);
process.exit(fails ? 1 : 0);
