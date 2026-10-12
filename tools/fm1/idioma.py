#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Carga un idioma de pantalla en el FM-1 (o lo quita), por la consola MIDI.

  py -3 tools/fm1/idioma.py lang/es.bmlang     carga el español
  py -3 tools/fm1/idioma.py --quitar           vuelve al inglés (borra el paquete)

El firmware viene en inglés. Un paquete (.bmlang, lo arma tools/gen_lang.py) queda en la flash del
FM-1, en su propia zona de 16 KiB: no toca patrones ni tomas, y una actualización del firmware no
lo borra. Después se cambia entre inglés y el paquete en SISTEMA (HOME 2 s) > LANGUAGE.
Con el transporte parado (escribir la flash corta el audio). Cierra antes el instalador web u
otro programa que use el MIDI del FM-1.
"""
import argparse, os, struct, sys, zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import consola  # noqa: E402
from fm1_midi import Puerto  # noqa: E402

CAP = 16 * 1024
TROZO = 256


def revisar(d):
    """el nombre del paquete, o sale con el motivo si no sirve"""
    if len(d) < 28 or d[:4] != b'BMLG' or d[4] != 1:
        sys.exit('no es un paquete de idioma de Bubba Machine (.bmlang)')
    n, size, crc = struct.unpack('<H', d[6:8])[0], struct.unpack('<I', d[20:24])[0], struct.unpack('<I', d[24:28])[0]
    if size != len(d) or size > CAP:
        sys.exit('el paquete dice %d bytes y tiene %d (caben %d)' % (size, len(d), CAP))
    if zlib.crc32(d[28:]) & 0xFFFFFFFF != crc:
        sys.exit('el paquete está dañado (CRC)')
    return d[8:20].split(b'\0')[0].decode('ascii'), n


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('paquete', nargs='?', help='archivo .bmlang (lang/es.bmlang)')
    ap.add_argument('--quitar', action='store_true', help='borra el paquete: la pantalla vuelve al inglés')
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    if not a.quitar and not a.paquete:
        ap.error('falta el paquete (o --quitar)')
    datos = b''
    if not a.quitar:
        datos = open(a.paquete, 'rb').read()
        nombre, n = revisar(datos)
        print('%s: %d frases, %d bytes' % (nombre, n, len(datos)))
    with Puerto('Bubba') as p:
        def pedido(sub, off=0, cuerpo=b''):
            r = consola.pedir(p, 0x21, bytes([sub]) + off.to_bytes(4, 'big') + bytes(2) + cuerpo)
            if len(r) < 2 or r[1] != 1:
                sys.exit('el FM-1 rechazó el paso %d: ¿está sonando? Para el transporte (STOP) y prueba otra vez' % sub)
        if a.quitar:
            pedido(3)
            print('listo: la pantalla vuelve al inglés')
            return
        pedido(0)
        for off in range(0, len(datos), TROZO):
            pedido(1, off, datos[off:off + TROZO])
        pedido(2)
    print('listo: %s cargado y activo (SISTEMA > LANGUAGE cambia entre él y el inglés)' % nombre)


if __name__ == '__main__':
    main()
