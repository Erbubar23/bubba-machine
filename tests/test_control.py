# Prueba de la plataforma de control (web/control.html) con un FM-1 simulado en el navegador.
# Corre en Docker: docker run --rm -v "$PWD:/src" -w /src mcr.microsoft.com/playwright/python:v1.48.0-jammy \n#   sh -c "pip install -q playwright==1.48.0; python3 tests/test_control.py"
import subprocess, time, sys, struct, math, wave, json
from playwright.sync_api import sync_playwright

# un WAV de prueba: 4 s de un tono, para ver que la página lo recorta a lo que cabe
with wave.open('/src/build/prueba.wav', 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100)
    w.writeframes(b''.join(struct.pack('<h', int(12000 * math.sin(2 * math.pi * 220 * i / 44100))) for i in range(44100 * 4)))

MOCK = r"""
(() => {
  const st = { takes: [{len: 23851, bpm: 1610, pads: 8}], snds: [{eng: 5, name: 'MY PAD', author: 'YO', sect: 2}], heap: 70 * 1024,
               playing: 0, log: [], cpu: 41 };
  window.__fm1 = st;
  const le32 = (v) => [v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >>> 24) & 255];
  const le16 = (v) => [v & 255, (v >> 8) & 255];
  const s = (t, n) => { const a = new Array(n).fill(0); for (let i = 0; i < t.length && i < n; i++) a[i] = t.charCodeAt(i); return a; };
  function report() {
    const used = st.takes.reduce((a, t) => a + t.len, 0), cap = 96000, free = Math.max(0, cap - used);
    const sb = st.snds.reduce((a, x) => a + x.sect * 4096, 0), tsec = st.takes.reduce((a, t) => a + Math.ceil((t.len + 128) / 4096), 0);
    const zf = 102400 - sb - tsec * 4096;
    return [1, st.playing, st.cpu, 1, st.takes.length, st.snds.length, 12, 0, ...le32(used), ...le32(cap), ...le32(free), ...le32(Math.min(free, 49152)),
            ...le32(102400), ...le32(zf), ...le32(sb), ...le16(16000), ...le32(49152), ...s('', 12), ...le16(0), ...le16(16384), ...s('0.6.1-beta', 14)];
  }
  function answer(cmd, d) {
    st.log.push([cmd, d[0]]);
    if (cmd === 0x01) { const w = new Array(27).fill(0); w[8] = 410; w[9] = 1000; w[26] = 1; return w.flatMap(le32); }
    if (cmd === 0x23) {
      const sub = d[0], i = d[1];
      if (sub === 0) return [0, 1, ...le32(st.heap), ...le32(st.heap), ...report()];
      if (sub === 1) { const t = st.takes[i]; return [1, 1, ...(t ? [1, i, ...le32(t.len), ...le16(t.bpm), 1, 64, t.pads, 0] : new Array(12).fill(0))]; }
      if (sub === 2) { const x = st.snds[i]; if (!x) return [2, 0]; return [2, 1, x.eng, 0, x.sect, 0, ...le16(3000), 0xF9, 0xA4, ...s(x.name, 12), ...s(x.author, 12)]; }
      if (sub === 3) { st.takes.splice(i, 1); return [3, 1]; }
    }
    if (cmd === 0x20 || cmd === 0x21 || cmd === 0x22) {
      const sub = d[0];
      if (st.playing) return [sub, 0];
      if (cmd === 0x20 && sub === 2) { const len = (d[1] << 24) | (d[2] << 16) | (d[3] << 8) | d[4]; st.takes.push({len, bpm: (d[5] << 8) | d[6], pads: 0}); st.lastTake = {len, bpm: (d[5] << 8) | d[6]}; }
      if (cmd === 0x22 && sub === 4) st.snds.splice(d[4], 1);
      if (cmd === 0x22 && sub === 2) st.snds.push({eng: 5, name: 'NEW', author: '', sect: 3});
      return [sub, 1];
    }
    return null;
  }
  const inp = { name: 'Bubba Machine', state: 'connected', onmidimessage: null };
  const out = { name: 'Bubba Machine', state: 'connected', send(m) {
    const cmd = m[4], d = []; for (let i = 5; i + 1 < m.length - 1; i += 2) d.push((m[i] << 4) | m[i + 1]);
    const r = answer(cmd, d); if (!r) return;
    const msg = [0xF0, 0x7D, 0x42, 0x4D, cmd | 0x40]; for (const b of r) msg.push(b >> 4, b & 15); msg.push(0xF7);
    setTimeout(() => { if (inp.onmidimessage) inp.onmidimessage({ data: new Uint8Array(msg) }); }, 2);
  } };
  const access = { inputs: new Map([['i', inp]]), outputs: new Map([['o', out]]), onstatechange: null };
  navigator.requestMIDIAccess = async () => access;
})();
"""

srv = subprocess.Popen([sys.executable, '-m', 'http.server', '8766', '--directory', '/src/web'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1.5)
errors, fails = [], []
def check(c, what):
    print(('ok   ' if c else 'MAL  ') + what)
    if not c: fails.append(what)
with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page(viewport={'width': 1200, 'height': 1500})
    pg.on('console', lambda m: errors.append(m.text) if m.type == 'error' else None)
    pg.on('pageerror', lambda e: errors.append(str(e)))
    pg.on('dialog', lambda d: d.accept())
    pg.add_init_script(MOCK)
    pg.goto('http://localhost:8766/control.html?lang=es')
    pg.click('#conectar')
    pg.wait_for_timeout(1500)
    check('0.6.1-beta' in pg.inner_text('#estado'), 'conecta y lee la versión')
    check('MY PAD' in pg.inner_text('#lsnd') and '1.5 s' in pg.inner_text('#ltak'), 'lista tomas y sonidos')
    check('41 %' in pg.inner_text('#mcpu') and '70 KiB' in pg.inner_text('#mram'), 'memoria: CPU y RAM')
    check('núcleo 1' in pg.inner_text('#avisos'), 'aviso de dos núcleos')
    # una toma de 4 s: caben 3 s (la RAM de grabación)
    pg.set_input_files('#ftak', '/src/build/prueba.wav')
    pg.wait_for_timeout(1500)
    check('3.072' in pg.input_value('#tdur'), 'propone lo que cabe (%s s)' % pg.input_value('#tdur'))
    pg.fill('#tbpm', '128')
    pg.click('#tsend')
    pg.wait_for_timeout(6000)
    st = pg.evaluate('window.__fm1.lastTake')
    check(st and st['len'] == 49152 and st['bpm'] == 1280, 'manda la toma con su largo y BPM: %s' % st)
    check('Toma enviada' in pg.inner_text('#log'), 'mensaje de toma enviada')
    pg.wait_for_timeout(2500)
    check('4.6 / 6.0 s' in pg.inner_text('#mtak'), 'la barra de tomas: %s' % pg.inner_text('#mtak'))
    # memoria baja: aviso rojo
    pg.evaluate('window.__fm1.heap = 20 * 1024; window.__fm1.cpu = 90')
    pg.wait_for_timeout(2600)
    av = pg.inner_text('#avisos')
    check('puede colgarse' in av, 'aviso de RAM baja')
    # sonando: no deja escribir
    pg.evaluate('window.__fm1.playing = 1')
    pg.wait_for_timeout(2600)
    check(pg.is_disabled('#les') and 'Sonando' in pg.inner_text('#avisos'), 'sonando: avisa y no deja escribir')
    pg.evaluate('window.__fm1.playing = 0')
    pg.wait_for_timeout(2600)
    pg.click('#lsnd button.del')
    pg.wait_for_timeout(2500)
    check(pg.evaluate('window.__fm1.snds.length') == 0, 'borra un sonido del usuario')
    pg.click('#les')
    pg.wait_for_timeout(4000)
    check('Idioma cargado' in pg.inner_text('#log'), 'carga el español')
    pg.screenshot(path='/src/build/control.png', full_page=True)
    b.close()
srv.terminate()
print('errores de consola:', errors or 'ninguno')
sys.exit(1 if fails or errors else 0)
