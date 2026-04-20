/* File: smithing.h */
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
 * Transitional public API for the smithing split.
 *
 * Smithing logic still lives in cmd4.c on this branch. This header gives the
 * future smithing modules a stable place to own that surface.
 */

#ifndef INCLUDED_SMITHING_H
#define INCLUDED_SMITHING_H

#include "h-basic.h"

typedef struct object_type object_type;

extern object_type* smith_o_ptr;
extern int object_difficulty(object_type* o_ptr);
extern void create_smithing_item(void);
bool is_smithed_by_player(const object_type* o_ptr);

#endif /* INCLUDED_SMITHING_H */
