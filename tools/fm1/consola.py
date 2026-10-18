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
          'past1', 'past2', 'past3', 'past4', 'audio_cpu', 'midi_in', 'midi_out', 'loop_max', 'heap_free', 'heap_min',
          'par_cpu', 'freno', 'par_modo', 'par_espera_max_us', 'par_mask', 'carga0_us', 'carga1_us', 'par_mismo',
          'voces',
          # 0.8.1
          'rst_raw', 'rst_pmu', 'rail_min', 'rail_now', 'vbat', 'batt_adc', 'pwr0', 'pwr1', 'rastro0', 'rastro1',
          'loop_us', 'loop_max_us', 'c1', 'freno_ui', 'nan_fix']
NUCLEO1 = ['sin probar', 'probando', 'listo', 'no contestó al arrancar', 'retired: did not answer in time',
           'retired: misread a job', 'retired by an earlier boot (until power-off)']
ETAPA = ['antes de la pantalla', 'pantalla y USB', 'núcleo listo', 'teclas', 'audio iniciado', 'audio sonando',
         '1 s andando', '5 s andando', '5 s andando']
FIN = {1: 'POWER ON', 2: 'WDT', 3: 'soft reset (fault or watchdog of the loop)', 4: 'other',
       5: 'VDDIO LOW (brownout)', 6: 'LONG PRESS', 0xFF: '-'}
# P3_RST_SRC (apps/common/system/system_reset_reason.c of the SDK)
RST_BITS = [(0, 'POWER ON'), (1, 'VDDIO LOW (brownout)'), (2, 'WDT'), (3, 'VCM'), (4, 'LONG PRESS'), (5, '12V'),
            (6, 'soft')]
BF = [(1, 'TWO CORES on'), (2, 'core 1 retired'), (4, 'stock power rails set'), (8, 'breadcrumb found')]


def bits(v, tabla):
    return ', '.join(n for b, n in tabla if v & (b if tabla is BF else 1 << b)) or 'none'


def rastro(lo, hi):
    b = lo.to_bytes(4, 'little') + hi.to_bytes(4, 'little')
    paso = b[2:4].decode('ascii', 'replace')
    mv = lambda x, k, base: '?' if x == 0xFF else '%d mV' % (base + x * k)
    t = b[7] if b[7] < 128 else (b[7] - 128) * 30
    return 'step %s, lowest SYSVDD %s, VDC14 %s, VBAT %s, up %s s' % (
        paso, mv(b[4], 4, 1000), mv(b[5], 4, 1000), mv(b[6], 16, 2000), ('~%d' % t) if b[7] >= 128 else t)


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
    if 'heap_free' in v:
        print('memoria libre del montón: %d KiB (get_malloc_remain_heap_size), %d KiB (get_ram_free_heap_size)' % (v['heap_free'] // 1024, v['heap_min'] // 1024))
    if 'freno' in v:
        print('freno de voces: %d apagadas | núcleo 1: %s (espera máx %d us, último trabajo en el núcleo %d)' % (
            v['freno'], NUCLEO1[v['par_modo']] if v['par_modo'] < len(NUCLEO1) else v['par_modo'],
            v['par_espera_max_us'], v['par_cpu']))
    if v.get('voces') is not None:
        x = v['voces']
        print('voces sonando: %s | tope del freno: %d de 15 voces del pool' % (
            ' '.join('T%d %d' % (t + 1, (x >> (4 * t)) & 15) for t in range(6)), x >> 24))
    if v.get('par_mask') is not None and v['par_modo'] == 2:
        m = v['par_mask']
        print('reparto: núcleo 1 calcula %s | estimado: núcleo 0 %d us, núcleo 1 %d us (de %d)' % (
            ' '.join('T%d' % (t + 1) for t in range(6) if (m >> t) & 1) or 'nada', v['carga0_us'], v['carga1_us'], v['budget_us']))
    if v['loop_max']:
        c = v['loop_max'].to_bytes(4, 'little').decode('ascii', 'replace')
        print('cuelgue antes de este arranque (perro guardián): paso %s, sub-paso %s' % (c[:2], c[2:]))
    if 'rst_raw' in v:
        r = v['rst_raw']
        p3, sysr, wk, lvd = r & 0xFF, (r >> 8) & 0xFF, (r >> 16) & 0xFF, r >> 24
        print('reset before this boot: P3_RST_SRC %02X = %s | RST_SRC %02X | R3_WKUP_SRC %02X | P3_PMU_CON1 %02X | '
              'P3_VLVD_CON %02X%s' % (p3, bits(p3, RST_BITS), sysr, wk, v['rst_pmu'] & 0xFF, lvd,
                                      ' (LVD pending)' if lvd & 0x80 else ''))
        print('this boot: %s | previous boot: %s' % (bits((v['rst_pmu'] >> 16) & 0xFF, BF), bits((v['rst_pmu'] >> 8) & 0xFF, BF)))
        mn, nw = v['rail_min'], v['rail_now']
        print('rails: SYSVDD %d mV (lowest %d) | VDC14 %d mV (lowest %d) | VBAT %d mV (lowest %d) | battery ADC PB1 %d (lowest %d)' % (
            nw & 0xFFFF, mn & 0xFFFF, nw >> 16, mn >> 16, v['vbat'] >> 16, v['vbat'] & 0xFFFF, v['batt_adc'] >> 16, v['batt_adc'] & 0xFFFF))
        f = lambda w: 'ANA_CON0 %02X CON5 %02X CON6 %02X CON9 %02X' % (w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF, w >> 24)
        print('power setup: before %s | after %s' % (f(v['pwr0']), f(v['pwr1'])) if v['pwr1'] else 'power setup: not done')
        if v['rastro0']:
            print('breadcrumb of the last run: ' + rastro(v['rastro0'], v['rastro1']))
        else:
            print('breadcrumb of the last run: none (RAM lost, or a watchdog/fault record took its place)')
        c1 = v['c1']
        print('UI loop: %d us (longest %d) | brake from a slow UI: %d | core 1: %s%s%s%s, %d wait turns/us | nan reset %d' % (
            v['loop_us'], v['loop_max_us'], v['freno_ui'], 'retired now' if c1 & 1 else 'not retired now',
            ', retired by an earlier boot' if c1 & 2 else '', ', TWO CORES turned off (last boot with it < 5 s)' if c1 & 4 else '',
            ', its tracks waiting (silent)' if c1 & 8 else '', c1 >> 16, v['nan_fix']))
    for k in range(1, 5):
        x = v['past%d' % k]
        if x & 0x80:
            e, ok, fin, p3, fl = x & 15, (x >> 4) & 1, (x >> 8) & 0xFF, (x >> 16) & 0xFF, x >> 24
            print('  arranque -%d: llegó a %s, %s | P3_RST_SRC %02X = %s | %s' % (
                k, ETAPA[min(e, 8)], 'bien' if ok else 'terminó: ' + FIN.get(fin, '?'), p3, bits(p3, RST_BITS), bits(fl, BF)))


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
