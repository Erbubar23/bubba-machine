#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Traza de la arquitectura del FM-1: corre las pruebas del laboratorio y el perfil del audio,
junta los registros y guarda los resultados (hal/fm1/fm1_bench.c, docs/ARQUITECTURA_FM1.md).

  py -3 tools/fm1/traza.py pruebas          # en modo seguro: código, datos, núcleos, pantalla
  py -3 tools/fm1/traza.py perfil [--play] [--segundos 10]
                                            # con Bubba andando: dónde se va el tiempo del audio
  py -3 tools/fm1/traza.py motor 9 --canal 4 --fuente 6 [--voces 1,2,4,7]
                                            # costo de un motor por voz: el canal pasa a ese motor
                                            # (número de ENG_* en core/bm_pattern.h) y suena por MIDI

Los resultados quedan en build/traza/AAAAMMDD-HHMMSS-*.txt y se imprimen.
"""
import argparse, datetime, os, struct, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola  # noqa: E402
from fm1_midi import Puerto  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SECCIONES = ['comandos', 'secuenciador', 'canal 1', 'canal 2', 'canal 3', 'canal 4', 'canal 5', 'canal 6', 'mezcla y efectos']


def lab(p, n, arg=0, espera=60):
    antes = len(consola.leer_log(p))
    consola.pedir(p, 0x10, bytes([n, arg]))
    fin, texto = time.time() + espera, ''
    quieto = 0
    while time.time() < fin:
        time.sleep(0.5)
        try:
            nuevo = consola.leer_log(p)
        except SystemExit:
            continue                       # ocupado midiendo: la consola contesta después
        if len(nuevo) > len(texto):
            texto, quieto = nuevo, 0
        elif len(texto) > antes:
            quieto += 1
            if quieto >= 4:
                break
    return texto[antes:]


def guardar(nombre, texto):
    d = os.path.join(ROOT, 'build', 'traza')
    os.makedirs(d, exist_ok=True)
    f = os.path.join(d, datetime.datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + nombre + '.txt')
    open(f, 'w', encoding='utf-8').write(texto)
    return f


def pruebas(p):
    out = []
    v = consola.estado(p)
    out.append('reloj %d MHz, bus lento %d MHz' % (v['sys_hz'] // 1000000, v['lsb_hz'] // 1000000))
    for n, nombre in ((21, 'código'), (22, 'datos'), (23, 'caché'), (24, 'núcleos'), (25, 'pantalla')):
        print('…', nombre, flush=True)
        out.append(lab(p, n, espera=90).rstrip())
    return '\n'.join(out) + '\n'


def perfil(p, play, segundos):
    if play:
        consola.pedir(p, 0x10, bytes([7, 12]))
        time.sleep(1)
    consola.pedir(p, 0x10, bytes([20, 0]))
    time.sleep(segundos)
    d = consola.pedir(p, 0x08)
    w = struct.unpack('<%dI' % (len(d) // 4), d)
    acc, total, hz = w[:len(SECCIONES)], w[len(SECCIONES)], w[len(SECCIONES) + 1]
    v = consola.estado(p)
    seg = total / hz if hz else 1
    out = ['perfil del audio, %.1f s %s, reloj %d MHz' % (seg, 'con PLAY' if play else 'en reposo', v['sys_hz'] // 1000000),
           '%-18s %10s %8s' % ('sección', 'ms/s', '% núcleo')]
    suma = 0
    for nombre, a in zip(SECCIONES, acc):
        ms = a / hz * 1000 / seg
        suma += ms
        out.append('%-18s %10.2f %7.1f %%' % (nombre, ms, ms / 10))
    out.append('%-18s %10.2f %7.1f %%' % ('total audio', suma, suma / 10))
    out.append('pantalla/UI: %d us por cuadro (máx %d); barrido: interrupción más larga %d us' % (
        v['ui_us'], v['ui_max_us'], v['scanisr_max_us']))
    out.append('bloques tarde: %d de %d' % (v['late'], v['halves']))
    return '\n'.join(out) + '\n'


def leer_perfil(p):
    d = consola.pedir(p, 0x08)
    w = struct.unpack('<%dI' % (len(d) // 4), d)
    acc, total, hz = w[:len(SECCIONES)], w[len(SECCIONES)], w[len(SECCIONES) + 1]
    seg = total / hz if hz else 1
    return [a / hz * 1000 / seg / 10 for a in acc]          # % de un núcleo por sección


def motor(p, eng, canal, fuente, voces, segundos):
    t = canal - 1
    consola.pedir(p, 0x10, bytes([26, (t << 4) | eng]))
    time.sleep(0.3)
    consola.pedir(p, 0x10, bytes([27, (t << 5) | fuente]))
    time.sleep(0.3)
    v = consola.estado(p)
    out = ['motor %d en el canal %d, fuente %d, reloj %d MHz' % (eng, canal, fuente, v['sys_hz'] // 1000000),
           '%6s %10s %10s %10s' % ('voces', 'canal %', 'por voz %', 'total %')]
    notas = [48, 55, 60, 64, 67, 71, 74, 79]
    for n in voces:
        for k in range(n):
            p.corto(0x90 | t, notas[k], 100)
        time.sleep(0.8)
        consola.pedir(p, 0x10, bytes([20, 0]))
        time.sleep(segundos)
        pct = leer_perfil(p)
        for k in range(n):
            p.corto(0x80 | t, notas[k], 0)
        c = pct[2 + t]
        out.append('%6d %9.1f%% %9.2f%% %9.1f%%' % (n, c, c / n if n else 0, sum(pct)))
        time.sleep(2.5)
    consola.pedir(p, 0x10, bytes([32, 0]))                  # el canal vuelve a como estaba
    time.sleep(0.5)
    v = consola.estado(p)
    out.append('bloques tarde: %d de %d' % (v['late'], v['halves']))
    return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest='orden', required=True)
    sub.add_parser('pruebas')
    s = sub.add_parser('una'); s.add_argument('n', type=int); s.add_argument('arg', type=int, nargs='?', default=0)
    s = sub.add_parser('perfil'); s.add_argument('--play', action='store_true'); s.add_argument('--segundos', type=float, default=10)
    s = sub.add_parser('motor'); s.add_argument('eng', type=int); s.add_argument('--canal', type=int, default=4)
    s.add_argument('--fuente', type=int, default=0); s.add_argument('--voces', default='1,2,4,7')
    s.add_argument('--segundos', type=float, default=3)
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    with Puerto('Bubba') as p:
        if a.orden == 'pruebas':
            texto = pruebas(p)
        elif a.orden == 'motor':
            texto = motor(p, a.eng, a.canal, a.fuente, [int(x) for x in a.voces.split(',')], a.segundos)
        elif a.orden == 'una':
            texto = lab(p, a.n, a.arg, espera=120)
        else:
            texto = perfil(p, a.play, a.segundos)
    print(texto)
    print('guardado en', guardar(a.orden, texto))


if __name__ == '__main__':
    main()
