# Desarrollo

## Requisitos

- **Python 3** con `numpy` y `Pillow`: generan los sonidos, la fuente, el logo y el sitio.
- **Docker**: compila y corre las pruebas (GCC) y el emulador (Emscripten). No hace falta compilador local.

En Windows el comando de Python es `py -3`; en Linux y macOS, `python3`.

## Comandos

```sh
py -3 tools/build.py assets   # fuente, kits, ondas, loops de fábrica y logo (core/gen/)
py -3 tools/build.py test     # compila y corre las pruebas en PC; capturas en assets/screens/
py -3 tools/build.py web      # emulador: web/bm.js
py -3 tools/build.py site     # páginas de documentación del sitio: web/docs/ y web/en/docs/
py -3 tools/build.py all      # todo lo anterior
```

Las pruebas (`tests/test_core.c`) recorren el firmware como lo haría una persona: arrancan, tocan botones, graban, guardan, apagan y vuelven a prender. También verifican el tiempo del secuenciador, el presupuesto de la pantalla y que nunca se escriba la flash mientras suena.

## Estructura

```
core/        el firmware, C99 sin hardware: audio, secuenciador, interfaz y guardado
core/gen/    datos generados por tools/: fuente, kits, ondas, loops y logo
hal/         capas de hardware: sim (PC y web), web (WebAssembly) y fm1 (port)
web/         sitio: portada, emulador (emulador.html) y documentación (docs/, en/)
docs/        manual, especificación, diseño y guías (fuente del sitio); docs/en/ en inglés
tests/       pruebas automáticas
tools/       generadores y build
assets/      logo, capturas, loops de fábrica
```

## Reglas del núcleo

- `core/` no conoce el hardware: todo pasa por la plataforma (`bm_platform`) que da cada `hal/`.
- Sin `malloc`. La RAM del FM-1 es de 96 KiB; el presupuesto está en [`MEMORIA.md`](MEMORIA.md).
- El audio corre en su propio contexto: la interfaz le habla solo por la cola de comandos.
- La flash **solo se escribe con el transporte parado**: escribir detiene el audio.

## Sonidos y loops de fábrica

- Los kits y las ondas se sintetizan en `tools/gen_assets.py`.
- Los loops de fábrica se definen en `assets/loops/loops.json` y se sintetizan en `tools/gen_loops.py`.
- Para probar con tu propio audio, crea `assets/loops/loops.local.json` (está en `.gitignore`) y genera con:
  ```sh
  py -3 tools/gen_loops.py --local
  ```
  **Un build hecho con `--local` no se puede publicar**: ni `web/bm.js` ni binarios. Antes de subir cambios, vuelve a generar sin `--local`.

## Idiomas de la pantalla

El firmware habla inglés. Cada idioma es un paquete (`.bmlang`) en su propia zona de la flash; el formato está en `core/bm_lang.h`.

- **Al dibujar se traduce:** `gfx_text` y `gfx_small` buscan cada frase en el paquete activo (por el hash de la frase en inglés). Con el inglés activo no cuesta nada.
- **Formatos:** lo que pasa por `snprintf` se marca con `TF("...")` (si la traducción no tiene los mismos `%`, se usa el inglés) y las palabras que se pegan con `%s` van en `T(...)`.
- `py -3 tools/gen_lang.py` junta las frases de `core/` en `lang/plantilla.txt`, revisa `lang/<codigo>.txt` (solo ASCII, los mismos `%`, frases que existen) y arma `lang/<codigo>.bmlang` y `web/lang/` (para el emulador). `build.py test` y `build.py web` lo corren solos. `lang/ignorar.txt` lista las cadenas del código que no son de pantalla.
- En el FM-1 se carga con `py -3 tools/fm1/idioma.py lang/es.bmlang` (orden `0x21` de la consola).

## El sitio

- `web/index.html` es la portada en español, `web/en/index.html` la de inglés y `web/emulador.html` el emulador (en los dos idiomas: `?lang=es` o `?lang=en`).
- Las páginas de `web/docs/` y `web/en/docs/` se generan desde los `.md` con `tools/gen_site.py`: edita el `.md`, no el HTML. Las fuentes en inglés están en `docs/en/`; si cambias una guía, actualiza las dos.
- GitHub Pages publica la carpeta `web/` con el flujo `.github/workflows/pages.yml`.

## Integración continua

`.github/workflows/ci.yml` corre las pruebas en cada push y en cada pull request.

## Ramas

| Rama | Para qué |
|---|---|
| `dev` | Trabajo diario. Los pull requests van aquí |
| `qa` | Lo que se va a publicar, en prueba: emulador y, cuando exista, el FM-1 |
| `main` | Lo publicado. Cada versión sale de aquí y el sitio se publica desde aquí |

Los cambios suben en un solo sentido: `dev` › `qa` › `main`.

## Versiones

- El número de versión vive en `core/bm_config.h` (`BM_VERSION_STR`) y en [`CHANGELOG.md`](../CHANGELOG.md).
- Cada versión pública es una etiqueta `vX.Y.Z` con su código fuente. La GPL exige que cualquier binario que se publique tenga su fuente correspondiente.
