#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Consola de Bubba Machine en el FM-1, por USB-MIDI (hal/fm1/fm1_consola.c). Solo lee.

  py -3 tools/fm1/consola.py estado [--cada 1]    mediciones (audio, relojes, arranques…)
  py -3 tools/fm1/consola.py leer 0x1C7FD80 32    memoria o registros
  py -3 tools/fm1/consola.py flash 0xBF000 64     flash
  py -3 tools/fm1/consola.py cero                 pone a cero los máximos
  py -3 tools/fm1/consola.py log                  el registro de texto del firmware
  py -3 tools/fm1/consola.py lab N [ARG]          laboratorio (en modo seguro): 1 barrido, 2 audio en
                                                  silencio, 3 tono, 4 flash ARG veces, 5 audio detenido,
                                                  6 arrancar Bubba con audio

Cierra antes el instalador web u otro programa que use el MIDI del FM-1. Lee el aparato y no
escribe nada, salvo la prueba de flash del laboratorio (sector de prueba 0xE1000).
"""
import argparse, os, struct, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fm1_midi import Puerto  # noqa: E402

CAMPOS = ['version', 'uptime_ms', 'sys_hz', 'lsb_hz', 'halves', 'late', 'render_us', 'render_max_us',
          'render_avg_us', 'budget_us', 'scan_ticks', 'scanisr_max_us', 'ui_us', 'ui_max_us', 'usb', 'fails',
          'past1', 'past2', 'past3', 'past4', 'audio_cpu', 'midi_in', 'midi_out', 'loop_max']
ETAPA = ['antes de la pantalla', 'pantalla y USB', 'núcleo listo', 'teclas', 'audio iniciado', 'audio sonando',
         '1 s andando', '5 s andando', '5 s andando']
FIN = {1: 'apagado', 2: 'perro guardián', 3: 'reinicio por falla', 4: 'otro', 0xFF: '-'}


def pedir(p, cmd, datos=b''):
    nib = bytearray()
    for b in datos:
        nib += bytes([b >> 4, b & 15])
    p.enviar(bytes([0xF0, 0x7D, 0x42, 0x4D, cmd]) + bytes(nib) + b'\xF7')
    r = p.recibir(2.0, hasta=lambda m: len(m) > 5 and m[1:5] == bytes([0x7D, 0x42, 0x4D, cmd | 0x40]))
    for m in r:
        if len(m) > 5 and m[1:5] == bytes([0x7D, 0x42, 0x4D, cmd | 0x40]):
            n = m[5:-1]
            return bytes((n[i] << 4) | n[i + 1] for i in range(0, len(n) - 1, 2))
    raise SystemExit('consola: el FM-1 no contestó (¿Bubba Machine con consola? ¿puerto libre?)')


def leer_log(p):
    texto, off = b'', 0
    while True:
        d = pedir(p, 0x07, off.to_bytes(2, 'big'))
        n = d[2]
        texto += d[3:3 + n]
        off += n
        if n < 64:
            return texto.decode('utf-8', 'replace')


def estado(p):
    d = pedir(p, 0x01)
    v = dict(zip(CAMPOS, struct.unpack('<%dI' % (len(d) // 4), d)))
    return v


def mostrar(v):
    pct = 100.0 * v['render_avg_us'] / v['budget_us'] if v['budget_us'] else 0
    pmx = 100.0 * v['render_max_us'] / v['budget_us'] if v['budget_us'] else 0
    print('versión %06X%s  en marcha %.1f s  reloj %d MHz (lsb %d MHz)' % (
        v['version'] & 0xFFFFFF, '  SIN SALIDA DE AUDIO' if v['version'] >> 24 else '', v['uptime_ms'] / 1000,
        v['sys_hz'] // 1000000, v['lsb_hz'] // 1000000))
    print('audio: %d bloques, %d tarde | cálculo %d us (prom %d, máx %d) de %d us = %.0f %% (máx %.0f %%) | núcleo %d' % (
        v['halves'], v['late'], v['render_us'], v['render_avg_us'], v['render_max_us'], v['budget_us'], pct, pmx,
        v['audio_cpu']))
    print('pantalla/UI: %d us (máx %d) | barrido: %d interrupciones, la más larga %d us' % (
        v['ui_us'], v['ui_max_us'], v['scan_ticks'], v['scanisr_max_us']))
    print('USB: %s | MIDI recibidos %d, enviados %d | arranques fallidos seguidos: %d' % (
        'configurado' if v['usb'] & 1 else ('encendido' if v['usb'] & 2 else 'apagado'), v['midi_in'], v['midi_out'],
        v['fails']))
    for k in range(1, 5):
        x = v['past%d' % k]
        if x >> 24:
            e, fin, ok = x & 0xFF, (x >> 8) & 0xFF, (x >> 16) & 0xFF
            print('  arranque -%d: llegó a %s, %s' % (k, ETAPA[min(e, 8)], 'bien' if ok else 'terminó: ' + FIN.get(fin, '?')))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--puerto', default='Bubba')
    sub = ap.add_subparsers(dest='orden', required=True)
    s = sub.add_parser('estado'); s.add_argument('--cada', type=float, default=0); s.add_argument('--json', action='store_true')
    for n in ('leer', 'flash'):
        s = sub.add_parser(n); s.add_argument('adr'); s.add_argument('n', type=int)
    sub.add_parser('cero')
    sub.add_parser('log')
    s = sub.add_parser('lab'); s.add_argument('n', type=int); s.add_argument('arg', type=int, nargs='?', default=0)
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    with Puerto(a.puerto) as p:
        if a.orden == 'estado':
            while True:
                v = estado(p)
                if a.json:
                    import json
                    print(json.dumps(v))
                else:
                    mostrar(v)
                if not a.cada:
                    break
                print()
                time.sleep(a.cada)
        elif a.orden in ('leer', 'flash'):
            adr = int(a.adr, 0)
            d = pedir(p, 0x02 if a.orden == 'leer' else 0x03, adr.to_bytes(4, 'big') + bytes([min(a.n, 64)]))
            data = d[5:]
            for i in range(0, len(data), 16):
                print('%08X  %s' % (adr + i, data[i:i + 16].hex(' ')))
        elif a.orden == 'log':
            print(leer_log(p), end='')
        elif a.orden == 'lab':
            pedir(p, 0x10, bytes([a.n, a.arg]))
            antes = len(leer_log(p))
            time.sleep(0.5)
            fin = time.time() + 60
            texto = leer_log(p)
            while time.time() < fin and len(texto) == antes:
                time.sleep(0.5)
                texto = leer_log(p)
            time.sleep(0.5)
            print(leer_log(p)[antes:], end='')
        else:
            pedir(p, 0x04)
            print('máximos en cero')


if __name__ == '__main__':
    main()
