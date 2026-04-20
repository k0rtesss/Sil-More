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

#ifndef INCLUDED_INIT_INIT_LIFECYCLE_H
#define INCLUDED_INIT_INIT_LIFECYCLE_H

#include "../h-basic.h"

void init_angband(void);
void re_init_some_things(void);
NavResult initial_menu(bool* start_new);
void cleanup_angband(void);

#endif /* INCLUDED_INIT_INIT_LIFECYCLE_H */
