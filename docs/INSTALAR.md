# Instalar Bubba Machine

## Estado actual: en pruebas, todavía sin sonido

Hay un **firmware de diagnóstico** para el FM-1 y un **instalador web**. Ninguno de los dos se ha probado aún en un aparato: la primera prueba la hace el autor. El firmware con sonido llega cuando el diagnóstico confirme la pantalla, los botones y el USB. El avance técnico está en [`hal/fm1/README_PORT.md`](../hal/fm1/README_PORT.md).

Si no quieres arriesgar tu FM-1, usa el emulador (abajo): corre el mismo código.

## Mientras tanto: el emulador

1. Abre el [emulador](https://erbubar23.github.io/bubba-machine/emulador.html) en Chrome o Edge.
2. Pulsa **Encender**. Arranca con una demo.
3. Con el teclado de la computadora:
   - **Espacio** es PLAY, **Enter** es REC y **Shift** es HOME (el modificador: mantenlo y gira ALGORITHM para ir a otro canal).
   - Los números **1-9 y 0** son HOME, SEQ, EDIT, ENV, FX, SCL, ARP, SAVE, LFO y GLO, y las **flechas** son OCT− y OCT+.
   - Las filas **Z-/** y **Q-P** tocan las teclas del FM-1.
   - La **rueda del mouse** sobre una perilla la gira.
4. En **Sampler y memoria** puedes cargar un WAV de hasta 3 s como toma nueva.

El emulador corre exactamente el mismo código C que irá en el aparato.

## Cómo se instala

Hace falta una computadora con **Chrome o Edge** (Web MIDI), un **cable USB de datos** y el archivo **FM-1.fwsc** (FM-1 V15) que M-VAVE publica en su sitio.

1. Abre el **instalador**: `instalar.html` del sitio. Para una versión armada por ti, sirve la carpeta `web/` en tu computadora (`py -3 -m http.server` dentro de `web/`) y abre `http://localhost:8000/instalar.html`.
2. Conecta el FM-1 y pulsa **Buscar el FM-1**. Puede tener el firmware de fábrica, X0X o Bubba Machine.
3. Elige el **FM-1.fwsc de M-VAVE**. La página comprueba que sea exactamente ese archivo.
4. Elige la **versión de Bubba Machine**. La página **arma el paquete en tu navegador** sobre el archivo de M-VAVE: este sitio solo publica lo nuestro, nunca lo de M-VAVE.
5. Marca que entiendes los riesgos y pulsa **Instalar Bubba Machine**. No desconectes hasta que diga «Listo».

Cómo funciona por dentro:

- El firmware que corre prepara el **cargador de actualización oficial de M-VAVE** y reinicia en él.
- Ese cargador escribe el programa nuevo.
- El arranque del chip y los datos del aparato (por encima de `0xED000`) no se tocan.
- El paquete se niega a instalarse si el arranque del aparato no es idéntico al del archivo.

## Volver al firmware de fábrica

Hay tres caminos, del más cómodo al de emergencia:

- **Desde el mismo instalador**: elige el FM-1.fwsc y pulsa **Volver al de fábrica**.
- **Con M-UPGRADE**, el actualizador oficial de M-VAVE. Bubba Machine se presenta como la versión 3 del FM-1 y habla el mismo protocolo, así que M-UPGRADE debería ofrecer la V15 (todavía sin probar).
- **Desde X0X o su modo seguro**, con su propio instalador.

## Si algo sale mal

- **Se cortó la instalación** (el FM-1 aparece como «ota-FM-1»): vuelve a conectar, elige los mismos archivos y pulsa **Continuar**. El FM-1 se queda en ese modo hasta terminar.
- **Modo seguro:** si Bubba Machine no llega a arrancar bien dos veces seguidas, arranca sin sonido y con el USB activo, para reinstalar o volver al de fábrica. También se entra prendiéndolo con OCT− y OCT+ apretados.
- **Modo del chip:** después de cuatro arranques fallidos, o con OCT− y OCT+ juntos 5 s, el FM-1 entra en el modo de actualización propio del chip y no aparece como MIDI. Desde una consola de Windows **abierta como administrador**:

  ```
  py -3 tools/fm1/rescate.py respaldo
  py -3 tools/fm1/rescate.py restaurar FM-1.fwsc
  ```

  Siempre guarda primero un respaldo completo de la flash. Después solo escribe la zona del programa, y comprueba lo escrito.
- **Nada de lo anterior responde:** queda el *FM-1 Transporter*, una placa RP2040 conectada a las líneas USB.

## Seguridad

Instalar firmware no oficial **puede dejar tu FM-1 inservible** y probablemente anula la garantía. Este software se entrega sin garantía (ver [licencia](../LICENSE), secciones 15 y 16). Mientras el [registro de cambios](../CHANGELOG.md) no marque una versión como **probada en hardware**, es una prueba.
