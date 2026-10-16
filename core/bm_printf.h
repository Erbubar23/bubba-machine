/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — snprintf propio (bm_printf.c): el del SDK del FM-1 no entiende %+d ni %f. */
#ifndef BM_PRINTF_H
#define BM_PRINTF_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
int bm_snprintf(char *buf, size_t cap, const char *fmt, ...);
int bm_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);
#ifdef __cplusplus
}
#endif

#endif
