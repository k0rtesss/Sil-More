/* File: object-flags.h */
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
 * Object flag accessor functions.
 * Provides access to an item's flags from base kind, artefact, and ego sources.
 */

#ifndef INCLUDED_OBJECT_FLAGS_H
#define INCLUDED_OBJECT_FLAGS_H

#include "h-basic.h"

typedef struct object_type object_type;

void object_flags(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
void object_flags4(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4);
void object_flags_known(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
void object_flags_known4(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4);
bool object_grants_ability(const object_type* o_ptr, int skilltype, int abilitynum);

#endif /* INCLUDED_OBJECT_FLAGS_H */
