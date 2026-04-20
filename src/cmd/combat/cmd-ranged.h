/* File: cmd-ranged.h */
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

#ifndef INCLUDED_CMD_RANGED_H
#define INCLUDED_CMD_RANGED_H

#include "h-basic.h"

int archery_range(const object_type* j_ptr);
int throwing_range(const object_type* i_ptr);
void attacks_of_opportunity(int neutralized_y, int neutralized_x);
void do_cmd_fire(int quiver);
void do_cmd_throw(bool automatic);

#endif /* INCLUDED_CMD_RANGED_H */
