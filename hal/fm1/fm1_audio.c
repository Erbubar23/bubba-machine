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
 * audio_decoder uses), que calcula el bloque del núcleo en un scratch y solo lo copia al DMA si
 * todavía va a tiempo. Late: the playing half is never written; the current free half is zeroed
 * (clean silence instead of a torn buffer). No 10 ms sleep on late (that was an audible gap).
 * alnk0_isr and the publish/late helper live in .volatile_ram_code so they do not depend on XIP
 * after the UI has thrashed the instruction cache. During flash erase/program XIP is off: the ISR
 * keeps DMA alive with silence and does not wake the flash-resident audio task (fm1_audio_pause). */
#include <string.h>
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "device/gpio.h"
#include "os/os_api.h"
#include "fm1_hw.h"

#define BM_FS_OUT_HZ 44118u          /* core/bm_config.h BM_FS_OUT */
#define HALF_FRAMES_MAX 256u         /* fm1_main.c HALF_FRAMES */
#define AUDIO_PRIO    30             /* configMAX_PRIORITIES - 1 */

static int32_t *buf;
static uint32_t half_words;
static fm1_audio_render render;
static volatile uint32_t halves, late, seq;
static volatile uint8_t calculando;
static volatile uint8_t libre;
static volatile uint8_t quiet;       /* flash write: ISR fills silence, no task wake */
static int en_marcha, tarea, mudo;
static OS_SEM sem;
/* one half of stereo int32: render here, memcpy to DMA only if still on time */
static int32_t scratch[HALF_FRAMES_MAX * 2u];

static void codec_linea(uint32_t io, int v)
{
    gpio_set_die(io, 1);
    gpio_direction_output(io, v);
}

/* No libc: must run with XIP off (flash program). */
SEC_USED(.volatile_ram_code) static void zero_half(uint8_t half)
{
    uint32_t n = half_words, i;
    int32_t *p = buf + (uint32_t)half * n;
    for (i = 0; i < n; i++)
        p[i] = 0;
}

SEC_USED(.volatile_ram_code) static ___interrupt void alnk0_isr(void)
{
    uint8_t p = JL_ALNK0->CON2;
    if (p & 0x70u)                                        /* avisos de los otros canales */
        JL_ALNK0->CON2 |= (uint8_t)((p >> 4) & 0x07u);
    if (!(p & 0x80u))
        return;
    libre = (uint8_t)(((JL_ALNK0->CON0 >> 15) & 1u) ^ 1u);   /* la mitad que no suena */
    JL_ALNK0->CON2 |= 0x08u;
    seq++;
    if (quiet) {
        zero_half(libre);
        halves++;
        return;                                           /* do not wake flash-resident audio_task */
    }
    os_sem_post(&sem);
}

/* Snapshot the free half and plant silence there before the (flash-resident) render runs. */
SEC_USED(.volatile_ram_code) static __attribute__((noinline)) void audio_begin(uint8_t *half_out, uint32_t *s_out)
{
    uint8_t half = libre;
    *half_out = half;
    *s_out = seq;
    memset(buf + (uint32_t)half * half_words, 0, half_words * 4u);
}

/* After render: copy scratch into the reserved half if still free, else leave that half as the
 * silence we planted at the start (it may already be playing) and silence the newest free half. */
SEC_USED(.volatile_ram_code) static __attribute__((noinline)) void audio_commit(uint8_t half, uint32_t s0)
{
    uint32_t n = half_words;
    if (seq == s0) {
        memcpy(buf + (uint32_t)half * n, scratch, n * 4u);
        return;
    }
    late++;
    if (seq - s0 > 1u)                                    /* more than one behind: only the next one counts */
        os_sem_set(&sem, 1);
    /* half was pre-zeroed: if DMA already took it, the underrun is clean silence. Do not write it
     * again. Zero the half that is free now so the next period is silent too, not stale. */
    memset(buf + (uint32_t)libre * n, 0, n * 4u);
}

static void audio_task(void *p)
{
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
        uint8_t half;
        uint32_t s, t0 = fm1_scan_now_us();
        uint32_t frames = half_words / 2u;
        audio_begin(&half, &s);
        calculando = 1;
        if (render && en_marcha)
            render(scratch, frames);
        else
            memset(scratch, 0, half_words * 4u);
        calculando = 0;
        audio_commit(half, s);
        halves++;
        uint32_t dt = fm1_scan_now_us() - t0;
        if (dt > 1000000u)
            dt = 0;                                       /* el reloj no anda (barrido apagado) */
        fm1_stat[FM1_STAT_RENDER_LAST] = dt;
        if (dt > fm1_stat[FM1_STAT_RENDER_MAX])
            fm1_stat[FM1_STAT_RENDER_MAX] = dt;
        fm1_stat[FM1_STAT_RENDER_AVG] = (fm1_stat[FM1_STAT_RENDER_AVG] * 15u + dt) / 16u;
        fm1_stat[FM1_STAT_AUDIO_CPU] = current_cpu_id();
    }
}

void fm1_audio_start(int32_t *dma, uint32_t frames_per_half, fm1_audio_render cb)
{
    if (frames_per_half > HALF_FRAMES_MAX)
        frames_per_half = HALF_FRAMES_MAX;
    fm1_stat[FM1_STAT_BUDGET] = frames_per_half * 1000000u / BM_FS_OUT_HZ;
    buf = dma;
    half_words = frames_per_half * 2u;
    render = cb;
    memset(buf, 0, half_words * 2u * 4u);
    memset(scratch, 0, sizeof scratch);

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
    if (frames_per_half > HALF_FRAMES_MAX)
        frames_per_half = HALF_FRAMES_MAX;
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

/* Flash write keep-alive: DMA stays running; ISR plants silence without waking audio_task
 * (render lives in flash / XIP, which is offline while NOR is programmed). */
void fm1_audio_pause(int on)
{
    if (!en_marcha || mudo)
        return;
    if (on) {
        quiet = 1;
        for (int i = 0; i < 20 && calculando; i++)
            os_time_dly(1);
        if (buf && half_words)
            memset(buf, 0, half_words * 2u * 4u);
    } else {
        if (buf && half_words)
            memset(buf, 0, half_words * 2u * 4u);
        JL_ALNK0->CON2 = 0x0Fu;
        quiet = 0;
    }
}

uint32_t fm1_audio_halves(void) { return halves; }
uint32_t fm1_audio_late(void) { return late; }
