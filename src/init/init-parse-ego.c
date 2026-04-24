/* File: init-parse-ego.c */
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
#include "init/init-object-bonuses.h"
#include "init/init-parse-internal.h"

#ifdef ALLOW_TEMPLATES

static bool grab_one_ego_item_flag(ego_item_type* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[TR1] = &(ptr->flags1);
    f[TR2] = &(ptr->flags2);
    f[TR3] = &(ptr->flags3);
    f[TR4] = &(ptr->flags4);
    return grab_one_flag(f, "object", what);
}

/*
 * Initialize the "e_info" array, by parsing an ascii "template" file
 */
errr parse_e_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static ego_item_type* e_ptr = NULL;

    static int cur_t = 0;

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        e_ptr = (ego_item_type*)head->info_ptr + i;

        /* Store the name */
        if (!(e_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Reset per-stat/skill bonus offsets. */
        for (int si = 0; si < A_MAX; si++)
        {
            e_ptr->stat_bonus[si] = 0;
            e_ptr->stat_bonus_set[si] = false;
        }
        for (int sk = 0; sk < S_MAX; sk++)
        {
            e_ptr->skill_bonus[sk] = 0;
            e_ptr->skill_bonus_set[sk] = false;
        }

        /* Start with the first of the tval indices */
        cur_t = 0;

        /* Reset allocation tracking */
        e_ptr->alloc_count = 0;
        e_ptr->elemental_block = 0;
    }

    /* Process 'W' for "More Info" (one line only) */
    else if (buf[0] == 'W')
    {
        int level, rarity, max_level;
        long cost;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4
            != sscanf(
                buf + 2, "%d:%d:%d:%ld", &level, &rarity, &max_level, &cost))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        e_ptr->level = level;
        e_ptr->rarity = rarity;
        e_ptr->max_level = max_level;
        e_ptr->cost = cost;
    }

    /* Process 'A' for "Allocation" (one line only) */
    else if (buf[0] == 'A')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Reset explicit allocation count */
        e_ptr->alloc_count = 0;

        for (s = buf + 1; s && (s[0] == ':') && s[1];)
        {
            if (e_ptr->alloc_count > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            int depth = atoi(s + 1);
            int rarity = 1;
            t = strchr(s + 1, '/');
            char* next = strchr(s + 1, ':');
            if (t && (!next || t < next))
                rarity = atoi(t + 1);
            if (rarity < 0)
                rarity = 0;

            e_ptr->alloc_depth[e_ptr->alloc_count] = (byte)depth;
            e_ptr->alloc_prob[e_ptr->alloc_count] = (byte)rarity;
            e_ptr->alloc_count++;

            s = next;
        }
    }

    /* Process 'T' for "Types allowed" (up to three lines) */
    else if (buf[0] == 'T')
    {
        int tval, sval1, sval2;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval1, &sval2))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        e_ptr->tval[cur_t] = (byte)tval;
        e_ptr->min_sval[cur_t] = (byte)sval1;
        e_ptr->max_sval[cur_t] = (byte)sval2;

        /* Increase counter for 'possible tval' index */
        cur_t++;

        /* Allow only a limited number of T: lines */
        if (cur_t > EGO_TVALS_MAX)
            return (PARSE_ERROR_GENERIC);
    }

    /* Hack -- Process 'C' for "creation" */
    else if (buf[0] == 'C')
    {
        int max_att, to_dd, to_ds, max_evn, to_pd, to_ps, pv;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (7
            != sscanf(buf + 2, "%d:%d:%d:%d:%d:%d:%d", &max_att, &to_dd, &to_ds,
                &max_evn, &to_pd, &to_ps, &pv))
            return (PARSE_ERROR_GENERIC);

        e_ptr->max_att = max_att;
        e_ptr->to_dd = to_dd;
        e_ptr->to_ds = to_ds;
        e_ptr->max_evn = max_evn;
        e_ptr->to_pd = to_pd;
        e_ptr->to_ps = to_ps;
        e_ptr->max_pval = pv;
    }

    /* Process 'X' for elemental block chance bonus */
    else if (buf[0] == 'X')
    {
        int chance;

        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (1 != sscanf(buf + 2, "%d", &chance))
            return (PARSE_ERROR_GENERIC);

        if ((chance < 0) || (chance > 100))
            return (PARSE_ERROR_GENERIC);

        e_ptr->elemental_block = (byte)chance;
    }

    /* Process 'M' for per-stat/skill bonus offsets (one per line) */
    else if (buf[0] == 'M')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        cptr token = buf + 2;
        int value = atoi(s);

        if (!apply_obj_bonus_token(token, value,
                &e_ptr->flags1,
                e_ptr->stat_bonus, e_ptr->stat_bonus_set,
                e_ptr->skill_bonus, e_ptr->skill_bonus_set))
        {
            return (PARSE_ERROR_GENERIC);
        }
    }

    /* Process 'B' for "aBilities" (one line only) */
    else if (buf[0] == 'B')
    {
        int ability_idx;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (ability_idx = 0, s = buf + 1; s && (s[0] == ':') && s[1];
             ++ability_idx)
        {
            /* Sanity check */
            if (ability_idx > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            e_ptr->abilitynum[ability_idx] = 0;

            /* Store the skilltype */
            e_ptr->skilltype[ability_idx] = atoi(s + 1);

            /* List this ability */
            e_ptr->abilities++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    e_ptr->abilitynum[ability_idx] = abilitynum;
            }
        }
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse every entry textually */
        for (s = buf + 2; *s;)
        {
            /* Find the end of this entry */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
                ;

            /* Nuke and skip any dividers */
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_ego_item_flag(e_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&e_ptr->text, head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

#endif /* ALLOW_TEMPLATES */
