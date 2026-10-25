/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Barrido de la matriz por interrupción (TIMER5, una columna cada 250 us ≈ 364 barridos
 * completos por segundo) y LEDs de botones y teclas con brillo por tiempo encendido.
 *
 * Brillo (2026-10-20): al empezar la columna se prenden todos sus LEDs y cada uno se apaga cuando
 * cumplió su tiempo: tenue ~4 %, medio 15 %, respirando de tenue a pleno, encendido hasta el final.
 * Cada apagado es otra interrupción y puede llegar tarde (USB, flash, otra interrupción): la
 * cuenta del temporizador al entrar dice cuánto, y ese exceso se descuenta en el próximo barrido
 * del mismo LED (si no alcanza, ese barrido no se prende). Antes los tenues eran los últimos
 * ~10 us antes de cambiar de columna y cualquier atraso de esa interrupción los estiraba al doble o
 * al triple: las luces tenues de los botones de menú temblaban. La respiración ya no salta entre
 * pleno y tenue de un barrido a otro (a brillo bajo eran destellos sueltos de 10-15 por segundo).
 * (El ojo ve el brillo en escala logarítmica: un medio al 50 % casi no se distinguía del 100 %.)
 * El tiempo de cada tramo se programa apenas entra la interrupción (cambiar de columna tarda
 * ~30 us: programarlo después hacía que el temporizador se pasara de largo).
 *
 * Cada interrupción: lee las filas de la columna que se eligió en la anterior (ya asentadas),
 * se las pasa a fm1_scan (antirrebote y perillas, código portable probado en PC), apaga los
 * LEDs, elige la columna siguiente en los 74HC595 y prende los LEDs que le tocan. Pines en
 * fm1_hw.h. Los registros de puertos y del timer son los de los encabezados del SDK AC79. */
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "asm/clock.h"
#include "device/gpio.h"
#include "fm1_hw.h"
#include "fm1_scan.h"
#include "bm_types.h"

#define SCAN_HZ 4000u

/* mapa de la matriz: dato medido y publicado por Felucca/X0X (el mismo de fm1_diag.c) */
static const fm1_scan_map MAPA = {
    .id = {
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
        {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
        {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
        { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    },
    /* perilla i: contacto A (columna, fila) y B (columna, fila) */
    .enc_a = {{0, 0}, {2, 0}, {8, 1}, {8, 0}, {6, 0}, {4, 0}, {0, 5}},
    .enc_b = {{1, 0}, {3, 0}, {9, 1}, {9, 0}, {7, 0}, {5, 0}, {1, 5}},
};

static fm1_scan scan;
static uint8_t nivel[FM1_IDS];               /* LED_OFF, LED_DIM, LED_ON, LED_BLINK, LED_BREATH */
static uint32_t vueltas, prd = 1, prd_tenue = 1, prd_medio = 1, cnt_us = 1, minimo = 1;
static volatile uint32_t ticks, isr_n;
static int en_marcha, pausado;

/* la columna en curso: los apagados que faltan (tiempo desde que entró la interrupción de la
 * columna, en ticks del temporizador, y qué filas), lo prendido y desde cuándo */
static uint8_t col, prendidas, ev_n, ev_i, ev_filas[5];
static uint16_t ev_t[5], fila_t[5], t_luz;
static volatile uint32_t col_t, prog;        /* ticks de la columna hasta la última entrada; el período programado */
static int16_t deuda[FM1_NCOL][5];           /* lo que cada LED estuvo prendido de más (ticks) */

#define PA_BIT(n) (1u << (n))

static inline uint8_t filas(void)
{
    uint32_t a = JL_PORTA->IN, b = JL_PORTB->IN;
    uint8_t v = 0;
    if (!(a & PA_BIT(0))) v |= 1u;
    if (!(a & PA_BIT(5))) v |= 2u;
    if (!(a & PA_BIT(6))) v |= 4u;
    if (!(a & PA_BIT(7))) v |= 8u;
    if (!(a & PA_BIT(8))) v |= 16u;
    if (!(b & PA_BIT(7))) v |= 32u;
    return v;
}

static inline void leds(uint8_t rows)        /* bit 1..4 = PA9, PA10, PH6, PH9 */
{
    if (rows & 2u) JL_PORTA->OUT |= PA_BIT(9); else JL_PORTA->OUT &= ~PA_BIT(9);
    if (rows & 4u) JL_PORTA->OUT |= PA_BIT(10); else JL_PORTA->OUT &= ~PA_BIT(10);
    if (rows & 8u) JL_PORTH->OUT |= PA_BIT(6); else JL_PORTH->OUT &= ~PA_BIT(6);
    if (rows & 16u) JL_PORTH->OUT |= PA_BIT(9); else JL_PORTH->OUT &= ~PA_BIT(9);
}

static inline void columna(uint32_t p)       /* 16 bits a los 74HC595, el bit 15 primero */
{
    uint32_t w = 0xFFFFu ^ (1u << p) ^ (p < 2u ? 1u << (11u + p) : 0u);
    for (int i = 15; i >= 0; i--) {
        if ((w >> i) & 1u) JL_PORTA->OUT |= PA_BIT(4); else JL_PORTA->OUT &= ~PA_BIT(4);
        JL_PORTA->OUT |= PA_BIT(3);
        JL_PORTA->OUT &= ~PA_BIT(3);
    }
    JL_PORTA->OUT |= PA_BIT(1);
    JL_PORTA->OUT &= ~PA_BIT(1);
}

/* el tiempo de la fila r de la columna c en la vuelta v, en ticks: 0 apagada, prd encendida todo */
static uint32_t tiempo_led(uint32_t c, int r, uint32_t v)
{
    int id = MAPA.id[r][c];
    if (id < 0)
        return 0;
    switch (nivel[id]) {
    case LED_ON: return prd;
    case LED_DIM: return prd_tenue;
    case LED_MID: return prd_medio;
    case LED_BLINK: return (v / 91u) & 1u ? prd : 0;                    /* ~2 Hz: parpadeo a propósito */
    case LED_BREATH: {                                                  /* de tenue a encendido, ~2.8 s */
        uint32_t t = (v / 4u) & 255u, b = t < 128u ? t : 255u - t;      /* 0..127 */
        return prd_tenue + (prd - prd_tenue) * b * b / (127u * 127u);   /* curva: el ojo es logarítmico */
    }
    default: return 0;
    }
}

/* Columna nueva: cuánto le toca a cada fila (menos lo que se pasó en el barrido anterior) y los
 * apagados ordenados. Devuelve las filas que se prenden. */
static uint8_t columna_plan(uint32_t c, uint32_t v)
{
    uint8_t luz = 0;
    ev_n = ev_i = 0;
    for (int r = 1; r <= 4; r++) {
        uint32_t t = tiempo_led(c, r, v);
        fila_t[r] = 0;
        if (!t)
            continue;
        uint8_t m = (uint8_t)(1u << r);
        if (t >= prd) { luz |= m; continue; }                           /* hasta el final de la columna */
        int32_t q = (int32_t)t - deuda[c][r];
        if (q < (int32_t)minimo / 2) {                                  /* debe más de lo que le toca: esta vez no */
            deuda[c][r] = (int16_t)(deuda[c][r] - (int32_t)t);
            continue;
        }
        if (q < (int32_t)minimo) q = (int32_t)minimo;
        if (q > (int32_t)(prd - 4u * minimo)) q = (int32_t)(prd - 4u * minimo);
        fila_t[r] = (uint16_t)t;
        luz |= m;
        int i = ev_n;                                                   /* insertar ordenado */
        while (i > 0 && ev_t[i - 1] > q) { ev_t[i] = ev_t[i - 1]; ev_filas[i] = ev_filas[i - 1]; i--; }
        ev_t[i] = (uint16_t)q; ev_filas[i] = m; ev_n++;
    }
    for (int i = 1; i < ev_n;) {                                        /* casi al mismo tiempo: un solo apagado */
        if ((uint32_t)(ev_t[i] - ev_t[i - 1]) < minimo) {
            ev_filas[i - 1] |= ev_filas[i];
            for (int k = i; k + 1 < ev_n; k++) { ev_t[k] = ev_t[k + 1]; ev_filas[k] = ev_filas[k + 1]; }
            ev_n--;
        } else i++;
    }
    return luz;
}

/* Perro guardián del bucle principal. Si el bucle no late en 3 s, se anotan la miga y el
 * sub-paso en los últimos 8 bytes del bloque del SDK para actualizaciones (UPDATA_BEG + 120 =
 * 0x01C7FDF8: el registro de actualización ocupa 8 + 112 bytes; esa RAM sobrevive a un reinicio
 * por software, igual que la marca del modo del chip) y se reinicia. El próximo arranque lo
 * anota y lo muestra la consola. Así un cuelgue se recupera solo y dice dónde fue. */
#define PERRO_ADR  0x01C7FDF8u
#define PERRO_MAG  0xB0BAu
volatile char fm1_miga_act[4] = {'-', '-', '-', '-'};
static volatile uint32_t latido, perro_on;
static uint32_t visto, quieto;

void fm1_latido(void) { latido++; }
int fm1_perro(int on) { int era = (int)perro_on; perro_on = (uint32_t)on; quieto = 0; return era; }

static void perro(void)
{
    if (!perro_on || latido != visto) {
        visto = latido;
        quieto = 0;
        return;
    }
    if (++quieto < 3u)
        return;
    volatile uint8_t *m = (volatile uint8_t *)PERRO_ADR;
    m[0] = (uint8_t)PERRO_MAG; m[1] = (uint8_t)(PERRO_MAG >> 8);
    for (int i = 0; i < 4; i++)
        m[2 + i] = (uint8_t)fm1_miga_act[i];
    m[6] = m[7] = 0;
    cpu_reset();
}

/* una falla del procesador antes de este arranque (hal/fm1/sdk/wl82_debug.c la dejó): 1 y sus datos */
int fm1_excepcion_previa(uint32_t *pc, uint8_t *cpu_emu, uint8_t *dbg)
{
    volatile uint8_t *m = (volatile uint8_t *)PERRO_ADR;
    if (m[0] != 0xBBu || m[1] != 0xB0u)
        return 0;
    *pc = (uint32_t)m[2] | (uint32_t)m[3] << 8 | (uint32_t)m[4] << 16 | (uint32_t)m[5] << 24;
    *cpu_emu = m[6];
    *dbg = m[7];
    m[0] = m[1] = 0;
    return 1;
}

int fm1_cuelgue_previo(char out[5])
{
    volatile uint8_t *m = (volatile uint8_t *)PERRO_ADR;
    int hay = m[0] == (uint8_t)PERRO_MAG && m[1] == (uint8_t)(PERRO_MAG >> 8);
    if (hay)
        for (int i = 0; i < 4; i++)
            out[i] = (char)m[2 + i];
    out[4] = 0;
    m[0] = m[1] = 0;
    return hay;
}

/* Lo que se programa: el próximo apagado o, si no queda ninguno, el final de la columna. Los
 * tiempos van desde la entrada de la interrupción de la columna; ahora = cuándo entró esta (la
 * cuenta del temporizador empezó en 0 ahí) y ya va CNT desde entonces.
 * La cuenta es de 32 bits: si ya pasó el límite cuando se escribe (otra interrupción o un fallo de
 * caché entre leer CNT y escribir PRD), el temporizador sigue hasta dar la vuelta, ~71 s sin teclas
 * ni luces (pasó en el aparato cambiando presets de la BX-7, 2026-10-21). Por eso se revisa. */
static void programar(uint32_t ahora)
{
    uint32_t fin = ev_i < ev_n ? t_luz + ev_t[ev_i] : prd, p = minimo;
    for (int k = 0; k < 4; k++) {
        uint32_t ya = JL_TIMER5->CNT;
        p = fin > ahora + ya + minimo ? fin - ahora : ya + minimo;
        JL_TIMER5->PRD = p;
        if (JL_TIMER5->CNT < p)
            break;
    }
    if (JL_TIMER5->CNT >= p) {               /* no hubo caso: la columna se corre un poco, nada más */
        JL_TIMER5->CNT = 0;
        p = minimo;
        JL_TIMER5->PRD = p;
    }
    prog = p;
}

static ___interrupt void scan_isr(void)
{
    if (!(JL_TIMER5->CON & BIT(15)))
        return;
    /* cuánto llegó tarde (la cuenta siguió desde el límite), y el tramo siguiente se mide desde
     * ahora: si no, con un atraso largo la cuenta ya habría pasado el límite nuevo y el
     * temporizador seguiría hasta dar la vuelta (en el aparato el barrido bajó a ~180 columnas/s) */
    uint32_t tarde = JL_TIMER5->CNT;
    JL_TIMER5->CNT = 0;
    JL_TIMER5->CON |= BIT(14);
    isr_n++;
    uint32_t ahora = col_t + prog + tarde;   /* desde que entró la interrupción de esta columna */
    if (tarde / cnt_us > fm1_stat[FM1_STAT_SCAN_LAT])
        fm1_stat[FM1_STAT_SCAN_LAT] = tarde / cnt_us;
    if (ev_i < ev_n) {                       /* un apagado: lo que estuvo de más queda debiendo */
        uint8_t m = ev_filas[ev_i++];
        prendidas &= (uint8_t)~m;
        leds(prendidas);
        for (int r = 1; r <= 4; r++)
            if ((m >> r) & 1u) {
                int32_t d = deuda[col][r] + (int32_t)(ahora - t_luz) - (int32_t)fila_t[r];
                deuda[col][r] = (int16_t)(d > (int32_t)prd ? (int32_t)prd : d < -(int32_t)prd ? -(int32_t)prd : d);
            }
        col_t = ahora;
        programar(ahora);
        return;
    }
    uint8_t sig = fm1_scan_column(&scan, filas());   /* columna nueva */
    leds(0);
    columna(sig);
    ticks++;
    if (sig == 0)
        vueltas++;
    if ((ticks & 4095u) == 0)                 /* ~1 vez por segundo */
        perro();
    col = sig;
    prendidas = columna_plan(sig, vueltas);
    t_luz = (uint16_t)JL_TIMER5->CNT;        /* los tiempos de cada LED cuentan desde que se prende */
    leds(prendidas);
    col_t = 0;
    programar(0);
    uint32_t us = JL_TIMER5->CNT / cnt_us;                   /* lo que tardó esta interrupción */
    if (us > fm1_stat[FM1_STAT_SCANISR_MAX])
        fm1_stat[FM1_STAT_SCANISR_MAX] = us;
}

void fm1_scan_start(void)
{
    fm1_keys_init();                         /* filas con pull-up, LEDs y 74HC595 como salidas */
    fm1_scan_init(&scan, &MAPA);
    columna(0);
    uint32_t clk = (uint32_t)clk_get("timer"), psc = 0;
    prd = clk / SCAN_HZ;
    if (prd > 0xFFFFu) {
        prd = clk / 4u / SCAN_HZ;            /* divisor de 4: código 1 */
        psc = 1;
    }
    prd = prd ? prd : 1;
    prd_tenue = prd / 25u ? prd / 25u : 1u;  /* ~4 %: lo disponible */
    prd_medio = prd * 15u / 100u;            /* 15 %: lo que tiene contenido (al 50 % casi no se distinguía del 100 %) */
    cnt_us = prd / 250u ? prd / 250u : 1u;
    minimo = 2u * cnt_us;                    /* dos apagados a menos de 2 us van juntos */
    ev_n = ev_i = 0;
    col_t = 0;
    prog = prd;
    /* por encima del audio y del USB (como el firmware de fábrica): un atraso aquí es luz de más */
    request_irq(IRQ_TIMER5_IDX, 5, scan_isr, 0);
    JL_TIMER5->CON = BIT(14);
    JL_TIMER5->CNT = 0;
    JL_TIMER5->PRD = prd;
    JL_TIMER5->CON |= psc << 4;
    JL_TIMER5->CON |= BIT(0);
    en_marcha = 1;
}

void fm1_scan_pause(int on)
{
    static uint32_t con;
    if (!en_marcha)
        return;
    if (on) {
        con = JL_TIMER5->CON;
        JL_TIMER5->CON = BIT(14);             /* parado y sin aviso pendiente */
        leds(0);                              /* sin barrido, una columna quedaría prendida a pleno */
        pausado = 1;
    } else {
        if (JL_TIMER5->CNT >= JL_TIMER5->PRD) /* (ver programar: con la cuenta pasada daría la vuelta) */
            JL_TIMER5->CNT = 0;
        JL_TIMER5->CON = (con & ~BIT(15)) | BIT(14);
        pausado = 0;
    }
}

/* La red de abajo, desde el bucle principal: si el barrido no cambió de columna en 20 ms sin estar
 * en pausa, se rearma (la cuenta a 0 y el temporizador prendido) y se cuenta. Devuelve 1 si rearmó. */
int fm1_scan_vigilar(uint32_t ms)
{
    static uint32_t visto_t, visto_ms;
    if (!en_marcha || pausado || ticks != visto_t) {
        visto_t = ticks;
        visto_ms = ms;
        return 0;
    }
    if (ms - visto_ms < 20u)
        return 0;
    int hubo = 0;
    CPU_CRITICAL_ENTER();
    if (ticks == visto_t && !pausado) {
        JL_TIMER5->CNT = 0;
        JL_TIMER5->CON |= BIT(0);
        fm1_stat[FM1_STAT_SCAN_KICK]++;
        hubo = 1;
    }
    CPU_CRITICAL_EXIT();
    visto_ms = ms;
    return hubo;
}

void fm1_scan_stop(void)
{
    if (!en_marcha)
        return;
    JL_TIMER5->CON = BIT(14);
    leds(0);
    en_marcha = pausado = 0;
}

int fm1_scan_edge(int *down)
{
    CPU_CRITICAL_ENTER();
    int id = fm1_scan_next_edge(&scan, down);
    CPU_CRITICAL_EXIT();
    return id;
}

int fm1_scan_encoder(int e)
{
    CPU_CRITICAL_ENTER();
    int v = fm1_scan_take_encoder(&scan, e);
    CPU_CRITICAL_EXIT();
    return v;
}

int fm1_scan_is_down(int id)
{
    return id >= 0 && id < FM1_IDS && ((scan.down_bits[id >> 5] >> (id & 31)) & 1u);
}

uint32_t fm1_scan_ticks(void) { return ticks; }

/* ticks del timer (clk_get("timer"), 60 MHz) desde que arrancó el barrido: para medir tramos
 * cortos con resolución de ~17 ns (fm1_bench.c, core/bm_prof.h). Da la vuelta cada ~71 s. */
/* lo que va de la columna en curso, en ticks (hasta prd) */
static uint32_t en_columna(uint32_t *n)
{
    uint32_t a, c, b, pend, base, p;
    do {
        a = isr_n;
        c = JL_TIMER5->CNT;
        pend = JL_TIMER5->CON & BIT(15);
        base = col_t;
        p = prog;
        *n = ticks;
        b = isr_n;
    } while (a != b);
    base += pend ? p + c : c;
    return base < prd ? base : prd;
}

uint32_t fm1_scan_now_ticks(void)
{
    uint32_t n, c = en_columna(&n);
    return n * prd + c;
}

uint32_t fm1_scan_ticks_hz(void) { return prd * 4000u; }

/* microsegundos desde que arrancó el barrido: columnas * 250 + lo que va de la actual */
uint32_t fm1_scan_now_us(void)
{
    uint32_t n, c = en_columna(&n);
    return n * 250u + c / cnt_us;
}

void fm1_scan_led(int id, uint8_t level)
{
    if (id >= 0 && id < FM1_IDS)
        nivel[id] = level;
}
