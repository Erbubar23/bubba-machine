#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Bubba Machine — build y pruebas (todo corre en Docker; no hace falta compilador local).

  py -3 tools/build.py assets   regenera fuente, sonidos, ondas y logo (Python + numpy + Pillow)
  py -3 tools/build.py test     compila y corre las pruebas en PC (gcc) y guarda capturas
  py -3 tools/build.py web      compila el emulador a WebAssembly (web/bm.js)
  py -3 tools/build.py site     genera la documentación del sitio (web/docs/) desde los .md
  py -3 tools/build.py fm1      compila el núcleo y el port con el compilador de JieLi (pi32v2) y mide
                                cuánto ocupa. Necesita el toolchain de JieLi en el volumen de Docker
                                "jieli-opt" (ver hal/fm1/README_PORT.md). No genera nada instalable.
  py -3 tools/build.py all      todo lo anterior
"""
import glob, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GCC_IMAGE = 'python:3.12-bookworm'
EMSDK_IMAGE = 'emscripten/emsdk:3.1.74'
CFLAGS = '-std=c99 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation'
SRC = 'core/*.c core/gen/*.c hal/bm_hal_sim.c'


def docker(image, cmd):
    vol = ROOT.replace('\\', '/') + ':/src'
    env = dict(os.environ, MSYS_NO_PATHCONV='1')
    r = subprocess.run(['docker', 'run', '--rm', '-v', vol, '-w', '/src', image, 'sh', '-c', cmd], env=env)
    if r.returncode:
        sys.exit(r.returncode)


def assets():
    for t in ('gen_font.py', 'gen_assets.py', 'gen_loops.py', 'gen_logo.py'):
        r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', t)])
        if r.returncode:
            sys.exit(r.returncode)


def test():
    os.makedirs(os.path.join(ROOT, 'build', 'out'), exist_ok=True)
    docker(GCC_IMAGE,
           f'gcc {CFLAGS} {SRC} tests/test_core.c -lm -o build/test_core && ./build/test_core build/out && '
           f'gcc {CFLAGS} hal/fm1/fm1_scan.c tests/test_scan.c -o build/test_scan && ./build/test_scan && '
           f'gcc {CFLAGS} -fsyntax-only hal/fm1/fm1_main.c')
    try:
        from PIL import Image
        os.makedirs(os.path.join(ROOT, 'assets', 'screens'), exist_ok=True)
        for f in sorted(glob.glob(os.path.join(ROOT, 'build', 'out', '*.ppm'))):
            name = os.path.splitext(os.path.basename(f))[0]
            Image.open(f).save(os.path.join(ROOT, 'assets', 'screens', name + '.png'))
        print('capturas en assets/screens/')
    except ImportError:
        print('(sin Pillow: las capturas quedan en build/out/*.ppm)')


def web():
    docker(EMSDK_IMAGE,
           f'emcc -O2 -std=c99 -Wno-format-truncation {SRC} hal/web/hal_web.c -o web/bm.js '
           '-sMODULARIZE=1 -sEXPORT_NAME=BubbaMachine -sENVIRONMENT=web -sSINGLE_FILE=1 '
           '-sINITIAL_MEMORY=33554432 -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU16,HEAP8,HEAPF32')
    print('emulador listo: abre web/emulador.html')


JIELI_VOLUME = 'jieli-opt'
JIELI_FLAGS = ('-target pi32v2 -integrated-as -mcpu=r3 -mfprev1 -fno-common -Os -fallow-pointer-null '
               '-fprefer-gnu-section -mllvm -pi32v2-large-program=true -ffunction-sections -fdata-sections '
               '-std=c99 -Wall -Wno-unused-parameter -I/opt/jieli/pi32v2/include')

SIZE_SCRIPT = r"""
import re, collections, sys
tot = collections.Counter(); per = {}; cur = None
for line in open(sys.argv[1]):
    m = re.match(r'/tmp/o/(\S+)\.o:', line)
    if m: cur = m.group(1); per[cur] = collections.Counter(); continue
    p = line.split()
    if len(p) >= 3 and p[0].isdigit() and p[1].startswith('.'):
        n, s = p[1], int(p[2], 16)
        k = 'bss' if n.startswith('.bss') else 'rodata' if n.startswith('.rodata') else 'data' if n.startswith('.data') else 'text' if n.startswith('.text') else None
        if k: per[cur][k] += s; tot[k] += s
for f, c in sorted(per.items()):
    print('  %-12s text %7d  rodata %7d  data %6d  bss %6d' % (f, c['text'], c['rodata'], c['data'], c['bss']))
print('  TOTAL        text %7d  rodata %7d  data %6d  bss %6d' % (tot['text'], tot['rodata'], tot['data'], tot['bss']))
print('  flash (text + rodata + data): %d KB de ~604 KiB para el firmware' % ((tot['text'] + tot['rodata'] + tot['data']) // 1024))
"""


def fm1():
    r = subprocess.run(['docker', 'volume', 'inspect', JIELI_VOLUME], capture_output=True)
    if r.returncode:
        sys.exit('falta el toolchain de JieLi en el volumen de Docker "%s" (ver hal/fm1/README_PORT.md)' % JIELI_VOLUME)
    vol = ROOT.replace('\\', '/') + ':/src'
    env = dict(os.environ, MSYS_NO_PATHCONV='1')
    cmd = ('mkdir -p /tmp/o && for f in core/*.c core/gen/*.c hal/fm1/fm1_scan.c hal/fm1/fm1_main.c; do '
           '/opt/jieli/pi32v2/bin/clang %s -c $f -o /tmp/o/$(basename $f .c).o || exit 1; done && '
           'for o in /tmp/o/*.o; do /opt/jieli/pi32v2/bin/objdump -section-headers $o; done > /tmp/h.txt && '
           'python3 -c "$SIZE_SCRIPT" /tmp/h.txt' % JIELI_FLAGS)
    env['SIZE_SCRIPT'] = SIZE_SCRIPT
    r = subprocess.run(['docker', 'run', '--rm', '-e', 'SIZE_SCRIPT', '-v', vol, '-v', JIELI_VOLUME + ':/opt/jieli:ro',
                        '-w', '/src', GCC_IMAGE, 'sh', '-c', cmd], env=env)
    if r.returncode:
        sys.exit(r.returncode)
    print('compilado para el FM-1 (pi32v2). Solo mide: todavía no hay firmware instalable.')


def site():
    r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'gen_site.py')])
    if r.returncode:
        sys.exit(r.returncode)


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if what in ('assets', 'all'):
        assets()
    if what in ('test', 'all'):
        test()
    if what in ('web', 'all'):
        web()
    if what == 'fm1':
        fm1()
    if what in ('site', 'all'):
        site()


if __name__ == '__main__':
    main()
