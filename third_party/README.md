# Código de terceros

Bibliotecas de otros proyectos que Bubba Machine compila tal cual o con cambios marcados. Cada carpeta conserva su licencia. Bubba Machine es GPL-3.0-or-later; todo lo de aquí es compatible con eso.

| Carpeta | Qué es | De dónde | Licencia | Cambios |
|---|---|---|---|---|
| `msfa/` | Motor FM de 6 operadores compatible con la DX7 («music synthesizer for android», de Google, mantenido por Dexed) | [Synth_Dexed](https://codeberg.org/dcoredump/Synth_Dexed), commit `e3f524c8bd92` (2025-09-10), carpeta `src/` | Apache-2.0 (`LICENSE`); `EngineMkI.*` es GPL-3.0-or-later (`LICENSE-GPL3-EngineMkI.txt`) y por ahora no se compila | `sin.h` y `fm_op_kernel.cpp`: sin multiplicaciones de 64 bits en el bucle interno (lentas en el AC791N); cada archivo lo dice en su cabecera |
| `open303/` | Solo la licencia: `core/bm_acid.cpp` (BB-303) adapta en float el filtro TB-303, las envolventes, el circuito de acento y los filtros fijos de Open303 (Robin Schmidt) | [RobinSchmidt/Open303](https://github.com/RobinSchmidt/Open303), commit `313bf0d` (2024-03-29), carpeta `Source/DSPCode/` | MIT (`LICENSE`; el aviso de copyright también en la cabecera de `core/bm_acid.cpp`) | código adaptado, no copiado tal cual: float, sin sobremuestreo (corte hasta 10 kHz), coeficientes cada 8 muestras, osciladores polyBLEP en vez de tablas; la cabecera de `core/bm_acid.cpp` lo detalla |
| `plaits/` | Solo lo que se usa de Plaits (Mutable Instruments): bombos, cajas y hats modelados, el oscilador de forma variable, el seno y el overdrive | [pichenettes/eurorack](https://github.com/pichenettes/eurorack), carpeta `plaits/` (2026-10) | MIT (`LICENSE`, y en cada archivo) | `dsp/dsp.h`: la frecuencia de muestreo se fija al compilar (`PLAITS_SAMPLE_RATE`, Bubba usa 44 118 Hz). `resources.cc/.h`: solo las 5 tablas que se usan, sacadas del original con `tools/gen_plaits_lut.py` (el original trae wavetables, voces de habla y bancos de DX7 que no se publican) |
| `stmlib/` | Utilidades DSP de Mutable Instruments: filtro de estado variable, polyBLEP, interpolador, unidades, azar | [pichenettes/eurorack](https://github.com/pichenettes/eurorack), carpeta `stmlib/` (2026-10) | MIT (`LICENSE`) | ninguno; se compila con `-DTEST` (versiones sin ensamblador de ARM: el FM-1 no es ARM) |

Lo que usa Bubba Machine de cada una y cómo se mide está en `docs/MOTORES.md` y `docs/TRAZA_FM1.md`.

No se copian aquí proyectos base de otros firmwares del FM-1 (Felucca, X0X, JIANT): solo bibliotecas de síntesis.
