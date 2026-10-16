/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — snprintf propio.
 *
 * El snprintf del SDK de JieLi (print/prints en el FM-1) no entiende el signo de %+d ni %f: no
 * consume el argumento y los siguientes quedan corridos. Con "%s  %+d ST  %u MS%s%s" (la línea
 * de un pad en PADS) el último %s recibía un número como puntero, leía la dirección 162 y el
 * procesador fallaba (axi_rd_inv, 0.7 en el aparato). En el FM-1, todo el código de Bubba usa
 * este (hal/fm1/sdk/Makefile: -Dsnprintf=bm_snprintf). En la computadora se compara con el de
 * la biblioteca (tests/test_core.c).
 *
 * Cubre: flags - + espacio 0 #, ancho y precisión (número o *), largos h hh l ll z, y
 * d i u x X o c s p f %. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "bm_printf.h"

typedef struct { char *b; size_t cap, n; } out_t;

static void put(out_t *o, char c)
{
    if (o->n + 1 < o->cap) o->b[o->n] = c;
    o->n++;
}

static void pad(out_t *o, char c, int k) { while (k-- > 0) put(o, c); }

/* el número ya convertido (dígitos al revés en d[0..nd)), con signo, prefijo, precisión y ancho */
static void emit_num(out_t *o, const char *d, int nd, char sign, const char *pre, int left, int zero, int width, int prec)
{
    int pl = 0;
    while (pre && pre[pl]) pl++;
    int digits = nd, zeros = prec > digits ? prec - digits : 0;
    int len = (sign ? 1 : 0) + pl + zeros + digits;
    if (!left && !zero) pad(o, ' ', width - len);
    if (sign) put(o, sign);
    for (int i = 0; i < pl; i++) put(o, pre[i]);
    if (!left && zero) pad(o, '0', width - len);
    pad(o, '0', zeros);
    while (nd) put(o, d[--nd]);
    if (left) pad(o, ' ', width - len);
}

static int utoa_rev(unsigned long long v, unsigned base, int upper, char *d)
{
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do { d[n++] = dig[v % base]; v /= base; } while (v);
    return n;
}

int bm_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    out_t o = {buf, cap, 0};
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') { put(&o, *f); continue; }
        f++;
        int left = 0, plus = 0, space = 0, zero = 0, alt = 0;
        for (;; f++) {
            if (*f == '-') left = 1;
            else if (*f == '+') plus = 1;
            else if (*f == ' ') space = 1;
            else if (*f == '0') zero = 1;
            else if (*f == '#') alt = 1;
            else break;
        }
        int width = 0, prec = -1;
        if (*f == '*') { width = va_arg(ap, int); if (width < 0) { left = 1; width = -width; } f++; }
        else while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        if (*f == '.') {
            f++;
            prec = 0;
            if (*f == '*') { prec = va_arg(ap, int); f++; }
            else while (*f >= '0' && *f <= '9') prec = prec * 10 + (*f++ - '0');
        }
        int lng = 0;                                     /* 0 int, 1 long, 2 long long, -1 short */
        for (;; f++) {
            if (*f == 'l') lng++;
            else if (*f == 'h') lng = -1;
            else if (*f == 'z') lng = 1;
            else break;
        }
        char d[24];
        switch (*f) {
        case 'd': case 'i': {
            long long v = lng >= 2 ? va_arg(ap, long long) : lng == 1 ? va_arg(ap, long) : va_arg(ap, int);
            unsigned long long u = v < 0 ? (unsigned long long)(-(v + 1)) + 1u : (unsigned long long)v;
            int nd = (prec == 0 && v == 0) ? 0 : utoa_rev(u, 10, 0, d);
            emit_num(&o, d, nd, v < 0 ? '-' : plus ? '+' : space ? ' ' : 0, NULL, left, zero && prec < 0, width, prec);
            break;
        }
        case 'u': case 'x': case 'X': case 'o': {
            unsigned long long v = lng >= 2 ? va_arg(ap, unsigned long long) : lng == 1 ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            unsigned base = *f == 'u' ? 10u : *f == 'o' ? 8u : 16u;
            int nd = (prec == 0 && v == 0) ? 0 : utoa_rev(v, base, *f == 'X', d);
            const char *pre = alt && v && base == 16 ? (*f == 'X' ? "0X" : "0x") : alt && base == 8 ? "0" : NULL;
            emit_num(&o, d, nd, 0, pre, left, zero && prec < 0, width, prec);
            break;
        }
        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            int nd = utoa_rev(v, 16, 0, d);
            emit_num(&o, d, nd, 0, "0x", left, 0, width, -1);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (!left) pad(&o, ' ', width - 1);
            put(&o, c);
            if (left) pad(&o, ' ', width - 1);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int n = 0;
            while (s[n] && (prec < 0 || n < prec)) n++;
            if (!left) pad(&o, ' ', width - n);
            for (int i = 0; i < n; i++) put(&o, s[i]);
            if (left) pad(&o, ' ', width - n);
            break;
        }
        case 'f': case 'F': {
            double v = va_arg(ap, double);
            if (prec < 0) prec = 6;
            if (prec > 9) prec = 9;
            char sign = v < 0 ? '-' : plus ? '+' : space ? ' ' : 0;
            if (v < 0) v = -v;
            double scale = 1.0;
            for (int i = 0; i < prec; i++) scale *= 10.0;
            double x = v * scale;
            unsigned long long all = (unsigned long long)x;
            double fr = x - (double)all;
            if (fr > 0.5 || (fr == 0.5 && (all & 1u))) all++;      /* la mitad exacta, al par (como glibc) */
            unsigned long long ip = all / (unsigned long long)scale, fp = all % (unsigned long long)scale;
            int nd = 0;
            for (int i = 0; i < prec; i++) { d[nd++] = (char)('0' + fp % 10u); fp /= 10u; }
            if (prec || alt) d[nd++] = '.';
            nd += utoa_rev(ip, 10, 0, d + nd);
            emit_num(&o, d, nd, sign, NULL, left, zero, width, -1);
            break;
        }
        case '%': put(&o, '%'); break;
        case 0: f--; break;                              /* un % al final: nada */
        default: put(&o, '%'); put(&o, *f); break;       /* desconocido: tal cual, sin tocar los argumentos */
        }
    }
    if (cap) buf[o.n < cap ? o.n : cap - 1] = 0;
    return (int)o.n;
}

int bm_snprintf(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = bm_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}
