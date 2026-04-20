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

/* project-path.h - projection helpers with optional path masking */

#ifndef INCLUDED_PROJECT_PATH_H
#define INCLUDED_PROJECT_PATH_H

#include "h-basic.h"

typedef struct project_path_mask {
    int y;
    int x;
} project_path_mask;

byte projectable_with_ignore(int y1, int x1, int y2, int x2, u32b flg,
    const project_path_mask* ignore);

#endif /* INCLUDED_PROJECT_PATH_H */
