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

#ifndef INCLUDED_PLATFORM_IO_H
#define INCLUDED_PLATFORM_IO_H

#include "h-basic.h"

/*
 * Core-facing opaque file-stream handle.
 *
 * The current backend is SDL_IOStream, but core/public headers should not need
 * to include SDL just to mention a stream handle type.
 */
typedef struct SDL_IOStream ang_file;
typedef s64b ang_file_off_t;

#endif /* INCLUDED_PLATFORM_IO_H */
