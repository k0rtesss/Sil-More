#ifndef INCLUDED_RELIABILITY_CHECKS_H
#define INCLUDED_RELIABILITY_CHECKS_H

#include "h-basic.h"

size_t reliability_clamp_initial_text_len(const char* text, size_t max_size);
bool reliability_sample_square_point(int center_y, int center_x, int radius,
    int sample_y, int sample_x, int max_hgt, int max_wid, int* out_y,
    int* out_x);
bool reliability_accept_rle_count(byte count, int* empty_runs,
    int max_empty_runs);

#endif /* INCLUDED_RELIABILITY_CHECKS_H */
