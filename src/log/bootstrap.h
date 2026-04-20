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

/* log/bootstrap.h - Logger initialization */

#ifndef INCLUDED_LOG_BOOTSTRAP_H
#define INCLUDED_LOG_BOOTSTRAP_H

#include "../h-basic.h"

/*
 * Initialises logger. Opens `log.txt` file and sets log level for stdout and
 * file from `SIL_LOG_LEVEL` environment variable. The `quiet` argument disables
 * stdout when set to true (essential for terminal modes like ncurses where
 * screen output would be garbled otherwise).
 */
extern void init_logger(bool quiet, const char* exe_path);

#endif /* INCLUDED_LOG_BOOTSTRAP_H */
