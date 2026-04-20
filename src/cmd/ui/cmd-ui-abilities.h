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

#ifndef INCLUDED_CMD_UI_ABILITIES_H
#define INCLUDED_CMD_UI_ABILITIES_H

/*
 * Transitional staging header for the Wave 7A abilities continuation lane.
 * Keeps the lane-owned command surface out of the broader cmd-ui umbrella.
 */

#include "h-basic.h"

typedef struct object_type object_type;

void do_cmd_change_song(void);
void do_cmd_ability_screen(void);
void add_random_curse(object_type* o_ptr);

#endif /* INCLUDED_CMD_UI_ABILITIES_H */
