/* File: object-flavor.h */
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
 * Object flavor initialization.
 * Assigns random colors/flavors to potions, scrolls, rings, etc.
 */

#ifndef INCLUDED_OBJECT_FLAVOR_H
#define INCLUDED_OBJECT_FLAVOR_H

#include "h-basic.h"

bool easter_time(void);
void flavor_init(void);

#endif /* INCLUDED_OBJECT_FLAVOR_H */
