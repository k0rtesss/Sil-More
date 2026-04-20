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

#include "platform-time.h"

#include <SDL3/SDL.h>

u64b platform_monotonic_ms(void)
{
    return (u64b)SDL_GetTicks();
}

u64b platform_perf_counter(void)
{
    return (u64b)SDL_GetPerformanceCounter();
}

u32b platform_rand_bits(u64b* state)
{
    return (u32b)SDL_rand_bits_r(state);
}

void platform_delay_ms(u32b msec)
{
    SDL_Delay(msec);
}
