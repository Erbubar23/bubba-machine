#!/usr/bin/env python3
"""Genera core/gen/bm_loops.c: loops de fábrica del sampler (T2), guardados en la ROM.

Lee assets/loops/loops.json, una lista de entradas:
  {"synth": "break160", "name": "BUBBA 160", "bpm": 160.0, "bars": 1}   se sintetiza aquí
  {"file": "x.wav", "name": "MI LOOP", "bpm": 120.0, "bars": 1}         WAV mono, 16 kHz, 8 o 16 bit
Los loops públicos se sintetizan en este script, así el firmware no lleva audio de terceros
y todo se puede redistribuir bajo la GPL.

Con --local también se agregan las entradas de assets/loops/loops.local.json (fuera de git),
para probar con audio propio o de terceros. Un build hecho con --local NO se puede publicar.

Uso: py -3 tools/gen_loops.py [--local]
"""
import argparse, json, math, os, sys, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'assets', 'loops')
DST = os.path.join(ROOT, 'core', 'gen', 'bm_loops.c')
RATE = 16000
MAX_LOOPS = 4
MAX_LEN = 65408          # igual que un slot de flash (BM_SMP_MAX)
BUDGET = 96 * 1024       # presupuesto de ROM para loops de fábrica


def read_wav(path):
    with wave.open(path, 'rb') as w:
        ch, width, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if ch != 1 or sr != RATE or width not in (1, 2):
        sys.exit('%s: tiene que ser mono, %d Hz, 8 o 16 bit (es %d canales, %d Hz, %d bit)' % (path, RATE, ch, sr, width * 8))
    if width == 1:
        return np.frombuffer(raw, np.uint8).astype(np.float64) / 128.0 - 1.0
    return np.frombuffer(raw, '<i2') / 32768.0


# ------------------------------------------------------------ síntesis de loops propios
def _env(n, attack, decay):
    t = np.arange(n) / RATE
    return np.minimum(1.0, t / max(attack, 1e-4)) * np.exp(-t / decay)


def _lp(x, fc):
    a = 1.0 - math.exp(-2 * math.pi * fc / RATE)
    y = np.empty_like(x); s = 0.0
    for i, v in enumerate(x):
        s += a * (v - s); y[i] = s
    return y


def _place(buf, snd, at):
    at = int(at)
    n = min(len(snd), len(buf) - at)
    if n > 0:
        buf[at:at + n] += snd[:n]


def synth_break(bpm, style, seed):
    """Un compás de batería sintetizada. style: 'hard' (bombo distorsionado, rumble,
    hats al contratiempo) o 'groove' (bombo limpio, hats en semicorcheas, clap)."""
    rng = np.random.default_rng(seed)
    step = RATE * 60.0 / bpm / 4.0
    n = int(round(step * 16))
    out = np.zeros(n)
    hard = style == 'hard'
    # bombo: barrido de seno + saturación
    kn = int(RATE * (0.32 if hard else 0.26))
    t = np.arange(kn) / RATE
    f = (55 if hard else 50) + (190 if hard else 140) * np.exp(-t / 0.028)
    kick = np.sin(2 * np.pi * np.cumsum(f) / RATE) * _env(kn, 0.0008, 0.16 if hard else 0.12)
    click = rng.uniform(-1, 1, kn) * _env(kn, 0.0001, 0.003) * 0.6
    kick = np.tanh((kick + click) * (4.0 if hard else 1.6)) * 0.9
    # rumble: grave filtrado que entra entre bombos
    rn = int(step * 3.2)
    rum = _lp(np.tanh(np.sin(2 * np.pi * 52 * np.arange(rn) / RATE) * 3) + rng.uniform(-.2, .2, rn), 180) * _env(rn, 0.01, 0.09) * 0.55
    # hats y clap: ruido filtrado
    hn = int(RATE * 0.05)
    hat_raw = rng.uniform(-1, 1, hn)
    hat = (hat_raw - _lp(hat_raw, 5000)) * _env(hn, 0.0005, 0.018)
    on = int(RATE * 0.24)
    open_raw = rng.uniform(-1, 1, on)
    ohat = (open_raw - _lp(open_raw, 4200)) * _env(on, 0.001, 0.07) * 0.6
    cn = int(RATE * 0.18)
    clap_raw = rng.uniform(-1, 1, cn)
    clap = (_lp(clap_raw, 3200) - _lp(clap_raw, 900)) * 3.0
    ce = np.zeros(cn)
    for d in (0, 0.009, 0.018):
        ce += _env(cn, 0.0005, 0.006) * (np.arange(cn) >= int(d * RATE))
    clap = clap * (ce * 0.5 + _env(cn, 0.003, 0.05))
    for s in range(0, 16, 4):
        _place(out, kick, s * step)
        if hard:
            _place(out, rum, (s + 1) * step)
            _place(out, ohat * 0.8, (s + 2) * step)
    if hard:
        for s in (3, 7, 11, 15):
            _place(out, hat * 0.5, s * step)
        _place(out, clap * 0.7, 4 * step); _place(out, clap * 0.7, 12 * step)
    else:
        for s in range(16):
            _place(out, hat * (0.8 if s % 2 else 0.4), s * step)
        _place(out, ohat, 2 * step); _place(out, ohat, 10 * step)
        _place(out, clap, 4 * step); _place(out, clap, 12 * step); _place(out, clap * 0.4, 15 * step)
    out = np.tanh(out * 1.2)
    out = out / (np.abs(out).max() + 1e-9) * 0.92
    fade = int(RATE * 0.003)
    out[:fade] *= np.linspace(0, 1, fade); out[-fade:] *= np.linspace(1, 0, fade)
    return out


SYNTHS = {
    'break160': lambda: synth_break(160.0, 'hard', 160),
    'groove128': lambda: synth_break(128.0, 'groove', 128),
}


def c_array(vals, per=32):
    return '\n'.join('  ' + ','.join(str(int(v)) for v in vals[i:i + per]) + ',' for i in range(0, len(vals), per))


def load_list(path):
    return json.load(open(path, encoding='utf-8')) if os.path.exists(path) else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--local', action='store_true', help='agrega assets/loops/loops.local.json (no publicar ese build)')
    args = ap.parse_args()
    loops = load_list(os.path.join(SRC, 'loops.json'))
    if args.local:
        loops += load_list(os.path.join(SRC, 'loops.local.json'))
        print('AVISO: build con loops locales. No subas web/bm.js ni binarios hechos así.')
    if len(loops) > MAX_LOOPS:
        sys.exit('como mucho %d loops de fábrica' % MAX_LOOPS)
    out = ['/* SPDX-License-Identifier: GPL-3.0-or-later */',
           '/* GENERADO por tools/gen_loops.py: no editar a mano. */', '#include "../bm_rom.h"', '']
    rows, total = [], 0
    for i, lp in enumerate(loops):
        x = SYNTHS[lp['synth']]() if 'synth' in lp else read_wav(os.path.join(SRC, lp['file']))
        d = np.clip(np.round(x * 127), -128, 127).astype(np.int8)
        if len(d) > MAX_LEN:
            sys.exit('%s: %d muestras, el máximo es %d' % (lp['name'], len(d), MAX_LEN))
        name = lp['name'].upper()[:11]
        total += len(d)
        out.append('static const int8_t loop%d[%d] = {\n%s\n};' % (i, len(d), c_array(d)))
        rows.append('  {loop%d, %du, %du, %du, "%s"},' % (i, len(d), int(round(lp['bpm'] * 10)), lp['bars'], name))
        print('loop %d: %-11s %5d B  %.1f BPM  %d compas(es)' % (i, name, len(d), lp['bpm'], lp['bars']))
    out.append('')
    if rows:
        out.append('const bm_rom_loop bm_rom_loops[%d] = {\n%s\n};' % (len(rows), '\n'.join(rows)))
    else:
        out.append('const bm_rom_loop bm_rom_loops[1] = {{0, 0u, 0u, 0u, ""}};')
    out.append('const int bm_rom_nloops = %d;' % len(rows))
    out.append('')
    out.append('/* ROM: loops %d B (presupuesto %d) */' % (total, BUDGET))
    with open(DST, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out) + '\n')
    print('escrito %s  loops=%d B' % (DST, total))
    if total > BUDGET:
        sys.exit('los loops de fábrica pasan el presupuesto de ROM')


if __name__ == '__main__':
    main()
