/*
 * Copyright (C) 1996-2024 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#define _etext etext

#include "squid.h"
#include "util.h"

#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_MATH_H
#include <math.h>
#endif

void
Tolower(char *q)
{
    char *s = q;

    while (*s) {
        *s = xtolower(*s);
        s++;
    }
}

/* somewhat safer calculation of %s */
double
xpercent(double part, double whole)
{
    return xdiv(100 * part, whole);
}

int
xpercentInt(double part, double whole)
{
    return (int) rint(xpercent(part, whole));
}

/* somewhat safer division */
double
xdiv(double nom, double denom)
{
    return (denom != 0.0) ? nom / denom : -1.0;
}

/* integer to string */
const char *
xitoa(int num)
{
    static char buf[24];    /* 2^64 = 18446744073709551616 */
    snprintf(buf, sizeof(buf), "%d", num);
    return buf;
}

/* int64_t to string */
const char *
xint64toa(int64_t num)
{
    static char buf[24];    /* 2^64 = 18446744073709551616 */
    snprintf(buf, sizeof(buf), "%" PRId64, num);
    return buf;
}

void
gb_flush(gb_t * g)
{
    g->gb += (g->bytes >> 30);
    g->bytes &= (1 << 30) - 1;
}

double
gb_to_double(const gb_t * g)
{
    return ((double) g->gb) * ((double) (1 << 30)) + ((double) g->bytes);
}

const char *
double_to_str(char *buf, int buf_size, double value)
{
    /* select format */

    if (value < 1e9)
        snprintf(buf, buf_size, "%.2f MB", value / 1e6);
    else if (value < 1e12)
        snprintf(buf, buf_size, "%.3f GB", value / 1e9);
    else
        snprintf(buf, buf_size, "%.4f TB", value / 1e12);

    return buf;
}

const char *
gb_to_str(const gb_t * g)
{
    /*
     * it is often convenient to call gb_to_str several times for _one_ printf
     */
#define max_cc_calls 5
    typedef char GbBuf[32];
    static GbBuf bufs[max_cc_calls];
    static int call_id = 0;
    double value = gb_to_double(g);
    char *buf = bufs[call_id++];

    if (call_id >= max_cc_calls)
        call_id = 0;

    /* select format */
    if (value < 1e9)
        snprintf(buf, sizeof(GbBuf), "%.2f MB", value / 1e6);
    else if (value < 1e12)
        snprintf(buf, sizeof(GbBuf), "%.2f GB", value / 1e9);
    else
        snprintf(buf, sizeof(GbBuf), "%.2f TB", value / 1e12);

    return buf;
}

/**
 * rounds num to the next upper integer multiple of what
 */
unsigned int RoundTo(const unsigned int num, const unsigned int what)
{
    return what * ((num + what -1)/what);
}

