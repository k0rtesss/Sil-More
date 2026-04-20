/* File: object-util.h */
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
