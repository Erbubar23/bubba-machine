# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Prueba de tools/fm1/rescate.py contra un chip simulado (sin aparato, sin USB).

  py -3 tests/test_rescate.py
"""
import os, random, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools', 'fm1'))
import rescate as R  # noqa: E402

fallos = 0


def ok(c, m):
    global fallos
    print(('ok   ' if c else 'FALLA ') + m)
    fallos += 0 if c else 1


class ChipFalso:
    def __init__(self, flash):
        self.flash = bytearray(flash)
        self.borrados, self.escritos = [], 0

    def read(self, a, n):
        return bytes(self.flash[a:a + n])

    def erase(self, a):
        assert a % R.SECTOR == 0 and R.APP_LO <= a < R.APP_HI, 'borrado fuera de la zona del programa: 0x%X' % a
        self.flash[a:a + R.SECTOR] = b'\xFF' * R.SECTOR
        self.borrados.append(a)

    def write(self, a, d):
        assert R.APP_LO <= a and a + len(d) <= R.APP_HI, 'escritura fuera de la zona del programa: 0x%X' % a
        for i, b in enumerate(d):
            self.flash[a + i] &= b
        self.escritos += 1


rnd = random.Random(7)
actual = bytes(rnd.getrandbits(8) for _ in range(R.FLASH_SIZE))
meta = bytearray(actual[:R.APP_HI])
for a in (0x4000, 0x5000, 0x80000):
    meta[a + 10] ^= 0x55
meta = bytes(meta)
say = lambda t: None

ok(R.plan(actual, meta) == [0x4000, 0x5000, 0x80000], 'solo se escriben los 3 sectores distintos')

chip = ChipFalso(actual)
R.restaurar(chip, actual, meta, say, lambda: True)
ok(bytes(chip.flash[:R.APP_HI]) == meta and bytes(chip.flash[R.APP_HI:]) == actual[R.APP_HI:],
   'queda el programa del archivo y los datos del aparato intactos')
ok(chip.borrados == [0x4000, 0x5000, 0x80000], 'borra solo esos sectores')

otro = bytearray(meta)
otro[0x100] ^= 1
try:
    R.plan(actual, bytes(otro))
    ok(False, 'un arranque distinto se rechaza')
except R.Falla as e:
    ok('arranque' in str(e), 'un arranque distinto se rechaza: ' + str(e))

chip = ChipFalso(actual)
try:
    R.restaurar(chip, actual, meta, say, lambda: False)
    ok(False, 'sin confirmación no escribe')
except R.Falla:
    ok(not chip.borrados and chip.escritos == 0, 'sin confirmación no escribe nada')

chip = ChipFalso(actual)
R.restaurar(chip, actual, actual[:R.APP_HI], say, lambda: True)
ok(not chip.borrados, 'si ya es igual, no escribe')

# la llave: el mismo cifrado que jl-uboot-tool (vector calculado con su jltech.cipher.jl_crc_cipher)
ok(R.crc_cipher(bytes(range(16))).hex() == '2fa583cf4ac45bede6abbcad2587daf8', 'el cifrado de la llave es el de jl-uboot-tool')

print('%d FALLAS' % fallos if fallos else 'todo bien')
sys.exit(1 if fallos else 0)
