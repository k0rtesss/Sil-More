/* File: object-use.h */
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

#ifndef INCLUDED_OBJECT_USE_H
#define INCLUDED_OBJECT_USE_H

#include "../h-basic.h"

int consumable_healing_points(const object_type* o_ptr);
bool use_object(object_type* o_ptr, bool* ident);
bool use_sanctity_gem_on(object_type* target_o_ptr, bool* ident);

#endif /* INCLUDED_OBJECT_USE_H */
