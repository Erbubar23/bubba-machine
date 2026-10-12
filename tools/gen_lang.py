#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Idiomas de la pantalla: junta las frases y arma los paquetes.

  py -3 tools/gen_lang.py            revisa lang/*.txt y arma lang/<código>.bmlang (y web/lang/)
  py -3 tools/gen_lang.py --estricto falla si falta traducir algo

El firmware habla inglés: las frases de pantalla están en el código, marcadas con T("..."),
TF("...") (formato de printf) o N_("...") (dentro de una tabla). Este script las busca en core/
y escribe lang/plantilla.txt con todas, para quien quiera traducir.

Una traducción es un archivo lang/<código>.txt (por ejemplo lang/es.txt):

  # NOMBRE: ESPANOL          (lo que se ve en SISTEMA > LANGUAGE, hasta 11 letras)
  TAKES                      (la frase en inglés, tal cual)
  = TOMAS                    (su traducción)

Reglas que se comprueban: solo ASCII imprimible (la fuente del FM-1 no tiene tildes ni Ñ), los
mismos % en el mismo orden, y frases que siguen existiendo en el código. El paquete (.bmlang)
tiene el formato de core/bm_lang.h.
"""
import argparse, glob, os, re, struct, sys, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LANG = os.path.join(ROOT, 'lang')
WEB = os.path.join(ROOT, 'web', 'lang')
CAP = 16 * 1024                      # BM_LANG_HI - BM_LANG_LO
SPEC = re.compile(r'%%|%[-+ #0]*\d*(?:\.\d+)?(?:hh|h|l|ll|z)?[a-zA-Z]')


def unescape(s):
    return s.encode('latin-1').decode('unicode_escape')


def fnv1a(s):
    h = 2166136261
    for b in s.encode('latin-1'):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def specs(s):
    return [m for m in SPEC.findall(s) if m != '%%']


def literales(txt):
    """(texto, línea, marca) de cada cadena de C fuera de comentarios e #include; marca = T, TF, N_ o ''"""
    i, n, line = 0, len(txt), 1
    while i < n:
        c = txt[i]
        if txt.startswith('//', i):
            j = txt.find('\n', i)
            i = n if j < 0 else j
            continue
        if txt.startswith('/*', i):
            j = txt.find('*/', i) + 2
            line += txt.count('\n', i, j)
            i = j
            continue
        if c in '"\'':
            j = i + 1
            while txt[j] != c:
                j += 2 if txt[j] == '\\' else 1
            ls = txt.rfind('\n', 0, i) + 1
            if c == '"' and not txt[ls:i].lstrip().startswith('#'):
                m = re.search(r'\b(TF|T|N_)\(\s*$', txt[max(0, i - 8):i])
                yield unescape(txt[i + 1:j]), line, m.group(1) if m else ''
            i = j + 1
            continue
        if c == '\n':
            line += 1
        i += 1


def frases():
    """{frase: [archivo:línea, ...]}: lo marcado con T/TF/N_ y las demás cadenas de pantalla (con dos
    mayúsculas seguidas y sin %, que se traducen al dibujarse), menos lang/ignorar.txt"""
    ign = set()
    pi = os.path.join(LANG, 'ignorar.txt')
    if os.path.isfile(pi):
        ign = {l.rstrip('\n') for l in open(pi, encoding='utf-8') if l.strip() and not l.startswith('#')}
    out = {}
    paths = sorted(glob.glob(os.path.join(ROOT, 'core', '*.c')) + glob.glob(os.path.join(ROOT, 'core', '*.cpp'))
                   + [os.path.join(ROOT, 'core', 'gen', 'bm_fm_rom.c')])
    for path in paths:
        for s, line, mark in literales(open(path, encoding='utf-8').read()):
            if s in ign or not re.search(r'[A-Z]{2}', s) or (not mark and '%' in s):
                continue
            out.setdefault(s, []).append('%s:%d' % (os.path.relpath(path, ROOT).replace('\\', '/'), line))
    return out


def leer(path):
    """(nombre, {inglés: traducción}) de un lang/<código>.txt"""
    name, tr, en = None, {}, None
    for n, raw in enumerate(open(path, encoding='utf-8').read().splitlines(), 1):
        line = raw.rstrip('\n')
        if line.startswith('# NOMBRE:'):
            name = line.split(':', 1)[1].strip()
            continue
        if not line.strip() or line.startswith('#'):
            continue
        if line.startswith('= '):
            if en is None:
                sys.exit('%s:%d: traducción sin frase en inglés arriba' % (path, n))
            tr[en] = line[2:]
            en = None
        else:
            en = line
    if not name:
        sys.exit('%s: falta la línea "# NOMBRE: ..."' % path)
    return name, tr


def armar(name, tr):
    items = sorted((fnv1a(en), t) for en, t in tr.items())
    for a, b in zip(items, items[1:]):
        if a[0] == b[0]:
            sys.exit('dos frases con el mismo hash: cambia una (%r)' % b[1])
    textos, offs = bytearray(), []
    for _, t in items:
        offs.append(len(textos))
        textos += t.encode('ascii') + b'\0'
    if len(textos) > 0xFFFF:
        sys.exit('los textos pasan de 64 KiB')
    cuerpo = b''.join(struct.pack('<I', h) for h, _ in items) + b''.join(struct.pack('<H', o) for o in offs) + bytes(textos)
    size = 28 + len(cuerpo)
    nm = name.encode('ascii')[:11].ljust(12, b'\0')
    cab = b'BMLG' + struct.pack('<BBH', 1, 0, len(items)) + nm + struct.pack('<II', size, zlib.crc32(cuerpo) & 0xFFFFFFFF)
    return cab + cuerpo


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--estricto', action='store_true', help='falla si falta traducir algo')
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    fr = frases()
    malas = [s for s in fr if not all(32 <= ord(c) < 127 for c in s)]
    if malas:
        sys.exit('frases con letras que la fuente no tiene: %r' % malas[:5])
    os.makedirs(LANG, exist_ok=True)
    with open(os.path.join(LANG, 'plantilla.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('# Todas las frases de la pantalla (generado por tools/gen_lang.py; no se edita a mano).\n'
                '# Para traducir: copia este archivo a lang/<codigo>.txt, pon "# NOMBRE: ..." arriba y\n'
                '# escribe "= traduccion" debajo de cada frase. Lo que no traduzcas se ve en ingles.\n\n')
        for s in sorted(fr):
            f.write('# %s\n%s\n\n' % (' '.join(sorted({w.split(':')[0] for w in fr[s]})), s))
    falla = 0
    for path in sorted(glob.glob(os.path.join(LANG, '*.txt'))):
        code = os.path.splitext(os.path.basename(path))[0]
        if code in ('plantilla', 'ignorar'):
            continue
        name, tr = leer(path)
        for en, t in list(tr.items()):
            if en not in fr:
                print('%s: ya no está en el código, se ignora: %r' % (code, en))
                del tr[en]
                continue
            if not all(32 <= ord(c) < 127 for c in t):
                print('%s: solo ASCII (sin tildes ni Ñ): %r' % (code, t)); falla = 1
            if specs(en) != specs(t):
                print('%s: los %% no coinciden: %r -> %r' % (code, en, t)); falla = 1
        faltan = [s for s in fr if s not in tr]
        pack = armar(name, tr)
        if len(pack) > CAP:
            print('%s: el paquete ocupa %d bytes; caben %d' % (code, len(pack), CAP)); falla = 1
        for d in (LANG, WEB):
            os.makedirs(d, exist_ok=True)
            open(os.path.join(d, code + '.bmlang'), 'wb').write(pack)
        print('lang/%s.bmlang: %s, %d frases, %d bytes de %d%s' % (code, name, len(tr), len(pack), CAP,
              (', faltan %d' % len(faltan)) if faltan else ''))
        if faltan and a.estricto:
            for s in faltan[:20]:
                print('  falta: %r  (%s)' % (s, fr[s][0]))
            falla = 1
    print('%d frases marcadas en core/ (lang/plantilla.txt)' % len(fr))
    sys.exit(falla)


if __name__ == '__main__':
    main()
