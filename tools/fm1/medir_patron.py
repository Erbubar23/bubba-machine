#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Drive a factory pattern/scene over USB MIDI and optionally record line-in.

  py -3 tools/fm1/medir_patron.py [--patron 1] [--escena A] [--segundos 75] [--grabar]
  py -3 tools/fm1/medir_patron.py --patron 1 --escena B --segundos 15 --grabar

HOME → white key (pattern) → scene → cero → PLAY; polls LATE; optional arecord WAV.
"""
import argparse, datetime, os, signal, subprocess, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola  # noqa: E402
import traza  # noqa: E402
from fm1_midi import Puerto  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BTN_HOME, BTN_PLAY = 8, 12
# White 0..8 → chromatic (bm_key_is_black: F-based); scenes A-D = black 7..10
KEY_WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14]
KEY_SCENE = {'A': 17, 'B': 20, 'C': 22, 'D': 25}
DEFAULT_CAPTURE = 'hw:2,0'  # C-Media USB Audio Device


def lab_tap(p, n, arg=0, wait=0.35):
    consola.pedir(p, 0x10, bytes([n, arg & 0xFF]))
    time.sleep(wait)


def tap_btn(p, btn):
    lab_tap(p, 7, btn)


def tap_key(p, key):
    lab_tap(p, 8, key)


def select_patron(p, patron, escena):
    """patron is 1-based factory slot (1=DA FUNK)."""
    wi = patron - 1
    if wi < 0 or wi >= len(KEY_WHITE):
        raise SystemExit('patron fuera de rango (1..%d)' % len(KEY_WHITE))
    tap_btn(p, BTN_HOME)
    time.sleep(0.4)
    tap_key(p, KEY_WHITE[wi])
    time.sleep(0.5)
    tap_key(p, KEY_SCENE[escena])
    time.sleep(0.5)


def start_record(path, device, rate, channels):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    cmd = [
        'arecord', '-D', device, '-f', 'S16_LE', '-r', str(rate),
        '-c', str(channels), '-t', 'wav', '-q', path,
    ]
    return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def stop_record(proc):
    if not proc:
        return
    proc.send_signal(signal.SIGINT)
    try:
        _, err = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        _, err = proc.communicate()
    if proc.returncode not in (0, None) and err:
        # arecord often exits 1 on SIGINT after writing a valid WAV
        msg = err.decode('utf-8', 'replace').strip()
        if msg and 'Interrupted' not in msg:
            print('arecord:', msg, flush=True)


def medir(p, patron, escena, segundos, grabar, device, rate, channels, perfil):
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    tag = 'p%02d-%s' % (patron, escena.lower())
    wav = os.path.join(ROOT, 'build', 'traza', '%s-%s.wav' % (stamp, tag)) if grabar else None

    select_patron(p, patron, escena)
    consola.pedir(p, 0x04)
    time.sleep(0.2)
    antes = consola.estado(p)

    rec = start_record(wav, device, rate, channels) if grabar else None
    time.sleep(0.3)
    tap_btn(p, BTN_PLAY)
    time.sleep(1.0)
    t_end = time.time() + segundos
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
    time.sleep(0.4)
    stop_record(rec)
    time.sleep(0.4)
    despues = consola.estado(p)
    d_late = despues['late'] - antes['late']
    d_halves = despues['halves'] - antes['halves']
    out = [
        'medir_patron %s escena %s, %.0f s, reloj %d MHz' % (
            tag, escena, segundos, despues['sys_hz'] // 1000000),
        'antes:  late=%d halves=%d' % (antes['late'], antes['halves']),
        'despues: late=%d halves=%d render_avg=%d render_max=%d budget=%d' % (
            despues['late'], despues['halves'], despues['render_avg_us'],
            despues['render_max_us'], despues['budget_us']),
        'delta LATE=%d  delta halves=%d  rate=%.2f late/s' % (
            d_late, d_halves, d_late / segundos if segundos else 0),
        'par_modo=%d' % despues.get('par_modo', -1),
        'voces=%s' % despues.get('voces'),
    ]
    if wav:
        out.append('wav: %s' % wav)
    print('\n'.join(out), flush=True)

    if perfil:
        print('perfil…', flush=True)
        tap_btn(p, BTN_PLAY)
        time.sleep(0.5)
        texto_perfil = traza.perfil(p, play=False, segundos=10)
        tap_btn(p, BTN_PLAY)
        time.sleep(0.3)
        out.append('')
        out.append(texto_perfil.rstrip())
        print(texto_perfil, end='')

    path = traza.guardar('medir-%s' % tag, '\n'.join(out) + '\n')
    print('guardado en', path)
    return {
        'delta_late': d_late,
        'delta_halves': d_halves,
        'wav': wav,
        'report': path,
        'antes': antes,
        'despues': despues,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--patron', type=int, default=1, help='1=DA FUNK, 2=BLADE, …')
    ap.add_argument('--escena', choices=list(KEY_SCENE), default='A')
    ap.add_argument('--segundos', type=float, default=75)
    ap.add_argument('--grabar', action='store_true', help='record USB line-in in parallel')
    ap.add_argument('--device', default=os.environ.get('FM1_CAPTURE', DEFAULT_CAPTURE))
    ap.add_argument('--rate', type=int, default=44100)
    ap.add_argument('--channels', type=int, default=1)
    ap.add_argument('--perfil', action='store_true')
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
        r = medir(p, a.patron, a.escena, a.segundos, a.grabar, a.device, a.rate, a.channels, a.perfil)
        if a.grabar and r['wav']:
            print('WAV listo:', r['wav'])


if __name__ == '__main__':
    main()
