/* File: object-internal.h */
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
 * Internal scaffolding for the object subsystem split.
 * Shared object helper declarations move here as object1.c/object2.c shrink.
 */

#ifndef INCLUDED_OBJECT_INTERNAL_H
#define INCLUDED_OBJECT_INTERNAL_H

#include "h-basic.h"

typedef struct object_type object_type;

#define OBJECT_FLAGS_FULL 1
#define OBJECT_FLAGS_KNOWN 2

#define ENHANCED_MAX_LIST 80

#endif /* INCLUDED_OBJECT_INTERNAL_H */
