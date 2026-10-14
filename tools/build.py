#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Bubba Machine — build y pruebas (todo corre en Docker; no hace falta compilador local).

  py -3 tools/build.py assets   regenera fuente, sonidos, ondas y logo (Python + numpy + Pillow)
  py -3 tools/build.py test     compila y corre las pruebas en PC (gcc) y guarda capturas
  py -3 tools/build.py web      compila el emulador a WebAssembly (web/bm.js)
  py -3 tools/build.py site     genera la documentación del sitio (web/docs/) desde los .md
  py -3 tools/build.py fm1 [diag|bubba]
                                firmware para el FM-1 sobre el SDK de JieLi, empaquetado sobre el V15
                                oficial: build/fm1/bubba-<app>-<versión>.fwsc. Necesita el toolchain en
                                el volumen de Docker "jieli-opt", el SDK AC79 y el FM-1.fwsc de M-VAVE
                                (ver hal/fm1/README_PORT.md).
  py -3 tools/build.py all      todo lo anterior
"""
import glob, hashlib, json, os, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GCC_IMAGE = 'python:3.12-bookworm'
EMSDK_IMAGE = 'emscripten/emsdk:3.1.74'
CFLAGS = '-std=c99 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation'
SRC = 'core/*.c core/gen/*.c hal/bm_hal_sim.c'
# C++: los motores de terceros (third_party/) y su envoltorio. Sin excepciones ni RTTI, como en el FM-1.
MSFA = ['dx7note', 'env', 'exp2', 'fm_core', 'fm_op_kernel', 'freqlut', 'lfo', 'pitchenv', 'porta', 'sin', 'EngineMsfa']
# Plaits y stmlib (Mutable Instruments, MIT): batería modelada y el núcleo WAVE
PLAITS = ['third_party/plaits/resources.cc', 'third_party/stmlib/dsp/units.cc', 'third_party/stmlib/utils/random.cc']
CXX_SRC = ('core/bm_fm.cpp core/bm_drums.cpp core/bm_wave.cpp ' + ' '.join('third_party/msfa/%s.cpp' % f for f in MSFA)
           + ' ' + ' '.join(PLAITS))
CXXFLAGS = ('-std=c++14 -O2 -fno-exceptions -fno-rtti -Ithird_party/msfa -Ithird_party -Icore -w '
            '-DPLAITS_SAMPLE_RATE=44118.0f -DTEST')   # TEST: stmlib sin ensamblador de ARM


def cxx_objs(cxx, out):
    """compila el C++ a out/*.o (comando de shell para el contenedor)"""
    return (f'mkdir -p {out} && for f in {CXX_SRC}; do {cxx} {CXXFLAGS} -c $f -o {out}/$(basename ${{f%.*}}).o || exit 1; done && ')


def docker(image, cmd):
    vol = ROOT.replace('\\', '/') + ':/src'
    env = dict(os.environ, MSYS_NO_PATHCONV='1')
    r = subprocess.run(['docker', 'run', '--rm', '-v', vol, '-w', '/src', image, 'sh', '-c', cmd], env=env)
    if r.returncode:
        sys.exit(r.returncode)


def assets():
    for t in ('gen_font.py', 'gen_logo.py'):
        r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', t)])
        if r.returncode:
            sys.exit(r.returncode)


def lang():
    r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'gen_lang.py')])
    if r.returncode:
        sys.exit(r.returncode)


def test():
    lang()
    os.makedirs(os.path.join(ROOT, 'build', 'out'), exist_ok=True)
    docker(GCC_IMAGE,
           cxx_objs('g++', 'build/obj') +
           f'gcc {CFLAGS} {SRC} tests/test_core.c build/obj/*.o -lstdc++ -lm -o build/test_core && ./build/test_core build/out && '
           f'gcc {CFLAGS} hal/fm1/fm1_scan.c tests/test_scan.c -o build/test_scan && ./build/test_scan')
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
    lang()
    docker(EMSDK_IMAGE,
           cxx_objs('em++', 'build/wobj') +
           f'emcc -O2 -std=c99 -Wno-format-truncation {SRC} hal/web/hal_web.c build/wobj/*.o -o web/bm.js '
           '-sMODULARIZE=1 -sEXPORT_NAME=BubbaMachine -sENVIRONMENT=web -sSINGLE_FILE=1 '
           '-sINITIAL_MEMORY=33554432 -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPU16,HEAP8,HEAPF32')
    print('emulador listo: abre web/emulador.html')


JIELI_VOLUME = 'jieli-opt'
AC79_SDK = os.environ.get('AC79_SDK', os.path.join(os.path.expanduser('~'), 'jieli', 'fw-AC79_AIoT_SDK'))
FM1_V15 = os.environ.get('FM1_V15', os.path.join(os.path.expanduser('~'), 'jieli', 'stock', 'FM-1.fwsc'))


def fm1(app):
    """compila el firmware del FM-1 sobre el SDK de JieLi y lo empaqueta sobre el V15 oficial"""
    if app not in ('diag', 'bubba'):
        sys.exit('fm1: elige diag o bubba')
    r = subprocess.run(['docker', 'volume', 'inspect', JIELI_VOLUME], capture_output=True)
    if r.returncode:
        sys.exit('falta el toolchain de JieLi en el volumen de Docker "%s" (ver hal/fm1/README_PORT.md)' % JIELI_VOLUME)
    if not os.path.isfile(os.path.join(AC79_SDK, 'cpu', 'wl82', 'sdk_ld.c')):
        sys.exit('falta el SDK AC79 de JieLi en %s (o la variable AC79_SDK)' % AC79_SDK)
    if not os.path.isfile(FM1_V15):
        sys.exit('falta el FM-1 V15 de M-VAVE en %s (o la variable FM1_V15); se descarga de m-vave.com' % FM1_V15)
    out = os.path.join(ROOT, 'build', 'fm1')
    os.makedirs(out, exist_ok=True)
    env = dict(os.environ, MSYS_NO_PATHCONV='1')
    cmd = ('ulimit -n 8096; make -s -C /src/hal/fm1/sdk SDK=/sdk OUT=/tmp/fm1 APP=%s && '
           'cp /tmp/fm1/app.bin /src/build/fm1/app-%s.bin && cp /tmp/fm1/sections.txt /src/build/fm1/sections-%s.txt'
           % (app, app, app))
    r = subprocess.run(['docker', 'run', '--rm', '-v', ROOT.replace('\\', '/') + ':/src',
                        '-v', AC79_SDK.replace('\\', '/') + ':/sdk:ro', '-v', JIELI_VOLUME + ':/opt/jieli:ro',
                        GCC_IMAGE, 'bash', '-c', cmd], env=env)
    if r.returncode:
        sys.exit(r.returncode)
    base = 'bubba-%s-%s' % ('diag' if app == 'diag' else 'machine', version())
    pkg = os.path.join(out, base + '.fwsc')
    r = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'fm1', 'fwsc.py'), 'armar', FM1_V15,
                        os.path.join(out, 'app-%s.bin' % app), pkg, '--vm', '0xE5000:0x8000'])
    if r.returncode:
        sys.exit(r.returncode)
    # El .fwsc armado va a la release de GitHub (lo pide el FM-1 Firmware Hub) y desde 0.6 también al
    # sitio: el instalador web lo instala tal cual, sin pedir el V15 (plug and play). El app.bin queda
    # para versiones viejas y para armar el paquete en el navegador si alguien trae su propio V15.
    pub = os.path.join(ROOT, 'web', 'fm1')
    os.makedirs(pub, exist_ok=True)
    nombre = base + '.bin'
    with open(os.path.join(out, 'app-%s.bin' % app), 'rb') as f:
        data = f.read()
    with open(os.path.join(pub, nombre), 'wb') as f:
        f.write(data)
    with open(pkg, 'rb') as f, open(os.path.join(pub, base + '.fwsc'), 'wb') as g:
        g.write(f.read())
    manifiesto(pub)
    print('para el instalador web: web/fm1/%s (+ versiones.json)' % nombre)


def manifiesto(pub):
    """web/fm1/versiones.json: lo que ofrece el instalador web, según los archivos que hay en web/fm1/."""
    lista = []
    for n in sorted(os.listdir(pub)):
        if n.endswith('.bin'):
            d = open(os.path.join(pub, n), 'rb').read()
            diag = '-diag-' in n
            v = n[len('bubba-diag-' if diag else 'bubba-machine-'):-4]
            fw = n[:-4] + '.fwsc'
            seis = [int(k) for k in v.split('-')[0].split('.') if k.isdigit()] >= [0, 6]   # desde 0.6: máquinas
            extra = {}
            if os.path.isfile(os.path.join(pub, fw)):
                fd = open(os.path.join(pub, fw), 'rb').read()
                extra = {'fwsc': fw, 'fwsc_sha256': hashlib.sha256(fd).hexdigest()}
            lista.append({'archivo': n, 'sha256': hashlib.sha256(d).hexdigest(), 'bytes': len(d), **extra,
                          'nombre': ('Diagnóstico %s' if diag else 'Bubba Machine %s') % v,
                          'nombre_en': ('Diagnostic %s' if diag else 'Bubba Machine %s') % v,
                          'detalle': 'Sin sonido: muestra cada botón, tecla y perilla para comprobar el aparato.' if diag else
                          ('El groovebox completo: %s, secuenciador, sampler y sonido. Beta: una actualización puede '
                           'borrar lo guardado.' % ('seis máquinas' if seis else '6 canales')),
                          'detalle_en': 'No sound: shows every button, key and knob to check the device.' if diag else
                          ('The full groovebox: %s, sequencer, sampler and sound. Beta: an update may erase what '
                           'you saved.' % ('six machines' if seis else '6 channels'))})
    def ver(x):                                     # la más nueva primero: 0.10 va después de 0.9
        nums = [int(p) for p in x['archivo'].split('-')[2].split('.') if p.isdigit()]
        return nums
    lista.sort(key=lambda x: ('-diag-' in x['archivo'], [-k for k in ver(x)]))
    with open(os.path.join(pub, 'versiones.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(lista, f, ensure_ascii=False, indent=1)
        f.write('\n')


def version():
    for line in open(os.path.join(ROOT, 'core', 'bm_config.h'), encoding='utf-8'):
        if line.startswith('#define BM_VERSION_STR'):
            return line.split('"')[1]
    return '0'


def site():
    manifiesto(os.path.join(ROOT, 'web', 'fm1'))
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
        fm1(sys.argv[2] if len(sys.argv) > 2 else 'diag')
    if what in ('site', 'all'):
        site()


if __name__ == '__main__':
    main()
