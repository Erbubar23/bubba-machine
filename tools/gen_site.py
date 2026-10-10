#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Genera las páginas de documentación del sitio (web/docs/*.html) a partir de los .md.

Edita los .md: el HTML se regenera. También copia a web/img/ las capturas y el logo que usa
el sitio. Sin dependencias: trae un convertidor de Markdown pequeño, suficiente para nuestros
documentos (títulos, párrafos, listas, tablas, código, citas, enlaces, negritas).

Uso: py -3 tools/gen_site.py
"""
import html, os, re, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, 'web')
REPO = 'https://github.com/Erbubar23/bubba-machine/blob/main/'
VERSION = '0.2.0'

# (archivo de salida, título del menú, fuente, grupo)
PAGES = [
    ('manual', 'Manual', 'docs/MANUAL.md', 'Usar'),
    ('instalar', 'Instalar', 'docs/INSTALAR.md', 'Usar'),
    ('cambios', 'Cambios', 'CHANGELOG.md', 'Usar'),
    ('diseno-3', 'Diseño 3.0', 'docs/DISENO_3.0.md', 'Proyecto'),
    ('hoja-de-ruta', 'Hoja de ruta', 'docs/HOJA_DE_RUTA.md', 'Proyecto'),
    ('contribuir', 'Contribuir', 'CONTRIBUTING.md', 'Proyecto'),
    ('conducta', 'Código de conducta', 'CODE_OF_CONDUCT.md', 'Proyecto'),
    ('creditos', 'Créditos', 'CREDITOS.md', 'Proyecto'),
    ('desarrollo', 'Desarrollo', 'docs/DESARROLLO.md', 'Técnico'),
    ('spec', 'Especificación', 'docs/SPEC_V3.md', 'Técnico'),
    ('memoria', 'Memoria', 'docs/MEMORIA.md', 'Técnico'),
    ('diseno-0-2', 'Diseño 0.2', 'docs/DISENO_0.2.md', 'Técnico'),
]
IMAGES = ['00-home', '02-seq-tr', '05-time', '17-chords', '30-sampler-fab']

BY_SRC = {os.path.normpath(src): slug for slug, _, src, _ in PAGES}


# ------------------------------------------------------------ enlaces
def resolve_link(url, src):
    if re.match(r'^[a-z]+:', url) or url.startswith('#'):
        return url
    path, _, frag = url.partition('#')
    full = os.path.normpath(os.path.join(os.path.dirname(src), path))
    if full in BY_SRC:
        return BY_SRC[full] + '.html' + ('#' + frag if frag else '')
    return REPO + full.replace(os.sep, '/') + ('#' + frag if frag else '')


# ------------------------------------------------------------ Markdown en línea
def inline(text, src, refs):
    codes = []

    def keep_code(m):
        codes.append('<code>%s</code>' % html.escape(m.group(1)))
        return '\x00%d\x00' % (len(codes) - 1)
    text = re.sub(r'`([^`]+)`', keep_code, text)
    text = html.escape(text, quote=False)

    def link(m):
        label, url = m.group(1), m.group(2)
        return '<a href="%s">%s</a>' % (html.escape(resolve_link(url, src)), label)
    text = re.sub(r'!\[([^\]]*)\]\(([^)]+)\)', '', text)
    text = re.sub(r'\[([^\]]+)\]\(([^)\s]+)\)', link, text)

    def ref(m):
        key = m.group(1)
        return '<a href="%s">%s</a>' % (html.escape(refs[key]), key) if key in refs else m.group(0)
    text = re.sub(r'\[([^\]]+)\](?!\()', ref, text)
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    text = re.sub(r'(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])', r'<em>\1</em>', text)
    text = re.sub(r'~~(.+?)~~', r'<del>\1</del>', text)
    text = text.replace('[ ]', '&#9744;').replace('[x]', '&#9745;')
    return re.sub('\x00(\\d+)\x00', lambda m: codes[int(m.group(1))], text)


def slugify(text):
    s = re.sub(r'<[^>]+>', '', text).lower()
    s = re.sub(r'[^\w\s-]', '', s)
    return re.sub(r'\s+', '-', s.strip())


# ------------------------------------------------------------ Markdown en bloques
def markdown(md, src):
    refs = {}
    lines = []
    for ln in md.splitlines():
        m = re.match(r'^\[([^\]]+)\]:\s*(\S+)\s*$', ln)
        if m:
            refs[m.group(1)] = m.group(2)
        else:
            lines.append(ln.rstrip())
    out, i, title = [], 0, None
    lists = []                       # pila de ('ul'|'ol', sangría)

    def close_lists(to=0):
        while len(lists) > to:
            out.append('</li></%s>' % lists.pop()[0])

    while i < len(lines):
        ln = lines[i]
        s = ln.strip()
        if s.startswith('```'):
            close_lists()
            code = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                code.append(lines[i])
                i += 1
            ind = min((len(c) - len(c.lstrip()) for c in code if c.strip()), default=0)
            out.append('<pre><code>%s</code></pre>' % html.escape('\n'.join(c[ind:] for c in code)))
            i += 1
            continue
        if not s:
            i += 1
            continue
        m = re.match(r'^(#{1,4})\s+(.*)$', s)
        if m:
            close_lists()
            lvl, txt = len(m.group(1)), inline(m.group(2), src, refs)
            if lvl == 1 and title is None:
                title = re.sub(r'<[^>]+>', '', txt)
            out.append('<h%d id="%s">%s</h%d>' % (lvl, slugify(txt), txt, lvl))
            i += 1
            continue
        if re.match(r'^-{3,}$', s):
            close_lists()
            out.append('<hr>')
            i += 1
            continue
        if s.startswith('|'):
            close_lists()
            rows = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                rows.append([c.strip() for c in lines[i].strip().strip('|').split('|')])
                i += 1
            head, body = rows[0], [r for r in rows[1:] if not all(re.match(r'^:?-+:?$', c) for c in r if c)]
            t = ['<div class="tbl"><table><thead><tr>']
            t += ['<th>%s</th>' % inline(c, src, refs) for c in head]
            t.append('</tr></thead><tbody>')
            for r in body:
                t.append('<tr>' + ''.join('<td>%s</td>' % inline(c, src, refs) for c in r) + '</tr>')
            t.append('</tbody></table></div>')
            out.append(''.join(t))
            continue
        if s.startswith('>'):
            close_lists()
            q = []
            while i < len(lines) and lines[i].strip().startswith('>'):
                q.append(lines[i].strip()[1:].strip())
                i += 1
            out.append('<blockquote><p>%s</p></blockquote>' % inline(' '.join(q), src, refs))
            continue
        m = re.match(r'^(\s*)([-*]|\d+\.)\s+(.*)$', ln)
        if m:
            ind, kind, txt = len(m.group(1)), ('ol' if m.group(2)[0].isdigit() else 'ul'), m.group(3)
            while lists and ind < lists[-1][1]:
                out.append('</li></%s>' % lists.pop()[0])
            if lists and ind == lists[-1][1] and kind != lists[-1][0]:
                out.append('</li></%s>' % lists.pop()[0])
            if not lists or ind > lists[-1][1]:
                lists.append((kind, ind))
                out.append('<%s><li>' % kind)
            else:
                out.append('</li><li>')
            i += 1
            while i < len(lines) and lines[i].strip() and not re.match(r'^\s*([-*]|\d+\.)\s+', lines[i]) \
                    and not lines[i].strip().startswith(('```', '|', '#', '>')) and (len(lines[i]) - len(lines[i].lstrip())) > ind:
                txt += ' ' + lines[i].strip()
                i += 1
            out.append(inline(txt, src, refs))
            continue
        close_lists()
        para = [s]
        i += 1
        while i < len(lines) and lines[i].strip() and not re.match(r'^(#{1,4}\s|```|\||>|\s*([-*]|\d+\.)\s|-{3,}$)', lines[i].strip()):
            para.append(lines[i].strip())
            i += 1
        out.append('<p>%s</p>' % inline(' '.join(para), src, refs))
    close_lists()
    return title, '\n'.join(out)


# ------------------------------------------------------------ página
def page(slug, title, body, src):
    groups, nav = [], []
    for s, label, _, grp in PAGES:
        if grp not in groups:
            groups.append(grp)
            nav.append('<div class="grp">%s</div>' % grp)
        cur = ' aria-current="page"' if s == slug else ''
        nav.append('<a href="%s.html"%s>%s</a>' % (s, cur, label))
    return '''<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-or-later · GENERADO por tools/gen_site.py desde %(src)s: no editar a mano. -->
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%(title)s · Bubba Machine</title>
<link rel="icon" href="../icon.png">
<link rel="stylesheet" href="../site.css">
</head>
<body>
<header class="top">
  <div class="in">
    <a class="brand" href="../index.html"><img src="../img/icon.svg" alt="">Bubba Machine</a>
    <nav aria-label="Sitio">
      <a href="../emulador.html">Emulador</a>
      <a href="manual.html"%(m)s>Manual</a>
      <a href="instalar.html"%(i)s>Instalar</a>
      <a href="hoja-de-ruta.html"%(h)s>Hoja de ruta</a>
      <a href="contribuir.html"%(c)s>Contribuir</a>
      <a href="https://github.com/Erbubar23/bubba-machine">Código</a>
    </nav>
  </div>
</header>
<div class="docs">
  <aside aria-label="Documentación">%(nav)s</aside>
  <main class="doc">
%(body)s
    <p class="src">Esta página se genera desde <a href="%(srcurl)s">%(src)s</a>. ¿Ves un error? Edita el archivo y manda un pull request.</p>
  </main>
</div>
<footer>
  <div class="in">
    <p>Bubba Machine %(ver)s · GPL-3.0-or-later · M-VAVE y FM-1 son marcas de sus respectivos dueños; este proyecto no está afiliado a M-VAVE.</p>
  </div>
</footer>
</body>
</html>
''' % {'title': html.escape(title), 'nav': '\n    '.join(nav), 'body': body, 'src': src, 'srcurl': REPO + src,
       'ver': VERSION, 'm': cur_attr(slug, 'manual'), 'i': cur_attr(slug, 'instalar'),
       'h': cur_attr(slug, 'hoja-de-ruta'), 'c': cur_attr(slug, 'contribuir')}


def cur_attr(slug, name):
    return ' aria-current="page"' if slug == name else ''


def main():
    os.makedirs(os.path.join(WEB, 'docs'), exist_ok=True)
    os.makedirs(os.path.join(WEB, 'img'), exist_ok=True)
    for slug, label, src, _ in PAGES:
        with open(os.path.join(ROOT, src), encoding='utf-8') as f:
            title, body = markdown(f.read(), src)
        with open(os.path.join(WEB, 'docs', slug + '.html'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(page(slug, title or label, body, src))
        print('web/docs/%s.html  <-  %s' % (slug, src))
    for name in IMAGES:
        shutil.copyfile(os.path.join(ROOT, 'assets', 'screens', name + '.png'), os.path.join(WEB, 'img', name + '.png'))
    shutil.copyfile(os.path.join(ROOT, 'assets', 'logo', 'bubba-machine-logo.png'), os.path.join(WEB, 'img', 'logo.png'))
    shutil.copyfile(os.path.join(ROOT, 'assets', 'logo', 'bubba-machine-icon.svg'), os.path.join(WEB, 'img', 'icon.svg'))
    print('imágenes en web/img/')


if __name__ == '__main__':
    main()
