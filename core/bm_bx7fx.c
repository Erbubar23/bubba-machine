/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — los efectos propios de la BX-7 y el OVERDRIVE del BB-303. La BX-7, como la cadena del FM-1 de fábrica: filtro,
 * distorsión, chorus y phaser, en el orden de sus casillas (BX_ORD0..5). La reverb y el delay de la
 * cadena son los envíos del canal en la mezcla (core/bm_dsp.c): no se procesan dos veces.
 *
 * Solo calcula lo que está prendido (BX_FXON): con todo apagado no cuesta nada. Hay una sola BX-7
 * por patrón (es pesada), así que el estado es uno solo. Trabaja sobre el búfer del canal (int32,
 * 32768 ~ una voz a todo volumen) antes del filtro DJ y del drive del canal. */
#include <math.h>
#include <string.h>
#include "bm_audio.h"
#include "bm_params.h"
#include "bm_util.h"

#define CH_N 2048u                                   /* 46 ms a 44.1 kHz: el chorus barre 6-26 ms */

static struct {
    float lp, bp;                                    /* filtro de estado variable (Chamberlin, dos pasadas) */
    float tone;                                      /* el tono de la distorsión (un polo) */
    float cbuf[CH_N]; uint32_t cw; float clfo;       /* chorus */
    float ap[4], pfb, plfo;                          /* phaser: cuatro pasa-todo */
} S;

static float od_tone[BM_TRACKS];                     /* el BB-303 puede estar en varios canales */

void bm_bx7_fx_reset(void) { memset(&S, 0, sizeof S); memset(od_tone, 0, sizeof od_tone); }

/* 0..127 -> 100 Hz..20 kHz (logarítmico, como el CutOff del FM-1) */
static float cut_hz(int v) { return 100.0f * powf(200.0f, (float)v / 127.0f); }

static void fx_filter(float *x, uint32_t n, const uint8_t *p)
{
    float f = 2.0f * sinf(3.14159265f * fminf(cut_hz(p[BX_FLT_C]), 18000.0f) / (2.0f * (float)BM_FS));   /* dos pasadas por muestra */
    float q = 1.9f - 1.75f * (float)p[BX_FLT_Q] / 127.0f;                  /* Q 0..127: de suave a resonante */
    int type = p[BX_FLT_T] % 3;
    float lp = S.lp, bp = S.bp;
    for (uint32_t i = 0; i < n; i++) {
        float hp = 0.0f;
        for (int k = 0; k < 2; k++) {
            lp += f * bp;
            hp = x[i] - lp - q * bp;
            bp += f * hp;
        }
        x[i] = type == 0 ? lp : type == 1 ? bp : hp;
    }
    if (!(fabsf(lp) < 1e6f) || !(fabsf(bp) < 1e6f)) lp = bp = 0.0f;       /* NaN o desborde: de cero */
    S.lp = lp; S.bp = bp;
}

/* recorte suave con tono de un polo: la distorsión de la BX-7 y el OVERDRIVE del BB-303 */
static void fx_dist(float *x, uint32_t n, int gain, int tone, int level, float *st)
{
    float drive = 1.0f + (float)gain * 0.25f, lvl = (float)level / 127.0f / sqrtf(drive) * 1.4f;
    float k = 0.05f + 0.9f * (float)tone / 127.0f, t = *st;
    for (uint32_t i = 0; i < n; i++) {
        float v = x[i] * (drive / 32768.0f);                               /* el canal: 32768 = una voz a pleno */
        v = v / (1.0f + fabsf(v));                                         /* recorte suave, sin exp */
        t += k * (v - t);
        x[i] = t * lvl * 32768.0f;
    }
    *st = (fabsf(t) < 1e6f) ? t : 0.0f;
}

static float ch_read(float d)
{
    float pos = (float)S.cw - d;
    while (pos < 0.0f) pos += (float)CH_N;
    uint32_t i0 = (uint32_t)pos, i1 = (i0 + 1u) & (CH_N - 1u);
    float fr = pos - (float)i0;
    return S.cbuf[i0 & (CH_N - 1u)] * (1.0f - fr) + S.cbuf[i1] * fr;
}

static void fx_chorus(float *x, uint32_t n, const uint8_t *p)
{
    float rate = (0.05f + 4.0f * (float)p[BX_CHO_F] / 127.0f) / (float)BM_FS;
    float mid = 0.016f * (float)BM_FS, dep = 0.010f * (float)BM_FS * (float)p[BX_CHO_D] / 127.0f;
    float mix = (float)p[BX_CHO_M] / 127.0f;
    for (uint32_t i = 0; i < n; i++) {
        S.cbuf[S.cw] = x[i];
        S.clfo += rate; if (S.clfo >= 1.0f) S.clfo -= 1.0f;
        float tri = 4.0f * fabsf(S.clfo - 0.5f) - 1.0f;
        float w = ch_read(mid + dep * tri);
        S.cw = (S.cw + 1u) & (CH_N - 1u);
        x[i] = x[i] * (1.0f - 0.5f * mix) + w * mix;
    }
}

static void fx_phaser(float *x, uint32_t n, const uint8_t *p)
{
    float rate = (0.03f + 3.0f * (float)p[BX_PHA_F] / 127.0f) / (float)BM_FS;
    float dep = (float)p[BX_PHA_D] / 127.0f, mix = (float)p[BX_PHA_M] / 127.0f;
    float fb = S.pfb;
    for (uint32_t i = 0; i < n; i++) {
        S.plfo += rate; if (S.plfo >= 1.0f) S.plfo -= 1.0f;
        float tri = 4.0f * fabsf(S.plfo - 0.5f) - 1.0f;
        float a = 0.55f + 0.4f * dep * tri;                                /* coeficiente de los pasa-todo */
        float v = x[i] + fb * 0.5f;
        for (int k = 0; k < 4; k++) {
            float y = -a * v + S.ap[k];
            S.ap[k] = v + a * y;
            v = y;
        }
        fb = v;
        x[i] = x[i] * (1.0f - 0.5f * mix) + v * mix;
    }
    if (!(fabsf(fb) < 1e6f)) { fb = 0.0f; memset(S.ap, 0, sizeof S.ap); }
    S.pfb = fb;
}

void bm_bx7_fx(bm_ctx *c, int t, int32_t *buf, uint32_t n)
{
    const uint8_t *p = c->pat->prm[t];
    uint8_t on = p[BX_FXON];
    if (!(on & (1u << BXF_FILTER | 1u << BXF_DIST | 1u << BXF_CHORUS | 1u << BXF_PHASER))) return;
    float x[BM_BLOCK];
    if (n > BM_BLOCK) n = BM_BLOCK;
    for (uint32_t i = 0; i < n; i++) x[i] = (float)buf[i];
    for (int k = 0; k < BXF_N; k++) {
        int f = p[BX_ORD0 + k] % BXF_N;
        if (!(on & (1u << f))) continue;
        switch (f) {
        case BXF_FILTER: fx_filter(x, n, p); break;
        case BXF_DIST: fx_dist(x, n, p[BX_DST_G], p[BX_DST_T], p[BX_DST_L], &S.tone); break;
        case BXF_CHORUS: fx_chorus(x, n, p); break;
        case BXF_PHASER: fx_phaser(x, n, p); break;
        default: break;                                                    /* reverb y delay: envíos de la mezcla */
        }
    }
    for (uint32_t i = 0; i < n; i++) {
        float v = x[i];
        if (v != v) v = 0.0f;                                              /* NaN: silencio, nunca al bus */
        buf[i] = (int32_t)(v > 2.0e9f ? 2.0e9f : v < -2.0e9f ? -2.0e9f : v);
    }
}

/* B-MINI, BUNO-6, BITTAL: la DISTORTION del POLY D (DRIVE, TONE, LEVEL), el recorte del OVERDRIVE del BB-303 */
void bm_px_fx(bm_ctx *c, int t, int32_t *buf, uint32_t n)
{
    const uint8_t *p = c->pat->prm[t];
    if (!bm_px_fx_on(p, c->pat->trk[t].engine, PXF_DIST)) return;
    float x[BM_BLOCK];
    if (n > BM_BLOCK) n = BM_BLOCK;
    for (uint32_t i = 0; i < n; i++) x[i] = (float)buf[i];
    fx_dist(x, n, p[PX_DS_G], p[PX_DS_T], p[PX_DS_L], &od_tone[t]);
    for (uint32_t i = 0; i < n; i++) buf[i] = (int32_t)(x[i] > 2.0e9f ? 2.0e9f : x[i] < -2.0e9f ? -2.0e9f : x[i]);
}

/* BB-303: el OVERDRIVE del TB-03, antes del filtro DJ y del drive del canal (DELAY y REVERB son los envíos) */
void bm_acid_fx(bm_ctx *c, int t, int32_t *buf, uint32_t n)
{
    const uint8_t *p = c->pat->prm[t];
    if (!bm_acid_fx_on(p, ACF_OD)) return;
    float x[BM_BLOCK];
    if (n > BM_BLOCK) n = BM_BLOCK;
    for (uint32_t i = 0; i < n; i++) x[i] = (float)buf[i];
    fx_dist(x, n, p[AC_OD_G], p[AC_OD_T], p[AC_OD_L], &od_tone[t]);
    for (uint32_t i = 0; i < n; i++) buf[i] = (int32_t)(x[i] > 2.0e9f ? 2.0e9f : x[i] < -2.0e9f ? -2.0e9f : x[i]);
}
