#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// Arma el banco de fábrica de sonidos (docs/DISENO_0.6.md §4.4):
//   1. escribe los presets propios de BUNO-6 como presets de Vital (assets/presets/buno/*.vital, CC0)
//      — se abren en Vital, y sus ajustes exactos viajan en los comentarios ("bubba-buno: {...}");
//   2. baja los presets de la comunidad de assets/presets/banco.json en su commit fijado
//      (caché en assets/presets/vital/, fuera de git);
//   3. los convierte con web/bsnap.js a assets/presets/bsnap/*.bsnap;
//   4. escribe core/gen/bm_snap_rom.c con todos.
// Comprueba antes que el orden de los ajustes de web/bsnap.js sea el de core/bm_params.h.
//
//   node tools/gen_snaps.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { vitalToBsnap, MN, BN, BT, LIC } from '../web/bsnap.js';

const ROOT = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const P = (...a) => path.join(ROOT, ...a);

// ------------------------------------------------------------ el orden de los ajustes
function checkOrder() {
  const h = fs.readFileSync(P('core/bm_params.h'), 'utf8');
  for (const [pre, list] of [['MN', MN], ['BN', BN], ['BT', BT]]) {
    const m = new RegExp(`enum \\{ (${pre}_W1|${pre}_SAW|${pre}_L1)[^}]*\\}`, 's').exec(h);
    if (!m) throw new Error('no encuentro el enum ' + pre);
    const names = m[0].replace(/enum \{|\}/g, '').split(',').map(s => s.trim()).filter(Boolean)
      .filter(s => s !== `${pre}_NPRM`).map(s => s.replace(pre + '_', ''));
    if (names.join(',') !== list.join(','))
      throw new Error(`${pre}: web/bsnap.js no coincide con core/bm_params.h\n  C:  ${names}\n  JS: ${list}`);
  }
}

// ------------------------------------------------------------ BUNO-6: presets propios
// valores de Bubba (0..127); macro = [FREQ, RES, ENV, LFO] (el LFO al VCF); PWM es un ajuste
const BUNO = [
  ['BRASS 6', { SAW: 1, PULSE: 0, SUB: 20, HPF: 1, A: 45, D: 75, S: 85, R: 60, CHORUS: 0, PWM: 0, macro: [55, 20, 70, 0] }],
  ['STRINGS 6', { SAW: 1, PULSE: 0, SUB: 0, HPF: 2, A: 80, D: 90, S: 110, R: 85, CHORUS: 2, LRATE: 50, PWM: 0, macro: [90, 10, 10, 0] }],
  ['WARM PAD', { SAW: 1, PULSE: 1, PWMSRC: 0, SUB: 30, A: 85, D: 95, S: 100, R: 95, CHORUS: 1, LRATE: 45, PWM: 70, macro: [70, 25, 30, 0] }],
  ['PWM PAD', { SAW: 0, PULSE: 1, PWMSRC: 0, SUB: 25, A: 70, D: 90, S: 110, R: 90, CHORUS: 2, LRATE: 55, PWM: 90, macro: [85, 15, 15, 0] }],
  ['SUB BASS 6', { SAW: 1, PULSE: 0, SUB: 110, RANGE: 0, HPF: 0, A: 0, D: 60, S: 70, R: 30, CHORUS: 0, KEYT: 40, PWM: 0, macro: [40, 30, 60, 0] }],
  ['PLUCK 6', { SAW: 1, PULSE: 1, PWMSRC: 1, SUB: 30, A: 0, D: 62, S: 0, R: 55, CHORUS: 1, PWM: 30, macro: [45, 35, 90, 0] }],
  ['ORGAN 6', { SAW: 0, PULSE: 1, PWMSRC: 1, SUB: 90, HPF: 1, VCA: 1, A: 0, D: 60, S: 127, R: 20, CHORUS: 3, PWM: 0, macro: [110, 0, 0, 0] }],
  ['SOFT LEAD', { SAW: 1, PULSE: 0, SUB: 40, A: 10, D: 70, S: 100, R: 40, CHORUS: 1, LFODCO: 20, LDELAY: 80, LRATE: 70, PWM: 0, macro: [80, 30, 25, 0] }],
  ['SQUARE LEAD', { SAW: 0, PULSE: 1, PWMSRC: 1, SUB: 20, A: 5, D: 70, S: 110, R: 40, CHORUS: 0, LFODCO: 15, LDELAY: 70, LRATE: 72, PWM: 40, macro: [95, 20, 20, 0] }],
  ['STAB 6', { SAW: 1, PULSE: 0, SUB: 30, A: 0, D: 60, S: 20, R: 45, CHORUS: 2, PWM: 0, macro: [60, 25, 75, 0] }],
  ['NOISE SWEEP', { SAW: 0, PULSE: 0, NOISE: 110, SUB: 0, A: 90, D: 100, S: 40, R: 95, CHORUS: 1, LRATE: 30, PWM: 0, macro: [40, 80, 90, 60] }],
  ['BELL PAD 6', { SAW: 0, PULSE: 1, PWMSRC: 2, SUB: 0, HPF: 2, A: 0, D: 85, S: 50, R: 90, CHORUS: 2, PWM: 80, macro: [100, 50, 40, 0] }],
];

function waveB64(kind) {
  const f = new Float32Array(2048);
  for (let i = 0; i < 2048; i++) { const p = i / 2048; f[i] = kind === 'saw' ? 1 - 2 * p : p < 0.5 ? 1 : -1; }
  return Buffer.from(f.buffer).toString('base64');
}
const envSecs = (v) => 0.001 * Math.pow(10, v * 4 / 127);          // bm_env_s
const cutHz = (v) => 20 * Math.pow(2, v * 10 / 127);                 // bm_cut_hz

function bunoVital(name, p) {
  const wt = (nm, kind) => ({ name: nm, author: 'Bubba Machine', version: '1.0.5',
    groups: [{ components: [{ type: 'Wave Source', interpolation: 1, interpolation_style: 1, keyframes: [{ position: 0, wave_data: waveB64(kind) }] }] }] });
  const m = p.macro, midi = 69 + 12 * Math.log2(cutHz(m[0]) / 440);
  const s = {
    osc_1_on: p.SAW || p.PULSE ? 1 : 0, osc_1_level: 0.7, osc_1_transpose: (p.RANGE ?? 1) === 0 ? -12 : (p.RANGE ?? 1) === 2 ? 12 : 0,
    osc_2_on: p.SUB ? 1 : 0, osc_2_level: (p.SUB || 0) / 127, osc_2_transpose: ((p.RANGE ?? 1) === 0 ? -24 : -12),
    filter_1_on: 1, filter_1_model: 0, filter_1_style: 1, filter_1_cutoff: midi, filter_1_resonance: m[1] / 127,
    env_1_attack: Math.pow(envSecs(p.A ?? 10), 0.25), env_1_decay: Math.pow(envSecs(p.D ?? 70), 0.25),
    env_1_sustain: (p.S ?? 90) / 127, env_1_release: Math.pow(envSecs(p.R ?? 60), 0.25),
    env_2_attack: Math.pow(envSecs(p.A ?? 10), 0.25), env_2_decay: Math.pow(envSecs(p.D ?? 70), 0.25),
    env_2_sustain: (p.S ?? 90) / 127, env_2_release: Math.pow(envSecs(p.R ?? 60), 0.25),
    chorus_on: p.CHORUS ? 1 : 0, chorus_dry_wet: 0.5, chorus_voices: 2, polyphony: 6,
    modulations: [{ source: 'env_2', destination: 'filter_1_cutoff' }], modulation_1_amount: m[2] / 127 * 0.7,
    wavetables: [wt('Bubba ' + (p.SAW ? 'Saw' : 'Pulse'), p.SAW ? 'saw' : 'square'), wt('Bubba Sub', 'square'), wt('Bubba Saw', 'saw')],
  };
  if (p.NOISE) s.osc_3_on = 0;
  const tag = { ...p };
  return { author: 'Bubba Machine', comments: 'Bubba Machine factory sound for BUNO-6, CC0. bubba-buno: ' + JSON.stringify(tag),
    macro1: 'FREQ', macro2: 'RES', macro3: 'ENV', macro4: 'LFO', preset_name: name, preset_style: 'Keys', synth_version: '1.0.5', settings: s };
}

// ------------------------------------------------------------ main
async function getVital(repo, rel) {
  const cache = P('assets/presets/vital', repo.key, rel);
  if (fs.existsSync(cache)) return JSON.parse(fs.readFileSync(cache, 'utf8'));
  const url = repo.url.replace('https://github.com/', 'https://raw.githubusercontent.com/') + '/' + repo.commit + '/' + rel.split('/').map(encodeURIComponent).join('/');
  const r = await fetch(url);
  if (!r.ok) throw new Error(`${url}: ${r.status}`);
  const txt = await r.text();
  fs.mkdirSync(path.dirname(cache), { recursive: true });
  fs.writeFileSync(cache, txt);
  return JSON.parse(txt);
}

const slug = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');

async function main() {
  checkOrder();
  const bank = JSON.parse(fs.readFileSync(P('assets/presets/banco.json'), 'utf8'));
  const out = [];
  fs.mkdirSync(P('assets/presets/buno'), { recursive: true });
  fs.mkdirSync(P('assets/presets/bsnap'), { recursive: true });
  for (const [name, p] of BUNO) {
    const v = bunoVital(name, p);
    fs.writeFileSync(P('assets/presets/buno', slug(name) + '.vital'), JSON.stringify(v));
    out.push({ name, machine: 'BUNO-6', author: 'BUBBA', lic: LIC.OWN, json: v });
  }
  for (const e of bank.presets) {
    const repo = { ...bank.repos[e.repo], key: e.repo };
    const json = await getVital(repo, e.path);
    out.push({ name: e.name, machine: e.machine, author: e.author, lic: LIC[repo.license], json });
  }
  // orden: B-MINI, BITTAL, BUNO-6 (PRESETS los recorre en este orden)
  const ord = { 'B-MINI': 0, 'BITTAL': 1, 'BUNO-6': 2 };
  out.sort((a, b) => ord[a.machine] - ord[b.machine]);
  let c = `/* SPDX-License-Identifier: GPL-3.0-or-later
 * Generado por tools/gen_snaps.mjs: no editar a mano.
 * Banco de fábrica de sonidos (docs/DISENO_0.6.md §4.4). Los presets de la comunidad son de sus
 * autores (assets/presets/banco.json: repositorio, commit y licencia de cada uno). */
#include "../bm_snap.h"

`;
  const refs = [];
  let total = 0;
  for (const [i, e] of out.entries()) {
    const { bytes, report } = vitalToBsnap(e.json, { machine: e.machine, name: e.name, author: e.author, lic: e.lic });
    if (e.machine === 'BITTAL' && !bytes[5]) throw new Error(`${e.name}: no oscillator sounds (all levels at 0?)`);
    fs.writeFileSync(P('assets/presets/bsnap', slug(e.name) + '.bsnap'), bytes);
    total += bytes.length;
    c += `/* ${e.name} · ${e.machine} · ${e.author} · #${report.id} · ${bytes.length} B */\n`;
    c += `static const uint8_t s${i}[${bytes.length}] = {`;
    for (let k = 0; k < bytes.length; k++) c += (k % 24 ? '' : '\n    ') + bytes[k] + ',';
    c += '\n};\n';
    refs.push(`{s${i}, ${bytes.length}}`);
    console.log(`${e.machine.padEnd(7)} ${e.name.padEnd(12)} ${String(bytes.length).padStart(5)} B  #${report.id}  ${report.cpu}` +
      (report.dropped.length ? `\n          fuera: ${report.dropped.join(', ')}` : ''));
  }
  c += `\nconst bm_snap_ref bm_snap_rom[] = {\n    ${refs.join(',\n    ')}\n};\nconst int bm_snap_rom_n = ${refs.length};\n`;
  fs.writeFileSync(P('core/gen/bm_snap_rom.c'), c);
  console.log(`${out.length} sonidos, ${total} bytes en core/gen/bm_snap_rom.c`);
}

main().catch(e => { console.error(e.message || e); process.exit(1); });
