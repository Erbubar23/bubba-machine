/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Barrido de la matriz por interrupción (TIMER5, 4 kHz, una columna por interrupción ≈ 364
 * barridos completos por segundo) y LEDs de botones y teclas con brillo por tiempo encendido.
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
static uint32_t vueltas;
static volatile uint32_t ticks;
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

/* qué filas de la columna c se prenden en la vuelta v: brillo por fracción de vueltas */
static uint8_t filas_led(uint32_t c, uint32_t v)
{
    uint8_t m = 0;
    for (int r = 1; r <= 4; r++) {
        int id = MAPA.id[r][c];
        if (id < 0)
            continue;
        uint8_t on;
        switch (nivel[id]) {
        case LED_ON: on = 1; break;
        case LED_DIM: on = (v & 15u) == 0; break;                 /* 1/16 */
        case LED_BLINK: on = (v / 91u) & 1u; break;               /* ~2 Hz */
        case LED_BREATH: {
            uint32_t t = (v / 4u) & 255u, b = t < 128u ? t : 255u - t;   /* triángulo de ~2.8 s */
            on = (v & 7u) < (b >> 4); break;
        }
        default: on = 0;
        }
        if (on)
            m |= (uint8_t)(1u << r);
    }
    return m;
}

static ___interrupt void scan_isr(void)
{
    if (!(JL_TIMER5->CON & BIT(15)))
        return;
    JL_TIMER5->CON |= BIT(14);
    uint8_t sig = fm1_scan_column(&scan, filas());
    leds(0);
    columna(sig);
    ticks++;
    if (sig == 0)
        vueltas++;
    leds(filas_led(sig, vueltas));
}

void fm1_scan_start(void)
{
    fm1_keys_init();                         /* filas con pull-up, LEDs y 74HC595 como salidas */
    fm1_scan_init(&scan, &MAPA);
    columna(0);
    uint32_t clk = (uint32_t)clk_get("timer"), prd = clk / SCAN_HZ, psc = 0;
    if (prd > 0xFFFFu) {
        prd = clk / 4u / SCAN_HZ;            /* divisor de 4: código 1 */
        psc = 1;
    }
    request_irq(IRQ_TIMER5_IDX, 3, scan_isr, 0);
    JL_TIMER5->CON = BIT(14);
    JL_TIMER5->CNT = 0;
    JL_TIMER5->PRD = prd;
    JL_TIMER5->CON |= psc << 4;
    JL_TIMER5->CON |= BIT(0);
    en_marcha = 1;
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

void fm1_scan_led(int id, uint8_t level)
{
    if (id >= 0 && id < FM1_IDS)
        nivel[id] = level;
}
