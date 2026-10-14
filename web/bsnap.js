// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// Bubba Machine — de un preset de Vital (.vital, JSON) a un snapshot del FM-1 (.bsnap).
// docs/DISENO_0.6.md §4. Lo usan el editor web (web/sonidos.html) y el banco de fábrica
// (tools/gen_snaps.mjs, con node). Todo el cálculo pesado pasa aquí: el FM-1 solo carga.
//
//   vitalToBsnap(json, {machine, name, author, lic}) -> {bytes, report}
//   machine: 'BITTAL' (dos wavetables), 'B-MINI' (tres osciladores clásicos), 'BUNO-6' (un DCO).
//   report: {kept: [...], approx: [...], dropped: [...], cpu: '%'} en inglés (como la pantalla).

export const ENG = { 'B-MINI': 3, 'BITTAL': 5, 'BUNO-6': 6 };
export const LIC = { OWN: 0, CC0: 1, UNLICENSE: 2, GPL3: 3 };

// Los índices de bm_pattern.prm de cada máquina, en el orden de core/bm_params.h (gen_snaps.mjs lo comprueba).
export const MN = ['W1', 'R1', 'L1', 'W2', 'R2', 'T2', 'L2', 'W3', 'R3', 'T3', 'L3', 'NOISE', 'DRIVE', 'KEYT', 'VELF',
  'FA', 'FD', 'FS', 'FR', 'AA', 'AD', 'AS', 'AR', 'LRATE', 'LAMT', 'LDEST', 'GLIDE', 'MODE', 'VELA'];
export const BN = ['SAW', 'PULSE', 'RANGE', 'SUB', 'NOISE', 'PWMSRC', 'LFODCO', 'HPF', 'ENVPOL', 'LFOVCF', 'KEYT', 'VELF',
  'A', 'D', 'S', 'R', 'VCA', 'LRATE', 'LDELAY', 'CHORUS', 'VELA'];
export const BT = ['L1', 'S1', 'F1', 'P1', 'U1', 'D1', 'TAB1', 'L2', 'S2', 'F2', 'P2', 'U2', 'D2', 'TAB2',
  'FT', 'CUT', 'RES', 'DRIVE', 'FENV', 'KEYT', 'VELF', 'AA', 'AD', 'AS', 'AR', 'MA', 'MD', 'MS', 'MR',
  'LSHAPE', 'LRATE', 'LDEST', 'LAMT', 'M1D', 'M1A', 'M2D', 'M2A', 'M3D', 'M3A', 'M4D', 'M4A', 'MODE', 'GLIDE', 'VELA'];
const BTM = ['NONE', 'CUT', 'RES', 'POS1', 'POS2', 'POS', 'LVL2', 'DET', 'LFO', 'FENV', 'DRIVE', 'REL', 'SPACE'];
const WAVE = { TRI: 0, SHARK: 1, SAW: 2, SQR: 3, WIDE: 4, NARROW: 5 };

// Los valores de fábrica de cada ajuste (los mismos que las páginas de core/bm_params.c).
const DEF = {
  'B-MINI': { W1: 2, R1: 1, L1: 110, W2: 2, R2: 1, T2: 66, L2: 90, W3: 3, R3: 0, T3: 64, L3: 50, NOISE: 0, DRIVE: 30, KEYT: 40, VELF: 40,
    FA: 0, FD: 55, FS: 20, FR: 45, AA: 0, AD: 70, AS: 110, AR: 35, LRATE: 70, LAMT: 0, LDEST: 1, GLIDE: 35, MODE: 0, VELA: 40 },
  'BUNO-6': { SAW: 1, PULSE: 0, RANGE: 1, SUB: 40, NOISE: 0, PWMSRC: 0, LFODCO: 0, HPF: 1, ENVPOL: 0, LFOVCF: 0, KEYT: 60, VELF: 20,
    A: 10, D: 70, S: 90, R: 60, VCA: 0, LRATE: 60, LDELAY: 0, CHORUS: 1, VELA: 0 },
  'BITTAL': { L1: 100, S1: 64, F1: 64, P1: 0, U1: 0, D1: 20, TAB1: 0, L2: 0, S2: 64, F2: 64, P2: 0, U2: 0, D2: 20, TAB2: 0,
    FT: 0, CUT: 100, RES: 15, DRIVE: 0, FENV: 64, KEYT: 64, VELF: 20, AA: 0, AD: 70, AS: 110, AR: 55, MA: 0, MD: 65, MS: 40, MR: 55,
    LSHAPE: 1, LRATE: 60, LDEST: 2, LAMT: 0, M1D: 1, M1A: 110, M2D: 5, M2A: 120, M3D: 8, M3A: 120, M4D: 12, M4A: 120, MODE: 0, GLIDE: 0, VELA: 50 },
};
const MACRO_DEF = { 'B-MINI': [60, 50, 64, 70], 'BUNO-6': [72, 30, 50, 50], 'BITTAL': [64, 0, 0, 0] };

const clamp = (v, lo, hi) => v < lo ? lo : v > hi ? hi : v;
const r127 = (x) => clamp(Math.round(x), 0, 127);
// tiempos de Vital: el valor guardado es la raíz cuarta de los segundos
const vSecs = (v) => Math.pow(Math.max(0, v || 0), 4);
// bm_env_s(v) = 0.001 * 10^(4 v / 127)
const envV = (secs) => r127(Math.log10(Math.max(secs, 0.001) / 0.001) * 127 / 4);
// bm_cut_hz(v) = 20 * 2^(10 v / 127); Vital: el corte es una nota MIDI
const cutV = (midi) => r127(Math.log2(Math.max(20, 440 * Math.pow(2, (midi - 69) / 12)) / 20) * 12.7);
// bm_lfo_hz(v) = 0.05 * 2^(9 v / 127)
const lfoV = (hz) => r127(Math.log2(Math.max(hz, 0.05) / 0.05) * 127 / 9);
const bipV = (x) => r127(64 + x * 63);

function b64floats(s) {
  const bin = typeof atob === 'function' ? atob(s) : Buffer.from(s, 'base64').toString('binary');
  const u8 = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
  return new Float32Array(u8.buffer, 0, u8.length >> 2);
}

// ------------------------------------------------------------ wavetables
const N = 2048;

function lineFrame(line) {          // Line Source: puntos (x, y) con y = 0 arriba
  const p = line.points || [], n = (line.num_points || p.length / 2) | 0, out = new Float32Array(N);
  for (let i = 0; i < N; i++) {
    const x = i / N;
    let k = 0;
    while (k < n - 2 && p[2 * (k + 1)] < x) k++;
    const x0 = p[2 * k], y0 = p[2 * k + 1], x1 = p[2 * k + 2] ?? 1, y1 = p[2 * k + 3] ?? y0;
    const f = x1 > x0 ? clamp((x - x0) / (x1 - x0), 0, 1) : 0;
    out[i] = 1 - 2 * (y0 + (y1 - y0) * f);
  }
  return out;
}

function compFrames(comp, report, osc) {   // [{pos, data}] de un componente fuente, o null
  if (comp.type === 'Wave Source')
    return (comp.keyframes || []).filter(k => k.wave_data).map(k => ({ pos: k.position || 0, data: b64floats(k.wave_data) }));
  if (comp.type === 'Line Source')
    return (comp.keyframes || []).map(k => ({ pos: k.position || 0, data: lineFrame(k.line || {}) }));
  report.dropped.push(`OSC ${osc}: ${comp.type} (not supported yet)`);
  return null;
}

function frameAt(keys, pos) {             // interpolación lineal entre keyframes
  if (keys.length === 1) return keys[0].data;
  let a = keys[0], b = keys[keys.length - 1];
  for (let i = 0; i < keys.length - 1; i++) if (keys[i].pos <= pos && keys[i + 1].pos >= pos) { a = keys[i]; b = keys[i + 1]; break; }
  if (pos <= keys[0].pos) return keys[0].data;
  if (pos >= keys[keys.length - 1].pos) return keys[keys.length - 1].data;
  const f = b.pos > a.pos ? (pos - a.pos) / (b.pos - a.pos) : 0, out = new Float32Array(N);
  for (let i = 0; i < N; i++) out[i] = a.data[i] + (b.data[i] - a.data[i]) * f;
  return out;
}

// los frames de una wavetable de Vital (los grupos se suman; los modificadores no se aplican)
export function tableFrames(wt, report, osc) {
  const groups = [];
  for (const g of wt.groups || []) {
    const comps = g.components || [];
    if (!comps.length) continue;
    const src = compFrames(comps[0], report, osc);
    for (const m of comps.slice(1)) report.approx.push(`OSC ${osc}: ${m.type} not applied`);
    if (src && src.length) groups.push(src.sort((x, y) => x.pos - y.pos));
  }
  if (!groups.length) return null;
  const multi = groups.some(g => g.length > 1);
  const nf = multi ? 8 : 1, frames = [];
  for (let f = 0; f < nf; f++) {
    const pos = nf > 1 ? f * 256 / (nf - 1) : 0, sum = new Float32Array(N);
    for (const g of groups) { const d = frameAt(g, pos); for (let i = 0; i < N; i++) sum[i] += d[i]; }
    frames.push(sum);
  }
  return frames;
}

// espectro de cada frame: armónicos 1..256 (DFT directa: 256 x 2048 por frame)
function spectra(frames) {
  const H = 256, out = [];
  const cs = new Float32Array(N), sn = new Float32Array(N);
  for (let i = 0; i < N; i++) { cs[i] = Math.cos(2 * Math.PI * i / N); sn[i] = Math.sin(2 * Math.PI * i / N); }
  for (const x of frames) {
    const amp = new Float32Array(H + 1), ph = new Float32Array(H + 1);
    for (let h = 1; h <= H; h++) {
      let re = 0, im = 0;
      for (let i = 0, k = 0; i < N; i++, k = (k + h) & (N - 1)) { re += x[i] * cs[k]; im -= x[i] * sn[k]; }
      amp[h] = Math.hypot(re, im); ph[h] = Math.atan2(im, re);
    }
    out.push({ amp, ph });
  }
  return out;
}

// la tabla en el formato de core/bm_wt.h: nf, nh, y (magnitud, fase) por armónico
export function packTable(frames) {
  const sp = spectra(frames);
  let amax = 0;
  for (const s of sp) for (let h = 1; h <= 256; h++) amax = Math.max(amax, s.amp[h]);
  if (amax <= 0) return null;
  const q = sp.map(s => {
    const m = new Uint8Array(257), p = new Uint8Array(257);
    for (let h = 1; h <= 256; h++) {
      const db = 20 * Math.log10(s.amp[h] / amax + 1e-12);
      m[h] = db < -94 ? 0 : clamp(Math.round(255 + db / 0.375), 1, 255);
      p[h] = ((Math.round(s.ph[h] / (2 * Math.PI) * 256) % 256) + 256) % 256;
    }
    return { m, p };
  });
  let nh = 1;
  for (const s of q) for (let h = 256; h >= 1; h--) if (s.m[h]) { nh = Math.max(nh, h); break; }
  let frs = q;                                     // frames iguales: uno solo
  if (q.every(s => s.m.every((v, i) => v === q[0].m[i]))) frs = [q[0]];
  const t = new Uint8Array(2 + frs.length * nh * 2);
  t[0] = frs.length; t[1] = nh === 256 ? 0 : nh;
  let o = 2;
  for (const s of frs) for (let h = 1; h <= nh; h++) { t[o++] = s.m[h]; t[o++] = s.p[h]; }
  return t;
}

// forma clásica más parecida a un frame (para B-MINI y BUNO-6)
export function classify(frame) {
  const [s] = spectra([frame]);
  const a = (h) => s.amp[h] / (s.amp[1] || 1e-9);
  const even = a(2) + a(4) + a(6), odd = a(3) + a(5) + a(7);
  if (odd + even < 0.08) return 'TRI';                        // casi un seno
  if (even < 0.15 * odd) return a(3) > 0.2 ? 'SQR' : 'TRI';   // impares: cuadrada o triángulo
  return 'SAW';
}

// ------------------------------------------------------------ ajustes
function modAmount(s, i) { return s[`modulation_${i + 1}_amount`] ?? 0; }

function mods(s) { return (s.modulations || []).map((m, i) => ({ ...m, amount: modAmount(s, i) })).filter(m => m.source && m.destination); }

const MACRO_DEST = (d) => ({
  filter_1_cutoff: 'CUT', filter_2_cutoff: 'CUT', filter_1_resonance: 'RES', osc_1_wave_frame: 'POS1', osc_2_wave_frame: 'POS2',
  osc_2_level: 'LVL2', osc_1_unison_detune: 'DET', osc_2_unison_detune: 'DET', env_1_release: 'REL', filter_1_drive: 'DRIVE',
  distortion_drive: 'DRIVE', reverb_dry_wet: 'SPACE', delay_dry_wet: 'SPACE', chorus_dry_wet: 'SPACE', env_2_attack: 'FENV',
}[d] || (/^lfo_\d_/.test(d) ? 'LFO' : null));

function lfoHz(s, i, bpm) {
  if (s[`lfo_${i}_sync`] > 0.5) return (bpm / 60) * Math.pow(2, (s[`lfo_${i}_tempo`] ?? 7) - 7);
  return Math.pow(2, s[`lfo_${i}_frequency`] ?? 1);
}

// el nivel efectivo de un oscilador de Vital: el suyo más lo que le suman sus modulaciones
// (muchos presets lo abren con una envolvente)
function oscLevel(s, o) {
  const lm = mods(s).filter(m => m.destination === `osc_${o}_level` && m.amount > 0).reduce((a, m) => a + m.amount, 0);
  return { lvl: Math.min(1, (s[`osc_${o}_level`] ?? 0.7) + lm), mod: lm > 0 };
}

// los dos osciladores que suenan (de los tres de Vital): picks = [o, o]
export function pickOscs(s, frames) {
  return [1, 2, 3].filter(o => s[`osc_${o}_on`] > 0.5 && frames[o - 1] && oscLevel(s, o).lvl > 0.02).slice(0, 2);
}

function bittal(s, report, P, mac, picks) {
  picks.forEach((o, i) => {
    const k = i === 0 ? '1' : '2';
    const { lvl, mod } = oscLevel(s, o);
    P['L' + k] = r127(Math.min(1, lvl / 0.71) * 100);
    if (mod) report.approx.push(`OSC ${o}: level modulation -> fixed level`);
    P['S' + k] = r127(64 + clamp(s[`osc_${o}_transpose`] ?? 0, -24, 24) * 63 / 24);
    P['F' + k] = r127(64 + clamp((s[`osc_${o}_tune`] ?? 0) / 0.5, -1, 1) * 63);
    P['P' + k] = r127((s[`osc_${o}_wave_frame`] ?? 0) / 256 * 127);
    const uni = Math.round(s[`osc_${o}_unison_voices`] ?? 1);
    P['U' + k] = clamp(uni, 1, 4) - 1;
    P['D' + k] = r127((s[`osc_${o}_unison_detune`] ?? 2) * 10);
    if (uni > 4) report.approx.push(`OSC ${o}: unison ${uni} -> 4`);
    report.kept.push(`OSC ${o}` + (i !== o - 1 ? ` -> ${i + 1}` : ''));
  });
  if (picks.length < 2) P.L2 = 0;
  const off = [1, 2, 3].filter(o => s[`osc_${o}_on`] > 0.5 && !picks.includes(o));
  for (const o of off) report.dropped.push(`OSC ${o}`);
  const fon = s.filter_1_on > 0.5 ? 1 : s.filter_2_on > 0.5 ? 2 : 0;
  if (fon) {
    const f = `filter_${fon}_`, model = Math.round(s[f + 'model'] ?? 0), blend = s[f + 'blend'] ?? 0;
    const style = Math.round(s[f + 'style'] ?? 0);
    let ft = style === 1 ? 0 : 1;                                 // estilo 0 = 12 dB, 1 = 24 dB
    if (blend > 1.5) ft = 3; else if (blend > 0.5) ft = 2;        // mezcla 0 LP, 1 BP, 2 HP
    if (style > 1) report.approx.push('FILTER: notch/shelf style -> LP');
    if ((model === 2 || model === 1 || model === 4) && ft < 2) ft = 4;   // LADDER, DIRTY y DIODE: la escalera
    if (model >= 5) report.approx.push(`FILTER: ${['FORMANT', 'COMB', 'PHASER'][model - 5]} -> LP`);
    P.FT = ft;
    P.CUT = cutV(s[f + 'cutoff'] ?? 100);
    P.RES = r127((s[f + 'resonance'] ?? 0) * 127);
    P.DRIVE = r127((s[f + 'drive'] ?? 0) / 20 * 127);
    P.KEYT = r127(clamp(s[f + 'keytrack'] ?? 0, 0, 1) * 127);
    report.kept.push('FILTER');
  } else P.FT = 5;
  P.AA = envV(vSecs(s.env_1_attack)); P.AD = envV(vSecs(s.env_1_decay)); P.AS = r127((s.env_1_sustain ?? 1) * 127); P.AR = envV(vSecs(s.env_1_release));
  P.MA = envV(vSecs(s.env_2_attack)); P.MD = envV(vSecs(s.env_2_decay)); P.MS = r127((s.env_2_sustain ?? 1) * 127); P.MR = envV(vSecs(s.env_2_release));
  const M = mods(s);
  const fe = M.find(m => m.source === 'env_2' && /filter_\d_cutoff/.test(m.destination));
  if (fe) { P.FENV = bipV(clamp(fe.amount, -1, 1)); report.kept.push('ENV 2 > CUTOFF'); }
  const lf = M.find(m => m.source === 'lfo_1');
  if (lf) {
    P.LDEST = /cutoff/.test(lf.destination) ? 1 : /wave_frame/.test(lf.destination) ? 2 : /level/.test(lf.destination) ? 3 : 0;
    P.LAMT = r127(Math.abs(lf.amount) * 127); P.LRATE = lfoV(lfoHz(s, 1, s.beats_per_minute ? s.beats_per_minute * 60 : 120));
    report.kept.push('LFO 1');
  }
  // macros: las del preset, o cuatro de fábrica (TONE, WAVE, MOVE, SPACE)
  let used = 0;
  for (let i = 0; i < 4; i++) {
    const m = M.find(x => x.source === `macro_control_${i + 1}` && MACRO_DEST(x.destination));
    if (!m) continue;
    P[`M${i + 1}D`] = BTM.indexOf(MACRO_DEST(m.destination)); P[`M${i + 1}A`] = bipV(clamp(m.amount, -1, 1));
    mac[i] = r127((s[`macro_control_${i + 1}`] ?? 0) * 127);
    used++;
  }
  if (used) report.kept.push(`MACROS (${used})`);
  else {
    mac[0] = 64; mac[1] = 0; mac[2] = 0; mac[3] = s.reverb_on > 0.5 ? r127((s.reverb_dry_wet ?? 0.3) * 127) : 0;
    report.approx.push('MACROS: TONE, WAVE, MOVE, SPACE');
  }
  P.MODE = s.polyphony <= 1 ? (s.legato > 0.5 ? 2 : 1) : 0;
  P.GLIDE = s.portamento_time > -9 ? envV(Math.pow(2, s.portamento_time)) : 0;
  P.VELA = r127((s.velocity_track ?? 0.5) * 127);
}

function mini(s, report, P, mac, frames) {
  const W = [], L = [], R = [];
  for (const o of [1, 2, 3]) {
    if (!(s[`osc_${o}_on`] > 0.5) || !frames[o - 1]) { W.push(WAVE.SAW); L.push(0); R.push(1); continue; }
    W.push(WAVE[classify(frames[o - 1][0])]);
    L.push(r127(Math.min(1, oscLevel(s, o).lvl / 0.71) * 100));
    const oct = Math.round((s[`osc_${o}_transpose`] ?? 0) / 12);
    R.push(clamp(2 + oct, 0, 4));
  }
  [P.W1, P.W2, P.W3] = W; [P.L1, P.L2, P.L3] = L; [P.R1, P.R2, P.R3] = R;
  P.T2 = r127(64 + clamp(((s.osc_2_transpose ?? 0) % 12 + (s.osc_2_tune ?? 0)) / 7, -1, 1) * 63);
  P.T3 = r127(64 + clamp(((s.osc_3_transpose ?? 0) % 12 + (s.osc_3_tune ?? 0)) / 7, -1, 1) * 63);
  report.approx.push('OSC: wavetables -> closest classic wave');
  mac[0] = cutV(s.filter_1_cutoff ?? 80); mac[1] = r127((s.filter_1_resonance ?? 0.2) * 127);
  const M = mods(s), fe = M.find(m => m.source === 'env_2' && /filter_\d_cutoff/.test(m.destination));
  mac[2] = 64; mac[3] = fe ? r127(Math.abs(fe.amount) * 127) : 40;
  P.DRIVE = r127((s.filter_1_drive ?? 0) / 20 * 127 + 20);
  P.AA = envV(vSecs(s.env_1_attack)); P.AD = envV(vSecs(s.env_1_decay)); P.AS = r127((s.env_1_sustain ?? 1) * 127); P.AR = envV(vSecs(s.env_1_release));
  P.FA = envV(vSecs(s.env_2_attack)); P.FD = envV(vSecs(s.env_2_decay)); P.FS = r127((s.env_2_sustain ?? 0.2) * 127); P.FR = envV(vSecs(s.env_2_release));
  P.GLIDE = s.portamento_time > -9 ? envV(Math.pow(2, s.portamento_time)) : 0;
  P.MODE = s.legato > 0.5 ? 1 : 0;
  report.kept.push('FILTER', 'ENV 1', 'ENV 2');
}

function buno(s, report, P, mac, frames) {
  // presets propios: los ajustes de BUNO-6 viajan en los comentarios del .vital ("bubba-buno: {...}")
  const tag = /bubba-buno:\s*(\{[^}]*\})/.exec(s.__comments || '');
  if (tag) {
    const j = JSON.parse(tag[1]);
    for (const k of Object.keys(j)) if (k in P) P[k] = j[k];
    if (j.macro) for (let i = 0; i < 4; i++) mac[i] = j.macro[i];
    report.kept.push('BUNO-6 settings');
    return;
  }
  const w = frames[0] ? classify(frames[0][0]) : 'SAW';
  P.SAW = w === 'SAW' ? 1 : 0; P.PULSE = w === 'SAW' ? 0 : 1;
  P.SUB = s.osc_2_on > 0.5 && (s.osc_2_transpose ?? 0) <= -12 ? r127((s.osc_2_level ?? 0.5) * 127) : 0;
  mac[0] = cutV(s.filter_1_cutoff ?? 90); mac[1] = r127((s.filter_1_resonance ?? 0.2) * 127);
  P.A = envV(vSecs(s.env_1_attack)); P.D = envV(vSecs(s.env_1_decay)); P.S = r127((s.env_1_sustain ?? 1) * 127); P.R = envV(vSecs(s.env_1_release));
  P.CHORUS = s.chorus_on > 0.5 ? 1 : 0;
  report.approx.push('DCO: closest classic wave');
}

// ------------------------------------------------------------ armado
function fnv16(u8) {
  let h = 2166136261 >>> 0;
  for (let i = 0; i < u8.length; i++) { const b = (i === 8 || i === 9) ? 0 : u8[i]; h = Math.imul(h ^ b, 16777619) >>> 0; }
  return (h ^ (h >>> 16)) & 0xFFFF;
}

const ascii = (str, n) => {
  const s = String(str || '').normalize('NFD').replace(/[̀-ͯ]/g, '').toUpperCase().replace(/[^ -~]/g, '');
  const o = new Uint8Array(n);
  for (let i = 0; i < Math.min(n, s.length); i++) o[i] = s.charCodeAt(i);
  return o;
};

export function vitalToBsnap(json, opts = {}) {
  const machine = opts.machine || 'BITTAL';
  if (!(machine in ENG)) throw new Error('machine: ' + machine);
  const s = Object.assign({}, json.settings || {}, { __comments: json.comments || '' });
  const report = { kept: [], approx: [], dropped: [], cpu: '' };
  const P = Object.assign({}, DEF[machine]), mac = MACRO_DEF[machine].slice();
  const frames = [1, 2, 3].map(o => (s[`osc_${o}_on`] > 0.5 && s.wavetables && s.wavetables[o - 1]) ? tableFrames(s.wavetables[o - 1], report, o) : null);
  if (s.sample_on > 0.5) report.dropped.push('SAMPLE OSC');
  const fx = ['chorus', 'compressor', 'delay', 'distortion', 'eq', 'flanger', 'phaser', 'reverb'].filter(x => s[x + '_on'] > 0.5);
  for (const x of fx) {
    if (x === 'chorus' && machine === 'BUNO-6') { report.kept.push('CHORUS (BBD)'); continue; }
    (x === 'reverb' || x === 'delay' ? report.approx : report.dropped).push(`FX ${x.toUpperCase()}` + (x === 'reverb' || x === 'delay' ? ' -> channel send' : ''));
  }
  let tables = [];
  if (machine === 'BITTAL') {
    const picks = pickOscs(s, frames);
    tables = picks.map(o => packTable(frames[o - 1]));
    bittal(s, report, P, mac, picks);
  } else if (machine === 'B-MINI') mini(s, report, P, mac, frames);
  else buno(s, report, P, mac, frames);
  const tabs = tables.filter(Boolean);
  const order = machine === 'BITTAL' ? BT : machine === 'B-MINI' ? MN : BN;
  const prm = new Uint8Array(48);
  order.forEach((k, i) => { prm[i] = clamp(P[k] | 0, 0, 255); });
  const size = 88 + tabs.reduce((n, t) => n + t.length, 0);
  const out = new Uint8Array(size);
  out.set([0x42, 0x4E, 0x53, 0x31], 0);                 // "BNS1"
  out[4] = ENG[machine]; out[5] = tabs.length; out[6] = opts.lic ?? LIC.OWN; out[7] = 1;
  out[10] = size & 255; out[11] = size >> 8;
  out.set(ascii(opts.name || json.preset_name || 'SOUND', 12), 12);
  out.set(ascii(opts.author || json.author || '', 12), 24);
  out.set(mac.map(v => clamp(v | 0, 0, 127)), 36);
  out.set(prm, 40);
  let o = 88;
  for (const t of tabs) { out.set(t, o); o += t.length; }
  const id = fnv16(out);
  out[8] = id & 255; out[9] = id >> 8;
  const voices = machine === 'BITTAL' ? 4 : machine === 'BUNO-6' ? 6 : 1;
  const per = machine === 'BITTAL' ? 1.5 * ((P.U1 + 1) * (P.L1 ? 1 : 0) + (P.U2 + 1) * (P.L2 ? 1 : 0)) + 2 : machine === 'BUNO-6' ? 3 : 4;
  report.cpu = `~${Math.round(per * voices)}% of a core with ${voices} voice${voices > 1 ? 's' : ''} (estimate)`;
  report.id = id.toString(16).toUpperCase().padStart(4, '0');
  report.size = size;
  return { bytes: out, report };
}

// ------------------------------------------------------------ bancos de la DX7 (.syx) para BX-7
// Un volcado de 32 voces (F0 43 0n 09 20 00, 4096 bytes empaquetados, suma, F7) a un banco de
// BX-7: las 32 voces sin empaquetar (156 bytes, el formato del motor msfa) detrás de la cabecera.
export function syxToBsnap(syx, opts = {}) {
  const u = syx instanceof Uint8Array ? syx : new Uint8Array(syx);
  let at = -1;
  for (let i = 0; i + 6 < u.length; i++)
    if (u[i] === 0xF0 && u[i + 1] === 0x43 && u[i + 3] === 0x09 && u[i + 4] === 0x20 && u[i + 5] === 0x00) { at = i + 6; break; }
  if (at < 0 || at + 4096 > u.length) throw new Error('not a DX7 32-voice bank (.syx)');
  const d = u.subarray(at, at + 4096);
  let sum = 0;
  for (const b of d) sum = (sum + b) & 0x7F;
  const report = { kept: ['32 VOICES'], approx: [], dropped: [], cpu: '' };
  if (at + 4096 < u.length && ((128 - sum) & 0x7F) !== u[at + 4096]) report.approx.push('CHECKSUM DOES NOT MATCH');
  const size = 88 + 32 * 156, out = new Uint8Array(size);
  out.set([0x42, 0x4E, 0x53, 0x31], 0);
  out[4] = 4; out[5] = 0; out[6] = opts.lic ?? LIC.OWN; out[7] = 1;      // 4 = ENG_FM
  out[10] = size & 255; out[11] = size >> 8;
  const names = [];
  for (let v = 0; v < 32; v++) {
    const p = d.subarray(v * 128, v * 128 + 128), o = 88 + v * 156;
    for (let op = 0; op < 6; op++) {                     // OP6 primero, como en la DX7
      const s = p.subarray(op * 17, op * 17 + 17), q = o + op * 21;
      for (let i = 0; i < 11; i++) out[q + i] = s[i];    // R1-4, L1-4, BP, LD, RD
      out[q + 11] = s[11] & 3; out[q + 12] = (s[11] >> 2) & 3;          // LC, RC
      out[q + 13] = s[12] & 7; out[q + 20] = (s[12] >> 3) & 15;         // RS, DET
      out[q + 14] = s[13] & 3; out[q + 15] = (s[13] >> 2) & 7;          // AMS, KVS
      out[q + 16] = s[14];                                               // OL
      out[q + 17] = s[15] & 1; out[q + 18] = (s[15] >> 1) & 31;         // M, FC
      out[q + 19] = s[16];                                               // FF
    }
    for (let i = 0; i < 8; i++) out[o + 126 + i] = p[102 + i];          // PEG
    out[o + 134] = p[110] & 31;                                          // ALG
    out[o + 135] = p[111] & 7; out[o + 136] = (p[111] >> 3) & 1;         // FB, OKS
    for (let i = 0; i < 4; i++) out[o + 137 + i] = p[112 + i];          // LFS LFD LPMD LAMD
    out[o + 141] = p[116] & 1; out[o + 142] = (p[116] >> 1) & 7; out[o + 143] = (p[116] >> 4) & 7;   // LKS LFW LPMS
    out[o + 144] = p[117];                                               // TRANSPOSE
    for (let i = 0; i < 10; i++) out[o + 145 + i] = p[118 + i] & 0x7F;  // NAME
    out[o + 155] = 0x3F;                                                 // los 6 operadores encendidos
    names.push(String.fromCharCode(...p.subarray(118, 128)).trim());
  }
  out.set(ascii(opts.name || names[0] || 'DX7 BANK', 12), 12);
  out.set(ascii(opts.author || '', 12), 24);
  const id = fnv16(out);
  out[8] = id & 255; out[9] = id >> 8;
  report.id = id.toString(16).toUpperCase().padStart(4, '0');
  report.size = size;
  report.names = names;
  report.cpu = 'BX-7: ~3% of a core per voice (measured)';
  return { bytes: out, report };
}
