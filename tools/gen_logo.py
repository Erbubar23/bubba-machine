#!/usr/bin/env python3
"""Logo de Bubba Machine: el sol con lentes oscuros, sonriente.

Rediseño respecto a Bubba Box: los 8 rayos redondos ahora son 12 dientes de
engranaje (el sol es una "máquina"), y debajo de "bubba" dice "machine".

Genera (una sola geometría para todo):
  assets/logo/bubba-machine-logo.svg / .png   (760x260, sol + palabra)
  assets/logo/bubba-machine-icon.svg  / .png  (240x240)
  assets/logo/bubba-machine-splash.png        (pantalla de inicio 240x240)
  core/gen/bm_logo.c                          (splash en RLE con paleta, RGB565)
"""
import math, os, sys
from PIL import Image, ImageDraw

BLUE, GREEN, YELLOW, ORANGE = '#287CFF', '#1ECC70', '#FFC618', '#FF621A'
WHITE, BLACK = '#F2F2F2', '#000000'
RAYS = [BLUE, GREEN, YELLOW, ORANGE]


# ------------------------------------------------------------ primitivas
def arc_pts(cx, cy, r, t0, t1, n=48, ry=None):
    ry = r if ry is None else ry
    return [(cx + r * math.cos(t0 + (t1 - t0) * i / n), cy - ry * math.sin(t0 + (t1 - t0) * i / n))
            for i in range(n + 1)]


def svg_arc_pts(x1, y1, rx, ry, large, sweep, x2, y2, n=40):
    """Convierte un arco SVG (sin rotación) a polilínea."""
    dx, dy = (x1 - x2) / 2, (y1 - y2) / 2
    lam = (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry)
    if lam > 1:
        rx *= math.sqrt(lam); ry *= math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * dy * dy - ry * ry * dx * dx
    den = rx * rx * dy * dy + ry * ry * dx * dx
    co = math.sqrt(max(0.0, num / den)) * (-1 if large == sweep else 1)
    cxp, cyp = co * rx * dy / ry, -co * ry * dx / rx
    cx, cy = cxp + (x1 + x2) / 2, cyp + (y1 + y2) / 2
    a1 = math.atan2((dy - cyp) / ry, (dx - cxp) / rx)
    a2 = math.atan2((-dy - cyp) / ry, (-dx - cxp) / rx)
    da = a2 - a1
    if sweep and da < 0:
        da += 2 * math.pi
    if not sweep and da > 0:
        da -= 2 * math.pi
    return [(cx + rx * math.cos(a1 + da * i / n), cy + ry * math.sin(a1 + da * i / n)) for i in range(n + 1)]


def tf(prims, ox, oy, s, cx=0.0, cy=0.0):
    """Escala s alrededor de (cx,cy) y traslada a (ox,oy)."""
    def p(x, y):
        return (ox + (x - cx) * s, oy + (y - cy) * s)
    out = []
    for pr in prims:
        k = pr[0]
        if k == 'circle':
            _, x, y, r, c = pr; px, py = p(x, y); out.append((k, px, py, r * s, c))
        elif k == 'ring':
            _, x, y, r, c, w = pr; px, py = p(x, y); out.append((k, px, py, r * s, c, w * s))
        elif k == 'poly':
            _, pts, c = pr; out.append((k, [p(*q) for q in pts], c))
        elif k == 'rrect':
            _, x, y, w, h, rx, c = pr; px, py = p(x, y); out.append((k, px, py, w * s, h * s, rx * s, c))
        elif k == 'line':
            _, pts, c, w = pr; out.append((k, [p(*q) for q in pts], c, w * s))
    return out


def L(pts, c, w):
    return ('line', pts, c, w)


# ------------------------------------------------------------ el sol
def sun_icon():
    """Sol en un espacio de 240x240 con centro (120,120)."""
    cx = cy = 120.0
    pr = []
    for k in range(12):  # 12 dientes de engranaje en lugar de 8 rayos
        a = math.radians(90 - k * 30)
        ux, uy = math.cos(a), -math.sin(a)
        vx, vy = -uy, ux
        r0, r1, w0, w1 = 74.0, 102.0, 10.5, 8.0
        pts = [(cx + ux * r0 + vx * w0, cy + uy * r0 + vy * w0),
               (cx + ux * r1 + vx * w1, cy + uy * r1 + vy * w1),
               (cx + ux * r1 - vx * w1, cy + uy * r1 - vy * w1),
               (cx + ux * r0 - vx * w0, cy + uy * r0 - vy * w0)]
        pr.append(('poly', pts, RAYS[k % 4]))
    pr.append(('circle', cx, cy, 66.0, YELLOW))
    pr.append(('rrect', 62.0, 100.0, 51.0, 30.0, 11.0, BLACK))
    pr.append(('rrect', 127.0, 100.0, 51.0, 30.0, 11.0, BLACK))
    pr.append(L([(73, 108), (81, 116)], WHITE, 4))
    pr.append(L([(138, 108), (146, 116)], WHITE, 4))
    pr.append(L([(111, 108), (129, 108)], BLACK, 6))
    pr.append(L([(64, 106), (56, 100)], BLACK, 6))
    pr.append(L([(176, 106), (184, 100)], BLACK, 6))
    pr.append(L(svg_arc_pts(144.63, 145.46, 17, 7, 0, 1, 117.53, 149.52), BLACK, 6))
    return pr


def word_bubba():
    """'bubba' en el espacio del logo de 760x260 (misma geometría que Bubba Box)."""
    W, w = WHITE, 12.0
    pr = [('ring', 306, 132, 30, W, w), L([(276, 70), (276, 162)], W, w),
          L(svg_arc_pts(426, 132, 30, 30, 0, 1, 366, 132), W, w),
          L([(366, 102), (366, 132)], W, w), L([(426, 102), (426, 162)], W, w),
          ('ring', 486, 132, 30, W, w), L([(456, 70), (456, 162)], W, w),
          ('ring', 576, 132, 30, W, w), L([(546, 70), (546, 162)], W, w),
          ('ring', 666, 132, 30, W, w), L([(696, 132), (696, 162)], W, w)]
    return pr


def word_machine(right, base, h, w, color=YELLOW):
    """'machine' con trazos geométricos, alineado a la derecha en x=right."""
    r = h / 2.0
    rm = h * 0.28
    gap = h * 0.42
    letters = []

    def m(x):
        return [L([(x, base), (x, base - h + rm)], color, w),
                L(arc_pts(x + rm, base - h + rm, rm, math.pi, 0), color, w),
                L([(x + 2 * rm, base - h + rm), (x + 2 * rm, base)], color, w),
                L(arc_pts(x + 3 * rm, base - h + rm, rm, math.pi, 0), color, w),
                L([(x + 4 * rm, base - h + rm), (x + 4 * rm, base)], color, w)], 4 * rm

    def a(x):
        return [('ring', x + r, base - r, r, color, w), L([(x + 2 * r, base - h), (x + 2 * r, base)], color, w)], 2 * r

    def c(x):
        return [L(arc_pts(x + r, base - r, r, 0.28 * math.pi, 1.72 * math.pi), color, w)], 2 * r

    def hh(x):
        return [L([(x, base - 1.6 * h), (x, base)], color, w),
                L(arc_pts(x + r, base - h + r, r, math.pi, 0), color, w),
                L([(x + 2 * r, base - h + r), (x + 2 * r, base)], color, w)], 2 * r

    def i(x):
        return [L([(x, base - h), (x, base)], color, w), ('circle', x, base - h - 0.45 * h, w * 0.62, color)], 0.0

    def n(x):
        return [L([(x, base - h), (x, base)], color, w),
                L(arc_pts(x + r, base - h + r, r, math.pi, 0), color, w),
                L([(x + 2 * r, base - h + r), (x + 2 * r, base)], color, w)], 2 * r

    def e(x):
        return [L([(x, base - r), (x + 2 * r, base - r)], color, w),
                L(arc_pts(x + r, base - r, r, 0.0, 1.76 * math.pi), color, w)], 2 * r

    seq = [m, a, c, hh, i, n, e]
    widths = [f(0)[1] for f in seq]
    total = sum(widths) + gap * (len(seq) - 1)
    x = right - total
    for f, wd in zip(seq, widths):
        letters += f(x)[0]
        x += wd + gap
    return letters


# ------------------------------------------------------------ render
def render(prims, size, ss=4, bg=BLACK):
    W, H = size
    img = Image.new('RGB', (W * ss, H * ss), bg)
    d = ImageDraw.Draw(img)

    def S(v):
        return v * ss

    for pr in prims:
        k = pr[0]
        if k == 'circle':
            _, x, y, r, c = pr
            d.ellipse([S(x - r), S(y - r), S(x + r), S(y + r)], fill=c)
        elif k == 'ring':
            _, x, y, r, c, w = pr
            d.ellipse([S(x - r - w / 2), S(y - r - w / 2), S(x + r + w / 2), S(y + r + w / 2)], outline=c, width=max(1, round(S(w))))
        elif k == 'poly':
            _, pts, c = pr
            d.polygon([(S(x), S(y)) for x, y in pts], fill=c)
        elif k == 'rrect':
            _, x, y, w, h, rx, c = pr
            d.rounded_rectangle([S(x), S(y), S(x + w), S(y + h)], radius=S(rx), fill=c)
        elif k == 'line':
            _, pts, c, w = pr
            sp = [(S(x), S(y)) for x, y in pts]
            d.line(sp, fill=c, width=max(1, round(S(w))), joint='curve')
            for (x, y) in (sp[0], sp[-1]):
                rr = S(w) / 2
                d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=c)
    return img.resize((W, H), Image.LANCZOS)


def to_svg(prims, size, label):
    W, H = size
    o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" role="img" aria-label="%s">' % (W, H, W, H, label),
         '<rect width="%d" height="%d" fill="#000"/>' % (W, H)]
    for pr in prims:
        k = pr[0]
        if k == 'circle':
            _, x, y, r, c = pr
            o.append('<circle cx="%.2f" cy="%.2f" r="%.2f" fill="%s"/>' % (x, y, r, c))
        elif k == 'ring':
            _, x, y, r, c, w = pr
            o.append('<circle cx="%.2f" cy="%.2f" r="%.2f" fill="none" stroke="%s" stroke-width="%.2f"/>' % (x, y, r, c, w))
        elif k == 'poly':
            _, pts, c = pr
            o.append('<polygon points="%s" fill="%s"/>' % (' '.join('%.2f,%.2f' % q for q in pts), c))
        elif k == 'rrect':
            _, x, y, w, h, rx, c = pr
            o.append('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" rx="%.2f" fill="%s"/>' % (x, y, w, h, rx, c))
        elif k == 'line':
            _, pts, c, w = pr
            o.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="%.2f" stroke-linecap="round" stroke-linejoin="round"/>'
                     % (' '.join('%.2f,%.2f' % q for q in pts), c, w))
    o.append('</svg>')
    return '\n'.join(o) + '\n'


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def rle_c(img):
    q = img.quantize(colors=48, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:48 * 3]
    px = q.load()
    W, H = q.size
    rle, rows = [], []
    for y in range(H):
        rows.append(len(rle))
        x = 0
        while x < W:
            c = px[x, y]; n = 1
            while x + n < W and px[x + n, y] == c and n < 255:
                n += 1
            rle += [n, c]
            x += n
    cols = [rgb565(tuple(pal[i * 3:i * 3 + 3])) for i in range(48)]
    out = ['/* GENERADO por tools/gen_logo.py: no editar a mano. */', '#include "../bm_gfx.h"', '',
           'const uint16_t bm_logo_pal[48] = {%s};' % ','.join('0x%04X' % c for c in cols),
           'const uint16_t bm_logo_row[240] = {%s};' % ','.join(str(r) for r in rows),
           'const uint8_t bm_logo_rle[%d] = {' % len(rle)]
    for i in range(0, len(rle), 32):
        out.append('  ' + ','.join(str(v) for v in rle[i:i + 32]) + ',')
    out.append('};')
    return '\n'.join(out) + '\n', len(rle)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    adir = os.path.join(root, 'assets', 'logo')
    os.makedirs(adir, exist_ok=True)
    os.makedirs(os.path.join(root, 'core', 'gen'), exist_ok=True)

    icon = sun_icon()
    logo = tf(icon, 130, 130, 1.0, 120, 120) + word_bubba() + word_machine(702, 216, 22.0, 5.04)
    splash = (tf(icon, 120, 86, 0.60, 120, 120)
              + tf(word_bubba(), 42, 160, 0.36, 270, 64)
              + word_machine(197.5, 223, 9.0, 2.2))

    for name, prims, size in (('bubba-machine-logo', logo, (760, 260)), ('bubba-machine-icon', icon, (240, 240))):
        with open(os.path.join(adir, name + '.svg'), 'w', encoding='utf-8', newline='\n') as f:
            f.write(to_svg(prims, size, 'Bubba Machine'))
        render(prims, size).save(os.path.join(adir, name + '.png'))
    sp = render(splash, (240, 240))
    sp.save(os.path.join(adir, 'bubba-machine-splash.png'))
    src, n = rle_c(sp)
    with open(os.path.join(root, 'core', 'gen', 'bm_logo.c'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(src)
    print('logo listo; splash RLE = %d bytes' % n)
    return 0


if __name__ == '__main__':
    sys.exit(main())
