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

#ifndef INCLUDED_APP_SCENE_BIRTH_H
#define INCLUDED_APP_SCENE_BIRTH_H

#include "angband.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BIRTH_MAX_COST 13
#define BIRTH_STAT_LORE A_MAX
#define BIRTH_STAT_COUNT (A_MAX + 1)

NavResult player_birth(void);
NavResult character_creation(void);
NavResult blitz_character_creation(void);
NavResult gain_skills(void);

void player_wipe(void);

int birth_get_start_xp(void);
int birth_curses_stat_adj(int stat);
int birth_stat_cost(int stat_value);
int birth_skill_cost(int base, int points);
void birth_prepare_character_extra(void);
void birth_finalize_character_creation_selection(void);
void birth_player_outfit(void);

NavResult birth_run_character_creation_menu(void);
NavResult birth_run_stats_allocation(void);
NavResult birth_select_oath(void);

bool birth_assignment_review_pending(void);
void birth_set_assignment_review_pending(bool pending);

NavResult birth_blitz_setup_menu(void);
void birth_blitz_pick_random_race_and_character(void);
NavResult birth_blitz_configure_effects(void);
NavResult birth_blitz_auto_build_character(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SCENE_BIRTH_H */
