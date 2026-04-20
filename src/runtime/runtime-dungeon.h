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

#ifndef INCLUDED_RUNTIME_DUNGEON_H
#define INCLUDED_RUNTIME_DUNGEON_H

#include "h-basic.h"

#include <stddef.h>

void clear_active_narrative_banner(void);
bool dungeon_active_narrative_banner_animating(u64b now_ms);
bool dungeon_query_active_narrative_banner(u64b now_ms, char* text,
    size_t text_size, u64b* started_ms, u32b* hold_ms);
bool can_be_pseudo_ided(const object_type* o_ptr);
int value_check_aux1(const object_type* o_ptr);
void death_spectator_view(void);
void land(void);
void pseudo_id_everything(void);
void id_everything(void);
int p_ptr_depth_proxy(void);

#endif /* INCLUDED_RUNTIME_DUNGEON_H */
