/* File: melee/melee-combat-display.h */
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

/*
 * Combat roll tracking, display, and history.
 * Split from melee1.c for better code organization.
 */

#ifndef INCLUDED_MELEE_COMBAT_DISPLAY_H
#define INCLUDED_MELEE_COMBAT_DISPLAY_H

#include "../angband.h"

typedef struct monster_type monster_type;
typedef struct combat_history_round combat_history_round;

extern combat_roll combat_rolls[2][MAX_COMBAT_ROLLS];
extern int combat_number;
extern int combat_number_old;
extern int turns_since_combat;
extern char combat_roll_special_char;
extern byte combat_roll_special_attr;
extern combat_history_round combat_history[MAX_COMBAT_HISTORY];
extern int combat_history_head;
extern int combat_history_count;

extern void new_combat_round(void);
extern void update_combat_rolls1(const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool vis, int att, int att_roll, int evn,
    int evn_roll);
extern void update_combat_rolls1b(
    const monster_type* m_ptr1, const monster_type* m_ptr2, bool vis);
extern void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps,
    int prot, int prt_percent, int dam_type, bool melee);
extern void refresh_main_combat_overlay(void);
extern void display_main_combat_rolls(void);
extern void clear_main_combat_rolls_area(void);
extern void add_combat_round_to_history(void);
extern void do_cmd_combat_history(void);

#endif /* INCLUDED_MELEE_COMBAT_DISPLAY_H */
