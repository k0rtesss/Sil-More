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

#ifndef INCLUDED_METARUN_PERSISTENCE_H
#define INCLUDED_METARUN_PERSISTENCE_H

#include "../metarun.h"

void reset_defaults(metarun *m);
void apply_difficulty_curses(metarun *m);
void ensure_run_dir(const metarun *m);
bool sync_current_metarun_slot(bool stamp_time);

#endif /* INCLUDED_METARUN_PERSISTENCE_H */
