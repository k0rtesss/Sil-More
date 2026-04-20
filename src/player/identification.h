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

#ifndef INCLUDED_PLAYER_IDENTIFICATION_H
#define INCLUDED_PLAYER_IDENTIFICATION_H

#include "h-basic.h"

typedef struct object_type object_type;

bool player_auto_identifies_object(const object_type* o_ptr);
void player_mark_object_experienced(object_type* o_ptr);
bool player_try_identify_smithing_object(
    object_type* o_ptr, bool is_equipped, int bonus);
bool player_try_identify_smithing_object_on_examine(
    object_type* o_ptr, bool is_equipped);
bool player_auto_identify_smithing_object(
    object_type* o_ptr, bool ignore_distance_penalty);
void player_update_lore(void);

#endif /* INCLUDED_PLAYER_IDENTIFICATION_H */
