/* File: monster/monster.h */
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
 * Transitional public header for monster utility APIs that still span the
 * legacy monster1/monster2 split.
 */

#ifndef INCLUDED_MONSTER_MONSTER_H
#define INCLUDED_MONSTER_MONSTER_H

#include "h-basic.h"

typedef struct app_ui_scene app_ui_scene;
typedef struct object_type object_type;

void monster_desc(char* desc, size_t max, const monster_type* m_ptr, int mode);
void monster_desc_race(char* desc, size_t max, int r_idx);
void describe_monster(int r_idx, bool spoilers, const monster_type* m_ptr);
s16b poly_r_idx(const monster_type* m_ptr);
void delete_monster(int y, int x);
void delete_monster_idx(int i);
void compact_monsters(int size);
void wipe_mon_list(void);
errr get_mon_num_prep(void);
s16b get_mon_num(int level, bool special, bool allow_non_smart, bool vault);
void lore_probe_aux(int r_idx);
void listen_hint_new_player_turn(void);
bool listen_hint_overlay(int m_idx, byte* a, char* c);
s16b monster_carry(int m_idx, object_type* j_ptr);
int monster_song_hp_loss(const monster_type* m_ptr);
void monster_swap(int y1, int x1, int y2, int x2);
s16b player_place(int y, int x);
s16b monster_place(int y, int x, monster_type* n_ptr);
void calc_monster_speed(int y, int x);
bool monster_race_is_vala(int r_idx);
bool monster_clear_vala_state(monster_type* m_ptr);
void set_monster_haste(s16b m_idx, s16b counter, bool message);
s16b monster_lookup_guid(u64b guid);
s16b monster_lookup_guid_text(const char* text);
bool place_monster_by_guid(
    int y, int x, u64b guid, bool slp, bool ignore_depth,
    monster_type* summoner);
void monster_special_vault_debug_context(
    int* build_vault_type, bool* exact_token);
void log_live_special_vault_only_monsters(const char* reason);
bool monster_special_vault_selection_allowed(void);
bool monster_special_vault_only_allowed_at(int y, int x);
bool place_monster_aux(int y, int x, int r_idx, bool slp, bool grp);
bool place_monster(int y, int x, bool slp, bool grp, bool vault);
bool quest_monster_spawn_okay(int r_idx);
bool alloc_monster(bool on_stairs, bool force_undead);
bool reproduce_monster(int old_m_idx, int new_r_idx);
void lore_treasure(int m_idx, int num_item);
void set_monster_slow(s16b m_idx, s16b counter, bool message);
void message_pain(int m_idx, int dam);
void make_alert(monster_type* m_ptr);
void set_alertness(monster_type* m_ptr, int alertness);
bool place_monster_one(
    int y, int x, int r_idx, bool slp, bool ignore_depth, monster_type* summoner);
void monster_add_song_hp_loss(monster_type* m_ptr, int amount);
int monster_skill(monster_type* m_ptr, int skill_type);
int monster_stat(monster_type* m_ptr, int stat_type);
int monster_base_armour_sides(const monster_type* m_ptr);
bool summon_specific(int y1, int x1, int lev, int type);
bool build_monster_recall_ui_scene(app_ui_scene* scene, int r_idx,
    const monster_type* m_ptr, cptr prompt, bool overlay_dungeon);
bool build_monlist_subwindow_ui_scene(app_ui_scene* scene);
void update_mon(int m_idx, bool full);
void update_monsters(bool full);
bool detect_monster_noise(monster_type* m_ptr, int skill);

#endif /* INCLUDED_MONSTER_MONSTER_H */
