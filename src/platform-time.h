/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#ifndef INCLUDED_PLATFORM_TIME_H
#define INCLUDED_PLATFORM_TIME_H

#include "h-basic.h"

u64b platform_monotonic_ms(void);
u64b platform_perf_counter(void);
u32b platform_rand_bits(u64b* state);
void platform_delay_ms(u32b msec);

#endif /* INCLUDED_PLATFORM_TIME_H */
