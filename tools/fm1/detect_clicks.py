#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Detect silence gaps and discontinuities in a captured FM-1 WAV.

  py -3 tools/fm1/detect_clicks.py WAV [--half-ms 5.8] [--late N] [--out DIR]

Buckets near-zero gaps (short / half / long), flags sample jumps, writes
report + CSV + waveform/spectrogram PNGs with markers.
"""
import argparse, csv, os, struct, sys, wave

import numpy as np

try:
    from PIL import Image, ImageDraw
except ImportError:
    Image = ImageDraw = None


HALF_MS_DEFAULT = 5.8  # one ALNK0 DMA half at BM_FS_OUT
GAP_FLOOR_MS = 0.5
NEAR_ZERO = 80 / 32768.0  # |sample| below this = silence (S16 scale)
JUMP_ABS = 0.35           # normalized |Δ| after one-pole HP
JUMP_CLUSTER_MS = 2.0


def load_wav(path):
    with wave.open(path, 'rb') as w:
        ch, width, rate, nframes, _, _ = w.getparams()
        raw = w.readframes(nframes)
    if width != 2:
        raise SystemExit('solo S16_LE')
    x = np.frombuffer(raw, dtype='<i2').astype(np.float32) / 32768.0
    if ch > 1:
        x = x.reshape(-1, ch).mean(axis=1)
    return x, rate


def _bucket_gap(ms, n, half, buckets):
    ratio = n / half if half else 0
    if ms < 1.0:
        buckets['sub_ms'] += 1
        return 'gap_short', ratio
    if ms < 3.0 or (ms >= 3.0 and ratio < 0.7):
        buckets['1_3_ms'] += 1
        return 'gap_short', ratio
    if 0.7 <= ratio <= 1.4:
        buckets['near_half'] += 1
        return 'gap_half', ratio
    if 1.4 < ratio <= 2.5:
        buckets['two_half'] += 1
        return 'gap_long', ratio
    buckets['longer'] += 1
    return 'gap_long', ratio


def find_gaps(x, rate, half_ms):
    """Absolute near-zero runs plus relative energy dips (capture rarely hits true 0)."""
    floor = int(GAP_FLOOR_MS * rate / 1000.0)
    half = half_ms * rate / 1000.0
    events = []
    buckets = {
        'sub_ms': 0, '1_3_ms': 0, 'near_half': 0, 'two_half': 0, 'longer': 0,
    }

    silent = np.abs(x) < NEAR_ZERO
    d = np.diff(silent.astype(np.int8), prepend=0, append=0)
    starts = np.where(d == 1)[0]
    ends = np.where(d == -1)[0]
    for s, e in zip(starts, ends):
        n = e - s
        if n < floor:
            continue
        if s == 0 or e >= len(x):
            continue
        ms = 1000.0 * n / rate
        kind, ratio = _bucket_gap(ms, n, half, buckets)
        events.append({
            'kind': kind, 't0': s / rate, 't1': e / rate, 'ms': ms,
            'samples': int(n), 'bucket_ratio': ratio, 'how': 'abs0',
        })

    # Relative energy dips: RMS in win << sides (underrun holes under analog noise/clip)
    win_ms_list = (0.5, 1.0, 2.0, 3.0, half_ms, half_ms * 2)
    x2 = x * x
    # integral for fast window energy
    csum = np.concatenate([[0.0], np.cumsum(x2, dtype=np.float64)])
    def win_e(i0, n):
        return (csum[i0 + n] - csum[i0]) / n

    seen = set()  # quantize starts to 0.5 ms to avoid double-count
    q = max(1, int(0.5 * rate / 1000.0))
    for win_ms in win_ms_list:
        n = max(floor, int(win_ms * rate / 1000.0))
        pad = n
        hop = max(1, n // 4)
        for i in range(pad, len(x) - pad - n, hop):
            mid = win_e(i, n)
            side = 0.5 * (win_e(i - pad, pad) + win_e(i + n, pad))
            if side < 1e-4:
                continue
            if mid > side * 0.08:  # mid must be << sides
                continue
            key = i // q
            if key in seen:
                continue
            # refine extent: grow while energy stays low
            lo, hi = i, i + n
            thr_e = side * 0.12
            while lo > pad and win_e(lo - hop, hop) < thr_e:
                lo -= hop
            while hi + hop < len(x) - pad and win_e(hi, hop) < thr_e:
                hi += hop
            n2 = hi - lo
            if n2 < floor:
                continue
            for k in range(lo // q, (hi // q) + 1):
                seen.add(k)
            ms = 1000.0 * n2 / rate
            kind, ratio = _bucket_gap(ms, n2, half, buckets)
            events.append({
                'kind': kind, 't0': lo / rate, 't1': hi / rate, 'ms': ms,
                'samples': int(n2), 'bucket_ratio': ratio, 'how': 'energy',
            })

    events.sort(key=lambda e: e['t0'])
    return events, buckets


def find_jumps(x, rate, gaps):
    # first difference as a cheap high-pass (vectorized)
    y = np.empty_like(x)
    y[0] = 0
    y[1:] = x[1:] - x[:-1]
    mag = np.abs(y)
    thr = max(JUMP_ABS, float(np.percentile(mag, 99.9)) * 1.5)
    hits = np.where(mag > thr)[0]
    cluster = int(JUMP_CLUSTER_MS * rate / 1000.0)
    events = []
    i = 0
    while i < len(hits):
        j = i + 1
        while j < len(hits) and hits[j] - hits[j - 1] <= cluster:
            j += 1
        peak = hits[i + int(np.argmax(mag[hits[i:j]]))]
        t = peak / rate
        at_edge = False
        for g in gaps:
            if abs(t - g['t0']) < 0.003 or abs(t - g['t1']) < 0.003:
                at_edge = True
                break
        events.append({
            'kind': 'jump', 't0': t, 't1': t, 'ms': 0.0,
            'samples': 1, 'peak': float(mag[peak]), 'at_gap_edge': at_edge,
        })
        i = j
    return events, thr


def write_csv(path, events):
    with open(path, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=['kind', 't0', 't1', 'ms', 'samples', 'at_gap_edge', 'peak'])
        w.writeheader()
        for e in events:
            w.writerow({
                'kind': e['kind'], 't0': '%.6f' % e['t0'], 't1': '%.6f' % e['t1'],
                'ms': '%.3f' % e['ms'], 'samples': e.get('samples', ''),
                'at_gap_edge': e.get('at_gap_edge', ''),
                'peak': '%.4f' % e['peak'] if 'peak' in e else '',
            })


def draw_waveform(path, x, rate, events, max_w=1600, h=320):
    if Image is None:
        return
    n = len(x)
    w = min(max_w, max(400, n // 200))
    img = Image.new('RGB', (w, h), (12, 14, 18))
    draw = ImageDraw.Draw(img)
    mid = h // 2
    # downsample peak envelope
    step = max(1, n // w)
    for i in range(w):
        sl = x[i * step:(i + 1) * step]
        if len(sl) == 0:
            continue
        lo, hi = float(sl.min()), float(sl.max())
        y0 = int(mid - hi * (mid - 4))
        y1 = int(mid - lo * (mid - 4))
        draw.line([(i, y0), (i, y1)], fill=(80, 180, 220))
    colors = {
        'gap_short': (255, 200, 40),
        'gap_half': (255, 80, 60),
        'gap_long': (255, 40, 180),
        'jump': (120, 255, 100),
    }
    for e in events:
        x0 = int(e['t0'] * rate / step)
        x1 = int(e.get('t1', e['t0']) * rate / step)
        x0 = max(0, min(w - 1, x0))
        x1 = max(0, min(w - 1, x1))
        c = colors.get(e['kind'], (255, 255, 255))
        if e['kind'] == 'jump':
            draw.line([(x0, 0), (x0, h - 1)], fill=c)
        else:
            draw.rectangle([x0, 2, max(x0 + 1, x1), h - 3], outline=c)
    img.save(path)


def draw_spectrogram(path, x, rate, events, max_w=1600, h=400):
    if Image is None:
        return
    # coarse STFT without scipy
    win = 1024
    hop = 512
    n = len(x)
    cols = max(1, (n - win) // hop + 1)
    cols = min(cols, max_w)
    hop = max(1, (n - win) // cols) if cols > 1 else hop
    cols = max(1, (n - win) // hop + 1)
    window = np.hanning(win).astype(np.float32)
    rows = win // 2
    spec = np.zeros((rows, cols), dtype=np.float32)
    for c in range(cols):
        i0 = c * hop
        if i0 + win > n:
            break
        frame = x[i0:i0 + win] * window
        mag = np.abs(np.fft.rfft(frame))[:rows]
        spec[:, c] = np.log10(mag + 1e-6)
    # normalize
    lo, hi = np.percentile(spec, [5, 99.5])
    if hi <= lo:
        hi = lo + 1
    img = Image.new('RGB', (cols, rows), (0, 0, 0))
    px = img.load()
    for c in range(cols):
        for r in range(rows):
            v = (spec[r, c] - lo) / (hi - lo)
            v = 0 if v < 0 else (1 if v > 1 else v)
            # flip freq axis
            y = rows - 1 - r
            px[c, y] = (int(40 + 200 * v), int(20 + 100 * v), int(60 + 180 * (1 - v)))
    # stretch to max_w × h
    img = img.resize((min(max_w, max(cols, 400)), h), Image.NEAREST)
    draw = ImageDraw.Draw(img)
    w = img.size[0]
    for e in events:
        x0 = int(e['t0'] * rate / hop * (w / cols)) if cols else 0
        x0 = max(0, min(w - 1, x0))
        col = {'gap_short': (255, 220, 0), 'gap_half': (255, 60, 40),
               'gap_long': (255, 0, 200), 'jump': (80, 255, 80)}.get(e['kind'], (255, 255, 255))
        draw.line([(x0, 0), (x0, h - 1)], fill=col)
    img.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('wav')
    ap.add_argument('--half-ms', type=float, default=HALF_MS_DEFAULT)
    ap.add_argument('--late', type=int, default=None, help='consola delta LATE for comparison')
    ap.add_argument('--out', default=None)
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')

    x, rate = load_wav(a.wav)
    dur = len(x) / rate
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x * x)))
    gaps, buckets = find_gaps(x, rate, a.half_ms)
    jumps, thr = find_jumps(x, rate, gaps)
    events = sorted(gaps + jumps, key=lambda e: e['t0'])

    base = a.out or (os.path.splitext(a.wav)[0] + '-clicks')
    os.makedirs(os.path.dirname(base) or '.', exist_ok=True)
    csv_path = base + '.csv'
    report_path = base + '.txt'
    wave_png = base + '-wave.png'
    spec_png = base + '-spec.png'

    write_csv(csv_path, events)
    draw_waveform(wave_png, x, rate, events)
    draw_spectrogram(spec_png, x, rate, events)

    n_half = sum(1 for e in gaps if e['kind'] == 'gap_half')
    n_short = sum(1 for e in gaps if e['kind'] == 'gap_short')
    n_long = sum(1 for e in gaps if e['kind'] == 'gap_long')
    n_jump = len(jumps)
    n_edge = sum(1 for e in jumps if e.get('at_gap_edge'))

    lines = [
        'detect_clicks %s' % a.wav,
        'duration %.2f s  rate %d  peak %.3f  rms %.4f  jump_thr %.3f' % (dur, rate, peak, rms, thr),
        'gaps: short=%d half=%d long=%d  | buckets sub_ms=%d 1_3_ms=%d near_half=%d two_half=%d longer=%d' % (
            n_short, n_half, n_long,
            buckets['sub_ms'], buckets['1_3_ms'], buckets['near_half'],
            buckets['two_half'], buckets['longer']),
        'jumps: %d (at gap edge: %d)' % (n_jump, n_edge),
        'rates: half/s=%.3f short/s=%.3f jump/s=%.3f' % (
            n_half / dur if dur else 0, n_short / dur if dur else 0, n_jump / dur if dur else 0),
    ]
    if a.late is not None:
        lines.append('consola delta LATE=%d  vs gap_half=%d  (match %s)' % (
            a.late, n_half, 'yes' if abs(a.late - n_half) <= max(1, a.late // 10) else 'no'))
    lines.append('csv %s' % csv_path)
    lines.append('wave %s' % wave_png)
    lines.append('spec %s' % spec_png)
    # first 20 events
    lines.append('events (first 20):')
    for e in events[:20]:
        extra = ''
        if e['kind'] == 'jump':
            extra = ' peak=%.3f edge=%s' % (e.get('peak', 0), e.get('at_gap_edge'))
        else:
            extra = ' %.2f ms' % e['ms']
        lines.append('  %8.3f s  %-9s%s' % (e['t0'], e['kind'], extra))

    text = '\n'.join(lines) + '\n'
    open(report_path, 'w', encoding='utf-8').write(text)
    print(text, end='')
    print('report', report_path)


if __name__ == '__main__':
    main()
