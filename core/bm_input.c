/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — entrada (1.0, etapa 1): HOME es el mezclador de 6 canales y todo su teclado, sus
 * botones y sus perillas son de la consola. PLAY/REC es HOME con el teclado tocando el canal del cursor.
 * La batería tiene su secuenciador (V_CH); las demás máquinas solo se asignan y se tocan. */
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "bm_mach.h"
#include "bm_util.h"
#include "bm_rom.h"
#include "bm_params.h"
#include "bm_drums.h"
#include "bm_snap.h"

static const char *const RAT_NAMES[5] = {"X1", "X2", "X3", "X4", "FLAM"};

#define SCENE_B0 7                 /* las escenas A-D: negras 8-11 (la 7 sin función) */
enum { ROLE_NONE, ROLE_MUTE, ROLE_PAT, ROLE_PATUSED, ROLE_SCENE, ROLE_SCNDONE, ROLE_CHAN, ROLE_SOLOADD, ROLE_PAD, ROLE_LREP };

static bool console(const bm_app *a) { return a->view == V_HOME || a->view == V_GLO; }

/* La batería (T1) es la única máquina con secuenciador propio */
static bool drums_track(const bm_app *a, int t) { return bm_is_tr(&a->pat, t); }

/* ------------------------------------------------------------ formato */
static void fmt_pct(int v, char *b, int cap) { snprintf(b, (size_t)cap, "%d%%", v * 100 / 127); }

static int tr_lane(const bm_app *a) { return a->sel_lane[a->track] & 15; }
static bool seq_page(const bm_app *a) { return a->view == V_CH; }

/* Perillas: `d` llega acelerado (para 0..127 y el BPM); las listas, los largos y los pasos usan los
 * detents tal cual (lst): uno por detent, ninguno se pierde ni se salta. Fuera de un giro (o si el
 * signo no coincide) vale ±1. */
static int lst(const bm_app *a, int d)
{
    int r = a->enc_raw;
    if (!r || (r > 0) != (d > 0)) return d > 0 ? 1 : -1;
    return r;
}

static int wrap(int v, int n) { return n > 0 ? ((v % n) + n) % n : 0; }
int app_lst(const bm_app *a, int d) { return lst(a, d); }

static int syn_step_events(const bm_app *a, int t, int s, int idx[4])
{
    int n = 0;
    for (int k = 0; k < BM_EVENTS; k++)
        if (!bm_event_empty(bm_evc(&a->pat, t, s, k))) idx[n++] = k;
    return n;
}

/* ------------------------------------------------------------ leyenda de las perillas */
static void L4(bm_legend *L, int i, const char *name, const char *val, int pct);
void app_layer_legend(const bm_app *a, bm_legend *L, int k, const char *name, const char *val, int pct) { (void)a; L4(L, k, name, val, pct); }

static void L4(bm_legend *L, int i, const char *name, const char *val, int pct)
{
    snprintf(L[i].name, 10, "%s", name);
    snprintf(L[i].val, 10, "%s", val);
    L[i].pct = (uint8_t)bm_clampi(pct, 0, 255);
}

/* HOME: el canal del cursor como una tira de mezclador (K1-K4 = VOLUME, FILTER, DELAY, REVERB) */
static void legend_home(const bm_app *a, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[a->track];
    char b[12];
    int p = !tr->djf || tr->djf == 64 ? 0 : tr->djf < 64 ? -(int)tr->djf : (int)tr->djf - 64;
    fmt_pct(tr->level, b, sizeof b); L4(L, 0, "VOLUME", b, tr->level * 2);
    if (!p) snprintf(b, sizeof b, "OPEN"); else snprintf(b, sizeof b, "%s %d", p < 0 ? "LP" : "HP", p < 0 ? -p : p);
    L4(L, 1, "FILTER", b, (p + 64) * 2);
    fmt_pct(tr->send, b, sizeof b); L4(L, 2, "DELAY", b, tr->send * 2);
    fmt_pct(tr->rsend, b, sizeof b); L4(L, 3, "REVERB", b, tr->rsend * 2);
}

static int lane_val(const bm_app *a, int slot)
{
    return ((const uint8_t *)&bm_trk_trc(&a->pat, a->track)->lane[tr_lane(a)])[slot & 7];
}

/* K1-K4 de las capas de sonido: página 0 = el golpe (TUNE DECAY TONE COLOR), 1 = el instrumento (MODEL LEVEL CHOKE) */
static void legend_lane(const bm_app *a, bm_legend *L, int page)
{
    const bm_ppage *P = &bm_lane_pages[page];
    char b[12];
    for (int k = 0; k < (page == 1 ? 3 : P->n); k++) {
        const bm_pdef *d = &P->p[k];
        int v = lane_val(a, d->slot);
        bm_prm_fmt(d, v, b, sizeof b);
        const char *nm = d->slot == LN_COLOR ? bm_drm_color[lane_val(a, LN_MODEL) % DRM_COUNT] : d->name;
        L4(L, k, nm, b, v * 255 / (d->max ? d->max : 1));
    }
}

static void legend_view(const bm_app *a, bm_legend *L);
static void legend_glo(const bm_app *a, bm_legend *L);

void bm_app_legend(const bm_app *a, bm_legend L[4])
{
    memset(L, 0, sizeof(bm_legend) * 4);
    const bm_track *tr = &a->pat.trk[a->track];
    char b[12];
    switch (a->layer) {
    case L_ACC: fmt_pct(tr->accent, b, sizeof b); L4(L, 0, "ACCENT", b, tr->accent * 2); return;
    case L_HIT: legend_lane(a, L, 0); return;
    case L_INST: legend_lane(a, L, 1); return;
    case L_REP: case L_MUTE: return;
    case L_LEN: {                                              /* Slicer: el largo del pad elegido y la velocidad del canal */
        int len = bm_trk_len(&a->pat, a->track, tr_lane(a));
        snprintf(b, sizeof b, len > 1 ? "%d STEPS" : "%d STEP", len); L4(L, 0, "LENGTH", b, (len - 1) * 255 / (BM_STEPS - 1));
        L4(L, 1, "SPEED", bm_speed_names[tr->speed % SPD_COUNT], (SPD_COUNT - 1 - tr->speed % SPD_COUNT) * 51);
        return;
    }
    default: break;
    }
    legend_view(a, L);
}

static void legend_view(const bm_app *a, bm_legend *L)
{
    char b[12];
    switch (a->view) {
    case V_HOME: legend_home(a, L); return;
    case V_MACH: mach_legend_view(a, L); return;
    case V_GLO: legend_glo(a, L); return;
    case V_SAVE: snprintf(b, sizeof b, "A%02d", a->save_target + 1); L4(L, 0, "TARGET", b, a->save_target * 17); return;
    case V_SYSTEM: L4(L, 0, "VALUE", "ALGO", 0); return;
    case V_CH:
        if (a->held_step >= 0 && a->held_long) {               /* paso sostenido: K1 acento, K2 repetición */
            uint8_t v = bm_trk_trc(&a->pat, a->track)->st[tr_lane(a)][a->held_step];
            int rat = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
            L4(L, 0, "ACCENT", (v & ST_ACC) ? "ON" : "OFF", (v & ST_ACC) ? 255 : 0);
            L4(L, 1, "REPEAT", RAT_NAMES[rat], rat * 63);
            return;
        }
        if (bm_is_pads(a->track)) { slicer_grid_legend(a, L); return; }   /* LEVEL LENGTH PITCH RELEASE del pad */
        legend_lane(a, L, 2);
        return;
    default: return;
    }
}

/* ------------------------------------------------------------ las tres perillas grandes
 * El mismo reparto que enc_dispatch, en palabras cortas para la fila de abajo de la pantalla. */
void bm_app_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre)
{
    const char *S = "-", *A = "-", *P = "-";
    int t = a->track;
    if (a->layer) { if (a->layer == L_INST) A = "MODEL"; else if (a->layer == L_LEN) A = "PAD"; goto out; }   /* las capas usan K1-K4 */
    switch (a->view) {
    case V_HOME: S = "TRACK"; A = "BPM"; P = drums_track(a, t) ? "KIT" : bm_is_pads(t) ? "TAKE" : "SOUND"; break;
    case V_GLO:                                            /* las páginas, con GLO */
        if (a->glo_page == 0) { S = "TRACK"; A = app_fixed(t) ? "-" : "MACHINE"; }
        else if (a->glo_page == 1) A = "BPM";
        break;
    case V_SYSTEM: S = "ROW"; A = "VALUE"; break;
    case V_SAVE: S = "ROW"; A = "TARGET"; P = "TARGET"; break;
    case V_CH:
        if (bm_is_pads(t)) { A = "PAD"; P = "TAKE"; } else { A = "INSTR"; P = "KIT"; }
        break;
    case V_MACH: mach_bigknobs(a, &S, &A, &P); break;
    default: break;
    }
out:
    *sel = S; *alg = A; *pre = P;
}

/* ------------------------------------------------------------ mapa del teclado (capas, HOME)
 * Las luces son blancas y solo dicen el estado; la tira de teclas de la pantalla nombra cada tecla
 * con la misma intensidad. Una sola gramática en todas las pantallas:
 *   OFF    no hay nada: sin función, hueco vacío que no hace nada, fuera del largo
 *   DIM    disponible pero quieto o vacío, o con contenido pero callado (muteado)
 *   MID    tiene contenido y va a sonar; una referencia (carril con pasos, patrón guardado, tónica)
 *   ON     elegido, sostenido, activo ahora, o el destello de un golpe (live)
 *   BLINK  solo el cabezal o lo que está en cola
 *   BREATH armado, fijo o especial (paso con p-lock, la salida de una capa fija) */
static bm_keyfn *KW(bm_keyfn *K, int w, const char *l, uint8_t led, uint16_t c)
{
    bm_keyfn *k = &K[bm_white_key(w)];
    snprintf(k->label, sizeof k->label, "%s", l); k->led = led; k->color = c;
    return k;
}

static bm_keyfn *KB(bm_keyfn *K, int b, const char *l, uint8_t led, uint16_t c)
{
    bm_keyfn *k = &K[bm_black_key(b)];
    snprintf(k->label, sizeof k->label, "%s", l); k->led = led; k->color = c;
    return k;
}

static void KL(bm_keyfn *K, int k, const char *l) { snprintf(K[k].label, sizeof K[k].label, "%s", l); }

/* La tira muestra 3 letras: cada instrumento de los kits tiene su nombre corto fijo (KICK y KICK2,
 * o los tres TOM, no pueden leerse iguales). Si se le cambió el modelo, el del modelo. */
static const char KIT_SHORT[BM_KITS_N][BM_LANES][4] = {
    {"BD", "BD2", "CH", "OH", "SD", "CP", "RS", "LT", "MT", "HT", "CB", "CY", "SH", "CL", "CG", "ZAP"},
    {"BD", "BD2", "CH", "OH", "SD", "CP", "RS", "LT", "MT", "HT", "RD", "CR", "SH", "CL", "CG", "ZAP"},
    {"BD", "SUB", "CH", "OH", "SD", "CP", "BLP", "ZAP", "MTL", "BLK", "CB", "NZ", "SH", "TOM", "PRC", "BD3"},
};
static const char DRM_SHORT[DRM_COUNT][4] = {"BD", "BD", "SD", "SD", "HH", "MTL", "CP", "TOM", "CB", "RES", "NZ", "FM"};

static const char *lane_short(const bm_app *a, int t, int ln)
{
    ln &= 15;
    if (!bm_is_tr(&a->pat, t)) return bm_lane_name(a, t, ln);          /* pads P1..P16 */
    int kit = (a->pat.trk[t].source % BM_KITS_N + BM_KITS_N) % BM_KITS_N;
    int m = bm_trk_trc(&a->pat, t)->lane[ln].model;
    const char *s = (m == bm_kits[kit].lane[ln].model || m >= DRM_COUNT) ? KIT_SHORT[kit][ln] : DRM_SHORT[m];
    return s[0] ? s : bm_lane_name(a, t, ln);
}

/* el carril tiene algún golpe dentro de su largo */
static bool lane_has(const bm_tr *tr, int ln)
{
    int len = tr->lane[ln].len ? tr->lane[ln].len : 16;
    for (int s = 0; s < len && s < BM_STEPS; s++)
        if (tr->st[ln][s] & ST_ON) return true;
    return false;
}

/* un instrumento en un selector: el elegido ON, con pasos MID (muteado DIM), vacío OFF */
static uint8_t lane_led(const bm_tr *tr, int ln, int sel)
{
    if (ln == sel) return LED_ON;
    if (ln < 0 || ln >= BM_LANES || !lane_has(tr, ln)) return LED_OFF;
    return ((tr->mute >> ln) & 1) ? LED_DIM : LED_MID;
}

/* el instrumento acaba de sonar (los muteados no suenan, así que no destellan) */
static uint8_t lane_live(const bm_app *a, int t, int ln)
{
    ln &= 15;
    return (uint8_t)(a->now < a->lane_flash[t][ln] && !((bm_trk_trc(&a->pat, t)->mute >> ln) & 1));
}

/* Páginas de pasos del secuenciador (los compases de 16 pasos del instrumento elegido) */
static int step_pages(const bm_app *a)
{
    int pages = (bm_trk_len(&a->pat, a->track, tr_lane(a)) + 15) / 16;
    return pages > 0 ? pages : 1;
}

/* negras 9 y 10 = el compás anterior / siguiente (en círculo). Se ven siempre (pedido del usuario,
 * 2026-10-20): con un solo compás quedan tenues y al tocarlas lo dicen. */
static void page_blacks(const bm_app *a, bm_keyfn *K)
{
    uint8_t led = step_pages(a) < 2 ? LED_DIM : LED_MID;
    KB(K, 8, "<", led, C_WHITE);
    KB(K, 9, ">", led, C_WHITE);
}

static int notes_empty(const bm_app *a, int t)
{
    for (int s = 0; s < BM_STEPS; s++)
        for (int k = 0; k < BM_EVENTS; k++)
            if (!bm_event_empty(bm_evc(&a->pat, t, s, k))) return 0;
    return 1;
}

/* el canal tiene algo que suena (golpes o notas) */
static bool track_any(const bm_app *a, int t)
{
    if (app_kind(a, t) == KIND_NOTE) return !notes_empty(a, t);
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    for (int l = 0; l < BM_LANES; l++) if (lane_has(tr, l)) return true;
    return false;
}

/* El mezclador (HOME y su GLO):
 *   blancas 1-9   patrones 01-09: ON el actual, BLINK en cola (entra al final del compás), MID guardado, apagado vacío
 *   blanca 10     sin función: espacio entre los patrones y los canales (no se cambia de patrón por error)
 *   blancas 11-16 canales T1-T6: ON el cursor, MID con pasos (destella con cada golpe), DIM vacío
 *   negras 1-6    mutes: MID suena (destella), DIM muteado, ON solo
 *   negra 7       sin función
 *   negras 8-11   escenas A-D: ON la activa, BLINK en cola, MID guardada, DIM vacía (con un patrón sostenido: guardar) */
static void map_console(const bm_app *a, bm_keyfn *K)
{
    const bm_status *st = &a->ctx.st;
    char b[6];
    for (int w = 0; w < BM_PATTERNS; w++) {
        bool has = bm_store_has(&a->store, (uint8_t)w) || w == st->pattern || a->pend_obj == w;
        bool q = w == st->queued && st->queued != st->pattern;
        snprintf(b, sizeof b, "%02d", w + 1);
        KW(K, w, b, w == st->pattern ? LED_ON : q ? LED_BLINK : has ? LED_MID : LED_OFF, w == st->pattern ? C_WHITE : q ? C_CYAN : 0);
    }
    for (int n = 0; n < BM_TRACKS; n++) {
        int t = BM_ORDER[n];
        bool any = track_any(a, t);
        snprintf(b, sizeof b, "T%d", n + 1);
        KW(K, BM_CH_W0 + n, b, t == a->track ? LED_ON : any ? LED_MID : LED_DIM, bm_track_color(t))->live =
            (uint8_t)(any && !a->pat.trk[t].mute && a->now < a->trig_until[t]);
    }
    for (int n = 0; n < BM_TRACKS; n++) {
        int t = BM_ORDER[n];
        bool mute = a->pat.trk[t].mute != 0;
        uint8_t led = a->solo_on && a->solo_track == t ? LED_ON : mute ? LED_DIM : LED_MID;
        snprintf(b, sizeof b, "M%d", n + 1);
        KB(K, n, b, led, bm_track_color(t))->live = (uint8_t)(!mute && led == LED_MID && a->now < a->trig_until[t]);
    }
    for (int s = 0; s < BM_SCENES; s++) {
        snprintf(b, sizeof b, a->pat_held ? "S%c" : "%c", 'A' + s);
        KB(K, SCENE_B0 + s, b, st->scene == s ? LED_ON : st->scene_q == s ? LED_BLINK : (a->song.scene[st->pattern % BM_PATTERNS][s].flags & SCN_STORED) ? LED_MID : LED_DIM, C_CYAN);
    }
}

/* ------------------------------------------------------------ notas y teclado de PLAY/REC */
/* las máquinas de bajo (BB-303 y B-MINI) tocan una octava más abajo, en el canal que estén */
static int bass_machine(const bm_app *a, int t) { int e = app_engine(a, t); return e == ENG_ACID || e == ENG_MINI; }

/* la nota de una tecla en PLAY/REC: cromática desde F, con la octava corrida por OCT -/+ */
static int key_note(const bm_app *a, int t, int k)
{
    return BM_KEY_BASE_NOTE + k + 12 * a->oct - (bass_machine(a, t) ? 12 : 0);
}

/* las notas que el canal t acaba de tocar desde sus pasos (para que destellen sus teclas) */
static int live_notes(const bm_app *a, int t, uint8_t out[4])
{
    if (!(a->now < a->trig_until[t])) return 0;
    int s = bm_app_track_step(a, t, -1), idx[4];
    if (s < 0) return 0;
    int n = syn_step_events(a, t, s, idx);
    for (int i = 0; i < n; i++) out[i] = bm_evc(&a->pat, t, s, idx[i])->note;
    return n;
}

static uint8_t note_in(int note, const uint8_t *nn, int n)
{
    for (int i = 0; i < n; i++) if (nn[i] == note) return 1;
    return 0;
}

/* El teclado de un canal de notas en PLAY/REC: cromático (la escala DIM, la tónica MID, fuera de la
 * escala OFF) */
static void note_keys(const bm_app *a, bm_keyfn *K, int t);
void app_note_keys(const bm_app *a, bm_keyfn *K, int t) { note_keys(a, K, t); }

static void note_keys(const bm_app *a, bm_keyfn *K, int t)
{
    int root, sc = app_scale(a, t, &root), sl = bm_scale_len(sc);
    uint8_t nn[4];
    int nl = live_notes(a, t, nn);
    uint16_t col = bm_track_color(t);
    uint16_t pcs = 0;
    for (int d = 0; d < sl; d++) pcs |= (uint16_t)(1u << (bm_scale_note(root, sc, d, 0) % 12));
    for (int k = 0; k < BM_KEYS; k++) {
        int note = key_note(a, t, k), pc = (note + 120) % 12;
        KL(K, k, bm_note_names[pc]);
        K[k].led = pc == root ? LED_MID : ((pcs >> pc) & 1) ? LED_DIM : LED_OFF;
        K[k].color = col;
        K[k].live = note_in(note, nn, nl);
    }
}

/* Las blancas de un canal de batería o de pads en PLAY/REC: los 16 instrumentos, como un selector */
static void lane_whites(const bm_app *a, bm_keyfn *K, int t)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    for (int w = 0; w < 16; w++)
        KW(K, w, lane_short(a, t, w), lane_led(tr, w, tr_lane(a)), bm_track_color(t))->live = (uint8_t)(lane_has(tr, w) && lane_live(a, t, w));
}

/* PLAY/REC: las teclas tocan la máquina del cursor. Notas: las 27 cromáticas desde F. Batería y
 * sampler: las blancas son los 16 instrumentos o pads. */
static void map_sloop(const bm_app *a, bm_keyfn *K)
{
    int t = a->track;
    if (app_kind(a, t) == KIND_NOTE) note_keys(a, K, t); else lane_whites(a, K, t);
}

/* mode 0: acento · 1: repetir · 2: borrar. En acento y repetir los pasos con golpe quedan MID y los
 * que tienen la marca ON (pedido del usuario 2026-10-21: antes los golpes sin marca iban apagados y
 * no se veía dónde había sonido); vacíos apagados, sin el paso que suena. */
static void map_steps(const bm_app *a, bm_keyfn *K, int mode)
{
    int t = a->track;
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = tr_lane(a);
    for (int w = 0; w < 16; w++) {
        int s = a->step_page * 16 + w;
        const char *l = "";
        uint8_t led = LED_OFF;
        if (s >= tr->lane[lane].len) { KW(K, w, "", LED_OFF, 0); continue; }
        uint8_t v = tr->st[lane][s];
        if (v & ST_ON) {
            int rat = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
            if (mode == 0) { l = (v & ST_ACC) ? "A" : "-"; led = (v & ST_ACC) ? LED_ON : LED_MID; }
            else if (mode == 1) { l = rat ? RAT_NAMES[rat] : "-"; led = rat ? LED_ON : LED_MID; }
            else { l = "X"; led = LED_MID; }
        }
        if (mode == 2 && s == bm_app_track_step(a, t, lane)) led = LED_BLINK;
        KW(K, w, l, led, mode == 0 ? C_WHITE : mode == 1 ? C_YELLOW : C_RED);
    }
}

/* Las blancas como los 16 instrumentos: con pasos MID, vacío OFF; en MUTE, el muteado DIM (y destella al sonar) */
static void map_lanes(const bm_app *a, bm_keyfn *K, int t, int mute_mode)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    for (int w = 0; w < 16; w++) {
        bool has = lane_has(tr, w), mute = (tr->mute >> w) & 1;
        if (mute_mode) {                                         /* MUTE: fijo; suena = fuerte, muteado = tenue (aunque no tenga golpes) */
            KW(K, w, lane_short(a, t, w), mute ? LED_DIM : has ? LED_ON : LED_OFF, bm_track_color(t));
            continue;
        }
        uint8_t led = has ? LED_MID : LED_OFF;
        if (w == tr_lane(a)) led = LED_ON;
        KW(K, w, lane_short(a, t, w), led, bm_track_color(t))->live = (uint8_t)(has && lane_live(a, t, w));
    }
}

/* Navegación: las negras 1-8 son los instrumentos de la página de sonidos (la elegida ON) y 9/10 los compases */
static void nav_blacks(const bm_app *a, bm_keyfn *K)
{
    int t = a->track, lane = tr_lane(a), bank = (lane / 8) * 8;
    const bm_tr *tg = bm_trk_trc(&a->pat, t);
    for (int i = 0; i < 8; i++)
        KB(K, i, lane_short(a, t, bank + i), lane_led(tg, bank + i, lane), bm_track_color(t))->live = (uint8_t)(lane_has(tg, bank + i) && lane_live(a, t, bank + i));
    page_blacks(a, K);
}

/* LFO, ENV, EDIT y SCL de la batería: las negras 1-8 son acceso rápido a los instrumentos de la
 * página de sonidos, sin sonar (en SCL mutean, como su blanca); 10 y 11 = el compás anterior /
 * siguiente; la 9 nada. */
static void layer_blacks(const bm_app *a, bm_keyfn *K)
{
    int t = a->track, lane = tr_lane(a), bank = (lane / 8) * 8;
    const bm_tr *tg = bm_trk_trc(&a->pat, t);
    for (int i = 0; i < 8; i++) {
        int l = bank + i;
        if (a->layer == L_MUTE) {
            bool has = lane_has(tg, l), mute = (tg->mute >> l) & 1;
            KB(K, i, lane_short(a, t, l), mute ? LED_DIM : has ? LED_ON : LED_OFF, bm_track_color(t));
        } else KB(K, i, lane_short(a, t, l), lane_led(tg, l, lane), bm_track_color(t))->live = (uint8_t)(lane_has(tg, l) && lane_live(a, t, l));
    }
    uint8_t led = step_pages(a) < 2 ? LED_DIM : LED_MID;
    KB(K, 9, "<", led, C_WHITE);
    KB(K, 10, ">", led, C_WHITE);
}

/* Las pistas del visor de cada capa: la línea 1 dice qué hacen teclas y perillas, la 2 empieza
 * siempre por cómo se sale. CONTROL = VERBO OBJETO, dos espacios entre pares, hasta 38 letras, los
 * controles con el nombre impreso en el equipo. */
void bm_app_keymap(const bm_app *a, bm_keyfn K[BM_KEYS], char *title, int cap, uint16_t *color, const char **h1, const char **h2)
{
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    int t = a->track;
    *h1 = ""; *h2 = "";
    *color = C_WHITE;
    title[0] = 0;
    switch (a->layer) {
    case L_ACC:
        snprintf(title, (size_t)cap, "ACCENTS");
        *h1 = "WHITE = TOGGLE ACCENT  K1 = AMOUNT";
        *h2 = "EXIT: LFO OR HOME  BLACK = SOUND, BAR";
        map_steps(a, K, 0);
        layer_blacks(a, K);
        return;
    case L_REP:
        snprintf(title, (size_t)cap, "REPEAT");
        *color = C_YELLOW;
        *h1 = "WHITE = CYCLE X1-X4 AND FLAM";
        *h2 = a->layer_latch ? "EXIT: BLACK 11 OR HOME" : "EXIT: RELEASE BLACK 11";
        map_steps(a, K, 1);
        nav_blacks(a, K);
        KB(K, 10, "RPT", LED_BREATH, C_YELLOW);
        return;
    case L_HIT:
        snprintf(title, (size_t)cap, "SOUND");
        *color = bm_track_color(t);
        *h1 = "WHITE = PICK AND HEAR  K1-K4 = THE HIT";
        *h2 = "EXIT: ENV OR HOME  BLACK = SOUND, BAR";
        map_lanes(a, K, t, 0);
        layer_blacks(a, K);
        return;
    case L_INST:
        snprintf(title, (size_t)cap, "INSTRUMENT");
        *color = bm_track_color(t);
        *h1 = "ALGORITHM = THIS DRUM'S SOUND  K1-K3";
        *h2 = "EXIT: EDIT OR HOME  WHITE = PICK";
        map_lanes(a, K, t, 0);
        layer_blacks(a, K);
        return;
    case L_LEN: {                                              /* Slicer, SEQ otra vez (como la capa de antes) */
        snprintf(title, (size_t)cap, "LENGTH/SPEED");
        *color = bm_track_color(t);
        char b[6];
        int len = bm_trk_len(&a->pat, t, tr_lane(a));
        for (int w = 0; w < 16; w++) {                         /* blancas: el largo del pad elegido */
            int v = a->len_page * 16 + w + 1;
            snprintf(b, sizeof b, "%d", v);
            KW(K, w, b, v == len ? LED_ON : (v < len ? LED_MID : LED_DIM), bm_track_color(t));
        }
        static const char *const SP[SPD_COUNT] = {"X2", "3/2", "X1", "3/4", "1/2", "1/4"};
        for (int s = 0; s < SPD_COUNT; s++) KB(K, s, SP[s], a->pat.trk[t].speed == s ? LED_ON : LED_DIM, C_WHITE);   /* negras 1-6: la velocidad */
        int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
        for (int c = 1; c <= 4; c++) {                         /* negras 7-10: 1 a 4 compases */
            snprintf(b, sizeof b, "%dBR", c);
            KB(K, 5 + c, b, c * spb > BM_STEPS ? LED_OFF : len == c * spb ? LED_ON : LED_DIM, C_YELLOW);
        }
        KB(K, 10, "+16", a->len_page ? LED_ON : LED_MID, C_WHITE);   /* 1-16 > 17-32 > 33-48 > 49-64 */
        return;
    }
    case L_MUTE:
        snprintf(title, (size_t)cap, "MUTE SOUNDS");
        *color = C_RED;
        *h1 = "WHITE = MUTE OR PLAY THAT SOUND";
        *h2 = "EXIT: SCL OR HOME  BLACK = SOUND, BAR";
        map_lanes(a, K, t, 1);
        layer_blacks(a, K);
        return;
    default: break;
    }
}

/* ------------------------------------------------------------ grabación en vivo */
static void rec_syn(bm_app *a, int k, int t, const uint8_t *notes, int n) { app_rec_at(a, k, t, notes, n, a->ctx.st.rtick); }

void app_rec_at(bm_app *a, int k, int t, const uint8_t *notes, int n, uint32_t rt)
{
    int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    int len = a->pat.trk[t].length ? a->pat.trk[t].length : 16;
    uint32_t q = (rt + (uint32_t)tps / 2) / (uint32_t)tps;
    int master = bm_master_steps(&a->pat.meta) * BM_TICKS_STEP;
    if (master) q %= ((uint32_t)master + (uint32_t)tps - 1u) / (uint32_t)tps;   /* pasado el final del maestro: el 0 */
    int s = (int)(q % (uint32_t)len);
    int micro = bm_clampi((int)(rt - q * (uint32_t)tps), -11, 11);   /* lo que se adelantó o se atrasó */
    if (micro > -3 && micro < 3) micro = 0;                    /* lo que está casi en el paso, en el paso */
    micro = micro * (a->pat.trk[t].qfree & 3) / 4;               /* QUANT (GLO): 100 % = justo en el paso */
    a->key_rec_step[k] = (int8_t)s;
    uint32_t bt = a->ctx.st.playing ? a->ctx.st.btick : 0;      /* REC armado: todavía no arrancó, empieza en 0 */
    a->key_rec_tick[k] = rt;
    a->key_rec_bt[k] = bt;
    a->key_rec_slot[k] = -1;
    a->rec_steps |= 1ull << s;                                   /* esta pasada: se ve en rojo */
    uint8_t busy = 0;                                            /* las casillas de las teclas que siguen apretadas */
    for (int k2 = 0; k2 < BM_KEYS; k2++)
        if (k2 != k && a->keys_down[k2] && a->key_trk[k2] == t && a->key_rec_step[k2] >= 0 && a->key_rec_slot[k2] >= 0)
            busy |= (uint8_t)(1u << a->key_rec_slot[k2]);
    /* Reescribe: lo que había en el paso se borra la primera vez que lo tocas en esta vuelta; lo que
     * tocas otra vez en la misma vuelta (un acorde) se suma. Lo que no tocas sigue como estaba. */
    uint8_t gone = 0;
    uint32_t loop = (uint32_t)len * (uint32_t)tps;
    uint32_t same = loop > (uint32_t)tps ? loop - (uint32_t)tps : (uint32_t)tps / 2u;
    if (a->rec_clr_trk != t) { a->rec_clr_on = 0; a->rec_clr_trk = (int8_t)t; }
    if (!((a->rec_clr_on >> s) & 1u) || bt - a->rec_clr_bt[s] >= same) {
        bm_event z;
        bm_event_clear(&z);
        for (int e = 0; e < BM_EVENTS; e++)
            if (!((busy >> e) & 1u) && !bm_event_empty(bm_evc(&a->pat, t, s, e))) { app_push_ev(a, t, s, e, &z); gone |= (uint8_t)(1u << e); }
    }
    a->rec_clr_bt[s] = bt; a->rec_clr_on |= 1ull << s;
    for (int i = 0; i < n; i++) {
        int slot = -1;
        for (int e = 0; e < BM_EVENTS; e++) {
            const bm_event *ev = bm_evc(&a->pat, t, s, e);
            bool empty = ((gone >> e) & 1u) || bm_event_empty(ev);   /* (lo borrado recién: la cola todavía no lo aplicó) */
            if (!empty && ev->note == notes[i]) { slot = e; break; }
            if (slot < 0 && empty && !((busy >> e) & 1u)) slot = e;   /* (su gate cortaría a la otra) */
        }
        if (slot >= 0) gone &= (uint8_t)~(1u << slot);
        if (slot < 0) continue;
        bm_event ev;
        bm_event_clear(&ev);
        ev.note = notes[i]; ev.len = 1; ev.micro = (int8_t)micro;
        if (a->rec_vel >= 127) ev.vel = BM_VEL_ACC;                 /* BB-303: LFO sostenido */
        app_push_ev(a, t, s, slot, &ev);
        if (a->key_rec_slot[k] < 0) a->key_rec_slot[k] = (int8_t)slot;
    }
    app_edited(a);
}

static int quant_step(const bm_app *a, int t, int len)
{
    int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    return (int)(((a->ctx.st.rtick + (uint32_t)tps / 2) / (uint32_t)tps) % (uint32_t)(len ? len : 16));
}

/* un golpe grabado: sobrescribe el paso (se suma a lo que ya estaba) */
static void rec_tr(bm_app *a, int t, int lane)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int s = quant_step(a, t, tr->lane[lane].len);
    uint8_t v = (uint8_t)(tr->st[lane][s] | ST_ON);
    if (bm_gesture_is_down(&a->ges, BTN_OCT_UP)) v |= ST_ACC;                 /* OCT+ sostenido: golpe fuerte = acento */
    else if (bm_gesture_is_down(&a->ges, BTN_OCT_DN)) v &= (uint8_t)~ST_ACC;  /* OCT- sostenido: suave */
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, v);
    a->rec_steps |= 1ull << s; a->rec_lanes |= (uint16_t)(1u << (lane & 15));
    app_edited(a);
}

static void play_notes(bm_app *a, int k, int t, const uint8_t *notes, int n, int vel);
void app_play_notes(bm_app *a, int k, int t, const uint8_t *notes, int n, int vel) { play_notes(a, k, t, notes, n, vel); }

static void play_notes(bm_app *a, int k, int t, const uint8_t *notes, int n, int vel)
{
    for (int i = 0; i < n; i++) app_push(a, CMD_NOTE_ON, (uint8_t)t, notes[i], (uint8_t)vel);
    a->rec_vel = (uint8_t)vel;
    memcpy(a->key_notes[k], notes, (size_t)n);
    a->key_nn[k] = (uint8_t)n;
    a->key_trk[k] = (uint8_t)t;
    if (n && app_kind(a, t) == KIND_NOTE) a->last_note[t] = notes[0];
    a->key_rec_step[k] = -1;
    if (a->rec && app_playing(a) && !seq_page(a)) {
        if (bm_is_grid(&a->pat, t)) rec_tr(a, t, notes[0] & 15);
        else if (a->ctx.st.loop_mode[t - 2] == LOOP_OFF && !a->pat.trk[t].arp_mode) rec_syn(a, k, t, notes, n);   /* con ARP graba el arpegiador */
    }
}

/* ------------------------------------------------------------ REC y PLAY/REC
 * Como SLOOP: se graba sobre el largo que tiene el canal; al llegar al final da la vuelta y lo que
 * tocas reescribe sus pasos. Grabar nunca alarga el canal. */
/* REC: lo que tocas reescribe los pasos que tocas del canal del cursor */
static void rec_set(bm_app *a, int on);
void app_rec_set(bm_app *a, int on) { rec_set(a, on); }

static void rec_set(bm_app *a, int on)
{
    a->rec = (uint8_t)on;
    a->rec_clr_on = 0;
    app_push(a, CMD_REC, a->rec, a->track, 0);
    if (a->rec) { a->rec_steps = 0; a->rec_lanes = 0; }
    app_toast(a, a->rec ? "REC ON" : "REC OFF");
}

/* REC parado en el SEQ de una máquina: paso a paso (las teclas escriben en el paso del cursor). Con
 * PLAY sigue grabando en vivo. */
void app_step_rec(bm_app *a)
{
    a->rec = 1;
    app_push(a, CMD_REC, 1, a->track, 0);
    a->rec_steps = 0; a->rec_lanes = 0;
    app_toast(a, "STEP REC");
}

/* PLAY/REC: HOME en la misma pantalla, pero el teclado deja de ser del mezclador y toca (y graba) la
 * máquina del cursor. Se entra con la blanca del canal del cursor (o con REC, ya grabando); se sale
 * con HOME. */
void app_sloop(bm_app *a, int on)
{
    if (!on == !a->sloop) return;
    a->sloop = (uint8_t)(on ? 1 : 0);
    if (!on && a->rec) rec_set(a, 0);
}

/* PLAY/REC: SELECT cambió de canal: la grabación (y el arpegiador) pasa al nuevo, con el rojo de
 * "esta pasada". */
static void sloop_follow(bm_app *a)
{
    if (!a->rec) return;
    app_push(a, CMD_REC, a->rec, a->track, 0);                     /* el arpegiador graba en el canal nuevo */
    a->rec_steps = 0; a->rec_lanes = 0;
}

/* ------------------------------------------------------------ mutes, escenas y canales */
static void toggle_mute(bm_app *a, int t)
{
    char b[24];
    bool muted = !a->pat.trk[t].mute;                         /* el patrón cambia cuando el audio lo aplica */
    app_set_track(a, t, TRK_OFF(mute), muted ? 1 : 0);
    snprintf(b, sizeof b, "%s %s", bm_track_name(a, t), T(muted ? "MUTED" : "ON"));
    app_toast(a, b);
}

/* Escenas: cada patrón tiene 4, una foto de los mutes. Guardar: la blanca de un patrón sostenida + la
 * negra de la escena (se guarda en ese patrón). Una escena guardada del patrón que suena, con un toque,
 * entra en el próximo compás (parado: ya). Borrar: la negra de la escena sostenida 3 s. */
#define SCENE_DEL_MS 3000u
static bm_scene *scn(const bm_app *a, int pat, int s) { return (bm_scene *)&a->song.scene[pat % BM_PATTERNS][s]; }

static void scene_store(bm_app *a, int pat, int s)
{
    bm_scene *sc = scn(a, pat, s);
    uint16_t lm[BM_TR_TRACKS];
    sc->trk_mute = bm_mutes_get(&a->pat, lm);
    memcpy(sc->lane_mute, lm, sizeof lm);
    sc->flags = SCN_STORED;
    a->song_dirty = 1; a->t_edit = a->now;
    char b[30]; snprintf(b, sizeof b, TF("SCENE %c A%02d STORED"), 'A' + s, pat + 1);
    app_toast(a, b);
}

static void scene_clear(bm_app *a, int s)
{
    memset(scn(a, a->ctx.st.pattern, s), 0, sizeof(bm_scene));
    a->song_dirty = 1; a->t_edit = a->now;
    char b[24]; snprintf(b, sizeof b, TF("SCENE %c DELETED"), 'A' + s);
    app_toast(a, b);
}

static void scene_launch(bm_app *a, int s)
{
    app_push(a, CMD_SCENE, (uint8_t)s, 0, 0);
    char b[24]; snprintf(b, sizeof b, app_playing(a) ? TF("SCENE %c: NEXT BAR") : TF("SCENE %c"), 'A' + s);
    app_toast(a, b);
}

static void scene_key(bm_app *a, int k, int s)
{
    a->key_role[k] = ROLE_SCENE; a->key_t0[k] = a->now;
    if (!a->pat_held) return;                              /* sola: un toque la lanza, 3 s la borra */
    int pat = 0;
    while (!((a->pat_held >> pat) & 1)) pat++;             /* el patrón sostenido */
    scene_store(a, pat, s);                                     /* con un patrón sostenido: guardar los mutes de ahora */
    a->key_role[k] = ROLE_SCNDONE;
    for (int i = 0; i < BM_KEYS; i++)                      /* el patrón sostenido sirvió de modificador: no se elige */
        if (a->keys_down[i] && a->key_role[i] == ROLE_PAT) a->key_role[i] = ROLE_PATUSED;
}

static void mach_reset(bm_app *a, int t);

/* La tecla de un canal (blancas 11-16): con el cursor ya en él entra a PLAY/REC; si no, mueve el cursor. */
static void chan_key(bm_app *a, int k, int t)
{
    a->key_role[k] = ROLE_CHAN;
    if (a->view == V_GLO) { a->track = (uint8_t)t; mach_reset(a, t); return; }
    if (t == a->track) { app_sloop(a, 1); return; }
    a->track = (uint8_t)t;
}

/* El mezclador: blancas 1-9 patrones (se eligen al soltar), 10 nada, 11-16 canales; negras 1-6 mutes, 7 sin
 * función, 8-11 escenas. */
static void console_key(bm_app *a, int k, int w, int b)
{
    if (w >= 0) {
        if (w >= BM_CH_W0) { chan_key(a, k, BM_ORDER[w - BM_CH_W0]); return; }
        if (w >= BM_PATTERNS) return;                          /* la 10: espacio de seguridad */
        a->key_role[k] = ROLE_PAT;
        a->pat_held |= (uint16_t)(1u << w);
        return;
    }
    if (b < 6) {
        a->key_t0[k] = a->now;
        if (a->solo_on) { toggle_mute(a, BM_ORDER[b]); a->key_role[k] = ROLE_SOLOADD; return; }   /* en SOLO: ese canal se suma */
        a->key_role[k] = ROLE_MUTE;
        return;
    }
    if (b >= SCENE_B0 && b < SCENE_B0 + BM_SCENES) scene_key(a, k, b - SCENE_B0);
}

/* Una tecla toca un pad del sampler: sostenida, según su modo (ONESHOT, KEY, THRU, LOOP). Con REC
 * sonando, el golpe queda en la línea del pad. */
static void pad_down(bm_app *a, int k, int pad)
{
    a->key_role[k] = ROLE_PAD;
    a->pad_key = (int8_t)k;
    a->sel_lane[1] = (uint8_t)pad;
    app_push(a, CMD_PAD_KEY, 1, (uint8_t)pad, 110);
    if (a->rec && app_playing(a)) rec_tr(a, 1, pad);
}

void app_pad_down(bm_app *a, int k, int pad) { pad_down(a, k, pad & 15); }

/* PLAY/REC: las teclas tocan (y graban) el canal del cursor */
static void sloop_key(bm_app *a, int k, int w, int b)
{
    (void)b;
    int t = a->track;
    if (app_kind(a, t) == KIND_NOTE) {
        uint8_t n = (uint8_t)bm_clampi(key_note(a, t, k), 0, 127);
        play_notes(a, k, t, &n, 1, 105);
        return;
    }
    if (w < 0) return;                                       /* las negras de la batería y del sampler: sin función */
    a->sel_lane[t] = (uint8_t)w;
    if (bm_is_pads(t)) { pad_down(a, k, w); return; }
    uint8_t n = (uint8_t)w;
    int vel = bm_gesture_is_down(&a->ges, BTN_OCT_UP) ? 127 : bm_gesture_is_down(&a->ges, BTN_OCT_DN) ? 55 : 100;   /* como SLOOP */
    play_notes(a, k, t, &n, 1, vel);
}

/* ------------------------------------------------------------ secuenciador de la batería: pasos */
static void step_page_set(bm_app *a, int p)
{
    int pages = step_pages(a);
    if (pages < 2) { app_toast(a, "ONE BAR ONLY"); return; }    /* con un solo compás no hay a dónde ir */
    p = (p % pages + pages) % pages;                            /* en círculo */
    a->step_page = (uint8_t)p;
    char b[24]; snprintf(b, sizeof b, TF("BAR %d/%d"), p + 1, pages);
    app_toast(a, b);
}

static void seq_step_down(bm_app *a, int w)
{
    int t = a->track;
    int s = a->step_page * BM_PAGE_STEPS + w;
    int len = bm_trk_len(&a->pat, t, tr_lane(a));
    if (s >= len) { app_toast(a, "OUTSIDE THE LENGTH"); return; }
    a->held_white = (int8_t)w; a->held_step = (int8_t)s;
    a->held_long = 0; a->held_edit = 0; a->held_notes = 0; a->step_t0 = a->now;
    int lane = tr_lane(a);
    a->held_had_event = (bm_trk_trc(&a->pat, t)->st[lane][s] & ST_ON) != 0;
    if (!a->held_had_event) { app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, ST_ON); app_edited(a); }
}

static void seq_step_up(bm_app *a, int w)
{
    if (w != a->held_white) return;
    int t = a->track, s = a->held_step;
    if (s >= 0 && a->held_had_event && !a->held_long && !a->held_edit) {
        app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)tr_lane(a), (uint8_t)s, 0); app_edited(a);
    }
    a->held_white = -1; a->held_step = -1; a->held_long = 0; a->held_edit = 0;
}

/* Una capa abierta con un toque queda hasta que la cierre el mismo botón, otro botón o HOME */
static void key_screen_close(bm_app *a)
{
    if (a->layer_latch) a->layer = L_NONE;
    a->layer_latch = 0;
}

static void layer_latch_open(bm_app *a, int L)
{
    if (a->layer || a->view != V_CH) return;                 /* nunca dos capas a la vez */
    a->layer = (uint8_t)L;
    a->layer_latch = 1;
    a->ov.on = 0;
}

static void toggle_lane_mute(bm_app *a, int t, int lane)
{
    uint16_t m = (uint16_t)(bm_trk_trc(&a->pat, t)->mute ^ (1u << lane));
    app_push_i(a, CMD_LANE_MUTE, (uint8_t)t, 0, 0, m);
    app_edited(a);
    char b[24]; snprintf(b, sizeof b, "%s %s", T(bm_lane_name(a, t, lane)), T((m >> lane) & 1 ? "MUTE" : "PLAYS"));
    app_toast(a, b);
}

/* ------------------------------------------------------------ capas: pasos pintados */
static void paint_accent(bm_app *a, int w)
{
    int t = a->track, s = a->step_page * 16 + w;
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = tr_lane(a);
    if (s >= tr->lane[lane].len) return;
    uint8_t v = tr->st[lane][s];
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, (v & ST_ON) ? (v ^ ST_ACC) : (ST_ON | ST_ACC));
    app_edited(a);
}

static void paint_repeat(bm_app *a, int w)
{
    int t = a->track, s = a->step_page * 16 + w;
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = tr_lane(a);
    if (s >= tr->lane[lane].len) return;
    uint8_t v = tr->st[lane][s];
    if (!(v & ST_ON)) v = ST_ON;
    int r = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
    r = (r + 1) % 5;
    v = (uint8_t)(v & ~(ST_RAT_MASK | ST_FLAM));
    if (r == 4) v |= ST_FLAM; else v |= (uint8_t)(r << ST_RAT_SH);
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, v);
    app_edited(a);
}

/* ------------------------------------------------------------ teclas */
static void key_release(bm_app *a, int k, int w, int b)
{
    if (a->key_nn[k]) {
        int t = a->key_trk[k];
        if (app_kind(a, t) == KIND_NOTE) for (int i = 0; i < a->key_nn[k]; i++) app_push(a, CMD_NOTE_OFF, (uint8_t)t, a->key_notes[k][i], 0);
        if (a->key_rec_step[k] >= 0 && a->key_rec_slot[k] >= 0 && app_kind(a, t) == KIND_NOTE) {
            int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
            int s = a->key_rec_step[k];
            uint32_t held = a->ctx.st.btick - a->key_rec_bt[k];       /* lo que la sostuviste, en ticks (btick no vuelve a 0 con el maestro) */
            int steps = (int)(held / (uint32_t)tps), frac = (int)(held % (uint32_t)tps), gate = 0;
            if (frac * 8 > tps) { steps++; gate = frac * 100 / tps; if (gate >= 90) gate = 0; }
            if (steps < 1) { steps = 1; gate = bm_clampi(frac * 100 / tps, 10, 99); if (gate >= 90) gate = 0; }
            {                                                      /* el compás corta la nota (auto-gate clip) */
                int spb = BM_BAR_TICKS / tps, end = (s / spb + 1) * spb;
                if (s + steps > end) { steps = end - s; gate = 0; }
            }
            int Lc = a->pat.trk[t].length ? a->pat.trk[t].length : 16;   /* y nunca más que el canal: no se pisa a sí misma */
            steps = bm_clampi(steps, 1, Lc < BM_STEPS ? Lc : BM_STEPS);
            uint8_t tie = 0;
            if (a->pat.trk[t].engine == ENG_ACID) {                /* BB-303: tocada ligada = SLIDE hasta la siguiente */
                int L = a->pat.trk[t].length ? a->pat.trk[t].length : 16;
                for (int k2 = 0; k2 < BM_KEYS; k2++)
                    if (k2 != k && a->keys_down[k2] && a->key_nn[k2] && a->key_trk[k2] == t && a->key_rec_step[k2] >= 0 &&
                        (int32_t)(a->key_rec_bt[k2] - a->key_rec_bt[k]) > 0) {
                        int dist = (a->key_rec_step[k2] - s + L) % L;
                        if (dist >= 1) { steps = dist; gate = 0; tie = BM_LEN_TIE; }
                        break;
                    }
            }
            for (int e = 0; e < BM_EVENTS; e++) {
                bm_event ev = *bm_evc(&a->pat, t, s, e);
                for (int i = 0; i < a->key_nn[k]; i++)
                    if (!bm_event_empty(&ev) && ev.note == a->key_notes[k][i]) { ev.len = (uint8_t)(steps | tie); ev.gate = (uint8_t)gate; app_push_ev(a, t, s, e, &ev); }
            }
        }
        a->key_nn[k] = 0;
        a->key_rec_step[k] = -1;
    }
    if (a->view == V_MACH) mach_key_up(a, k);
    switch (a->key_role[k]) {
    case ROLE_MUTE:
        if (a->solo_key == k && a->solo_on) {                 /* fin del solo: vuelven los mutes de antes */
            for (int t = 0; t < BM_TRACKS; t++) app_set_track(a, t, TRK_OFF(mute), (a->solo_saved >> t) & 1);
            a->solo_on = 0; a->solo_key = -1;
            app_toast(a, "SOLO OFF");
        } else if (b >= 0 && b < 6) toggle_mute(a, BM_ORDER[b]);
        if (a->solo_key == k) a->solo_key = -1;
        break;
    case ROLE_PAT:                                          /* un toque: el patrón entra al final del compás */
        if (w >= 0 && w < BM_PATTERNS) app_select_pattern(a, w);
        break;
    case ROLE_SCENE:                                        /* un toque: lanzar la escena (si está guardada) */
        if (b >= SCENE_B0 && b < SCENE_B0 + BM_SCENES && (scn(a, a->ctx.st.pattern, b - SCENE_B0)->flags & SCN_STORED)) scene_launch(a, b - SCENE_B0);
        break;
    case ROLE_LREP:                                         /* mantenida: solo mientras la sostienes; un toque la deja abierta */
        if (a->layer == L_REP && a->layer_latch && a->now - a->key_t0[k] >= UI_HOLD_MS) key_screen_close(a);
        break;
    case ROLE_PAD:                                          /* solo la última tecla de pad lo suelta */
        if (a->pad_key == k) { app_push(a, CMD_PAD_KEY, 0, (uint8_t)(a->sel_lane[1] & 15), 0); a->pad_key = -1; }
        break;
    default: break;
    }
    a->key_role[k] = ROLE_NONE;
    a->key_t0[k] = 0;
    if (w >= 0) {
        if (w < BM_PATTERNS) a->pat_held &= (uint16_t)~(1u << w);
        seq_step_up(a, w);
    }
}

/* Elegir (y escuchar) un instrumento */
static void pick_lane(bm_app *a, int k, int lane)
{
    a->sel_lane[a->track] = (uint8_t)lane;
    uint8_t n = (uint8_t)lane;
    play_notes(a, k, a->track, &n, 1, 100);
}

/* LENGTH AND SPEED (Slicer): el largo del pad elegido en pasos, la velocidad del canal */
static void pad_len_set(bm_app *a, int len)
{
    int t = a->track, lane = tr_lane(a);
    len = bm_clampi(len, 1, BM_STEPS);
    app_push_i(a, CMD_SET_LANE, (uint8_t)t, (uint8_t)lane, LN_LEN, len);
    app_edited(a);
    if (a->step_page * 16 >= len) a->step_page = (uint8_t)((len - 1) / 16);
    char b[24]; snprintf(b, sizeof b, TF("P%d LENGTH %d"), lane + 1, len);
    app_toast(a, b);
}

static void speed_set(bm_app *a, int s)
{
    int t = a->track;
    s = bm_clampi(s, 0, SPD_COUNT - 1);
    app_set_track(a, t, TRK_OFF(speed), s);
    char b[24]; snprintf(b, sizeof b, TF("T%d SPEED %s"), app_pos(t) + 1, bm_speed_names[s]);
    app_toast(a, b);
}

static void len_key(bm_app *a, int w, int b)
{
    int t = a->track;
    if (w >= 0) { pad_len_set(a, a->len_page * 16 + w + 1); return; }
    if (b < SPD_COUNT) { speed_set(a, b); return; }
    if (b >= 6 && b <= 9) {                                     /* compases completos */
        int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
        if ((b - 5) * spb <= BM_STEPS) pad_len_set(a, (b - 5) * spb);
        return;
    }
    if (b == 10) {                                              /* +16: 1-16 > 17-32 > 33-48 > 49-64 */
        a->len_page = (uint8_t)((a->len_page + 1) % 4);
        char s[24]; snprintf(s, sizeof s, TF("LENGTHS %d-%d"), a->len_page * 16 + 1, a->len_page * 16 + 16);
        app_toast(a, s);
    }
}

static void layer_key(bm_app *a, int k, int w, int b)
{
    int t = a->track;
    if (a->layer == L_LEN) { len_key(a, w, b); return; }
    if (a->layer != L_REP && b >= 0) {                                                       /* LFO, ENV, EDIT y SCL */
        int lane = (tr_lane(a) / 8) * 8 + b;
        if (b < 8 && a->layer == L_MUTE) toggle_lane_mute(a, t, lane);                       /* acceso rápido: mutea */
        else if (b < 8) a->sel_lane[t] = (uint8_t)lane;                                       /* acceso rápido: elige, sin sonar */
        else if (b == 9 || b == 10) step_page_set(a, a->step_page + (b == 10 ? 1 : -1));     /* los compases, en círculo */
        return;
    }
    if (b >= 0 && b < 8) { pick_lane(a, k, (tr_lane(a) / 8) * 8 + b); return; }              /* navegar: los instrumentos de la página */
    if (b == 8 || b == 9) { step_page_set(a, a->step_page + (b == 9 ? 1 : -1)); return; }     /* y los compases, en círculo */
    if (b == 10) { if (a->layer == L_REP && a->layer_latch) key_screen_close(a); return; }
    if (w < 0) return;
    switch (a->layer) {
    case L_ACC: paint_accent(a, w); return;
    case L_REP: paint_repeat(a, w); return;
    case L_HIT: case L_INST: pick_lane(a, k, w); return;
    case L_MUTE: toggle_lane_mute(a, t, w); return;
    default: return;
    }
}

void bm_app_key(bm_app *a, int k, int down, uint32_t now)
{
    a->now = now;
    if (k < 0 || k >= BM_KEYS) return;
    if (down) {
        if (a->keys_down[k]) return;
        a->keys_down[k] = 1;
        bm_gesture_other(&a->ges, now);
    } else {
        if (!a->keys_down[k]) return;
        a->keys_down[k] = 0;
    }
    if (a->view == V_BOOT) return;
    int w = bm_key_white_index(k), b = bm_key_black_index(k);
    if (!down) { key_release(a, k, w, b); return; }
    if (a->layer) { layer_key(a, k, w, b); return; }
    switch (a->view) {
    case V_HOME:
        if (a->sloop) sloop_key(a, k, w, b); else console_key(a, k, w, b);
        return;
    case V_GLO: console_key(a, k, w, b); return;
    case V_MACH: mach_key(a, k, w, b); return;
    case V_CH:
        if (w >= 0) seq_step_down(a, w);
        else if (b >= 0 && b < 8) pick_lane(a, k, (tr_lane(a) / 8) * 8 + b);   /* negras 1-8: eligen (y tocan) un instrumento de la página de sonidos */
        else if (b == 10) {                                      /* negra 11: la página de REPEAT (un toque la deja abierta) */
            a->key_role[k] = ROLE_LREP; a->key_t0[k] = a->now;
            layer_latch_open(a, L_REP);
        }
        else if (b == 8 || b == 9) step_page_set(a, a->step_page + (b == 9 ? 1 : -1));   /* negras 9 y 10: el compás anterior / siguiente */
        return;
    default: return;                                          /* SYSTEM y SAVE: el teclado no hace nada */
    }
}

/* ------------------------------------------------------------ botones */
/* OCT -/+: en el secuenciador de la batería (y en su PLAY/REC) cambian la página de sonidos; en
 * PLAY/REC de una máquina de notas, la octava. En el mezclador no hacen nada. */
static bool oct_ok(const bm_app *a)
{
    if (a->view == V_CH) return true;
    return a->view == V_HOME && a->sloop && (app_kind(a, a->track) == KIND_NOTE || drums_track(a, a->track));   /* batería: sostenidos dan golpes suaves / fuertes */
}

/* la capa de la rejilla que abre cada botón: en la batería LFO, ENV, EDIT y SCL; en el Slicer LFO, SCL y
 * SEQ otra vez (LENGTH AND SPEED): su EDIT, ENV, FX y GLO son páginas de la máquina (V_MACH) */
static int layer_of(const bm_app *a, int id)
{
    bool pads = bm_is_pads(a->track);
    switch (id) {
    case BTN_SCL: return L_MUTE;
    case BTN_LFO: return L_ACC;
    case BTN_ENV: return pads ? L_NONE : L_HIT;
    case BTN_EDIT: return pads ? L_NONE : L_INST;
    case BTN_SEQ: return pads ? L_LEN : L_NONE;
    default: return L_NONE;
    }
}

/* a la rejilla del Slicer, con esa capa abierta (LFO, SCL) o ninguna */
void app_slicer_grid(bm_app *a, int layer)
{
    app_sloop(a, 0);
    app_enter(a, 1, PG_SEQ);
    if (layer) layer_latch_open(a, layer);
}

/* OCT -/+ en la batería: la página de sonidos (8 instrumentos: 1-8 o 9-16); el instrumento elegido
 * se queda en su lugar de la página. */
static void sound_page(bm_app *a, int dir)
{
    int t = a->track, lane = tr_lane(a), pg = lane / 8;
    int np = bm_clampi(pg + dir, 0, BM_LANES / 8 - 1);
    if (np == pg) return;
    a->sel_lane[t] = (uint8_t)(np * 8 + lane % 8);
    app_toast(a, np ? "SOUNDS 9-16" : "SOUNDS 1-8");
}

void app_on_gesture(void *u, int id, bm_gkind k)
{
    bm_app *a = (bm_app *)u;
    if (a->view == V_BOOT) return;
    if (a->view == V_MACH && mach_button(a, id, k)) return;  /* la máquina toma sus botones (PLAY, SAVE y HOME siguen) */
    /* capa abierta con un toque: HOME la cierra; otro botón la cierra y hace lo suyo (el suyo otra
     * vez: solo la cierra). PLAY y OCT -/+ no la cierran. */
    if (a->layer_latch && id != BTN_PLAY && id != BTN_OCT_DN && id != BTN_OCT_UP) {
        if (id == BTN_HOME) {
            if (k == G_TAP) { key_screen_close(a); return; }
        } else if (k == G_TAP || k == G_HOLD) {
            bool same = layer_of(a, id) == a->layer;
            key_screen_close(a);
            if (same) return;
        }
    }
    char b[30];
    switch (id) {
    case BTN_PLAY:
        if (k != G_PRESS) return;
        app_push(a, CMD_PLAY, app_playing(a) ? 0 : 1, 0, 0);
        if (a->bx.armed && !app_playing(a)) { a->bx.armed = 0; rec_set(a, 1); }   /* REC armado: PLAY también arranca la toma */
        /* con el reloj externo, PLAY espera los pulsos MIDI: sin ellos no avanza nada y parecía
         * colgado (dos veces en el aparato, con CLOCK cambiado sin querer en SYSTEM) */
        if (!app_playing(a) && a->set.clock && !a->ctx.st.ext_ok) app_toast(a, "EXT CLOCK: WAITING FOR MIDI");
        return;
    case BTN_OCT_DN: case BTN_OCT_UP:
        if (k != G_PRESS || !oct_ok(a)) return;
        if (a->view == V_CH) { sound_page(a, id == BTN_OCT_UP ? 1 : -1); return; }
        if (drums_track(a, a->track)) return;                 /* PLAY/REC de la batería: se sostienen (golpes suaves / fuertes) */
        a->oct = (int8_t)bm_clampi(a->oct + (id == BTN_OCT_UP ? 1 : -1), -2, 2);
        snprintf(b, sizeof b, TF("OCTAVE %+d"), a->oct);
        app_toast(a, b);
        return;
    case BTN_REC:                                           /* grabar: solo en el mezclador y su PLAY/REC */
        if (k == G_LONG && (a->view == V_HOME || a->view == V_CH)) {   /* 2 s: borra el canal del cursor, sonando o parado */
            if (a->rec) rec_set(a, 0);
            app_push(a, CMD_TRACK_CLEAR, a->track, 0, 0);
            app_edited(a);
            char b[24]; snprintf(b, sizeof b, TF("T%d CLEARED"), app_pos(a->track) + 1);
            app_toast(a, b);
            return;
        }
        if (k != G_TAP || a->view != V_HOME) return;
        if (a->sloop) rec_set(a, !a->rec);
        else { app_sloop(a, 1); rec_set(a, 1); }
        return;
    case BTN_GLO:                                           /* el menú global del mezclador */
        if (k != G_TAP) return;
        if (a->view == V_CH && bm_is_pads(a->track)) { mach_enter(a, a->track, BXP_GLO); return; }   /* la rejilla del Slicer: su GLO */
        if (a->view == V_HOME) { mach_reset(a, a->track); a->glo_page = 0; app_set_view(a, V_GLO); }
        else if (a->view == V_GLO) { a->mach_arm = 0; a->glo_page = (uint8_t)wrap(a->glo_page + 1, BM_GLO_PAGES); a->ov.on = 0; }   /* la página siguiente; sale HOME */
        return;
    case BTN_SAVE:
        if (k == G_TAP) {
            if (a->view == V_SAVE) {
                switch (a->save_row) {
                case 0:                                         /* el patrón y, si cambiaron, las escenas */
                    app_save_jobs(a, 1);
                    if (a->song_dirty) app_job(a, JOB_SAVE_SONG, 0);
                    snprintf(b, sizeof b, TF("SAVED A%02d"), a->ctx.st.pattern + 1);
                    app_toast(a, app_playing(a) ? "SAVES WHEN STOPPED" : b);
                    break;
                case 1:
                    app_save_jobs(a, 0);
                    app_job(a, JOB_SAVE_AS, a->save_target);
                    snprintf(b, sizeof b, TF("SAVED AS A%02d"), a->save_target + 1);
                    app_toast(a, app_playing(a) ? "SAVES WHEN STOPPED" : b);
                    break;
                case 2:
                    if (a->clear_armed && a->now - a->clear_t < 3000) {
                        for (int t = 0; t < BM_TRACKS; t++) app_push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
                        app_edited(a); a->clear_armed = 0;
                        app_toast(a, "PATTERN CLEARED");
                    } else { a->clear_armed = 1; a->clear_t = a->now; app_toast(a, "SAVE AGAIN = CLEAR"); }
                    break;
                default: break;
                }
            } else app_set_view(a, V_SAVE);
        } else if (k == G_LONG) {
            app_save_jobs(a, 1);
            app_toast(a, app_playing(a) ? "SAVES WHEN STOPPED" : "QUICK SAVE");
        }
        return;
    case BTN_HOME:                                          /* salir: a la consola; 2 s: SYSTEM */
        if (k == G_TAP) {
            a->mach_arm = 0;
            app_sloop(a, 0);
            app_set_view(a, V_HOME);
        } else if (k == G_LONG) { app_sloop(a, 0); app_set_view(a, V_SYSTEM); }
        return;
    default: break;
    }
    /* Un clic entra y otro sale: SEQ abre el secuenciador de la batería y lo cierra; LFO, ENV y EDIT,
     * ahí dentro, abren su capa y la cierran (sostener es lo mismo que un clic). En HOME, con el cursor
     * en la batería, cada uno lleva directo a lo suyo: SEQ al secuenciador, LFO/ENV/EDIT a su capa. */
    if (k != G_TAP && k != G_HOLD) return;
    int L = layer_of(a, id);
    if (bm_is_pads(a->track) && (a->view == V_HOME || a->view == V_CH)) {   /* el Slicer: SEQ, LFO y SCL su rejilla; EDIT, ENV y FX sus páginas */
        int p = slicer_page_of(id);
        if (a->view == V_HOME && (id == BTN_SEQ || L)) app_slicer_grid(a, id == BTN_SEQ ? L_NONE : L);
        else if (a->view == V_CH && L) layer_latch_open(a, L);   /* SEQ otra vez: LENGTH AND SPEED */
        else if (p >= 0 && !(a->view == V_HOME && id == BTN_GLO)) mach_enter(a, a->track, p);   /* (GLO en HOME es el de la consola) */
        return;
    }
    if (a->view == V_HOME && mach_has(a->pat.trk[a->track].engine)) {   /* una máquina de notas: cada botón a su página */
        int p = mach_page_of(a->pat.trk[a->track].engine, id);
        if (p >= 0 && id != BTN_GLO) mach_enter(a, a->track, p);
        return;
    }
    if (a->view == V_HOME) {
        if (!drums_track(a, a->track) || (id != BTN_SEQ && !L)) return;
        app_sloop(a, 0);
        app_enter(a, a->track, PG_SEQ);
        if (L) layer_latch_open(a, L);
        return;
    }
    if (a->view != V_CH) return;
    if (id == BTN_SEQ) { app_set_view(a, V_HOME); return; }
    if (L) layer_latch_open(a, L);
}

void bm_app_button(bm_app *a, int id, int down, uint32_t now)
{
    a->now = now;
    if (id < 0 || id >= BTN_COUNT) return;
    if (down) bm_gesture_down(&a->ges, id, now);
    else bm_gesture_up(&a->ges, id, now);
}

static void enc_track_u8(bm_app *a, int t, int k, uint8_t off, int cur, int d, int lo, int hi, const char *name, int pct_mul)
{
    int v = bm_clampi(cur + d, lo, hi);
    app_set_track(a, t, off, v);
    char b[12], ti[22];
    fmt_pct(v, b, sizeof b);
    snprintf(ti, sizeof ti, "T%d %s", app_pos(t) + 1, T(name));
    app_overlay(a, k, ti, b, "", v * pct_mul, -1, k >= 0 ? bm_knob_color(k) : C_WHITE);
}

/* GLO de HOME, páginas TEMPO y MASTER FX: lo del proyecto entero */
typedef struct { const char *name; uint8_t off, lo, hi; } glo_par;
static const glo_par GLO_P[2][4] = {
    {{"SWING", offsetof(bm_meta, swing), 50, 75}, {"SCALE", offsetof(bm_meta, scale), 0, BM_SCALES - 1},
     {"ROOT", offsetof(bm_meta, root), 0, 11}, {"TRANSP", offsetof(bm_meta, transpose), 12, 36}},
    {{"DLY TIME", offsetof(bm_meta, dly_time), 0, 3}, {"DLY FDBK", offsetof(bm_meta, dly_fb), 0, 127},
     {"REV SIZE", offsetof(bm_meta, rv_size), 0, 127}, {"REV MIX", offsetof(bm_meta, rv_mix), 0, 127}}};
static const char *const DLY_T[4] = {"1/16", "1/8", "3/16", "1/4"};

static void glo_fmt(int pg, int k, int v, char *b, int cap)
{
    if (pg == 1 && k == 1) snprintf(b, (size_t)cap, "%s", bm_scale_names[v % BM_SCALES]);
    else if (pg == 1 && k == 2) snprintf(b, (size_t)cap, "%s", bm_note_names[v % 12]);
    else if (pg == 1 && k == 3) snprintf(b, (size_t)cap, v > 24 ? "+%d" : "%d", v - 24);
    else if (pg == 1 && k == 0) snprintf(b, (size_t)cap, "%d%%", v);
    else if (pg == 2 && k == 0) snprintf(b, (size_t)cap, "%s", DLY_T[v & 3]);
    else snprintf(b, (size_t)cap, "%d%%", (v * 100 + 63) / 127);
}

static void glo_knob(bm_app *a, int k, int d)
{
    int pg = a->glo_page;
    if (pg < 1 || pg > 2) return;
    const glo_par *p = &GLO_P[pg - 1][k];
    int cur = ((const uint8_t *)&a->pat.meta)[p->off];
    int v = bm_clampi(cur + (p->hi - p->lo <= 24 ? lst(a, d) : d), p->lo, p->hi);
    if (v == cur) return;
    app_set_meta(a, p->off, v);
    char b[12];
    glo_fmt(pg, k, v, b, sizeof b);
    app_overlay(a, k, p->name, b, "", (v - p->lo) * 255 / (p->hi - p->lo), -1, bm_knob_color(k));
}

static void legend_glo(const bm_app *a, bm_legend *L)
{
    int pg = a->glo_page;
    if (pg < 1 || pg > 2) return;
    char b[12];
    for (int k = 0; k < 4; k++) {
        const glo_par *p = &GLO_P[pg - 1][k];
        int v = ((const uint8_t *)&a->pat.meta)[p->off];
        glo_fmt(pg, k, v, b, sizeof b);
        L4(L, k, p->name, b, (v - p->lo) * 255 / (p->hi - p->lo));
    }
}

/* HOME, ALGORITHM: el BPM del proyecto (global) */
static void enc_bpm(bm_app *a, int d)
{
    bm_meta *m = &a->pat.meta;
    int bpm = bm_clampi(m->bpm_x10 + d * 10, 400, 2400);
    char b[16];
    app_push_i(a, CMD_SET_BPM, 0, 0, 0, bpm); app_edited(a);
    snprintf(b, sizeof b, "%d.%d", bpm / 10, bpm % 10);
    app_overlay(a, -1, "TEMPO", b, "BPM", (bpm - 400) * 255 / 2000, -1, C_TEMPO);
    a->ov.beat = 1;
}

/* Controles bipolares (el filtro DJ): el centro retiene 2 detents antes de salir, y un giro que lo
 * cruza se para en él. */
static int center_stop(bm_app *a, int cur, int d, int c, int lo, int hi)
{
    int nv = bm_clampi(cur + d, lo, hi);
    if (a->stick_id != a->enc_cur || a->enc_new) { a->stick = 0; a->stick_id = a->enc_cur; }
    if (cur != c) {
        a->stick = 0;
        return (cur < c && nv > c) || (cur > c && nv < c) ? c : nv;
    }
    int r = lst(a, d);
    if (a->stick && (a->stick > 0) != (r > 0)) a->stick = 0;
    a->stick = (int8_t)bm_clampi(a->stick + r, -20, 20);
    if (a->stick >= -2 && a->stick <= 2) return c;
    int out = a->stick > 0 ? a->stick - 2 : a->stick + 2;
    a->stick = 0;
    return bm_clampi(c + out, lo, hi);
}

/* filtro DJ: posición -63..63 (0 abierto) <-> djf (0 abierto, 1-63 pasa-bajos, 65-127 pasa-altos) */
static int djf_pos(uint8_t djf) { return !djf || djf == 64 ? 0 : djf < 64 ? -(int)djf : (int)djf - 64; }
static uint8_t djf_of(int pos) { return (uint8_t)(pos < 0 ? -pos : pos > 0 ? 64 + pos : 0); }
static void fmt_djf(uint8_t djf, char *b, int cap)
{
    int p = djf_pos(djf);
    if (!p) snprintf(b, (size_t)cap, "OPEN");
    else snprintf(b, (size_t)cap, "%s %d", p < 0 ? "LP" : "HP", p < 0 ? -p : p);
}

/* HOME, K1-K4: el canal del cursor como una tira de mezclador: VOLUME, FILTER (DJ), DELAY, REVERB */
static void enc_home(bm_app *a, int k, int d)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[16], ti[22];
    switch (k) {
    case 0: enc_track_u8(a, t, 0, TRK_OFF(level), tr->level, d, 0, 127, "VOLUME", 2); return;
    case 1: {
        int p = center_stop(a, djf_pos(tr->djf), d, 0, -63, 63);
        app_set_track(a, t, TRK_OFF(djf), djf_of(p));
        a->pat.trk[t].djf = djf_of(p);
        fmt_djf(djf_of(p), b, sizeof b);
        snprintf(ti, sizeof ti, TF("T%d FILTER"), app_pos(t) + 1);
        app_overlay(a, 1, ti, b, "", (p + 64) * 2, 128, bm_knob_color(1));
        return;
    }
    case 2: enc_track_u8(a, t, 2, TRK_OFF(send), tr->send, d, 0, 127, "DELAY", 2); return;
    case 3: enc_track_u8(a, t, 3, TRK_OFF(rsend), tr->rsend, d, 0, 127, "REVERB", 2); return;
    default: return;
    }
}

/* Asignar una máquina a un canal (T3-T6), desde GLO > SELECT MACHINE: ALGORITHM mueve la elegida
 * entre las 4, sin pregunta; quedarse 2 s en una la carga (en vivo: se elige girando y se suelta);
 * irse antes, o salir de GLO, cancela. Una pesada (BUNO-6, BITTAL) que ya está en otro canal no
 * arranca la cuenta. La máquina queda por patrón. */
static void mach_reset(bm_app *a, int t)                       /* empieza en la que suena, sin cuenta */
{
    a->mach_trk = (uint8_t)t; a->mach_sel = a->pat.trk[t].engine; a->mach_arm = 0;
}

static void glo_mach_turn(bm_app *a, int t, int d)
{
    if (app_fixed(t)) { app_toast(a, "THIS MACHINE IS FIXED"); return; }
    if (a->mach_trk != t) mach_reset(a, t);
    int i = 0;
    while (i < BM_MACHINES && bm_machines[i] != a->mach_sel) i++;
    if (i >= BM_MACHINES) i = 0;
    i = bm_clampi(i + lst(a, d), 0, BM_MACHINES - 1);
    if (bm_machines[i] == a->mach_sel && a->mach_arm) return;
    a->mach_sel = bm_machines[i];
    a->mach_t0 = a->now;
    a->mach_arm = 0;
    if (a->mach_sel == a->pat.trk[t].engine) return;            /* la que suena: nada que cargar */
    int o = bm_machine_conflict(&a->pat, t, a->mach_sel);
    if (o >= 0) {
        char b[30]; snprintf(b, sizeof b, TF("%s ALREADY ON T%d"), bm_engine_names[a->mach_sel], app_pos(o) + 1);
        app_toast(a, b);
        return;
    }
    if (bm_engine_heavy(a->mach_sel) && bm_heavy_count(&a->pat, t) >= BM_HEAVY_MAX) {   /* dice dónde están, para cambiar una de ellas */
        char b[30]; int n = snprintf(b, sizeof b, "%s", T("2 HEAVY ON"));
        for (int i = 0; i < BM_TRACKS && n < (int)sizeof b - 4; i++) {
            int o = BM_ORDER[i];
            if (o != t && bm_engine_heavy(a->pat.trk[o].engine)) n += snprintf(b + n, sizeof b - (size_t)n, " T%d", i + 1);
        }
        app_toast(a, b);
        return;
    }
    a->mach_arm = 1;
}

/* El GLO de cada máquina, MACHINE: solo las que este canal puede tener ahora (una pesada que ya está
 * en otro canal, o una tercera pesada, no aparecen); quedarse 2 s en una la carga, como en HOME. */
static bool mach_avail(const bm_app *a, int t, int e)
{
    if (e == a->pat.trk[t].engine) return true;
    if (!bm_engine_ok(t, e) || bm_machine_conflict(&a->pat, t, e) >= 0) return false;
    return !(bm_engine_heavy(e) && bm_heavy_count(&a->pat, t) >= BM_HEAVY_MAX);
}

int app_mach_shown(const bm_app *a) { return a->mach_trk == a->track ? a->mach_sel : a->pat.trk[a->track].engine; }

void app_mach_pick(bm_app *a, int d)
{
    int t = a->track;
    if (a->mach_trk != t) mach_reset(a, t);
    int i = 0;
    while (i < BM_MACHINES && bm_machines[i] != a->mach_sel) i++;
    if (i >= BM_MACHINES) i = 0;
    for (int n = lst(a, d), dir = n > 0 ? 1 : -1; n; n -= dir) {   /* saltando las que no se pueden */
        int j = i + dir;
        while (j >= 0 && j < BM_MACHINES && !mach_avail(a, t, bm_machines[j])) j += dir;
        if (j < 0 || j >= BM_MACHINES) break;
        i = j;
    }
    a->mach_sel = bm_machines[i];
    a->mach_t0 = a->now;
    a->mach_arm = (uint8_t)(a->mach_sel != a->pat.trk[t].engine);
    char ti[22];
    snprintf(ti, sizeof ti, "T%d %s", app_pos(t) + 1, T("MACHINE"));
    app_overlay(a, 1, ti, bm_engine_names[a->mach_sel], "", i * 255 / (BM_MACHINES - 1), -1, bm_knob_color(1));
}

/* la cuenta: a los 2 s en la misma, el canal cambia de máquina (mientras su GLO siga abierto) */
static void mach_tick(bm_app *a)
{
    if (!a->mach_arm) return;
    bool glo = a->view == V_GLO || (a->view == V_MACH && a->bx.page == BXP_GLO);
    if (!glo || a->track != a->mach_trk) { a->mach_arm = 0; return; }
    if (a->now - a->mach_t0 < BM_MACH_MS) return;
    a->mach_arm = 0;
    app_set_machine(a, a->mach_trk, a->mach_sel);
}

/* ALGORITHM en el secuenciador de la batería: un recuadro dice a qué instrumento vas; al rato lo eliges (y suena). */
static void box_commit(bm_app *a)
{
    a->box_on = 0;
    int t = a->track;
    a->sel_lane[t] = a->box_lane;
    app_push(a, CMD_NOTE_ON, (uint8_t)t, a->box_lane, 100);
    char b[24]; snprintf(b, sizeof b, "%d %s", a->box_lane + 1, T(bm_lane_name(a, t, a->box_lane)));
    app_toast(a, b);
}

static void box_turn(bm_app *a, int d)
{
    if (!a->box_on) { a->box_track = a->track; a->box_lane = (uint8_t)tr_lane(a); a->box_mode = BOX_LANE; }
    a->box_lane = (uint8_t)bm_clampi(a->box_lane + lst(a, d), 0, BM_LANES - 1);
    int ms = bm_app_enter_ms(a);
    a->box_on = 1; a->box_hold = 0;
    if (ms <= 0) { box_commit(a); return; }                  /* sin espera (u OFF): directo */
    a->box_until = a->now + (uint32_t)ms;
}

/* PRESETS: el sonido del canal (el kit de la batería, la toma del sampler, el preset de cada máquina) */
static void enc_source(bm_app *a, int t, int d);
void app_enc_source(bm_app *a, int t, int d) { enc_source(a, t, d); }

static void enc_source(bm_app *a, int t, int d)
{
    char b[16];
    int kind = app_kind(a, t);
    if (bm_is_pads(t)) {                                         /* la toma entera en el pad elegido */
        uint8_t list[BM_TAKES + 8];
        int n = bm_app_sources(a, list), i = 0, pad = a->sel_lane[1] & 15;
        if (!n) { app_toast(a, "NO TAKE"); return; }
        while (i < n && list[i] != a->pat.pad[pad].src) i++;
        i = i >= n ? 0 : wrap(i + lst(a, d), n);
        app_pad_take(a, pad, list[i]);
        char ti[16]; snprintf(ti, sizeof ti, TF("PAD %d"), pad + 1);
        bm_app_src_name(a, list[i], b, sizeof b);
        app_overlay(a, -1, ti, b, "", i * 255 / (n > 1 ? n - 1 : 1), -1, bm_track_color(1));
        return;
    }
    int lim = bm_sound_count(a, t), e = a->pat.trk[t].engine;
    int v = wrap(a->pat.trk[t].source % lim + lst(a, d), lim);
    if (kind == KIND_TR || bm_engine_paged(e)) { app_push(a, CMD_PRESET, (uint8_t)t, (uint8_t)v, 0); app_edited(a); }
    else app_set_track(a, t, TRK_OFF(source), v);
    a->pat.trk[t].source = (uint8_t)v;
    char ti[22];
    if (kind == KIND_TR) snprintf(ti, sizeof ti, "T%d KIT", app_pos(t) + 1);
    else snprintf(ti, sizeof ti, "T%d %s %s", app_pos(t) + 1, bm_engine_names[e % ENG_COUNT], T("SOUND"));
    app_overlay(a, -1, ti, bm_sound_name(a, t), "", v * 255 / (lim > 1 ? lim - 1 : 1), -1, bm_track_color(t));
}

static void system_change(bm_app *a, int d)
{
    bm_settings *s = &a->set;
    /* los interruptores: a la derecha = encendido (o el siguiente), a la izquierda = apagado; nunca
     * dan la vuelta, así que girar de más no los cambia otra vez */
    int on = d > 0;
    switch (BM_SYS_ROWS[a->menu_row % BM_SYS_VIS]) {
    case 0:                                                     /* derecha EXTERNAL, izquierda INTERNAL */
        if (on == s->clock) return;
        s->clock = (uint8_t)on; app_push(a, CMD_CLOCK, s->clock, 0, 0);
        app_toast(a, s->clock ? "CLOCK: EXTERNAL (MIDI)" : "CLOCK: INTERNAL");
        break;
    case 1: if (on == s->sync_mode) return; s->sync_mode = (uint8_t)on; break;
    case 2: {
        static const uint8_t PQ[3] = {1, 2, 4};
        int i = s->sync_ppqn == 4 ? 2 : s->sync_ppqn == 2 ? 1 : 0;
        s->sync_ppqn = PQ[bm_clampi(i + lst(a, d), 0, 2)];
        break;
    }
    case 3: s->sync_lat = (uint8_t)bm_clampi(s->sync_lat + lst(a, d), 0, 20); break;
    case 4: if (on == s->autosave) return; s->autosave = (uint8_t)on; break;
    case 5: s->led_dim = (uint8_t)bm_clampi(s->led_dim + lst(a, d), 0, 2); break;
    case 6:
        if (on == s->fast_lcd) return;
        s->fast_lcd = (uint8_t)on;
        app_toast(a, s->fast_lcd ? "FAST SCREEN ON (TEST)" : "FAST SCREEN OFF");
        break;
    case 7:
        if (on == s->dual_core) return;
        s->dual_core = (uint8_t)on;
        app_toast(a, s->dual_core ? "TWO CORES ON (TEST)" : "TWO CORES OFF");
        break;
    case 9:                                                     /* idioma: derecha el paquete cargado, izquierda inglés */
        if (on && !bm_app_lang_pack(a)) { app_toast(a, T("NO LANGUAGE PACK: LOAD ONE BY USB")); return; }
        if (on == s->lang) return;
        s->lang = (uint8_t)on;
        bm_lang_use(s->lang ? bm_app_lang_pack(a) : NULL);
        break;
    default: return;
    }
    app_push(a, CMD_SYNC, s->sync_mode, s->sync_ppqn, s->sync_lat);
    a->set_dirty = 1; a->t_edit = a->now;
}

/* Aceleración por el tiempo entre giros de cada perilla (no por cuántos detents juntó el bucle, que
 * depende de la pantalla): rápido = x2 o x4 en los valores continuos. Las listas usan lst(). */
#define ENC_FAST_MS  18u     /* menos de esto por detent: x4 */
#define ENC_MED_MS   35u     /* menos de esto: x2 */
#define ENC_GESTURE_MS 800u  /* quieta más que esto: el giro empieza un gesto nuevo */

/* K de una capa de sonido: un parámetro del instrumento elegido */
static void lane_set(bm_app *a, int slot, int v)
{
    app_push_i(a, CMD_SET_LANE, (uint8_t)a->track, (uint8_t)tr_lane(a), (uint8_t)slot, v);
    app_edited(a);
}

static void enc_lane(bm_app *a, int page, int k, int d)
{
    const bm_pdef *def = &bm_lane_pages[page].p[k];
    int cur = lane_val(a, def->slot), step = def->max <= 16 ? lst(a, d) : d;
    int nv = bm_clampi(cur + step, 0, def->max);
    if (nv == cur) return;
    lane_set(a, def->slot, nv);
    char b[12], ti[22];
    bm_prm_fmt(def, nv, b, sizeof b);
    snprintf(ti, sizeof ti, "%s %s", T(bm_lane_name(a, a->track, tr_lane(a))), T(def->slot == LN_COLOR ? bm_drm_color[lane_val(a, LN_MODEL) % DRM_COUNT] : def->name));
    app_overlay(a, k, ti, b, "", nv * 255 / (def->max ? def->max : 1), -1, bm_knob_color(k));
}

/* ALGORITHM en EDIT: el sonido de este instrumento = su modelo de batería (se puede combinar cualquiera con cualquiera) */
static void enc_model(bm_app *a, int d)
{
    int nv = wrap(lane_val(a, LN_MODEL) + lst(a, d), DRM_COUNT);
    lane_set(a, LN_MODEL, nv);
    app_push(a, CMD_NOTE_ON, (uint8_t)a->track, (uint8_t)tr_lane(a), 100);
    char ti[22];
    snprintf(ti, sizeof ti, "%s", T(bm_lane_name(a, a->track, tr_lane(a))));
    app_overlay(a, -1, ti, bm_drm_names[nv], "", nv * 255 / (DRM_COUNT - 1), -1, bm_track_color(a->track));
}

/* K1-K4 en el secuenciador de la batería. Con un paso sostenido: K1 su acento, K2 su repetición
 * (sin abrir capas). Si no: LEVEL del instrumento elegido, LENGTH del kit entero (todos los
 * instrumentos, 1-64 pasos), DECAY y TONE del elegido. */
static void kit_len(bm_app *a, int d) { app_kit_len(a, a->track, 1, d); }

void app_kit_len(bm_app *a, int t, int k, int d)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int cur = tr->lane[tr_lane(a)].len ? tr->lane[tr_lane(a)].len : 16;
    int v = bm_clampi(cur + d, 1, BM_STEPS);
    if (v == cur) return;
    for (int l = 0; l < BM_LANES; l++) app_push_i(a, CMD_SET_LANE, (uint8_t)t, (uint8_t)l, LN_LEN, v);
    app_edited(a);
    if (a->step_page * 16 >= v) a->step_page = (uint8_t)((v - 1) / 16);
    char b[12], ti[22];
    snprintf(b, sizeof b, v > 1 ? "%d STEPS" : "%d STEP", v);
    snprintf(ti, sizeof ti, "T%d %s", app_pos(t) + 1, T("LENGTH"));
    app_overlay(a, k, ti, b, "", (v - 1) * 255 / (BM_STEPS - 1), -1, bm_knob_color(k));
}

static void seq_knob(bm_app *a, int k, int d)
{
    if (a->held_step >= 0) {
        if (k > 1) return;
        int t = a->track, lane = tr_lane(a), s = a->held_step;
        uint8_t v = bm_trk_trc(&a->pat, t)->st[lane][s];
        if (!(v & ST_ON)) return;
        if (k == 0) v = (uint8_t)(d > 0 ? v | ST_ACC : v & ~ST_ACC);
        else {
            int r = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
            r = bm_clampi(r + lst(a, d), 0, 4);
            v = (uint8_t)(v & ~(ST_RAT_MASK | ST_FLAM));
            if (r == 4) v |= ST_FLAM; else v |= (uint8_t)(r << ST_RAT_SH);
        }
        app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, v);
        app_edited(a);
        a->held_edit = 1; a->held_long = 1;                    /* soltarlo no lo borra */
        return;
    }
    if (bm_is_pads(a->track)) { slicer_grid_knob(a, k, d); return; }   /* LEVEL LENGTH PITCH RELEASE del pad */
    if (k == 1) { kit_len(a, d); return; }
    enc_lane(a, 2, k, d);
}

static void enc_dispatch(bm_app *a, int id, int d);

void bm_app_encoder(bm_app *a, int id, int d, uint32_t now)
{
    a->now = now;
    if (!d || a->view == V_BOOT || id < 0 || id >= ENC_COUNT) return;
    bm_gesture_other(&a->ges, now);
    d = bm_clampi(d, -100, 100);
    uint32_t dt = now - a->enc_t[id];
    a->enc_t[id] = now;
    uint32_t per = dt / (uint32_t)(d < 0 ? -d : d);
    a->enc_new = dt > ENC_GESTURE_MS;
    a->enc_raw = (int8_t)d;
    a->enc_cur = (uint8_t)(id + 1);
    int mul = per < ENC_FAST_MS ? 4 : per < ENC_MED_MS ? 2 : 1;
    enc_dispatch(a, id, d * mul);
    a->enc_raw = 0;
    a->enc_cur = 0;
}

static void enc_dispatch(bm_app *a, int id, int d)
{
    int k = id - ENC_K1;
    if (a->view == V_MACH) { mach_encoder(a, id, d); return; }
    /* capas: las perillas siguen a la capa */
    switch (a->layer) {
    case L_ACC: if (k == 0) enc_track_u8(a, a->track, 0, TRK_OFF(accent), a->pat.trk[a->track].accent, d, 0, 127, "ACCENT", 2); return;
    case L_HIT: if (k >= 0 && k <= 3) enc_lane(a, 0, k, d); return;
    case L_INST:
        if (id == ENC_ALGO) enc_model(a, d);
        else if (k >= 0 && k <= 2) enc_lane(a, 1, k, d);
        return;
    case L_REP: case L_MUTE: return;
    case L_LEN:                                                /* Slicer: K1 el largo del pad, K2 la velocidad; ALGORITHM el pad */
        if (id == ENC_ALGO) box_turn(a, d);
        else if (k == 0) pad_len_set(a, bm_trk_len(&a->pat, a->track, tr_lane(a)) + lst(a, d));
        else if (k == 1) speed_set(a, a->pat.trk[a->track].speed - lst(a, d));
        return;
    default: break;
    }
    switch (a->view) {
    case V_HOME:                                               /* el mezclador: SELECT el canal, ALGORITHM el BPM, PRESETS el sonido */
        if (id == ENC_ALGO) enc_bpm(a, d);
        else if (id == ENC_SELECT) {
            int t0 = a->track;
            a->track = BM_ORDER[wrap(app_pos(a->track) + lst(a, d), BM_TRACKS)];   /* de arriba a abajo, y vuelve al 1 */
            if (a->sloop && a->track != t0) sloop_follow(a);   /* PLAY/REC: el teclado sigue al canal */
        }
        else if (id == ENC_PRESETS) enc_source(a, a->track, d);
        else enc_home(a, k, d);
        return;
    case V_GLO:                                                /* PRESETS: la página; MACHINES: SELECT el canal, ALGORITHM su máquina */
        if (a->glo_page == 0) {
            if (id == ENC_SELECT) { a->track = BM_ORDER[wrap(app_pos(a->track) + lst(a, d), BM_TRACKS)]; mach_reset(a, a->track); }
            else if (id == ENC_ALGO) glo_mach_turn(a, a->track, d);
            return;
        }
        if (a->glo_page == 1 && id == ENC_ALGO) { enc_bpm(a, d); return; }
        if (k >= 0 && k <= 3) glo_knob(a, k, d);
        return;
    case V_SYSTEM:
        if (id == ENC_SELECT) a->menu_row = (uint8_t)bm_clampi(a->menu_row + lst(a, d), 0, BM_SYS_VIS - 1);
        else if (id == ENC_ALGO || id == ENC_K1) system_change(a, d);
        return;
    case V_SAVE:                                               /* PRESETS = el patrón de destino, como ALGORITHM */
        if (id == ENC_SELECT) a->save_row = (uint8_t)bm_clampi(a->save_row + lst(a, d), 0, BM_SAVE_ROWS - 1);
        else if (id == ENC_ALGO || id == ENC_K1 || id == ENC_PRESETS) a->save_target = (uint8_t)bm_clampi(a->save_target + lst(a, d), 0, BM_PATTERNS - 1);
        a->clear_armed = 0;
        return;
    case V_CH:                                                 /* secuenciador de la batería */
        if (id == ENC_PRESETS) enc_source(a, a->track, d);
        else if (id == ENC_ALGO) box_turn(a, d);
        else if (k >= 0 && k <= 3) seq_knob(a, k, d);
        return;
    default: return;
    }
}

/* ------------------------------------------------------------ tick de la entrada */
void app_input_tick(bm_app *a)
{
    uint32_t now = a->now;
    mach_tick(a);
    if (a->held_white >= 0 && !a->held_long && now - a->step_t0 >= UI_HOLD_MS) a->held_long = 1;
    for (int k = 0; k < BM_KEYS; k++) {
        if (!a->keys_down[k] || !a->key_t0[k] || now - a->key_t0[k] < UI_HOLD_MS) continue;
        switch (a->key_role[k]) {
        case ROLE_MUTE:                                          /* negra sostenida en la consola: solo */
            if (!a->solo_on) {
                int t = BM_ORDER[bm_key_black_index(k)];
                a->solo_saved = 0;
                for (int i = 0; i < BM_TRACKS; i++) if (a->pat.trk[i].mute) a->solo_saved |= (uint8_t)(1u << i);
                for (int i = 0; i < BM_TRACKS; i++) app_set_track(a, i, TRK_OFF(mute), i != t);
                a->solo_on = 1; a->solo_key = (int8_t)k; a->solo_track = (int8_t)t;
                char b[16]; snprintf(b, sizeof b, "SOLO T%d", app_pos(t) + 1);
                app_toast(a, b);
            }
            break;
        case ROLE_SCENE:                                         /* escena 2 s: borrarla (si estaba guardada) */
            if (now - a->key_t0[k] >= SCENE_DEL_MS) {
                int s = bm_key_black_index(k) - SCENE_B0;
                if (s >= 0 && s < BM_SCENES && (scn(a, a->ctx.st.pattern, s)->flags & SCN_STORED)) scene_clear(a, s);
                a->key_role[k] = ROLE_SCNDONE;
            }
            break;
        default: break;
        }
    }
    if (a->box_on && (int32_t)(now - a->box_until) >= 0) box_commit(a);
}

/* ------------------------------------------------------------ anillos (para el dibujo) */
/* una escena guardada sostenida: el anillo hacia los 2 s en que se borra */
int bm_app_key_ring(const bm_app *a, char *label, int cap)
{
    for (int k = 0; k < BM_KEYS; k++) {
        if (!a->keys_down[k] || a->key_role[k] != ROLE_SCENE) continue;
        int s = bm_key_black_index(k) - SCENE_B0;
        if (s < 0 || s >= BM_SCENES || !(scn(a, a->ctx.st.pattern, s)->flags & SCN_STORED)) continue;
        uint32_t el = a->now - a->key_t0[k];
        if (el < UI_HOLD_MS) continue;
        snprintf(label, (size_t)cap, TF("DELETE SCENE %c"), 'A' + s);
        uint32_t p = (el - UI_HOLD_MS) * 255u / (SCENE_DEL_MS - UI_HOLD_MS);
        return (int)(p > 255 ? 255 : p);
    }
    return -1;
}

/* ------------------------------------------------------------ LEDs */
static uint8_t led_last[BM_LEDS];

static void led(bm_app *a, int id, uint8_t lvl)
{
    if (id < BM_LED_KEY0 && lvl == LED_DIM) {                /* SISTEMA > LED: solo los botones en reposo */
        if (a->set.led_dim == 0) lvl = LED_OFF;
        else if (a->set.led_dim == 2) lvl = LED_MID;
    }
    if (led_last[id] == lvl + 1) return;
    led_last[id] = (uint8_t)(lvl + 1);
    if (a->pf.led) a->pf.led(a->pf.user, id, lvl);
}

/* El botón sostenido de cada capa (respira) */
static int layer_button(int L)
{
    switch (L) {
    case L_ACC: return BTN_LFO;
    case L_HIT: return BTN_ENV;
    case L_INST: return BTN_EDIT;
    case L_MUTE: return BTN_SCL;
    case L_LEN: return BTN_SEQ;
    default: return -1;                                      /* REPEAT se abre con la negra 11 */
    }
}

/* El botón de "dónde estás" en cada pantalla */
static int view_button(const bm_app *a)
{
    switch (a->view) {
    case V_HOME: case V_SYSTEM: return BTN_HOME;
    case V_GLO: return BTN_GLO;
    case V_SAVE: return BTN_SAVE;
    case V_CH: return BTN_SEQ;
    default: return -1;
    }
}

/* ¿el botón hace algo en esta pantalla? (los que no, quedan apagados) */
static bool btn_used(const bm_app *a, int i)
{
    switch (a->view) {
    case V_HOME: return i == BTN_HOME || i == BTN_GLO || i == BTN_SAVE || i == BTN_REC ||
                        ((i == BTN_SEQ || layer_of(a, i)) && bm_is_grid(&a->pat, a->track)) ||
                        (i != BTN_GLO && mach_page_of(a->pat.trk[a->track].engine, i) >= 0);   /* la máquina: sus páginas */
    case V_GLO: return i == BTN_HOME || i == BTN_GLO || i == BTN_SAVE;
    case V_CH: return i == BTN_HOME || i == BTN_SAVE || i == BTN_SEQ || layer_of(a, i) || (bm_is_pads(a->track) && slicer_page_of(i) >= 0);
    default: return i == BTN_HOME || i == BTN_SAVE;
    }
}

/* Hay algo sin guardar (el patrón, las escenas o una toma). Los ajustes no: SAVE no los escribe, se
 * guardan solos. */
static bool unsaved(const bm_app *a)
{
    return a->dirty || a->song_dirty || a->pend_obj >= 0 || a->take_dirty || a->ctx.st.smp_unsaved;
}

static bool writing(const bm_app *a)
{
    for (int i = 0; i < a->njob; i++) if (a->job[i] != JOB_LOAD) return true;
    return a->job_fading;
}

/* Un patrón o una escena en cola */
static bool queued(const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    return (st->queued != 0xFF && st->queued != st->pattern && st->pchain_n <= 1) || (st->scene_q != 0xFF && !st->chain_n);
}

static uint8_t play_led(const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    if (st->playing) return (st->btick % BM_BAR_TICKS) < 24 ? LED_ON : (st->btick % BM_PPQN) < 24 ? LED_MID : LED_DIM;
    return a->set.clock && !st->ext_ok ? LED_BREATH : LED_DIM;      /* reloj externo sin señal: esperando */
}

static uint8_t rec_led(const bm_app *a)
{
    if (a->view != V_HOME) return LED_OFF;
    return a->rec ? LED_ON : LED_DIM;                        /* sobregrabando / disponible */
}

static uint8_t oct_led(const bm_app *a, int up)
{
    if (!oct_ok(a)) return LED_OFF;
    if (a->view == V_CH) return (up ? tr_lane(a) >= 8 : tr_lane(a) < 8) ? LED_ON : LED_DIM;   /* la página de sonidos */
    if (drums_track(a, a->track)) return bm_gesture_is_down(&a->ges, up ? BTN_OCT_UP : BTN_OCT_DN) ? LED_ON : LED_DIM;
    return (up ? a->oct > 0 : a->oct < 0) ? LED_ON : LED_DIM;                                  /* octava corrida */
}

/* La luz de un botón. Blancas: dicen el estado, nunca la función (eso lo dice la pantalla).
 * OFF no hace nada aquí · DIM disponible · MID significado especial ahora · ON dónde estás o un
 * estado que queda · BLINK te necesita ya · BREATH armado o pendiente. */
static uint8_t btn_led(const bm_app *a, int i)
{
    if (a->view == V_MACH && i != BTN_PLAY && i != BTN_SAVE) return mach_btn_led(a, i);
    if (a->layer) {                                         /* capa sostenida, o REPETIR abierta */
        if (i == layer_button(a->layer)) return LED_BREATH;
        switch (i) {
        case BTN_OCT_DN: case BTN_OCT_UP: return oct_led(a, i == BTN_OCT_UP);
        case BTN_PLAY: return play_led(a);
        case BTN_HOME: return a->layer_latch ? LED_MID : LED_OFF;
        default: return LED_OFF;
        }
    }
    switch (i) {
    case BTN_PLAY: return play_led(a);
    case BTN_REC: return rec_led(a);
    case BTN_OCT_DN: case BTN_OCT_UP: return oct_led(a, i == BTN_OCT_UP);
    case BTN_HOME:
        if (queued(a) && !console(a)) return LED_BLINK;     /* patrón o escena en cola */
        if (a->sloop) return LED_MID;                       /* PLAY/REC: HOME es la salida */
        break;
    case BTN_SAVE:
        if (writing(a)) return app_playing(a) ? LED_BLINK : LED_ON;   /* sonando: se escribe al parar */
        if (a->view != V_SAVE && a->view != V_CH && unsaved(a)) return LED_BREATH;   /* global: no dentro del secuenciador */
        break;
    default: break;
    }
    if (i == view_button(a)) return LED_ON;
    return btn_used(a, i) ? LED_DIM : LED_OFF;
}

/* Qué hace cada tecla ahora y cómo está su luz. Las luces del FM-1 son blancas: solo dicen el estado
 * (la gramática está arriba, en el mapa del teclado), así que la pantalla muestra estos mismos nombres
 * (tira de teclas) y la luz y el dibujo salen siempre de aquí. */
static void keys_fill(const bm_app *a, bm_keyfn K[BM_KEYS])
{
    int t = a->track;
    char title[24], b8[8]; uint16_t col; const char *h1, *h2;
    if (a->layer) { bm_app_keymap(a, K, title, sizeof title, &col, &h1, &h2); return; }
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    switch (a->view) {
    case V_HOME:
        if (a->sloop) map_sloop(a, K); else map_console(a, K);
        return;
    case V_GLO: map_console(a, K); return;
    case V_MACH: mach_keys(a, K); return;
    case V_CH: {
        const bm_tr *tr = bm_trk_trc(&a->pat, t);
        int lane = tr_lane(a);
        for (int w = 0; w < 16; w++) {
            int s = a->step_page * 16 + w;
            uint8_t l = LED_OFF;
            snprintf(b8, sizeof b8, "%d", s + 1);
            if (s < tr->lane[lane].len) {
                uint8_t bb = tr->st[lane][s];
                l = (bb & ST_ON) ? ((bb & ST_LOCK) ? LED_BREATH : LED_ON) : w % 4 == 0 ? LED_DIM : LED_OFF;   /* tiempos 1-4: tenues */
                if (s == bm_app_track_step(a, t, lane)) l = LED_BLINK;
            }
            KW(K, w, b8, l, 0);
        }
        nav_blacks(a, K);
        KB(K, 10, "RPT", LED_DIM, C_YELLOW);
        return;
    }
    default: return;                                                    /* SYSTEM y SAVE: el teclado no hace nada */
    }
}

void bm_app_keys(const bm_app *a, bm_keyfn K[BM_KEYS])
{
    keys_fill(a, K);
}

void app_leds(bm_app *a)
{
    if (a->view == V_BOOT) {
        int lit = (int)((a->now - a->boot_t) / 40u) % (BM_KEYS + 6);
        for (int k = 0; k < BM_KEYS; k++) led(a, BM_LED_KEY0 + k, k == lit ? LED_ON : LED_OFF);
        for (int i = 0; i < BTN_COUNT; i++) led(a, i, LED_DIM);
        return;
    }
    for (int i = 0; i < BTN_COUNT; i++) led(a, i, btn_led(a, i));
    bm_keyfn K[BM_KEYS];
    bm_app_keys(a, K);
    for (int k = 0; k < BM_KEYS; k++)                         /* sostenida (si hace algo) o un golpe que suena: ON */
        led(a, BM_LED_KEY0 + k, (a->keys_down[k] && K[k].label[0]) || K[k].live ? LED_ON : K[k].led);
}
