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

#ifndef INCLUDED_SUPPORT_STRL_H
#define INCLUDED_SUPPORT_STRL_H

#include <stddef.h>

size_t SDL_strlcpy(char* buf, const char* src, size_t bufsize);
size_t SDL_strlcat(char* buf, const char* src, size_t bufsize);
int SDL_strcasecmp(const char* left, const char* right);
int SDL_strncasecmp(const char* left, const char* right, size_t len);

#endif /* INCLUDED_SUPPORT_STRL_H */
