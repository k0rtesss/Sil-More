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

#ifndef INCLUDED_PLAYER_ABILITIES_H
#define INCLUDED_PLAYER_ABILITIES_H

#include "h-basic.h"

typedef struct ability_type ability_type;
typedef struct object_type object_type;

int ability_index(int skilltype, int abilitynum);
bool ability_prereqs_met(int skilltype, int abilitynum);
int abilities_in_skill(int skilltype);
bool prereqs(int skilltype, int abilitynum);
int ability_requirement_level(const ability_type* b_ptr);
bool ability_requirements_currently_met(int skilltype, int abilitynum);
bool ability_requirement_is_suspended(int skilltype, int abilitynum);
bool object_grants_usable_ability(
    const object_type* o_ptr, int skilltype, int abilitynum);
void update_active_ability_requirements(void);
int ability_score(int skilltype, int abilitynum);
bool ability_score_has_custom_weights(int skilltype, int abilitynum);

#endif /* INCLUDED_PLAYER_ABILITIES_H */
