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

/* Una interrupción por núcleo: IRQ_SOFT2 habilitada solo en el 0 e IRQ_SOFT3 solo en el 1. Cada
 * IRQ_SOFTn es un único pendiente, el bit n de ILAT_SET (0x1EEF1A0), y la toma el núcleo que la tiene
 * habilitada. Así lo hace el sistema del SDK (vPortYield, OS_ClrPending: bit 7 - núcleo, SOFT7 y
 * SOFT6 para cambiar de tarea). Con una sola registrada en los dos núcleos la tomaba el que la pedía
 * (medido en 0.6.3: casi todos los trabajos en serie). */
#define C1_SOFT     2u                              /* IRQ_SOFT2 + núcleo */
#define C1_IDX(c)   (IRQ_SOFT0_IDX + C1_SOFT + (c))
#define C1_BIT(c)   (1u << (C1_SOFT + (c)))
#define C1_ILAT_SET (*(volatile uint32_t *)0x1EEF1A0u)
#define C1_ILAT_CLR (*(volatile uint32_t *)0x1EEF1A4u)
#define C1_PRIO     1                               /* bajo: lo del sistema en el núcleo 1 puede interrumpirla */
#define C1_SYNC()   __asm__ volatile("csync" ::: "memory")
#define C1_CHECK(fn, arg, job) ((uint32_t)(uintptr_t)(fn) ^ (arg) ^ (job) * 0x9E3779B1u)

/* cada núcleo escribe en líneas de caché propias (el diseño de Jangada, vía Felucca) */
typedef struct {
    volatile uint32_t job, arg;                     /* núcleo 0: job + 1 entrega fn(arg) */
    void (*volatile fn)(uint32_t);
    volatile uint32_t check;
    volatile uint32_t sesion;                       /* núcleo 0: render en curso (0 = ninguno) */
    volatile uint32_t desde;                        /* el núcleo del que pide (el del audio, ahora) */
    uint32_t pad0[10];
    volatile uint32_t done, cpu, bad;               /* núcleo 1: el último trabajo terminado */
    volatile uint32_t dentro, dentro_cpu;           /* núcleo 1: la sesión en la que gira, y dónde */
    volatile uint32_t mismo;                        /* trabajos que corrieron en el núcleo del que pidió */
    uint32_t pad1[10];
} c1_mb_t;
static c1_mb_t mb __attribute__((aligned(64)));
static volatile uint8_t modo;                       /* FM1_C1_* */
static uint32_t espera_max;                         /* ticks del barrido */

/* el trabajo pendiente, si hay uno y el buzón se leyó bien (en RAM, como el giro) */
SEC_USED(.volatile_ram_code) static void c1_uno(void)
{
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
    if (mb.cpu == mb.desde)                         /* en serie: el que pidió está esperando aquí mismo */
        mb.mismo++;
    C1_SYNC();
    mb.done = job;
    C1_SYNC();
}

/* Sesión (0.6.3): durante un render, el núcleo 1 no sale de la interrupción. Hace el primer trabajo
 * y se queda girando por los siguientes hasta que el núcleo 0 cierra la sesión, como el bucle de
 * Felucca y X0X. Cada traspaso cuesta un csync en vez de una interrupción (un render tiene ~6, uno
 * por tramo de 64 muestras y por paso del secuenciador). Las tareas del núcleo 1 (la pantalla)
 * siguen entre renders. Si en ~C1_GIRO vueltas no llega nada, sale solo.
 *
 * El giro va en RAM y no lee ningún reloj (como el bucle de Felucca, .c1_text): la primera versión
 * giraba desde la flash y leía el temporizador en cada vuelta, y el núcleo 0 tardaba hasta 20 veces
 * más en su render (medido: 2137 %). */
#define C1_GIRO 80000u                          /* ~6 ms (medido: 2 000 000 vueltas = ~145 ms) */

SEC_USED(.volatile_ram_code) static ___interrupt void c1_isr(void)
{
    C1_ILAT_CLR = C1_BIT(current_cpu_id());
    c1_uno();
    C1_SYNC();
    uint32_t s = mb.sesion;
    if (!s)
        return;
    mb.dentro_cpu = (uint32_t)current_cpu_id();
    mb.dentro = s;
    C1_SYNC();
    for (uint32_t v = 0; v < C1_GIRO; v++) {
        C1_SYNC();
        if (mb.sesion != s || mb.desde == mb.dentro_cpu)   /* cerrada, o el audio se mudó a este núcleo */
            break;
        if (mb.job != mb.done) {
            c1_uno();
            v = 0;
        }
    }
    mb.dentro = 0;
    C1_SYNC();
    c1_uno();                                       /* uno que llegó justo al salir */
}

/* abre (id != 0) o cierra (0) la sesión de un render */
void fm1_cpu1_sesion(uint32_t id)
{
    mb.sesion = id;
    C1_SYNC();
}

int fm1_cpu1_run(void (*fn)(uint32_t), uint32_t arg)
{
    if (modo != FM1_C1_LISTO && modo != FM1_C1_PRUEBA)
        return 0;
    /* La tarea del audio no tiene núcleo fijo (el planificador del SDK la mueve): el trabajo va al
     * otro, el que no la está corriendo. En 0.6.2 iba siempre al 1 y, con el audio ahí, se hacía en
     * serie dentro de su propia interrupción (o, girando, esperaba a un render que no avanzaba). */
    uint32_t yo = (uint32_t)current_cpu_id(), otro = yo ^ 1u;
    uint32_t job = mb.job + 1u;
    mb.desde = yo;
    mb.fn = fn;
    mb.arg = arg;
    mb.check = C1_CHECK(fn, arg, job);
    C1_SYNC();
    mb.job = job;
    C1_SYNC();
    if (!(mb.sesion && mb.dentro == mb.sesion && mb.dentro_cpu == otro))   /* si ya gira allá, lo ve solo */
        C1_ILAT_SET = C1_BIT(otro);
    return 1;
}

static void dejar(uint8_t por, const char *msg)
{
    modo = por;
    fm1_stat[FM1_STAT_PAR_MODO] = por;
    fm1_log(msg);
}

/* hasta `vueltas` mirando si terminó, en RAM y sin tocar la flash ni el reloj: el otro núcleo
 * está calculando desde la flash y no hay que quitarle el bus */
SEC_USED(.volatile_ram_code) static int c1_listo(uint32_t vueltas)
{
    for (uint32_t v = 0; v < vueltas; v++) {
        C1_SYNC();
        if (mb.done == mb.job || mb.bad)
            return 1;
    }
    return 0;
}

/* 0 = terminó; -1 = no contestó en `us` o leyó mal: desde ahora un núcleo (si el trabajo sigue
 * corriendo en el núcleo 1, el que llama no debe tocar lo suyo hasta que termine: fm1_cpu1_idle) */
int fm1_cpu1_wait(uint32_t us)
{
    uint32_t hz = fm1_scan_ticks_hz(), t0 = fm1_scan_now_ticks();
    uint32_t lim = hz ? us * (hz / 1000000u) : 0;
    for (uint32_t vueltas = 0;; vueltas++) {
        c1_listo(2000u);
        C1_SYNC();
        if (mb.done == mb.job) {
            uint32_t d = fm1_scan_now_ticks() - t0;
            /* leído desde otro núcleo, el reloj del barrido puede ir un pelo atrás y la resta da la
             * vuelta (la consola mostró 71 s): eso no es una espera */
            if (d > espera_max && (!hz || d < hz)) {
                espera_max = d;
                fm1_stat[FM1_STAT_PAR_ESPERA_MAX] = hz ? d / (hz / 1000000u) : 0;
            }
            return 0;
        }
        if (mb.bad) {
            if (modo == FM1_C1_LISTO) dejar(FM1_C1_LEYO_MAL, "nucleo 1: leyo mal un trabajo, sigo con uno");
            return -1;
        }
        uint32_t d = fm1_scan_now_ticks() - t0;
        if (d >= 0x80000000u)                       /* el reloj, leído desde otro núcleo, un pelo atrás */
            d = 0;
        if (hz ? d > lim : vueltas > us / 16u) {
            if (modo == FM1_C1_LISTO) dejar(FM1_C1_TARDE, "nucleo 1: no contesto a tiempo, sigo con uno");
            return -1;
        }
    }
}

/* el último trabajo ya terminó (o el núcleo 1 nunca lo tomó) */
int fm1_cpu1_idle(void) { C1_SYNC(); return mb.done == mb.job; }
int fm1_cpu1_ok(void) { return modo == FM1_C1_LISTO; }
int fm1_cpu1_cpu(void) { return (int)mb.cpu; }
uint32_t fm1_cpu1_mismo(void) { return mb.mismo; }

static char *num(char *p, uint32_t v)
{
    char t[10]; int n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *p++ = t[--n];
    return p;
}

static char *txt(char *p, const char *s) { while (*s) *p++ = *s++; return p; }

/* Registra una interrupción en cada núcleo y prueba con 32 trabajos vacíos que vayan al otro (el
 * que pide puede cambiar de núcleo entre trabajo y trabajo: cuenta los de cada lado). Va con el
 * barrido andando: mide el tiempo con él. */
int fm1_cpu1_init(void)
{
    char l[80], *p;
    if (modo == FM1_C1_LISTO)
        return 0;
    request_irq(C1_IDX(0), C1_PRIO, c1_isr, 0);
    request_irq(C1_IDX(1), C1_PRIO, c1_isr, 1);
    modo = FM1_C1_PRUEBA;
    mb.bad = 0;
    uint32_t bien = 0, serie = 0, en[2] = {0, 0};
    for (int k = 0; k < 32; k++) {
        mb.cpu = 0xFFu;
        fm1_cpu1_run(NULL, 0);
        if (fm1_cpu1_wait(2000u) != 0)
            break;
        if (mb.cpu > 1u) continue;
        en[mb.cpu]++;
        if (mb.cpu != mb.desde) bien++; else serie++;
    }
    p = txt(l, "nucleo 1: prueba, al otro "); p = num(p, bien); p = txt(p, ", en serie "); p = num(p, serie);
    p = txt(p, " (nucleo 0: "); p = num(p, en[0]); p = txt(p, ", nucleo 1: "); p = num(p, en[1]); p = txt(p, ")");
    *p = 0;
    fm1_log(l);
    C1_ILAT_CLR = C1_BIT(0) | C1_BIT(1);
    mb.done = mb.job;                               /* si quedó uno colgado, se descarta */
    if (bien >= 24u) {
        modo = FM1_C1_LISTO;
        fm1_stat[FM1_STAT_PAR_MODO] = FM1_C1_LISTO;
        return 0;
    }
    modo = FM1_C1_NO;
    fm1_stat[FM1_STAT_PAR_MODO] = FM1_C1_NO;
    fm1_log("nucleo 1: no contesto al arrancar, TWO CORES usa uno");
    return -1;
}
