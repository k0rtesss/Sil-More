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

#ifndef INCLUDED_SUPPORT_GAME_TABLES_H
#define INCLUDED_SUPPORT_GAME_TABLES_H

#include "h-basic.h"

extern const s16b ddd[9];
extern const s16b ddx[10];
extern const s16b ddy[10];
extern const s16b ddx_ddd[9];
extern const s16b ddy_ddd[9];
extern const char hexsym[16];
extern const byte extract_energy[8];
extern const byte chest_traps[25 + 1];
extern byte get_angle_to_grid[41][41];
extern cptr color_names[16];
extern cptr stat_names[A_MAX];
extern cptr stat_names_reduced[A_MAX];
extern cptr stat_names_full[A_MAX];
extern cptr skill_names[S_MAX];
extern cptr skill_names_full[S_MAX];
extern cptr window_flag_desc[32];
extern cptr option_text[OPT_MAX];
extern cptr option_desc[OPT_MAX];
extern const bool option_norm[OPT_MAX];
extern const byte option_page[OPT_PAGE_MAX][OPT_PAGE_PER];
extern cptr inscrip_text[MAX_INSCRIP];
extern byte spell_info_RF4[32][3];
extern byte spell_desire_RF4[32][2];

#endif /* INCLUDED_SUPPORT_GAME_TABLES_H */
