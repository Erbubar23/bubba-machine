#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Instala un paquete .fwsc en el FM-1 desde la consola, anotando cada pedido.

  py -3 tools/fm1/instalar.py PAQUETE.fwsc              (Bubba Machine -> cargador -> escritura)
  py -3 tools/fm1/instalar.py PAQUETE.fwsc --continuar  (el FM-1 ya está en modo actualización)
  py -3 tools/fm1/instalar.py PAQUETE.fwsc --mirar      (cargador: da la orden y no contesta)

Hace lo mismo que web/fm1_update.js. Paso 1: el firmware que corre (hal/fm1/fm1_update.c)
revisa el paquete y copia el cargador oficial; paso 2: el cargador de M-VAVE pide el paquete
entero y lo escribe. Solo se contestan pedidos dentro del paquete. El registro completo queda
en build/fm1/instalar.log.
"""
import argparse, os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fm1_midi import Puerto, puertos  # noqa: E402
import fwsc  # noqa: E402

HANDSHAKE = bytes([0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7])
UPGRADE = bytes([0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7])
ADDR_VERIFIED, ADDR_WRITTEN = 0xE0000000, 0xF0000000
LOG = os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'fm1', 'instalar.log')


def pack7(data):
    out, acc, nb = [], 0, 0
    for b in data:
        acc |= b << nb; nb += 8
        while nb >= 7:
            out.append(acc & 0x7F); acc >>= 7; nb -= 7
    if nb:
        out.append(acc & 0x7F)
    return out


def unpack7(s):
    out, acc, nb = [], 0, 0
    for b in s:
        acc |= (b & 0x7F) << nb; nb += 7
        while nb >= 8:
            out.append(acc & 0xFF); acc >>= 8; nb -= 8
    return out


def pedido(m):
    """(flags, dirección, largo) de un pedido del FM-1, o None"""
    if len(m) < 4 or m[0] != 0xF0 or m[-1] != 0xF7:
        return None
    u = unpack7(m[1:-1])
    if len(u) != 15 or u[0] != 0 or u[1] != 0x59 or u[2] != 0x30:
        return None
    if (u[3] | u[4] << 8 | u[5] << 16) != 8 or (~sum(u[6:14]) & 0xFF) != u[14]:
        return None
    return u[6], struct.unpack_from('<I', bytes(u), 7)[0], u[11] | u[12] << 8 | u[13] << 16


def identidad(m):
    u = unpack7(m[1:-1]) if len(m) > 3 and m[0] == 0xF0 else []
    if len(u) != 34 or u[1] != 0x59 or u[2] != 0x11:
        return None
    return bytes(u[6:33]).rstrip(b'\0').decode('latin-1')


def respuesta(addr, data, fl=0):
    n = len(data)
    body = [0x00, 0x59, 0x30, (n + 8) & 0xFF, ((n + 8) >> 8) & 0xFF, ((n + 8) >> 16) & 0xFF, fl,
            addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, (addr >> 24) & 0xFF,
            n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF] + list(data)
    body.append(~sum(body[6:]) & 0xFF)
    return bytes([0xF0] + pack7(body) + [0xF7])


class Registro:
    def __init__(self):
        os.makedirs(os.path.dirname(LOG), exist_ok=True)
        self.f = open(LOG, 'w', encoding='utf-8')
        self.t0 = time.time()

    def __call__(self, txt, pantalla=True):
        self.f.write('%7.2f %s\n' % (time.time() - self.t0, txt)); self.f.flush()
        if pantalla:
            print(txt, flush=True)


def saludar(p, veces=1):
    for _ in range(veces):               # el cargador recién aparecido tarda en contestar
        p.enviar(HANDSHAKE)
        ids = [i for i in (identidad(m) for m in p.recibir(1.5)) if i]
        if ids:
            return ids
    return []


def esperar_puerto(nombre, seg):
    t = time.time() + seg
    while time.time() < t:
        if any(nombre.lower() in n.lower() for n in puertos()[0]):
            return True
        time.sleep(0.5)
    return False


def servir(p, img, fin, log, quieto=60, cargador=True):
    """contesta pedidos hasta la confirmación `fin`; True si llegó"""
    n, last, prev, reinicios = 0, time.time(), -1, 0
    while True:
        ms = p.recibir(0.05)
        if not ms and time.time() - last > quieto:
            log('el FM-1 dejó de pedir hace %d s (%d pedidos, último 0x%X)' % (quieto, n, prev))
            return False
        for m in ms:
            if identidad(m) is not None:
                continue
            r = pedido(m)
            if not r:
                continue
            fl, addr, ln = r
            last = time.time()
            if addr in (ADDR_VERIFIED, ADDR_WRITTEN):
                p.enviar(respuesta(addr, b'success\0'))
                log('confirmación 0x%08X (%d pedidos)' % (addr, n))
                if addr == fin:
                    return True
                continue
            if ln > 512 or addr + ln > len(img):
                log('pidió fuera del paquete: 0x%X + %d. Me detengo sin contestar.' % (addr, ln))
                return False
            if cargador and prev > 0x8000 and addr < 0x1000:
                reinicios += 1
                log('!! el cargador volvió a empezar (después de 0x%X). Vez %d' % (prev, reinicios))
                if reinicios >= 3:
                    log('se reinicia una y otra vez: el cargador está retomando una escritura cortada que '
                        'empezó con OTRO paquete. Termínala con ese paquete (si no sabes cuál, la versión '
                        'anterior que tenías) usando --continuar, y después instala este de nuevo.')
                    return False
            p.enviar(respuesta(addr, img[addr:addr + ln], fl))
            n += 1
            log('%5d  0x%06X +%d fl %d' % (n, addr, ln, fl), n <= 12 or n % 200 == 0 or addr < 0x1000)
            prev = addr


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('paquete')
    ap.add_argument('--continuar', action='store_true', help='el FM-1 ya está en modo actualización')
    ap.add_argument('--mirar', action='store_true', help='con el cargador: da la orden y no contesta')
    ap.add_argument('--puerto', default='Bubba')
    ap.add_argument('--cargador', default='USB-Midi')
    a = ap.parse_args()
    raw = open(a.paquete, 'rb').read()
    img = bytes(fwsc.logica(raw))
    fwsc.Flash(fwsc.Paquete(raw).flash())          # que se entienda antes de mandarlo
    log = Registro()
    log('paquete %s: %s, imagen %d bytes' % (os.path.basename(a.paquete), fwsc.identidad(raw), len(img)))

    if not (a.continuar or a.mirar):
        with Puerto(a.puerto) as p:
            log('paso 1 con %s: %s' % (p.nombre, saludar(p) or 'no contesta'))
            p.enviar(UPGRADE)
            time.sleep(2)
            if not servir(p, img, ADDR_VERIFIED, log, quieto=15, cargador=False):
                sys.exit('paso 1 sin terminar: no se escribió nada (mira la pantalla del FM-1)')
        log('esperando al cargador...')
        time.sleep(3)
        if not esperar_puerto(a.cargador, 30):
            sys.exit('el cargador no apareció: desconecta y conecta el USB y usa --continuar')

    with Puerto(a.cargador) as p:
        ids = saludar(p, 5)
        log('paso 2 con %s: %s' % (p.nombre, ids or 'no contesta'))
        if not any(i.startswith('ota-') for i in ids):
            sys.exit('el FM-1 no está en modo actualización')
        p.enviar(UPGRADE)
        if a.mirar:
            t0 = time.time()
            while time.time() - t0 < 10:
                for m in p.recibir(0.2):
                    r = pedido(m)
                    if r:
                        log('%6.2f pide 0x%06X +%d' % (time.time() - t0, r[1], r[2]))
            return
        time.sleep(2)
        if not servir(p, img, ADDR_WRITTEN, log):
            sys.exit('paso 2 sin terminar (registro en build/fm1/instalar.log)')
    log('LISTO: el FM-1 se reinicia con el firmware nuevo.')
    if esperar_puerto(a.puerto, 40):
        with Puerto(a.puerto) as p:
            log('volvió: %s' % (saludar(p) or 'no contesta'))
    else:
        log('no volvió a aparecer "%s" en 40 s' % a.puerto)


if __name__ == '__main__':
    main()
