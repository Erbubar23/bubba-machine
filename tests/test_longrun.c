/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — long run of the audio core on the PC (offline, as fast as it goes).
 *
 *   test_longrun [minutes] [scenario] [out.wav]
 *     scenario: demo (the first-boot pattern), live (a player through the UI: channels, keys
 *     held, REC on and off), dense (all six machines, many notes, accents, long notes, ROLL, pads
 *     held, FX sends, knobs moving, a load drop at minutes 11-12), wrap (dense with the 32-bit
 *     counters started near their end), tails (every sound: time until its voices free), chorus
 *     (BUNO-6 chorus noise over time), dual (TWO CORES against one), bench (each machine alone,
 *     every sound: its cost), all (default: everything)
 *   py -3 tools/build.py longrun [minutes] [scenario] [asan]
 *
 * Every 64-frame block it checks the output, each channel's buffer, the float state of the mix
 * and of each WAVE voice (NaN / inf / blow-ups), the voices (stuck notes, voices that never free,
 * pending voices that never start) and it runs the device's voice brake (hal/fm1/fm1_main.c) on
 * a cost model, to see that it engages and lets go. Per minute it prints peak, RMS, clipped
 * blocks, voices per channel, brake activity, the PC time per 256 frames, the core's own cost
 * meter per channel, the UI time and the pattern's events. Exit code 1 if something went wrong. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "../hal/bm_hal_sim.h"
#include "../core/bm_app_int.h"
#include "../core/bm_util.h"
#include "../core/bm_wave.h"
#include "../core/bm_params.h"
#include "../core/bm_drums.h"

static bm_sim sim;
static bm_app app;
static uint32_t now_ms, next_tick;
static int problems;

#define PROBLEM(...) do { if (problems++ < 40) { printf("  PROBLEM: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t now_ticks(void);

static void boot_with_take(void)
{
    static int8_t d[23851];
    memset(d, 0, sizeof d);
    for (int b = 0; b < 8; b++) {
        int at = b * 2981;
        for (int i = 0; i < 1500; i++) {
            float e = 1.0f - i / 1500.0f, v;
            if (b % 2 == 0 && (b / 2) % 2 == 0) v = 100.0f * e * e * (float)sin(6.2831853 * 60.0 * i / 16000.0);
            else if (b % 2 == 0) v = 70.0f * e * e * e * (float)(((i * 7919) % 200) - 100) / 100.0f;
            else v = 30.0f * e * e * e * e * (float)(((i * 104729) % 200) - 100) / 100.0f;
            d[at + i] = (int8_t)v;
        }
    }
    bm_sim_flash_erase_all(&sim);
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    bm_take tk[BM_TAKES];
    memset(tk, 0, sizeof tk);
    bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, sizeof d, 1610, 1);
    bm_sim_platform(&sim, &pf);
    now_ms = 1000; next_tick = now_ms;
    bm_app_init(&app, &pf, now_ms);
    for (int i = 0; i < 4; i++) bm_app_master(&app, 1023);
    app.ctx.now = now_ticks; app.ctx.par_auto = 1;          /* per-channel cost, as on the device (one core) */
}

static void cmd(uint8_t type, uint8_t a, uint8_t b, uint8_t c, int32_t i)
{
    bm_cmd k;
    memset(&k, 0, sizeof k);
    k.type = type; k.a = a; k.b = b; k.c = c; k.u.i = i;
    bm_cmd_push(&app.cmdq, &k);
}

static uint32_t rng = 0x1234567u;
static uint32_t rnd(uint32_t n) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return n ? rng % n : 0; }

static void ev(int t, int s, int k, int note, int vel, int len)
{
    bm_event *e = bm_ev(&app.pat, t, s, k);
    bm_event_clear(e);
    e->note = (uint8_t)note; e->vel = (uint8_t)vel; e->len = (uint8_t)len; e->prob = 100;
}

/* all six machines busy: drums on every lane with accents, ratchets and flams, pads in every
 * mode, an acid line with slides and accents at full resonance, chords on BX-7, BITTAL and
 * BUNO-6 (four notes per step, long notes that overlap), sends and reverb up, feedback high */
static void dense_pattern(void)
{
    bm_pattern *p = &app.pat;
    p->meta.bpm_x10 = 1450; p->meta.dly_fb = 120; p->meta.dly_mix = 100; p->meta.dly_time = 3;
    p->meta.rv_size = 127; p->meta.rv_mix = 110; p->meta.rv_damp = 20;
    bm_tr *d = &p->tr[0];
    for (int l = 0; l < BM_LANES; l++)
        for (int s = 0; s < 16; s++)
            if ((s * 7 + l * 3) % 5 < 2) d->st[l][s] = (uint8_t)(ST_ON | ((s % 4 == 0) ? ST_ACC : 0) | ((s % 7 == 3) ? (3 << ST_RAT_SH) : 0) | ((s % 9 == 4) ? ST_FLAM : 0));
    bm_tr *pd = &p->tr[1];
    for (int l = 0; l < BM_LANES; l++) {
        if (bm_pad_ok(&p->pad[l])) p->pad[l].mode = (uint8_t)(l % PAD_MODES);
        if (bm_pad_ok(&p->pad[l])) { p->pad[l].loop_s = 20; p->pad[l].loop_e = 200; p->pad[l].flags = (uint8_t)(l % 3 == 1 ? PAD_REV : 0); }
        for (int s = 0; s < 16; s++) if ((s + l) % 3 == 0) pd->st[l][s] = (uint8_t)(ST_ON | (s % 4 == 0 ? ST_ACC : 0) | (s % 5 == 1 ? (1 << ST_RAT_SH) : 0));
    }
    p->trk[1].gen[0] = 60; p->trk[1].gen[2] = 50; p->trk[1].gen[3] = 40;
    p->trk[1].rt[0] = p->trk[1].rt[1] = p->trk[1].rt[2] = p->trk[1].rt[3] = 30;
    for (int t = 2; t < BM_TRACKS; t++) {
        bm_track_clear_events(p, t);
        p->trk[t].length = 64;
        p->trk[t].send = 100; p->trk[t].rsend = 110; p->trk[t].level = 110;
        p->trk[t].accent = 127;
    }
    p->trk[0].level = 120; p->trk[1].level = 120; p->trk[0].send = 60; p->trk[1].send = 60; p->trk[0].rsend = 60;
    /* T3: acid line, accents and slides, resonance up */
    p->trk[2].macro[0] = 70; p->trk[2].macro[1] = 127; p->trk[2].macro[3] = 127;
    for (int s = 0; s < 64; s++) if (s % 4 != 3) ev(2, s, 0, 33 + (s * 5) % 24, s % 3 == 0 ? BM_VEL_ACC : BM_VEL_NORMAL, 1 | (s % 5 == 2 ? BM_LEN_TIE : 0));
    /* T4-T6: chords of four, some very long (they overlap the next ones) */
    for (int t = 3; t < BM_TRACKS; t++)
        for (int s = 0; s < 64; s += 2)
            for (int k = 0; k < 4; k++) {
                int len = (s % 16 == 0) ? 40 + 20 * k : ((s % 8 == 4) ? 9 : 1);
                ev(t, s, k, 48 + (t - 3) * 5 + k * 4 + (s / 8) % 5, (s + k) % 3 == 0 ? BM_VEL_ACC : BM_VEL_NORMAL, len);
            }
    /* resonance and filters near the edge */
    p->trk[3].macro[0] = 127; p->trk[3].macro[1] = 127;               /* BX-7: brightness and feedback up */
    p->trk[4].macro[0] = 127; p->trk[4].macro[1] = 127;
    p->prm[4][BT_RES] = 127; p->prm[4][BT_DRIVE] = 127; p->prm[4][BT_U1] = 3; p->prm[4][BT_U2] = 3; p->prm[4][BT_L2] = 127;
    p->trk[5].macro[0] = 127; p->trk[5].macro[1] = 127; p->trk[5].macro[2] = 127;
    p->prm[5][BN_CHORUS] = BN_CH_BOTH; p->prm[5][BN_SUB] = 127; p->prm[5][BN_NOISE] = 60; p->prm[5][BN_R] = 110;
}

/* ------------------------------------------------------------ the brake, as on the device */
/* cost of one 256-frame render in % of its time, from the voices that sound (a model fitted to
 * what 0.6.1 measured: mix and FX ~20 %, BR-8 ~19 %, BUNO-6 ~32 %, the demo 88-115 %) */
static const float VCOST[BM_TRACKS] = {4.5f, 1.5f, 5.0f, 5.0f, 7.0f, 9.0f};
static float load_scale = 1.0f;
static uint32_t brake_n, brake_total, br_last, br_avg, br_renders, br_since, br_relief, relief_n, cap_min = 99;

#define POOL_CAP() (app.ctx.pool_cap ? app.ctx.pool_cap : BM_V_POOL_N)

static uint32_t model_load(void)
{
    float l = 20.0f;
    for (int i = 0; i < BM_VOICES; i++) {
        const bm_voice *v = &app.ctx.voice[i];
        if (v->active) l += VCOST[v->track % BM_TRACKS];
    }
    return (uint32_t)(l * load_scale);
}

static void brake_step(void)          /* once per 256 frames, before the render (fm1_main.c render()) */
{
    br_renders++;
    if (br_renders - br_since >= 4u && (br_last > 95u || br_avg > 85u)) {
        int f = bm_audio_overload(&app.ctx) != 0;
        br_since = br_renders;
        brake_n += (uint32_t)f; brake_total += (uint32_t)f;
    }
    else if (app.ctx.pool_cap && br_renders - br_since >= 64u && br_renders - br_relief >= 64u && br_avg < 70u) {
        bm_audio_relief(&app.ctx);
        br_relief = br_renders;
        relief_n++;
    }
    uint32_t cap = POOL_CAP();
    if (cap < cap_min) cap_min = cap;
}

static void brake_after(void)
{
    uint32_t dt = model_load();
    br_last = dt;
    br_avg = (br_avg * 15u + dt) / 16u;
}

/* ------------------------------------------------------------ checks */
typedef struct {
    double pk, sq; uint64_t n;
    uint32_t clipped_blocks, out_of_range;
    double tpk[BM_TRACKS];
    uint32_t tbig[BM_TRACKS];
    uint32_t vmax[BM_TRACKS];
    double vsum[BM_TRACKS];
    uint32_t vblocks;
    uint32_t nan_state, nan_voice;
    uint32_t sat_dly;
} stats;

static stats S;
static double cpu_ns, cpu_max_ns; static uint32_t cpu_n;
static double now_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1e9 + ts.tv_nsec; }
static double cpu_r_ns;
static uint32_t now_ticks(void) { return (uint32_t)(uint64_t)(now_ns() / 16.0); }   /* the core's cost meter (par_auto) */
static double cost_sum[BM_TRACKS + 1], ui_ns;
static uint32_t cost_n;
static void cost_sample(void)
{
    for (int t = 0; t < BM_TRACKS; t++) cost_sum[t] += app.ctx.cost[t];
    cost_sum[BM_TRACKS] += app.ctx.cost_over;
    cost_n++;
}
static uint32_t v_age[BM_VOICES], v_since[BM_VOICES], v_pend[BM_VOICES], v_fade[BM_VOICES];
static uint64_t frames_done;
static uint32_t worst_hold_s;

static int finite(float x) { return isfinite(x); }

static void check_block(const int32_t *out, uint32_t n)
{
    bm_ctx *c = &app.ctx;
    for (uint32_t i = 0; i < n * 2; i++) {
        int32_t v = out[i];
        if (v > 8388607 || v < -8388608) S.out_of_range++;
        double x = v / 8388608.0;
        S.sq += x * x;
        if (fabs(x) > S.pk) S.pk = fabs(x);
    }
    S.n += n * 2;
    if (c->st.clip) { S.clipped_blocks++; c->st.clip = 0; }
    for (int t = 0; t < BM_TRACKS; t++) {
        if (!(c->tact & (1u << t))) continue;
        for (uint32_t i = 0; i < n; i++) {
            int32_t x = c->tbuf[t][i];
            double a = fabs((double)x) / 32768.0;
            if (a > S.tpk[t]) S.tpk[t] = a;
            if (a > 64.0) S.tbig[t]++;           /* 64x full scale: a filter or a NaN cast to int */
        }
    }
    /* float state of the mix */
    float f[] = {c->sc_env, c->d_acc, c->d_prev, c->d_cur, c->pf_ic1[0], c->pf_ic1[1], c->pf_ic2[0], c->pf_ic2[1],
                 c->mf_ic1[0], c->mf_ic1[1], c->mf_ic2[0], c->mf_ic2[1], c->sh_l, c->sh_r, c->gate_g, c->fade_gain,
                 c->smp_z[0], c->smp_z[1], c->smp_z[2], c->smp_z[3]};
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) if (!finite(f[i])) S.nan_state++;
    for (int t = 0; t < BM_TRACKS; t++) if (!finite(c->dj_ic1[t]) || !finite(c->dj_ic2[t])) S.nan_state++;
    /* voices */
    uint32_t cnt[BM_TRACKS] = {0};
    uint32_t now_s = (uint32_t)(frames_done / 44118u);
    for (int i = 0; i < BM_VOICES; i++) {
        const bm_voice *v = &c->voice[i];
        int wave = v->active && (c->pat->trk[v->track % BM_TRACKS].engine == ENG_MINI || c->pat->trk[v->track % BM_TRACKS].engine == ENG_BITTAL ||
                                 c->pat->trk[v->track % BM_TRACKS].engine == ENG_BUNO);
        if (wave && !finite(bm_wv_level(i))) S.nan_voice++;
        if (v->active) {
            cnt[v->track % BM_TRACKS]++;
            if (v->age != v_age[i]) { v_age[i] = v->age; v_since[i] = now_s; }
            uint32_t held = now_s - v_since[i];
            if (held > worst_hold_s) worst_hold_s = held;
            if (held == 180u && (frames_done % 44118u) < n) PROBLEM("voice %d (T%d note %d) has sounded for 180 s (gate %d, stage %d)", i, v->track + 1, v->note, v->gate, v->stage);
        } else v_age[i] = 0xFFFFFFFFu;
        if (v->pend && !v->active) { if (++v_pend[i] == 4) PROBLEM("voice %d pending without sounding (T%d -> T%d)", i, v->track + 1, v->pend_track + 1); }
        else v_pend[i] = 0;
        if (v->fade && v->active) { if (++v_fade[i] == 2000) PROBLEM("voice %d stuck in its 2 ms fade", i); }
        else v_fade[i] = 0;
    }
    for (int t = 0; t < BM_TRACKS; t++) { if (cnt[t] > S.vmax[t]) S.vmax[t] = cnt[t]; S.vsum[t] += cnt[t]; }
    S.vblocks++;
}

static void check_buffers(void)       /* once a second: delay line saturated, reverb with garbage */
{
    const int16_t *dl = app.amem.fx.dly;
    if (app.ctx.arena_busy || app.ctx.smp_rec) return;
    uint32_t sat = 0;
    for (uint32_t i = 0; i < BM_DELAY_LEN; i++) if (dl[i] >= 32767 || dl[i] <= -32768) sat++;
    S.sat_dly += sat;
}

/* ------------------------------------------------------------ the run */
static FILE *wav;
static uint32_t wav_frames;

static void wav_put(const int32_t *out, uint32_t n)
{
    if (!wav) return;
    for (uint32_t i = 0; i < n * 2; i++) { int16_t s = (int16_t)(out[i] >> 8); fwrite(&s, 2, 1, wav); }
    wav_frames += n;
}

static void print_header(void)
{
    printf("  min   peak   rms   clipblk  T1pk  T2pk  T3pk  T4pk  T5pk  T6pk   voices avg/max T1..T6            brake relief cap  hold  pc-us/256 avg max\n");
}

static void print_minute(int m)
{
    double rms = S.n ? sqrt(S.sq / (double)S.n) : 0;
    printf("  %3d  %5.3f  %5.3f  %6u  ", m, S.pk, rms, S.clipped_blocks);
    for (int t = 0; t < BM_TRACKS; t++) printf(" %5.2f", S.tpk[t]);
    printf("  ");
    for (int t = 0; t < BM_TRACKS; t++) printf(" %.1f/%u", S.vblocks ? S.vsum[t] / S.vblocks : 0.0, S.vmax[t]);
    printf("  %5u %5u %2u-%2u %3us  %6.1f %6.1f\n", brake_n, relief_n, cap_min, POOL_CAP(), worst_hold_s, cpu_n ? cpu_ns / cpu_n / 1000.0 : 0.0, cpu_max_ns / 1000.0);
    cpu_ns = cpu_max_ns = 0; cpu_n = 0;
    if (cost_n) {
        printf("        core cost/render (x16 ns) T1..T6 + mix:");
        for (int t = 0; t <= BM_TRACKS; t++) printf(" %5.0f", cost_sum[t] / cost_n);
        printf(" | UI %.2f ms/s | events %d", ui_ns / 60.0 / 1e6, bm_pattern_count_events(&app.pat));
        puts("");
    }
    memset(cost_sum, 0, sizeof cost_sum); cost_n = 0; ui_ns = 0;
    if (S.out_of_range) PROBLEM("minute %d: %u output samples beyond 24 bits", m, S.out_of_range);
    for (int t = 0; t < BM_TRACKS; t++) if (S.tbig[t]) PROBLEM("minute %d: T%d buffer blew up (%u samples above 64x full scale, peak %.1f)", m, t + 1, S.tbig[t], S.tpk[t]);
    if (S.nan_state) PROBLEM("minute %d: NaN/inf in the mix state (%u blocks)", m, S.nan_state);
    if (S.nan_voice) PROBLEM("minute %d: NaN/inf in a WAVE voice (%u)", m, S.nan_voice);
    /* the DSP guard resets a NaN/inf state before the checks above can see it: it counts them */
    static uint32_t fix_seen;
    if (app.ctx.st.nan_fix < fix_seen) fix_seen = 0;                     /* a new boot */
    if (app.ctx.st.nan_fix != fix_seen) PROBLEM("minute %d: the DSP guard reset %u states with NaN/inf", m, app.ctx.st.nan_fix - fix_seen);
    fix_seen = app.ctx.st.nan_fix;
    /* a saturated delay line is the int16 store clipping a hot feedback loop (like tape): reported, not a fault */
    if (S.sat_dly > 1000) printf("  (minute %d: delay line clipped in %u samples)\n", m, S.sat_dly);
}

static double minute_rms_track[64][BM_TRACKS];

typedef void (*event_fn)(uint32_t sec);

static void run(const char *name, int minutes, event_fn each_second)
{
    static int32_t out[BM_BLOCK * 2];
    printf("\n== %s: %d simulated minutes\n", name, minutes);
    print_header();
    memset(&S, 0, sizeof S);
    brake_n = 0;
    uint32_t total = (uint32_t)minutes * 60u * 44118u, sub = 0, sec_frames = 0, sec = 0;
    double tsq[BM_TRACKS] = {0};
    uint64_t tn = 0;
    for (uint32_t f = 0; f < total; f += BM_BLOCK) {
        if (sub == 0) { brake_step(); cpu_r_ns = 0; }
        double t0 = now_ns();
        bm_app_audio(&app, out, BM_BLOCK);
        cpu_r_ns += now_ns() - t0;
        frames_done += BM_BLOCK;
        check_block(out, BM_BLOCK);
        for (int t = 0; t < BM_TRACKS; t++)
            if (app.ctx.tact & (1u << t)) for (int i = 0; i < (int)BM_BLOCK; i++) { double x = app.ctx.tbuf[t][i] / 32768.0; tsq[t] += x * x; }
        tn += BM_BLOCK;
        wav_put(out, BM_BLOCK);
        sub = (sub + 1) & 3u;
        if (sub == 0) { brake_after(); cpu_ns += cpu_r_ns; cpu_n++; if (cpu_r_ns > cpu_max_ns) cpu_max_ns = cpu_r_ns; }
        sec_frames += BM_BLOCK;
        if (sec_frames >= 44118u) {
            sec_frames -= 44118u;
            sec++;
            now_ms += 1000;
            { double u0 = now_ns(); for (int k = 0; k < 30; k++) bm_app_tick(&app, now_ms - 1000 + (uint32_t)k * 33u); ui_ns += now_ns() - u0; }
            cost_sample();
            check_buffers();
            if (each_second) each_second(sec);
            if (sec % 60u == 0) {
                int m = (int)(sec / 60u);
                print_minute(m);
                if (m - 1 < 64) for (int t = 0; t < BM_TRACKS; t++) minute_rms_track[m - 1][t] = sqrt(tsq[t] / (double)tn);
                memset(&S, 0, sizeof S); memset(tsq, 0, sizeof tsq); tn = 0;
                brake_n = 0; relief_n = 0; cap_min = 99; worst_hold_s = 0;
            }
        }
    }
    /* each channel still sounds at the end as at the start (no machine went silent) */
    int last = minutes - 1 < 64 ? minutes - 1 : 63;
    for (int t = 0; t < BM_TRACKS; t++) {
        double a = minute_rms_track[0][t], b = minute_rms_track[last][t];
        int muted = app.pat.trk[t].mute != 0;              /* 0.9: mutes are live console state and survive pattern swaps */
        printf("  T%d rms: first minute %.4f, last %.4f%s\n", t + 1, a, b, muted ? " (muted at the end)" : "");
        if (a > 0.002 && b < a * 0.2 && !muted) PROBLEM("%s: T%d went (almost) silent: rms %.4f -> %.4f", name, t + 1, a, b);
    }
}

/* ------------------------------------------------------------ what a player does */
static uint32_t quiet_cap = 0, quiet_t6 = 0;

static void tweak_dense(uint32_t sec)
{
    bm_pattern *p = &app.pat;
    /* minutes 10-12: T1, T4 and T5 muted, the load drops; the brake has to let T6 have its voices */
    if (sec == 600 || sec == 720)
        for (int t = 0; t < BM_TRACKS; t++)
            if (t == 0 || t == 3 || t == 4) cmd(CMD_SET_TRACK, (uint8_t)t, (uint8_t)offsetof(bm_track, mute), sec == 600, 0);
    if (sec > 690 && sec <= 720) {
        uint32_t c6 = 0;
        for (int i = 0; i < BM_VOICES; i++) c6 += app.ctx.voice[i].active && app.ctx.voice[i].track == 5;
        if (c6 > quiet_t6) quiet_t6 = c6;
        if (sec == 720) quiet_cap = POOL_CAP();
    }
    if (sec % 3 == 0) {                                   /* knobs: filters and resonance anywhere */
        int t = 2 + (int)rnd(4);
        cmd(CMD_SET_TRACK, (uint8_t)t, (uint8_t)offsetof(bm_track, macro), (uint8_t)rnd(128), 0);
        cmd(CMD_SET_TRACK, (uint8_t)t, (uint8_t)(offsetof(bm_track, macro) + 1), (uint8_t)(64 + rnd(64)), 0);
        cmd(CMD_SET_TRACK, (uint8_t)t, (uint8_t)(offsetof(bm_track, macro) + 2 + rnd(2)), (uint8_t)rnd(128), 0);
    }
    if (sec % 7 == 0) {                                   /* BITTAL filter type, BUNO-6 VCA and HPF */
        cmd(CMD_SET_PRM, 4, BT_FT, (uint8_t)rnd(BT_FILTERS), 0);
        cmd(CMD_SET_PRM, 5, BN_HPF, (uint8_t)rnd(4), 0);
        cmd(CMD_SET_PRM, 5, BN_VCA, (uint8_t)rnd(2), 0);
    }
    if (sec % 300 == 150) cmd(CMD_ENGINE, 2, ENG_MINI, 0, 0);       /* T3: B-MINI */
    if (sec % 300 == 0) cmd(CMD_ENGINE, 2, ENG_ACID, 0, 0);         /* and back */
    if (sec % 300 == 151 || sec % 300 == 1) {
        cmd(CMD_SET_TRACK, 2, (uint8_t)offsetof(bm_track, macro), 120, 0);
        cmd(CMD_SET_TRACK, 2, (uint8_t)(offsetof(bm_track, macro) + 1), 127, 0);
    }
    if (sec % 11 == 0) {                                  /* ROLL on a drum lane for a few seconds */
        uint8_t lane = (uint8_t)rnd(BM_LANES);
        cmd(CMD_ROLL_CFG, (uint8_t)rnd(3), 50 + (uint8_t)rnd(20), (uint8_t)rnd(40), 384);
        cmd(CMD_ROLL, 0, lane, 1, 6 + (int32_t)rnd(3) * 6);
    }
    if (sec % 11 == 4) for (int l = 0; l < BM_LANES; l++) cmd(CMD_ROLL, 0, (uint8_t)l, 0, 0);
    if (sec % 13 == 0) cmd(CMD_PAD_KEY, 1, (uint8_t)rnd(16), 120, 0);   /* a pad held for a while */
    if (sec % 13 == 6) for (int l = 0; l < 16; l++) cmd(CMD_PAD_KEY, 0, (uint8_t)l, 0, 0);
    if (sec % 17 == 0) cmd(CMD_PUNCH, (uint8_t)rnd(16), 0, 0, 0);
    if (sec % 17 == 3) cmd(CMD_PUNCH, 0xFF, 0, 0, 0);
    if (sec % 19 == 0) cmd(CMD_MOVE, (uint8_t)(int8_t)((int)rnd(127) - 63), 0, 0, 0);
    if (sec % 19 == 9) cmd(CMD_MOVE, 0, 0, 0, 0);
    if (sec % 23 == 0) cmd(CMD_DRIVE, (uint8_t)rnd(128), 0, 0, 0);
    if (sec % 29 == 0) {                                  /* live notes held long on the poly synths */
        for (int k = 0; k < 4; k++) cmd(CMD_NOTE_ON, (uint8_t)(3 + rnd(3)), (uint8_t)(40 + rnd(40)), 127, 0);
    }
    if (sec % 29 == 20) for (int t = 3; t < 6; t++) for (int n = 40; n < 80; n++) cmd(CMD_NOTE_OFF, (uint8_t)t, (uint8_t)n, 0, 0);
    if (sec % 31 == 0) for (int t = 0; t < BM_TRACKS; t++) cmd(CMD_SET_TRACK, (uint8_t)t, (uint8_t)offsetof(bm_track, djf), (uint8_t)(rnd(3) ? 0 : rnd(128)), 0);
    if (sec % 37 == 0) cmd(CMD_FILL, 1, 0, 0, 0);
    if (sec % 37 == 5) cmd(CMD_FILL, 0, 0, 0, 0);
    (void)p;
}

static void tweak_demo(uint32_t sec)
{
    if (sec % 47 == 0) cmd(CMD_SCENE, (uint8_t)rnd(4), 0, 0, 0);
    if (sec % 47 == 30) cmd(CMD_SCENE, 0, 0, 0, 0);
}

/* a player: through the UI, PLAY, then every few seconds into a channel (HOME, SELECT, SEQ), REC
 * on for a while, white keys held and released (they record into the pattern with REC) */
static int live_key = -1;
static void tweak_live(uint32_t sec)
{
    if (live_key >= 0) { bm_app_key(&app, live_key, 0, now_ms); live_key = -1; }
    switch (sec % 8) {
    case 0: bm_app_button(&app, BTN_HOME, 1, now_ms); bm_app_button(&app, BTN_HOME, 0, now_ms + 30); break;
    case 1: bm_app_encoder(&app, ENC_SELECT, (int)rnd(3) - 1, now_ms); break;
    case 2: bm_app_button(&app, BTN_SEQ, 1, now_ms); bm_app_button(&app, BTN_SEQ, 0, now_ms + 30); break;
    case 3: if (sec % 64 == 3 || sec % 64 == 35) { bm_app_button(&app, BTN_REC, 1, now_ms); bm_app_button(&app, BTN_REC, 0, now_ms + 30); } break;
    default:
        live_key = rnd(4) ? bm_white_key(BM_CH_W0 + (int)rnd(6)) : bm_black_key((int)rnd(11));   /* canales y mutes: los patrones vacíos callarían el final */
        bm_app_key(&app, live_key, 1, now_ms);
        if (rnd(3) == 0) bm_app_encoder(&app, ENC_K1 + (int)rnd(4), (int)rnd(5) - 2, now_ms);
        break;
    }
    tweak_demo(sec);
}

/* every sound of every machine: hold a chord 2 s, let go, and time until its voices free (they
 * cost CPU and take pool slots until then) */
static int track_voices(int t)
{
    int n = 0;
    for (int i = 0; i < BM_VOICES; i++) n += app.ctx.voice[i].active && app.ctx.voice[i].track == t;
    return n;
}

static void tails(void)
{
    static int32_t out[BM_BLOCK * 2];
    printf("\n== tails: time from note off until the voices free, for every sound\n");
    boot_with_take();
    int long_ones = 0;
    for (int pass = 0; pass < 5; pass++) {
        int t = pass == 0 ? 2 : pass + 1, eng = pass == 0 ? ENG_ACID : pass == 1 ? ENG_MINI : -1;
        if (pass <= 1) { t = 2; cmd(CMD_ENGINE, 2, (uint8_t)eng, 0, 0); }
        bm_app_audio(&app, out, BM_BLOCK);
        int ns = bm_sound_count(&app, t);
        double worst = 0; int worst_s = -1, never = 0;
        for (int s = 0; s < ns; s++) {
            cmd(CMD_ALL_OFF, 0, 0, 0, 0);
            cmd(CMD_PRESET, (uint8_t)t, (uint8_t)s, 0, 0);
            for (int k = 0; k < 40; k++) { bm_app_audio(&app, out, BM_BLOCK); if (k % 10 == 0) { now_ms += 20; bm_app_tick(&app, now_ms); } }
            for (int k = 0; k < 600; k++) { bm_app_audio(&app, out, BM_BLOCK); if (!track_voices(t)) break; }   /* silence first */
            static const uint8_t N[4] = {48, 55, 60, 64};
            for (int k = 0; k < 4; k++) cmd(CMD_NOTE_ON, (uint8_t)t, N[k], 120, 0);
            for (uint32_t f = 0; f < 2u * 44118u; f += BM_BLOCK) bm_app_audio(&app, out, BM_BLOCK);
            for (int k = 0; k < 4; k++) cmd(CMD_NOTE_OFF, (uint8_t)t, N[k], 0, 0);
            uint32_t f = 0;
            for (; f < 60u * 44118u && (f == 0 || track_voices(t)); f += BM_BLOCK) bm_app_audio(&app, out, BM_BLOCK);
            double sec = f / 44118.0;
            if (track_voices(t)) never++;
            if (sec > worst) { worst = sec; worst_s = s; }
            if (sec > 12.0) { long_ones++; printf("  T%d %-12s sound %3d (%s): %s %.1f s\n", t + 1, bm_engine_names[app.pat.trk[t].engine], s, bm_sound_name(&app, t), track_voices(t) ? "still sounding after" : "frees after", sec); }
        }
        printf("  T%d %-8s %3d sounds: longest tail %.1f s (sound %d), %d never free\n", t + 1, bm_engine_names[app.pat.trk[t].engine], ns, worst, worst_s, never);
        if (never) PROBLEM("T%d %s: %d sounds whose voices never free after note off", t + 1, bm_engine_names[app.pat.trk[t].engine], never);
    }
    (void)long_ones;
}

/* BUNO-6 alone, one held note through its chorus: the high-frequency content of the output per
 * minute (energy of the second difference over the energy) must not grow with time */
static void chorus(int minutes)
{
    static int32_t out[BM_BLOCK * 2];
    printf("\n== chorus: one BUNO-6 note held through CHORUS I for %d minutes\n", minutes);
    boot_with_take();
    for (int t = 0; t < 5; t++) app.pat.trk[t].mute = 1;
    bm_prm_defaults(&app.pat, 5);                         /* a fixed sound, not whatever factory sound 2 is */
    app.pat.trk[5].macro[0] = 50; app.pat.trk[5].macro[1] = 0;
    app.pat.prm[5][BN_CHORUS] = BN_CH_I; app.pat.prm[5][BN_S] = 127; app.pat.prm[5][BN_VCA] = 1;
    app.pat.trk[5].rsend = 0; app.pat.trk[5].send = 0; app.pat.meta.rv_mix = 0;
    cmd(CMD_NOTE_ON, 5, 45, 100, 0);
    double first = 0;
    for (int m = 1; m <= minutes; m++) {
        double e = 0, h = 0;
        int32_t p1 = 0, p2 = 0;
        for (uint32_t f = 0; f < 60u * 44118u; f += BM_BLOCK) {
            if (f % (44118u * 20u) < BM_BLOCK) cmd(CMD_NOTE_ON, 5, 45, 100, 0);   /* keep it held (the brake model is off) */
            bm_app_audio(&app, out, BM_BLOCK);
            for (uint32_t i = 0; i < BM_BLOCK; i++) {
                int32_t x = out[2 * i] >> 8;
                double d2 = (double)x - 2.0 * p1 + p2;
                e += (double)x * x; h += d2 * d2;
                p2 = p1; p1 = x;
            }
        }
        double r = e > 0 ? h / e : 0;
        if (m == 1) first = r;
        printf("  minute %2d: HF ratio %.3g (%.2fx the first minute)\n", m, r, first > 0 ? r / first : 0);
        if (m > 1 && first > 0 && r > first * 1.5) { PROBLEM("chorus: minute %d has %.1fx the high-frequency noise of minute 1", m, r / first); break; }
    }
}

/* TWO CORES: the dense scenario with the channels split between "cores" (in turn, as on the PC),
 * the split changing every render; it must give the same samples as one core */
static void par_go_pc(void *u, uint32_t done, uint32_t n) { bm_audio_par_process((bm_ctx *)u, done, n); }
static int par_wait_pc(void *u) { (void)u; return 0; }

static void dual(int minutes)
{
    static int32_t out[BM_BLOCK * 2];
    static uint32_t sum[3][60 * 60];
    static uint32_t bsum[3][44118 / BM_BLOCK + 1];
    uint32_t first_x = 0xFFFFFFFFu;
    printf("\n== dual: dense, %d minutes with one core and with the split changing every render\n", minutes);
    if (minutes > 60) minutes = 60;
    for (int pass = 0; pass < 3; pass++) {
        boot_with_take();
        dense_pattern();
        rng = 0x1234567u;
        app.ctx.par_go = par_go_pc; app.ctx.par_wait = par_wait_pc; app.ctx.par_user = &app.ctx;
        app.ctx.par_on = (uint8_t)(pass == 2);
        cmd(CMD_PLAY, 1, 0, 0, 0);
        uint32_t r = 777u, sec = 0, sf = 0, h = 2166136261u;
        for (uint32_t f = 0; f < (uint32_t)minutes * 60u * 44118u; f += BM_BLOCK) {
            r = r * 1664525u + 1013904223u;
            app.ctx.par_mask = (uint8_t)((r >> 24) & 0x3Fu);
            bm_app_audio(&app, out, BM_BLOCK);
            for (int i = 0; i < (int)BM_BLOCK * 2; i++) h = (h ^ (uint32_t)out[i]) * 16777619u;
            uint32_t blk = f / BM_BLOCK;
            if (blk < 44118 / BM_BLOCK + 1) {
                uint32_t hb = 2166136261u;
                for (int i = 0; i < (int)BM_BLOCK * 2; i++) hb = (hb ^ (uint32_t)out[i]) * 16777619u;
                bsum[pass][blk] = hb;
                if (pass == 1 && first_x == 0xFFFFFFFFu)
                    for (int i = 0; i < BM_VOICES; i++)
                        if (app.ctx.voice[i].pend && app.ctx.voice[i].pend_track != app.ctx.voice[i].track &&
                            ((app.ctx.par_mask >> app.ctx.voice[i].track) ^ (app.ctx.par_mask >> app.ctx.voice[i].pend_track)) & 1) first_x = blk;
            }
            if ((sf += BM_BLOCK) >= 44118u) { sf -= 44118u; sum[pass][sec++] = h; now_ms += 1000; bm_app_tick(&app, now_ms); tweak_dense(sec); }
        }
        app.ctx.par_on = 0;
    }
    int bad = -1;
    for (int s = 0; s < minutes * 60 && bad < 0; s++) if (sum[1][s] != sum[2][s]) bad = s;
    int bb = -1;
    for (int k = 0; k < 44118 / (int)BM_BLOCK && bb < 0; k++) if (bsum[1][k] != bsum[2][k]) bb = k;
    /* pass 0 only warms up the global state a first boot leaves (it differs from a second boot) */
    printf("  first different block %d, first steal across the split at block %d\n", bb, (int)first_x);
    /* a voice stolen across the split starts after the other core is done (par_defer): from then on
     * the voice ages, and so later steals, may differ. Before that, any difference is a fault. */
    if (bb >= 0 && (uint32_t)bb < first_x) PROBLEM("dual: two cores differ from one at block %d, before any steal across the split", bb);
    else if (bad < 0) printf("  identical for %d seconds\n", minutes * 60);
    else printf("  identical until the first steal across the split (second %d differs, by design)\n", bad);
}

static void play(void)
{
    cmd(CMD_PLAY, 1, 0, 0, 0);
}

/* ------------------------------------------------------------ bench: the cost of each machine alone
 * Sequencer stopped, the brake off, every other channel silent. Each machine plays every one of its
 * sounds for a few seconds the same way (BR-8: a hit on 1-3 lanes every 16th; BB-303 and B-MINI: a
 * 16th-note line with accents; BX-7, BITTAL and BUNO-6: chords that change every half second, as many
 * notes as the machine has voices). Reported: PC time per 256 frames for the whole render, minus the
 * render with nothing sounding, and the voices that sounded on average. */
static double bench_render(int frames256, int (*step)(int k, int t), int t, double *voices)
{
    static int32_t out[BM_BLOCK * 2];
    double ns = 0, vs = 0;
    int k = 0;
    for (int r = 0; r < frames256; r++) {
        if (step && (r % 22) == 0) k = step(k, t);        /* every 22 renders ~ a 16th at 120 BPM */
        double t0 = now_ns();
        for (int b = 0; b < 4; b++) bm_app_audio(&app, out, BM_BLOCK);
        ns += now_ns() - t0;
        if (voices) vs += track_voices(t);
    }
    if (voices) *voices = vs / frames256;
    return ns / frames256;
}

static int step_drums(int k, int t)
{
    static const uint8_t L[6] = {0, 2, 4, 1, 6, 5};
    cmd(CMD_NOTE_ON, (uint8_t)t, L[k % 6], k % 4 == 0 ? 127 : 100, 0);
    if (k % 2) cmd(CMD_NOTE_ON, (uint8_t)t, L[(k + 3) % 6], 100, 0);
    if (k % 4 == 3) cmd(CMD_NOTE_ON, (uint8_t)t, 8 + k % 8, 100, 0);
    return k + 1;
}

static int step_one(int k, int t)
{
    cmd(CMD_NOTE_ON, (uint8_t)t, 0, k % 4 == 0 ? 127 : 100, 0);
    return k + 1;
}

static int step_mono(int k, int t)
{
    static const uint8_t N[8] = {36, 36, 48, 39, 41, 36, 43, 46};
    if (k) cmd(CMD_NOTE_OFF, (uint8_t)t, N[(k - 1) % 8], 0, 0);
    cmd(CMD_NOTE_ON, (uint8_t)t, N[k % 8], k % 4 == 0 ? 127 : 100, 0);
    return k + 1;
}

static int bench_poly_n = 4;
static int step_poly(int k, int t)
{
    static const uint8_t C[4][6] = {{48, 52, 55, 59, 62, 64}, {45, 48, 52, 55, 57, 60}, {41, 45, 48, 52, 55, 57}, {43, 47, 50, 53, 55, 59}};
    if (k % 8) return k + 1;                               /* a chord every half second */
    int c = (k / 8) % 4, p = (c + 3) % 4;
    for (int i = 0; i < 6; i++) cmd(CMD_NOTE_OFF, (uint8_t)t, C[p][i], 0, 0);
    for (int i = 0; i < bench_poly_n; i++) cmd(CMD_NOTE_ON, (uint8_t)t, C[c][i], 100, 0);
    return k + 1;
}

static void bench(int seconds)
{
    printf("\n== bench: each machine alone, every sound, %d s each (PC us per 256 frames over silence)\n", seconds);
    boot_with_take();
    app.ctx.par_auto = 0;
    for (int t = 0; t < BM_TRACKS; t++) { app.pat.trk[t].send = 0; app.pat.trk[t].rsend = 0; }
    app.pat.meta.rv_mix = 0; app.pat.meta.dly_mix = 0;
    double base = bench_render(400, NULL, 0, NULL);
    printf("  silence: %.1f us per 256 frames\n", base / 1000.0);
    static const int ENG[6] = {ENG_DRUM, ENG_ACID, ENG_MINI, ENG_FM, ENG_BITTAL, ENG_BUNO};
    static const int POLY[6] = {0, 0, 0, 6, 4, 6};
    int renders = seconds * 44118 / 256;
    for (int e = 0; e < 6; e++) {
        int t = ENG[e] == ENG_DRUM ? 0 : -1;
        for (int k = 2; k < BM_TRACKS && t < 0; k++) if (app.pat.trk[k].engine == ENG[e]) t = k;
        if (t < 0) { t = 2; cmd(CMD_ENGINE, 2, (uint8_t)ENG[e], 0, 0); bm_app_audio(&app, (int32_t[BM_BLOCK * 2]){0}, BM_BLOCK); }
        int ns = t == 0 ? 1 : bm_sound_count(&app, t);
        double sum = 0, vsum = 0, worst = 0; int worst_s = 0;
        for (int s = 0; s < ns; s++) {
            cmd(CMD_ALL_OFF, 0, 0, 0, 0);
            if (t) cmd(CMD_PRESET, (uint8_t)t, (uint8_t)s, 0, 0);
            for (int k = 0; k < 40; k++) { bm_app_audio(&app, (int32_t[BM_BLOCK * 2]){0}, BM_BLOCK); if (k % 10 == 0) { now_ms += 20; bm_app_tick(&app, now_ms); } }
            for (int k = 0; k < 2000 && track_voices(t); k++) bm_app_audio(&app, (int32_t[BM_BLOCK * 2]){0}, BM_BLOCK);
            bench_poly_n = POLY[e];
            double v = 0, us = (bench_render(renders, t == 0 ? step_drums : POLY[e] ? step_poly : step_mono, t, &v) - base) / 1000.0;
            sum += us; vsum += v;
            if (us > worst) { worst = us; worst_s = s; }
        }
        cmd(CMD_ALL_OFF, 0, 0, 0, 0);
        for (int k = 0; k < 4000 && track_voices(t); k++) bm_app_audio(&app, (int32_t[BM_BLOCK * 2]){0}, BM_BLOCK);
        printf("  %-7s T%d %2d sounds: %6.1f us avg, %6.1f us worst (sound %d), %.1f voices avg\n", bm_engine_names[ENG[e]], t + 1, ns,
               sum / ns, worst, worst_s, vsum / ns);
    }
    /* BR-8 by model: one lane, a hit every 16th (the decay as in the kit) */
    printf("  BR-8 per model, a hit every 16th:");
    bm_lane keep = app.pat.tr[0].lane[0];
    for (int m = 0; m < DRM_COUNT; m++) {
        app.pat.tr[0].lane[0].model = (uint8_t)m;
        cmd(CMD_ALL_OFF, 0, 0, 0, 0);
        for (int k = 0; k < 4000 && track_voices(0); k++) bm_app_audio(&app, (int32_t[BM_BLOCK * 2]){0}, BM_BLOCK);
        double v = 0, us = (bench_render(renders / 2, step_one, 0, &v) - base) / 1000.0;
        printf(" %s %.1f (%.1f v)", bm_drm_short[m], us, v);
    }
    app.pat.tr[0].lane[0] = keep;
    printf("\n");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int minutes = argc > 1 ? atoi(argv[1]) : 10;
    const char *sc = argc > 2 ? argv[2] : "all";
    if (argc > 3) { wav = fopen(argv[3], "wb"); if (wav) { uint8_t h[44] = {0}; fwrite(h, 1, 44, wav); } }
    if (minutes < 1) minutes = 1;
    bm_tables_init();
    int all = !strcmp(sc, "all");
    if (all || !strcmp(sc, "demo")) {
        boot_with_take();
        load_scale = 1.0f;
        play();
        run("demo", minutes, tweak_demo);
    }
    if (all || !strcmp(sc, "live")) {
        boot_with_take();
        play();
        run("live", minutes, tweak_live);
    }
    if (all || !strcmp(sc, "dense")) {
        boot_with_take();
        dense_pattern();
        play();
        run("dense", minutes, tweak_dense);
    }
    if ((all || !strcmp(sc, "dense")) && minutes >= 12) {
        printf("  load dropped (minutes 11-12): pool cap %u of %u, T6 up to %u voices\n", quiet_cap, BM_V_POOL_N, quiet_t6);
        if (quiet_cap < BM_V_POOL_N - 2 || quiet_t6 < 4) PROBLEM("dense: with the load down the voices did not come back (cap %u, T6 %u voices)", quiet_cap, quiet_t6);
    }
    if (all || !strcmp(sc, "wrap")) {
        boot_with_take();
        dense_pattern();
        play();
        bm_app_audio(&app, (int32_t[BM_BLOCK * 2]){0}, BM_BLOCK);
        /* 32-bit counters near their end (27 hours of uptime and more) */
        app.ctx.sample_clock = 0xFFFFFFFFu - 44118u * 20u;
        app.ctx.fx_w = 0xFFFFFFFEu - 44118u * 20u;
        app.ctx.st.free_ticks = 0xFFFFFFFFu - 4000u;
        app.ctx.age = 0xFFFFFFFFu - 2000u;
        run("wrap", minutes < 3 ? minutes : 3, tweak_dense);
    }
    if (all || !strcmp(sc, "tails")) tails();
    if (all || !strcmp(sc, "dual")) dual(minutes < 5 ? minutes : 5);
    if (all || !strcmp(sc, "chorus")) chorus(minutes > 8 ? minutes : 8);
    if (all || !strcmp(sc, "bench")) bench(4);
    if (wav) {
        uint8_t h[44];
        memcpy(h, "RIFF", 4);
        uint32_t v[2] = {36 + wav_frames * 4, wav_frames * 4};
        memcpy(h + 4, &v[0], 4); memcpy(h + 8, "WAVEfmt ", 8);
        uint32_t f16 = 16, sr = 44118, br = 44118 * 4;
        memcpy(h + 16, &f16, 4); h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0;
        memcpy(h + 24, &sr, 4); memcpy(h + 28, &br, 4); h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0;
        memcpy(h + 36, "data", 4); memcpy(h + 40, &v[1], 4);
        fseek(wav, 0, SEEK_SET); fwrite(h, 1, 44, wav); fclose(wav);
    }
    printf("\nbrake: %u voices faded in total\n", brake_total);
    printf("%d problems\n", problems);
    return problems ? 1 : 0;
}
