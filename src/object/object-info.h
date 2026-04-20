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

#ifndef INCLUDED_OBJECT_INFO_H
#define INCLUDED_OBJECT_INFO_H

#include "h-basic.h"

typedef struct object_type object_type;

bool object_info_out(const object_type* o_ptr);
void note_info_screen(const object_type* o_ptr);
void object_info_screen(const object_type* o_ptr);
void object_info_screen_multi(const object_type** objects,
    const char** headings, int count);

#endif /* INCLUDED_OBJECT_INFO_H */
