/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Traza y mediciones de la arquitectura del FM-1 (reingeniería, docs/ARQUITECTURA_FM1.md).
 *
 * 1. Perfil del audio real: core/bm_prof.h pone puntos de medición en bm_audio_render; aquí se
 *    acumula el tiempo de cada sección en ticks del timer del barrido (60 MHz) y la consola lo lee
 *    (orden 08). Sirve con Bubba sonando: dice qué parte del cálculo cuesta cuánto.
 * 2. Pruebas de la arquitectura (laboratorio, órdenes 21..25): el mismo cálculo DSP desde la
 *    flash y desde la RAM, lectura de datos de la flash (por la caché) contra la RAM, coma
 *    flotante contra enteros, uno contra dos núcleos y la pantalla a distintas velocidades del
 *    SPI. Los resultados van al registro de texto (consola: log). */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "os/os_api.h"
#include "fm1_hw.h"
#include "bm_prof.h"
#include "bm_rom.h"
#include "bm_fm.h"
#include "bm_app.h"
#include "bm_wave.h"
#include "bm_params.h"

void fm1_log(const char *s);

/* ------------------------------------------------------------------ perfil */
static volatile uint32_t prof_acc[BM_P_N], prof_t0;

uint32_t bm_prof_now(void) { return fm1_scan_now_ticks(); }

void bm_prof_add(int id, uint32_t t0)
{
    uint32_t d = fm1_scan_now_ticks() - t0;
    if (id >= 0 && id < BM_P_N && d < 0x10000000u)
        prof_acc[id] += d;
}

void fm1_prof_reset(void)
{
    for (int i = 0; i < BM_P_N; i++)
        prof_acc[i] = 0;
    prof_t0 = fm1_scan_now_ticks();
}

/* out: BM_P_N acumulados, ticks transcurridos, ticks por segundo */
int fm1_prof_read(uint32_t *out)
{
    for (int i = 0; i < BM_P_N; i++)
        out[i] = prof_acc[i];
    out[BM_P_N] = fm1_scan_now_ticks() - prof_t0;
    out[BM_P_N + 1] = fm1_scan_ticks_hz();
    return BM_P_N + 2;
}

/* ------------------------------------------------------------------ utilidades */
static char *num(char *p, uint32_t v)
{
    char t[11];
    int n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *p++ = t[--n];
    return p;
}

static char *txt(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    return p;
}

/* "nombre: a.bcd unidad" con v en milésimas */
static void log_mil(const char *nombre, uint32_t mil, const char *unidad)
{
    char l[96], *p = txt(l, nombre);
    p = txt(p, ": ");
    p = num(p, mil / 1000u);
    *p++ = '.';
    uint32_t f = mil % 1000u;
    *p++ = (char)('0' + f / 100u); *p++ = (char)('0' + f / 10u % 10u); *p++ = (char)('0' + f % 10u);
    *p++ = ' ';
    p = txt(p, unidad);
    *p = 0;
    fm1_log(l);
}

static uint32_t ns(uint32_t ticks, uint32_t n)           /* ticks del timer -> ns por unidad, en milésimas de ns */
{
    uint64_t hz = fm1_scan_ticks_hz();
    return (uint32_t)((uint64_t)ticks * 1000000000000ull / hz / (n ? n : 1u));
}

/* ------------------------------------------------------------------ núcleos de cálculo */
#define KN 256
/* Los búferes de las pruebas no viven en la RAM fija del firmware (eran ~59 KiB que el montón del
 * SDK necesita): se piden al montón al empezar una prueba y se devuelven al terminar. */
typedef struct {
    float kx[KN], ky[KN];
    int32_t ix[KN], iy[KN];
    uint8_t rbuf[32768];
    float dkx[2][KN], dky[2][KN];
    uint16_t fila[240 * 8];
    bm_pattern pat;
} lab_mem;
static lab_mem *L;
#define kx (L->kx)
#define ky (L->ky)
#define ix (L->ix)
#define iy (L->iy)
#define rbuf (L->rbuf)
#define dkx (L->dkx)
#define dky (L->dky)
#define fila (L->fila)
static volatile float kc[5] = {0.2f, 0.4f, 0.2f, -0.6f, 0.3f};
static volatile int32_t icf[5] = {6554, 13107, 6554, -19661, 9830};

#define BIQUAD(x, y, n)                                                            \
    float b0 = kc[0], b1 = kc[1], b2 = kc[2], a1 = kc[3], a2 = kc[4];              \
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;                                          \
    for (int i = 0; i < (n); i++) {                                                \
        float v = b0 * (x)[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;             \
        x2 = x1; x1 = (x)[i]; y2 = y1; y1 = v; (y)[i] = v;                         \
    }

#define IMAC(x, y, n)                                                              \
    int32_t b0 = icf[0], b1 = icf[1], b2 = icf[2], a1 = icf[3], a2 = icf[4];       \
    int32_t x1 = 0, x2 = 0, y1 = 0, y2 = 0;                                        \
    for (int i = 0; i < (n); i++) {                                                \
        int64_t a = (int64_t)b0 * (x)[i] + (int64_t)b1 * x1 + (int64_t)b2 * x2     \
                    - (int64_t)a1 * y1 - (int64_t)a2 * y2;                         \
        int32_t v = (int32_t)(a >> 15);                                            \
        x2 = x1; x1 = (x)[i]; y2 = y1; y1 = v; (y)[i] = v;                         \
    }

static __attribute__((noinline)) void bq_flash(const float *x, float *y, int n) { BIQUAD(x, y, n) }
SEC_USED(.volatile_ram_code) static __attribute__((noinline)) void bq_ram(const float *x, float *y, int n) { BIQUAD(x, y, n) }
static __attribute__((noinline)) void im_flash(const int32_t *x, int32_t *y, int n) { IMAC(x, y, n) }
SEC_USED(.volatile_ram_code) static __attribute__((noinline)) void im_ram(const int32_t *x, int32_t *y, int n) { IMAC(x, y, n) }

typedef void (*fk)(const float *, float *, int);
typedef void (*ik)(const int32_t *, int32_t *, int);

static uint32_t medir_f(fk f, int vueltas)
{
    uint32_t t0 = fm1_scan_now_ticks();
    for (int r = 0; r < vueltas; r++)
        f(kx, ky, KN);
    return fm1_scan_now_ticks() - t0;
}

static uint32_t medir_i(ik f, int vueltas)
{
    uint32_t t0 = fm1_scan_now_ticks();
    for (int r = 0; r < vueltas; r++)
        f(ix, iy, KN);
    return fm1_scan_now_ticks() - t0;
}

/* 21: el mismo filtro (biquad) desde la flash y desde la RAM, en coma flotante y en enteros */
static void bench_codigo(void)
{
    for (int i = 0; i < KN; i++) {
        kx[i] = (float)((i * 37) % 200 - 100) / 100.0f;
        ix[i] = ((i * 37) % 200 - 100) * 300;
    }
    const int R = 400;
    medir_f(bq_flash, 4);                                  /* calentar la caché */
    uint32_t a = medir_f(bq_flash, R), b = medir_f(bq_ram, R);
    uint32_t c = medir_i(im_flash, R), d = medir_i(im_ram, R);
    log_mil("biquad float, codigo en flash", ns(a, R * KN), "ns/muestra");
    log_mil("biquad float, codigo en RAM  ", ns(b, R * KN), "ns/muestra");
    log_mil("biquad int64, codigo en flash", ns(c, R * KN), "ns/muestra");
    log_mil("biquad int64, codigo en RAM  ", ns(d, R * KN), "ns/muestra");
}

/* 22: lectura de datos: flash por la caché contra RAM. Desde 0.4 no hay samples de kits en la
 * flash: se leen 170 KiB seguidos del programa (también está en la flash, por la misma caché). */
#define FLASH_SPAN (170u * 1024u)
static const uint8_t *flash_base(void) { return (const uint8_t *)(uintptr_t)&bm_app_init; }

static void bench_datos(void)
{
    volatile uint32_t sum = 0;
    uint32_t bytes = 0, t0, tf, tr;
    /* secuencial: 170 KiB de flash */
    t0 = fm1_scan_now_ticks();
    {
        const uint8_t *d = flash_base();
        uint32_t acc = 0;
        for (uint32_t i = 0; i < FLASH_SPAN; i++)
            acc += d[i];
        sum += acc;
        bytes = FLASH_SPAN;
    }
    tf = fm1_scan_now_ticks() - t0;
    for (uint32_t i = 0; i < sizeof rbuf; i++)
        rbuf[i] = (uint8_t)i;
    uint32_t vueltas = bytes / sizeof rbuf + 1u;
    t0 = fm1_scan_now_ticks();
    for (uint32_t v = 0; v < vueltas; v++) {
        uint32_t acc = 0;
        for (uint32_t i = 0; i < sizeof rbuf; i++)
            acc += rbuf[i];
        sum += acc;
    }
    tr = fm1_scan_now_ticks() - t0;
    log_mil("lectura secuencial flash (cache)", ns(tf, bytes), "ns/byte");
    log_mil("lectura secuencial RAM          ", ns(tr, vueltas * (uint32_t)sizeof rbuf), "ns/byte");
    /* al azar: 20000 lecturas en ~85 KiB de flash contra 32 KiB de RAM */
    const uint8_t *base = flash_base();
    uint32_t span = 85u * 1024u;
    uint32_t x = 12345u;
    t0 = fm1_scan_now_ticks();
    for (int i = 0; i < 20000; i++) {
        x = x * 1664525u + 1013904223u;
        sum += base[(x >> 8) % span];
    }
    tf = fm1_scan_now_ticks() - t0;
    t0 = fm1_scan_now_ticks();
    for (int i = 0; i < 20000; i++) {
        x = x * 1664525u + 1013904223u;
        sum += rbuf[(x >> 8) % sizeof rbuf];
    }
    tr = fm1_scan_now_ticks() - t0;
    log_mil("lectura al azar flash (cache)   ", ns(tf, 20000), "ns/lectura");
    log_mil("lectura al azar RAM             ", ns(tr, 20000), "ns/lectura");
    (void)sum;
}

/* 23: tamaño real de la caché de datos: lecturas al azar en zonas de la flash de distinto
 * tamaño. Mientras la zona entra en la caché el tiempo es el de la RAM; cuando no, salta. */
static void bench_cache(void)
{
    static const uint32_t KB[] = {2, 4, 8, 12, 16, 24, 32, 48, 64};
    const uint8_t *base = flash_base();                    /* programa: cientos de KiB seguidos en la flash */
    volatile uint32_t sum = 0;
    uint32_t x = 777u;
    for (unsigned k = 0; k < sizeof KB / sizeof KB[0]; k++) {
        uint32_t span = KB[k] * 1024u;
        for (int i = 0; i < 4000; i++) {                   /* calentar */
            x = x * 1664525u + 1013904223u;
            sum += base[(x >> 8) % span];
        }
        uint32_t t0 = fm1_scan_now_ticks();
        for (int i = 0; i < 20000; i++) {
            x = x * 1664525u + 1013904223u;
            sum += base[(x >> 8) % span];
        }
        uint32_t t = fm1_scan_now_ticks() - t0;
        char l[48], *p = txt(l, "azar en ");
        p = num(p, KB[k]);
        p = txt(p, " KiB de flash");
        *p = 0;
        log_mil(l, ns(t, 20000), "ns/lectura");
    }
    (void)sum;
}

/* 24: uno contra dos núcleos: dos tareas con el mismo cálculo a la vez */
static OS_SEM sem_go, sem_fin;
static volatile uint32_t dc_ticks[2], dc_cpu[2];

static void dc_task(void *p)
{
    int k = (int)(uintptr_t)p;
    for (;;) {
        os_sem_pend(&sem_go, 0);
        uint32_t t0 = fm1_scan_now_ticks();
        for (int r = 0; r < 600; r++)
            bq_ram(dkx[k], dky[k], KN);
        dc_ticks[k] = fm1_scan_now_ticks() - t0;
        dc_cpu[k] = current_cpu_id();
        os_sem_post(&sem_fin);
    }
}

static void bench_nucleos(void)
{
    static int creadas;
    if (!creadas) {
        os_sem_create(&sem_go, 0);
        os_sem_create(&sem_fin, 0);
        os_task_create(dc_task, (void *)0, 27, 1024, 0, "bench0");
        os_task_create(dc_task, (void *)1, 27, 1024, 0, "bench1");
        creadas = 1;
    }
    /* una sola tarea */
    uint32_t t0 = fm1_scan_now_ticks();
    os_sem_post(&sem_go);
    os_sem_pend(&sem_fin, 0);
    uint32_t uno = fm1_scan_now_ticks() - t0;
    /* las dos a la vez */
    t0 = fm1_scan_now_ticks();
    os_sem_post(&sem_go);
    os_sem_post(&sem_go);
    os_sem_pend(&sem_fin, 0);
    os_sem_pend(&sem_fin, 0);
    uint32_t dos = fm1_scan_now_ticks() - t0;
    log_mil("un calculo solo        (ms)", ns(uno, 1000), "ms");
    log_mil("dos calculos a la vez  (ms)", ns(dos, 1000), "ms");
    char l[64], *p = txt(l, "nucleos usados: ");
    p = num(p, dc_cpu[0]); p = txt(p, " y "); p = num(p, dc_cpu[1]); *p = 0;
    fm1_log(l);
    log_mil("aceleracion con dos tareas (x)", (uint32_t)((uint64_t)uno * 2000u / (dos ? dos : 1u)), "x");
}

/* 25: la pantalla entera a distintas velocidades del SPI (divisor del reloj del bus) */
static void bench_pantalla(int solo)
{
    static const uint16_t COL[4] = {0xF800u, 0x07E0u, 0x001Fu, 0xFFE0u};
    static const uint8_t BAUD[4] = {4, 3, 2, 1};
    for (int k = 0; k < 4; k++) {
        if (solo >= 0 && k != solo)
            continue;
        JL_SPI1->BAUD = BAUD[k];
        for (int i = 0; i < 240 * 8; i++)
            fila[i] = (uint16_t)(((i % 240) / 30) & 1 ? COL[k] : 0xFFFFu);
        uint32_t t0 = fm1_scan_now_ticks();
        for (int y = 0; y < 240; y += 8)
            fm1_lcd_blit(NULL, 0, y, 240, 8, fila);
        uint32_t t = fm1_scan_now_ticks() - t0;
        char l[64], *p = txt(l, "pantalla SPI baud ");
        p = num(p, BAUD[k]);
        *p = 0;
        log_mil(l, ns(t, 1000), "ms por cuadro (240x240)");
        os_time_dly(150);
    }
    JL_SPI1->BAUD = 4;                                    /* vuelve a lo de siempre (12 MHz) */
}

/* El motor FM por piezas: un operador, uno modulado, uno con realimentación y la voz entera
 * (msfa, core/bm_fm.cpp), en ns por muestra. */
/* 34: una voz de cada máquina del núcleo WAVE (B-MINI, BITTAL, BUNO-6) con el snapshot arg */
static void bench_an(int preset)
{
    enum { REPS = 1000 };
    bm_pattern *p = &L->pat;
    bm_pattern_init(p);
    p->trk[2].engine = ENG_MINI;
    bm_preset_apply(p, 2, preset);
    bm_preset_apply(p, 4, preset);
    bm_preset_apply(p, 5, preset);
    uint32_t t[3];
    bm_wv_bench(p->prm[2], p->prm[4], p->prm[5], p->trk[4].macro, fm1_scan_now_ticks, t, REPS);
    static const char *const NOM[3] = {"B-MINI voz entera      ", "BITTAL voz entera      ", "BUNO-6 voz entera      "};
    for (int i = 0; i < 3; i++) log_mil(NOM[i], ns(t[i], 64u * REPS), "ns/muestra");
}

static void bench_fm(int patch)
{
    enum { REPS = 2000 };
    uint32_t t[5];
    bm_fm_bench(bm_fm_rom[patch % BM_FM_PATCHES], fm1_scan_now_ticks, t, REPS);
    static const char *const NOM[5] = {"FM operador solo      ", "FM operador modulado  ", "FM operador realiment.",
                                       "FM voz entera (6 op)  ", "bucle vacio           "};
    for (int i = 0; i < 5; i++) log_mil(NOM[i], ns(t[i], REPS * 64u), "ns/muestra");
}

extern unsigned int get_malloc_remain_heap_size(void);

static int lab_bench(int cmd, int arg);

int fm1_bench(int cmd, int arg)
{
    if (cmd == 20) return lab_bench(cmd, arg);
    /* malloc del SDK no avisa cuando no alcanza: se mira antes lo que queda (con 16 KiB de margen) */
    if (get_malloc_remain_heap_size() < sizeof(lab_mem) + 16384u) {
        fm1_log("prueba: no hay montón libre para los búferes");
        return -1;
    }
    L = (lab_mem *)malloc(sizeof(lab_mem));
    if (!L) return -1;
    int r = lab_bench(cmd, arg);
    free(L);
    L = NULL;
    return r;
}

static int lab_bench(int cmd, int arg)
{
    switch (cmd) {
    case 20: fm1_prof_reset(); fm1_log("perfil: contadores a cero"); return 0;
    case 21: fm1_log("prueba: codigo flash/RAM"); bench_codigo(); return 0;
    case 22: fm1_log("prueba: datos flash/RAM"); bench_datos(); return 0;
    case 23: fm1_log("prueba: tamano de la cache de datos"); bench_cache(); return 0;
    case 24: fm1_log("prueba: dos nucleos"); bench_nucleos(); return 0;
    case 25: fm1_log("prueba: pantalla"); bench_pantalla(arg ? arg - 1 : -1); return 0;
    case 28: fm1_log("prueba: motor FM"); bench_fm(arg); return 0;
    case 34: fm1_log("prueba: motor ANALOG"); bench_an(arg); return 0;
    }
    return -1;
}
