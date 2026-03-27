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
