/* File: cmd-pickup.h */
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

#ifndef INCLUDED_CMD_PICKUP_H
#define INCLUDED_CMD_PICKUP_H

#include "h-basic.h"

void give_player_item(object_type* o_ptr);
void py_pickup_aux(int o_idx);
void do_cmd_pickup_from_pile(void);

#endif /* INCLUDED_CMD_PICKUP_H */
