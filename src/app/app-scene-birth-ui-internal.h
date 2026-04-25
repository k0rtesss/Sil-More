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

#ifndef INCLUDED_APP_SCENE_BIRTH_UI_INTERNAL_H
#define INCLUDED_APP_SCENE_BIRTH_UI_INTERNAL_H

#include "angband.h"

typedef struct birth_compact_flag_line {
    cptr txt;
    byte attr;
} birth_compact_flag_line;

int birth_collect_character_trait_lines(int race, int character,
    bool short_labels, birth_compact_flag_line out[], int out_max,
    int* max_line_len);

#endif /* INCLUDED_APP_SCENE_BIRTH_UI_INTERNAL_H */
