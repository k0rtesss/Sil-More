/* File: init-parse-limits.c */
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
#include "log/log.h"

#ifdef ALLOW_TEMPLATES

errr parse_z_info(char* buf, header* head)
{
    maxima* limits = head->info_ptr;

    if (!limits)
        return PARSE_ERROR_GENERIC;

    if (buf[0] != 'M')
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    if (!buf[2])
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    if (buf[3] != ':')
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    if (buf[2] == 'F')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->f_max = max;
    }
    else if (buf[2] == 'K')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->k_max = max;
    }
    else if (buf[2] == 'B')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->b_max = max;
    }
    else if (buf[2] == 'A')
    {
        int art_special_max, art_normal_max, art_random_max, art_self_made_max;

        if (4 != sscanf(buf + 4, "%d:%d:%d:%d", &art_special_max, &art_normal_max,
                &art_random_max, &art_self_made_max))
        {
            return PARSE_ERROR_GENERIC;
        }

        limits->art_spec_max = art_special_max;
        limits->art_norm_max = art_normal_max + art_special_max;
        limits->art_rand_max = limits->art_norm_max + art_random_max;
        limits->art_self_made_max = limits->art_rand_max + art_self_made_max;
        limits->art_max = art_special_max + art_normal_max + art_random_max
            + art_self_made_max;
    }
    else if (buf[2] == 'E')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->e_max = max;
    }
    else if (buf[2] == 'G')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->ghost_other_max = max;
    }
    else if (buf[2] == 'R')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->r_max = max;
    }
    else if (buf[2] == 'V')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->v_max = max;
    }
    else if (buf[2] == 'P')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->p_max = max;
    }
    else if (buf[2] == 'C')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->c_max = max;
    }
    else if (buf[2] == 'H')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->h_max = max;
    }
    else if (buf[2] == 'S')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->st_max = max;
    }
    else if (buf[2] == 'U')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->cu_max = max;
    }
    else if (buf[2] == 'J')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->mb_max = max;
    }
    else if (buf[2] == 'Q')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->quest_max = max;
    }
    else if (buf[2] == 'W')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->oath_max = max;
    }
    else if (buf[2] == 'L')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->flavor_max = max;
    }
    else if (buf[2] == 'O')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        limits->o_max = max;
    }
    else if (buf[2] == 'N')
    {
        long max;

        if (1 != sscanf(buf + 4, "%ld", &max))
            return PARSE_ERROR_GENERIC;

        limits->fake_name_size = max;
    }
    else if (buf[2] == 'T')
    {
        long max;

        if (1 != sscanf(buf + 4, "%ld", &max))
            return PARSE_ERROR_GENERIC;

        limits->fake_text_size = max;
    }
    else if (buf[2] == 'Y')
    {
        limits->rt_max = (u16b)atoi(buf + 4);
    }
    else if (buf[2] == 'Z')
    {
        limits->style_max = (u16b)atoi(buf + 4);
    }
    else if (buf[2] == 'X')
    {
        limits->skeleton_note_max = (u16b)atoi(buf + 4);
        log_debug("Parsed skeleton_note_max (M:X): %d", limits->skeleton_note_max);
    }
    else
    {
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    }

    return 0;
}

#endif /* ALLOW_TEMPLATES */
