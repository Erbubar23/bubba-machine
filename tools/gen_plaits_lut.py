#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Tablas de Plaits que usa Bubba Machine, sacadas del resources.cc original (MIT).

El resources.cc de Plaits trae además wavetables, voces de habla y bancos de la DX7 que Bubba no
usa (y que no se publican). Este script copia solo las tablas pedidas, con su licencia, a
third_party/plaits/resources.cc y .h.

  py -3 tools/gen_plaits_lut.py ruta/al/resources.cc/original
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLAS = ['lut_sine', 'lut_stiffness', 'lut_svf_shift', 'lut_4x_downsampler_fir', 'lut_fm_frequency_quantizer']

LICENCIA = '''// Copyright 2016 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Resources definitions.
//
// Modificado para Bubba Machine (2026): solo las tablas que se usan, sacadas del
// resources.cc original con tools/gen_plaits_lut.py.
'''


def main():
    src = open(sys.argv[1], encoding='utf-8').read()
    cc, h = [LICENCIA, '\n#include "plaits/resources.h"\n\nnamespace plaits {\n'], []
    for nombre in TABLAS:
        m = re.search(r'const float %s\[\] = \{.*?\};\n' % nombre, src, re.S)
        if not m:
            sys.exit('no está ' + nombre)
        cc.append('\n' + m.group(0))
        n = len(re.findall(r'[-+]?\d[\d.e+-]*', m.group(0).split('{', 1)[1]))
        h.append('extern const float %s[];\n#define %s_SIZE %d\n' % (nombre, nombre.upper(), n))
    cc.append('\n}  // namespace plaits\n')
    hdr = (LICENCIA + '\n#ifndef PLAITS_RESOURCES_H_\n#define PLAITS_RESOURCES_H_\n\n#include "stmlib/stmlib.h"\n\n'
           'namespace plaits {\n\n' + ''.join(h) + '\n}  // namespace plaits\n\n#endif  // PLAITS_RESOURCES_H_\n')
    d = os.path.join(ROOT, 'third_party', 'plaits')
    open(os.path.join(d, 'resources.cc'), 'w', encoding='utf-8', newline='\n').write(''.join(cc))
    open(os.path.join(d, 'resources.h'), 'w', encoding='utf-8', newline='\n').write(hdr)
    print('tablas:', ', '.join(TABLAS))


if __name__ == '__main__':
    main()
