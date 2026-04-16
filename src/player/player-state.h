#ifndef INCLUDED_PLAYER_PLAYER_STATE_H
#define INCLUDED_PLAYER_PLAYER_STATE_H

#include "h-basic.h"

extern const player_race* rp_ptr;
extern character_profile* current_character_profile;
extern player_other* op_ptr;
extern player_type* p_ptr;

extern s16b stealth_score;
extern bool player_attacked;
extern bool attacked_player;

#endif /* INCLUDED_PLAYER_PLAYER_STATE_H */
