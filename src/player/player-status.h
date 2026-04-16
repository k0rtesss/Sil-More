#ifndef INCLUDED_PLAYER_STATUS_H
#define INCLUDED_PLAYER_STATUS_H

#include "h-basic.h"

typedef struct monster_type monster_type;

int health_level(int current, int max);
bool get_alertness_text(monster_type* m_ptr, int text_size, char* text,
    int* color);
byte health_attr(int current, int max);

bool set_blind(int v);
bool allow_player_confusion(monster_type* m_ptr);
bool set_confused(int v);
bool set_poisoned(int v);
bool set_afraid(int v);
bool allow_player_entrancement(monster_type* m_ptr);
bool set_entranced(int v);
bool allow_player_image(monster_type* m_ptr);
bool set_image(int v);
bool set_fast(int v);
bool set_slow(int v);
bool set_shield(int v);
bool set_blessed(int v);
bool set_hero(int v);
bool set_rage(int v);
bool set_tmp_str(int v);
bool set_tmp_dex(int v);
bool set_tmp_con(int v);
bool set_tmp_gra(int v);
bool set_protevil(int v);
bool set_tmp_per(int v);
bool set_tim_invis(int v);
bool set_darkened(int v);
bool set_oppose_fire(int v);
bool set_oppose_cold(int v);
bool set_oppose_pois(int v);
bool allow_player_stun(monster_type* m_ptr);
bool set_stun(int v);
bool set_cut(int v);
bool set_food(int v);

#endif /* INCLUDED_PLAYER_STATUS_H */
