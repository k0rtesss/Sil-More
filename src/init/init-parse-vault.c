/* File: init-parse-vault.c */
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

static errr grab_one_vault_flag(vault_type* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[VLT] = &(ptr->flags);
    return grab_one_flag(f, "vault", what);
}

errr parse_v_info(char* buf, header* head)
{
    int i;
    char* s;
    char* t;
    static vault_type* v_ptr = NULL;

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

        v_ptr = (vault_type*)head->info_ptr + i;
        v_ptr->color = 0;
        v_ptr->message = 0;
        v_ptr->style_count = 0;
        for (int j = 0; j < 16; ++j)
        {
            v_ptr->style_idx[j] = -1;
            v_ptr->style_weight[j] = 0;
        }

        if (!(v_ptr->name = add_name(head, s)))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }
    else if (buf[0] == 'X')
    {
        int typ;
        int depth;
        int rarity;
        int max_depth;
        int num_scanned;

        if (!v_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;

        num_scanned = sscanf(buf + 2, "%d:%d:%d:%d", &typ, &depth, &rarity, &max_depth);
        if (num_scanned == 3)
        {
            max_depth = 0;
        }
        else if (num_scanned != 4)
        {
            return PARSE_ERROR_GENERIC;
        }

        v_ptr->typ = typ;
        v_ptr->depth = depth;
        v_ptr->max_depth = max_depth;
        v_ptr->rarity = rarity;
        v_ptr->hgt = 0;
        v_ptr->wid = 0;
    }
    else if (buf[0] == 'C')
    {
        int color;

        if (!v_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (1 != sscanf(buf + 2, "%d", &color))
            return PARSE_ERROR_GENERIC;
        if (color < 0 || color > 255)
            return PARSE_ERROR_GENERIC;

        v_ptr->color = color;
    }
    else if (buf[0] == 'S')
    {
        char* style_data;
        char* tok;

        if (!v_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;

        style_data = buf + 2;
        while (*style_data)
        {
            int sidx;
            int w;
            char* colon;

            while (*style_data == ' ')
                style_data++;
            if (!*style_data)
                break;

            tok = style_data;
            while (*style_data && *style_data != ' ')
                style_data++;
            if (*style_data)
            {
                *style_data = '\0';
                style_data++;
            }

            colon = strchr(tok, ':');
            if (!colon)
                return PARSE_ERROR_GENERIC;
            *colon = '\0';

            if (tok[0] == '*' && tok[1] == '\0')
                sidx = -1;
            else if (tok[0] == '$' && tok[1] == '\0')
                sidx = -2;
            else
                sidx = atoi(tok);

            w = atoi(colon + 1);
            if (v_ptr->style_count < 16 && sidx >= -2 && w > 0)
            {
                v_ptr->style_idx[v_ptr->style_count] = (s16b)sidx;
                v_ptr->style_weight[v_ptr->style_count] = (s16b)w;
                v_ptr->style_count++;
            }
        }
    }
    else if (buf[0] == 'F')
    {
        if (!v_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s;)
        {
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t)
                ;

            if (*t)
            {
                *t++ = '\0';
                while (*t == ' ' || *t == '|')
                    t++;
            }

            if (0 != grab_one_vault_flag(v_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }
    else if (buf[0] == 'M')
    {
        if (!v_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&v_ptr->message, head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }
    else if (buf[0] == 'D')
    {
        if (!v_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;

        s = buf + 2;

        if (v_ptr->wid == 0)
        {
            v_ptr->wid = strlen(buf + 2);
        }
        else if (v_ptr->wid != strlen(buf + 2))
        {
            return PARSE_ERROR_VAULT_NOT_RECTANGULAR;
        }

        if (!add_text(&v_ptr->text, head, s))
            return PARSE_ERROR_OUT_OF_MEMORY;

        if (strchr(buf, '0'))
            v_ptr->forge = true;

        v_ptr->hgt++;

        if ((v_ptr->typ == 6) && ((v_ptr->wid > 33) || (v_ptr->hgt > 22)))
            return PARSE_ERROR_VAULT_TOO_BIG;

        if ((v_ptr->typ == 7) && ((v_ptr->wid > 33) || (v_ptr->hgt > 22)))
            return PARSE_ERROR_VAULT_TOO_BIG;

        if ((v_ptr->typ == 8) && ((v_ptr->wid > 66) || (v_ptr->hgt > 44)))
            return PARSE_ERROR_VAULT_TOO_BIG;
    }
    else
    {
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    }

    return 0;
}

#endif /* ALLOW_TEMPLATES */
