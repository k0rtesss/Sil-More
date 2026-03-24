#ifndef INCLUDED_PLAYER_IDENTIFICATION_H
#define INCLUDED_PLAYER_IDENTIFICATION_H

#include "h-basic.h"

typedef struct object_type object_type;

bool player_auto_identifies_object(const object_type* o_ptr);
void player_mark_object_experienced(object_type* o_ptr);
bool player_try_identify_smithing_object(
    object_type* o_ptr, bool is_equipped, int bonus);
bool player_auto_identify_smithing_object(
    object_type* o_ptr, bool ignore_distance_penalty);
void player_update_lore(void);

#endif /* INCLUDED_PLAYER_IDENTIFICATION_H */
