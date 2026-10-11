# Bubba Machine (FM-1 "K.O. Edition") — Especificación Técnica V3.1

> **V3.1 (implementación 0.1.0):** la sección 12 lista lo que se ajustó al escribir el código. Donde la sección 12 y el resto del documento no coinciden, manda la sección 12.
>
> **Qué cambió respecto a V2:** V2 se escribió con un contexto de hardware desactualizado (OLED SSD1306, 25+2 teclas, 4 encoders, ADC de 12 bits, flash "de sobra"). V3 toma los datos del hardware real, sacados del código fuente público de Felucca (firmware custom para el FM-1). Cada dato lleva su fuente en la sección 11. Lo que nadie ha medido todavía está marcado como **`VERIFICAR`**. Las decisiones que el autor aún no ha tomado están en la sección 10.

---

## 0. Directivas para la IA que escriba el código

Actúa como arquitecto principal de software embebido y DSP. Escribes C99 para el M-VAVE FM-1.

1. **Fuente de verdad:** la sección 1 es el hardware. Si algo no está ahí, **no lo supongas**: escribe `// TODO(HW-VERIFY): ...` y sigue.
2. **Decisiones abiertas:** si una función depende de algo de la sección 10, implementa la opción marcada como *por defecto* detrás de un `#define` y deja `// TODO(DECISION-n)`.
3. **Sin sistema de archivos:** se escribe en direcciones raw de la flash, **solo** dentro de las particiones de la sección 4.4.
4. **Sin memoria dinámica** en la ISR de audio, en el secuenciador ni en la UI. Todo se reserva estáticamente al arrancar.
5. **Sin código bloqueante** en la UI (`while` de espera, `delay()`). Única excepción: las rutinas de escritura en flash (4.5), que por hardware bloquean.
6. **No inventes funciones** que no estén en este documento.
7. **Diseño propio, no derivado de Felucca (DECISIÓN-1 resuelta):** está **prohibido copiar, traducir o parafrasear código de Felucca** (GPL-3.0-only). De Felucca solo se toman **hechos de hardware** (pines, direcciones de registro, tiempos, IDs de la matriz), que no tienen copyright. Para drivers a nivel de registro, la referencia es el **SDK de JieLi AC79 (Apache-2.0)** y los datasheets (ST7789V, 74HC595, flash NOR). La arquitectura, los nombres, las estructuras de datos y la UI son propios de este documento.

---

## 1. Hardware real del M-VAVE FM-1

### 1.1 SoC, memoria y arranque
| Elemento | Valor | Notas |
|---|---|---|
| SoC | JieLi familia AC79 (AC791N según la ingeniería inversa). En el SDK aparece como `cpu/wl82` | Núcleo pi32v2 |
| Toolchain | JieLi clang 4.0.1 para pi32v2 | **No** es `pi32v2-gcc` como decía V2 |
| SDK | `AC79NN_SDK_V1.2.1_2023-12-13` (Apache-2.0) | Solo se usan 3 archivos de boot para empaquetar |
| Reloj del CPU | `VERIFICAR` | |
| RAM | **96 KiB** de `.data/.bss` (+ 24 KiB de RAM ejecutable para rutinas de flash), según el mapa de memoria publicado de los firmwares custom | Por eso el sampler es de 1.1 s (5.2) |
| Flash | **1 MiB** NOR SPI, ejecución XIP. Borrado por sector de 4 KiB (hasta ~400 ms), programación por página de 256 B (hasta ~3 ms) | Comparte espacio con el firmware |
| Formato de actualización | `.fwsc` (app + loader + `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` del SDK) | Identidad de paquete `FM-1_910` |
| Instalación | Por USB (el instalador de Felucca usa USB-MIDI) | |
| Recuperación | Modo boot del chip: USB `WL80UBOOT` / mass storage `4C4A:8057`. Recuperación completa con *FM-1 Transporter* (RP2040) | **Respaldar la flash antes de la primera prueba** |

### 1.2 Pantalla
| Elemento | Valor |
|---|---|
| Panel | TFT a color, controlador **ST7789V**, **240×240**, RGB565 |
| Bus | SPI1 a 12 MHz (`BAUD 4`), con DMA. Pines: PC9 CLK, PC10 MOSI, PC7 CS, PC8 D/C |
| Backlight | PA2, activo en bajo. **Esa línea también habilita botones y teclas**: no apagarla para ahorrar energía si el teclado debe seguir vivo |
| Framebuffer | **No hay** (240×240×2 = 115 200 B). Se dibuja por franjas o rectángulos desde buffers en RAM |
| Coste de un frame completo | 115 200 B a 12 MHz ≈ **77 ms**, o sea ≈ 13 FPS como máximo redibujando todo |

### 1.3 Controles (matriz de teclado)
Todo pasa por **una matriz con diodos de 11 columnas × 6 filas**, manejada por 2× 74HC595 y escaneada desde la ISR de TIMER5 a ~10 kHz (una columna por tick). Antirrebote: 2 frames para presionar y 8 para soltar. Sin ghosting.

| Grupo | Cantidad | Detalle |
|---|---|---|
| Teclas de piano | **27**, IDs 14–40 | **F3 a G5**: 16 blancas + 11 negras. Las negras van en grupos 3-2-3-2-1 (F#3 G#3 A#3 · C#4 D#4 · F#4 G#4 A#4 · C#5 D#5 · F#5) |
| Botones | **14**, IDs 0–13 | Serigrafía: **OCT−, OCT+, FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC**. Solo los IDs de PLAY y REC están medidos; el resto debe calibrarse. **No hay botón SHIFT dedicado** |
| Encoders | **7**, con detent | Serigrafía: **SELECT, ALGORITHM, PRESETS**, más **KNOB 1–4**. Se leen **dentro de la matriz** (no por interrupción GPIO), con decodificador de cuadratura por tabla: 1 detent = 4 transiciones, hasta ~300 clics/s. **No tienen pulsador** (no hay IDs libres en la matriz) |
| Pot MASTER | 1 | PB6, ADC canal 4, **10 bits (0–1023)**, sin filtrado por hardware |
| Batería (lectura) | ADC | PB1, ADC canal 3. **PB1 a nivel bajo durante 8 s reinicia el chip**: configurarlo solo como entrada analógica |
| LEDs | En **botones y teclas** | Multiplexados en las columnas de la matriz (líneas PH6, PH9, PA9, PA10). Brillo por tiempo de encendido (apagado / tenue / encendido / respiración). Colores: `VERIFICAR` |

### 1.4 Audio y conectividad
| Elemento | Valor |
|---|---|
| Salida | Codec externo por I2S (ALNK0), DMA de doble buffer, estéreo de 24 bits en palabras int32 |
| Frecuencia de muestreo | **≈ 44 117.6 Hz** (24 MHz / 544) |
| Bloque | Felucca usa 128 frames por medio buffer (2.9 ms) y renderiza **dentro de la ISR** de ALNK0 |
| Altavoz | Integrado; se silencia por hardware al conectar el jack |
| Jack | 3.5 mm TRS estéreo |
| USB-C | MIDI in/out *class-compliant*; audio USB (Felucca envía la mezcla al computador a 44.1/48 kHz); consola serie opcional |
| MIDI TRS | **Solo entrada** (UART). No hay salida MIDI TRS |
| Bluetooth | El firmware de fábrica tiene MIDI y audio BT. **Ningún firmware custom lo ha demostrado** (ver DECISIÓN-4) |
| Batería | 2000 mAh |

---

## 2. Arquitectura de software

- **`core/`**: C99 puro, sin includes de hardware. DSP, secuenciador, máquinas de estado, modelo de UI.
- **`hal/`**: interfaces `hal_audio.h`, `hal_lcd.h`, `hal_input.h`, `hal_flash.h`, `hal_adc.h`, `hal_midi.h`, `hal_led.h`. Hay dos implementaciones: JieLi y simulador (WASM/PC).
- **Motores de sonido** con vtable común:
  ```c
  typedef struct SoundEngine SoundEngine;
  struct SoundEngine {
      void (*init)(SoundEngine *self);
      void (*note_on)(SoundEngine *self, uint8_t note, uint8_t vel);
      void (*note_off)(SoundEngine *self, uint8_t note);
      void (*set_macro)(SoundEngine *self, uint8_t param_id, uint8_t val);
      bool (*process)(SoundEngine *self, int32_t *buf, uint16_t frames);
  };
  ```
- **Hilos de ejecución** (bare-metal):
  1. **ISR de audio (ALNK0):** renderiza un bloque; consume la cola de comandos UI→DSP y publica medidores/playhead en la cola DSP→UI.
  2. **ISR de escaneo (TIMER5, ~10 kHz):** matriz, cuadratura y LEDs. Solo **acumula** (ver 6.1).
  3. **Bucle principal:** UI a 30 Hz, dibujo, ADC, MIDI, escrituras en flash.
- **Comunicación:** colas SPSC *lock-free* de tamaño fijo, una por dirección. El DSP nunca lee estructuras de la UI directamente.

---

## 3. Motor de audio

### 3.1 Bus y frecuencia
- Bus sumador de 32 bits; salida a 24 bits.
- **Fs de salida = 44 117.6 Hz**, fija por hardware. *(Cambio V3: V2 pedía 32 kHz, pero el codec y el audio USB trabajan a 44.1 kHz.)*
- **Carácter lo-fi:** los motores pueden correr a Fs/2 (≈ 22 059 Hz) con sobremuestreo ×2 (por defecto, *zero-order hold*). Así se reduce el CPU a la mitad y se consigue el sonido "SP". Ver DECISIÓN-2.

### 3.2 Interpolación de muestras
- Modo por pista: **LINEAL** (*por defecto* en Drums; hasta 0.3 era DROP, sin interpolación) o **HERMITE** (cúbica, *por defecto* en Sampler).
- *(Corrección V3: Hermite reduce el ruido de interpolación, pero no elimina el aliasing al subir el tono.)*
- Wavetables: tablas **band-limited por octava** (mipmaps) generadas por el packer (8.1).

### 3.3 Salida maestra (orden de la cadena)
`suma 32-bit → soft-clipper (DRIVE) → volumen MASTER (pot) → DAC`
- El **soft-clipper** es un polinomio o una LUT, no `tanh()` por muestra. Su `DRIVE` se ajusta con GLO+KNOB4 y se guarda en los globales. *(Cambio V3: el pot ya no controla el clipper; si lo hiciera, el timbre cambiaría con el volumen.)*
- **Pot MASTER:** ADC de 10 bits leído en el bucle principal, con media móvil de 4 lecturas + histéresis de ±4 cuentas + rampa de ganancia por bloque. **Nunca se guarda.**
- **Indicador de saturación:** si el bus pasa del umbral en un bloque, el DSP levanta un flag y la UI dibuja un borde rojo de 2 px durante 200 ms.

### 3.4 Polifonía y robo de voces
- Objetivo: **12 voces**. `VERIFICAR` midiendo el CPU real; Felucca usa 8.
- **Reserva estática** *(cambio V3)*: T1 Drums = 4 voces con choke groups · T3 Bass = 1 voz monofónica con glide · **pool compartido de 7 voces** para T2 y T4–T6.
- Robo dentro del pool, de menor a mayor prioridad: (4) colas de release y voces de poca amplitud → (3) sustain de acordes/pads → (2) ataques nuevos. Se roba siempre la de menor prioridad con menor amplitud, con micro-fade de 2 ms (~88 muestras).
- **Protección de CPU:** si dos bloques seguidos superan el 85 % del tiempo de bloque, se mata una voz del pool por bloque mientras dure la sobrecarga.

---

## 4. Secuenciador y almacenamiento

### 4.1 StepEvent: 8 bytes *(cambio V3: antes 7)*
| Byte | Campo | Rango / codificación |
|---|---|---|
| 0 | `note` | 0–127; **0xFF = evento vacío** |
| 1 | `vel` | 1–127 |
| 2 | `len` | bits 0–6 = largo en pasos (1–127); bit 7 = **TIE/GLIDE** |
| 3 | `prob` | 0–100 (%) |
| 4 | `p_cutoff` | 0–254; 0xFF = valor de la pista |
| 5 | `p_res` | ídem |
| 6 | `p_decay` | ídem |
| 7 | `p_pos` | posición de wavetable / inicio de sample; ídem |

- Hasta 4 eventos por paso, 64 pasos (4 páginas de 16) y 6 pistas.
- **Probabilidad:** PRNG `xorshift32` con semilla por patrón, que se reinicia al dar PLAY (cada vuelta suena distinta, pero dos corridas desde PLAY son idénticas). El simulador y el hardware deben dar el mismo resultado. **No usar `rand()`.**

### 4.2 Formato de patrón en flash (disperso)
*(Cambio V3: el formato denso ocuparía 12 KiB por patrón, y con 1 MiB de flash no caben.)*
```
Header (16 B): magic 'KOP3' | slot u8 | rsv u8 | len u16 | seq u32 | crc32 u32
Meta   (64 B): bpm, swing, largo, delay, sidechain, kit, waves...
Pistas (6 × 32 B): parámetros por pista (macros, nivel, pan, envío, duck, motor)
Eventos: por pista → count u16 + count × [step u8 + StepEvent 8 B]
```
- **Máximo 768 eventos por patrón.** Tamaño máximo: 16 + 64 + 192 + 12 + 768 × 9 = **7 196 B, que caben en 2 sectores (8 KiB)**.
- Si se llena, la UI muestra `PATRON LLENO` y no acepta más eventos.
- Song mode: lista de `{patrón u8, repeticiones u8}`, con un máximo de 64 entradas.

### 4.3 Reloj
- **Resolución interna: 96 PPQN**, calculada a partir del contador de muestras del audio (sin timer aparte, así no hay deriva). Los eventos se disparan con offset dentro del bloque, con precisión de muestra.
- `INTERNAL`: el FM-1 es maestro y envía MIDI Clock (24 PPQN) **solo por USB**.
- `EXTERNAL`: esclavo de MIDI Clock por **USB o TRS**. Entre ticks se interpola ×4 y hay un filtro de jitter.
- **Swing: 50–75 %** (50 = recto) aplicado a los pasos pares. *(Cambio V3: antes 0–100 %.)*
- El cambio de patrón siempre se **cuantiza al final del patrón**.

### 4.4 Mapa de flash (1 MiB)
Tomado del mapa que usa Felucca. **`VERIFICAR` el tamaño final del binario antes de fijarlo.**

| Rango | Tamaño | Uso | Regla |
|---|---|---|---|
| `0x000000–0x096FFF` | ~604 KiB | Boot + firmware + **ROM de sonidos** (drums, AKWF) | Solo lo escribe el instalador |
| `0x097000–0x0DFFFF` | 292 KiB | **Store de usuario** (log de patrones, song, samples guardados) | Escribible |
| `0x0E0000–0x0E4FFF` | 20 KiB | Staging del loader OTA | No tocar si se usa ese loader |
| `0x0E5000–0x0E7FFF` | 12 KiB | Reservado | No tocar en V3 |
| `0x0E8000–0x0FBFFF` | 80 KiB | Respaldo VM del firmware de fábrica | **Nunca escribir** |
| `0x0FC000–0x0FEFFF` | 12 KiB | Globales (A/B) | Escribible |
| `0x0FF000–0x0FFFFF` | 4 KiB | Desconocido | `VERIFICAR`; no tocar |

**Presupuesto propuesto de la ROM de sonidos:** Drums ≤ 192 KiB (16 bits a 22 kHz, o IMA-ADPCM 4:1) · AKWF: 64 ondas × 256 muestras × 16 bits = 32 KiB (≈ 64 KiB con mipmaps) · Código ≤ ~340 KiB.

**Store de usuario:** 292 KiB / 8 KiB = 36 slots. Por defecto: **16 patrones + 1 song**, y el resto queda como espacio libre para el wear-leveling (ver DECISIÓN-3 sobre guardar samples).

### 4.5 Escritura en flash (wear-leveling y seguridad)
- **Log circular** de slots de 8 KiB: siempre se escribe en el siguiente slot libre, nunca encima del slot vigente.
- **Orden de commit:** primero el cuerpo, al final el header con `seq` + `crc32`. Al arrancar se escanea y gana, por id, el `seq` más alto con CRC válido. Si se corta la batería a mitad, el patrón anterior sigue intacto.
- Antes de borrar, comprobar batería (ADC) > umbral (`VERIFICAR`).
- **Mientras se borra o programa, el XIP se apaga y las IRQ se desactivan**: el código corre desde `.ram_text` y **el audio se detiene** (hasta ~400 ms por sector).
  - **Regla:** solo se guarda con el **transporte en STOP**, con un fade-out de 5 ms antes de empezar. La pantalla muestra `GUARDANDO`.
  - Autosave: solo en STOP y tras 10 s sin cambios.

---

## 5. Pistas

| Pista | Motor | Detalle |
|---|---|---|
| T1 Drums | ROMpler | Hits CC0 (estilo 808/909/Linn) desde la ROM. Choke: HH cerrado corta HH abierto. 4 voces reservadas. Interpolación LINEAL por defecto |
| T2 Sampler | RAM | Ver 5.2 |
| T3 Bass | Wavetable mono | 1 voz, glide cuando el evento tiene TIE, SVF paso-bajo de 2 polos |
| T4–T6 Synth | Wavetable poly | Pool compartido; ondas AKWF remuestreadas a 256 muestras; SVF de 2 polos |

**Sidechain:** cada NoteOn de T1 dispara una envolvente de atenuación (attack 0, release ajustable) que multiplica la ganancia de T3–T6. Profundidad **por pista** (0–100 %). Se calcula por bloque con interpolación lineal (es barato, pero no "gratis").

### 5.2 Sampler
- **Fuente** (ver DECISIÓN-4), por fases: 1) **resample** de la mezcla interna o de una pista (sin riesgo de hardware) → 2) audio USB entrante → 3) Bluetooth A2DP.
- Formato: **8 bits** mono a Fs/2 (≈ 22 kHz) en un buffer de 24 KiB = **1.11 s** (la RAM es de 96 KiB).
- **Chop mode:** durante la grabación, las teclas no suenan; cada pulsación guarda un marcador (offset de inicio). Máximo 16 marcadores. **No se dibuja la forma de onda**: se muestra el nivel de entrada, los segundos restantes y una línea de tiempo con los marcadores.
- **El sample es volátil por defecto:** se pierde al apagar. La UI lo avisa con la etiqueta `VOLATIL` en T2.

---

## 6. UI/UX

### 6.1 Entrada asíncrona
- La ISR de escaneo **acumula** un contador `int16_t` por encoder (7) y un bitmap de teclas/botones con flancos. **No se usa una FIFO de eventos**, porque con ráfagas se desborda.
- La UI (30 Hz) lee y pone a cero los contadores en una sección crítica corta y aplica aceleración (≥ 3 clics por tick → ×4).

### 6.2 Gramática de gestos
| Gesto | Regla |
|---|---|
| **TAP** | Se suelta antes de **300 ms** → se ejecuta **al soltar**: abre la página del botón |
| **HOLD** | Pasan 300 ms sin soltar → se ejecuta **al vencer el timer** (no al soltar): modo live momentáneo. Al soltar, vuelve a la vista anterior |
| **COMBO** | Otra tecla o encoder antes de los 300 ms → se cancela el TAP y se entra al HOLD **de inmediato** |
| **Instantáneos** | PLAY, teclas de nota, selección de pista y GLO (como modificador) actúan **al presionar**, sin esperar umbral |
| **Hold largo** | SAVE = 1 000 ms (porque escribe en flash) |

Umbrales en `config.h`: `UI_HOLD_MS 300`, `UI_SAVE_HOLD_MS 1000`, `UI_OVERLAY_MS 1500`.

### 6.3 Pantalla 240×240
```
y=0   ┌────────────────────────────┐
      │ ESTADO (24 px, siempre fijo)│  ▶/■ · patrón · compás.tiempo · pista (color) · BPM · batería
y=24  ├────────────────────────────┤
      │ CONTEXTO (176 px)          │  vista del modo actual; los overlays solo tapan esta zona
y=200 ├────────────────────────────┤
      │ K1 │ K2 │ K3 │ K4  (40 px) │  leyenda de KNOB 1–4, en el mismo orden físico
y=240 └────────────────────────────┘
```
- *(Cambio V3: antes era 50/50; con 50 % de estado se desperdician 120 px.)*
- **Presupuesto de dibujo:** cada tick de 33 ms puede mandar como mucho ~37 KB por SPI (≈ 1/3 de la pantalla). Solo se redibujan rectángulos sucios. Buffer de franja: 240 × 16 × 2 = 7.5 KiB.
- **Overlay de parámetro:** aparece al primer clic del encoder dentro de la zona de contexto, la celda de la leyenda se invierte, y se oculta 1 500 ms después del último clic.
- **Vista SEQ:** las 4 páginas a la vez (grilla de 4 × 16 de la pista actual, celdas de 15 px), con la página editable resaltada, y debajo 6 mini-carriles con la actividad de cada pista.
- **Live Key-Map:** dibujo 1:1 del teclado real F3–G5 (blancas de 15 px) con fuente de 3×5 px para los nombres. La nota o acorde que se está tocando aparece grande en la parte superior de la zona de contexto.

### 6.4 LEDs *(nuevo en V3: el hardware los tiene)*
- **Teclas en SEQ:** blanca encendida = paso activo · tenue = vacío · parpadeo = playhead · respiración = paso con P-Lock.
- **Negras en SEQ:** pista seleccionada encendida; pistas muteadas apagadas.
- **Botones:** modo activo encendido; PLAY encendido mientras suena; REC encendido mientras graba.
- **En SCL live:** las teclas que están en la escala, encendidas.

### 6.5 Mapa de controles
**Botones** (GLO hace de modificador SHIFT):

| Botón | TAP | HOLD | GLO + |
|---|---|---|---|
| PLAY | Play/Stop (al presionar) | — | Reinicia desde el paso 1 |
| REC | Grabación en vivo on/off | — | — |
| SEQ | Vista de pasos | Banco de patrones en vivo (blanca = encolar) | Song mode |
| HOME | Vista general de las 6 pistas + mixer | Mute en vivo (blancas 1–6) y solo (9–14) | — |
| EDIT | Parámetros del motor de la pista (páginas con SELECT) | — | — |
| ENV | Envolvente de la pista | — | — |
| LFO | DECISIÓN-5 | | |
| FX | Página de FX globales (delay, sidechain) | **Punch-in FX:** blancas = 16 efectos | — |
| SCL | Página de escala y acordes | **Live Key-Map:** tocar escala y acordes | — |
| ARP | Página del arpegiador | Arpegiador en vivo mientras se mantiene | — |
| SAVE | Menú de guardado | 1 s = guardado rápido (solo en STOP) | — |
| GLO | Ajustes globales (reloj, sync, MIDI, fuente del sampler) | Modificador | — |
| OCT− / OCT+ | Octava − / + · **en SEQ: página − / +** | — | Transponer la pista ± semitono |

**Encoders:**

| Encoder | Función | GLO + |
|---|---|---|
| KNOB 1–4 | Macros de la pista: CUTOFF · RESO · DECAY · POS (son los mismos 4 P-Locks) | BPM · SWING · LARGO · DRIVE |
| SELECT | Navegar páginas o filas de menú | — |
| ALGORITHM | **Selección de pista 1–6** (la pantalla muestra el nombre real) | Motor o kit de la pista |
| PRESETS | Patrón A01–A16 (se encola al final si está sonando) | Onda AKWF / kit |
| MASTER (pot) | Volumen final, no se guarda | — |

**Teclas en SEQ:** blancas 1–16 = pasos de la página · mantener paso + KNOB = P-Lock · negras 1–6 = pista (GLO + negra = mute) · negra 7 = COPY · 8 = PASTE · 9 = CLEAR · 10 = FILL (mantener) · 11 = reservada.
**Teclas fuera de SEQ:** tocan la pista seleccionada (en T1, cada tecla es un sonido del kit).
**SCL live:** blancas = notas de la escala · negras 1–7 = acordes diatónicos · negras 8–11 = +7, +9, SUS2, SUS4.

**Portapapeles:** `struct { uint8_t type; union { StepEvent ev[4]; TrackData trk; PatternData pat; } }`. **No usar `void*`.** Undo de un nivel para la última edición (paso, CLEAR o PASTE).

---

## 7. Conectividad

- **Split Sync** (en GLO): el canal L emite pulsos y el R lleva la mezcla en mono.
  - PPQN configurable 1/2/4 (por defecto 2; `VERIFICAR` con un Pocket Operator o Volca real).
  - Pulso de 5 ms a escala completa, que **no pasa por el clipper ni por el pot MASTER**.
  - Compensación de latencia en ms ajustable.
- **MIDI:** entrada por USB y TRS; salida solo por USB. Canales 1–6 = pistas.
- **Bluetooth:** ver DECISIÓN-4.

---

## 8. Herramientas

1. **Packer (Python):** WAV → 22 050 Hz mono (16 bits o IMA-ADPCM) con filtro antialiasing. AKWF (600 muestras) → 256 muestras + mipmaps por octava. Genera `rom_assets.c` (`const`) + índice. Comprueba que se respete el presupuesto de 4.4.
2. **CMake** con 3 targets: `sim` (PC, miniaudio + SDL2), `wasm` (emcc) y `fm1` (JieLi clang 4.0.1 pi32v2 + empaquetado `.fwsc`).
3. **Companion web:** Web MIDI + SysEx para volcar o cargar patrones, con 7 bits por byte (8 bytes → 7 de datos).
4. **Instalador propio:** escrito desde cero (no se reutiliza `fm1_install.py`). Instala por USB-MIDI y, antes de instalar, verifica que existe un respaldo completo de la flash. Mientras no exista, la primera prueba se hace con un método de recuperación ya probado a mano (modo boot `WL80UBOOT` o el Transporter RP2040). **Usarlos como herramientas externas no copia código dentro del firmware.**

---

## 9. Seguridad del dispositivo

- No escribir nunca fuera de las particiones escribibles de 4.4.
- Combinación de arranque (OCT− + OCT+ al encender) = modo seguro: firmware sin cargar el store, por si un patrón corrupto provoca un crash.
- Documentar la vuelta al firmware de fábrica (actualizador de M-VAVE) y la recuperación con FM-1 Transporter.

---

## 10. Decisiones abiertas

| # | Decisión | Opciones | Por defecto |
|---|---|---|---|
| 1 | ~~Licencia y base de código~~ | **RESUELTA:** firmware propio desde cero, sin código de Felucca. La licencia del proyecto la elige el autor | — |
| 1b | ~~Licencia del proyecto~~ | **RESUELTA (2026-10-10): GPL-3.0-or-later.** Compatible con usar el SDK de JieLi (Apache-2.0): sus binarios se distribuyen con su licencia | — |
| 2 | Frecuencia de los motores | Fs completa (44.1k) vs Fs/2 (22k, lo-fi, mitad de CPU) | Fs/2 |
| 3 | ¿Guardar samples? | Volátil · 1 slot de ~2 s (≈ 88 KiB, quitando 11 slots de patrón) | Volátil |
| 4 | Fuente del sampler | Resample → USB audio → BT (cada fase depende de la anterior) | Resample |
| 5 | Botón LFO | LFO por pista (función nueva) · asignarlo al mixer · reservado | Reservado (pantalla "RESERVADO" en 0.1.0) |
| 6 | Polifonía final | 12 si el CPU lo permite, si no 8 | Medir |

---

## 11. Fuentes

- Código y wiki de Felucca (hugelton/Felucca): `firmware/hal/fm1_input.h` (matriz, IDs, encoders, LEDs), `fm1_lcd_hw.h` y `src/lcd.c` (ST7789V 240×240, SPI1 12 MHz), `fm1_adc.h` (MASTER PB6 ch4 y batería PB1 ch3, 10 bits), `fm1_audio.h` y `src/audio.c` (I2S, 24 bits, 44 117.6 Hz), `fm1_flash.h` (1 MiB, sectores, mapa, XIP desactivado durante la escritura), `src/panel.c` (nombres de botones y encoders), `BUILDING.md` y `LICENSING.md`.
- Reseñas: Synth Anatomy (TFT a color, batería de 2000 mAh, BT MIDI + audio, entrada MIDI TRS), Elektronauts (4 encoders + rejilla de 6×2 botones).

---

## 12. Ajustes hechos al implementar (V3.1, firmware 0.1.0)

| Tema | V3 | Implementado | Por qué |
|---|---|---|---|
| RAM | `VERIFICAR` | 96 KiB; la app completa usa ~81 KiB estáticos + ~6 KiB de gráficos | Ver `docs/MEMORIA.md` |
| Sampler | 16 bits, RAM libre | 8 bits, 24 KiB = 1.11 s, volátil | Cabe en la RAM |
| Delay | sin detalle | Mono, 16 bits, a Fs/4 efectivos (escribe una de cada dos muestras del motor): 8192 muestras = 0.74 s. Tiempos 1/16, 1/8, 3/16, 1/4 | Repeticiones oscuras, carácter lo-fi y poca RAM |
| Buffer de punch-in | sin detalle | 8 KiB mu-law = 0.37 s; si 1/4 no cabe, usa la mitad | RAM |
| Punch-in FX (16) | STUT, REV, TAPE STOP/START... | STUT 1/4·1/8·1/16·1/32, REV 1/8·1/4, TAPE STOP, GATE 1/16, LPF SWEEP, HPF SWEEP, BITCRUSH, SR DOWN, DLY THROW, ECHO FREEZE, KILL LOW, KILL HIGH | TAPE START necesita leer el futuro: se cambió por GATE |
| Nombres de las 4 macros | CUTOFF RESO DECAY POS | Synth: CUTOFF RESO DECAY WAVE · Drums/Sampler: TONE TUNE DECAY START | Mismos 4 bytes de P-Lock, nombre según el motor |
| HOME | vista general | Vista general + mixer: K1 NIVEL, K2 PAN, K3 ENVÍO, K4 DUCK | El mixer necesita un lugar sin botón propio |
| ENV | envolvente | Página 1: ATTACK DECAY SUSTAIN RELEASE · ENV otra vez = página 2: ENV>FILTRO, GLIDE | Regla "el mismo botón pasa páginas" |
| FX (tap) | FX globales | K1 tiempo de delay, K2 feedback, K3 mix, K4 release del sidechain | — |
| SCL | escala y acordes | K1 tónica, K2 escala (9), K3 acorde base (tríada/+7/+9/SUS2/SUS4), K4 LOCK (teclado siempre en escala) | — |
| ARP | arpegiador | K1 modo (OFF/UP/DOWN/UP-DN/RAND), K2 rate (1/8 1/16 1/32), K3 octavas (1-3), K4 gate | Solo pistas synth |
| Negra 11 en SEQ | reservada | **UNDO** (un nivel) | El undo existía pero no tenía tecla |
| GLO + ALGORITHM | motor o kit | T1: kit (KO-808 / KO-909) · T2: interpolación (LINEAL/HERMITE) · T3-T6: banco de ondas | — |
| GLO + PRESETS | onda / kit | Sin uso | Quedó cubierto por GLO + ALGORITHM |
| Paso mantenido | P-Lock con KNOBs | Además: SELECT = nota/sonido, ALGORITHM = probabilidad (pasos de 5 %), PRESETS = largo 1-16 o TIE | Los 7 encoders tienen trabajo |
| Formato disperso | paso u8 | Byte de posición = paso (bits 0-5) + slot (bits 6-7) | Conservar el slot de cada evento al recargar |
| Ganancia del bus | sin detalle | Nivel 100 = -12 dB de headroom; DRIVE por defecto 25 (≈ x1.35) | Medido con la demo: satura en ~0.5 % de los bloques |
| Primer arranque | sin detalle | Si la flash está vacía, se carga una demo (Am-F-C-G, 112 BPM) | Para que PLAY suene de inmediato |
| Sobremuestreo x2 | ZOH (escalón) | Interpolación lineal | El ZOH dejaba -22 dB de imágenes arriba de 11 kHz; lineal baja a -27 dB por casi el mismo costo |
| Botones sin capa | TAP al soltar antes de 300 ms | EDIT, ENV y LFO hacen TAP al soltar aunque se hayan mantenido (salvo combo) | Más tolerante, sin función que perder |

---

## 13. Versión 0.2

El diseño de la 0.2 está en [`DISENO_0.2.md`](DISENO_0.2.md) y reemplaza a las secciones 4.1, 5, 6.2 y 6.5 donde no coinciden:

- **Pistas TR** (T1 y T2): un byte por paso y por instrumento (on, acento, repetición, flam, condición de fill, P-Lock) en lugar de eventos de 8 bytes.
- **Tiempo por pista:** largo y velocidad por pista; largo por instrumento TR; largo maestro.
- **Tercera pulsación de 2 s** para pantallas especiales, que se cancela si se toca otro control.
- **Sampler:** 2 slots de 64 KiB en flash (4 s a 16 kHz) y grabación de hasta 3 s en una arena de 48 KiB compartida con el delay y el punch-in. Decisión 3 resuelta: el sample se guarda.
- **Flash:** el log de patrones baja a 20 slots (`0x097000-0x0BFFFF`) y los samples ocupan `0x0C0000-0x0DFFFF`.
- **Escenas** A-D, **loopera** por synth, **roll**, **strum** y **cuadrícula de acordes**.
