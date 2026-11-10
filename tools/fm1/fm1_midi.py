#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""MIDI hacia el FM-1 por SysEx, sin dependencias.

    with Puerto('Bubba') as p:
        p.enviar(bytes([0xF0, ..., 0xF7]))
        for m in p.recibir(1.0): ...

Windows: WinMM. Linux: /dev/snd/midi* (ALSA raw MIDI), nombres vía `amidi -l`.
Abre la entrada y la salida cuyo nombre contiene el texto dado.
"""
import os, re, select, subprocess, sys, time


def _puertos_win():
    import ctypes
    from ctypes import wintypes as W

    class Caps(ctypes.Structure):
        _fields_ = [('wMid', W.WORD), ('wPid', W.WORD), ('v', W.UINT),
                    ('szPname', W.WCHAR * 32), ('pad', ctypes.c_byte * 64)]

    w = ctypes.windll.winmm
    ins, outs = [], []
    for i in range(w.midiInGetNumDevs()):
        c = Caps(); w.midiInGetDevCapsW(i, ctypes.byref(c), ctypes.sizeof(c)); ins.append(c.szPname)
    for i in range(w.midiOutGetNumDevs()):
        c = Caps(); w.midiOutGetDevCapsW(i, ctypes.byref(c), ctypes.sizeof(c)); outs.append(c.szPname)
    return ins, outs


def _amidi_list():
    """[(hw, name), ...] from `amidi -l` (IO lines only)."""
    try:
        out = subprocess.check_output(['amidi', '-l'], text=True, stderr=subprocess.DEVNULL)
    except (OSError, subprocess.CalledProcessError):
        return []
    rows = []
    for line in out.splitlines():
        m = re.match(r'^IO\s+(hw:\S+)\s+(.+)$', line.strip())
        if m:
            rows.append((m.group(1), m.group(2).strip()))
    return rows


def _hw_to_dev(hw):
    # hw:1,0,0 -> /dev/snd/midiC1D0
    m = re.match(r'^hw:(\d+),(\d+)', hw)
    if not m:
        raise OSError('dispositivo ALSA no reconocido: %s' % hw)
    return '/dev/snd/midiC%sD%s' % (m.group(1), m.group(2))


def puertos():
    if sys.platform == 'win32':
        return _puertos_win()
    names = [n for _, n in _amidi_list()]
    return names, names


class _PuertoWin:
    def __init__(self, nombre):
        import ctypes
        from ctypes import wintypes as W

        class Caps(ctypes.Structure):
            _fields_ = [('wMid', W.WORD), ('wPid', W.WORD), ('v', W.UINT),
                        ('szPname', W.WCHAR * 32), ('pad', ctypes.c_byte * 64)]

        class Hdr(ctypes.Structure):
            pass

        Hdr._fields_ = [('lpData', ctypes.c_void_p), ('dwBufferLength', W.DWORD),
                        ('dwBytesRecorded', W.DWORD), ('dwUser', ctypes.c_size_t),
                        ('dwFlags', W.DWORD), ('lpNext', ctypes.c_void_p),
                        ('reserved', ctypes.c_size_t), ('dwOffset', W.DWORD),
                        ('dwReserved', ctypes.c_size_t * 8)]

        self.w = w = ctypes.windll.winmm
        ins, outs = _puertos_win()
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
        self.bufs, self._Hdr, self._MHDR_DONE = [], Hdr, 1
        for _ in range(16):
            b = ctypes.create_string_buffer(4096)
            h = Hdr(); h.lpData = ctypes.cast(b, ctypes.c_void_p); h.dwBufferLength = 4096
            w.midiInPrepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
            w.midiInAddBuffer(self.hin, ctypes.byref(h), ctypes.sizeof(h))
            self.bufs.append((b, h))
        w.midiInStart(self.hin)

    def enviar(self, data):
        import ctypes
        from ctypes import wintypes as W
        b = ctypes.create_string_buffer(bytes(data), len(data))
        h = self._Hdr(); h.lpData = ctypes.cast(b, ctypes.c_void_p); h.dwBufferLength = len(data)
        self.w.midiOutPrepareHeader(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        r = self.w.midiOutLongMsg(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        t = time.time() + 2
        while not (h.dwFlags & self._MHDR_DONE) and time.time() < t:
            time.sleep(0.001)
        self.w.midiOutUnprepareHeader(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        if r:
            raise OSError('no se pudo enviar (%d)' % r)

    def corto(self, st, d1=0, d2=0):
        self.w.midiOutShortMsg(self.hout, st | (d1 << 8) | (d2 << 16))

    def recibir(self, espera=1.0, hasta=None):
        fin, out = time.time() + espera, []
        while time.time() < fin:
            for b, h in self.bufs:
                if h.dwFlags & self._MHDR_DONE:
                    d = b.raw[:h.dwBytesRecorded]
                    self.w.midiInUnprepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
                    h.dwBytesRecorded = 0; h.dwFlags = 0
                    self.w.midiInPrepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
                    self.w.midiInAddBuffer(self.hin, ctypes.byref(h), ctypes.sizeof(h))
                    if d:
                        out.append(d)
                        if hasta and hasta(d):
                            return out
            time.sleep(0.002)
        return out

    def cerrar(self):
        w = self.w
        w.midiInStop(self.hin); w.midiInReset(self.hin)
        for b, h in self.bufs:
            w.midiInUnprepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
        w.midiInClose(self.hin); w.midiOutClose(self.hout)


class _PuertoLinux:
    def __init__(self, nombre):
        rows = _amidi_list()
        hit = next(((hw, n) for hw, n in rows if nombre.lower() in n.lower()), None)
        if hit is None:
            raise OSError('no hay un puerto MIDI "%s" (vistos: %s)' % (
                nombre, [n for _, n in rows]))
        self.nombre = hit[1]
        path = _hw_to_dev(hit[0])
        try:
            self.fd = os.open(path, os.O_RDWR | os.O_NONBLOCK)
        except OSError as e:
            raise OSError('no se pudo abrir %s (%s): %s' % (path, self.nombre, e)) from e
        self._buf = bytearray()

    def enviar(self, data):
        data = bytes(data)
        off, t = 0, time.time() + 2
        while off < len(data):
            if time.time() >= t:
                raise OSError('no se pudo enviar (timeout)')
            try:
                off += os.write(self.fd, data[off:])
            except BlockingIOError:
                select.select([], [self.fd], [], 0.05)

    def corto(self, st, d1=0, d2=0):
        self.enviar(bytes([st & 0xFF, d1 & 0x7F, d2 & 0x7F]))

    def _sacar_sysex(self):
        out = []
        while True:
            try:
                i = self._buf.index(0xF0)
            except ValueError:
                self._buf.clear()
                return out
            if i:
                del self._buf[:i]
            try:
                j = self._buf.index(0xF7)
            except ValueError:
                return out
            out.append(bytes(self._buf[:j + 1]))
            del self._buf[:j + 1]

    def recibir(self, espera=1.0, hasta=None):
        fin, out = time.time() + espera, []
        while time.time() < fin:
            r, _, _ = select.select([self.fd], [], [], min(0.05, max(0, fin - time.time())))
            if r:
                try:
                    chunk = os.read(self.fd, 4096)
                except BlockingIOError:
                    chunk = b''
                if chunk:
                    self._buf.extend(chunk)
                    for m in self._sacar_sysex():
                        out.append(m)
                        if hasta and hasta(m):
                            return out
            else:
                for m in self._sacar_sysex():
                    out.append(m)
                    if hasta and hasta(m):
                        return out
        return out

    def cerrar(self):
        if getattr(self, 'fd', None) is not None:
            os.close(self.fd)
            self.fd = None


class Puerto:
    def __init__(self, nombre):
        self._p = _PuertoWin(nombre) if sys.platform == 'win32' else _PuertoLinux(nombre)
        self.nombre = self._p.nombre

    def enviar(self, data):
        self._p.enviar(data)

    def corto(self, st, d1=0, d2=0):
        self._p.corto(st, d1, d2)

    def recibir(self, espera=1.0, hasta=None):
        return self._p.recibir(espera, hasta)

    def cerrar(self):
        self._p.cerrar()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.cerrar()
