/* File: init-parse-major-blessings.c */
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
#include "metarun.h"

#ifdef ALLOW_TEMPLATES

static int parse_major_blessing_effect_code(cptr code)
{
    if (!code || !*code) return METARUN_MAJOR_EFFECT_NONE;
    if (streq(code, "NONE")) return METARUN_MAJOR_EFFECT_NONE;
    if (streq(code, "SUPPLY_CAP") || streq(code, "SUPPLY_LIMIT") || streq(code, "SUPPLY_COVENANT"))
        return METARUN_MAJOR_EFFECT_SUPPLY_LIMIT;
    if (streq(code, "START_ARTIFACT") || streq(code, "ARTEFACT_PATRONAGE") || streq(code, "ARTEFACT_START"))
        return METARUN_MAJOR_EFFECT_START_ARTIFACT;
    return -1;
}

errr parse_mb_info(char *buf, header *head)
{
    static major_blessing_type *mb_ptr = NULL;

    if (buf[0] == 'N')
    {
        char *s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;
        *s++ = '\0';
        if (!*s) return PARSE_ERROR_GENERIC;

        int idx = atoi(buf + 2);
        if (idx <= error_idx)          return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (idx >= head->info_num)     return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = idx;

        mb_ptr = ((major_blessing_type *)head->info_ptr) + idx;
        memset(mb_ptr, 0, sizeof(major_blessing_type));
        mb_ptr->cost = 3; /* default cost */

        if (!(mb_ptr->name = add_name(head, s)))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }
    else if (buf[0] == 'S')
    {
        if (!mb_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!add_text(&(mb_ptr->short_desc), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }
    else if (buf[0] == 'D')
    {
        if (!mb_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!add_text(&(mb_ptr->detail_desc), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }
    else if (buf[0] == 'M')
    {
        if (!mb_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!add_text(&(mb_ptr->unlock_msg), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }
    else if (buf[0] == 'E')
    {
        if (!mb_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        int effect = parse_major_blessing_effect_code(buf + 2);
        if (effect < 0) return PARSE_ERROR_INVALID_FLAG;
        mb_ptr->effect = (byte)effect;
    }
    else if (buf[0] == 'C')
    {
        if (!mb_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        long cost = atol(buf + 2);
        if (cost < 0 || cost > 255) return PARSE_ERROR_OUT_OF_BOUNDS;
        mb_ptr->cost = (byte)cost;
    }
    else
    {
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    }

    return 0;
}

/*
 * Initialize the "flavor_info" array, by parsing an ascii "template" file
 */

#endif /* ALLOW_TEMPLATES */
