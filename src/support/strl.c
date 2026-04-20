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

#include "support/strl.h"
#include <ctype.h>
#include <string.h>

size_t SDL_strlcpy(char* buf, const char* src, size_t bufsize)
{
    size_t len = strlen(src);
    size_t ret = len;

    if (bufsize == 0)
        return ret;

    if (len >= bufsize)
        len = bufsize - 1;

    memcpy(buf, src, len);
    buf[len] = '\0';

    return ret;
}

size_t SDL_strlcat(char* buf, const char* src, size_t bufsize)
{
    size_t dlen = strlen(buf);

    if (dlen < bufsize - 1)
    {
        return dlen + SDL_strlcpy(buf + dlen, src, bufsize - dlen);
    }

    return dlen + strlen(src);
}

int SDL_strcasecmp(const char* left, const char* right)
{
    unsigned char a;
    unsigned char b;

    if (left == right)
        return 0;
    if (!left)
        return -1;
    if (!right)
        return 1;

    while (*left && *right)
    {
        a = (unsigned char)tolower((unsigned char)*left++);
        b = (unsigned char)tolower((unsigned char)*right++);
        if (a != b)
            return (int)a - (int)b;
    }

    return (int)(unsigned char)tolower((unsigned char)*left)
        - (int)(unsigned char)tolower((unsigned char)*right);
}

int SDL_strncasecmp(const char* left, const char* right, size_t len)
{
    unsigned char a;
    unsigned char b;

    if (left == right || len == 0)
        return 0;
    if (!left)
        return -1;
    if (!right)
        return 1;

    while (len-- > 0)
    {
        a = (unsigned char)tolower((unsigned char)*left++);
        b = (unsigned char)tolower((unsigned char)*right++);
        if (a != b)
            return (int)a - (int)b;
        if (a == '\0')
            return 0;
    }

    return 0;
}
