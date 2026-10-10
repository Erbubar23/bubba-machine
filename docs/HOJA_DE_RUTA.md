# Hoja de ruta

Lo que viene para Bubba Machine, en el orden en que importa. Las fechas dependen de tener el hardware funcionando.

## Ahora: 0.2.x, vista previa pública

- Firmware completo en el emulador web.
- Documentación, licencia GPL-3.0 y sitio.
- Recibir reportes y opiniones sobre la forma de tocar.

## Port al FM-1

El paso más importante: que corra en el aparato. El plan está en [`hal/fm1/README_PORT.md`](../hal/fm1/README_PORT.md):

1. Respaldo completo de la flash y vuelta al firmware de fábrica comprobada.
2. Arranque, reloj, pantalla y matriz de botones.
3. Audio por I2S, flash y USB-MIDI.
4. Medir CPU, RAM y voces reales, y ajustar.

## Instalador web

- Instalación por USB-C desde el navegador, con Web MIDI.
- Respaldo obligatorio antes de instalar y vuelta atrás en un clic.

## 0.3: la interfaz 3.0

La nueva forma de tocar, ya diseñada en [`DISENO_3.0.md`](DISENO_3.0.md):

- Cuatro gestos por botón que significan lo mismo en todos: toque, mantener, mantener 2 s y HOME + botón.
- HOME en todas partes: controlador de la secuencia y modificador global.
- ALGORITHM navega entre canales, con un recuadro que confirma a dónde entras.
- 3 canales fijos (batería, sampler y bajo) y 3 libres (SYNTH, MONO, KIT o MIDI).
- Sampler 3.0: un break que el generador rearma, con la fila SUENA para ver lo que eligió.
- Grabadora propia con biblioteca de tomas y un editor para cortar después.

## Más adelante

- **Editor web:** cargar y bajar patrones, samples y loops por USB.
- **Packs de loops y kits libres** (CC0 o GPL), hechos por la comunidad.
- **Bluetooth MIDI**, si el hardware y el CPU lo permiten.
