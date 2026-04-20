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

#include "angband.h"
#include "support/reliability-checks.h"

#include <stdint.h>
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

static bool reliability_add_size(size_t* total, size_t value)
{
    if (!total)
        return false;

    if (SIZE_MAX - *total < value)
        return false;

    *total += value;
    return true;
}

reliability_layout_status reliability_validate_serialized_layout(
    size_t file_size, size_t head_size, size_t info_size, size_t name_size,
    size_t text_size, size_t* expected_size)
{
    size_t expected = 0;

    if (!reliability_add_size(&expected, head_size)
        || !reliability_add_size(&expected, info_size)
        || !reliability_add_size(&expected, name_size)
        || !reliability_add_size(&expected, text_size))
    {
        if (expected_size)
            *expected_size = 0;
        return RELIABILITY_LAYOUT_OVERFLOW;
    }

    if (expected_size)
        *expected_size = expected;

    if (file_size < expected)
        return RELIABILITY_LAYOUT_TRUNCATED;
    if (file_size > expected)
        return RELIABILITY_LAYOUT_TRAILING_BYTES;

    return RELIABILITY_LAYOUT_VALID;
}

reliability_metarun_layout reliability_detect_metarun_layout(
    size_t file_size, size_t header_size, u32b entry_count,
    size_t current_entry_size, size_t legacy_v10_size, size_t legacy_v9_size,
    size_t legacy_v8_size, size_t* payload_size, size_t* entry_size)
{
    size_t payload;
    size_t stride;

    if (payload_size)
        *payload_size = 0;
    if (entry_size)
        *entry_size = 0;

    if (entry_count == 0 || file_size < header_size)
        return RELIABILITY_METARUN_LAYOUT_INVALID;

    payload = file_size - header_size;
    if (payload_size)
        *payload_size = payload;

    if ((payload % (size_t)entry_count) != 0)
        return RELIABILITY_METARUN_LAYOUT_INVALID;

    stride = payload / (size_t)entry_count;
    if (entry_size)
        *entry_size = stride;

    if (stride == current_entry_size)
        return RELIABILITY_METARUN_LAYOUT_CURRENT;
    if (stride == legacy_v10_size)
        return RELIABILITY_METARUN_LAYOUT_V10;
    if (stride == legacy_v9_size)
        return RELIABILITY_METARUN_LAYOUT_V9;
    if (stride == legacy_v8_size)
        return RELIABILITY_METARUN_LAYOUT_V8;

    return RELIABILITY_METARUN_LAYOUT_INVALID;
}

bool reliability_should_update_runs_db(bool ranked_run, bool legacy_write_ok)
{
    return legacy_write_ok || !ranked_run;
}

int reliability_smithing_phase01_flag_delta(u32b flags2, u32b flags3,
    u32b flags4)
{
    int delta = 0;

    if (flags4 & TR4_SUBTLETY_THROW)
        delta += 15;
    if (flags3 & TR3_OATH_BOOST)
        delta += 5;
    if (flags3 & TR3_OATH_NEGATE)
        delta -= 5;
    if (flags2 & TR2_TRAITOR)
        delta -= 2;

    return delta;
}
