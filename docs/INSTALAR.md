# Instalar Bubba Machine

## Estado actual: beta (0.6.1-beta)

Bubba Machine para el FM-1 tiene **firmware completo, con sonido**, un **firmware de diagnóstico** y un **instalador web**. Corre en el FM-1 del autor, pero es una **beta**: se probó en un solo aparato y todavía cambia mucho. **La 0.6.1-beta todavía no se probó en el aparato** (pasa las pruebas automáticas y el emulador; la 0.6.0-beta no debe instalarse: le faltaba RAM); si algo falla, se vuelve a la 0.5.0-beta o al firmware de fábrica desde el instalador. El avance técnico está en [`hal/fm1/README_PORT.md`](../hal/fm1/README_PORT.md).

> **Lo que guardes puede perderse al actualizar.** Hasta la v1 no hay compatibilidad hacia atrás: si una versión nueva cambia cómo se guardan los patrones, la canción, los ajustes o las tomas, al primer arranque lo anterior se borra y vuelve la demo. Si algo te importa, grábalo por la salida de audio antes de actualizar.

Cada versión también se publica como `bubba-machine-<versión>.fwsc` en las [releases de GitHub](https://github.com/Erbubar23/bubba-machine/releases), para los instaladores que usan ese paquete (como el [FM-1 Firmware Hub](https://fm1.designburgapps.com/)).

Si no quieres arriesgar tu FM-1, usa el emulador (abajo): corre el mismo código.

## Mientras tanto: el emulador

1. Abre el [emulador](https://erbubar23.github.io/bubba-machine/emulador.html) en Chrome o Edge.
2. Pulsa **Encender**. Arranca con una demo.
3. Con el teclado de la computadora:
   - **Espacio** es PLAY, **Enter** es REC y **Shift** es HOME (el modificador: mantenlo y gira SELECT para ir a otro canal).
   - Los números **1-9 y 0** son HOME, SEQ, EDIT, ENV, FX, SCL, ARP, SAVE, LFO y GLO, y las **flechas** son OCT− y OCT+.
   - Las filas **Z-/** y **Q-P** tocan las teclas del FM-1.
   - La **rueda del mouse** sobre una perilla la gira.
4. En **Sampler y memoria** puedes cargar un WAV de hasta 3 s como toma nueva.

El emulador corre exactamente el mismo código C que irá en el aparato.

## Cómo se instala

Es **plug and play**: hace falta una computadora con **Chrome o Edge** (Web MIDI) y un **cable USB de datos**. Nada más.

1. Conecta el FM-1 por USB y préndelo. Puede tener el firmware de fábrica, X0X o Bubba Machine. Cierra cualquier otro programa que use su MIDI.
2. Abre el **instalador** (`instalar.html` del sitio) y acepta el permiso de MIDI. La página **lee el FM-1 sola** y dice qué firmware tiene.
3. La página ya eligió la versión más nueva de **Bubba Machine** (también puedes elegir el **Diagnóstico**, sin sonido, para comprobar botones, teclas y perillas, o el **Firmware de fábrica** de M-VAVE).
4. Pulsa **Instalar** y confirma. No desconectes hasta que diga «Listo».
5. Después, la [plataforma de control](../web/control.html) carga el idioma de la pantalla, las tomas y los sonidos, y muestra la memoria.

Si la página no encuentra el FM-1, debajo de «Tu FM-1» muestra qué puertos MIDI ve y qué les pasa (por ejemplo, «ocupado» si REAPER u otro programa lo tiene abierto).

La página baja el **mismo paquete `.fwsc` de la release** (el que usa el FM-1 Firmware Hub) y comprueba su SHA-256 antes de escribir nada.

El **FM-1.fwsc de M-VAVE** (FM-1 V15) solo hace falta para **volver al firmware de fábrica** o para una versión vieja sin paquete publicado: ahí la página lo pide (lo publica M-VAVE en su sitio), comprueba que sea exactamente ese archivo y el navegador lo recuerda para la próxima vez.

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
