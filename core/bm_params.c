/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — parámetros de sonido, kits y presets de fábrica (core/bm_params.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bm_params.h"
#include "bm_drums.h"
#include "bm_snap.h"
#include "bm_util.h"
#include "bm_acid.h"

/* ------------------------------------------------------------ valores físicos */
float bm_cut_hz(int v) { return 20.0f * exp2f((float)v * (10.0f / 127.0f)); }
float bm_env_s(int v) { return 0.001f * powf(10.0f, (float)v * (4.0f / 127.0f)); }
float bm_lfo_hz(int v) { return 0.05f * exp2f((float)v * (9.0f / 127.0f)); }
float bm_reso_q(int v) { float x = (float)v / 127.0f; return 0.5f + 24.5f * x * x * x; }

/* ------------------------------------------------------------ las páginas de cada máquina
 * En el orden del panel de la máquina de verdad y con sus nombres (8 letras como mucho). */
#define P(n, s, d, m, f, l) {n, s, d, m, f, l}
static const char *const AC_WAVES[2] = {"SAW", "SQUARE"};
static const char *const MN_WAVES_N[MN_WAVES] = {"TRI", "SHARK", "SAW", "SQUARE", "WIDE", "NARROW"};
static const char *const RANGES[5] = {"32'", "16'", "8'", "4'", "2'"};
static const char *const WMODES[WM_MODES] = {"POLY", "MONO", "LEGATO"};
static const char *const MONO2[2] = {"MONO", "LEGATO"};

/* BB-303: TUNING es la transposición del canal; ACCENT y SLIDE, los del canal */
const bm_ppage bm_ac_pages[BM_AC_PAGES] = {
    {"VCO VCF", 4, {P("TUNING", PS_TRK(transpose), 24, 48, PF_TRSP, 0), P("CUTOFF", PS_M0 + 0, 40, 127, PF_OHZ, 0),
                    P("RESO", PS_M0 + 1, 96, 127, PF_PCT, 0), P("ENV MOD", PS_M0 + 2, 90, 127, PF_PCT, 0)}},
    {"ENV", 4, {P("DECAY", PS_M0 + 3, 56, 127, PF_DMS, 0), P("ACCENT", PS_TRK(accent), 100, 127, PF_PCT, 0),
                P("SLIDE", PS_TRK(glide), 7, 127, PF_GLIDE, 0), P("WAVE", AC_WAVE, 0, 1, PF_LIST, AC_WAVES)}},
};

/* los sonidos de fábrica del BB-303 (PRESETS). 0.9: the knobs of the TB-303 (0..127 = the pot from
 * 7:00 to 5:00; SLIDE 7 = the 303's fixed ~60 ms). The owner's manual's "Sample Sounds" and "Initial
 * Settings" as drawn; the rest, sounds of famous acid records tuned by ear (assets/presets/SOURCES.md). */
const bm_ac_preset bm_ac_presets[BM_AC_PRESETS] = {
    /* name          CUT  RES  ENV  DEC  ACC  SLIDE WAVE */
    {"CLASSIC",       38, 105,  90,  55,  90,  7, 0},     /* own: the classic acid squelch */
    {"E.BASS A",      44,  76, 102,  44,  25,  7, 0},     /* manual: Electric Bass Type A */
    {"E.BASS B",      76,  38,  95,   4,  44,  7, 0},     /* manual: Electric Bass Type B */
    {"UPRIGHT",       25,   0,  73,  73,  44,  7, 0},     /* manual: Acoustic Bass */
    {"SYNTH BASS",    51, 102, 102,  83,  64,  7, 1},     /* manual: Synthesized Bass */
    {"INIT 1982",     44,  19, 102,  48,  25,  7, 0},     /* manual: Initial Settings */
    {"CHICAGO 86",    30, 115,  85,  70, 100,  7, 0},     /* acid house's first jam, by ear */
    {"PHILLY 95",     50, 122, 110,  60, 127,  7, 0},     /* the screaming sweeps, by ear */
    {"ROBOT FUNK",    60,  70,  70,  45,  90,  7, 0},     /* the French riff (add DRIVE), by ear */
    {"BIG BEAT",      45, 110, 100,  60, 110,  7, 0},     /* "everybody needs" one, by ear */
    {"WINDSOR 93",    25,  95,  60,  35,  60,  7, 1},     /* minimal and sparse, by ear */
    {"BOMBAY 82",     55,  50,  55,  50,  60,  7, 0},     /* raga lines on a disco beat, by ear */
};

/* B-MINI: CONTROLLERS, OSCILLATOR BANK, MIXER, MODIFIERS y OUTPUT. MOD AMT lleva el destino: a la
 * izquierda los osciladores (vibrato), a la derecha el filtro. */
const bm_ppage bm_mn_pages[BM_MN_PAGES] = {
    {"CONTROL", 4, {P("TUNE", MN_TUNE, 64, 127, PF_BIP, 0), P("GLIDE", MN_GLIDE, 35, 127, PF_TIME, 0),
                    P("MOD RATE", MN_LRATE, 70, 127, PF_RATE, 0), P("MOD AMT", MN_LAMT, 64, 127, PF_MOD, 0)}},
    {"OSC 1", 2, {P("RANGE", MN_R1, 1, 4, PF_LIST, RANGES), P("WAVE", MN_W1, MN_WAVE_SAW, MN_WAVES - 1, PF_LIST, MN_WAVES_N)}},
    {"OSC 2", 3, {P("RANGE", MN_R2, 1, 4, PF_LIST, RANGES), P("FREQ", MN_T2, 66, 127, PF_BIP, 0),
                  P("WAVE", MN_W2, MN_WAVE_SAW, MN_WAVES - 1, PF_LIST, MN_WAVES_N)}},
    {"OSC 3", 3, {P("RANGE", MN_R3, 0, 4, PF_LIST, RANGES), P("FREQ", MN_T3, 64, 127, PF_BIP, 0),
                  P("WAVE", MN_W3, MN_WAVE_SQR, MN_WAVES - 1, PF_LIST, MN_WAVES_N)}},
    {"MIXER", 4, {P("OSC 1", MN_L1, 110, 127, PF_PCT, 0), P("OSC 2", MN_L2, 90, 127, PF_PCT, 0),
                  P("OSC 3", MN_L3, 50, 127, PF_PCT, 0), P("NOISE", MN_NOISE, 0, 127, PF_PCT, 0)}},
    {"FILTER", 4, {P("CUTOFF", PS_M0 + 0, 60, 127, PF_HZ, 0), P("EMPHASIS", PS_M0 + 1, 50, 127, PF_PCT, 0),
                   P("CONTOUR", PS_M0 + 2, 70, 127, PF_PCT, 0), P("KBD CTRL", MN_KEYT, 40, 127, PF_PCT, 0)}},
    {"F CONTOUR", 4, {P("ATTACK", MN_FA, 0, 127, PF_TIME, 0), P("DECAY", MN_FD, 55, 127, PF_TIME, 0),
                      P("SUSTAIN", MN_FS, 20, 127, PF_PCT, 0), P("RELEASE", MN_FR, 45, 127, PF_TIME, 0)}},
    {"L CONTOUR", 4, {P("ATTACK", MN_AA, 0, 127, PF_TIME, 0), P("DECAY", MN_AD, 70, 127, PF_TIME, 0),
                      P("SUSTAIN", MN_AS, 110, 127, PF_PCT, 0), P("RELEASE", MN_AR, 35, 127, PF_TIME, 0)}},
    {"OUTPUT", 4, {P("DRIVE", MN_DRIVE, 30, 127, PF_PCT, 0), P("MODE", MN_MODE, 0, 1, PF_LIST, MONO2),
                   P("VEL FILT", MN_VELF, 40, 127, PF_PCT, 0), P("VEL LVL", MN_VELA, 40, 127, PF_PCT, 0)}},
};

/* BX-7 (2026-10-21): lo que va encima de la voz (pat->fmv), con 64 = la voz tal cual. La voz misma
 * (algoritmo, feedback, LFO, operadores) se edita byte a byte con CMD_FMV desde core/bm_bx7.c.
 * MAIN: lo de la pantalla principal · ENV: el ADSR de volumen del FM-1 de fábrica. */
const bm_ppage bm_fx_pages[BM_FX_PAGES] = {
    {"MAIN", 3, {P("BRIGHT", PS_M0 + 0, 64, 127, PF_BIP, 0), P("PORTA", PS_TRK(glide), 0, 127, PF_PCT, 0),
                 P("KEY VEL", PS_TRK(accent), 100, 127, PF_PCT, 0)}},
    {"ENV", 4, {P("ATTACK", PS_TRK(attack), 0, 127, PF_PCT, 0), P("DECAY", PS_M0 + 2, 64, 127, PF_BIP, 0),
                P("SUSTAIN", PS_TRK(sustain), 64, 127, PF_BIP, 0), P("RELEASE", PS_M0 + 3, 64, 127, PF_BIP, 0)}},
};

/* BUNO-6: el panel del Juno-6 de izquierda a derecha */
static const char *const PWMS[BN_PWMS] = {"LFO", "MANUAL", "ENV"};
static const char *const BN_RANGES[3] = {"16'", "8'", "4'"};
static const char *const HPFS[4] = {"0", "1", "2", "3"};
static const char *const POLS[2] = {"+", "-"};
static const char *const VCAS[2] = {"ENV", "GATE"};
const char *const bm_bn_chorus_names[BN_CHORUSES] = {"OFF", "I", "II", "I+II"};

const bm_ppage bm_bn_pages[BM_BN_PAGES] = {
    {"LFO", 2, {P("RATE", BN_LRATE, 60, 127, PF_RATE, 0), P("DELAY", BN_LDELAY, 0, 127, PF_TIME, 0)}},
    {"DCO", 4, {P("LFO", BN_LFODCO, 0, 127, PF_PCT, 0), P("PWM", BN_PWM, 50, 127, PF_PCT, 0),
                P("PWM MODE", BN_PWMSRC, BN_PWM_LFO, BN_PWMS - 1, PF_LIST, PWMS), P("RANGE", BN_RANGE, 1, 2, PF_LIST, BN_RANGES)}},
    {"DCO WAVE", 4, {P("PULSE", BN_PULSE, 0, 1, PF_ONOFF, 0), P("SAW", BN_SAW, 1, 1, PF_ONOFF, 0),
                     P("SUB", BN_SUB, 40, 127, PF_PCT, 0), P("NOISE", BN_NOISE, 0, 127, PF_PCT, 0)}},
    {"HPF VCF", 4, {P("HPF", BN_HPF, 1, 3, PF_LIST, HPFS), P("FREQ", PS_M0 + 0, 72, 127, PF_HZ, 0),
                    P("RES", PS_M0 + 1, 30, 127, PF_PCT, 0), P("ENV", PS_M0 + 2, 50, 127, PF_PCT, 0)}},
    {"VCF MOD", 4, {P("ENV +/-", BN_ENVPOL, 0, 1, PF_LIST, POLS), P("LFO", PS_M0 + 3, 0, 127, PF_PCT, 0),
                    P("KYBD", BN_KEYT, 60, 127, PF_PCT, 0), P("VELO", BN_VELF, 20, 127, PF_PCT, 0)}},
    {"VCA", 2, {P("ENV/GATE", BN_VCA, 0, 1, PF_LIST, VCAS), P("VELO", BN_VELA, 0, 127, PF_PCT, 0)}},
    {"ENV", 4, {P("A", BN_A, 10, 127, PF_TIME, 0), P("D", BN_D, 70, 127, PF_TIME, 0),
                P("S", BN_S, 90, 127, PF_PCT, 0), P("R", BN_R, 60, 127, PF_TIME, 0)}},
    {"CHORUS", 1, {P("CHORUS", BN_CHORUS, BN_CH_I, BN_CHORUSES - 1, PF_LIST, bm_bn_chorus_names)}},
};

/* BITTAL: los nombres de Vital */
static const char *const BT_FILTERS_N[BT_FILTERS] = {"LP 24", "LP 12", "BP", "HP", "LADDER", "OFF"};
static const char *const BT_LSHAPES_N[BT_LSHAPES] = {"SINE", "TRI", "SAW", "SQUARE", "RANDOM"};
static const char *const BT_LDESTS_N[BT_LDESTS] = {"PITCH", "CUTOFF", "POSITION", "LEVEL"};
static const char *const UNIS[4] = {"1", "2", "3", "4"};
const char *const bm_btm_names[BTM_TARGETS] = {"---", "CUTOFF", "RESO", "POS 1", "POS 2", "POSITION", "LEVEL 2", "DETUNE",
                                               "LFO", "ENV", "DRIVE", "RELEASE", "SPACE"};

const bm_ppage bm_bt_pages[BM_BT_PAGES] = {
    {"OSC 1", 4, {P("LEVEL", BT_L1, 100, 127, PF_PCT, 0), P("TRANSP", BT_S1, 64, 127, PF_SEMI, 0),
                  P("FRAME", BT_P1, 0, 127, PF_PCT, 0), P("UNISON", BT_U1, 0, 3, PF_LIST, UNIS)}},
    {"OSC 1 +", 3, {P("DETUNE", BT_D1, 20, 127, PF_PCT, 0), P("FINE", BT_F1, 64, 127, PF_CENT, 0),
                    P("TABLE", BT_TAB1, 0, 63, PF_INT, 0)}},
    {"OSC 2", 4, {P("LEVEL", BT_L2, 0, 127, PF_PCT, 0), P("TRANSP", BT_S2, 64, 127, PF_SEMI, 0),
                  P("FRAME", BT_P2, 0, 127, PF_PCT, 0), P("UNISON", BT_U2, 0, 3, PF_LIST, UNIS)}},
    {"OSC 2 +", 3, {P("DETUNE", BT_D2, 20, 127, PF_PCT, 0), P("FINE", BT_F2, 64, 127, PF_CENT, 0),
                    P("TABLE", BT_TAB2, 0, 63, PF_INT, 0)}},
    {"FILTER", 4, {P("MODEL", BT_FT, BT_F_LP24, BT_FILTERS - 1, PF_LIST, BT_FILTERS_N), P("CUTOFF", BT_CUT, 100, 127, PF_HZ, 0),
                   P("RESO", BT_RES, 15, 127, PF_PCT, 0), P("DRIVE", BT_DRIVE, 0, 127, PF_PCT, 0)}},
    {"FILTER +", 4, {P("ENV2 AMT", BT_FENV, 64, 127, PF_BIP, 0), P("KEYTRK", BT_KEYT, 64, 127, PF_PCT, 0),
                     P("VELO", BT_VELF, 20, 127, PF_PCT, 0), P("ENV2 POS", BT_EPOS, 0, 127, PF_PCT, 0)}},
    {"ENV 1", 4, {P("A", BT_AA, 0, 127, PF_TIME, 0), P("D", BT_AD, 70, 127, PF_TIME, 0),
                  P("S", BT_AS, 110, 127, PF_PCT, 0), P("R", BT_AR, 55, 127, PF_TIME, 0)}},
    {"ENV 2", 4, {P("A", BT_MA, 0, 127, PF_TIME, 0), P("D", BT_MD, 65, 127, PF_TIME, 0),
                  P("S", BT_MS, 40, 127, PF_PCT, 0), P("R", BT_MR, 55, 127, PF_TIME, 0)}},
    {"LFO 1", 4, {P("SHAPE", BT_LSHAPE, BT_L_TRI, BT_LSHAPES - 1, PF_LIST, BT_LSHAPES_N), P("RATE", BT_LRATE, 60, 127, PF_RATE, 0),
                  P("DEST", BT_LDEST, BT_D_POS, BT_LDESTS - 1, PF_LIST, BT_LDESTS_N), P("AMOUNT", BT_LAMT, 0, 127, PF_PCT, 0)}},
    {"MACRO 1-2", 4, {P("M1 TO", BT_M1D, BTM_CUT, BTM_TARGETS - 1, PF_LIST, bm_btm_names), P("M1 AMT", BT_M1A, 110, 127, PF_BIP, 0),
                      P("M2 TO", BT_M2D, BTM_POS, BTM_TARGETS - 1, PF_LIST, bm_btm_names), P("M2 AMT", BT_M2A, 120, 127, PF_BIP, 0)}},
    {"MACRO 3-4", 4, {P("M3 TO", BT_M3D, BTM_LFO, BTM_TARGETS - 1, PF_LIST, bm_btm_names), P("M3 AMT", BT_M3A, 120, 127, PF_BIP, 0),
                      P("M4 TO", BT_M4D, BTM_SPACE, BTM_TARGETS - 1, PF_LIST, bm_btm_names), P("M4 AMT", BT_M4A, 120, 127, PF_BIP, 0)}},
    {"VOICE", 3, {P("MODE", BT_MODE, WM_POLY, WM_MODES - 1, PF_LIST, WMODES), P("GLIDE", BT_GLIDE, 0, 127, PF_TIME, 0),
                  P("VEL LVL", BT_VELA, 50, 127, PF_PCT, 0)}},
};

/* las macros que no están en ninguna página: F DECAY de B-MINI y las 4 de BITTAL */
static const bm_pdef MN_FDEC = P("F DECAY", PS_M0 + 3, 64, 127, PF_BIP, 0);
static const bm_pdef BT_MAC[4] = {P("MACRO 1", PS_M0 + 0, 64, 127, PF_PCT, 0), P("MACRO 2", PS_M0 + 1, 0, 127, PF_PCT, 0),
                                  P("MACRO 3", PS_M0 + 2, 0, 127, PF_PCT, 0), P("MACRO 4", PS_M0 + 3, 0, 127, PF_PCT, 0)};

/* T3-T6: las máquinas que se eligen, en el orden de GLO > SELECT MACHINE */
const uint8_t bm_machines[BM_MACHINES] = {ENG_ACID, ENG_MINI, ENG_BUNO, ENG_BITTAL, ENG_FM};

const char *bm_machine_origin(int engine)
{
    switch (engine) {
    case ENG_ACID: return "INSPIRED BY THE TB-303";
    case ENG_MINI: return "INSPIRED BY THE MINIMOOG";
    case ENG_FM: return "INSPIRED BY THE DX7";
    case ENG_BITTAL: return "INSPIRED BY VITAL";
    case ENG_BUNO: return "INSPIRED BY THE JUNO-6";
    default: return "";
    }
}

/* ------------------------------------------------------------ batería */
static const char *const DRM_LIST[DRM_COUNT] = {"BD 808", "BD 909", "SD 808", "SD 909", "HAT 808", "METAL", "CLAP",
                                                "TOM", "COWBELL", "RESON", "NOISE", "FM"};
static const char *const CHOKES[4] = {"NO", "1", "2", "3"};
const bm_ppage bm_lane_pages[BM_LANE_PAGES] = {
    {"HIT", 4, {P("TUNE", LN_TUNE, 64, 127, PF_DSEMI, 0), P("DECAY", LN_DECAY, 64, 127, PF_PCT, 0),
                  P("TONE", LN_TONE, 64, 127, PF_PCT, 0), P("COLOR", LN_COLOR, 64, 127, PF_PCT, 0)}},
    {"INSTRUMENT", 4, {P("MODEL", LN_MODEL, 0, DRM_COUNT - 1, PF_LIST, DRM_LIST), P("LEVEL", LN_LEVEL, 100, 127, PF_INT, 0),
                        P("CHOKE", LN_CHOKE, 0, 3, PF_LIST, CHOKES), P("LENGTH", LN_LEN, 16, 64, PF_INT, 0)}},
    /* el secuenciador sin capa (2026-10-21): lo que más se toca del instrumento elegido */
    {"SEQ", 4, {P("LEVEL", LN_LEVEL, 100, 127, PF_INT, 0), P("LENGTH", LN_LEN, 16, 64, PF_INT, 0),   /* LENGTH: todo el kit */
                P("DECAY", LN_DECAY, 64, 127, PF_PCT, 0), P("TONE", LN_TONE, 64, 127, PF_PCT, 0)}},
};

/* AFINA: semitonos = (tune - 64) * 3 / 8 sobre el tono base del modelo (bm_drm_base_hz).
 * Los bombos: TONO y COLOR arriba (más click y más FM de ataque) y más nivel, para que
 * pasen por encima del bajo y los pads; el 909 con un poco más de cuerpo (DECAY 66). */
#define L(n, m, tu, de, to, co, le, ch) {n, m, tu, de, to, co, le, ch}
const bm_kit bm_kits[BM_KITS_N] = {
    {"808", {
        L("KICK", DRM_BD_ANALOG, 64, 70, 64, 62, 120, 0), L("KICK2", DRM_BD_ANALOG, 72, 40, 70, 90, 100, 0),
        L("CL HH", DRM_HH_ANALOG, 64, 25, 100, 20, 80, 1), L("OP HH", DRM_HH_ANALOG, 64, 85, 100, 20, 75, 1),
        L("SNARE", DRM_SD_ANALOG, 64, 55, 70, 75, 95, 0), L("CLAP", DRM_CLAP, 64, 50, 60, 50, 95, 0),
        L("RIM", DRM_RESON, 8, 15, 100, 76, 85, 0), L("TOM L", DRM_TOM, 51, 70, 50, 30, 100, 0),
        L("TOM M", DRM_TOM, 68, 68, 50, 30, 100, 0), L("TOM H", DRM_TOM, 81, 66, 50, 30, 100, 0),
        L("COWBL", DRM_COWBELL, 64, 50, 50, 100, 85, 0), L("CYMBL", DRM_HH_METAL, 60, 110, 80, 40, 75, 0),
        L("SHAKE", DRM_NOISE, 64, 25, 100, 50, 80, 0), L("CLAVE", DRM_RESON, 82, 40, 0, 0, 127, 0),
        L("CONGA", DRM_TOM, 106, 45, 40, 10, 95, 0), L("ZAP", DRM_FM, 90, 35, 80, 20, 85, 0)}},
    {"909", {
        L("KICK", DRM_BD_SINTE, 64, 66, 72, 64, 127, 0), L("KICK2", DRM_BD_SINTE, 70, 35, 80, 110, 100, 0),
        L("CL HH", DRM_HH_METAL, 64, 20, 110, 60, 80, 1), L("OP HH", DRM_HH_METAL, 64, 80, 110, 60, 75, 1),
        L("SNARE", DRM_SD_SINTE, 64, 50, 60, 80, 95, 0), L("CLAP", DRM_CLAP, 64, 60, 70, 70, 95, 0),
        L("RIM", DRM_RESON, 12, 12, 110, 70, 85, 0), L("TOM L", DRM_TOM, 56, 62, 80, 50, 100, 0),
        L("TOM M", DRM_TOM, 72, 60, 80, 50, 100, 0), L("TOM H", DRM_TOM, 86, 58, 80, 50, 100, 0),
        L("RIDE", DRM_HH_METAL, 72, 100, 120, 30, 70, 0), L("CRASH", DRM_HH_METAL, 60, 120, 100, 90, 70, 0),
        L("SHAKE", DRM_NOISE, 64, 20, 110, 40, 75, 0), L("CLAVE", DRM_RESON, 82, 35, 10, 0, 127, 0),
        L("CONGA", DRM_TOM, 104, 40, 60, 20, 95, 0), L("ZAP", DRM_FM, 96, 30, 100, 30, 85, 0)}},
    {"SYNTH", {
        L("KICK", DRM_BD_SINTE, 56, 90, 56, 84, 124, 0), L("SUB", DRM_BD_ANALOG, 52, 110, 40, 10, 127, 0),
        L("HAT", DRM_HH_ANALOG, 70, 15, 110, 90, 80, 1), L("OHAT", DRM_HH_METAL, 66, 70, 100, 40, 75, 1),
        L("SNARE", DRM_SD_SINTE, 76, 40, 100, 100, 95, 0), L("CLAP", DRM_CLAP, 70, 70, 90, 110, 95, 0),
        L("BLIP", DRM_FM, 100, 10, 50, 80, 85, 0), L("ZAP", DRM_FM, 70, 30, 120, 40, 90, 0),
        L("METAL", DRM_FM, 80, 60, 100, 110, 80, 0), L("BLOCK", DRM_RESON, 40, 30, 30, 30, 90, 0),
        L("COWB", DRM_COWBELL, 70, 40, 70, 60, 85, 0), L("NOISE", DRM_NOISE, 64, 60, 40, 0, 80, 0),
        L("SHAK", DRM_NOISE, 64, 15, 110, 70, 80, 0), L("TOM", DRM_TOM, 60, 70, 60, 40, 100, 0),
        L("PERC", DRM_TOM, 100, 40, 90, 120, 90, 0), L("KICK3", DRM_BD_ANALOG, 80, 30, 80, 127, 100, 0)}},
};

void bm_kit_apply(bm_tr *tr, int kit)
{
    const bm_kit *k = &bm_kits[(kit % BM_KITS_N + BM_KITS_N) % BM_KITS_N];
    for (int l = 0; l < BM_LANES; l++) {
        const bm_kit_lane *s = &k->lane[l];
        bm_lane *d = &tr->lane[l];
        d->model = s->model; d->tune = s->tune; d->decay = s->decay; d->tone = s->tone;
        d->color = s->color; d->level = s->level; d->choke = s->choke;
    }
}

const char *bm_lane_label(const bm_tr *tr, int kit, int lane)
{
    const bm_kit_lane *s = &bm_kits[(kit % BM_KITS_N + BM_KITS_N) % BM_KITS_N].lane[lane & 15];
    int m = tr->lane[lane & 15].model;
    return m == s->model || m >= DRM_COUNT ? s->name : bm_drm_short[m];
}

/* ------------------------------------------------------------ comunes */
const bm_ppage *bm_engine_pages(int engine, int *n)
{
    switch (engine) {
    case ENG_ACID: *n = BM_AC_PAGES; return bm_ac_pages;
    case ENG_MINI: *n = BM_MN_PAGES; return bm_mn_pages;
    case ENG_FM: *n = BM_FX_PAGES; return bm_fx_pages;
    case ENG_BUNO: *n = BM_BN_PAGES; return bm_bn_pages;
    case ENG_BITTAL: *n = BM_BT_PAGES; return bm_bt_pages;
    default: *n = 0; return NULL;
    }
}

int bm_engine_page_of(int engine, int what)
{
    /* ENV, ENV otra vez, LFO: BB-303, B-MINI, BX-7, BITTAL, BUNO-6 */
    static const int8_t PG[5][3] = {{1, 1, -1}, {6, 7, 0}, {1, 1, 3}, {6, 7, 8}, {6, 6, 0}};
    if (engine < ENG_ACID || engine >= ENG_COUNT || what < 0 || what > 2) return -1;
    return PG[engine - ENG_ACID][what];
}

const bm_pdef *bm_engine_macro(int engine, int i)
{
    int n;
    const bm_ppage *pg = bm_engine_pages(engine, &n);
    i &= 3;
    for (int k = 0; k < n; k++)
        for (int j = 0; j < pg[k].n; j++)
            if (pg[k].p[j].slot == PS_M0 + (unsigned)i) return &pg[k].p[j];
    if (engine == ENG_MINI && i == 3) return &MN_FDEC;
    if (engine == ENG_BITTAL) return &BT_MAC[i];
    return NULL;
}

/* B-MINI, MOD AMT: una perilla sobre la cantidad y el destino del LFO (64 = nada) */
static int mod_get(const uint8_t *q)
{
    int h = (q[MN_LAMT] + 1) / 2;
    if (!q[MN_LAMT]) return 64;
    return q[MN_LDEST] == MN_D_CUT ? (64 + h > 127 ? 127 : 64 + h) : 64 - h;
}

int bm_prm_get(const bm_pattern *p, int t, const bm_pdef *d)
{
    if (d->slot & PS_M0) return p->trk[t].macro[d->slot & 3];
    if (bm_ps_trk(d->slot)) return ((const uint8_t *)&p->trk[t])[d->slot & 0x3F];
    if (d->fmt == PF_MOD) return mod_get(p->prm[t]);
    return p->prm[t][d->slot];
}

int bm_prm_split(const bm_pdef *d, int v, uint8_t slot[2], uint8_t val[2])
{
    if (d->fmt != PF_MOD) { slot[0] = d->slot; val[0] = (uint8_t)v; return 1; }
    int amt = v > 64 ? (v - 64) * 2 : (64 - v) * 2;
    slot[0] = MN_LAMT; val[0] = (uint8_t)(amt > 127 ? 127 : amt);
    slot[1] = MN_LDEST; val[1] = (uint8_t)(v >= 64 ? MN_D_CUT : MN_D_PITCH);
    return 2;
}

static void prm_set(bm_pattern *p, int t, const bm_pdef *d, int v)
{
    if (d->slot & PS_M0) { p->trk[t].macro[d->slot & 3] = (uint8_t)v; return; }
    if (bm_ps_trk(d->slot)) { ((uint8_t *)&p->trk[t])[d->slot & 0x3F] = (uint8_t)v; return; }
    uint8_t s[2], x[2];
    int n = bm_prm_split(d, v, s, x);
    for (int i = 0; i < n; i++) if (s[i] < BM_PRM) p->prm[t][s[i]] = x[i];
}

void bm_prm_defaults(bm_pattern *p, int t)
{
    memset(p->prm[t], 0, BM_PRM);
    int n, e = p->trk[t].engine;
    for (int i = 0; i < 4; i++) { const bm_pdef *d = bm_engine_macro(e, i); if (d) prm_set(p, t, d, d->def); }
    const bm_ppage *pg = bm_engine_pages(e, &n);
    for (int i = 0; i < n; i++)
        for (int k = 0; k < pg[i].n; k++)
            if (pg[i].p[k].slot != PS_TRK(transpose)) prm_set(p, t, &pg[i].p[k], pg[i].p[k].def);   /* el tono del canal queda */
    if (e == ENG_FM) bm_bx7_fx_defaults(p->prm[t]);
    if (e == ENG_ACID) bm_acid_fx_defaults(p->prm[t]);
    if (bm_px_eng(e)) bm_px_fx_defaults(p->prm[t]);
}

typedef char bm_assert_px[(BM_SNAP_PRM <= PX_FX) ? 1 : -1];   /* un preset no pisa los efectos */

void bm_px_fx_defaults(uint8_t *prm)
{
    prm[PX_FX] = 0;
    prm[PX_DS_G] = 60; prm[PX_DS_T] = 70; prm[PX_DS_L] = 90;
    prm[PX_CHORUS] = 0; prm[PX_LATCH] = 0;
}

const char *const bm_px_chorus_names[3] = {"OFF", "I", "II"};

void bm_acid_fx_defaults(uint8_t *prm)
{
    prm[AC_FX] = 0;
    prm[AC_OD_G] = 60; prm[AC_OD_T] = 70; prm[AC_OD_L] = 90;
    prm[AC_LATCH] = 0;
    prm[AC_LFO_RATE] = 4; prm[AC_LFO_DEPTH] = 0; prm[AC_LFO_WAVE] = ACW_SINE; prm[AC_LFO_DEST] = ACL_CUTOFF;   /* 1 compás, apagado */
}

const uint16_t bm_acid_lfo_ticks[AC_LFO_RATES] = {24, 48, 96, 192, 384, 768, 1536, 3072};
const char *const bm_acid_lfo_rates[AC_LFO_RATES] = {"1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BARS", "4 BARS", "8 BARS"};
const char *const bm_acid_lfo_waves[ACW_N] = {"SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H", "PUMP", "GATE", "STEPS", "3-3-2", "BUILD"};
const char *const bm_acid_lfo_dests[ACL_DESTS] = {"CUTOFF", "PITCH", "RESO", "VOLUME"};

/* los patrones: 16 pasos por ciclo (con RATE = 1 BAR, uno por semicorchea) */
static const uint8_t LFO_GATE[16] = {1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1};      /* trance gate */
static const uint8_t LFO_STEPS[16] = {0, 96, 45, 127, 19, 76, 114, 38, 6, 102, 57, 127, 25, 83, 51, 121};   /* una línea de filtro */
static const uint8_t LFO_332[16] = {1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0};       /* tresillo: 3 + 3 + 2 */

float bm_acid_lfo_shape(int wave, float ph, uint32_t cyc)
{
    if (ph < 0.0f) ph = 0.0f;
    if (ph >= 1.0f) ph = 0.9999f;
    int st = (int)(ph * 16.0f);
    float in = ph * 16.0f - (float)st;                     /* dentro del paso, 0..1 */
    switch (wave) {
    case ACW_TRI: return ph < 0.5f ? 2.0f * ph : 2.0f - 2.0f * ph;
    case ACW_SAWUP: return ph;
    case ACW_SAWDN: return 1.0f - ph;
    case ACW_SQUARE: return ph < 0.5f ? 1.0f : 0.0f;
    case ACW_SH: {                                         /* un valor por ciclo, el mismo en cada vuelta del patrón */
        uint32_t h = (cyc + 1u) * 2654435761u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return (float)(h & 1023u) * (1.0f / 1023.0f);
    }
    case ACW_PUMP: {                                       /* cuatro bombeos por ciclo: abajo en el golpe, sube rápido */
        float q = ph * 4.0f;
        q -= (float)(int)q;
        float r = 1.0f - q;
        return 1.0f - r * r * r;
    }
    case ACW_GATE: return LFO_GATE[st] ? 1.0f : 0.0f;
    case ACW_STEPS: return (float)LFO_STEPS[st] * (1.0f / 127.0f);
    case ACW_332: {                                        /* cada golpe arriba y cae hasta el siguiente */
        int k = st;
        while (k > 0 && !LFO_332[k]) k--;
        float since = (float)(st - k) + in;
        return 1.0f / (1.0f + 1.5f * since);
    }
    case ACW_BUILD: return (float)st * (1.0f / 15.0f);     /* una escalera que sube: con 4 u 8 compases, una subida */
    default: return 0.5f - 0.5f * cosf(6.2831853f * ph);   /* SINE */
    }
}

bool bm_send_on(const bm_pattern *p, int t, int rev)
{
    switch (p->trk[t].engine) {
    case ENG_FM: return (p->prm[t][BX_FXON] >> (rev ? BXF_REVERB : BXF_DELAY)) & 1u;
    case ENG_ACID: return bm_acid_fx_on(p->prm[t], rev ? ACF_REVERB : ACF_DELAY);
    case ENG_MINI: case ENG_BUNO: case ENG_BITTAL: case ENG_BREAK: return !rev || bm_px_fx_on(p->prm[t], p->trk[t].engine, PXF_REVERB);   /* el DELAY, desde HOME */
    default: return true;
    }
}

void bm_bx7_fx_defaults(uint8_t *prm)
{
    for (int i = BX_FX_FIRST; i <= BX_LATCH; i++) prm[i] = 0;
    prm[BX_FXON] = (uint8_t)(1u << BXF_REVERB | 1u << BXF_DELAY);   /* los envíos de la mezcla, como siempre */
    for (int i = 0; i < BXF_N; i++) prm[BX_ORD0 + i] = (uint8_t)i;
    prm[BX_FLT_C] = 90; prm[BX_FLT_Q] = 30;
    prm[BX_DST_G] = 40; prm[BX_DST_T] = 80; prm[BX_DST_L] = 90;
    prm[BX_CHO_F] = 30; prm[BX_CHO_D] = 60; prm[BX_CHO_M] = 50;
    prm[BX_PHA_F] = 30; prm[BX_PHA_D] = 80; prm[BX_PHA_M] = 50;
}

static int bx_ord_ok(const uint8_t *prm)
{
    unsigned seen = 0;
    for (int i = 0; i < BXF_N; i++) { if (prm[BX_ORD0 + i] >= BXF_N) return 0; seen |= 1u << prm[BX_ORD0 + i]; }
    return seen == (1u << BXF_N) - 1u;
}

/* El preset: en BB-303 uno de bm_ac_presets; en BX-7 el patch (source) con los ajustes de fábrica
 * encima; en las demás un snapshot del banco de la máquina (core/bm_snap.h), sus ajustes y sus macros. */
void bm_preset_apply(bm_pattern *p, int t, int preset)
{
    int eng = p->trk[t].engine;
    if (!bm_engine_paged(eng)) return;
    if (eng == ENG_FM) {                       /* la voz del preset, para editarla; los efectos y MONO/POLY quedan */
        uint8_t keep[BX_LATCH + 1];
        memcpy(keep, p->prm[t], sizeof keep);
        bm_prm_defaults(p, t);
        if (bx_ord_ok(keep)) memcpy(p->prm[t] + BX_FX_FIRST, keep + BX_FX_FIRST, (size_t)(BX_LATCH + 1 - BX_FX_FIRST));
        memcpy(p->fmv, bm_fm_patch(preset), BM_FMV_BYTES);
        return;
    }
    if (eng == ENG_ACID) {                     /* el sonido del preset; los efectos y LATCH quedan */
        uint8_t keep[AC_NPRM];
        memcpy(keep, p->prm[t], sizeof keep);
        bm_prm_defaults(p, t);
        if (keep[AC_OD_G] || keep[AC_OD_T] || keep[AC_OD_L]) memcpy(p->prm[t] + AC_FX, keep + AC_FX, (size_t)(AC_NPRM - AC_FX));
        const bm_ac_preset *a = &bm_ac_presets[(preset % BM_AC_PRESETS + BM_AC_PRESETS) % BM_AC_PRESETS];
        bm_track *k = &p->trk[t];
        k->macro[0] = a->cut; k->macro[1] = a->res; k->macro[2] = a->env; k->macro[3] = a->dec;
        k->accent = a->acc; k->glide = a->glide;
        p->prm[t][AC_WAVE] = a->wave;
        return;
    }
    uint8_t keep[BM_PRM];                      /* B-MINI, BUNO-6, BITTAL: sus efectos y LATCH quedan */
    memcpy(keep, p->prm[t], sizeof keep);
    bm_prm_defaults(p, t);
    int n = bm_snap_count(eng);
    if (!n) return;
    const uint8_t *s = bm_snap_get(eng, (preset % n + n) % n);
    if (!s) return;
    memcpy(p->prm[t], s + 40, BM_PRM < BM_SNAP_PRM ? BM_PRM : BM_SNAP_PRM);   /* (los efectos, PX_*, no están: quedan) */
    memcpy(p->trk[t].macro, s + 36, 4);
    if (bm_panel_eng(eng)) {
        if (keep[PX_DS_G] || keep[PX_DS_T] || keep[PX_DS_L]) memcpy(p->prm[t] + PX_FX, keep + PX_FX, (size_t)(PX_END - PX_FX));
        else bm_px_fx_defaults(p->prm[t]);
    }
    if (eng == ENG_BITTAL) { p->prm[t][BT_TAB1] = 0; p->prm[t][BT_TAB2] = 0; }
}

void bm_prm_fmt(const bm_pdef *d, int v, char *b, int cap)
{
    float x;
    switch (d->fmt) {
    case PF_PCT: snprintf(b, (size_t)cap, "%d%%", (v * 100 + 63) / 127); break;
    case PF_BIP: snprintf(b, (size_t)cap, "%+d", v - 64); break;
    case PF_SEMI: snprintf(b, (size_t)cap, "%+d ST", (v - 64) * 24 / 63); break;
    case PF_DSEMI: snprintf(b, (size_t)cap, "%+d ST", ((v - 64) * 3) / 8); break;
    case PF_CENT: snprintf(b, (size_t)cap, "%+d CT", (v - 64) * 50 / 63); break;
    case PF_HZ:
        x = bm_cut_hz(v);
        if (x < 1000.0f) snprintf(b, (size_t)cap, "%d HZ", (int)x); else snprintf(b, (size_t)cap, "%.1f KHZ", (double)(x / 1000.0f));
        break;
    case PF_TIME:
        x = bm_env_s(v);
        if (x < 1.0f) snprintf(b, (size_t)cap, "%d MS", (int)(x * 1000.0f + 0.5f)); else snprintf(b, (size_t)cap, "%.1f S", (double)x);
        break;
    case PF_RATE: x = bm_lfo_hz(v); snprintf(b, (size_t)cap, x < 10.0f ? "%.2f HZ" : "%.1f HZ", (double)x); break;
    case PF_LIST: snprintf(b, (size_t)cap, "%s", d->list && v <= d->max ? d->list[v] : "?"); break;
    case PF_ONOFF: snprintf(b, (size_t)cap, "%s", v ? "YES" : "NO"); break;
    case PF_TRSP: snprintf(b, (size_t)cap, "%+d ST", v - 24); break;
    case PF_OHZ: {
        unsigned hz = (unsigned)(bm_acid_cut_hz(v) + 0.5f);
        if (hz < 1000) snprintf(b, (size_t)cap, "%u HZ", hz); else snprintf(b, (size_t)cap, "%u.%u KHZ", hz / 1000, hz % 1000 / 100);
        break;
    }
    case PF_DMS: {
        unsigned ms = (unsigned)(bm_acid_decay_ms(v) + 0.5f);
        if (ms < 1000) snprintf(b, (size_t)cap, "%u MS", ms); else snprintf(b, (size_t)cap, "%u.%u S", ms / 1000, ms % 1000 / 100);
        break;
    }
    case PF_GLIDE: snprintf(b, (size_t)cap, "%d MS", (v + 8) * 1000 / 254); break;
    case PF_MOD:
        if (v == 64) snprintf(b, (size_t)cap, "OFF");
        else { int pc = ((v < 64 ? 64 - v : v - 64) * 200 + 63) / 127; snprintf(b, (size_t)cap, v < 64 ? "OSC %d%%" : "FILT %d%%", pc > 100 ? 100 : pc); }
        break;
    case PF_ALG: if (v) snprintf(b, (size_t)cap, "%d", v); else snprintf(b, (size_t)cap, "PATCH"); break;
    case PF_OVR: if (v) snprintf(b, (size_t)cap, "%d", v - 1); else snprintf(b, (size_t)cap, "PATCH"); break;
    default: snprintf(b, (size_t)cap, "%d", v); break;
    }
}
