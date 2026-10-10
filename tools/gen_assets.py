#!/usr/bin/env python3
"""Genera core/gen/bm_rom.c: kits de batería y wavetables de Bubba Machine.

- Batería: 2 kits x 16 sonidos SINTETIZADOS aquí (sin samples de terceros),
  a BM_FS = 22059 Hz, codificados en mu-law de 8 bits.
- Wavetables: 16 ondas de 256 muestras, con 8 niveles band-limited (mipmaps),
  generadas por síntesis aditiva. Con --akwf DIR se reemplazan por ciclos
  únicos de WAV (p. ej. AKWF, CC0): cada ciclo se analiza por DFT y se
  reconstruye a 256 muestras por nivel.
- Comprueba el presupuesto de ROM de SPEC_V3 §4.4.

Uso: py -3 tools/gen_assets.py [--akwf CARPETA]
"""
import argparse, math, os, sys, wave
import numpy as np

FS = 22059
WAVE_LEN = 256
LEVELS = 8
MAX_H = [127, 64, 32, 16, 8, 4, 2, 1]
BUDGET_DRUMS = 192 * 1024
BUDGET_WAVES = 64 * 1024

rng = np.random.default_rng(23)


def t_of(dur):
    return np.arange(int(dur * FS)) / FS


def noise(n):
    return rng.uniform(-1.0, 1.0, n)


def onepole_lp(x, fc):
    a = 1.0 - math.exp(-2 * math.pi * fc / FS)
    y = np.empty_like(x); s = 0.0
    for i, v in enumerate(x):
        s += a * (v - s); y[i] = s
    return y


def hp(x, fc):
    return x - onepole_lp(x, fc)


def bp(x, fc, q=2.0):
    # biquad band-pass (RBJ)
    w0 = 2 * math.pi * fc / FS; al = math.sin(w0) / (2 * q)
    b0, b1, b2 = al, 0.0, -al
    a0, a1, a2 = 1 + al, -2 * math.cos(w0), 1 - al
    y = np.zeros_like(x); x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = (b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, v, y1, o; y[i] = o
    return y


def sine_sweep(t, f):
    ph = 2 * math.pi * np.cumsum(f) / FS
    return np.sin(ph)


def metal(t, freqs):
    s = np.zeros_like(t)
    for f in freqs:
        s += np.sign(np.sin(2 * math.pi * f * t))
    return s / len(freqs)


def kick(dur, f0, fsweep, tsweep, dec, click, drive):
    t = t_of(dur)
    f = f0 + fsweep * np.exp(-t / tsweep)
    s = sine_sweep(t, f) * np.exp(-t / dec)
    c = onepole_lp(noise(len(t)), 3000) * np.exp(-t / 0.002) * click
    return np.tanh((s + c) * drive) / math.tanh(drive)


def snare(dur, tones, tdec, ndec, nbright):
    t = t_of(dur)
    s = sum(np.sin(2 * math.pi * f * t) for f in tones) / len(tones) * np.exp(-t / tdec)
    n = hp(noise(len(t)), nbright) * np.exp(-t / ndec)
    return 0.55 * s + 0.75 * n


def clap(dur):
    t = t_of(dur)
    n = bp(noise(len(t)), 1100, 1.6)
    e = np.zeros_like(t)
    for d in (0.0, 0.011, 0.022):
        m = t >= d
        e[m] += np.exp(-(t[m] - d) / 0.005)
    m = t >= 0.03
    e[m] += 0.6 * np.exp(-(t[m] - 0.03) / 0.12)
    return n * e


def hat(dur, dec, freqs, cut):
    t = t_of(dur)
    m = metal(t, freqs)
    return hp(bp(m, 9000, 0.9) + 0.3 * noise(len(t)), cut) * np.exp(-t / dec)


def tom(dur, base, dec, n_amt):
    t = t_of(dur)
    f = base * (1 + 0.45 * np.exp(-t / 0.05))
    s = sine_sweep(t, f) * np.exp(-t / dec)
    return s + n_amt * onepole_lp(noise(len(t)), 2500) * np.exp(-t / 0.03)


def tone(dur, f, dec):
    t = t_of(dur)
    return np.sin(2 * math.pi * f * t) * np.exp(-t / dec)


def cowbell(dur):
    t = t_of(dur)
    s = (np.sign(np.sin(2 * math.pi * 540 * t)) + np.sign(np.sin(2 * math.pi * 800 * t))) * 0.5
    return bp(s, 800, 2.5) * np.exp(-t / 0.09)


def cymbal(dur, dec):
    t = t_of(dur)
    m = metal(t, [205.3, 304.4, 369.6, 522.7, 540.0, 800.0])
    return hp(m * 0.6 + noise(len(t)) * 0.5, 4000) * np.exp(-t / dec)


def shaker(dur):
    t = t_of(dur)
    env = (1 - np.exp(-t / 0.008)) * np.exp(-t / 0.035)
    return hp(noise(len(t)), 5000) * env


def zap(dur):
    t = t_of(dur)
    f = 80 + 1400 * np.exp(-t / 0.025)
    return sine_sweep(t, f) * np.exp(-t / 0.08)


HH808 = [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]
HH909 = [263.0, 400.0, 421.0, 474.0, 587.0, 845.0]

NAMES = ['KICK', 'KICK2', 'CL HH', 'OP HH', 'SNARE', 'CLAP', 'RIM', 'TOM L',
         'TOM M', 'TOM H', 'COWBL', 'CYMBL', 'SHAKE', 'CLAVE', 'CONGA', 'ZAP']
CHOKE = [0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
DUCKS = [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]


def kit_808():
    return [
        kick(0.50, 47, 120, 0.040, 0.32, 0.4, 1.4),
        kick(0.28, 58, 170, 0.020, 0.13, 0.9, 2.2),
        hat(0.06, 0.018, HH808, 6000),
        hat(0.40, 0.20, HH808, 6000),
        snare(0.25, [185, 330], 0.07, 0.11, 1800),
        clap(0.30),
        tone(0.04, 1700, 0.006) + tone(0.04, 480, 0.01),
        tom(0.30, 92, 0.17, 0.0),
        tom(0.28, 132, 0.15, 0.0),
        tom(0.25, 182, 0.13, 0.0),
        cowbell(0.25),
        cymbal(0.55, 0.30),
        shaker(0.09),
        tone(0.05, 2500, 0.018),
        tom(0.20, 310, 0.08, 0.0),
        zap(0.15),
    ]


def kit_909():
    return [
        kick(0.36, 54, 210, 0.018, 0.20, 1.0, 2.6),
        kick(0.24, 64, 260, 0.012, 0.09, 1.2, 3.2),
        hat(0.06, 0.016, HH909, 7000),
        hat(0.38, 0.17, HH909, 7000),
        snare(0.27, [200, 345], 0.05, 0.16, 2600),
        clap(0.30) * 1.1,
        tone(0.04, 2100, 0.005) + 0.4 * hp(noise(int(0.04 * FS)), 3000) * np.exp(-t_of(0.04) / 0.004),
        tom(0.30, 100, 0.18, 0.15),
        tom(0.27, 142, 0.15, 0.15),
        tom(0.24, 200, 0.12, 0.15),
        cowbell(0.25) * 0.9,
        cymbal(0.60, 0.38),
        shaker(0.09),
        tone(0.05, 2900, 0.015),
        tom(0.20, 330, 0.07, 0.05),
        zap(0.15),
    ]


def mulaw_encode(x):
    x = np.clip(x, -1.0, 1.0)
    mu = 255.0
    y = np.sign(x) * np.log1p(mu * np.abs(x)) / np.log1p(mu)
    q = np.round(y * 127.0).astype(int)  # -127..127
    return [(v & 0xFF) for v in q]


def mulaw_decode_table():
    mu = 255.0
    out = []
    for b in range(256):
        v = b - 256 if b >= 128 else b
        y = max(-1.0, min(1.0, v / 127.0))
        x = np.sign(y) * (np.power(1 + mu, abs(y)) - 1) / mu
        out.append(int(round(x * 32767)))
    return out


# ---------------------------------------------------------------- ondas
WAVE_NAMES = ['SAW', 'SQUARE', 'PULSE25', 'PULSE10', 'TRI', 'SINE', 'OCTAVE', 'ORGAN',
              'EPIANO', 'VOX A', 'VOX O', 'BRASS', 'STRINGS', 'HOLLOW', 'BELL', 'BUZZ']


def formant(n, f0, peaks):
    return sum(math.exp(-((n * f0 - p) / bw) ** 2) for p, bw in peaks)


def spectrum(name):
    """Devuelve lista de (armónico, amplitud, fase) para 1..127."""
    H = range(1, 128)
    sp = []
    for n in H:
        a, ph = 0.0, 0.0
        if name == 'SAW':
            a = 1.0 / n
        elif name == 'SQUARE':
            a = 1.0 / n if n % 2 else 0.0
        elif name in ('PULSE25', 'PULSE10'):
            d = 0.25 if name == 'PULSE25' else 0.10
            a = abs(math.sin(math.pi * n * d)) / n
            ph = math.pi / 2 if math.sin(math.pi * n * d) >= 0 else -math.pi / 2
        elif name == 'TRI':
            a = (1.0 / (n * n)) if n % 2 else 0.0
            ph = 0.0 if (n // 2) % 2 == 0 else math.pi
        elif name == 'SINE':
            a = 1.0 if n == 1 else 0.0
        elif name == 'OCTAVE':
            a = {1: 1.0, 2: 0.5, 4: 0.25}.get(n, 0.0)
        elif name == 'ORGAN':
            a = {1: 1.0, 2: 0.8, 3: 0.6, 4: 0.5, 6: 0.3, 8: 0.25}.get(n, 0.0)
        elif name == 'EPIANO':
            a = {1: 1.0, 2: 0.35, 3: 0.12, 7: 0.10, 14: 0.05}.get(n, 0.0)
        elif name == 'VOX A':
            a = (1.0 / n) * (0.15 + formant(n, 220, [(800, 160), (1200, 200), (2500, 300)]))
        elif name == 'VOX O':
            a = (1.0 / n) * (0.15 + formant(n, 220, [(500, 120), (900, 160), (2400, 300)]))
        elif name == 'BRASS':
            a = (1.0 / n ** 0.8) * (1.6 if 2 <= n <= 6 else 1.0)
        elif name == 'STRINGS':
            a = 1.0 / n ** 1.2
            ph = (n * 1.7) % (2 * math.pi)
        elif name == 'HOLLOW':
            a = (1.0 / n ** 0.5) if n % 2 else 0.0
        elif name == 'BELL':
            a = {1: 1.0, 4: 0.6, 7: 0.4, 11: 0.3, 15: 0.2}.get(n, 0.0)
        elif name == 'BUZZ':
            a = 0.35
        sp.append((n, a, ph))
    return sp


def build_levels(sp):
    t = np.arange(WAVE_LEN) / WAVE_LEN
    levels = []
    for mh in MAX_H:
        y = np.zeros(WAVE_LEN)
        for n, a, ph in sp:
            if n <= mh and a:
                y += a * np.sin(2 * math.pi * n * t + ph)
        levels.append(y)
    peak = max(np.max(np.abs(l)) for l in levels) or 1.0
    return [np.round(l / peak * 0.9 * 32767).astype(int) for l in levels]


def akwf_spectrum(path):
    with wave.open(path, 'rb') as w:
        n = w.getnframes(); sw = w.getsampwidth(); ch = w.getnchannels()
        raw = w.readframes(n)
    dt = {1: np.uint8, 2: np.int16}[sw]
    x = np.frombuffer(raw, dtype=dt).astype(float)
    if sw == 1:
        x -= 128
    x = x[::ch]
    X = np.fft.rfft(x) / (len(x) / 2)
    sp = []
    for h in range(1, 128):
        if h < len(X):
            sp.append((h, abs(X[h]), math.atan2(X[h].real, X[h].imag)))
    return sp


def c_array(vals, per=24):
    s = []
    for i in range(0, len(vals), per):
        s.append('  ' + ','.join(str(v) for v in vals[i:i + per]) + ',')
    return '\n'.join(s)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--akwf', help='carpeta con WAV de ciclo único (hasta 16)')
    args = ap.parse_args()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    dst = os.path.join(root, 'core', 'gen', 'bm_rom.c')
    os.makedirs(os.path.dirname(dst), exist_ok=True)

    out = ['/* GENERADO por tools/gen_assets.py: no editar a mano. */',
           '#include "../bm_rom.h"', '']
    kits = [('KO-808', kit_808()), ('KO-909', kit_909())]
    total_drum = 0
    table = []
    for ki, (kname, sounds) in enumerate(kits):
        for si, s in enumerate(sounds):
            s = np.asarray(s, dtype=float)
            # fade-out final de 3 ms para que ningún sample termine con click
            nf = min(len(s), int(0.003 * FS))
            s[-nf:] *= np.linspace(1, 0, nf)
            s = s / (np.max(np.abs(s)) or 1.0) * 0.95
            enc = mulaw_encode(s)
            total_drum += len(enc)
            out.append('static const uint8_t k%d_s%d[%d] = {\n%s\n};' % (ki, si, len(enc), c_array(enc)))
            table.append((ki, si, len(enc)))
    out.append('')
    out.append('const bm_drum_sound bm_rom_kits[BM_KITS][BM_KIT_SOUNDS] = {')
    for ki in range(len(kits)):
        out.append('  {')
        for si in range(16):
            ln = [x for x in table if x[0] == ki and x[1] == si][0][2]
            out.append('    {k%d_s%d, %du, %d, %d, "%s"},' % (ki, si, ln, CHOKE[si], DUCKS[si], NAMES[si]))
        out.append('  },')
    out.append('};')
    out.append('const char bm_rom_kit_names[BM_KITS][8] = {%s};' % ','.join('"%s"' % k for k, _ in kits))
    out.append('')
    out.append('const int16_t bm_ulaw_dec[256] = {\n%s\n};' % c_array(mulaw_decode_table(), 16))
    out.append('')

    names = list(WAVE_NAMES)
    spectra = [spectrum(n) for n in names]
    if args.akwf:
        files = sorted(f for f in os.listdir(args.akwf) if f.lower().endswith('.wav'))[:16]
        for i, f in enumerate(files):
            spectra[i] = akwf_spectrum(os.path.join(args.akwf, f))
            names[i] = os.path.splitext(f)[0].upper().replace('AKWF_', '')[:7]
    out.append('const int16_t bm_rom_waves[BM_WAVES][BM_WAVE_LEVELS][BM_WAVE_LEN] = {')
    for sp in spectra:
        out.append('  {')
        for lvl in build_levels(sp):
            out.append('    {\n%s\n    },' % c_array(list(lvl), 16))
        out.append('  },')
    out.append('};')
    out.append('const char bm_rom_wave_names[BM_WAVES][8] = {%s};' % ','.join('"%s"' % n[:7] for n in names))

    waves_bytes = 16 * LEVELS * WAVE_LEN * 2
    out.append('')
    out.append('/* ROM: batería %d B (presupuesto %d), ondas %d B (presupuesto %d) */' %
               (total_drum, BUDGET_DRUMS, waves_bytes, BUDGET_WAVES))
    with open(dst, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out) + '\n')
    print('escrito %s  batería=%d B  ondas=%d B' % (dst, total_drum, waves_bytes))
    if total_drum > BUDGET_DRUMS or waves_bytes > BUDGET_WAVES:
        print('ERROR: se excede el presupuesto de ROM (SPEC_V3 §4.4)')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
