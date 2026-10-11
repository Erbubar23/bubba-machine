# Código de terceros

Bibliotecas de otros proyectos que Bubba Machine compila tal cual o con cambios marcados. Cada carpeta conserva su licencia. Bubba Machine es GPL-3.0-or-later; todo lo de aquí es compatible con eso.

| Carpeta | Qué es | De dónde | Licencia | Cambios |
|---|---|---|---|---|
| `msfa/` | Motor FM de 6 operadores compatible con la DX7 («music synthesizer for android», de Google, mantenido por Dexed) | [Synth_Dexed](https://codeberg.org/dcoredump/Synth_Dexed), commit `e3f524c8bd92` (2025-09-10), carpeta `src/` | Apache-2.0 (`LICENSE`); `EngineMkI.*` es GPL-3.0-or-later (`LICENSE-GPL3-EngineMkI.txt`) y por ahora no se compila | `sin.h` y `fm_op_kernel.cpp`: sin multiplicaciones de 64 bits en el bucle interno (lentas en el AC791N); cada archivo lo dice en su cabecera |

Lo que usa Bubba Machine de cada una y cómo se mide está en `docs/MOTORES.md` y `docs/TRAZA_FM1.md`. No se copian aquí proyectos base de otros firmwares del FM-1 (Felucca, X0X, JIANT): solo bibliotecas de síntesis.
