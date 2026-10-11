// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Erbubar23
//
// El empaquetador del navegador (web/fm1_pack.js) tiene que dar los mismos bytes que
// tools/fm1/fwsc.py.
//
//   node tests/test_pack.mjs FM-1.fwsc APP.bin PAQUETE_DE_FWSC_PY.fwsc
import { readFileSync } from "node:fs";
import { armarPaquete } from "../web/fm1_pack.js";

const [v15p, appp, pyp] = process.argv.slice(2);
if (!pyp) { console.log("uso: node tests/test_pack.mjs FM-1.fwsc APP.bin PAQUETE.fwsc"); process.exit(2); }
const v15 = new Uint8Array(readFileSync(v15p));
let fallos = 0;
const ok = (c, m) => { console.log(`${c ? "ok  " : "FALLA"} ${m}`); if (!c) fallos++; };
const igual = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);

const py = new Uint8Array(readFileSync(pyp));
const js = armarPaquete(v15, new Uint8Array(readFileSync(appp)));
ok(igual(js, py), `el paquete del navegador es idéntico al de fwsc.py (${js.length} bytes)`);
let tooBig = false;
try { armarPaquete(v15, new Uint8Array(600000)); } catch (_) { tooBig = true; }
ok(tooBig, "una app que no cabe se rechaza");
process.exit(fallos ? 1 : 0);
