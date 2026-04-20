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

#ifndef INCLUDED_SCORE_LOGIC_H
#define INCLUDED_SCORE_LOGIC_H

#include "h-basic.h"

struct high_score;
typedef struct high_score high_score;

int parse_score_int(const char* field, size_t field_len, int fallback);
void parse_score_string(const char* field, size_t field_len,
                        char* out, size_t out_len);

int score_points(const high_score* score);
int score_compare(const high_score* a, const high_score* b);

#endif /* INCLUDED_SCORE_LOGIC_H */
