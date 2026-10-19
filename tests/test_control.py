# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Prueba de la plataforma de control (web/control.html) con el FM-1 de mentira (?mock=1).

  py -3 tests/test_control.py [--shots carpeta] [--chrome ruta]

Abre Chrome sin ventana (--headless=new) y lo maneja por el protocolo de DevTools (pip install
websocket-client). Sirve web/ en un puerto libre. Recorre: conectar, agregar una toma, un sonido y el
idioma, la memoria, borrar, mandar (esperando STOP), lo comprobado, lo que no cabe y descartar; y
guarda capturas en español e inglés, a 1280 y a 390 px, conectado y sin conectar.
"""
import argparse, base64, functools, http.server, json, math, os, random, shutil, socket, struct, subprocess, sys
import tempfile, threading, time, urllib.request, wave

import websocket  # websocket-client

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, 'web')
CHROMES = [os.environ.get('CHROME', ''), r'C:\Program Files\Google\Chrome\Application\chrome.exe',
           r'C:\Program Files (x86)\Google\Chrome\Application\chrome.exe', '/usr/bin/google-chrome', '/usr/bin/chromium',
           '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome']


def libre():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Callado(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


def servir(port):
    h = functools.partial(Callado, directory=WEB)
    srv = http.server.ThreadingHTTPServer(('127.0.0.1', port), h)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


class Pagina:
    """Una pestaña de Chrome por DevTools: evaluar, esperar, archivos, capturas."""

    def __init__(self, ws_url):
        self.ws = websocket.create_connection(ws_url, timeout=30)
        self.n = 0
        self.errores = []
        for m in ('Page.enable', 'Runtime.enable', 'DOM.enable', 'Log.enable'):
            self.cmd(m)

    def cmd(self, method, **params):
        self.n += 1
        mid = self.n
        self.ws.send(json.dumps({'id': mid, 'method': method, 'params': params}))
        while True:
            m = json.loads(self.ws.recv())
            if m.get('id') == mid:
                if 'error' in m:
                    raise RuntimeError('%s: %s' % (method, m['error']))
                return m.get('result', {})
            self.evento(m)

    def evento(self, m):
        meth = m.get('method')
        if meth == 'Page.javascriptDialogOpening':        # el aviso de cambios sin mandar al irse
            self.n += 1
            self.ws.send(json.dumps({'id': self.n, 'method': 'Page.handleJavaScriptDialog', 'params': {'accept': True}}))
        elif meth == 'Runtime.exceptionThrown':
            d = m['params']['exceptionDetails']
            self.errores.append((d.get('exception') or {}).get('description') or d.get('text'))
        elif meth == 'Runtime.consoleAPICalled' and m['params']['type'] == 'error':
            self.errores.append(' '.join(str(a.get('value', a.get('description'))) for a in m['params']['args']))
        elif meth == 'Log.entryAdded' and m['params']['entry']['level'] == 'error':
            e = m['params']['entry']
            if 'favicon' not in e.get('url', ''):
                self.errores.append(e['text'] + ' ' + e.get('url', ''))

    def js(self, expr):
        r = self.cmd('Runtime.evaluate', expression=expr, awaitPromise=True, returnByValue=True)
        if 'exceptionDetails' in r:
            raise RuntimeError('js: %s -> %s' % (expr[:80], r['exceptionDetails'].get('exception', {}).get('description')))
        return r['result'].get('value')

    def esperar(self, expr, ms=8000, que=''):
        t0 = time.time()
        while time.time() - t0 < ms / 1000:
            try:
                if self.js(expr):
                    return True
            except RuntimeError:
                pass
            time.sleep(0.1)
        print('     (no llegó: %s)' % (que or expr))
        return False

    def ir(self, url, ancho, alto=900, movil=False):
        self.cmd('Emulation.setDeviceMetricsOverride', width=ancho, height=alto, deviceScaleFactor=1, mobile=movil)
        self.cmd('Page.navigate', url=url)
        self.esperar('document.readyState === "complete"', 10000)
        time.sleep(0.3)

    def archivo(self, selector, path):
        doc = self.cmd('DOM.getDocument', depth=-1)
        node = self.cmd('DOM.querySelector', nodeId=doc['root']['nodeId'], selector=selector)['nodeId']
        self.cmd('DOM.setFileInputFiles', files=[path], nodeId=node)

    def captura(self, path, ancho, movil=False):
        self.js('window.scrollTo(0, 0)')
        h = self.js('Math.ceil(document.documentElement.scrollHeight)')
        self.cmd('Emulation.setDeviceMetricsOverride', width=ancho, height=min(h, 6000), deviceScaleFactor=1, mobile=movil)
        time.sleep(0.4)
        d = self.cmd('Page.captureScreenshot', format='png')
        with open(path, 'wb') as f:
            f.write(base64.b64decode(d['data']))
        self.cmd('Emulation.setDeviceMetricsOverride', width=ancho, height=900, deviceScaleFactor=1, mobile=movil)


def wav_loop(path, secs=2.0, bpm=120.0, rate=44100):
    """un loop de prueba: un golpe en cada negra sobre un tono suave"""
    rnd = random.Random(1)
    n, beat = int(secs * rate), 60.0 / bpm
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        fr = bytearray()
        for i in range(n):
            t = i / rate
            k = t % beat
            v = 0.15 * math.sin(2 * math.pi * 110 * t) + (0.8 * math.exp(-k * 40) * (rnd.random() * 2 - 1) if k < 0.08 else 0)
            fr += struct.pack('<h', int(max(-1, min(1, v)) * 30000))
        w.writeframes(bytes(fr))


def syx_bank(path):
    """un banco de 32 voces de la DX7 (todas iguales, con nombre), con su suma"""
    data = bytearray()
    for v in range(32):
        voz = bytearray(128)
        for op in range(6):
            voz[op * 17:op * 17 + 4] = bytes([99, 50, 35, 50])
            voz[op * 17 + 4:op * 17 + 8] = bytes([99, 80, 60, 0])
            voz[op * 17 + 14] = 90 if op == 5 else 70
        voz[110] = 4
        voz[118:128] = ('TEST %02d   ' % (v + 1)).encode()[:10]
        data += voz
    s = (128 - (sum(data) & 0x7F)) & 0x7F
    open(path, 'wb').write(bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]) + bytes(data) + bytes([s, 0xF7]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--shots', default=os.path.join(ROOT, 'build', 'control'))
    ap.add_argument('--chrome', default='')
    a = ap.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    chrome = next((c for c in [a.chrome] + CHROMES if c and os.path.isfile(c)), None)
    if not chrome:
        sys.exit('no encuentro Chrome (usa --chrome o la variable CHROME)')
    os.makedirs(a.shots, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='bm-control-')
    wav = os.path.join(tmp, 'loop 120bpm.wav')
    wav_loop(wav)
    largo = os.path.join(tmp, 'long.wav')
    wav_loop(largo, secs=4.0, bpm=100)
    syx = os.path.join(tmp, 'test bank.syx')
    syx_bank(syx)
    vital = os.path.join(tmp, 'dark rave.vital')
    shutil.copy(os.path.join(ROOT, 'assets', 'presets', 'buno', 'dark-rave.vital'), vital)

    port, dbg = libre(), libre()
    srv = servir(port)
    base = 'http://127.0.0.1:%d/' % port
    prof = os.path.join(tmp, 'perfil')
    ch = subprocess.Popen([chrome, '--headless=new', '--remote-debugging-port=%d' % dbg, '--user-data-dir=' + prof,
                           '--no-first-run', '--no-default-browser-check', '--autoplay-policy=no-user-gesture-required',
                           '--remote-allow-origins=*', 'about:blank'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    fails = []

    def check(c, what):
        print(('ok   ' if c else 'MAL  ') + what)
        if not c:
            fails.append(what)

    try:
        ws = None
        for _ in range(50):
            try:
                ws = next(t['webSocketDebuggerUrl'] for t in json.load(urllib.request.urlopen('http://127.0.0.1:%d/json' % dbg))
                          if t['type'] == 'page')
                break
            except Exception:
                time.sleep(0.2)
        p = Pagina(ws)
        shot = lambda name, w, movil=False: p.captura(os.path.join(a.shots, name), w, movil)
        txt = lambda sel: p.js('(document.querySelector(%s) || {}).innerText || ""' % json.dumps(sel)).replace(' ', ' ')
        raw = lambda sel: p.js('(document.querySelector(%s) || {}).textContent || ""' % json.dumps(sel)).replace(' ', ' ')   # sin mayúsculas de CSS

        # ------------------------------------------------ sin conectar
        p.ir(base + 'control.html?lang=es', 1280)
        check('Sin conectar' in txt('#estado'), 'sin FM-1: dice "Sin conectar"')
        check(not p.js('"bmMock" in window'), 'sin ?mock=1 no carga el FM-1 de mentira')
        shot('es-1280-sin-conectar.png', 1280)
        p.ir(base + 'control.html?lang=en', 390, movil=True)
        check('Not connected' in txt('#estado') and txt('h1') == 'FM-1 control', 'EN: estado y título en inglés')
        check(p.js('document.querySelector("nav a[href=\'en/docs/manual.html\']") !== null'), 'EN: el menú lleva al manual en inglés')
        check(p.js('document.documentElement.scrollWidth <= 390'), 'EN 390 px: sin barra horizontal')
        shot('en-390-sin-conectar.png', 390, True)

        # sin conectar se puede preparar: una toma queda pendiente y la memoria lo cuenta
        p.ir(base + 'control.html?lang=es', 1280)
        p.archivo('#ftake', wav)
        p.esperar('!document.getElementById("tok").disabled', 10000, 'editor de toma (sin conectar)')
        p.js('document.getElementById("tok").click()')
        check('1 cambio pendiente' in txt('#pcount') and p.js('document.getElementById("send").disabled'),
              'sin conectar: queda pendiente y Mandar está apagado')
        check('Conecta el FM-1 para mandarlos' in txt('#why'), 'sin conectar: dice por qué no se manda')

        # ------------------------------------------------ conectado (mock)
        p.ir(base + 'sonidos.html?lang=es&mock=1', 1280)
        p.esperar('location.pathname.endsWith("control.html")', 5000)
        check(p.js('location.pathname.endsWith("control.html") && location.hash === "#sounds" && location.search.includes("mock=1")'),
              'sonidos.html lleva a control.html#sounds con la consulta')
        p.esperar('document.getElementById("estado").innerText.includes("0.9.0-beta")', 8000, 'conectado')
        check('FM-1 · Bubba Machine 0.9.0-beta' in txt('#estado'), 'conecta y lee la versión')
        p.esperar('document.getElementById("slots").innerText.includes("1.49")', 5000)
        check('1.49 s' in txt('#slots') and 'TAKE 03' in txt('#slots') and txt('#slots').count('Libre') == 5, 'TAKES: 3 tomas y 5 lugares libres')
        m = txt('#machines')
        check('GLASS PAD' in m and 'FAT LEAD' in m and 'MY DX BANK' in m and 'Solo sonidos de fábrica' in m, 'SOUNDS: los del usuario por máquina; BB-303 de fábrica')
        check('Tus sonidos: 3 de 12' in txt('#scount'), 'SOUNDS: el tope de 12')
        check('3.2 / 6.0 s' in txt('#mtxt') and '32 KB' in txt('#mtxt'), 'memoria: tomas y libre (%s)' % txt('#mtxt').replace('\n', ' | '))
        check('Hay una versión más nueva' not in txt('#bar'), 'la versión instalada es la última: sin aviso')
        p.js('document.getElementById("device").open = true')
        p.esperar('document.getElementById("dinfo").innerText.includes("Bien")', 3000)
        d = txt('#dinfo')
        check('Se prendió normalmente' in d and 'TWO CORES' not in d and 'batería 3.95 V' in d, 'DEVICE: arranque, tensión y sin TWO CORES')
        p.js('document.getElementById("dlog").click()')
        p.esperar('!document.getElementById("logtxt").hidden', 3000)
        check('Bubba Machine 0.9.0-beta' in txt('#logtxt'), 'DEVICE: el registro')
        p.js('document.getElementById("device").open = false')
        shot('es-1280-conectado.png', 1280)

        # una toma: 2 s a 120 BPM (del nombre), 1 compás
        p.archivo('#ftake', wav)
        p.esperar('!document.getElementById("tok").disabled', 10000, 'editor de toma')
        check(abs(float(p.js('document.getElementById("tbpm").value')) - 120) < 0.01, 'toma: el BPM del nombre')
        check('2.00' == p.js('document.getElementById("tlen").value'), 'toma: dura lo que cabe (%s)' % p.js('document.getElementById("tlen").value'))
        check('compás' in txt('#tbars') and 'TAKE 04' in txt('#tnote'), 'toma: compases y a qué lugar va (%s)' % txt('#tnote'))
        p.js('document.getElementById("tplay").click()')
        time.sleep(0.3)
        p.js('document.getElementById("tplay").click()')
        shot('es-1280-editor-toma.png', 1280)
        p.js('document.getElementById("tok").click()')
        check('Pendiente · nueva' in raw('#slots') and '1 cambio pendiente' in txt('#pcount'), 'toma pendiente en TAKE 04')
        check('+32 KB' in txt('#mtxt'), 'memoria: lo pendiente (%s)' % txt('#mtxt').replace('\n', ' | '))

        # un sonido de Vital: cambiar de máquina y de nombre lo convierte de nuevo
        p.archivo('#fsnd', vital)
        p.esperar('!document.getElementById("sed").hidden && document.getElementById("snote").innerText.length > 0', 8000, 'editor de sonido')
        a1 = txt('#snote')
        check(a1.startswith('BITTAL'), 'sonido: un .vital va a BITTAL si no eliges otra máquina')
        p.js('const s = document.getElementById("smach"); s.value = "B-MINI"; s.dispatchEvent(new Event("change"))')
        time.sleep(0.3)
        a2 = txt('#snote')
        check(a2.startswith('B-MINI') and a1 != a2, 'sonido: cambiar la máquina lo convierte de nuevo (%s → %s)' % (a1[:20], a2[:20]))
        p.js('const n = document.getElementById("sname"); n.value = "my lead"; n.dispatchEvent(new Event("input"))')
        time.sleep(0.5)
        check('MY LEAD' == p.js('document.getElementById("sname").value') and a2.split('·')[1] != txt('#snote').split('·')[1],
              'sonido: cambiar el nombre cambia la huella')
        check('aproximado' in txt('#srep'), 'sonido: el informe de la conversión')
        p.js('document.getElementById("sok").click()')
        check('MY LEAD' in txt('.mach[data-mach="B-MINI"]'), 'sonido pendiente en B-MINI')
        # un banco de la DX7 soltado en la columna de BITTAL va a BX-7
        p.js('document.querySelector("[data-spick=BITTAL]").click()')
        p.archivo('#fsnd', syx)
        p.esperar('document.getElementById("snote").innerText.startsWith("BX-7")', 5000, 'banco .syx')
        check('TEST 01' in txt('#srep') and 'va a BX-7' in txt('#snote'), 'banco .syx en la columna de BITTAL: va a BX-7 con sus 32 voces')
        p.js('document.getElementById("scancel").click()')

        # el idioma
        p.js('document.querySelector("input[name=lang][value=es]").click()')
        p.esperar('document.getElementById("pcount").innerText.includes("3 cambios")', 3000, 'idioma pendiente')
        check('Pendiente' in raw('#lopts'), 'idioma pendiente')

        # borrar una toma y un sonido (pendiente, sin preguntar)
        p.js('document.querySelector("[data-tdel=\'1\']").click()')
        p.js('document.querySelector("[data-sdel=\'0\']").click()')
        check('5 cambios pendientes' in txt('#pcount') and 'Pendiente · reemplaza' in raw('#slots'), 'borrar queda pendiente; la toma nueva ocupa ese lugar (%s)' % txt('#pcount'))
        check('−' in txt('#mtxt'), 'memoria: lo que se libera')
        p.js('document.getElementById("plist").open = true')
        shot('es-1280-pendientes.png', 1280)
        p.ir(base + 'control.html?lang=es&mock=1', 390, movil=True)
        p.esperar('document.getElementById("estado").innerText.includes("0.9.0")', 8000)
        check(p.js('document.documentElement.scrollWidth <= 390'), 'ES 390 px: sin barra horizontal')
        shot('es-390-conectado.png', 390, True)

        # de vuelta en escritorio: armar todo y mandar, primero sonando
        p.ir(base + 'control.html?lang=en&mock=1', 1280)
        p.esperar('document.getElementById("estado").innerText.includes("0.9.0")', 8000)
        p.archivo('#ftake', wav)
        p.esperar('!document.getElementById("tok").disabled', 10000)
        p.js('document.getElementById("tok").click()')
        p.archivo('#fsnd', vital)
        p.esperar('!document.getElementById("sed").hidden && document.getElementById("snote").innerText.length > 0', 8000)
        p.js('document.getElementById("sok").click()')
        p.js('document.querySelector("input[name=lang][value=es]").click()')
        p.esperar('document.getElementById("pcount").innerText.includes("3 pending")', 3000)
        p.js('document.querySelector("[data-tdel=\'1\']").click()')
        p.js('document.querySelector("[data-sdel=\'0\']").click()')
        check('5 pending changes' in txt('#pcount'), 'EN: 5 pending')
        p.js('bmMock.playing = 1')
        time.sleep(2.3)
        p.js('document.getElementById("send").click()')
        p.esperar('document.getElementById("why").innerText.includes("Press STOP")', 4000, 'pide STOP')
        check('Press STOP on the FM-1' in txt('#why'), 'sonando: pide STOP y espera')
        shot('en-1280-esperando-stop.png', 1280)
        p.js('bmMock.playing = 0; bmMock.busy = 1; setTimeout(() => { bmMock.busy = 0; }, 1500)')
        ok = p.esperar('document.getElementById("pcount").innerText.includes("No pending")', 40000, 'mandado')
        check(ok and 'Done: 5 changes verified on the FM-1' in txt('#why'), 'manda todo y lo comprueba (%s)' % txt('#why'))
        st = p.js('({takes: bmMock.takes.map(t => t && t.len), snds: bmMock.snds.map(s => s.name), lang: bmMock.lang && bmMock.lang.name, refused: bmMock.refused.length})')
        check(st['takes'][1] == 32000 and st['takes'][3] is None and 16000 not in st['takes'],
              'en el FM-1: TAKE 02 borrada primero y la toma nueva en su lugar (%s)' % st['takes'])
        check('GLASS PAD' not in st['snds'] and len(st['snds']) == 3 and st['lang'] == 'ESPANOL', 'en el FM-1: sonidos e idioma (%s)' % st)
        check(st['refused'] > 0, 'reintenta cuando el FM-1 está ocupado (%d rechazos)' % st['refused'])
        p.esperar('document.getElementById("slots").textContent.includes("Verified")', 3000)
        check('Verified on the FM-1' in raw('#slots') and 'Verified on the FM-1' in raw('#machines') and 'Verified on the FM-1' in raw('#lopts'),
              'marca lo comprobado en cada sección')
        p.js('document.getElementById("plist").open = true')
        shot('en-1280-comprobado.png', 1280)

        # lo que no cabe: tres tomas de 3 s pasan los 6 s
        for _ in range(3):
            p.archivo('#ftake', largo)
            p.esperar('!document.getElementById("tok").disabled && !document.getElementById("ted").hidden', 10000)
            p.js('document.getElementById("tlen").value = "3.07"; document.getElementById("tlen").dispatchEvent(new Event("change"))')
            p.js('document.getElementById("tok").click()')
        check(p.js('document.getElementById("send").disabled') and 'doesn' in txt('#why'), 'lo que no cabe se marca y no deja mandar (%s)' % txt('#why'))
        check("Doesn't fit" in raw('#takes'), 'TAKES: marcado en rojo')
        shot('en-1280-no-cabe.png', 1280)
        # descartar (pide confirmar en el mismo botón)
        p.js('document.getElementById("discard").click()')
        check('Yes, discard all 3' in txt('#discard'), 'descartar pide confirmar')
        p.js('document.getElementById("discard").click()')
        check('No pending' in txt('#pcount'), 'descartar vacía la lista')
        p.ir(base + 'control.html?lang=en&mock=1', 390, movil=True)
        p.esperar('document.getElementById("estado").innerText.includes("0.9.0")', 8000)
        shot('en-390-conectado.png', 390, True)
        errores = [e for e in p.errores if e and 'beforeunload' not in e]   # Chrome avisa que no mostró el aviso de cambios sin mandar
        check(not errores, 'sin errores en la consola: %s' % (errores or 'ninguno'))
    finally:
        ch.terminate()
        srv.shutdown()
        time.sleep(0.5)
        shutil.rmtree(tmp, ignore_errors=True)
    print('capturas en', a.shots)
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
