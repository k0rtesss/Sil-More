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

#include "angband.h"
#include "support/utf8.h"

static bool utf8_is_continuation(unsigned char c)
{
    return (c & 0xC0u) == 0x80u;
}

static int utf8_sequence_len_from_lead(unsigned char c)
{
    if (c < 0x80u)
        return 1;
    if (c >= 0xC2u && c <= 0xDFu)
        return 2;
    if (c >= 0xE0u && c <= 0xEFu)
        return 3;
    if (c >= 0xF0u && c <= 0xF4u)
        return 4;
    return 1;
}

size_t utf8_char_len(cptr s)
{
    unsigned char c;
    int len;

    if (!s || !s[0])
        return 0;

    c = (unsigned char)s[0];
    len = utf8_sequence_len_from_lead(c);
    if (len == 1)
        return 1;

    for (int i = 1; i < len; i++)
    {
        if (!s[i] || !utf8_is_continuation((unsigned char)s[i]))
            return 1;
    }

    return (size_t)len;
}

static bool utf8_decode_one(cptr s, size_t len, u32b* out_codepoint)
{
    const unsigned char* p = (const unsigned char*)s;
    u32b cp;

    if (!s || len == 0)
        return false;

    if (len == 1)
    {
        if (out_codepoint)
            *out_codepoint = p[0];
        return p[0] < 0x80u;
    }

    if (len == 2)
    {
        cp = ((u32b)(p[0] & 0x1Fu) << 6) | (u32b)(p[1] & 0x3Fu);
    }
    else if (len == 3)
    {
        cp = ((u32b)(p[0] & 0x0Fu) << 12)
            | ((u32b)(p[1] & 0x3Fu) << 6)
            | (u32b)(p[2] & 0x3Fu);
    }
    else if (len == 4)
    {
        cp = ((u32b)(p[0] & 0x07u) << 18)
            | ((u32b)(p[1] & 0x3Fu) << 12)
            | ((u32b)(p[2] & 0x3Fu) << 6)
            | (u32b)(p[3] & 0x3Fu);
    }
    else
    {
        return false;
    }

    if (out_codepoint)
        *out_codepoint = cp;
    return true;
}

bool utf8_next_codepoint(cptr* cursor, u32b* out_codepoint)
{
    cptr s;
    size_t len;
    u32b cp = 0;

    if (!cursor || !*cursor || !**cursor)
        return false;

    s = *cursor;
    len = utf8_char_len(s);
    if (len == 0)
        return false;

    if (!utf8_decode_one(s, len, &cp))
        cp = (unsigned char)s[0];

    *cursor += len;
    if (out_codepoint)
        *out_codepoint = cp;
    return true;
}

static int utf8_codepoint_width(u32b codepoint)
{
    if (codepoint == '\n' || codepoint == '\r')
        return 0;
    if (codepoint >= 0x0300u && codepoint <= 0x036Fu)
        return 0;
    return 1;
}

size_t utf8_clip_bytes(cptr s, size_t max_bytes)
{
    size_t i = 0;

    if (!s)
        return 0;

    while (s[i] && i < max_bytes)
    {
        size_t len = utf8_char_len(s + i);

        if (len == 0 || i + len > max_bytes)
            break;
        i += len;
    }

    return i;
}

size_t utf8_clip_cells(cptr s, size_t max_cells, size_t max_bytes)
{
    size_t bytes = 0;
    size_t cells = 0;

    if (!s)
        return 0;

    while (s[bytes] && bytes < max_bytes)
    {
        size_t len = utf8_char_len(s + bytes);
        u32b cp = 0;
        int width;

        if (len == 0 || bytes + len > max_bytes)
            break;
        if (!utf8_decode_one(s + bytes, len, &cp))
            cp = (unsigned char)s[bytes];
        width = utf8_codepoint_width(cp);
        if (width > 0 && cells + (size_t)width > max_cells)
            break;
        cells += (size_t)width;
        bytes += len;
    }

    return bytes;
}

size_t utf8_strlcpy(char* dst, cptr src, size_t dst_size)
{
    size_t src_len;
    size_t copy_len;

    if (!src)
        src = "";

    src_len = strlen(src);
    if (!dst || dst_size == 0)
        return src_len;

    copy_len = utf8_clip_bytes(src, dst_size - 1u);
    if (copy_len > 0)
        memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
    return src_len;
}

size_t utf8_strnlen_cells(cptr s, size_t max_bytes)
{
    size_t bytes = 0;
    size_t cells = 0;

    if (!s)
        return 0;

    while (s[bytes] && bytes < max_bytes)
    {
        size_t len = utf8_char_len(s + bytes);
        u32b cp = 0;

        if (len == 0 || bytes + len > max_bytes)
            break;
        if (!utf8_decode_one(s + bytes, len, &cp))
            cp = (unsigned char)s[bytes];
        cells += (size_t)utf8_codepoint_width(cp);
        bytes += len;
    }

    return cells;
}

size_t utf8_strlen_cells(cptr s)
{
    return utf8_strnlen_cells(s, s ? strlen(s) : 0);
}

int utf8_latin1_fold_codepoint(u32b codepoint)
{
    if (codepoint >= 'A' && codepoint <= 'Z')
        return (int)(codepoint - 'A' + 'a');
    if (codepoint >= 'a' && codepoint <= 'z')
        return (int)codepoint;

    switch (codepoint)
    {
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3:
    case 0x00C4: case 0x00C5: case 0x00E0: case 0x00E1:
    case 0x00E2: case 0x00E3: case 0x00E4: case 0x00E5:
    case 0x0100: case 0x0101:
        return 'a';
    case 0x00C6: case 0x00E6:
        return 'a';
    case 0x00C7: case 0x00E7:
        return 'c';
    case 0x00D0: case 0x00F0:
        return 'd';
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
    case 0x0112: case 0x0113:
        return 'e';
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
    case 0x012A: case 0x012B:
        return 'i';
    case 0x00D1: case 0x00F1:
        return 'n';
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5:
    case 0x00D6: case 0x00D8: case 0x00F2: case 0x00F3:
    case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F8:
    case 0x014C: case 0x014D:
        return 'o';
    case 0x00DE: case 0x00FE:
        return 'p';
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
    case 0x016A: case 0x016B:
        return 'u';
    case 0x00DD: case 0x00FD: case 0x00FF:
        return 'y';
    default:
        return -1;
    }
}
