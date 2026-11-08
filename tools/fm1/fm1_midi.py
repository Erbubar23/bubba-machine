#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""MIDI sin dependencias para hablar con el FM-1 por SysEx.

    with Puerto('Bubba') as p:
        p.enviar(bytes([0xF0, ..., 0xF7]))
        for m in p.recibir(1.0): ...

Windows: WinMM. Linux: ALSA raw MIDI (/dev/snd/midi*). Abre el puerto cuyo
nombre contiene el texto dado (entrada y salida del mismo aparato).
"""
import glob
import os
import select
import sys
import time

# --------------------------------------------------------------------------- API

def puertos():
    """([nombres entrada], [nombres salida]) del sistema."""
    return _backend().listar()


class Puerto:
    def __init__(self, nombre):
        self._impl = _backend().abrir(nombre)
        self.nombre = self._impl.nombre

    def enviar(self, data):
        self._impl.enviar(data)

    def corto(self, st, d1=0, d2=0):
        """un mensaje MIDI corto (nota, CC...)"""
        self._impl.corto(st, d1, d2)

    def recibir(self, espera=1.0, hasta=None):
        """SysEx completos que llegan en `espera` s; corta antes si `hasta(msg)` es verdadero"""
        return self._impl.recibir(espera, hasta)

    def cerrar(self):
        self._impl.cerrar()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.cerrar()


def _backend():
    if sys.platform == 'win32':
        return _WinMM()
    if sys.platform.startswith('linux'):
        return _AlsaRaw()
    raise OSError('MIDI solo en Windows o Linux')


# -------------------------------------------------------------------- Windows

class _WinMM:
    def listar(self):
        import ctypes
        w, Caps = self._winmm()
        ins, outs = [], []
        for i in range(w.midiInGetNumDevs()):
            c = Caps()
            w.midiInGetDevCapsW(i, ctypes.byref(c), ctypes.sizeof(c))
            ins.append(c.szPname)
        for i in range(w.midiOutGetNumDevs()):
            c = Caps()
            w.midiOutGetDevCapsW(i, ctypes.byref(c), ctypes.sizeof(c))
            outs.append(c.szPname)
        return ins, outs

    def abrir(self, nombre):
        return _PuertoWin(nombre, self)

    def _winmm(self):
        import ctypes
        from ctypes import wintypes as W

        class Caps(ctypes.Structure):
            _fields_ = [('wMid', W.WORD), ('wPid', W.WORD), ('v', W.UINT),
                        ('szPname', W.WCHAR * 32), ('pad', ctypes.c_byte * 64)]

        return ctypes.windll.winmm, Caps


class _PuertoWin:
    MHDR_DONE = 1
    NBUF, TBUF = 16, 4096

    def __init__(self, nombre, backend):
        import ctypes
        from ctypes import wintypes as W

        class Hdr(ctypes.Structure):
            pass

        Hdr._fields_ = [
            ('lpData', ctypes.c_void_p), ('dwBufferLength', W.DWORD),
            ('dwBytesRecorded', W.DWORD), ('dwUser', ctypes.c_size_t),
            ('dwFlags', W.DWORD), ('lpNext', ctypes.c_void_p),
            ('reserved', ctypes.c_size_t), ('dwOffset', W.DWORD),
            ('dwReserved', ctypes.c_size_t * 8),
        ]

        w, Caps = backend._winmm()
        self.w, self._ctypes, self._Hdr = w, ctypes, Hdr
        ins, outs = backend.listar()
        ii = next((i for i, n in enumerate(ins) if nombre.lower() in n.lower()), None)
        oo = next((i for i, n in enumerate(outs) if nombre.lower() in n.lower()), None)
        if ii is None or oo is None:
            raise OSError('no hay un puerto MIDI "%s" (entradas %s, salidas %s)' % (nombre, ins, outs))
        self.nombre = ins[ii]
        self.hin, self.hout = W.HANDLE(), W.HANDLE()
        if w.midiInOpen(ctypes.byref(self.hin), ii, 0, 0, 0):
            raise OSError('la entrada MIDI "%s" está ocupada por otro programa' % ins[ii])
        if w.midiOutOpen(ctypes.byref(self.hout), oo, 0, 0, 0):
            w.midiInClose(self.hin)
            raise OSError('la salida MIDI "%s" está ocupada por otro programa' % outs[oo])
        self.bufs = []
        for _ in range(self.NBUF):
            b = ctypes.create_string_buffer(self.TBUF)
            h = Hdr()
            h.lpData = ctypes.cast(b, ctypes.c_void_p)
            h.dwBufferLength = self.TBUF
            w.midiInPrepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
            w.midiInAddBuffer(self.hin, ctypes.byref(h), ctypes.sizeof(h))
            self.bufs.append((b, h))
        w.midiInStart(self.hin)

    def enviar(self, data):
        ctypes, Hdr, w = self._ctypes, self._Hdr, self.w
        b = ctypes.create_string_buffer(bytes(data), len(data))
        h = Hdr()
        h.lpData = ctypes.cast(b, ctypes.c_void_p)
        h.dwBufferLength = len(data)
        w.midiOutPrepareHeader(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        r = w.midiOutLongMsg(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        t = time.time() + 2
        while not (h.dwFlags & self.MHDR_DONE) and time.time() < t:
            time.sleep(0.001)
        w.midiOutUnprepareHeader(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        if r:
            raise OSError('no se pudo enviar (%d)' % r)

    def corto(self, st, d1=0, d2=0):
        self.w.midiOutShortMsg(self.hout, st | (d1 << 8) | (d2 << 16))

    def recibir(self, espera=1.0, hasta=None):
        ctypes, fin, out = self._ctypes, time.time() + espera, []
        while time.time() < fin:
            for b, h in self.bufs:
                if h.dwFlags & self.MHDR_DONE:
                    d = b.raw[:h.dwBytesRecorded]
                    self.w.midiInUnprepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
                    h.dwBytesRecorded = 0
                    h.dwFlags = 0
                    self.w.midiInPrepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
                    self.w.midiInAddBuffer(self.hin, ctypes.byref(h), ctypes.sizeof(h))
                    if d:
                        out.append(d)
                        if hasta and hasta(d):
                            return out
            time.sleep(0.002)
        return out

    def cerrar(self):
        w, ctypes = self.w, self._ctypes
        w.midiInStop(self.hin)
        w.midiInReset(self.hin)
        for b, h in self.bufs:
            w.midiInUnprepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
        w.midiInClose(self.hin)
        w.midiOutClose(self.hout)


# ---------------------------------------------------------------------- Linux

def _nombre_tarjeta(card):
    """Prefer USB product string, then /proc/asound, then the short card id."""
    sys_card = '/sys/class/sound/card%d' % card
    # Walk up from the sound card to the USB device product (works inside containers).
    try:
        node = os.path.realpath(sys_card)
        for _ in range(8):
            product = os.path.join(node, 'product')
            if os.path.isfile(product):
                name = open(product, encoding='utf-8', errors='replace').read().strip()
                if name:
                    return name
            parent = os.path.dirname(node)
            if parent == node:
                break
            node = parent
    except OSError:
        pass
    try:
        lines = open('/proc/asound/cards', encoding='utf-8', errors='replace').read().splitlines()
        for i in range(0, len(lines) - 1, 2):
            head = lines[i].strip()
            if not head or not head[0].isdigit() or int(head.split()[0]) != card:
                continue
            long_name = lines[i + 1].strip()
            return long_name.split(' - ', 1)[-1] if ' - ' in long_name else long_name
    except OSError:
        pass
    try:
        return open(os.path.join(sys_card, 'id'), encoding='utf-8', errors='replace').read().strip()
    except OSError:
        return 'card%d' % card


def _dispositivos_alsa():
    """[(nombre visible, path /dev/snd/midiC*D*), ...]"""
    found = []
    for path in sorted(glob.glob('/dev/snd/midiC*D*')):
        base = os.path.basename(path)  # midiC1D0
        try:
            rest = base[len('midiC'):]
            card_s, dev_s = rest.split('D', 1)
            card, dev = int(card_s), int(dev_s)
        except ValueError:
            continue
        label = _nombre_tarjeta(card)
        found.append(('%s (hw:%d,%d)' % (label, card, dev), path))
    return found


class _AlsaRaw:
    def listar(self):
        names = [n for n, _ in _dispositivos_alsa()]
        return names, list(names)

    def abrir(self, nombre):
        return _PuertoAlsa(nombre)


class _PuertoAlsa:
    def __init__(self, nombre):
        match = next(((n, p) for n, p in _dispositivos_alsa() if nombre.lower() in n.lower()), None)
        if match is None:
            hay = [n for n, _ in _dispositivos_alsa()]
            raise OSError('no hay un puerto MIDI "%s" (disponibles %s)' % (nombre, hay))
        self.nombre, path = match
        try:
            self.fd = os.open(path, os.O_RDWR | os.O_NONBLOCK)
        except OSError as e:
            raise OSError('no se pudo abrir "%s" (%s): %s' % (self.nombre, path, e)) from e
        self._sysex = None  # bytes being assembled, or None

    def enviar(self, data):
        raw = bytes(data)
        off, t = 0, time.time() + 2
        while off < len(raw):
            try:
                off += os.write(self.fd, raw[off:])
            except BlockingIOError:
                if time.time() > t:
                    raise OSError('no se pudo enviar (timeout)')
                time.sleep(0.001)

    def corto(self, st, d1=0, d2=0):
        msg = bytes([st & 0xFF])
        if st < 0xF0:
            n = 1 if (st & 0xE0) == 0xC0 or (st & 0xE0) == 0xD0 else 2
            msg += bytes([d1 & 0x7F] + ([d2 & 0x7F] if n == 2 else []))
        elif st in (0xF1, 0xF3):
            msg += bytes([d1 & 0x7F])
        elif st == 0xF2:
            msg += bytes([d1 & 0x7F, d2 & 0x7F])
        self.enviar(msg)

    def recibir(self, espera=1.0, hasta=None):
        fin, out = time.time() + espera, []
        while time.time() < fin:
            for msg in self._leer():
                out.append(msg)
                if hasta and hasta(msg):
                    return out
            timeout = max(0.0, min(0.05, fin - time.time()))
            try:
                select.select([self.fd], [], [], timeout)
            except (ValueError, OSError):
                break
        return out

    def _leer(self):
        """Consume available bytes; yield complete SysEx messages."""
        while True:
            try:
                chunk = os.read(self.fd, 4096)
            except BlockingIOError:
                return
            if not chunk:
                return
            for msg in self._alimentar(chunk):
                yield msg

    def _alimentar(self, chunk):
        for b in chunk:
            if b >= 0xF8:  # realtime: may appear anywhere, never part of SysEx payload
                continue
            if self._sysex is None:
                if b == 0xF0:
                    self._sysex = bytearray([0xF0])
                continue
            self._sysex.append(b)
            if b == 0xF7:
                msg = bytes(self._sysex)
                self._sysex = None
                yield msg

    def cerrar(self):
        if getattr(self, 'fd', None) is not None:
            os.close(self.fd)
            self.fd = None
