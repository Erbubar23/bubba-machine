#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Bubba Machine — build y pruebas (todo corre en Docker; no hace falta compilador local).

  py -3 tools/build.py assets   regenera fuente, sonidos, ondas y logo (Python + numpy + Pillow)
  py -3 tools/build.py test     compila y corre las pruebas en PC (gcc) y guarda capturas
  py -3 tools/build.py web      compila el emulador a WebAssembly (web/bm.js)
  py -3 tools/build.py site     genera la documentación del sitio (web/docs/) desde los .md
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
    if what in ('site', 'all'):
        site()


if __name__ == '__main__':
    main()
