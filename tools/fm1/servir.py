#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
"""Sirve el instalador web en esta computadora y lo abre en el navegador.

  py -3 tools/fm1/servir.py [--puerto 8765] [--no-abrir]

Sirve la carpeta web/ solo para esta computadora (127.0.0.1). Además entrega el FM-1 V15 de
M-VAVE en fm1/local/FM-1.fwsc, para que el instalador lo tenga sin elegirlo a mano:
  - de ~/jieli/stock/FM-1.fwsc (o la variable FM1_V15) si existe;
  - si no, lo descarga de M-VAVE una vez y lo guarda ahí.
En los dos casos se comprueba que sea exactamente el V15 (SHA-256). Ese archivo es de M-VAVE:
nunca se copia a web/ ni se sube al repositorio; solo pasa de esta computadora a su navegador.
"""
import argparse, functools, hashlib, http.server, os, sys, urllib.request, webbrowser

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WEB = os.path.join(ROOT, 'web')
FM1_V15 = os.environ.get('FM1_V15', os.path.join(os.path.expanduser('~'), 'jieli', 'stock', 'FM-1.fwsc'))
V15_URL = 'https://yms-file-store.oss-cn-hongkong.aliyuncs.com/software/firmware/FM-1.fwsc'
V15_SHA256 = 'db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a'
V15_RUTA = '/fm1/local/FM-1.fwsc'


def v15():
    """los bytes del V15, comprobados; None si no se pudo conseguir"""
    if os.path.isfile(FM1_V15):
        d = open(FM1_V15, 'rb').read()
        if hashlib.sha256(d).hexdigest() == V15_SHA256:
            return d
        print('aviso: %s no es el FM-1 V15 de M-VAVE (SHA-256 distinto); no se usa' % FM1_V15)
        return None
    try:
        print('descargando el FM-1 V15 de M-VAVE…')
        d = urllib.request.urlopen(V15_URL, timeout=60).read()
    except OSError as e:
        print('no se pudo descargar el V15: %s' % e)
        return None
    if hashlib.sha256(d).hexdigest() != V15_SHA256:
        print('aviso: lo que publica M-VAVE ya no es el V15 conocido; el instalador lo pedirá a mano')
        return None
    os.makedirs(os.path.dirname(FM1_V15), exist_ok=True)
    with open(FM1_V15, 'wb') as f:
        f.write(d)
    print('guardado en %s' % FM1_V15)
    return d


class Pagina(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache')
        super().end_headers()

    def do_GET(self):
        if self.path.split('?')[0] != V15_RUTA:
            return super().do_GET()
        d = v15()
        if d is None:
            return self.send_error(404, 'sin el FM-1 V15')
        self.send_response(200)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(len(d)))
        self.end_headers()
        self.wfile.write(d)

    def log_message(self, fmt, *args):
        pass


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--puerto', type=int, default=8765)
    ap.add_argument('--no-abrir', action='store_true', help='no abrir el navegador')
    a = ap.parse_args()
    srv = http.server.ThreadingHTTPServer(('127.0.0.1', a.puerto), functools.partial(Pagina, directory=WEB))
    url = 'http://localhost:%d/instalar.html' % a.puerto
    print('instalador en %s (Ctrl+C para terminar)' % url)
    if not a.no_abrir:
        webbrowser.open(url)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    sys.exit(main())
