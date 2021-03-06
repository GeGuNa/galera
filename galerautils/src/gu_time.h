// Copyright (C) 2008 Codership Oy <info@codership.com>

/**
 * @file time manipulation functions/macros
 *
 * $Id$
 */

#ifndef _gu_time_h_
#define _gu_time_h_

#include <time.h>

/** Returns seconds */
static inline double
gu_timeval_diff (struct timeval* left, struct timeval* right)
{
    register long long diff = left->tv_sec;
    diff = ((diff - right->tv_sec) * 1000000) + left->tv_usec - right->tv_usec;
    return (((double)diff) * 1.0e-06);
}

static const double SEC_PER_CLOCK = ((double)1.0)/CLOCKS_PER_SEC;

/** Returns seconds */
static inline double
gu_clock_diff (clock_t left, clock_t right)
{
    return ((double)(left - right)) * SEC_PER_CLOCK;
}
#endif /* _gu_time_h_ */
