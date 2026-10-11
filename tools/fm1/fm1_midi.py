#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""MIDI de Windows (WinMM) sin dependencias, para hablar con el FM-1 por SysEx.

    with Puerto('Bubba') as p:
        p.enviar(bytes([0xF0, ..., 0xF7]))
        for m in p.recibir(1.0): ...

Solo Windows. Abre la entrada y la salida cuyo nombre contiene el texto dado.
"""
import ctypes, sys, time
from ctypes import wintypes as W


class _Caps(ctypes.Structure):
    _fields_ = [('wMid', W.WORD), ('wPid', W.WORD), ('v', W.UINT), ('szPname', W.WCHAR * 32), ('pad', ctypes.c_byte * 64)]


class _Hdr(ctypes.Structure):
    pass


_Hdr._fields_ = [('lpData', ctypes.c_void_p), ('dwBufferLength', W.DWORD), ('dwBytesRecorded', W.DWORD),
                 ('dwUser', ctypes.c_size_t), ('dwFlags', W.DWORD), ('lpNext', ctypes.c_void_p),
                 ('reserved', ctypes.c_size_t), ('dwOffset', W.DWORD), ('dwReserved', ctypes.c_size_t * 8)]

MHDR_DONE = 1
NBUF, TBUF = 16, 4096


def puertos():
    w = ctypes.windll.winmm
    ins, outs = [], []
    for i in range(w.midiInGetNumDevs()):
        c = _Caps(); w.midiInGetDevCapsW(i, ctypes.byref(c), ctypes.sizeof(c)); ins.append(c.szPname)
    for i in range(w.midiOutGetNumDevs()):
        c = _Caps(); w.midiOutGetDevCapsW(i, ctypes.byref(c), ctypes.sizeof(c)); outs.append(c.szPname)
    return ins, outs


class Puerto:
    def __init__(self, nombre):
        if sys.platform != 'win32':
            raise OSError('solo Windows')
        self.w = w = ctypes.windll.winmm
        ins, outs = puertos()
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
        for _ in range(NBUF):
            b = ctypes.create_string_buffer(TBUF)
            h = _Hdr(); h.lpData = ctypes.cast(b, ctypes.c_void_p); h.dwBufferLength = TBUF
            w.midiInPrepareHeader(self.hin, ctypes.byref(h), ctypes.sizeof(h))
            w.midiInAddBuffer(self.hin, ctypes.byref(h), ctypes.sizeof(h))
            self.bufs.append((b, h))
        w.midiInStart(self.hin)

    def enviar(self, data):
        b = ctypes.create_string_buffer(bytes(data), len(data))
        h = _Hdr(); h.lpData = ctypes.cast(b, ctypes.c_void_p); h.dwBufferLength = len(data)
        self.w.midiOutPrepareHeader(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        r = self.w.midiOutLongMsg(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        t = time.time() + 2
        while not (h.dwFlags & MHDR_DONE) and time.time() < t:
            time.sleep(0.001)
        self.w.midiOutUnprepareHeader(self.hout, ctypes.byref(h), ctypes.sizeof(h))
        if r:
            raise OSError('no se pudo enviar (%d)' % r)

    def corto(self, st, d1=0, d2=0):
        """un mensaje MIDI corto (nota, CC...)"""
        self.w.midiOutShortMsg(self.hout, st | (d1 << 8) | (d2 << 16))

    def recibir(self, espera=1.0, hasta=None):
        """SysEx completos que llegan en `espera` s; corta antes si `hasta(msg)` es verdadero"""
        fin, out = time.time() + espera, []
        while time.time() < fin:
            for b, h in self.bufs:
                if h.dwFlags & MHDR_DONE:
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

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.cerrar()
