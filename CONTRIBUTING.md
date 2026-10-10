# Contribuir a Bubba Machine

Gracias por querer ayudar. Bubba Machine es firmware libre para el M-VAVE FM-1 y cualquier ayuda suma: probar en el emulador, reportar errores, mejorar el manual, diseñar sonidos o escribir código.

Se puede escribir en **español o en inglés**.

## Formas de ayudar

- **Probar y reportar.** Usa el emulador web y abre un *issue* con lo que pasó, lo que esperabas y cómo repetirlo. Las capturas ayudan mucho.
- **Ideas de interfaz.** Abre un *issue* de tipo idea. La dirección actual está en [`docs/DISENO_3.0.md`](docs/DISENO_3.0.md).
- **Documentación.** El manual vive en [`docs/MANUAL.md`](docs/MANUAL.md); el sitio se genera a partir de los `.md`.
- **Código.** Lee [`docs/DESARROLLO.md`](docs/DESARROLLO.md) para compilar y probar.
- **Hardware.** Si tienes un FM-1 y experiencia con el SDK de JieLi, el port en [`hal/fm1/`](hal/fm1/README_PORT.md) es lo que más ayuda necesita.

## Reglas del código

1. **Todo el código es propio.** No copies, traduzcas ni parafrasees código de otros firmwares para el FM-1 (incluido Felucca). Los hechos de hardware (pines, tiempos, registros documentados) sí se pueden usar, citando la fuente en [`docs/SPEC_V3.md`](docs/SPEC_V3.md).
2. **Código de terceros solo con licencia compatible** con la GPL-3.0, y anotado en [`CREDITOS.md`](CREDITOS.md).
3. **Sonidos:** solo sintetizados en `tools/`, de tu autoría o con licencia libre compatible (CC0, CC-BY o GPL). Nada de samples de discos o de packs comerciales. Para probar con audio de terceros usa `assets/loops/loops.local.json`, que nunca se sube.
4. **C99 sin dependencias** en `core/`: nada de `malloc`, nada de punto flotante en el camino del audio y nada que dependa del hardware. Todo lo del hardware va en `hal/`.
5. **Cada archivo nuevo** empieza con el identificador de licencia:
   ```c
   /* SPDX-License-Identifier: GPL-3.0-or-later
    * Copyright (C) 2026 Tu Nombre */
   ```

## Antes de abrir un pull request

Los pull requests van a la rama **`dev`**. De ahí los cambios pasan a `qa` para probarlos y luego a `main`, que es lo publicado (ver [ramas](docs/DESARROLLO.md#ramas)).

- [ ] `py -3 tools/build.py test` pasa sin fallos.
- [ ] Si cambiaste la interfaz, se actualizaron las capturas (`assets/screens/`) y el manual.
- [ ] Si cambiaste el formato de los patrones o de la flash, está explicado en el PR y en el [registro de cambios](CHANGELOG.md).
- [ ] Si agregaste algo de terceros, está en [`CREDITOS.md`](CREDITOS.md).

## Licencia de las contribuciones

Al enviar una contribución aceptas que se publique bajo la misma licencia del proyecto, **GPL-3.0-or-later**. Se recomienda firmar los commits con `git commit -s` ([Developer Certificate of Origin](https://developercertificate.org/)) para dejar constancia de que tienes derecho a aportar ese código.

## Convivencia

Este proyecto sigue un [código de conducta](CODE_OF_CONDUCT.md).
