#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""A2 LATE harness over USB MIDI (consola lab).

  py -3 tools/fm1/medir_a2.py [--segundos 120] [--escena A|B|C]
  py -3 tools/fm1/medir_a2.py --flash-stress [--veces 20]
  py -3 tools/fm1/medir_a2.py --dual 0|1

Selects HOME → pattern 02 → scene → PLAY, reports LATE/halves delta and saves perfil.
Flash-stress: while stopped, lab 4; checks halves keep advancing (Class F keep-alive).
"""
import argparse, datetime, os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola  # noqa: E402
import traza  # noqa: E402
from fm1_midi import Puerto  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BTN_HOME, BTN_PLAY = 8, 12
# Chromatic: white 1 = pattern 02; black 7/8/9 = scenes A/B/C (bm_input SCENE_B0=7)
KEY_PAT02, KEY_SCENE = 2, {'A': 17, 'B': 20, 'C': 22}


def lab_tap(p, n, arg=0, wait=0.35):
    consola.pedir(p, 0x10, bytes([n, arg & 0xFF]))
    time.sleep(wait)


def tap_btn(p, btn):
    lab_tap(p, 7, btn)


def tap_key(p, key):
    lab_tap(p, 8, key)


def select_a2(p, escena='A'):
    tap_btn(p, BTN_HOME)
    time.sleep(0.4)
    tap_key(p, KEY_PAT02)
    time.sleep(0.5)
    tap_key(p, KEY_SCENE[escena])
    time.sleep(0.5)


def medir(p, segundos, escena):
    select_a2(p, escena)
    consola.pedir(p, 0x04)  # cero máximos
    time.sleep(0.2)
    antes = consola.estado(p)
    tap_btn(p, BTN_PLAY)
    time.sleep(1.0)
    t_end = time.time() + segundos
    mid = None
    while time.time() < t_end:
        time.sleep(min(5.0, max(0.5, t_end - time.time())))
        try:
            mid = consola.estado(p)
            print('… t+%.0fs late=%d halves=%d render_max=%d par_modo=%d' % (
                segundos - (t_end - time.time()), mid['late'], mid['halves'],
                mid['render_max_us'], mid.get('par_modo', -1)), flush=True)
        except SystemExit:
            pass
    tap_btn(p, BTN_PLAY)
    time.sleep(0.8)
    despues = consola.estado(p)
    d_late = despues['late'] - antes['late']
    d_halves = despues['halves'] - antes['halves']
    out = [
        'medir_a2 escena %s, %.0f s, reloj %d MHz' % (escena, segundos, despues['sys_hz'] // 1000000),
        'antes:  late=%d halves=%d' % (antes['late'], antes['halves']),
        'despues: late=%d halves=%d render_avg=%d render_max=%d budget=%d' % (
            despues['late'], despues['halves'], despues['render_avg_us'],
            despues['render_max_us'], despues['budget_us']),
        'delta LATE=%d  delta halves=%d  rate=%.2f late/s' % (
            d_late, d_halves, d_late / segundos if segundos else 0),
        'par_modo=%d dual path (2=listo)' % despues.get('par_modo', -1),
        'voces=%s' % despues.get('voces'),
    ]
    print('\n'.join(out), flush=True)
    print('perfil…', flush=True)
    # PLAY again briefly for perfil --play style (already stopped; toggle play)
    tap_btn(p, BTN_PLAY)
    time.sleep(0.5)
    perfil = traza.perfil(p, play=False, segundos=10)
    tap_btn(p, BTN_PLAY)
    time.sleep(0.3)
    texto = '\n'.join(out) + '\n\n' + perfil
    path = traza.guardar('medir-a2-%s' % escena.lower(), texto)
    print(perfil, end='')
    print('guardado en', path)
    return d_late, d_halves, path


def flash_stress(p, veces):
    """Class F: halves must keep advancing across quiet flash writes while stopped."""
    tap_btn(p, BTN_HOME)
    time.sleep(0.3)
    # ensure stopped
    st = consola.estado(p)
    # If somehow playing, stop (PLAY toggles)
    # We cannot know playing from estado; lab assumes stopped for flash writes while playing waits.
    consola.pedir(p, 0x04)
    time.sleep(0.2)
    a = consola.estado(p)
    h0, l0 = a['halves'], a['late']
    print('flash stress: halves=%d late=%d → lab 4 x%d' % (h0, l0, veces), flush=True)
    # fire lab 4; poll halves while it runs (consola may stall during erase)
    consola.pedir(p, 0x10, bytes([4, veces & 0xFF]))
    samples = []
    t0 = time.time()
    while time.time() - t0 < 90:
        time.sleep(0.4)
        try:
            v = consola.estado(p)
            samples.append((time.time() - t0, v['halves'], v['late']))
            print('  t+%.1f halves=%d (+%d) late=%d (+%d)' % (
                samples[-1][0], v['halves'], v['halves'] - h0, v['late'], v['late'] - l0), flush=True)
            if len(samples) >= 3 and samples[-1][1] > samples[0][1] + 20:
                # keep sampling a bit more then check log done
                pass
            # stop when halves stop growing for a while after growth AND enough time
            if len(samples) >= 8 and samples[-1][1] == samples[-4][1] and samples[-1][1] > h0 + 50:
                break
        except SystemExit:
            samples.append((time.time() - t0, -1, -1))
    time.sleep(1.0)
    b = consola.estado(p)
    d_h, d_l = b['halves'] - h0, b['late'] - l0
    ok = d_h > 50  # ~0.3 s of halves at 5.8 ms → plenty during multi-sector flash
    texto = [
        'flash-stress veces=%d' % veces,
        'halves %d → %d (delta %d)  late %d → %d (delta %d)' % (h0, b['halves'], d_h, l0, b['late'], d_l),
        'keep-alive %s' % ('OK' if ok else 'FAIL'),
        'samples: ' + ', '.join('%.1f:%s' % (t, h) for t, h, _ in samples if h >= 0),
    ]
    body = '\n'.join(texto) + '\n'
    path = traza.guardar('flash-stress', body)
    print(body, end='')
    print('guardado en', path)
    return ok, d_h, d_l


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--segundos', type=float, default=120)
    ap.add_argument('--escena', choices=list(KEY_SCENE), default='A')
    ap.add_argument('--flash-stress', action='store_true')
    ap.add_argument('--veces', type=int, default=20)
    ap.add_argument('--dual', type=int, choices=(0, 1), default=None)
    ap.add_argument('--puerto', default='Bubba')
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    with Puerto(a.puerto) as p:
        if a.dual is not None:
            lab_tap(p, 9, a.dual)
            print('lab 9: dual_core=%d' % a.dual)
            time.sleep(0.5)
            v = consola.estado(p)
            print('par_modo=%d' % v.get('par_modo', -1))
        if a.flash_stress:
            ok, _, _ = flash_stress(p, a.veces)
            sys.exit(0 if ok else 2)
        medir(p, a.segundos, a.escena)


if __name__ == '__main__':
    main()
