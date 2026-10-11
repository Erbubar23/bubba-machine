#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Paquetes .fwsc del FM-1: leer, comprobar y armar.

  py -3 tools/fm1/fwsc.py info     PAQUETE.fwsc
  py -3 tools/fm1/fwsc.py idavuelta OFICIAL.fwsc
  py -3 tools/fm1/fwsc.py armar    OFICIAL.fwsc APP.bin SALIDA.fwsc [--identidad FM-1_003]
  py -3 tools/fm1/fwsc.py sacar    PAQUETE.fwsc DIR

El paquete nuestro se arma sobre el FM-1 V15 oficial de M-VAVE (no se incluye: lo
descarga cada quien). Solo cambia app.bin, dentro de la zona de la app de flash.bin;
el resto (cabecera del flash, SPL, isd_config, cargador de actualización ota.bin, USR,
script.ver, blimit.bin, tail.bin) queda idéntico al oficial. Se recalculan los CRC que
cubren lo que cambió y la zona de la app se vuelve a cifrar con la llave del chip.

Antes de armar nada se comprueba que el formato se entiende: desarmar el V15 y volver a
armarlo con su propia app tiene que dar el archivo oficial byte por byte (idavuelta).

Datos del formato (no código): FM-1-RE (WTFPL), docs/io/11-ota-protocol.md y
docs/io/12-v15-reflash-proof.md; jl-misctools de kagaimiq (MIT), firmware/.
  .fwsc   20 bloques de 0x30 bytes: 0x2F de datos + 1 marcador (la identidad del
          paquete: marcador = carácter + índice + 1; 0x7D = vacío). Después, todo tal cual.
          Lo que el FM-1 pide por USB es la imagen "lógica", sin los marcadores.
  UFW     cabecera de 0x40 + una entrada de 0x50 por archivo, cifradas con ENC 0xFFFF.
  flash   cabecera y entradas JLFS de 32 bytes cifradas con ENC 0xFFFF; desde app_dir_head
          (0x4000) todo va con SFC (ENC por bloques de 32 con la llave del chip mezclada
          con la dirección).
  CRC     CRC-16 polinomio 0x1021, inicio 0, sin reflejar.
"""
import argparse, hashlib, os, struct, sys

V15_SHA256 = 'db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a'
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
MARK_EMPTY = 0x7D
ENTRY_POINT = 0x02000120
IDENTIDAD = 'FM-1_003'


class Error(Exception):
    pass


# ---------------------------------------------------------------- algoritmos ---
def _tabla():
    t = []
    for i in range(256):
        c = i << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
        t.append(c & 0xFFFF)
    return t


_T = _tabla()


def crc16(data, crc=0):
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ _T[((crc >> 8) ^ b) & 0xFF]
    return crc


def enc(buf, off, n, key=0xFFFF):
    """cifrado ENC de JieLi (XOR con un registro de 16 bits); es su propio inverso"""
    for i in range(n):
        buf[off + i] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF


def sfc(buf, off, n, base, key):
    """SFC: ENC por bloques de 32 bytes, con la llave mezclada con la dirección"""
    for i in range(0, n, 32):
        enc(buf, off + i, min(n - i, 32), key ^ ((off + i - base) >> 2))


def llave_isd(blob):
    """llave del chip guardada en los 32 bytes del principio de isd_config (+ su CRC)"""
    d, c = blob[:32], struct.unpack_from('<H', blob, 32)[0]
    if crc16(d) != c:
        raise Error('isd_config: el bloque de la llave no pasa su CRC')
    s = sum(d[:16]) & 0xFF
    s = 0xAA if s >= 0xE0 else 0x55 if s <= 0x10 else s
    return sum(1 << i for i in range(16) if (d[16 + i] ^ d[15 - i]) < s)


# ------------------------------------------------------------------ JLFS ---
JLFS = struct.Struct('<HHIIBBH16s')   # hcrc dcrc offset size flags resvd index name


def jlfs_leer(buf, off):
    hcrc, dcrc, eoff, size, flags, resvd, idx, name = JLFS.unpack_from(buf, off)
    if crc16(buf[off + 2:off + 32]) != hcrc:
        raise Error('entrada JLFS en 0x%X: CRC de cabecera incorrecto' % off)
    return dict(dcrc=dcrc, off=eoff, size=size, flags=flags, resvd=resvd, idx=idx, raw_name=name,
                name=name.split(b'\0')[0].decode('latin-1'))


def jlfs_armar(e):
    body = struct.pack('<HIIBBH16s', e['dcrc'], e['off'] & 0xFFFFFFFF, e['size'] & 0xFFFFFFFF,
                       e['flags'], e['resvd'], e['idx'], e['raw_name'])
    return struct.pack('<H', crc16(body)) + body


# ------------------------------------------------------------------ .fwsc ---
def logica(raw):
    if len(raw) < BLOCKS * BLK:
        raise Error('no es un paquete del FM-1 (muy corto)')
    out = bytearray()
    for i in range(BLOCKS):
        out += raw[i * BLK:i * BLK + KEEP]
    return out + raw[BLOCKS * BLK:]


def identidad(raw):
    s = ''
    for i in range(BLOCKS):
        m = raw[i * BLK + KEEP]
        if m != MARK_EMPTY:
            s += chr((m - i - 1) & 0xFF)
    return s


def marcar(log, ident):
    if len(ident) > BLOCKS or any((ord(c) + i + 1) & 0xFF == MARK_EMPTY for i, c in enumerate(ident)):
        raise Error('identidad %r: hasta 20 caracteres, ninguno que caiga en el marcador vacío' % ident)
    raw = bytearray()
    for i in range(BLOCKS):
        m = (ord(ident[i]) + i + 1) & 0xFF if i < len(ident) else MARK_EMPTY
        raw += log[i * KEEP:(i + 1) * KEEP] + bytes([m])
    return raw + log[BLOCKS * KEEP:]


UFW_HDR = struct.Struct('<HHIHHI48s')          # hdrcrc listcrc imgsize numents wa3 wa4 chip
UFW_ENT = struct.Struct('<HHHHIII44s16s')      # type index dcrc wa1 offset size size2 wa2 name


class Paquete:
    def __init__(self, raw):
        self.raw = bytes(raw)
        self.ident = identidad(self.raw)
        self.log = logica(self.raw)
        h = bytearray(self.log[:0x40])
        enc(h, 0, 0x40)
        if crc16(h[2:0x40]) != struct.unpack_from('<H', h)[0]:
            raise Error('cabecera UFW: CRC incorrecto')
        (self.hdrcrc, self.listcrc, self.imgsize, n, self.wa3, self.wa4, chip) = UFW_HDR.unpack_from(h)
        self.chip = chip.split(b'\0')[0].decode('latin-1')
        self.hdr = h
        lst = self.log[0x40:0x40 + n * 0x50]
        if crc16(lst) != self.listcrc:
            raise Error('lista de entradas UFW: CRC incorrecto')
        if self.imgsize != len(self.log):
            raise Error('el tamaño declarado (%d) no es el del archivo (%d)' % (self.imgsize, len(self.log)))
        self.ents = []
        for k in range(n):
            e = bytearray(lst[k * 0x50:(k + 1) * 0x50])
            enc(e, 0, 0x50)
            t, idx, dcrc, wa1, off, size, size2, wa2, name = UFW_ENT.unpack_from(e)
            self.ents.append(dict(type=t, index=idx, dcrc=dcrc, wa1=wa1, off=off, size=size, size2=size2,
                                  wa2=wa2, name=name.split(b'\0')[0].decode('latin-1'), plain=bytes(e)))

    def entrada(self, tipo):
        for e in self.ents:
            if e['type'] == tipo:
                return e
        raise Error('el paquete no trae la entrada de tipo %d' % tipo)

    def datos(self, e):
        return bytes(self.log[e['off']:e['off'] + e['size']])

    def flash(self):
        e = self.entrada(0)
        d = self.datos(e)
        if crc16(d) != e['dcrc']:
            raise Error('flash.bin: CRC de datos incorrecto')
        return d


def armar_paquete(base, flash, ident):
    """el paquete `base` con otro flash.bin del mismo tamaño y otra identidad"""
    e0 = base.entrada(0)
    if len(flash) != e0['size']:
        raise Error('flash.bin tiene que medir %d bytes' % e0['size'])
    log = bytearray(base.log)
    log[e0['off']:e0['off'] + len(flash)] = flash
    lst = bytearray()
    for e in base.ents:
        p = bytearray(e['plain'])
        if e['type'] == 0:
            struct.pack_into('<H', p, 4, crc16(flash))
        enc(p, 0, 0x50)
        lst += p
    h = bytearray(base.hdr)
    struct.pack_into('<H', h, 2, crc16(lst))
    struct.pack_into('<H', h, 0, crc16(h[2:0x40]))
    enc(h, 0, 0x40)
    log[0:0x40] = h
    log[0x40:0x40 + len(lst)] = lst
    return bytes(marcar(log, ident))


# ------------------------------------------------------------- flash.bin ---
class Flash:
    """flash.bin desarmado: cabecera (intacta), zona de la app descifrada y sus partes"""

    def __init__(self, img):
        self.img = bytes(img)
        h = bytearray(img[:32])
        enc(h, 0, 32)
        if crc16(h[2:32]) != struct.unpack_from('<H', h)[0]:
            raise Error('flash.bin: cabecera del flash con CRC incorrecto')
        self.top = []
        o = 32
        while True:
            e = bytearray(img[o:o + 32])
            enc(e, 0, 32)
            ent = jlfs_leer(e, 0)
            self.top.append(ent)
            o += 32
            if ent['idx'] or len(self.top) > 16:
                break
        app = [e for e in self.top if e['flags'] == 0x81 and e['name'] == 'app_dir_head']
        isd = [e for e in self.top if e['name'] == 'isd_config.ini']
        if not app or not isd:
            raise Error('flash.bin: falta app_dir_head o isd_config.ini')
        self.base = app[0]['off']
        self.key = llave_isd(img[isd[0]['off']:isd[0]['off'] + 34])
        z = bytearray(img)
        sfc(z, self.base, len(z) - self.base, self.base, self.key)
        self.head = jlfs_leer(z, self.base)
        if self.head['flags'] != 0x83:
            raise Error('app_area_head inesperado (flags %02X)' % self.head['flags'])
        blk = self.base + self.head['size']
        if crc16(z[self.base + 32:blk]) != self.head['dcrc']:
            raise Error('app_area_head: CRC de datos incorrecto')
        self.subs, o = [], self.base + 32
        while True:
            e = jlfs_leer(z, o)
            self.subs.append(e)
            o += 32
            if e['idx']:
                break
        self.files = {}
        for e in self.subs:
            if e['flags'] == 0x82:
                d = bytes(z[self.base + e['off']:self.base + e['off'] + e['size']])
                if crc16(d) != e['dcrc']:
                    raise Error('%s: CRC de datos incorrecto' % e['name'])
                self.files[e['name']] = d
        if self.subs[0]['name'] != 'app.bin':
            raise Error('la primera entrada de la app no es app.bin')
        # recursos después del bloque: una cadena de entradas (la última con idx 1)
        o, self.res = blk, []
        last = self.head['idx']
        while not last:
            e = jlfs_leer(z, o)
            if o + e['size'] > len(z):
                raise Error('recurso %s fuera de flash.bin' % e['name'])
            if crc16(z[o + 32:o + e['size']]) != e['dcrc']:
                raise Error('recurso %s: CRC de datos incorrecto' % e['name'])
            self.res.append(bytes(z[o:o + e['size']]))
            o += e['size']
            last = e['idx']
        self.end = o
        self.dec = bytes(z)

    def app(self):
        return self.files['app.bin']

    def espacio(self):
        """bytes que puede medir app.bin sin pasarse del final de flash.bin"""
        fijo = 0x20 + 32 * len(self.subs)
        resto = sum(len(d) for n, d in self.files.items() if n != 'app.bin') + sum(len(r) for r in self.res)
        hueco = self.subs[0]['off'] - fijo          # el relleno entre la lista y app.bin
        return len(self.img) - self.base - fijo - hueco - resto

    def armar(self, app):
        """flash.bin con otra app.bin; todo lo demás igual"""
        if len(app) > self.espacio():
            raise Error('app.bin mide %d bytes y caben %d' % (len(app), self.espacio()))
        files = dict(self.files, **{'app.bin': bytes(app)})
        subs, o = [], self.subs[0]['off']
        datos = bytearray()
        for e in self.subs:
            e = dict(e)
            if e['flags'] == 0x82:
                d = files[e['name']]
                e.update(off=o, size=len(d), dcrc=crc16(d))
                datos += d
                o += len(d)
            subs.append(e)
        blk = o
        area = bytearray(b'\xFF' * blk)
        lista = b''.join(jlfs_armar(e) for e in subs)
        area[32:32 + len(lista)] = lista
        area[self.subs[0]['off']:blk] = datos
        head = dict(self.head, size=blk, dcrc=crc16(area[32:blk]))
        area[0:32] = jlfs_armar(head)
        zona = area + b''.join(self.res)
        img = bytearray(self.img[:self.base]) + zona
        if len(img) > len(self.img):
            raise Error('la app no cabe en flash.bin')
        img += b'\xFF' * (len(self.img) - len(img))
        sfc(img, self.base, len(zona), self.base, self.key)
        return bytes(img)


# ----------------------------------------------------------------- órdenes ---
def leer(path):
    with open(path, 'rb') as f:
        return f.read()


def oficial(path):
    raw = leer(path)
    sha = hashlib.sha256(raw).hexdigest()
    if sha != V15_SHA256:
        raise Error('%s no es el FM-1 V15 de M-VAVE (sha256 %s)' % (path, sha))
    return raw


def cmd_info(a):
    raw = leer(a.paquete)
    p = Paquete(raw)
    print('archivo     %s (%d bytes, sha256 %s)' % (a.paquete, len(raw), hashlib.sha256(raw).hexdigest()))
    print('identidad   %s   chip %s   imagen lógica %d bytes' % (p.ident, p.chip, len(p.log)))
    for e in p.ents:
        print('  %-15s tipo %3d  @0x%06X  %7d bytes  crc %04X' % (e['name'], e['type'], e['off'], e['size'], e['dcrc']))
    f = Flash(p.flash())
    print('flash.bin   llave del chip %04X, zona de la app en 0x%X, punto de entrada 0x%08X' % (f.key, f.base, f.head['off']))
    for e in f.subs:
        print('  %-15s flags %02X  +0x%06X  %7d bytes' % (e['name'] or e['raw_name'][:4].hex(), e['flags'], e['off'], e['size']))
    print('  recursos: %d (%d bytes); la zona cifrada termina en 0x%X' % (len(f.res), sum(map(len, f.res)), f.end))
    print('app.bin     %d bytes; caben %d (%d libres)' % (len(f.app()), f.espacio(), f.espacio() - len(f.app())))


def cmd_idavuelta(a):
    raw = oficial(a.oficial)
    p = Paquete(raw)
    f = Flash(p.flash())
    otra = armar_paquete(p, f.armar(f.app()), p.ident)
    if otra != raw:
        n = next(i for i in range(min(len(otra), len(raw))) if otra[i] != raw[i]) if len(otra) == len(raw) else -1
        raise Error('la vuelta NO da el archivo oficial (primer byte distinto: %d)' % n)
    print('ida y vuelta: el V15 oficial desarmado y vuelto a armar es idéntico byte por byte (%d bytes)' % len(raw))


def cmd_armar(a):
    raw = oficial(a.oficial)
    p = Paquete(raw)
    f = Flash(p.flash())
    app = leer(a.app)
    out = armar_paquete(p, f.armar(app), a.identidad)
    # comprobación: lo que salió se vuelve a leer con el mismo lector y trae la app tal cual
    q = Paquete(out)
    g = Flash(q.flash())
    if g.app() != app or q.ident != a.identidad or g.dec[:g.base] != f.dec[:f.base]:
        raise Error('el paquete armado no se lee igual que lo que se metió')
    for e in p.ents:
        if e['type'] != 0 and p.datos(e) != q.datos(q.entrada(e['type'])):
            raise Error('cambió %s, que tenía que quedar igual' % e['name'])
    with open(a.salida, 'wb') as fo:
        fo.write(out)
    print('paquete %s: %d bytes, identidad %s, app.bin %d bytes (%d libres), sha256 %s' % (
        a.salida, len(out), a.identidad, len(app), f.espacio() - len(app), hashlib.sha256(out).hexdigest()))


def cmd_sacar(a):
    p = Paquete(leer(a.paquete))
    os.makedirs(a.dir, exist_ok=True)
    fl = p.flash()
    f = Flash(fl)
    for n, d in [('flash.bin', fl)] + [(e['name'], p.datos(e)) for e in p.ents if e['type'] != 0] + list(f.files.items()):
        with open(os.path.join(a.dir, n), 'wb') as fo:
            fo.write(d)
    print('extraído en', a.dir)


def main():
    ap = argparse.ArgumentParser(description='paquetes .fwsc del FM-1')
    sub = ap.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('info'); s.add_argument('paquete'); s.set_defaults(fn=cmd_info)
    s = sub.add_parser('idavuelta'); s.add_argument('oficial'); s.set_defaults(fn=cmd_idavuelta)
    s = sub.add_parser('armar'); s.add_argument('oficial'); s.add_argument('app'); s.add_argument('salida')
    s.add_argument('--identidad', default=IDENTIDAD); s.set_defaults(fn=cmd_armar)
    s = sub.add_parser('sacar'); s.add_argument('paquete'); s.add_argument('dir'); s.set_defaults(fn=cmd_sacar)
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    try:
        a.fn(a)
    except Error as e:
        sys.exit('fwsc: ' + str(e))


if __name__ == '__main__':
    main()
