/* File: melee/melee-combat-display.h */

/*
 * Combat roll tracking, display, and history.
 * Split from melee1.c for better code organization.
 */

#ifndef INCLUDED_MELEE_COMBAT_DISPLAY_H
#define INCLUDED_MELEE_COMBAT_DISPLAY_H

#include "../h-basic.h"

typedef struct monster_type monster_type;
typedef struct combat_history_round combat_history_round;

extern void new_combat_round(void);
extern void update_combat_rolls1(const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool vis, int att, int att_roll, int evn,
    int evn_roll);
extern void update_combat_rolls1b(
    const monster_type* m_ptr1, const monster_type* m_ptr2, bool vis);
extern void update_combat_rolls2(int dd, int ds, int dam, int pd, int ps,
    int prot, int prt_percent, int dam_type, bool melee);
extern void display_combat_rolls(void);
extern void display_main_combat_rolls(void);
extern void clear_main_combat_rolls_area(void);
extern void add_combat_round_to_history(void);
extern void do_cmd_combat_history(void);
extern void display_combat_round_details(combat_history_round* round);

#endif /* INCLUDED_MELEE_COMBAT_DISPLAY_H */
