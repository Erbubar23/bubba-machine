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

// ------------------------------------------------------------ presets propios (0.9: sonidos históricos)
// Las fuentes de cada uno (panel publicado, receta o diseño propio) están en assets/presets/SOURCES.md.
// Tiempos y valores de Bubba: bm_env_s(v) = 0.001 * 10^(4 v / 127) s, bm_lfo_hz(v) = 0.05 * 2^(9 v / 127).
const rnd127 = (x) => Math.max(0, Math.min(127, Math.round(x)));
const envV = (secs) => rnd127(Math.log10(Math.max(secs, 0.001) / 0.001) * 127 / 4);
const lfoV = (hz) => rnd127(Math.log2(Math.max(hz, 0.05) / 0.05) * 127 / 9);
const s127 = (s) => rnd127(s * 12.7);                                  // slider 0..10

// B-MINI desde el panel del Minimoog: osc [pie, onda, mezcla 0-10], cutoff -5..+5, emphasis y contour
// 0-10, kb 0 / 1 (KB1) / 2 (KB2) / 3 (los dos), contornos [ataque ms, decay ms, sustain 0-10] y el
// interruptor DECAY (la salida = el decay; si no, ~10 ms), glide 0-10, lfo (el osc 3 como LFO)
const FEET = { "32'": 0, "16'": 1, "8'": 2, "4'": 3, "2'": 4 };
const MW = { TRI: 0, SHARK: 1, SAW: 2, SQR: 3, WIDE: 4, NARROW: 5 };
function mini(p) {
  const o = p.osc, P = { TUNE: 64, MODE: 0, DRIVE: p.drive ?? 30, VELF: 15, VELA: 20, NOISE: s127(p.noise ?? 0) };
  for (let i = 0; i < 3; i++) {
    const [ft, w, m] = o[i] || ["8'", 'SAW', 0];
    P['R' + (i + 1)] = FEET[ft]; P['W' + (i + 1)] = MW[w]; P['L' + (i + 1)] = rnd127(m * 11.5);
  }
  P.T2 = rnd127(64 + (p.t2 ?? 0) * 63 / 7); P.T3 = rnd127(64 + (p.t3 ?? 0) * 63 / 7);   // semitonos (±7)
  P.KEYT = [0, 42, 85, 127][p.kb ?? 2];
  const rel = (sw, d) => sw ? envV(d / 1000) : envV(0.01);
  const [fa, fd, fs] = p.f, [la, ld, ls] = p.l;
  P.FA = envV(fa / 1000); P.FD = envV(fd / 1000); P.FS = s127(fs); P.FR = rel(p.decaySw, fd);
  P.AA = envV(la / 1000); P.AD = envV(ld / 1000); P.AS = s127(ls); P.AR = rel(p.decaySw, ld);
  P.GLIDE = p.glide ? envV(0.01 * Math.pow(10, p.glide / 5)) : 0;
  if (p.lfo) { P.LRATE = lfoV(p.lfo.hz); P.LAMT = p.lfo.amt; P.LDEST = p.lfo.dest === 'pitch' ? 0 : 1; } else { P.LAMT = 0; P.LDEST = 1; }
  P.macro = [rnd127(18 + (p.cut + 5) * 10.9), s127(p.emph), s127(p.contour), 64];   // -5 ~ 60 Hz, +5 = 20 kHz
  return P;
}

const MINI = [
  ['WIZARD BASS', mini({ osc: [["32'", 'SAW', 8], ["16'", 'SAW', 8]], t2: 0.15, cut: 0, emph: 0, contour: 8, kb: 2, f: [0, 120, 0], l: [0, 300, 4], glide: 4 })],
  ['FAT UNISON', mini({ osc: [["32'", 'SAW', 10], ["16'", 'SAW', 10], ["16'", 'SAW', 10]], t2: 0.1, t3: -0.1, cut: -2, emph: 5, contour: 3.5, kb: 2, f: [0, 300, 0], l: [0, 600, 5] })],
  ['TUTTI BRASS', mini({ osc: [["8'", 'SAW', 5], ["8'", 'SAW', 7], ["8'", 'SAW', 9]], t2: 0.1, t3: -0.1, cut: -3, emph: 0, contour: 4, kb: 2, f: [400, 400, 10], l: [200, 400, 10], decaySw: true })],
  ['RASP BRASS', mini({ osc: [["4'", 'SAW', 5]], cut: -5, emph: 2, contour: 6.5, kb: 3, f: [600, 800, 5], l: [100, 400, 10], lfo: { hz: 5, amt: 25, dest: 'cut' } })],
  ['SLIDE BONE', mini({ osc: [["8'", 'SAW', 5]], cut: -2.5, emph: 2, contour: 4.5, kb: 2, f: [500, 400, 8], l: [400, 300, 8], glide: 3, lfo: { hz: 4.5, amt: 30, dest: 'pitch' } })],
  ['NOISE TUBA', mini({ osc: [["32'", 'SAW', 5]], noise: 1, cut: -3, emph: 0, contour: 4, kb: 2, f: [400, 400, 8], l: [200, 350, 7], decaySw: true })],
  ['OCTAVE GLIDE', mini({ osc: [["16'", 'WIDE', 2], ["2'", 'WIDE', 8], ["16'", 'WIDE', 2]], cut: -1, emph: 6, contour: 3, kb: 2, f: [0, 800, 0], l: [0, 400, 10], glide: 5 })],
  ['FIFTHS', mini({ osc: [["8'", 'SAW', 8], ["8'", 'SAW', 8]], t2: 7, cut: -2, emph: 0, contour: 0, kb: 2, f: [0, 600, 10], l: [0, 600, 0], glide: 5 })],
  ['TLR SOUND', mini({ osc: [["16'", 'WIDE', 8], ["2'", 'WIDE', 8]], noise: 8, cut: -3, emph: 0, contour: 0, kb: 2, f: [0, 800, 10], l: [0, 800, 5], glide: 5 })],
  ['COLD FRIEND', mini({ osc: [["16'", 'SAW', 8], ["16'", 'SAW', 8], ["16'", 'SAW', 8]], t2: 0.1, t3: -0.1, cut: 0, emph: 3, contour: 5, kb: 2, f: [0, 600, 10], l: [0, 600, 10], decaySw: true })],
  ['MUTANT ARP', mini({ osc: [["16'", 'SQR', 8], ["16'", 'SQR', 8]], t2: 7, cut: 1, emph: 0, contour: 5, kb: 2, f: [0, 600, 10], l: [0, 600, 10], glide: 8 })],
  ['SOFT HORN', mini({ osc: [["8'", 'SAW', 8]], cut: -5, emph: 0, contour: 6, kb: 2, f: [600, 400, 9], l: [250, 1000, 7], glide: 6, lfo: { hz: 4, amt: 20, dest: 'cut' } })],
  ['WHINE LEAD', mini({ osc: [["2'", 'SAW', 10], ["2'", 'SQR', 10], ["8'", 'SAW', 7]], t2: 0.15, cut: 1, emph: 3, contour: 0, kb: 1, f: [0, 200, 10], l: [0, 200, 10], glide: 5 })],
  ['MOTOR RIFF', mini({ osc: [["16'", 'SAW', 8], ["8'", 'SQR', 6]], t2: 0.1, cut: 0, emph: 2, contour: 4, kb: 2, f: [0, 400, 4], l: [0, 300, 8] })],
  ['HIGHWAY BASS', mini({ osc: [["16'", 'SAW', 8], ["32'", 'TRI', 6]], cut: -2, emph: 3, contour: 3, kb: 2, f: [0, 250, 2], l: [0, 350, 6] })],
  ['BENDER LEAD', mini({ osc: [["8'", 'SAW', 10], ["8'", 'SQR', 8]], t2: 0.15, drive: 95, cut: 1.5, emph: 2, contour: 2, kb: 2, f: [0, 300, 8], l: [5, 300, 10], glide: 2 })],
];

// BUNO-6 desde los deslizadores del Juno (0-10): lfo [rate, delay], dco [lfo, pwm, sub, noise], hpf (el
// nuestro, 0-3), vcf [freq, res, env, lfo, kybd], adsr [a, d, s, r]; wave 'S' sierra, 'P' pulso, 'PS'
const PWMM = { LFO: 0, MAN: 1, ENV: 2 };
function juno(p) {
  const [lr, ld] = p.lfo, [dl, pw, sub, nz] = p.dco, [fq, rs, ev, lf, kb] = p.vcf, [a, d, s, r] = p.adsr;
  const P = {
    SAW: p.wave.includes('S') ? 1 : 0, PULSE: p.wave.includes('P') ? 1 : 0, RANGE: { "16'": 0, "8'": 1, "4'": 2 }[p.range ?? "8'"],
    SUB: s127(sub), NOISE: s127(nz), PWMSRC: PWMM[p.pwm ?? 'MAN'], LFODCO: s127(dl), PWM: s127(pw), HPF: p.hpf ?? 0,
    ENVPOL: p.neg ? 1 : 0, KEYT: s127(kb), VELF: 0, VELA: 0, VCA: p.gate ? 1 : 0,
    A: envV(0.0015 * Math.pow(2000, a / 10)), D: envV(0.0015 * Math.pow(8000, d / 10)), S: s127(s), R: envV(0.0015 * Math.pow(8000, r / 10)),
    LRATE: lfoV(0.3 * Math.pow(2, lr * 0.606)), LDELAY: ld ? envV(ld / 10 * 1.5) : 0,
    CHORUS: { OFF: 0, I: 1, II: 2, 'I+II': 3 }[p.chorus ?? 'OFF'],
  };
  P.macro = [s127(fq), s127(rs), s127(ev), s127(lf)];
  return P;
}

// orden: el 2 es un pad (la demo de fábrica lo usa en T6)
const BUNO = [
  ['POLY BRASS', juno({ wave: 'S', lfo: [1.6, 3.9], dco: [0, 8.0, 0, 0], vcf: [2.8, 1.0, 4.6, 0, 6.8], adsr: [0.2, 3.9, 3.5, 2.5], chorus: 'I' })],
  ['SAW STRINGS', juno({ wave: 'S', lfo: [4.4, 0], dco: [0, 0, 0, 0], vcf: [6.9, 0, 0, 0, 10], adsr: [5.9, 0, 10, 6.5], chorus: 'I' })],
  ['SOFT PAD', juno({ wave: 'PS', pwm: 'LFO', lfo: [6, 4], dco: [0, 5, 3, 0], vcf: [5, 1, 1, 2, 5], adsr: [6, 5, 8, 6], chorus: 'I' })],
  ['PWM STRINGS', juno({ wave: 'PS', pwm: 'LFO', lfo: [2.0, 9.8], dco: [0, 7.6, 10, 0], vcf: [5.2, 0, 0, 0, 10], adsr: [4.7, 0, 10, 7.6], chorus: 'II' })],
  ['MOVING STRS', juno({ wave: 'PS', pwm: 'LFO', lfo: [5.0, 0], dco: [0, 3.1, 1.1, 0], vcf: [6.1, 1.6, 0.3, 0, 8.7], adsr: [1.0, 6.9, 6.9, 2.8], chorus: 'II' })],
  ['VOX CHOIR', juno({ wave: 'P', pwm: 'MAN', hpf: 2, lfo: [4.6, 1.1], dco: [1.0, 2.0, 0, 0], vcf: [4.6, 7.4, 0.2, 0, 4.9], adsr: [5.4, 0.9, 10, 3.8], chorus: 'I' })],
  ['DEEP CHORDS', juno({ wave: 'P', pwm: 'MAN', lfo: [3, 0], dco: [0, 3, 3, 0], vcf: [4.5, 2, 2, 0, 5], adsr: [2, 6, 6, 5], chorus: 'II' })],
  ['PHASE BRASS', juno({ wave: 'PS', pwm: 'ENV', gate: true, lfo: [4.5, 0], dco: [0, 10, 10, 0], vcf: [2.6, 0.3, 5.5, 0, 10], adsr: [3.9, 6.5, 4.2, 5.4], chorus: 'I' })],
  ['PULSE ORGAN', juno({ wave: 'P', pwm: 'LFO', gate: true, range: "16'", lfo: [4.3, 1.2], dco: [0, 4.2, 1.8, 0], vcf: [3.4, 6.0, 1.1, 0.1, 10], adsr: [0, 0.8, 6.5, 0] })],
  ['FUNKY PLUCK', juno({ wave: 'PS', pwm: 'MAN', gate: true, range: "16'", lfo: [0.3, 0], dco: [0, 5.7, 5.0, 0], vcf: [2.4, 1.8, 4.0, 0, 3.2], adsr: [0, 2.4, 2.8, 0.2], chorus: 'I' })],
  ['POP RIFF', juno({ wave: 'PS', pwm: 'MAN', lfo: [5, 0], dco: [0, 2, 0, 0], vcf: [6.5, 1.5, 3, 0, 5], adsr: [0, 3.5, 3, 2.5], chorus: 'I' })],
  ['DELAY LEAD', juno({ wave: 'P', pwm: 'MAN', gate: true, lfo: [5.7, 6.9], dco: [1.4, 0, 0, 0], vcf: [3.1, 7.6, 2.8, 0, 4.1], adsr: [0, 3.5, 5.7, 0] })],
  ['PULSE BASS', juno({ wave: 'P', pwm: 'MAN', range: "16'", lfo: [4.6, 1.3], dco: [0.1, 4.5, 8.0, 3.9], vcf: [2.5, 0, 5.2, 0, 2.8], adsr: [0, 2.7, 0, 2.8] })],
  ['ROUND BASS', juno({ wave: 'PS', pwm: 'MAN', gate: true, range: "16'", lfo: [2.6, 8.3], dco: [0, 5.0, 2.9, 0], vcf: [3.0, 2.1, 3.1, 0, 0], adsr: [0.2, 6.1, 0.2, 4.5], chorus: 'I' })],
  ['DARK RAVE', juno({ wave: 'PS', pwm: 'LFO', lfo: [7, 0], dco: [1, 9, 6, 0], vcf: [6, 2, 3, 0, 5], adsr: [1.5, 5, 8, 4.5], chorus: 'II' })],
  ['NOISE SWEEP', { SAW: 0, PULSE: 0, NOISE: 110, SUB: 0, A: 90, D: 100, S: 40, R: 95, CHORUS: 1, LRATE: 30, PWM: 0, macro: [40, 80, 90, 60] }],
];

// BITTAL: tablas propias (aditivas, 8 frames, hasta 64 armónicos), al estilo de las del PPG Wave
// (sin datos de su ROM). f = 0..7 frame, h = armónico 1..64: amplitud (la fase, 0).
const TABLES = {
  // armónicos 1-8 con un pico resonante que sube (simulación de un filtro resonante)
  RES: (f, h) => (1 / h) * (1 + 5 * Math.exp(-Math.pow(h - (1 + f), 2) / 0.8)),
  // campanas: pocos parciales altos que cambian de peso
  BELL: (f, h) => ({ 1: 1, 3: 0.3 + 0.05 * f, 7: 0.5 - 0.04 * f, 11: 0.15 + 0.06 * f, 17: 0.25 * (1 - f / 8), 23: 0.05 * f, 31: 0.04 * f })[h] || 0,
  // espectro de vibráfono: la fundamental, el 4 y el 10, que se apagan hacia el final
  VIBE: (f, h) => ({ 1: 1, 4: 0.6 * (1 - f / 9), 10: 0.3 * (1 - f / 8), 2: 0.1 })[h] || 0,
  // coro: una sierra con dos formantes que se mueven (vocales)
  CHOIR: (f, h) => {
    const f1 = 2.5 + f * 0.35, f2 = 9 - f * 0.6;
    return (1 / h) * (0.15 + Math.exp(-Math.pow(h - f1, 2) / 1.5) + 0.7 * Math.exp(-Math.pow(h - f2, 2) / 4));
  },
  // metales: una sierra con un formante que se abre de frame en frame
  BRASS: (f, h) => (1 / h) * (0.3 + 1.2 * Math.exp(-Math.pow(h - (3 + f * 1.5), 2) / (4 + f * 2))) * (h <= 6 + f * 6 ? 1 : 0.25),
  // de cuadrada a pulso angosto (PWM)
  PWM: (f, h) => { const w = 0.5 - f * 0.064; return Math.abs(Math.sin(Math.PI * h * w)) / h; },
};
function tableB64(fn) {
  const N = 2048, keys = [];
  for (let f = 0; f < 8; f++) {
    const x = new Float32Array(N);
    for (let h = 1; h <= 64; h++) {
      const a = fn(f, h);
      if (!a) continue;
      for (let i = 0; i < N; i++) x[i] += a * Math.sin(2 * Math.PI * h * i / N);
    }
    let pk = 0; for (const v of x) pk = Math.max(pk, Math.abs(v));
    for (let i = 0; i < N; i++) x[i] /= pk || 1;
    keys.push({ position: Math.round(f * 256 / 7), wave_data: Buffer.from(x.buffer).toString('base64') });
  }
  return keys;
}

// ajustes de BITTAL (valores de Bubba): tab = tabla del osc 1; el resto, los de bm_params.h
const BITTAL = [
  ['WAVE SWEEP', { tab: 'RES', P: { macro: [0, 0, 0, 0], L1: 110, P1: 0, EPOS: 127, FT: 0, CUT: 115, RES: 10, MA: envV(0.15), MD: envV(0.3), MS: 0, MR: envV(0.3), AA: 0, AD: 70, AS: 110, AR: envV(0.3), FENV: 64, LAMT: 0, VELA: 40 } }],
  ['GLASS BELLS', { tab: 'BELL', P: { macro: [0, 0, 0, 0], L1: 110, P1: 20, EPOS: 80, FT: 0, CUT: 120, RES: 5, MA: 0, MD: envV(1.2), MS: 0, MR: envV(1.2), AA: 0, AD: envV(2.5), AS: 0, AR: envV(1.5), U1: 1, D1: 6, LAMT: 0, VELA: 60 } }],
  ['MALLET VIBES', { tab: 'VIBE', P: { macro: [0, 0, 0, 0], L1: 110, P1: 0, EPOS: 100, FT: 0, CUT: 118, RES: 0, MA: 0, MD: envV(0.6), MS: 0, MR: envV(0.6), AA: 0, AD: envV(1.6), AS: 0, AR: envV(0.8), LSHAPE: 0, LRATE: lfoV(5), LDEST: 3, LAMT: 45, VELA: 70 } }],
  ['MIXED CHOIR', { tab: 'CHOIR', P: { macro: [0, 0, 0, 0], L1: 105, P1: 40, U1: 1, D1: 18, FT: 0, CUT: 100, RES: 10, AA: envV(0.3), AD: 90, AS: 120, AR: envV(0.8), LSHAPE: 1, LRATE: lfoV(0.25), LDEST: 2, LAMT: 40, VELA: 30 } }],
  ['WAVE BRASS', { tab: 'BRASS', P: { macro: [0, 0, 0, 0], L1: 110, P1: 10, EPOS: 90, FT: 0, CUT: 100, RES: 15, FENV: 80, MA: envV(0.08), MD: envV(0.4), MS: 40, MR: envV(0.25), AA: envV(0.03), AD: 80, AS: 120, AR: envV(0.25), U1: 1, D1: 10, VELA: 50 } }],
  ['WAVE STRINGS', { tab: 'PWM', P: { macro: [0, 0, 0, 0], L1: 105, P1: 0, U1: 1, D1: 20, FT: 0, CUT: 104, RES: 5, AA: envV(0.4), AD: 90, AS: 120, AR: envV(0.9), LSHAPE: 1, LRATE: lfoV(0.5), LDEST: 2, LAMT: 60, VELA: 30 } }],
  ['CLICK BASS', { tab: 'RES', P: { macro: [0, 0, 0, 0], L1: 115, P1: 0, EPOS: 70, FT: 0, CUT: 72, RES: 25, FENV: 90, MA: 0, MD: envV(0.15), MS: 0, MR: envV(0.1), AA: 0, AD: envV(0.6), AS: 50, AR: envV(0.1), MODE: 1, VELA: 50 } }],
];

function ownVital(name, machine, tag, P, keys) {
  const s = { osc_1_on: 1, osc_1_level: 0.7, osc_2_on: 0, osc_3_on: 0, polyphony: machine === 'B-MINI' ? 1 : 6 };
  if (keys) s.wavetables = [{ name: name, author: 'Bubba Machine', version: '1.0.5', groups: [{ components: [{ type: 'Wave Source', interpolation: 1, interpolation_style: 1, keyframes: keys }] }] }];
  else s.osc_1_on = 0;
  return { author: 'Bubba Machine', comments: `Bubba Machine factory sound for ${machine}, CC0. bubba-${tag}: ` + JSON.stringify(P),
    preset_name: name, synth_version: '1.0.5', settings: s };
}

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
  fs.mkdirSync(P('assets/presets/own'), { recursive: true });
  fs.mkdirSync(P('assets/presets/bsnap'), { recursive: true });
  // empty the generated folders: a sound that left the bank does not stay behind
  for (const d of ['assets/presets/buno', 'assets/presets/own', 'assets/presets/bsnap'])
    for (const f of fs.readdirSync(P(d))) fs.unlinkSync(P(d, f));
  for (const [name, p] of MINI) {
    const v = ownVital(name, 'B-MINI', 'mini', p, null);
    fs.writeFileSync(P('assets/presets/own', slug(name) + '.vital'), JSON.stringify(v));
    out.push({ name, machine: 'B-MINI', author: 'BUBBA', lic: LIC.OWN, json: v });
  }
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
  // BITTAL's own go after the community ones: the factory demo plays BITTAL sound 13
  for (const [name, b] of BITTAL) {
    const v = ownVital(name, 'BITTAL', 'bittal', b.P, tableB64(TABLES[b.tab]));
    fs.writeFileSync(P('assets/presets/own', slug(name) + '.vital'), JSON.stringify(v));
    out.push({ name, machine: 'BITTAL', author: 'BUBBA', lic: LIC.OWN, json: v });
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
