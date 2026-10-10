# Instalar Bubba Machine

## Estado actual: todavía no se puede instalar

**No hay instalador ni binario para el FM-1.** El firmware funciona completo en el [emulador web](https://erbubar23.github.io/bubba-machine/emulador.html) y en las pruebas de PC, pero el port al hardware es un esqueleto que **nunca se ha compilado con el toolchain de JieLi ni probado en un FM-1**. El avance está en [`hal/fm1/README_PORT.md`](../hal/fm1/README_PORT.md).

No intentes flashear nada de este repositorio en tu aparato.

## Mientras tanto: el emulador

1. Abre el [emulador](https://erbubar23.github.io/bubba-machine/emulador.html) en Chrome o Edge.
2. Pulsa **Encender**. Arranca con una demo.
3. Con el teclado de la computadora:
   - **Espacio** es PLAY, **Enter** es REC y **Shift** es GLO.
   - Los números **1-8** son HOME, SEQ, EDIT, ENV, FX, SCL, ARP y SAVE, y las **flechas** son OCT− y OCT+.
   - Las filas **Z-/** y **Q-P** tocan las teclas del FM-1.
   - La **rueda del mouse** sobre una perilla la gira.
4. En **Sampler y memoria** puedes cargar un WAV de hasta 4 s al slot A.

El emulador corre exactamente el mismo código C que irá en el aparato.

## Cómo será la instalación

Cuando exista una versión probada en hardware:

1. **Respaldo primero.** El instalador se va a negar a instalar si no hay un respaldo completo de la flash (1 MiB) de tu FM-1.
2. **Instalación por USB-C**, desde el navegador (Web MIDI), sin programas extra.
3. **Vuelta atrás:** siempre se podrá volver al firmware de fábrica con el actualizador oficial de M-VAVE o, si algo sale mal, restaurar el respaldo con el modo de arranque del chip o con el *FM-1 Transporter* (RP2040).
4. **Modo seguro:** mantener OCT− y OCT+ al encender arranca sin cargar los patrones, por si alguno estuviera dañado.

Bubba Machine solo escribe en las zonas de la flash reservadas para patrones, samples y ajustes. Nunca toca el cargador ni el respaldo del firmware de fábrica.

## Seguridad

Instalar firmware no oficial **puede dejar tu FM-1 inservible** y probablemente anula la garantía. Este software se entrega sin garantía (ver [licencia](../LICENSE), secciones 15 y 16). Instala solo versiones marcadas como **probadas en hardware** en el [registro de cambios](../CHANGELOG.md).
