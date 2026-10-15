/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* El segundo núcleo del FM-1 para el audio (SISTEMA > TWO CORES), dentro del sistema del SDK.
 *
 * X0X, Felucca y Melodee arrancan el núcleo 1 ellos mismos y lo dejan esperando en un bucle en
 * RAM: no usan el sistema operativo. Bubba sí: el FreeRTOS del SDK ya reparte sus tareas en los
 * dos núcleos y no deja fijar una. En 0.6.1 el reparto era una tarea despertada con semáforos y en
 * el aparato se turnaba con la del audio en vez de correr a la vez (con TWO CORES la carga subía de
 * ~105 a 123 %, docs/ARQUITECTURA_FM1.md §3).
 *
 * Aquí el núcleo 1 trabaja dentro de una interrupción por software registrada en él (request_irq
 * con cpu 1; el SDK usa IRQ_SOFT4 y IRQ_SOFT5 para cambiar de tarea): el núcleo 0 deja el trabajo
 * en un buzón, levanta la interrupción en el banco del núcleo 1 (ILAT_SET) y espera girando, sin
 * planificador de por medio. Del buzón de X0X y Felucca (hal/fm1_cpu1.h, GPL-3.0) vienen la palabra
 * de control de cada trabajo, el csync a cada lado, el tiempo máximo y la regla: si algo sale mal
 * (no contesta a tiempo o lee mal un trabajo), un núcleo hasta el próximo arranque. Algunos FM-1
 * leen mal la RAM desde el núcleo 1 (fm1-x0x#10, medido por Jangada).
 *
 * Lo que no se sabe del aparato se prueba al arrancar (fm1_cpu1_init): qué bit de ILAT_SET levanta
 * IRQ_SOFT2 (las macros del SDK sugieren 7 - n, CPU_TASK_SW) y si el núcleo 0 puede levantarla en
 * el banco del 1. Si no contesta, TWO CORES se queda en un núcleo y la consola lo dice. Que la
 * interrupción guarde bien la coma flotante de la tarea que interrumpe lo mide la prueba 35 del
 * laboratorio (fm1_bench.c). */
#include <stdint.h>
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "fm1_hw.h"

void fm1_log(const char *s);

#define C1_SOFT     2u                              /* IRQ_SOFT2 */
#define C1_IDX      (IRQ_SOFT0_IDX + C1_SOFT)
#define C1_PRIO     1                               /* bajo: lo del sistema en el núcleo 1 puede interrumpirla */
#define C1_SYNC()   __asm__ volatile("csync" ::: "memory")
#define C1_CHECK(fn, arg, job) ((uint32_t)(uintptr_t)(fn) ^ (arg) ^ (job) * 0x9E3779B1u)

/* cada núcleo escribe en líneas de caché propias (el diseño de Jangada, vía Felucca) */
typedef struct {
    volatile uint32_t job, arg;                     /* núcleo 0: job + 1 entrega fn(arg) */
    void (*volatile fn)(uint32_t);
    volatile uint32_t check;
    uint32_t pad0[12];
    volatile uint32_t done, cpu, bad;               /* núcleo 1: el último trabajo terminado */
    uint32_t pad1[13];
} c1_mb_t;
static c1_mb_t mb __attribute__((aligned(64)));
static uint32_t bit;                                /* el bit de ILAT_SET que levanta IRQ_SOFT2 */
static volatile uint8_t modo;                       /* FM1_C1_* */
static uint32_t espera_max;                         /* ticks del barrido */

static ___interrupt void c1_isr(void)
{
    q32DSP(current_cpu_id())->ILAT_CLR = bit;
    C1_SYNC();
    uint32_t job = mb.job;
    if (job == mb.done)                             /* nada nuevo (una interrupción repetida) */
        return;
    void (*fn)(uint32_t) = mb.fn;
    uint32_t arg = mb.arg;
    if (mb.check != C1_CHECK(fn, arg, job)) {       /* leyó mal el buzón: el núcleo 0 lo deja */
        mb.bad++;
        C1_SYNC();
        return;
    }
    if (fn)
        fn(arg);
    mb.cpu = current_cpu_id();
    C1_SYNC();
    mb.done = job;
    C1_SYNC();
}

int fm1_cpu1_run(void (*fn)(uint32_t), uint32_t arg)
{
    if (modo != FM1_C1_LISTO && modo != FM1_C1_PRUEBA)
        return 0;
    uint32_t job = mb.job + 1u;
    mb.fn = fn;
    mb.arg = arg;
    mb.check = C1_CHECK(fn, arg, job);
    C1_SYNC();
    mb.job = job;
    C1_SYNC();
    q32DSP(1)->ILAT_SET = bit;
    return 1;
}

static void dejar(uint8_t por, const char *msg)
{
    modo = por;
    fm1_stat[FM1_STAT_PAR_MODO] = por;
    fm1_log(msg);
}

/* 0 = terminó; -1 = no contestó en `us` o leyó mal: desde ahora un núcleo (si el trabajo sigue
 * corriendo en el núcleo 1, el que llama no debe tocar lo suyo hasta que termine: fm1_cpu1_idle) */
int fm1_cpu1_wait(uint32_t us)
{
    uint32_t hz = fm1_scan_ticks_hz(), t0 = fm1_scan_now_ticks();
    uint32_t lim = hz ? us * (hz / 1000000u) : 0;
    for (uint32_t vueltas = 0;; vueltas++) {
        C1_SYNC();
        if (mb.done == mb.job) {
            uint32_t d = fm1_scan_now_ticks() - t0;
            if (d > espera_max) {
                espera_max = d;
                fm1_stat[FM1_STAT_PAR_ESPERA_MAX] = hz ? d / (hz / 1000000u) : 0;
            }
            return 0;
        }
        if (mb.bad) {
            if (modo == FM1_C1_LISTO) dejar(FM1_C1_LEYO_MAL, "nucleo 1: leyo mal un trabajo, sigo con uno");
            return -1;
        }
        if (hz ? fm1_scan_now_ticks() - t0 > lim : vueltas > us * 64u) {
            if (modo == FM1_C1_LISTO) dejar(FM1_C1_TARDE, "nucleo 1: no contesto a tiempo, sigo con uno");
            return -1;
        }
    }
}

/* el último trabajo ya terminó (o el núcleo 1 nunca lo tomó) */
int fm1_cpu1_idle(void) { C1_SYNC(); return mb.done == mb.job; }
int fm1_cpu1_ok(void) { return modo == FM1_C1_LISTO; }
int fm1_cpu1_cpu(void) { return (int)mb.cpu; }

static char *hex(char *p, uint32_t v)
{
    for (int i = 28; i >= 0; i -= 4) *p++ = "0123456789ABCDEF"[(v >> i) & 15u];
    return p;
}

/* Registra la interrupción en el núcleo 1 y prueba los dos bits posibles con un trabajo vacío.
 * Con el bit equivocado, en el banco del núcleo 1 no se levanta nada que esté habilitado (la 5 es
 * del núcleo 0). Va con el barrido andando: mide el tiempo con él. */
int fm1_cpu1_init(void)
{
    static const uint32_t CAND[2] = {1u << C1_SOFT, 1u << (7u - C1_SOFT)};
    char l[64], *p;
    if (modo == FM1_C1_LISTO)
        return 0;
    p = l;
    for (const char *s = "nucleo 1: vector SOFT2 antes 0x"; *s; s++) *p++ = *s;
    p = hex(p, *(volatile uint32_t *)(ISR_ENTRY + 4u * C1_IDX));
    *p = 0;
    fm1_log(l);
    request_irq(C1_IDX, C1_PRIO, c1_isr, 1);
    for (int k = 0; k < 2; k++) {
        bit = CAND[k];
        modo = FM1_C1_PRUEBA;
        mb.bad = 0;
        mb.cpu = 0xFFu;
        fm1_cpu1_run(NULL, 0);
        if (fm1_cpu1_wait(2000u) == 0 && mb.cpu == 1u) {
            modo = FM1_C1_LISTO;
            fm1_stat[FM1_STAT_PAR_MODO] = FM1_C1_LISTO;
            p = l;
            for (const char *s = "nucleo 1: listo, ILAT bit 0x"; *s; s++) *p++ = *s;
            p = hex(p, bit);
            *p = 0;
            fm1_log(l);
            return 0;
        }
        q32DSP(1)->ILAT_CLR = bit;
        mb.done = mb.job;                           /* el trabajo de prueba se descarta */
    }
    modo = FM1_C1_NO;
    fm1_stat[FM1_STAT_PAR_MODO] = FM1_C1_NO;
    fm1_log("nucleo 1: no contesto al arrancar, TWO CORES usa uno");
    return -1;
}
