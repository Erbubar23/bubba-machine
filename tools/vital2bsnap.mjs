#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
// Un preset de Vital o un banco de la DX7 a un sonido de Bubba Machine (.bsnap), desde la consola.
// Es lo mismo que hace el editor web (web/sonidos.html) con web/bsnap.js.
//
//   node tools/vital2bsnap.mjs preset.vital --machine BITTAL [--name "MY LEAD"] [-o salida.bsnap]
//   node tools/vital2bsnap.mjs banco.syx [-o salida.bsnap]
// Máquinas: BITTAL (T5), B-MINI (T3), BUNO-6 (T6). Después: py -3 tools/fm1/sonido.py salida.bsnap
import fs from 'node:fs';
import path from 'node:path';
import { vitalToBsnap, syxToBsnap } from '../web/bsnap.js';

const args = process.argv.slice(2);
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : d; };
const src = args.find(a => !a.startsWith('-') && args[args.indexOf(a) - 1]?.startsWith('-') !== true);
if (!src) { console.error('uso: node tools/vital2bsnap.mjs archivo.vital|archivo.syx [--machine BITTAL|B-MINI|BUNO-6] [--name N] [-o salida]'); process.exit(2); }
const out = opt('-o', src.replace(/\.(vital|syx)$/i, '') + '.bsnap');
let r;
if (/\.syx$/i.test(src)) r = syxToBsnap(fs.readFileSync(src), { name: opt('--name') });
else r = vitalToBsnap(JSON.parse(fs.readFileSync(src, 'utf8')), { machine: opt('--machine', 'BITTAL'), name: opt('--name') });
fs.writeFileSync(out, r.bytes);
const rep = r.report;
console.log(`${path.basename(out)}: ${rep.size} bytes, #${rep.id}`);
if (rep.names) console.log('  voces: ' + rep.names.join(', '));
if (rep.kept.length) console.log('  entra: ' + rep.kept.join(', '));
if (rep.approx.length) console.log('  aproximado: ' + rep.approx.join(', '));
if (rep.dropped.length) console.log('  queda fuera: ' + rep.dropped.join(', '));
console.log('  ' + rep.cpu);
