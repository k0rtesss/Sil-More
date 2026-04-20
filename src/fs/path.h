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

#ifndef INCLUDED_FS_PATH_H
#define INCLUDED_FS_PATH_H

#include "h-basic.h"

/*
 * SDL-backed path helpers.
 *
 * These replace the legacy SET_UID + tmpnam() logic with modern routines that
 * normalize separators, expand "~/" against the user folder, and generate
 * per-user temporary files inside SDL's pref-path tree.
 */
extern bool path_parse(char* buf, size_t max, cptr file);
extern bool path_build(char* buf, size_t max, cptr path, cptr file);
extern bool path_temp(char* buf, size_t max);
extern bool fd_kill(cptr file);
extern bool fd_move(cptr file, cptr what);
extern bool fd_copy(cptr file, cptr what);

#endif /* INCLUDED_FS_PATH_H */
