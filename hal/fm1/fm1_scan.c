/* Bubba Machine — escaneo de matriz: antirrebote y cuadratura (portable, probado en PC). */
#include <string.h>
#include "fm1_scan.h"

/* Tabla de cuadratura: índice = (estado anterior << 2) | estado nuevo. */
static const int8_t QDEC[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void fm1_scan_init(fm1_scan *s, const fm1_scan_map *m)
{
    memset(s, 0, sizeof *s);
    s->map = m;
}

static void set_bit(volatile uint32_t *b, int id, int v)
{
    uint32_t m = 1u << (id & 31);
    if (v) b[id >> 5] |= m; else b[id >> 5] &= ~m;
}

static int get_bit(const volatile uint32_t *b, int id) { return (b[id >> 5] >> (id & 31)) & 1; }

static int raw_at(const fm1_scan *s, int col, int row) { return (s->raw[col] >> row) & 1; }

uint8_t fm1_scan_column(fm1_scan *s, uint8_t rows)
{
    uint8_t c = s->col;
    s->raw[c] = rows;
    for (int r = 0; r < FM1_ROWS; r++) {
        int id = s->map->id[r][c];
        if (id < 0 || id >= FM1_IDS) continue;
        int closed = (rows >> r) & 1;
        int st = s->state[id];
        if (closed != st) {
            if (++s->cnt[id] >= (closed ? FM1_DEB_PRESS : FM1_DEB_RELEASE)) {
                s->state[id] = (uint8_t)closed;
                s->cnt[id] = 0;
                set_bit(s->down_bits, id, closed);
                set_bit(s->edge_bits, id, 1);
            }
        } else {
            s->cnt[id] = 0;
        }
    }
    if (c == FM1_COLS - 1) {                  /* fin de frame: cuadratura de los 7 encoders */
        for (int e = 0; e < FM1_ENCS; e++) {
            int a = raw_at(s, s->map->enc_a[e][0], s->map->enc_a[e][1]);
            int b = raw_at(s, s->map->enc_b[e][0], s->map->enc_b[e][1]);
            uint8_t ns = (uint8_t)((a << 1) | b);
            int8_t d = QDEC[(s->q[e] << 2) | ns];
            s->q[e] = ns;
            if (d) {
                s->qsub[e] = (int8_t)(s->qsub[e] + d);
                if (s->qsub[e] >= 4 || s->qsub[e] <= -4) {      /* 4 transiciones = 1 detent */
                    s->enc_acc[e] = (int16_t)(s->enc_acc[e] + (s->qsub[e] > 0 ? 1 : -1));
                    s->qsub[e] = 0;
                }
            }
        }
    }
    s->col = (uint8_t)((c + 1) % FM1_COLS);
    return s->col;
}

int fm1_scan_next_edge(fm1_scan *s, int *down)
{
    for (int id = 0; id < FM1_IDS; id++)
        if (get_bit(s->edge_bits, id)) {
            set_bit(s->edge_bits, id, 0);      /* TODO(HW-VERIFY): hacerlo con IRQ apagadas en el port */
            *down = get_bit(s->down_bits, id);
            return id;
        }
    return -1;
}

int16_t fm1_scan_take_encoder(fm1_scan *s, int e)
{
    int16_t v = s->enc_acc[e];                  /* TODO(HW-VERIFY): leer y poner a 0 con IRQ apagadas */
    s->enc_acc[e] = 0;
    return v;
}
