/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — one memory per pool voice, shared by the machines.
 *
 * A pool voice (BM_V_POOL0..BM_VOICES-1, T2-T6) belongs to one machine at a time, so the state each
 * machine keeps per voice (BX-7: msfa's Dx7Note, the biggest; B-MINI, BITTAL and BUNO-6: the WAVE
 * voice; BB-303: its filter and envelopes) lives in the same bytes instead of one array per machine.
 * owner[] says whose state is in there: a machine that starts a voice it does not own builds its
 * state from scratch (placement new) before using it. The T1 drum voices keep their own memory
 * (core/bm_drums.cpp); the sampler (T2) keeps everything in bm_voice. 0.9: about 14 KiB less RAM. */
#ifndef BM_VPOOL_H
#define BM_VPOOL_H

#include <stdint.h>
#include "bm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* bytes per voice: msfa's voice (980 B with its 64-sample buffer) rounded to 8. Every machine checks
 * its own state against it at compile time (static_assert). */
#define BM_VPOOL_BYTES 984u

enum { VP_NONE, VP_WAVE, VP_FM, VP_ACID };

extern unsigned char bm_vpool_mem[BM_V_POOL_N][BM_VPOOL_BYTES];
extern uint8_t bm_vpool_owner[BM_V_POOL_N];

static inline int bm_vpool_ok(int v) { return v >= BM_V_POOL0 && v < BM_VOICES; }
static inline void *bm_vpool(int v) { return bm_vpool_mem[v - BM_V_POOL0]; }
/* 1 if the voice already held this machine's state; else marks it as the owner (the caller builds it) */
static inline int bm_vpool_take(int v, uint8_t who)
{
    uint8_t *o = &bm_vpool_owner[v - BM_V_POOL0];
    if (*o == who) return 1;
    *o = who;
    return 0;
}
static inline int bm_vpool_is(int v, uint8_t who) { return bm_vpool_ok(v) && bm_vpool_owner[v - BM_V_POOL0] == who; }

#ifdef __cplusplus
}
#endif

#endif
