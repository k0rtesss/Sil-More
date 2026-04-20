/* File: object-desc.h */
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
 * Object description generation.
 * Creates textual descriptions of objects for display.
 */

#ifndef INCLUDED_OBJECT_DESC_H
#define INCLUDED_OBJECT_DESC_H

#include "h-basic.h"

typedef struct object_type object_type;

void strip_name(char* buf, int k_idx);
void object_desc(char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
void object_desc_floor(char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
void object_desc_spoil(char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
void identify_random_gen(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_DESC_H */
