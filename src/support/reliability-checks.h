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

#ifndef INCLUDED_RELIABILITY_CHECKS_H
#define INCLUDED_RELIABILITY_CHECKS_H

#include "h-basic.h"

typedef enum reliability_layout_status
{
    RELIABILITY_LAYOUT_VALID = 0,
    RELIABILITY_LAYOUT_TRUNCATED,
    RELIABILITY_LAYOUT_TRAILING_BYTES,
    RELIABILITY_LAYOUT_OVERFLOW
} reliability_layout_status;

typedef enum reliability_metarun_layout
{
    RELIABILITY_METARUN_LAYOUT_INVALID = 0,
    RELIABILITY_METARUN_LAYOUT_CURRENT,
    RELIABILITY_METARUN_LAYOUT_V10,
    RELIABILITY_METARUN_LAYOUT_V9,
    RELIABILITY_METARUN_LAYOUT_V8
} reliability_metarun_layout;

size_t reliability_clamp_initial_text_len(const char* text, size_t max_size);
bool reliability_sample_square_point(int center_y, int center_x, int radius,
    int sample_y, int sample_x, int max_hgt, int max_wid, int* out_y,
    int* out_x);
bool reliability_accept_rle_count(byte count, int* empty_runs,
    int max_empty_runs);
reliability_layout_status reliability_validate_serialized_layout(
    size_t file_size, size_t head_size, size_t info_size, size_t name_size,
    size_t text_size, size_t* expected_size);
reliability_metarun_layout reliability_detect_metarun_layout(
    size_t file_size, size_t header_size, u32b entry_count,
    size_t current_entry_size, size_t legacy_v10_size, size_t legacy_v9_size,
    size_t legacy_v8_size, size_t* payload_size, size_t* entry_size);
bool reliability_should_update_runs_db(bool ranked_run, bool legacy_write_ok);
int reliability_smithing_phase01_flag_delta(u32b flags2, u32b flags3,
    u32b flags4);

#endif /* INCLUDED_RELIABILITY_CHECKS_H */
