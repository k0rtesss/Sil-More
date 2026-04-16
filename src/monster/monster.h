/* File: monster/monster.h */

/*
 * Transitional public header for monster utility APIs that still span the
 * legacy monster1/monster2 split.
 */

#ifndef INCLUDED_MONSTER_MONSTER_H
#define INCLUDED_MONSTER_MONSTER_H

#include "h-basic.h"

typedef struct app_ui_scene app_ui_scene;

void monster_desc(char* desc, size_t max, const monster_type* m_ptr, int mode);
void monster_desc_race(char* desc, size_t max, int r_idx);
void delete_monster_idx(int i);
void make_alert(monster_type* m_ptr);
void set_alertness(monster_type* m_ptr, int alertness);
bool place_monster_one(
    int y, int x, int r_idx, bool slp, bool ignore_depth, monster_type* summoner);
void monster_add_song_hp_loss(monster_type* m_ptr, int amount);
int monster_skill(monster_type* m_ptr, int skill_type);
int monster_stat(monster_type* m_ptr, int stat_type);
bool build_monster_recall_ui_scene(app_ui_scene* scene, int r_idx,
    const monster_type* m_ptr, cptr prompt, bool overlay_dungeon);
bool build_monlist_subwindow_ui_scene(app_ui_scene* scene);
void update_mon(int m_idx, bool full);
void update_monsters(bool full);
bool detect_monster_noise(monster_type* m_ptr, int skill);

#endif /* INCLUDED_MONSTER_MONSTER_H */
