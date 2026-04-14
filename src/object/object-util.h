/* File: object-util.h */

/*
 * Miscellaneous object utility functions.
 */

#ifndef INCLUDED_OBJECT_UTIL_H
#define INCLUDED_OBJECT_UTIL_H

#include "h-basic.h"

typedef struct object_type object_type;

int get_paired_artefact(int art_idx);
bool player_can_treat_as_throwing_flags(const object_type* o_ptr, u32b f3);
bool weapon_is_impale_eligible(const object_type* o_ptr);
bool player_can_treat_as_throwing(const object_type* o_ptr);
bool object_break_brass_lantern(object_type* o_ptr);
bool object_is_fire_broken(const object_type* o_ptr);
bool object_break_shafted_weapon_by_fire(object_type* o_ptr);
bool object_repair_fire_broken_weapon(object_type* o_ptr);

#endif /* INCLUDED_OBJECT_UTIL_H */
