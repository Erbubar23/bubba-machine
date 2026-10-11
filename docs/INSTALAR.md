# Instalar Bubba Machine

## Estado actual: en pruebas

Bubba Machine para el FM-1 ya tiene **firmware completo, con sonido**, un **firmware de diagnóstico** y un **instalador web**. Todavía **no se ha probado en un aparato**: la primera prueba la hace el autor. El avance técnico está en [`hal/fm1/README_PORT.md`](../hal/fm1/README_PORT.md).

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

1. Conecta el FM-1 por USB y préndelo. Puede tener el firmware de fábrica, X0X o Bubba Machine. Cierra cualquier otro programa que use su MIDI.
2. Abre el **instalador** (`instalar.html` del sitio) y acepta el permiso de MIDI. La página **lee el FM-1 sola** y dice qué firmware tiene.
3. Elige qué instalar: **Bubba Machine**, el **Diagnóstico** (sin sonido, para comprobar botones, teclas y perillas) o el **Firmware de fábrica** de M-VAVE.
4. Pulsa el botón y confirma. No desconectes hasta que diga «Listo».

Si la página no encuentra el FM-1, debajo de «Tu FM-1» muestra qué puertos MIDI ve y qué les pasa (por ejemplo, «ocupado» si REAPER u otro programa lo tiene abierto).

La primera vez, la página pide el **FM-1.fwsc de M-VAVE** (FM-1 V15), que M-VAVE publica en su sitio: el paquete se **arma en tu navegador** sobre ese archivo, porque este sitio solo publica lo nuestro, nunca lo de M-VAVE. Se comprueba que sea exactamente ese archivo, y el navegador lo recuerda para la próxima vez.

**Desde una copia del repositorio** (por ejemplo, para una versión armada por ti): `py -3 tools/fm1/servir.py` abre el instalador en `http://localhost:8765/instalar.html` y le entrega el V15 directamente (lo toma de `~/jieli/stock/FM-1.fwsc` o lo descarga de M-VAVE una vez), así no hay que elegir nada. En **Más opciones** se puede instalar un archivo propio.

Cómo funciona por dentro:

- El firmware que corre prepara el **cargador de actualización oficial de M-VAVE** y reinicia en él.
- Ese cargador escribe el programa nuevo.
- El arranque del chip y los datos del aparato (por encima de `0xED000`) no se tocan.
- El paquete se niega a instalarse si el arranque del aparato no es idéntico al del archivo.

## Volver al firmware de fábrica

Hay tres caminos, del más cómodo al de emergencia:

- **Desde el mismo instalador**: elige **Firmware de fábrica** y pulsa **Recuperar el firmware de fábrica**.
- **Con M-UPGRADE**, el actualizador oficial de M-VAVE. Bubba Machine se presenta como la versión 3 del FM-1 y habla el mismo protocolo, así que M-UPGRADE debería ofrecer la V15 (todavía sin probar).
- **Desde X0X o su modo seguro**, con su propio instalador.

## Si algo sale mal

- **Se cortó la instalación** (el FM-1 aparece como «ota-FM-1»): vuelve a conectarlo con el instalador abierto: el botón cambia a **Continuar la instalación**. El FM-1 se queda en ese modo hasta terminar.
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
