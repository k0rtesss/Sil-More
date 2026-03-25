/* File: spell/spell-utility.h */

/*
 * Utility spells: healing, curses, identification, self-knowledge, and hooks.
 */

#ifndef INCLUDED_SPELL_UTILITY_H
#define INCLUDED_SPELL_UTILITY_H

#include "../h-basic.h"

typedef struct object_type object_type;
typedef struct monster_type monster_type;

bool hp_player(int x, bool percent, bool message);
void warding_glyph(void);
bool do_dec_stat(int stat, monster_type* m_ptr);
bool do_res_stat(int stat, int points);
bool do_inc_stat(int stat);

void identify_pack(void);
void uncurse_object(object_type* o_ptr);
bool remove_curse(bool star_curse);
void self_knowledge(void);
void analyze_weapon_properties(int* count, char s[][200], char t[][200],
    bool good[], bool identify[], int slot, const char* weapon_name);
void display_attributes(char s[][200], char t[][200], bool good[], int count);
void identify_revealed_items(bool identify[]);

bool item_tester_hook_digger(const object_type* o_ptr);
bool item_tester_hook_ided_weapon(const object_type* o_ptr);
bool item_tester_hook_weapon(const object_type* o_ptr);
bool item_tester_hook_wieldable_ided_weapon(const object_type* o_ptr);
bool item_tester_hook_wieldable_weapon(const object_type* o_ptr);
bool item_tester_hook_ided_armour(const object_type* o_ptr);
bool item_tester_hook_armour(const object_type* o_ptr);
bool item_tester_hook_non_herb_food(const object_type* o_ptr);
bool item_tester_hook_light_with_fuel(const object_type* o_ptr);
bool item_tester_hook_enchantable_amulet(const object_type* o_ptr);
bool item_tester_hook_recharge(const object_type* o_ptr);
bool item_tester_hook_ided_ammo(const object_type* o_ptr);
bool item_tester_hook_ammo(const object_type* o_ptr);
bool item_tester_hook_ordinary_ammo(const object_type* o_ptr);

int do_ident_item(int item, object_type* o_ptr);
bool ident_spell(bool include_floor);
bool recharge(int num);
void identify_and_squelch_pack(void);
bool mass_identify(int rad);

#endif /* INCLUDED_SPELL_UTILITY_H */
