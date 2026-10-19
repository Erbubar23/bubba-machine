#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Genera la documentación del sitio en español (web/docs/) e inglés (web/en/docs/) desde los .md.

Edita los .md: el HTML se regenera. Las fuentes en inglés viven en docs/en/. Las páginas
técnicas que solo existen en español aparecen en el menú en inglés marcadas con (ES).
También copia a web/img/ las capturas y el logo que usa el sitio. Sin dependencias: trae un
convertidor de Markdown pequeño, suficiente para nuestros documentos.

Uso: py -3 tools/gen_site.py
"""
import html, os, re, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, 'web')
REPO = 'https://github.com/Erbubar23/bubba-machine/blob/main/'
SITE = 'https://erbubar23.github.io/bubba-machine/'
VERSION = '0.6.1-beta'

LANGS = {'es': 'docs', 'en': 'en/docs'}          # carpeta de salida dentro de web/

# clave, grupo, {idioma: (archivo, título del menú, fuente)}
PAGES = [
    ('manual', 'use', {'es': ('manual', 'Manual', 'docs/MANUAL.md'),
                       'en': ('manual', 'Manual', 'docs/en/MANUAL.md')}),
    ('install', 'use', {'es': ('instalar', 'Instalar', 'docs/INSTALAR.md'),
                        'en': ('install', 'Install', 'docs/en/INSTALL.md')}),
    ('changes', 'use', {'es': ('cambios', 'Cambios', 'CHANGELOG.md'),
                        'en': ('changelog', 'Changelog', 'docs/en/CHANGELOG.md')}),
    ('design3', 'project', {'es': ('diseno-3', 'Diseño 3.0', 'docs/DISENO_3.0.md'),
                            'en': ('design-3', 'Design 3.0', 'docs/en/DESIGN_3.0.md')}),
    ('design06', 'project', {'es': ('diseno-0-6', 'Diseño 0.6', 'docs/DISENO_0.6.md')}),
    ('roadmap', 'project', {'es': ('hoja-de-ruta', 'Hoja de ruta', 'docs/HOJA_DE_RUTA.md'),
                            'en': ('roadmap', 'Roadmap', 'docs/en/ROADMAP.md')}),
    ('contrib', 'project', {'es': ('contribuir', 'Contribuir', 'CONTRIBUTING.md'),
                            'en': ('contributing', 'Contributing', 'docs/en/CONTRIBUTING.md')}),
    ('conduct', 'project', {'es': ('conducta', 'Código de conducta', 'CODE_OF_CONDUCT.md'),
                            'en': ('conduct', 'Code of conduct', 'docs/en/CODE_OF_CONDUCT.md')}),
    ('credits', 'project', {'es': ('creditos', 'Créditos', 'CREDITOS.md'),
                            'en': ('credits', 'Credits', 'docs/en/CREDITS.md')}),
    ('audio', 'tech', {'es': ('audio', 'Audio', 'docs/AUDIO.md'),
                       'en': ('audio', 'Audio', 'docs/en/AUDIO.md')}),
    ('dev', 'tech', {'es': ('desarrollo', 'Desarrollo', 'docs/DESARROLLO.md'),
                     'en': ('development', 'Development', 'docs/en/DEVELOPMENT.md')}),
    ('spec', 'tech', {'es': ('spec', 'Especificación', 'docs/SPEC_V3.md')}),
    ('memory', 'tech', {'es': ('memoria', 'Memoria', 'docs/MEMORIA.md')}),
    ('design02', 'tech', {'es': ('diseno-0-2', 'Diseño 0.2', 'docs/DISENO_0.2.md')}),
]
ES_ONLY_EN = {'design06': 'Design 0.6 (ES)', 'spec': 'Specification (ES)', 'memory': 'Memory budget (ES)', 'design02': 'Design 0.2 (ES)'}

TXT = {
    'es': {
        'groups': {'use': 'Usar', 'project': 'Proyecto', 'tech': 'Técnico'},
        'nav': [('emu', 'Emulador'), ('manual', 'Manual'), ('install', 'Instalar'), ('control', 'Control'), ('roadmap', 'Hoja de ruta'), ('contrib', 'Contribuir')],
        'code': 'Código', 'site': 'Sitio', 'docs': 'Documentación', 'other': ('en', 'English'),
        'src': 'Esta página se genera desde <a href="%s">%s</a>. ¿Ves un error? Edita el archivo y manda un pull request.',
        'foot': 'Bubba Machine %s · GPL-3.0-or-later · M-VAVE y FM-1 son marcas de sus respectivos dueños; este proyecto no está afiliado a M-VAVE.',
    },
    'en': {
        'groups': {'use': 'Use', 'project': 'Project', 'tech': 'Technical'},
        'nav': [('emu', 'Emulator'), ('manual', 'Manual'), ('install', 'Install'), ('control', 'Control'), ('roadmap', 'Roadmap'), ('contrib', 'Contribute')],
        'code': 'Code', 'site': 'Site', 'docs': 'Documentation', 'other': ('es', 'Español'),
        'src': 'This page is generated from <a href="%s">%s</a>. Spotted a mistake? Edit the file and send a pull request.',
        'foot': 'Bubba Machine %s · GPL-3.0-or-later · M-VAVE and FM-1 are trademarks of their respective owners; this project is not affiliated with M-VAVE.',
    },
}
IMAGES = ['00-home', '02-seq-tr', '37-pads', '52-seq-long-note', '56b-fm-alg5', '57-bittal-table', '60-buno-dco', '51-rec-gate-micro']

BY_SRC = {os.path.normpath(p[2]): (lang, p[0]) for _, _, d in PAGES for lang, p in d.items()}


def out_file(lang, slug):
    return os.path.join(WEB, LANGS[lang], slug + '.html')


def rel(target, cur_dir):
    return os.path.relpath(target, cur_dir).replace(os.sep, '/')


# ------------------------------------------------------------ enlaces
def resolve_link(url, src, cur_dir):
    if re.match(r'^[a-z]+:', url) or url.startswith('#'):
        return url
    path, _, frag = url.partition('#')
    full = os.path.normpath(os.path.join(os.path.dirname(src), path))
    tail = '#' + frag if frag else ''
    if full in BY_SRC:
        return rel(out_file(*BY_SRC[full]), cur_dir) + tail
    if full.split(os.sep)[0] == 'web' and full.endswith('.html'):    # una página del sitio (control.html…)
        en = os.path.normpath(cur_dir).startswith(os.path.join(WEB, 'en'))
        return rel(os.path.join(ROOT, full), cur_dir) + ('?lang=en' if en else '') + tail
    return REPO + full.replace(os.sep, '/') + tail


# ------------------------------------------------------------ Markdown en línea
def inline(text, ctx):
    src, cur_dir, refs = ctx
    codes = []

    def keep_code(m):
        codes.append('<code>%s</code>' % html.escape(m.group(1)))
        return '\x00%d\x00' % (len(codes) - 1)
    text = re.sub(r'`([^`]+)`', keep_code, text)
    text = html.escape(text, quote=False)

    def link(m):
        return '<a href="%s">%s</a>' % (html.escape(resolve_link(m.group(2), src, cur_dir)), m.group(1))
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
def markdown(md, src, cur_dir):
    refs, lines = {}, []
    for ln in md.splitlines():
        m = re.match(r'^\[([^\]]+)\]:\s*(\S+)\s*$', ln)
        if m:
            refs[m.group(1)] = m.group(2)
        else:
            lines.append(ln.rstrip())
    ctx = (src, cur_dir, refs)
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
            lvl, txt = len(m.group(1)), inline(m.group(2), ctx)
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
            t += ['<th>%s</th>' % inline(c, ctx) for c in head]
            t.append('</tr></thead><tbody>')
            for r in body:
                t.append('<tr>' + ''.join('<td>%s</td>' % inline(c, ctx) for c in r) + '</tr>')
            t.append('</tbody></table></div>')
            out.append(''.join(t))
            continue
        if s.startswith('>'):
            close_lists()
            q = []
            while i < len(lines) and lines[i].strip().startswith('>'):
                q.append(lines[i].strip()[1:].strip())
                i += 1
            out.append('<blockquote><p>%s</p></blockquote>' % inline(' '.join(q), ctx))
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
            out.append(inline(txt, ctx))
            continue
        close_lists()
        para = [s]
        i += 1
        while i < len(lines) and lines[i].strip() and not re.match(r'^(#{1,4}\s|```|\||>|\s*([-*]|\d+\.)\s|-{3,}$)', lines[i].strip()):
            para.append(lines[i].strip())
            i += 1
        out.append('<p>%s</p>' % inline(' '.join(para), ctx))
    close_lists()
    return title, '\n'.join(out)


# ------------------------------------------------------------ página
def page(lang, key, title, body, src):
    T = TXT[lang]
    cur_dir = os.path.join(WEB, LANGS[lang])
    up = rel(WEB, cur_dir)                                      # '..' o '../..'
    by_key = {k: d for k, _, d in PAGES}

    def href(k):
        d = by_key[k]
        return rel(out_file(lang, d[lang][0]), cur_dir) if lang in d else rel(out_file('es', d['es'][0]), cur_dir)

    def cur(k):
        return ' aria-current="page"' if k == key else ''

    # menú lateral
    side, seen = [], []
    for k, grp, d in PAGES:
        if lang not in d and lang != 'en':
            continue
        if grp not in seen:
            seen.append(grp)
            side.append('<div class="grp">%s</div>' % T['groups'][grp])
        label = d[lang][1] if lang in d else ES_ONLY_EN[k]
        side.append('<a href="%s"%s>%s</a>' % (href(k), cur(k), html.escape(label)))

    # barra superior
    nav = []
    for k, label in T['nav']:
        if k == 'emu':
            nav.append('<a href="%s/emulador.html?lang=%s">%s</a>' % (up, lang, label))
        elif k == 'control':
            nav.append('<a href="%s/control.html?lang=%s">%s</a>' % (up, lang, label))
        else:
            nav.append('<a href="%s"%s>%s</a>' % (href(k), cur(k), label))
    nav.append('<a href="https://github.com/Erbubar23/bubba-machine">%s</a>' % T['code'])

    # el otro idioma: la misma página si existe; si no, su portada
    olang, olabel = T['other']
    d = by_key[key]
    if olang in d:
        other = rel(out_file(olang, d[olang][0]), cur_dir)
    else:
        other = rel(os.path.join(WEB, 'en' if olang == 'en' else '', 'index.html'), cur_dir)
    nav.append('<a class="lang" href="%s" hreflang="%s" lang="%s" data-lang="%s">%s</a>' % (other, olang, olang, olang, olabel))

    alt = ''
    if len(d) == 2:
        alt = ''.join('<link rel="alternate" hreflang="%s" href="%s%s/%s.html">\n' % (l, SITE, LANGS[l], d[l][0]) for l in ('es', 'en'))

    return '''<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-or-later · GENERADO por tools/gen_site.py desde %(src)s: no editar a mano. -->
<html lang="%(lang)s">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%(title)s · Bubba Machine</title>
%(alt)s<link rel="icon" href="%(up)s/icon.png">
<link rel="stylesheet" href="%(up)s/site.css">
</head>
<body>
<header class="top">
  <div class="in">
    <a class="brand" href="../index.html"><img src="%(up)s/img/icon.svg" alt="">Bubba Machine</a>
    <nav aria-label="%(sitel)s">
      %(nav)s
    </nav>
  </div>
</header>
<div class="docs">
  <aside aria-label="%(docsl)s">%(side)s</aside>
  <main class="doc">
%(body)s
    <p class="src">%(srcnote)s</p>
  </main>
</div>
<footer>
  <div class="in">
    <p>%(foot)s</p>
  </div>
</footer>
<script>document.querySelectorAll('[data-lang]').forEach(function (a) { a.addEventListener('click', function () { try { localStorage.setItem('bm-lang', a.getAttribute('data-lang')); } catch (e) {} }); });</script>
</body>
</html>
''' % {'lang': lang, 'title': html.escape(title), 'alt': alt, 'up': up, 'sitel': T['site'], 'docsl': T['docs'],
       'nav': '\n      '.join(nav), 'side': '\n    '.join(side), 'body': body, 'src': src,
       'srcnote': T['src'] % (REPO + src, src), 'foot': T['foot'] % VERSION}


def main():
    for lang, sub in LANGS.items():
        os.makedirs(os.path.join(WEB, sub), exist_ok=True)
    os.makedirs(os.path.join(WEB, 'img'), exist_ok=True)
    for key, _, d in PAGES:
        for lang, (slug, label, src) in d.items():
            cur_dir = os.path.join(WEB, LANGS[lang])
            with open(os.path.join(ROOT, src), encoding='utf-8') as f:
                title, body = markdown(f.read(), src, cur_dir)
            with open(out_file(lang, slug), 'w', encoding='utf-8', newline='\n') as f:
                f.write(page(lang, key, title or label, body, src))
            print('web/%s/%s.html  <-  %s' % (LANGS[lang], slug, src))
    for name in IMAGES:
        shutil.copyfile(os.path.join(ROOT, 'assets', 'screens', name + '.png'), os.path.join(WEB, 'img', name + '.png'))
    shutil.copyfile(os.path.join(ROOT, 'assets', 'logo', 'bubba-machine-logo.png'), os.path.join(WEB, 'img', 'logo.png'))
    shutil.copyfile(os.path.join(ROOT, 'assets', 'logo', 'bubba-machine-icon.svg'), os.path.join(WEB, 'img', 'icon.svg'))
    print('imagenes en web/img/')


if __name__ == '__main__':
    main()
