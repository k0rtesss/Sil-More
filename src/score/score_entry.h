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

#ifndef INCLUDED_SCORE_ENTRY_H
#define INCLUDED_SCORE_ENTRY_H

#include "h-basic.h"

struct high_score;

bool highscore_is_empty(void);
errr create_score(struct high_score* the_score);
bool build_live_preview_score(struct high_score* out);
bool score_entry_is_ranked_run(void);
errr score_entry_submit(struct high_score* the_score);
const char* kinslayer_try_kill(uint8_t n_sils, bool do_roll);

#endif /* INCLUDED_SCORE_ENTRY_H */
