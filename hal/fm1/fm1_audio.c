/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Salida de audio del FM-1: ALNK0 (I2S) hacia el codec externo, con DMA de doble buffer.
 *
 * Hechos de la placa (medidos y publicados por Felucca, hugelton/Felucca, fm1_audio.h):
 *   - el codec cuelga de ALNK0, canal 3, estéreo de 24 bits: cada muestra es un int32 con el
 *     valor en los 24 bits de abajo (Felucca escribe Q15 * 2^7, audio.c); la frecuencia queda
 *     en 24 MHz / 544 ≈ 44 117.6 Hz (BM_FS_OUT);
 *   - las líneas de control del codec son PC0, PC1 y PC2 (en alto) y PC6 (habilitación: en
 *     bajo unos ms antes de configurar ALNK0, en alto después);
 *   - ALNK0 usa el grupo de pines 0 (IOMAP_CON5 bits 6-7) y el reloj de audio con la fuente 0
 *     (CLK_CON2 bits 8-11);
 *   - CON2 bit 7 = medio buffer libre (se borra con el bit 3); los bits 4-6 son los avisos de
 *     los otros canales (se borran con los bits 0-2); CON0 bit 15 = la mitad que suena ahora;
 *     CON0 bit 11 = en marcha.
 * Los registros y la interrupción salen de los encabezados del SDK AC79 (Apache-2.0).
 *
 * La interrupción solo anota qué mitad quedó libre y despierta a la tarea "audio" (prioridad
 * 30, above the SDK's sys_event at 29: the highest one FreeRTOS allows here, the one the SDK's own
 * audio_decoder uses), que calcula el bloque del núcleo. A block that took longer than a half
 * (late): the half that is playing is not touched and nobody sleeps; the next free half is
 * computed right away (0.8.1: before, both halves went silent and the task slept a 10 ms tick, so
 * each late block was an audible gap). Only after LATE_SEGUIDOS late blocks in a row (the audio
 * cannot keep up at all) does the task leave the free half silent and give up one tick, so the
 * rest of the system (and the watchdog) still get time. */
#include <string.h>
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "device/gpio.h"
#include "os/os_api.h"
#include "fm1_hw.h"

#define BM_FS_OUT_HZ 44118u          /* core/bm_config.h BM_FS_OUT */

static int32_t *buf;
static uint32_t half_words;
static fm1_audio_render render;
static volatile uint32_t halves, late, seq;
static volatile uint8_t calculando;
static volatile uint8_t libre;
static int en_marcha, tarea, mudo;
static OS_SEM sem;

static void codec_linea(uint32_t io, int v)
{
    gpio_set_die(io, 1);
    gpio_direction_output(io, v);
}

static ___interrupt void alnk0_isr(void)
{
    uint8_t p = JL_ALNK0->CON2;
    if (p & 0x70u)                                        /* avisos de los otros canales */
        JL_ALNK0->CON2 |= (uint8_t)((p >> 4) & 0x07u);
    if (!(p & 0x80u))
        return;
    libre = (uint8_t)(((JL_ALNK0->CON0 >> 15) & 1u) ^ 1u);   /* la mitad que no suena */
    JL_ALNK0->CON2 |= 0x08u;
    seq++;
    os_sem_post(&sem);
}

#define LATE_SEGUIDOS 8u                                  /* ~46 ms without keeping up */
#define AUDIO_PRIO    30                                  /* configMAX_PRIORITIES - 1 */

static void audio_task(void *p)
{
    uint32_t seguidos = 0;
    (void)p;
    for (;;) {
        if (mudo) {                                       /* sin salida: el núcleo corre igual, callado */
            if (render && en_marcha)
                render(buf, half_words / 2u);
            halves++;
            os_time_dly(1);
            continue;
        }
        os_sem_pend(&sem, 0);
        uint32_t s = seq, t0 = fm1_scan_now_us();
        int32_t *h = buf + (uint32_t)libre * half_words;
        calculando = 1;
        if (render && en_marcha)
            render(h, half_words / 2u);
        else
            memset(h, 0, half_words * 4u);
        halves++;
        calculando = 0;
        uint32_t dt = fm1_scan_now_us() - t0;
        if (dt > 1000000u)
            dt = 0;                                       /* el reloj no anda (barrido apagado) */
        fm1_stat[FM1_STAT_RENDER_LAST] = dt;
        if (dt > fm1_stat[FM1_STAT_RENDER_MAX])
            fm1_stat[FM1_STAT_RENDER_MAX] = dt;
        fm1_stat[FM1_STAT_RENDER_AVG] = (fm1_stat[FM1_STAT_RENDER_AVG] * 15u + dt) / 16u;
        fm1_stat[FM1_STAT_AUDIO_CPU] = current_cpu_id();
        if (seq != s) {                                   /* se liberó otra mitad mientras calculaba */
            late++;
            if (seq - s > 1u)                             /* more than one behind: only the next one counts */
                os_sem_set(&sem, 1);
            if (++seguidos >= LATE_SEGUIDOS) {
                seguidos = 0;
                os_sem_set(&sem, 0);
                memset(buf + (uint32_t)libre * half_words, 0, half_words * 4u);   /* the free half only */
                os_time_dly(1);
            }
        } else {
            seguidos = 0;
        }
    }
}

void fm1_audio_start(int32_t *dma, uint32_t frames_per_half, fm1_audio_render cb)
{
    fm1_stat[FM1_STAT_BUDGET] = frames_per_half * 1000000u / BM_FS_OUT_HZ;
    buf = dma;
    half_words = frames_per_half * 2u;
    render = cb;
    memset(buf, 0, half_words * 2u * 4u);

    codec_linea(IO_PORTC_06, 0);                          /* codec deshabilitado mientras se configura */
    os_time_dly(1);

    JL_ALNK0->CON0 = 0;
    JL_ALNK0->CON1 = 0;
    JL_ALNK0->CON2 = 0;
    JL_ALNK0->CON3 = 0;
    JL_IOMAP->CON5 &= ~0xC0u;                             /* grupo de pines 0 */
    JL_ALNK0->CON3 |= 0x03u;
    codec_linea(IO_PORTC_00, 1);
    JL_ALNK0->CON0 |= 0x100u;
    codec_linea(IO_PORTC_02, 1);
    codec_linea(IO_PORTC_01, 1);
    JL_ALNK0->CON0 |= 0x80u;
    JL_ALNK0->CON0 &= ~0x40u;
    JL_ALNK0->LEN = (uint16_t)half_words;
    JL_ALNK0->CON0 &= ~0x600u;
    JL_CLOCK->CLK_CON2 &= ~0xF00u;                        /* reloj de audio: fuente 0 */
    JL_ALNK0->CON3 = (uint8_t)((JL_ALNK0->CON3 & 0x03u) | 0x80u);
    JL_ALNK0->CON1 |= (1u << 12) | (1u << 14);            /* canal 3: salida de 24 bits */
    JL_ALNK0->ADR3 = (uint32_t)(uintptr_t)buf;
    codec_linea(IO_PORTC_06, 1);
    JL_ALNK0->CON1 &= ~(1u << 15);
    JL_ALNK0->CON2 = 0x0Fu;                               /* avisos viejos fuera */
    if (!tarea) {
        os_sem_create(&sem, 0);
        os_task_create(audio_task, NULL, AUDIO_PRIO, 4096, 0, "audio");
        tarea = 1;
    }
    request_irq(IRQ_ALNK_IDX, 1, alnk0_isr, 0);
    JL_ALNK0->CON0 |= 0x800u;
    en_marcha = 1;
}

/* el núcleo corre sin tocar ALNK0 ni el codec (modo de prueba desde el modo seguro) */
void fm1_audio_start_mute(int32_t *dma, uint32_t frames_per_half, fm1_audio_render cb)
{
    buf = dma;
    half_words = frames_per_half * 2u;
    render = cb;
    mudo = 1;
    en_marcha = 1;
    if (!tarea) {
        os_sem_create(&sem, 0);
        os_task_create(audio_task, NULL, AUDIO_PRIO, 4096, 0, "audio");
        tarea = 1;
    }
}

void fm1_audio_stop(void)
{
    if (!en_marcha)
        return;
    if (!mudo)
        JL_ALNK0->CON0 &= ~0x800u;                        /* sin DMA: nada de zumbido repetido */
    en_marcha = 0;
}

/* pausa para escribir la flash: sin DMA no hay interrupción; se espera a que la tarea termine el
 * bloque que esté calculando (código en la flash). Al volver, silencio y DMA de nuevo. */
void fm1_audio_pause(int on)
{
    if (!en_marcha || mudo)
        return;
    if (on) {
        JL_ALNK0->CON0 &= ~0x800u;
        for (int i = 0; i < 20 && calculando; i++)
            os_time_dly(1);
    } else {
        memset(buf, 0, half_words * 2u * 4u);
        JL_ALNK0->CON2 = 0x0Fu;
        JL_ALNK0->CON0 |= 0x800u;
    }
}

uint32_t fm1_audio_halves(void) { return halves; }
uint32_t fm1_audio_late(void) { return late; }
