#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Sube un audio al FM-1 como toma del sampler (T2), por la consola MIDI.

  py -3 tools/fm1/toma.py archivo.wav --bpm 160 [--compases 2]

Convierte con ffmpeg a 16 kHz, mono, 8 bits (el formato de las tomas), normaliza el pico y lo
manda en pedazos de 256 bytes. En el FM-1 queda como una toma recién grabada: suena ya como
LOOP A y se guarda en la biblioteca de tomas al parar (STOP), como las que graba la grabadora.
Hasta 3 s (49 152 muestras) por toma y 6 s entre todas, si queda lugar en la zona que comparten tomas y sonidos. El audio no se copia a ningún lado más: solo viaja al aparato.
Cierra antes el instalador web u otro programa que use el MIDI del FM-1.
"""
import argparse, os, subprocess, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola  # noqa: E402
from fm1_midi import Puerto  # noqa: E402

SMP_RATE = 16000
REC_MAX = 49152                     # core/bm_config.h: BM_SMP_REC_MAX (la RAM de grabación)
TROZO = 256


def convertir(ruta):
    r = subprocess.run(['ffmpeg', '-v', 'error', '-i', ruta, '-ac', '1', '-ar', str(SMP_RATE), '-f', 's16le', '-'],
                       capture_output=True)
    if r.returncode:
        sys.exit('ffmpeg no pudo leer %s: %s' % (ruta, r.stderr.decode(errors='replace').strip()))
    import array, random
    a = array.array('h', r.stdout)
    if not a:
        sys.exit('el archivo no tiene audio')
    pico = max(abs(x) for x in a) or 1
    g = 0.95 * 127.0 / pico
    rnd = random.Random(1)
    out = bytearray()
    for x in a:                                   # 8 bits con ruido triangular (dither)
        v = x * g + (rnd.random() - rnd.random())
        v = max(-128, min(127, int(round(v))))
        out.append(v & 0xFF)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('archivo')
    ap.add_argument('--bpm', type=float, required=True, help='tempo del audio (para que siga al proyecto)')
    ap.add_argument('--compases', type=int, default=0, help='solo para comprobar el largo')
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    datos = convertir(a.archivo)
    if len(datos) > REC_MAX:
        sys.exit('dura %.2f s: el máximo de una toma es %.2f s' % (len(datos) / SMP_RATE, REC_MAX / SMP_RATE))
    bpm_x10 = int(round(a.bpm * 10))
    if a.compases:
        esperado = SMP_RATE * 60.0 / a.bpm * 4 * a.compases
        print('largo %d muestras; %d compases a %.1f BPM serían %d' % (len(datos), a.compases, a.bpm, esperado))
    with Puerto('Bubba') as p:
        def pedido(sub, x=0, y=0, cuerpo=b''):
            cab = bytes([sub]) + x.to_bytes(4, 'big') + y.to_bytes(2, 'big')
            r = consola.pedir(p, 0x20, cab + cuerpo)
            if len(r) < 2 or r[1] != 1:
                sys.exit('el FM-1 rechazó el paso %d' % sub)
        pedido(0)
        time.sleep(0.2)                           # el audio suelta la arena
        for off in range(0, len(datos), TROZO):
            pedido(1, off, 0, datos[off:off + TROZO])
            if off // TROZO % 32 == 0:
                print('  %d%%' % (100 * off // len(datos)), flush=True)
        pedido(2, len(datos), bpm_x10)
    print('listo: %.2f s a %.1f BPM en el LOOP A de T2; se guarda en TOMAS al parar' % (len(datos) / SMP_RATE, a.bpm))


if __name__ == '__main__':
    main()
