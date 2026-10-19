#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Genera core/gen/bm_fm_rom.c: los patches de fábrica del motor FM (6 operadores, formato de voz
de la DX7 sin empaquetar, 156 bytes). Son voces propias, programadas aquí con las recetas clásicas
de la síntesis FM (no son los bancos de Yamaha). Bancos .syx propios: desde el editor web.

Uso: py -3 tools/gen_fm.py
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def op(ratio=1.0, ol=99, r=(99, 50, 35, 50), l=(99, 80, 0, 0), kvs=2, rs=2, dt=7, ams=0, fixed=False,
       bp=39, ld=0, rd=0, lc=0, rc=3):
    """Un operador. ratio: 0.5 o coarse * (1 + fine/100). dt: 0..14 (7 = centro)."""
    if fixed:
        fc, ff = int(ratio), 0
    elif ratio < 1.0:
        fc, ff = 0, int(round((ratio / 0.5 - 1) * 100))
    else:
        fc = int(ratio)
        ff = int(round((ratio / fc - 1) * 100))
    assert 0 <= ff <= 99 and 0 <= fc <= 31, ratio
    return list(r) + list(l) + [bp, ld, rd, lc, rc, rs, ams, kvs, ol, 1 if fixed else 0, fc, ff, dt]


OFF = op(ol=0)


def voice(name, alg, ops, fb=0, lfo=(35, 0, 0, 0, 1, 0, 0), pr=(99, 99, 99, 99), pl=(50, 50, 50, 50), trsp=24):
    """ops: OP1..OP6 (en el orden de la DX7). alg: 1..32. lfo: velocidad, retardo, PMD, AMD, sync, onda, PMS."""
    assert len(ops) == 6
    d = []
    for o in reversed(ops):          # el formato guarda primero el OP6
        assert len(o) == 21
        d += o
    d += list(pr) + list(pl) + [alg - 1, fb, 1] + list(lfo[:6]) + [lfo[6], trsp]
    d += list(name.ljust(10)[:10].encode('ascii'))
    d += [0x3F]                       # operadores encendidos
    assert len(d) == 156, len(d)
    return name, d


V = [
    # piano eléctrico de lengüetas de los 80 (receta publicada del E.PIANO 1, sin datos de la ROM):
    # algoritmo 5, tres pares; moduladores a 1, 1 y 14 (el 14:1 es el «tine», cortado arriba de D3),
    # niveles 81 / 91 / 60, sensibilidad a la velocidad alta en los moduladores, detune ±7 entre pares,
    # realimentación 6; los moduladores caen antes que las portadoras, sustain 0
    voice('TINEKEYS', 5, [
        op(1, 99, (96, 25, 25, 67), (99, 75, 0, 0), kvs=2, rs=3),
        op(14, 60, (95, 50, 35, 78), (99, 75, 0, 0), kvs=7, rs=3, bp=41, rd=50, rc=0),
        op(1, 99, (95, 20, 20, 50), (99, 95, 0, 0), kvs=2, rs=3, dt=0),
        op(1, 81, (95, 45, 25, 50), (99, 80, 0, 0), kvs=6, rs=3),
        op(1, 99, (95, 20, 20, 50), (99, 95, 0, 0), kvs=2, rs=3, dt=14),
        op(1, 91, (95, 45, 25, 50), (99, 80, 0, 0), kvs=7, rs=3)], fb=6),
    # Rhodes con tine (Rhodey de la STK): dos pares sumados, 1:0.5 y 1:15, niveles 99 / 90 / 99 / 67,
    # caídas de 1.5 / 1.5 / 1.0 / 0.25 s, sustain 0
    voice('FULLTINE', 5, [
        op(1, 99, (99, 38, 30, 55), (99, 0, 0, 0), kvs=2, rs=3),
        op(0.5, 90, (99, 38, 30, 55), (99, 0, 0, 0), kvs=4, rs=3),
        op(1, 99, (99, 45, 30, 55), (99, 0, 0, 0), kvs=2, rs=3, dt=8),
        op(15, 67, (99, 65, 40, 55), (99, 0, 0, 0), kvs=6, rs=3),
        OFF, OFF], fb=0),
    # bajo de sintetizador de los 80 (el BASS 1 de muchos discos; receta genérica publicada: un par
    # 1:1 con el modulador alto que cae rápido, y otro a 2:1 para el golpe)
    voice('POP BASS', 5, [
        op(0.5, 99, (99, 45, 30, 70), (99, 75, 0, 0), kvs=2, rs=2),
        op(0.5, 86, (99, 70, 40, 70), (99, 35, 0, 0), kvs=5, rs=2),
        op(0.5, 90, (99, 45, 30, 70), (99, 75, 0, 0), kvs=2, rs=2, dt=9),
        op(2, 72, (99, 80, 40, 70), (99, 0, 0, 0), kvs=6, rs=2),
        OFF, OFF], fb=0),
    # bajo de house de fines de los 80, inspirado en un preset de 4 operadores, rehecho para 6 (receta de
    # MusicTech con Dexed): algoritmo 14, portadora a 0.5; OP4 a 0.5 (74), OP5 a 0.5 un poco bajo (66,
    # solo ataque), OP6 a 1 (83) con realimentación 6; se apaga en unos 2 s
    voice('HOUSE BS', 14, [
        op(1, 0, (99, 99, 99, 99), (0, 0, 0, 0)),
        op(1, 0, (99, 99, 99, 99), (0, 0, 0, 0)),
        op(0.5, 99, (99, 30, 25, 60), (99, 85, 0, 0), kvs=2, rs=2),
        op(0.5, 74, (99, 30, 25, 60), (99, 80, 0, 0), kvs=3, rs=2),
        op(0.5, 66, (99, 99, 99, 60), (99, 99, 99, 0), kvs=3, dt=6),
        op(1, 83, (99, 40, 30, 60), (99, 70, 0, 0), kvs=4)], fb=6),
    # bajo sub: casi senoidal, con un golpe de brillo al principio
    voice('SUB BASS', 5, [
        op(0.5, 99, (99, 55, 40, 70), (99, 92, 85, 0), kvs=1),
        op(0.5, 66, (99, 80, 40, 70), (99, 45, 25, 0), kvs=6),
        op(1, 68, (99, 85, 50, 70), (99, 0, 0, 0), kvs=3),
        op(1, 70, (99, 88, 50, 70), (99, 0, 0, 0), kvs=5),
        OFF, OFF], fb=0),
    # campana tubular (TubeBell de la STK): pares a 0.995:1.407 y 1.005:1.414 (casi 1:raíz de 2),
    # niveles 94 / 76 / 99 / 71, caídas de unos 4 s, sustain 0
    voice('TUBECHIM', 5, [
        op(0.995, 94, (99, 30, 30, 30), (99, 0, 0, 0), kvs=2, rs=2),
        op(1.41, 76, (99, 30, 30, 30), (99, 0, 0, 0), kvs=3, rs=2),
        op(1, 99, (99, 30, 30, 30), (99, 0, 0, 0), kvs=2, rs=2, dt=9),
        op(1.41, 71, (99, 32, 30, 30), (99, 0, 0, 0), kvs=3, rs=2, dt=10),
        OFF, OFF], fb=0),
    # campanitas («Practically FM», Sound On Sound 1988): pares 1:3, 2:4 y 4:8, sin realimentación,
    # portadoras a 99, envolventes percusivas, rate scaling 2
    voice('CHIMES', 5, [
        op(1, 99, (99, 45, 30, 45), (99, 0, 0, 0), kvs=2, rs=2),
        op(3, 70, (99, 55, 30, 45), (99, 0, 0, 0), kvs=4, rs=2),
        op(2, 99, (99, 45, 30, 45), (99, 0, 0, 0), kvs=2, rs=2),
        op(4, 66, (99, 55, 30, 45), (99, 0, 0, 0), kvs=4, rs=2),
        op(4, 99, (99, 45, 30, 45), (99, 0, 0, 0), kvs=2, rs=2),
        op(8, 60, (99, 55, 30, 45), (99, 0, 0, 0), kvs=4, rs=2)], fb=0),
    # metales («Practically FM» parte 4, Sound On Sound): algoritmo 13, pares 1:1, moduladores a 85
    # (OP5 a 75), realimentación 3, detunes -1 / -3 / +4 / +7 / +5; los moduladores entran más lento
    voice('BRASS 80', 13, [
        op(1, 99, (72, 76, 99, 71), (99, 88, 96, 0), kvs=2, rs=2, dt=6),
        op(1, 85, (55, 51, 29, 71), (82, 95, 96, 0), kvs=4, dt=4),
        op(1, 99, (72, 76, 99, 71), (99, 88, 96, 0), kvs=2, dt=11),
        op(1, 85, (55, 51, 29, 71), (82, 95, 96, 0), kvs=4, dt=14),
        op(1, 75, (55, 51, 29, 71), (82, 95, 96, 0), kvs=4, dt=12),
        op(1, 85, (55, 51, 29, 71), (82, 95, 96, 0), kvs=4)], fb=3,
        lfo=(32, 40, 6, 0, 0, 4, 3)),
    # cuerdas: tres pares a 1:1, desafinados, ataque y salida lentos, vibrato con retardo
    voice('STRINGS', 5, [
        op(1, 97, (45, 30, 25, 40), (99, 95, 90, 0), kvs=1),
        op(1, 70, (40, 30, 25, 40), (90, 85, 80, 0), kvs=2),
        op(1, 95, (44, 30, 25, 40), (99, 95, 90, 0), kvs=1, dt=4),
        op(1, 68, (40, 30, 25, 40), (90, 85, 80, 0), kvs=2),
        op(1, 95, (46, 30, 25, 40), (99, 95, 90, 0), kvs=1, dt=10),
        op(2, 60, (40, 30, 25, 40), (90, 85, 80, 0), kvs=2)], fb=4,
        lfo=(30, 50, 9, 0, 0, 4, 3)),
    # marimba: pares a 1:3.5, los moduladores caen en 0.1-0.15 s y las portadoras más cortas que una
    # campana (receta publicada de DecentSampler, acortada)
    voice('MARIMBA', 5, [
        op(1, 99, (99, 55, 35, 60), (99, 0, 0, 0), kvs=3, rs=4),
        op(3.5, 75, (99, 85, 40, 60), (99, 0, 0, 0), kvs=5, rs=4),
        op(1, 90, (99, 58, 35, 60), (99, 0, 0, 0), kvs=3, rs=4, dt=9),
        op(3.5, 66, (99, 88, 40, 60), (99, 0, 0, 0), kvs=5, rs=4),
        OFF, OFF], fb=0),
    # flauta: soplido con un poco de ruido de la realimentación
    voice('FLUTE', 5, [
        op(1, 99, (60, 40, 99, 60), (99, 95, 95, 0), kvs=2),
        op(1, 58, (55, 40, 99, 60), (90, 85, 85, 0), kvs=3),
        op(2, 50, (60, 40, 99, 60), (99, 80, 80, 0), kvs=2),
        op(1, 40, (55, 40, 99, 60), (90, 70, 70, 0), kvs=2),
        op(4, 46, (70, 60, 99, 60), (99, 30, 30, 0), kvs=2),
        op(7, 70, (70, 60, 99, 60), (99, 40, 40, 0), kvs=2)], fb=7,
        lfo=(34, 50, 6, 0, 0, 4, 3)),
    # órgano de tubos (receta publicada, algoritmo 19): OP1 y OP2 a 0.5, OP3 a 1, OP4 a 4, OP5 a 2,
    # OP6 a 10; niveles a oído
    voice('PIPE ORG', 19, [
        op(0.5, 99, (70, 99, 99, 60), (99, 99, 99, 0), kvs=0, rs=0),
        op(0.5, 60, (70, 99, 99, 60), (99, 99, 99, 0), kvs=0, rs=0),
        op(1, 55, (70, 99, 99, 60), (99, 99, 99, 0), kvs=0, rs=0),
        op(4, 80, (70, 99, 99, 60), (99, 99, 99, 0), kvs=0, rs=0),
        op(2, 88, (70, 99, 99, 60), (99, 99, 99, 0), kvs=0, rs=0),
        op(10, 45, (70, 99, 99, 60), (99, 99, 99, 0), kvs=0, rs=0)], fb=2,
        lfo=(60, 0, 3, 0, 0, 4, 2)),
    # órgano de barras (algoritmo 32, receta publicada: 1 / 2 / 3 / 4 / 6 / 8), con un clic de percusión
    voice('ORGAN', 32, [
        op(1, 99, (99, 99, 99, 80), (99, 99, 99, 0), kvs=0, rs=0),
        op(2, 90, (99, 99, 99, 80), (99, 99, 99, 0), kvs=0, rs=0),
        op(3, 84, (99, 99, 99, 80), (99, 99, 99, 0), kvs=0, rs=0),
        op(4, 86, (99, 99, 99, 80), (99, 99, 99, 0), kvs=0, rs=0),
        op(6, 72, (99, 99, 99, 80), (99, 99, 99, 0), kvs=0, rs=0),
        op(8, 74, (99, 70, 99, 80), (99, 0, 0, 0), kvs=2, rs=0)], fb=0,
        lfo=(70, 0, 4, 0, 0, 4, 2)),
    # armónica (diseño propio: no hay receta publicada): un par 1:2 (armónicos impares, de lengüeta),
    # ataque un poco lento y vibrato con retardo
    voice('REED HRP', 5, [
        op(1, 99, (70, 50, 99, 65), (99, 95, 95, 0), kvs=2),
        op(2, 78, (60, 50, 99, 65), (99, 85, 85, 0), kvs=4),
        op(1, 80, (70, 50, 99, 65), (99, 95, 95, 0), kvs=2, dt=8),
        op(1, 60, (60, 50, 99, 65), (99, 80, 80, 0), kvs=3),
        OFF, OFF], fb=0,
        lfo=(30, 40, 8, 0, 0, 4, 3)),
    # clavinet: modulador a 3:1 que se cierra rápido, ataque seco
    voice('CLAVI', 5, [
        op(1, 99, (99, 70, 40, 85), (99, 60, 0, 0), kvs=3, rs=3),
        op(3, 84, (99, 80, 45, 85), (99, 40, 0, 0), kvs=5, rs=3),
        op(1, 86, (99, 72, 40, 85), (99, 55, 0, 0), kvs=3, rs=3, dt=9),
        op(1, 76, (99, 75, 45, 85), (99, 50, 0, 0), kvs=5),
        op(3, 70, (99, 75, 40, 85), (99, 30, 0, 0), kvs=3),
        op(5, 60, (99, 85, 45, 85), (99, 0, 0, 0), kvs=5)], fb=5),
    # pulsado: kalimba o arpa de juguete
    voice('PLUCKED', 5, [
        op(1, 99, (99, 45, 30, 55), (99, 0, 0, 0), kvs=3, rs=4),
        op(6, 70, (99, 78, 40, 55), (99, 0, 0, 0), kvs=6, rs=4),
        op(5.01, 66, (99, 60, 30, 55), (99, 0, 0, 0), kvs=3, rs=4),
        op(1, 58, (99, 82, 40, 55), (99, 0, 0, 0), kvs=5),
        op(2, 60, (99, 55, 30, 55), (99, 0, 0, 0), kvs=3, dt=9),
        op(2, 50, (99, 70, 40, 55), (99, 0, 0, 0), kvs=5)], fb=2),
    # pad de vidrio: moduladores inarmónicos suaves, entra y sale despacio
    voice('GLASS', 5, [
        op(1, 96, (40, 25, 20, 32), (99, 92, 85, 0), kvs=1),
        op(3.5, 62, (35, 25, 20, 32), (99, 80, 70, 0), kvs=2),
        op(1, 92, (42, 25, 20, 32), (99, 92, 85, 0), kvs=1, dt=10),
        op(7.07, 52, (35, 25, 20, 32), (99, 75, 60, 0), kvs=2),
        op(2, 82, (44, 25, 20, 32), (99, 90, 80, 0), kvs=1, dt=4),
        op(1, 60, (35, 25, 20, 32), (99, 80, 70, 0), kvs=2)], fb=3,
        lfo=(25, 30, 5, 0, 0, 4, 2)),
    # solista: una pila de cuatro con realimentación (casi diente de sierra) y vibrato
    voice('LEAD', 1, [
        op(1, 95, (99, 60, 99, 60), (99, 92, 92, 0), kvs=1),
        op(2, 70, (90, 60, 99, 60), (99, 80, 80, 0), kvs=3),
        op(1, 99, (99, 60, 99, 60), (99, 95, 95, 0), kvs=1, dt=8),
        op(1, 78, (99, 60, 99, 60), (99, 88, 88, 0), kvs=3),
        op(1, 74, (99, 60, 99, 60), (99, 85, 85, 0), kvs=2),
        op(1, 80, (99, 60, 99, 60), (99, 90, 90, 0), kvs=2)], fb=7,
        lfo=(38, 45, 8, 0, 0, 4, 4)),
    # super FM: tres portadoras con realimentación, ancho, para acordes
    voice('SUPER FM', 22, [
        op(1, 94, (80, 40, 99, 50), (99, 90, 90, 0), kvs=1),
        op(1, 74, (80, 40, 99, 50), (99, 85, 85, 0), kvs=2),
        op(1, 92, (80, 40, 99, 50), (99, 90, 90, 0), kvs=1, dt=3),
        op(1, 92, (80, 40, 99, 50), (99, 90, 90, 0), kvs=1, dt=11),
        op(2, 84, (80, 40, 99, 50), (99, 90, 90, 0), kvs=1, dt=6),
        op(1, 80, (80, 40, 99, 50), (99, 88, 88, 0), kvs=2)], fb=7,
        lfo=(28, 20, 4, 0, 0, 4, 2)),
    # zumbido: bajo agresivo para acid y techno (realimentación al máximo)
    voice('BUZZ', 2, [
        op(1, 99, (99, 50, 70, 70), (99, 85, 70, 0), kvs=2),
        op(1, 84, (99, 60, 70, 70), (99, 70, 50, 0), kvs=5),
        op(1, 90, (99, 55, 70, 70), (99, 80, 60, 0), kvs=2, dt=9),
        op(2, 70, (99, 65, 70, 70), (99, 60, 40, 0), kvs=4),
        op(1, 74, (99, 65, 70, 70), (99, 60, 40, 0), kvs=4),
        op(1, 74, (99, 65, 70, 70), (99, 60, 40, 0), kvs=4)], fb=7),
]


def main():
    assert len(V) == 20, len(V)
    rows, names = [], []
    for name, d in V:
        rows.append('    {%s},  /* %s */' % (','.join(str(x) for x in d), name))
        names.append('"%s"' % name[:8])
    out = os.path.join(ROOT, 'core', 'gen', 'bm_fm_rom.c')
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('/* SPDX-License-Identifier: GPL-3.0-or-later\n * Copyright (C) 2026 Erbubar23 */\n')
        f.write('/* Generado por tools/gen_fm.py: no editar a mano. */\n#include "../bm_rom.h"\n\n')
        f.write('const uint8_t bm_fm_rom[BM_FM_PATCHES][156] = {\n%s\n};\n\n' % '\n'.join(rows))
        f.write('const char bm_fm_names[BM_FM_PATCHES][9] = {%s};\n' % ', '.join(names))
    print('%s: %d patches, %d bytes' % (out, len(V), len(V) * 156))


if __name__ == '__main__':
    main()
