/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — BB-303: the TB-303 voice in float (0.9), after Open303 by Robin Schmidt (MIT,
 * third_party/open303/LICENSE): its TB-303 filter, the main envelope with the measured cutoff and
 * ENV MOD mapping, the accent circuit (the 15 ms RC that makes accents "wow"), the amp envelope
 * with its declicker and the fixed high-passes and notch around the filter. It runs at the output
 * rate (no oversampling): the cutoff stops at 10 kHz, where the filter still tracks and stays
 * stable at full resonance (Open303 runs it at 4x). One voice per channel, in the shared pool. */
#ifndef BM_ACID_H
#define BM_ACID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the knobs, 0..127 (as on the panel: CUTOFF, RESO, ENV MOD, DECAY, ACCENT, SLIDE, WAVE) */
typedef struct { uint8_t cut, res, env, dec, acc, slide, wave; } bm_acid_par;

void  bm_acid_init(float fs);         /* the fixed coefficients (when a channel becomes a BB-303) */
/* note: 0..127 (already transposed). accent: the step has an accent. legato: 1 = slide from the note
 * that sounds (no new envelopes), 0 = a new note on the voice that sounds, 2 = a voice that was free
 * (its state starts from zero). lvl: level for quiet notes (1 = full). */
void  bm_acid_start(int v, int note, int accent, int legato, float lvl, const bm_acid_par *p);
void  bm_acid_release(int v);
/* n samples (<= 64) of voice v into out (engine scale: 1.0 = one voice at full). Returns 1 while it
 * sounds, 0 when it ended, -1 if a state had NaN/inf (back to zero, out is silence). */
int   bm_acid_render(int v, const bm_acid_par *p, float *out, int n);
float bm_acid_level(int v);           /* the amp envelope, 0..1 (to pick whom to steal) */

/* what the panel shows (the TB-303 ranges) */
float bm_acid_cut_hz(int v);          /* CUTOFF: 314 Hz .. 2.4 kHz, as Open303 measured on a 303 */
float bm_acid_decay_ms(int v);        /* DECAY: 200 ms .. 2 s */
float bm_acid_slide_ms(int v);        /* SLIDE: time to reach the next note */

#ifdef __cplusplus
}
#endif

#endif
