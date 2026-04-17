/* File: init-parse-limits.c */

#include "angband.h"
#include "init.h"
#include "log/log.h"

#ifdef ALLOW_TEMPLATES

errr parse_z_info(char* buf, header* head)
{
    maxima* limits = head->info_ptr;

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

        z_info->b_max = max;
    }
    else if (buf[2] == 'A')
    {
        int art_special_max, art_normal_max, art_random_max, art_self_made_max;

        if (4 != sscanf(buf + 4, "%d:%d:%d:%d", &art_special_max, &art_normal_max,
                &art_random_max, &art_self_made_max))
        {
            return PARSE_ERROR_GENERIC;
        }

        z_info->art_spec_max = art_special_max;
        z_info->art_norm_max = art_normal_max + art_special_max;
        z_info->art_rand_max = z_info->art_norm_max + art_random_max;
        z_info->art_self_made_max = z_info->art_rand_max + art_self_made_max;
        z_info->art_max = art_special_max + art_normal_max + art_random_max
            + art_self_made_max;
    }
    else if (buf[2] == 'E')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->e_max = max;
    }
    else if (buf[2] == 'G')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->ghost_other_max = max;
    }
    else if (buf[2] == 'R')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->r_max = max;
    }
    else if (buf[2] == 'V')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->v_max = max;
    }
    else if (buf[2] == 'P')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->p_max = max;
    }
    else if (buf[2] == 'C')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->c_max = max;
    }
    else if (buf[2] == 'H')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->h_max = max;
    }
    else if (buf[2] == 'S')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->st_max = max;
    }
    else if (buf[2] == 'U')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->cu_max = max;
    }
    else if (buf[2] == 'J')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->mb_max = max;
    }
    else if (buf[2] == 'Q')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->quest_max = max;
    }
    else if (buf[2] == 'W')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->oath_max = max;
    }
    else if (buf[2] == 'L')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->flavor_max = max;
    }
    else if (buf[2] == 'O')
    {
        int max;

        if (1 != sscanf(buf + 4, "%d", &max))
            return PARSE_ERROR_GENERIC;

        z_info->o_max = max;
    }
    else if (buf[2] == 'N')
    {
        long max;

        if (1 != sscanf(buf + 4, "%ld", &max))
            return PARSE_ERROR_GENERIC;

        z_info->fake_name_size = max;
    }
    else if (buf[2] == 'T')
    {
        long max;

        if (1 != sscanf(buf + 4, "%ld", &max))
            return PARSE_ERROR_GENERIC;

        z_info->fake_text_size = max;
    }
    else if (buf[2] == 'Y')
    {
        z_info->rt_max = (u16b)atoi(buf + 4);
    }
    else if (buf[2] == 'Z')
    {
        z_info->style_max = (u16b)atoi(buf + 4);
    }
    else if (buf[2] == 'X')
    {
        z_info->skeleton_note_max = (u16b)atoi(buf + 4);
        log_debug("Parsed skeleton_note_max (M:X): %d", z_info->skeleton_note_max);
    }
    else
    {
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    }

    return 0;
}

#endif /* ALLOW_TEMPLATES */
