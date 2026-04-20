/* File: init-parse-terrain.c */
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
#include "init.h"
#include "init/init-parse-internal.h"

#ifdef ALLOW_TEMPLATES

errr parse_f_info(char* buf, header* head)
{
    int i;
    char* s;
    static feature_type* f_ptr = NULL;

    if (buf[0] == 'N')
    {
        s = strchr(buf + 2, ':');
        if (!s)
            return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        if (!*s)
            return PARSE_ERROR_GENERIC;

        i = atoi(buf + 2);
        if (i <= error_idx)
            return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (i >= head->info_num)
            return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = i;

        f_ptr = (feature_type*)head->info_ptr + i;
        if (!(f_ptr->name = add_name(head, s)))
            return PARSE_ERROR_OUT_OF_MEMORY;

        f_ptr->mimic = i;
    }
    else if (buf[0] == 'M')
    {
        int mimic;

        if (!f_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (1 != sscanf(buf + 2, "%d", &mimic))
            return PARSE_ERROR_GENERIC;

        f_ptr->mimic = mimic;
    }
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        if (!f_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!buf[2] || !buf[3] || !buf[4])
            return PARSE_ERROR_GENERIC;

        d_char = buf[2];
        if (buf[5])
        {
            buf += 4;
            d_attr = color_text_to_attr(buf);
        }
        else
        {
            d_attr = color_char_to_attr(buf[4]);
        }

        if (d_attr < 0)
            return PARSE_ERROR_GENERIC;

        f_ptr->d_attr = d_attr;
        f_ptr->d_char = d_char;
    }
    else if (buf[0] == 'T')
    {
        if (!f_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;

        return parse_tile_line(buf, &f_ptr->x_attr, &f_ptr->x_char);
    }
    else
    {
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    }

    return 0;
}

#endif /* ALLOW_TEMPLATES */
