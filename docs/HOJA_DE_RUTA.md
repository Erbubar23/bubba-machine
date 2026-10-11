# Hoja de ruta

Lo que viene para Bubba Machine, en el orden en que importa. Las fechas dependen de tener el hardware funcionando.

## Ahora: 0.3.x, la interfaz 3.0

- La interfaz 3.0 completa en el emulador web: cuatro gestos por botón, HOME en todas partes, canales fijos y libres, sampler 3.0 y grabadora con biblioteca de tomas.
- Recibir reportes y opiniones sobre la forma de tocar.
- Decisiones abiertas: más bancos de patrones (la flash guarda 16) y notas fantasma en la batería.

## Port al FM-1

El paso más importante: que corra en el aparato. El plan está en [`hal/fm1/README_PORT.md`](../hal/fm1/README_PORT.md):

1. Respaldo completo de la flash y vuelta al firmware de fábrica comprobada.
2. Arranque, reloj, pantalla y matriz de botones.
3. Audio por I2S, flash y USB-MIDI.
4. Medir CPU, RAM y voces reales, y ajustar.

## Instalador web

- Instalación por USB-C desde el navegador, con Web MIDI.
- Respaldo obligatorio antes de instalar y vuelta atrás en un clic.

## Hecho: 0.3, la interfaz 3.0

Ver el [registro de cambios](../CHANGELOG.md) y el [manual](MANUAL.md). El diseño está en [`DISENO_3.0.md`](DISENO_3.0.md).

## Después de la 0.3

- **LFO por canal** (el botón ya tiene su capa: repetir y ligar).
- **Bancos de patrones:** ampliar el mapa de flash o achicar los patrones.
- **Notas fantasma en la batería:** un P-Lock de nivel o un formato más grande.

## Más adelante

- **Editor web:** cargar y bajar patrones, samples y loops por USB.
- **Packs de loops y kits libres** (CC0 o GPL), hechos por la comunidad.
- **Bluetooth MIDI**, si el hardware y el CPU lo permiten.
