# Port al FM-1

**Estado (0.4.0-beta, octubre de 2026):** compilan y se empaquetan dos firmwares, que corren en el FM-1 del autor (beta: un solo aparato probado):

- **Bubba Machine** (`fm1_main.c`): el núcleo completo con sonido por el codec (ALNK0), la matriz y los LEDs por interrupción, la pantalla, USB-MIDI, el guardado y el servicio de actualización. `app.bin` de 559 008 bytes (quedan 23 KB).
- **Diagnóstico** (`fm1_diag.c`): sin sonido, muestra cada botón, tecla y perilla con su cruce en la matriz. Sirve para comprobar el aparato o calibrar.

Regla del proyecto: el código es propio. Los drivers son los del **SDK AC79 de JieLi (Apache-2.0)**; de Felucca y X0X solo se toman hechos de hardware (pines, mapa de la matriz, direcciones verificadas), nunca su código.

## Cómo se arma

```
py -3 tools/build.py fm1 bubba     # Bubba Machine completo
py -3 tools/build.py fm1 diag      # firmware de diagnóstico
py -3 tools/fm1/servir.py          # el instalador en esta computadora, con el V15 ya cargado
```

Necesita, fuera del repositorio:

| Qué | Dónde | Para qué |
|---|---|---|
| Toolchain de JieLi (clang pi32v2) | volumen de Docker `jieli-opt` en `/opt/jieli` | compilar |
| SDK `AC79NN_SDK_V1.2.1` | `~/jieli/fw-AC79_AIoT_SDK` (o `AC79_SDK`) | arranque, sistema, USB, SPI, flash, ADC |
| `FM-1.fwsc` V15 de M-VAVE | `~/jieli/stock/FM-1.fwsc` (o `FM1_V15`) | base del paquete |

Sale `build/fm1/bubba-machine-<versión>.fwsc` (o `bubba-diag-…`), y `web/fm1/` recibe solo el `app.bin` con su lista `versiones.json`, que es lo que publica el sitio. **Ese archivo no se publica**: lleva el arranque y el cargador de M-VAVE. Cada quien lo arma con su propia copia del V15.

### El paquete (`tools/fm1/fwsc.py`)

Se arma sobre el V15 oficial cambiando solo `app.bin` dentro de la zona de la app, que se vuelve a cifrar con la llave del chip (0x980F). Se recalculan los CRC que cubren lo que cambió. El arranque del chip, `isd_config`, el cargador oficial (`ota.bin`), `USR`, `script.ver`, `blimit.bin` y `tail.bin` quedan idénticos.

Comprobaciones:

- `fwsc.py idavuelta`: el V15 desarmado y vuelto a armar con su propia app da el archivo oficial byte por byte.
- El desempaquetador independiente de jl-misctools (kagaimiq, MIT) lee los paquetes armados y extrae la app tal cual.
- El cargador oficial, en la reinstalación documentada por FM-1-RE, pide la cabecera, la zona de la app y `flash.bin` entero. No pide `tail.bin` ni `USR`.

También mueve la "VM" del SDK (su almacén de ajustes) a `0xE5000`, con su tamaño por defecto de 32 KiB, para que no se mezcle con los datos (ver el mapa).

## Mapa de la flash (1 MiB)

| Zona | Qué | Quién escribe |
|---|---|---|
| `0x000000-0x003FFF` | cabecera, arranque del chip (`uboot.boot`), `isd_config` | nadie: tiene que quedar el de fábrica |
| `0x004000-0x092FFF` | zona de la app (`app.bin`, `cfg_tool.bin`, `cfg`) | solo el cargador oficial al instalar |
| `0x097000-0x0BEFFF` | patrones (20 slots de 8 KiB) | el firmware |
| `0x0BF000` | registro de arranques | el firmware |
| `0x0C0000-0x0DCFFF` | tomas (116 KiB) | el firmware |
| `0x0DD000-0x0DFFFF` | ajustes globales | el firmware |
| `0x0E0000-0x0E4FFF` | cargador en espera durante una actualización | el servicio de actualización |
| `0x0E5000-0x0ECFFF` | VM del SDK | el SDK |
| `0x0ED000-0x0FFFFF` | BTIF, USR, MAC del aparato | **nadie** |

El driver de flash (`fm1_flash.c`) rechaza cualquier borrado o escritura fuera de `0x097000-0x0E4FFF`.

## Seguridad

- **Instalación**: siempre con el cargador oficial de M-VAVE, que ya está probado en el aparato. El firmware que corre (X0X, el de fábrica o el nuestro) lo deja listo y el cargador escribe. El instalador web (`web/instalar.html`) y el servicio propio (`fm1_update.c`) aceptan solo el cargador oficial y un arranque idéntico al del aparato. Hasta que el computador confirma, no se compromete nada.
- **Modo seguro**: dos arranques seguidos que no llegan a los 10 s (cuelgue, watchdog o apagarlo enseguida) arrancan sin sonido y con el USB activo, para reinstalar. También se entra prendiéndolo con OCT− y OCT+ apretados.
- **Modo del chip**: cuatro arranques fallidos seguidos, o OCT− y OCT+ juntos 5 s, llevan al modo de actualización propio del chip (USB `4C4A:8057`). Se entra escribiendo `usb_update_mode` en `0x01C7FD80` y reiniciando, dato verificado por X0X. Desde ahí, `tools/fm1/rescate.py` (Windows) hace un respaldo completo y vuelve a poner un firmware.
- **Lo que no cubre**: si el firmware se cae *antes* de llegar a `app_main` (en el arranque del SDK), no corre nada de lo anterior. Para eso queda el *FM-1 Transporter* (una placa RP2040 conectada a las líneas USB). El arranque del SDK es el mismo que usa el firmware de fábrica (mismo `startup.S` y mismo sistema), y el nuestro no llama a `power_init()`: la energía queda como la deja el arranque del chip.

## Cómo corre el firmware completo

| Dónde | Qué |
|---|---|
| Interrupción de ALNK0 (`fm1_audio.c`, prioridad 1) | el render del núcleo: 256 frames por medio buffer (5.8 ms), estéreo de 24 bits alineado a la izquierda |
| Interrupción de TIMER5 (`fm1_scanhw.c`, 4 kHz, prioridad 3) | una columna de la matriz por interrupción (≈364 barridos/s), antirrebote y perillas (`fm1_scan.c`, probado en PC) y los LEDs con brillo por tiempo encendido |
| Tarea `bubba` (`fm1_main.c`) | flancos y perillas hacia el núcleo, MIDI por USB en las dos direcciones, MASTER, `bm_app_tick` (dibuja lo que cambió y guarda), el servicio de actualización y OCT− + OCT+ 5 s al modo del chip |

Las perillas aprenden en qué estados descansan: cuentan un paso por detent tanto si el detent es un ciclo de cuadratura entero como si es medio.

Al empezar una actualización el firmware calla el audio y deja la pantalla al servicio; si el computador la cancela, sigue tocando.

## Primera prueba en el aparato

1. Desde el FM-1 con X0X (o con el firmware de fábrica): `py -3 tools/fm1/servir.py` abre `web/instalar.html` en `localhost` con el V15 ya cargado. La página lee el FM-1 sola; se elige **Bubba Machine** o **Diagnóstico** y se pulsa el botón.
2. Con Bubba Machine, qué mirar:
   - que arranque al logo y después a HOME;
   - que suene la demo con PLAY (por el altavoz y por el jack);
   - que los botones hagan lo que dice su serigrafía y las perillas giren para el lado correcto (si KNOB 1-4 están cambiadas, se corrige con el diagnóstico);
   - que los LEDs sigan al secuenciador.
3. Con el diagnóstico, qué mirar en la pantalla:
   - que arranque, con «ARRANQUE NORMAL FALLIDOS 0» y «USB: MIDI LISTO» con la computadora conectada;
   - que cada botón y tecla marque su cruce en la cuadrícula y su número;
   - que cada perilla cuente para los dos lados;
   - que MASTER se mueva;
   - que las teclas manden notas a la computadora.
4. Volver: desde el mismo instalador, eligiendo **Firmware de fábrica**. Esto prueba el servicio de actualización propio.

## Hechos de hardware usados (fuentes)

| Hecho | Fuente |
|---|---|
| Pantalla ST7789V 240×240, SPI1 grupo B (PC9 reloj, PC10 datos), CS PC7, D/C PC8, luz PA2 activa en bajo | Felucca/X0X (`fm1_lcd_hw.h`); grupo B del SPI1 en `include_lib/driver/cpu/wl82/asm/spi.h` del SDK |
| Matriz 11×6: 74HC595 en PA4 (dato), PA3 (reloj), PA1 (latch); filas PA0, PA5-PA8, PB7; mapa de IDs y perillas; LEDs PA9, PA10, PH6, PH9 | Felucca/X0X (`fm1_input.h`) |
| USB0 de velocidad completa en el USB-C | X0X (`fm1_usb.h`), FM-1-RE (`docs/io/06-usb.md`) |
| Protocolo de actualización, formato del paquete, captura de la reinstalación | FM-1-RE (WTFPL), `docs/io/11-ota-protocol.md`, `12-v15-reflash-proof.md` |
| Registro de actualización (tipo 0x5A0D, `ota-FM-1_015`, cargador en 0xE0000, copia a 256 B del final) y entrada al modo del chip | X0X (`ota.c`, `fm1_sys.h`), verificados en el aparato; estructura `UPDATA_PARM` del SDK |
| Protocolo del modo del chip, cargador `wl82loader.bin` | jl-uboot-tool (kagaimiq, MIT) |
| Llave 0x980F, flash 0x856014 de 1 MiB | jl-misctools sobre el V15; X0X (`fm1_rescue.py`) |
| Audio: codec externo por ALNK0 canal 3, 24 bits, 24 MHz / 544; líneas del codec PC0, PC1, PC2 y PC6; grupo de pines y fuente de reloj de ALNK0; avisos de medio buffer | Felucca (`fm1_audio.h`); registros `JL_ALNK0`, `JL_CLOCK`, `JL_IOMAP` del SDK |
| Botones por serigrafía (OCT− 0, OCT+ 1, FX 2 … PLAY 12, REC 13) y perillas (SELECT 0, ALGORITHM 1, KNOB 1-4 2-5, PRESETS 6) | Felucca (`panel.c`: PLAY y REC medidos, el resto por serigrafía) |

## Primera prueba en el aparato (2026-10-11)

- El instalador web y el cargador oficial escribieron Bubba Machine sin problema (el cargador aparece como USB `4D4A:4155`).
- Bubba Machine arrancó en **modo seguro**: el modo normal se cayó dos veces seguidas y la red de seguridad funcionó (pantalla, flash y registro de arranques andan).
- El USB del firmware **no apareció nunca**, ni en modo seguro: esperaba el aviso de "computador conectado" de la detección del SDK, que no llegó. Corregido: el controlador se enciende al arrancar (`fm1_usb_midi.c`).
- El modo normal probablemente se quedaba sin tiempo con el render dentro de la interrupción (reinicio del perro guardián). Corregido: el render pasa a una tarea con freno (`fm1_audio.c`).
- Para la próxima: cada arranque anota hasta qué etapa llegó y cómo terminó, y el modo seguro lo muestra. Desde el modo seguro, PLAY arranca sin salida de audio.
- Sin USB, la vuelta es por el modo del chip (OCT− + OCT+ 5 s) y `tools/fm1/rescate.py`.

## Lo que se mide en la primera prueba

1. **Audio**: que el codec arranque con la secuencia de ALNK0 y que el render quepa en 5.8 ms por medio buffer. Si no cabe, se oye entrecortado: el arreglo es subir el reloj del sistema o el medio buffer.
2. **Perillas**: el orden de KNOB 1-4 y su sentido.
3. **Batería**: el firmware muestra 100 % hasta medir la curva del ADC (el diagnóstico muestra el valor crudo).
4. **Tomas**: las muestras grabadas se leen por XIP; si la flash cifra esa zona, se oyen como ruido y hay que leerlas por `norflash_origin_read`.
