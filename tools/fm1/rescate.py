#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Rescate del FM-1 desde Windows, por el modo de actualización del chip (USB 4C4A:8057).

Para cuando el FM-1 ya no arranca ningún firmware con USB (ni el modo seguro). Bubba Machine
lleva al chip a ese modo solo: después de cuatro arranques fallidos, o con OCT- y OCT+ juntos
5 s. El FM-1 aparece entonces como un disco "UBOOT" y esta herramienta habla con él.

  py -3 tools/fm1/rescate.py respaldo                 copia toda la flash (1 MiB) a un archivo
  py -3 tools/fm1/rescate.py restaurar FM-1.fwsc      vuelve a poner el firmware de un paquete
  py -3 tools/fm1/rescate.py restaurar respaldo.bin   o el de un respaldo
  py -3 tools/fm1/rescate.py salir                    reinicia el FM-1 sin cambiar nada

Necesita una consola de Windows abierta como administrador (para hablar con un disco) y Python 3.
No necesita drivers ni programas extra.

Lo que hace, en orden, y se detiene ante cualquier cosa rara:
  1. carga el cargador de flash de JieLi para el WL82 (wl82loader.bin, de jl-uboot-tool, que se
     descarga y se comprueba contra su huella) en la RAM del chip y lo arranca;
  2. comprueba que el chip sea el del FM-1: llave 0x980F y flash 0x856014 (1 MiB);
  3. SIEMPRE lee la flash entera y la guarda como respaldo antes de escribir nada;
  4. restaurar: solo escribe sectores de 4 KiB de la zona del programa [0x4000, 0x93000) que sean
     distintos de lo que tiene que quedar; el arranque (debajo de 0x4000) y los datos del aparato
     (de 0x93000 en adelante) no se escriben nunca, y si el arranque no coincide se detiene;
     cada sector se borra, se escribe y se vuelve a leer;
  5. lee todo de nuevo y lo compara; recién entonces reinicia el FM-1.

Protocolo (comandos SCSI propios de JieLi): jl-uboot-tool de kagaimiq (MIT),
https://github.com/kagaimiq/jl-uboot-tool, jltech/uboot.py y scsiio/win32port.py. La política de
escritura es la de FM-1-transporter y X0X, verificada en el aparato.
"""
import argparse, ctypes, datetime, hashlib, os, sys, time, urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fwsc  # noqa: E402

LOADER_URL = ('https://raw.githubusercontent.com/kagaimiq/jl-uboot-tool/'
              'c32b28e7a04f873f526ba929cb1bae9f05442fe9/data/loaderblobs/usb/wl82loader.bin')
LOADER_SHA256 = 'd41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376'
LOADER_ADDR, LOADER_ARG = 0x1C02000, 0x0001          # RAM; argumento 1 = flash SPI NOR
CHIP_KEY, FLASH_ID = 0x980F, 0x856014
FLASH_SIZE, APP_LO, APP_HI = 0x100000, 0x4000, 0x93000
SECTOR, IO = 0x1000, 512

# comandos: UBOOT1.00 (ROM) y el cargador
WRITE_MEMORY, JUMP_TO_MEMORY = 0xFB06, 0xFB08
ERASE_SECTOR, WRITE_FLASH, READ_FLASH = 0xFB01, 0xFB04, 0xFD05
READ_KEY, ONLINE_DEVICE, RUN_APP = 0xFC09, 0xFC0A, 0xFC0C


class Falla(Exception):
    pass


# ------------------------------------------------------------------ cifrados ---
MAGIC = bytes([0xC3, 0xCF, 0xC0, 0xE8, 0xCE, 0xD2, 0xB0, 0xAE, 0xC4, 0xE3, 0xA3, 0xAC, 0xD3, 0xF1, 0xC1, 0xD6])


def crc_cipher(data, key=0xFFFFFFFF):
    """el cifrado "CrcDecode" de JieLi (con el que el cargador devuelve la llave)"""
    buf = bytearray(data)
    crc = fwsc.crc16((key >> 16).to_bytes(2, 'little'), key & 0xFFFF)
    for i in range(len(buf)):
        crc = fwsc.crc16(MAGIC[i % 16:i % 16 + 1], crc)
        buf[i] ^= crc & 0xFF
    return bytes(buf)


# ------------------------------------------------------- SCSI en Windows ---
class Disco:
    """un disco abierto con SCSI_PASS_THROUGH_DIRECT (como scsiio/win32port.py)"""

    def __init__(self, path):
        from ctypes import wintypes as W

        class SPTD(ctypes.Structure):
            _fields_ = [('len', W.WORD), ('status', W.BYTE), ('path_id', W.BYTE), ('target_id', W.BYTE),
                        ('lun', W.BYTE), ('cdb_len', W.BYTE), ('sense_len', W.BYTE), ('dir', W.BYTE),
                        ('data_len', W.DWORD), ('timeout', W.DWORD), ('data_ptr', ctypes.c_void_p),
                        ('sense_off', W.DWORD), ('cdb', W.BYTE * 16)]
        self.SPTD = SPTD
        k = ctypes.WinDLL('kernel32', use_last_error=True)
        k.CreateFileW.restype = W.HANDLE
        k.CreateFileW.argtypes = [W.LPCWSTR, W.DWORD, W.DWORD, ctypes.c_void_p, W.DWORD, W.DWORD, W.HANDLE]
        self.k = k
        self.h = k.CreateFileW(path, 0xC0000000, 3, None, 3, 0, None)
        if self.h in (None, ctypes.c_void_p(-1).value, -1):
            raise OSError(ctypes.get_last_error(), 'no se pudo abrir ' + path)
        self.path = path

    def close(self):
        if self.h:
            self.k.CloseHandle(self.h)
            self.h = None

    def scsi(self, cdb, out=None, n_in=0, timeout=10):
        s = self.SPTD()
        s.len = ctypes.sizeof(s)
        s.target_id = 1
        s.timeout = timeout
        s.cdb_len = len(cdb)
        for i, b in enumerate(cdb):
            s.cdb[i] = b
        if out is not None:
            buf = ctypes.create_string_buffer(bytes(out), len(out))
            s.dir, s.data_len = 0, len(out)
        elif n_in:
            buf = ctypes.create_string_buffer(n_in)
            s.dir, s.data_len = 1, n_in
        else:
            buf = None
            s.dir, s.data_len = 2, 0
        s.data_ptr = ctypes.cast(buf, ctypes.c_void_p).value if buf is not None else None
        got = ctypes.c_ulong()
        ok = self.k.DeviceIoControl(self.h, 0x4D014, ctypes.byref(s), s.len, ctypes.byref(s), s.len,
                                    ctypes.byref(got), None)
        if not ok:
            raise Falla('el FM-1 no contestó (%d)' % ctypes.get_last_error())
        return buf.raw[:n_in] if n_in else b''


def cdb(cmd, args=b''):
    c = cmd.to_bytes(2, 'big') + args
    return c + b'\xFF' * (16 - len(c))


class Chip:
    def __init__(self, disco):
        self.d = disco

    def inquiry(self):
        r = self.d.scsi(b'\x12\x00\x00\x00\x24\x00', n_in=36)
        return r[8:16].decode('ascii', 'replace').strip(), r[16:32].decode('ascii', 'replace').strip()

    def cmd(self, c, args=b''):
        r = self.d.scsi(cdb(c, args), n_in=16)
        if int.from_bytes(r[:2], 'big') != c:
            raise Falla('respuesta inesperada al comando %04X' % c)
        return r[2:]

    def mem_write(self, addr, data):
        self.d.scsi(cdb(WRITE_MEMORY, addr.to_bytes(4, 'big') + len(data).to_bytes(2, 'big') + b'\x00' +
                        fwsc.crc16(data).to_bytes(2, 'little')), out=data)

    def jump(self, addr, arg):
        self.cmd(JUMP_TO_MEMORY, addr.to_bytes(4, 'big') + arg.to_bytes(2, 'big'))

    def key(self):
        r = self.cmd(READ_KEY, (0xAC6900).to_bytes(4, 'big'))
        return int.from_bytes(crc_cipher(r[4:6][::-1]), 'little')

    def flash_id(self):
        r = self.cmd(ONLINE_DEVICE)
        return int.from_bytes(r[2:6], 'little')

    def read(self, addr, n):
        return self.d.scsi(cdb(READ_FLASH, addr.to_bytes(4, 'big') + n.to_bytes(2, 'big')), n_in=n)

    def erase(self, addr):
        self.cmd(ERASE_SECTOR, addr.to_bytes(4, 'big'))

    def write(self, addr, data):
        self.d.scsi(cdb(WRITE_FLASH, addr.to_bytes(4, 'big') + len(data).to_bytes(2, 'big') + b'\x00' +
                        fwsc.crc16(data).to_bytes(2, 'little')), out=data)

    def run_app(self):
        try:
            self.cmd(RUN_APP, (1).to_bytes(4, 'big'))
        except Falla:
            pass                                 # se reinicia antes de contestar


# ------------------------------------------------------------------ pasos ---
def buscar(espera, say):
    """el primer disco UBOOT de JieLi; espera hasta `espera` s a que aparezca"""
    fin = time.time() + espera
    while True:
        for i in range(32):
            path = r'\\.\PhysicalDrive%d' % i
            try:
                d = Disco(path)
            except OSError:
                continue
            try:
                ven, prod = Chip(d).inquiry()
                if prod.startswith('UBOOT') or ven.upper().startswith('WL8'):
                    say('encontrado: %s %s en %s' % (ven, prod, path))
                    return d, ven, prod
            except Falla:
                pass
            d.close()
        if time.time() > fin:
            raise Falla('no aparece el FM-1 en modo del chip. ¿Consola como administrador? ¿Cable de datos?')
        time.sleep(0.5)


def cargador(path=None):
    cache = path or os.path.join(os.path.expanduser('~'), 'jieli', 'wl82loader.bin')
    if os.path.isfile(cache):
        blob = open(cache, 'rb').read()
    else:
        blob = urllib.request.urlopen(LOADER_URL, timeout=30).read()
    if hashlib.sha256(blob).hexdigest() != LOADER_SHA256:
        raise Falla('el cargador de flash no es el esperado (huella distinta): no se hizo nada')
    if not os.path.isfile(cache):
        os.makedirs(os.path.dirname(cache), exist_ok=True)
        open(cache, 'wb').write(blob)
    return blob


def preparar(chip, prod, blob, say):
    if prod.startswith('UBOOT1'):
        say('cargando el cargador de flash en la RAM del chip…')
        for o in range(0, len(blob), IO):
            chip.mem_write(LOADER_ADDR + o, blob[o:o + IO])
        chip.jump(LOADER_ADDR, LOADER_ARG)
        time.sleep(0.3)
    k, fid = chip.key(), chip.flash_id()
    say('llave del chip %04X, flash %06X' % (k, fid))
    if k != CHIP_KEY or fid != FLASH_ID:
        raise Falla('no es el chip del FM-1 (se esperaba llave %04X y flash %06X): no se hizo nada' % (CHIP_KEY, FLASH_ID))


def leer_todo(chip, say):
    out = bytearray()
    for a in range(0, FLASH_SIZE, IO):
        out += chip.read(a, IO)
        if a % 0x20000 == 0:
            say('  leyendo %3d %%' % (a * 100 // FLASH_SIZE))
    return bytes(out)


def respaldo(chip, carpeta, say):
    a = leer_todo(chip, say)
    b = leer_todo(chip, say)
    if a != b:
        raise Falla('dos lecturas de la flash no coinciden: no se sigue')
    os.makedirs(carpeta, exist_ok=True)
    nombre = os.path.join(carpeta, 'fm1-respaldo-%s.bin' % datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    open(nombre, 'wb').write(a)
    say('respaldo: %s (sha256 %s)' % (nombre, hashlib.sha256(a).hexdigest()))
    return a


def objetivo(path):
    """lo que tiene que quedar en [0, 0x93000): el flash.bin de un paquete, o un respaldo"""
    raw = open(path, 'rb').read()
    if len(raw) == FLASH_SIZE:
        return raw[:APP_HI]
    p = fwsc.Paquete(raw)
    fl = p.flash()
    if len(fl) != APP_HI:
        raise Falla('el flash.bin del paquete no mide 0x%X' % APP_HI)
    return fl


def plan(actual, meta):
    """sectores de la zona del programa que hay que escribir; falla si el arranque difiere"""
    if actual[:APP_LO] != meta[:APP_LO]:
        raise Falla('el arranque del FM-1 (debajo de 0x4000) no es el del archivo: no se escribe nada')
    return [a for a in range(APP_LO, APP_HI, SECTOR) if actual[a:a + SECTOR] != meta[a:a + SECTOR]]


def restaurar(chip, actual, meta, say, confirmar):
    sect = plan(actual, meta)
    if not sect:
        say('la zona del programa ya es igual al archivo: no hace falta escribir')
        return
    say('hay que escribir %d sectores de 4 KiB entre 0x%X y 0x%X' % (len(sect), sect[0], sect[-1] + SECTOR))
    if not confirmar():
        raise Falla('cancelado: no se escribió nada')
    for n, a in enumerate(sect):
        chip.erase(a)
        for o in range(0, SECTOR, IO):
            chip.write(a + o, meta[a + o:a + o + IO])
        if b''.join(chip.read(a + o, IO) for o in range(0, SECTOR, IO)) != meta[a:a + SECTOR]:
            raise Falla('el sector 0x%X no quedó bien: NO desconectes, vuelve a correr restaurar' % a)
        if n % 16 == 0:
            say('  escribiendo %d / %d' % (n + 1, len(sect)))
    final = leer_todo(chip, say)
    if final[:APP_HI] != meta or final[APP_HI:] != actual[APP_HI:]:
        raise Falla('la lectura final no coincide: NO desconectes, vuelve a correr restaurar')
    say('escrito y comprobado')


def main():
    ap = argparse.ArgumentParser(description='rescate del FM-1 por el modo del chip (Windows)')
    sub = ap.add_subparsers(dest='orden', required=True)
    sub.add_parser('respaldo')
    s = sub.add_parser('restaurar'); s.add_argument('archivo')
    s.add_argument('--si', action='store_true', help='no preguntar antes de escribir')
    sub.add_parser('salir')
    ap.add_argument('--carpeta', default=os.path.join(os.path.expanduser('~'), 'fm1-rescate'))
    ap.add_argument('--espera', type=int, default=300, help='segundos esperando al FM-1')
    ap.add_argument('--cargador', help='wl82loader.bin ya descargado')
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    say = lambda t: print(t, flush=True)
    if sys.platform != 'win32':
        sys.exit('rescate: esta herramienta es para Windows')
    try:
        meta = objetivo(a.archivo) if a.orden == 'restaurar' else None
        blob = cargador(a.cargador)
        say('esperando al FM-1 en modo del chip (conéctalo y préndelo)…')
        disco, ven, prod = buscar(a.espera, say)
        try:
            chip = Chip(disco)
            preparar(chip, prod, blob, say)
            if a.orden == 'salir':
                chip.run_app()
                say('reiniciado')
                return
            actual = respaldo(chip, a.carpeta, say)
            if a.orden == 'restaurar':
                pregunta = lambda: a.si or input('¿Escribir? Escribe "si" y Enter: ').strip().lower() in ('si', 'sí')
                restaurar(chip, actual, meta, say, pregunta)
                say('reiniciando el FM-1…')
                chip.run_app()
        finally:
            disco.close()
    except (Falla, fwsc.Error) as e:
        sys.exit('rescate: ' + str(e))


if __name__ == '__main__':
    main()
