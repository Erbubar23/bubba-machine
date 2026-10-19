#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Sube un sonido al FM-1 (o lo borra), por la consola MIDI.

  py -3 tools/fm1/sonido.py mi-lead.bsnap        un sonido de BITTAL, B-MINI o BUNO-6
  py -3 tools/fm1/sonido.py banco.syx            un banco de 32 voces de la DX7 para BX-7
  py -3 tools/fm1/sonido.py --borrar BITTAL 19   borra el sonido 20 de BITTAL en PRESETS (del usuario)
  py -3 tools/fm1/sonido.py --borrar BX-7 0      borra el primer banco .syx

Un .bsnap se arma desde un preset de Vital con el editor web (web/control.html#sounds) o con
`node tools/vital2bsnap.mjs preset.vital --machine BITTAL`. Un .syx se convierte aquí mismo.
Queda en la zona que comparten tomas y sonidos (100 KiB): las tomas desde abajo, los sonidos desde
arriba. En el FM-1, PRESETS en el canal de la máquina lo elige (después de los de fábrica).
Con el transporte parado (escribir la flash corta el audio). Cierra antes el instalador web u
otro programa que use el MIDI del FM-1.
"""
import argparse, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola  # noqa: E402
from fm1_midi import Puerto  # noqa: E402

TROZO = 256
HDR = 88
MAQUINAS = {3: 'B-MINI', 4: 'BX-7', 5: 'BITTAL', 6: 'BUNO-6'}


def fnv16(d):
    h = 2166136261
    for i, b in enumerate(d):
        h = ((h ^ (0 if i in (8, 9) else b)) * 16777619) & 0xFFFFFFFF
    return (h ^ (h >> 16)) & 0xFFFF


def syx_a_bsnap(raw, nombre=None):
    """un volcado de 32 voces de la DX7 a un banco de BX-7 (lo mismo que web/bsnap.js, syxToBsnap)"""
    at = -1
    for i in range(len(raw) - 6):
        if raw[i] == 0xF0 and raw[i + 1] == 0x43 and raw[i + 3] == 0x09 and raw[i + 4] == 0x20 and raw[i + 5] == 0:
            at = i + 6
            break
    if at < 0 or at + 4096 > len(raw):
        sys.exit('no es un banco de 32 voces de la DX7 (.syx)')
    d = raw[at:at + 4096]
    out = bytearray(HDR + 32 * 156)
    out[0:4] = b'BNS1'
    out[4], out[5], out[6], out[7] = 4, 0, 0, 1
    out[10:12] = struct.pack('<H', len(out))
    nombres = []
    for v in range(32):
        p, o = d[v * 128:v * 128 + 128], HDR + v * 156
        for op in range(6):
            s, q = p[op * 17:op * 17 + 17], o + op * 21
            out[q:q + 11] = s[0:11]
            out[q + 11], out[q + 12] = s[11] & 3, (s[11] >> 2) & 3
            out[q + 13], out[q + 20] = s[12] & 7, (s[12] >> 3) & 15
            out[q + 14], out[q + 15] = s[13] & 3, (s[13] >> 2) & 7
            out[q + 16] = s[14]
            out[q + 17], out[q + 18] = s[15] & 1, (s[15] >> 1) & 31
            out[q + 19] = s[16]
        out[o + 126:o + 134] = p[102:110]
        out[o + 134] = p[110] & 31
        out[o + 135], out[o + 136] = p[111] & 7, (p[111] >> 3) & 1
        out[o + 137:o + 141] = p[112:116]
        out[o + 141], out[o + 142], out[o + 143] = p[116] & 1, (p[116] >> 1) & 7, (p[116] >> 4) & 7
        out[o + 144] = p[117]
        out[o + 145:o + 155] = bytes(b & 0x7F for b in p[118:128])
        out[o + 155] = 0x3F
        nombres.append(bytes(p[118:128]).decode('ascii', 'replace').strip())
    nm = (nombre or nombres[0] or 'DX7 BANK').upper().encode('ascii', 'replace')[:12]
    out[12:12 + len(nm)] = nm
    out[8:10] = struct.pack('<H', fnv16(out))
    return bytes(out), nombres


def revisar(d):
    if len(d) < HDR or d[:4] != b'BNS1':
        sys.exit('no es un sonido de Bubba Machine (.bsnap)')
    size = struct.unpack('<H', d[10:12])[0]
    if size != len(d) or d[4] not in MAQUINAS:
        sys.exit('el sonido está dañado (dice %d bytes y tiene %d)' % (size, len(d)))
    return d[12:24].split(b'\0')[0].decode('ascii', 'replace'), MAQUINAS[d[4]], struct.unpack('<H', d[8:10])[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('archivo', nargs='?', help='un .bsnap o un .syx')
    ap.add_argument('--borrar', nargs=2, metavar=('MAQUINA', 'N'), help='borra un sonido del usuario: la máquina y su número en PRESETS (desde 0)')
    ap.add_argument('--nombre', help='nombre para un banco .syx (12 letras)')
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    if a.borrar is None and not a.archivo:
        ap.error('falta el archivo (o --borrar N)')
    datos = b''
    if a.archivo:
        raw = open(a.archivo, 'rb').read()
        if a.archivo.lower().endswith('.syx'):
            datos, nombres = syx_a_bsnap(raw, a.nombre)
            print('banco de la DX7: ' + ', '.join(nombres))
        else:
            datos = raw
        nombre, maq, ident = revisar(datos)
        print('%s para %s, #%04X, %d bytes' % (nombre, maq, ident, len(datos)))
    with Puerto('Bubba') as p:
        def pedido(sub, off=0, cuerpo=b''):
            r = consola.pedir(p, 0x22, bytes([sub]) + off.to_bytes(4, 'big') + bytes(2) + cuerpo)
            if len(r) < 2 or r[1] != 1:
                sys.exit('el FM-1 rechazó el paso %d: ¿está sonando, o no queda lugar? Para el transporte '
                         '(STOP); si sigue, borra una toma o un sonido' % sub)
        if a.borrar is not None:
            eng = {v: k for k, v in MAQUINAS.items()}.get(a.borrar[0].upper())
            if eng is None:
                sys.exit('máquina: ' + ', '.join(MAQUINAS.values()))
            pedido(3, eng << 8 | int(a.borrar[1]))
            print('listo: borrado')
            return
        pedido(0)
        for off in range(0, len(datos), TROZO):
            pedido(1, off, datos[off:off + TROZO])
        pedido(2)
    print('listo: en el FM-1, PRESETS en el canal de %s lo elige' % maq)


if __name__ == '__main__':
    main()
