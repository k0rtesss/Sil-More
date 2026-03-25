#include "reliability-checks.h"

#include <string.h>

size_t reliability_clamp_initial_text_len(const char* text, size_t max_size)
{
    size_t len = text ? strlen(text) : 0;

    if (max_size == 0)
        return 0;

    if (len >= max_size)
        return max_size - 1;

    return len;
}

bool reliability_sample_square_point(int center_y, int center_x, int radius,
    int sample_y, int sample_x, int max_hgt, int max_wid, int* out_y,
    int* out_x)
{
    int src_y = center_y - radius + sample_y;
    int src_x = center_x - radius + sample_x;

    if (out_y)
        *out_y = src_y;
    if (out_x)
        *out_x = src_x;

    return (src_y >= 0) && (src_y < max_hgt) && (src_x >= 0)
        && (src_x < max_wid);
}

bool reliability_accept_rle_count(byte count, int* empty_runs,
    int max_empty_runs)
{
    int runs = empty_runs ? *empty_runs : 0;

    if (count != 0)
    {
        if (empty_runs)
            *empty_runs = 0;
        return true;
    }

    runs++;
    if (empty_runs)
        *empty_runs = runs;

    return runs <= max_empty_runs;
}
