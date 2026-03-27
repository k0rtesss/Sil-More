#ifndef INCLUDED_PLATFORM_TIME_H
#define INCLUDED_PLATFORM_TIME_H

#include "h-basic.h"

u64b platform_monotonic_ms(void);
u64b platform_perf_counter(void);
u32b platform_rand_bits(u64b* state);
void platform_delay_ms(u32b msec);

#endif /* INCLUDED_PLATFORM_TIME_H */
