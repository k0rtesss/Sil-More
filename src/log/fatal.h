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

/* log/fatal.h - process termination helpers */

#ifndef INCLUDED_LOG_FATAL_H
#define INCLUDED_LOG_FATAL_H

#include "../h-basic.h"

typedef void (*quit_hook_fn)(cptr message);

void log_register_quit_hook(quit_hook_fn hook);
void plog(cptr str);
void quit(cptr str);
void core(cptr str);

#endif /* INCLUDED_LOG_FATAL_H */
