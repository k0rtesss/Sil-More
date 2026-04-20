/* File: object-ui-identify.h */
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
 * Unified identify menu helpers.
 */

#ifndef INCLUDED_OBJECT_UI_IDENTIFY_H
#define INCLUDED_OBJECT_UI_IDENTIFY_H

#include "../h-basic.h"

typedef struct object_type object_type;

bool display_unified_identify_menu(bool include_floor, int* out_item,
    object_type** out_object);

#endif /* INCLUDED_OBJECT_UI_IDENTIFY_H */
