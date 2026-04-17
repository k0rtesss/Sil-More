/* File: cmd-combat.h */

/*
 * Transitional public header for the combat command split.
 */

#ifndef INCLUDED_CMD_COMBAT_H
#define INCLUDED_CMD_COMBAT_H

#include "h-basic.h"

bool graphics_are_ascii(void);

bool check_hit(int power, bool display_roll);
int hit_roll(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll);
int crit_bonus(int hit_result, int weight, const monster_race* r_ptr,
    int skill_type, bool thrown, monster_type* attacker,
    const object_type* o_ptr);
int total_player_attack(monster_type* m_ptr, int base);
int total_player_evasion(monster_type* m_ptr, bool archery);
int total_monster_attack(monster_type* m_ptr, int base);
int total_monster_evasion(monster_type* m_ptr, bool archery);
int stealth_melee_bonus(const monster_type* m_ptr, bool allow_unseen);
int concentration_bonus(int y, int x);
int focused_attack_bonus(void);
int master_hunter_bonus(monster_type* m_ptr);
bool knock_back(int y1, int x1, int y2, int x2);
bool abort_for_mercy(monster_type* m_ptr);
bool abort_for_valorous(monster_type* m_ptr);
bool cowardly_attack(monster_type* m_ptr);
void break_mercy_oath(monster_type* m_ptr, int damage);
void break_valorous_oath(
    monster_type* m_ptr, int damage, int attack_type, int damage_source);
void attack_punctuation(char* punctuation, int net_dam, int crit_bonus_dice);
void display_hit(int y, int x, int net_dam, int dam_type, bool fatal_blow);
void py_attack_aux(int y, int x, int attack_type);
void hit_trap(int y, int x);
void py_attack(int y, int x, int attack_type);
void flanking_or_retreat(int y, int x);
void apply_oath_breaking_curse(int oath_id);

int skill_check(
    monster_type* m_ptr1, int skill, int difficulty, monster_type* m_ptr2);

#endif /* INCLUDED_CMD_COMBAT_H */
