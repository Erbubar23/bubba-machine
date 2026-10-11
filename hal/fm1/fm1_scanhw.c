/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Barrido de la matriz por interrupción (TIMER5, una columna cada 250 us ≈ 364 barridos
 * completos por segundo) y LEDs de botones y teclas con brillo por tiempo encendido.
 *
 * Brillo: cada columna dura 250 us en dos tramos. Al empezar se cambia de columna y se prenden
 * los LEDs encendidos; en los últimos ~31 us se suman los tenues. Tenue = 1/8 del brillo, sin
 * parpadeo porque cada LED se prende en cada barrido. El tiempo de cada tramo se programa apenas
 * entra la interrupción (cambiar de columna tarda ~30 us: programarlo después hacía que el
 * temporizador se pasara de largo y el barrido bajó a ~70 columnas por segundo). (Antes el tenue se hacía saltando
 * barridos: a 1/16 parpadeaba y a 1/2 no se distinguía del encendido; en el aparato las teclas
 * de la batería se veían todas iguales.)
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
static uint32_t vueltas, prd = 1, prd_tenue = 1, cnt_us = 1;
static volatile uint32_t ticks, isr_n;
static volatile uint8_t tramo, leds_llenos, leds_tenues;
static int en_marcha;

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

/* filas de la columna c para la vuelta v: *todas = las que se prenden al empezar la columna,
 * devuelve las que siguen prendidas después del tramo tenue */
static uint8_t filas_led(uint32_t c, uint32_t v, uint8_t *todas)
{
    uint8_t lleno = 0, tenue = 0;
    for (int r = 1; r <= 4; r++) {
        int id = MAPA.id[r][c];
        if (id < 0)
            continue;
        uint8_t m = (uint8_t)(1u << r);
        switch (nivel[id]) {
        case LED_ON: lleno |= m; break;
        case LED_DIM: tenue |= m; break;
        case LED_BLINK: if ((v / 91u) & 1u) lleno |= m; break;            /* ~2 Hz: parpadeo a propósito */
        case LED_BREATH: {                                              /* de tenue a encendido, ~2.8 s */
            uint32_t t = (v / 4u) & 255u, b = t < 128u ? t : 255u - t;  /* 0..127 */
            if (((v * 37u) & 127u) < b) lleno |= m; else tenue |= m;    /* fracción de vueltas a pleno */
            break;
        }
        default: break;
        }
    }
    *todas = (uint8_t)(lleno | tenue);
    return lleno;
}

static ___interrupt void scan_isr(void)
{
    if (!(JL_TIMER5->CON & BIT(15)))
        return;
    JL_TIMER5->CON |= BIT(14);
    isr_n++;
    if (tramo == 1) {                         /* últimos ~31 us de la columna: se suman los tenues */
        JL_TIMER5->PRD = prd_tenue;           /* primero el tiempo: la cuenta ya corre */
        leds(leds_llenos | leds_tenues);
        tramo = 0;
        return;
    }
    JL_TIMER5->PRD = prd - prd_tenue;         /* columna nueva: el resto del tiempo, solo los encendidos */
    uint8_t todas, sig = fm1_scan_column(&scan, filas());
    leds(0);
    columna(sig);
    ticks++;
    if (sig == 0)
        vueltas++;
    leds_llenos = filas_led(sig, vueltas, &todas);
    leds_tenues = (uint8_t)(todas & ~leds_llenos);
    leds(leds_llenos);
    tramo = 1;
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
    prd_tenue = prd / 8u ? prd / 8u : 1u;
    cnt_us = prd / 250u ? prd / 250u : 1u;
    tramo = 0;
    request_irq(IRQ_TIMER5_IDX, 3, scan_isr, 0);
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
    } else {
        JL_TIMER5->CON = (con & ~BIT(15)) | BIT(14);
    }
}

void fm1_scan_stop(void)
{
    if (!en_marcha)
        return;
    JL_TIMER5->CON = BIT(14);
    leds(0);
    en_marcha = 0;
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

/* microsegundos desde que arrancó el barrido: columnas * 250 + lo que va de la actual */
uint32_t fm1_scan_now_us(void)
{
    uint32_t a, c, b, pend, f, us;
    do {
        a = isr_n;
        c = JL_TIMER5->CNT;
        pend = JL_TIMER5->CON & BIT(15);
        f = tramo;
        us = ticks * 250u;
        b = isr_n;
    } while (a != b);
    if (f == 1)                               /* en el tramo largo, desde el inicio de la columna */
        us += pend ? (prd - prd_tenue) / cnt_us : c / cnt_us;
    else                                      /* en el tramo de los tenues, al final */
        us += pend ? 250u : (prd - prd_tenue + c) / cnt_us;
    return us;
}

void fm1_scan_led(int id, uint8_t level)
{
    if (id >= 0 && id < FM1_IDS)
        nivel[id] = level;
}
