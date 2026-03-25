/* File: fs/pref-files.c */

#include "angband.h"
#include "externs.h"

#include "fs/io_sdl.h"
#include "fs/path.h"
#include "fs/pref-files.h"
#include "log/log.h"

#include <ctype.h>

static bool parse_visual_component(const char* token, bool expect_row, byte* value)
{
    if (!token || !*token || !value)
        return false;

    {
        char prefix = token[0];
        if (expect_row && (prefix == 'R' || prefix == 'r'))
        {
            char* end = NULL;
            long row = strtol(token + 1, &end, 0);
            if (end && (*end == '\0') && (row >= 0) && (row <= TILE_INDEX_MASK))
            {
                *value = TILE_SET_INDEX(TILE_FLAG, (byte)row);
                return true;
            }
        }
        else if (!expect_row && (prefix == 'C' || prefix == 'c'))
        {
            char* end = NULL;
            long col = strtol(token + 1, &end, 0);
            if (end && (*end == '\0') && (col >= 0) && (col <= TILE_INDEX_MASK))
            {
                *value = TILE_SET_INDEX(TILE_FLAG, (byte)col);
                return true;
            }
        }
    }

    {
        char* end = NULL;
        long parsed = strtol(token, &end, 0);
        if (end && (*end == '\0') && (parsed >= 0) && (parsed <= UCHAR_MAX))
        {
            *value = (byte)parsed;
            return true;
        }
    }

    return false;
}

static s16b tokenize(char* buf, s16b num, char** tokens)
{
    int i = 0;
    char* s = buf;

    while (i < num - 1)
    {
        char* t;

        for (t = s; *t; t++)
        {
            if ((*t == ':') || (*t == '/'))
                break;

            if (*t == '\'')
            {
                t++;

                if (*t == '\\')
                    t++;

                if (!*t)
                    break;

                t++;

                if (*t != '\'')
                    *t = '\'';
            }

            if (*t == '\\')
                t++;
        }

        if (!*t)
            break;

        *t++ = '\0';
        tokens[i++] = s;
        s = t;
    }

    tokens[i++] = s;

    return i;
}

errr process_pref_file_command(char* buf)
{
    long i, n1, n2, sq;
    char* zz[16];

    if (!buf[0])
        return 0;

    if (isspace((unsigned char)buf[0]))
        return 0;

    if (buf[0] == '#')
        return 0;

    if (buf[1] != ':')
        return 1;

    if (buf[0] == 'R')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            monster_race* r_ptr;
            byte parsed_attr, parsed_char;
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= (long)z_info->r_max))
                return 1;
            r_ptr = &r_info[i];
            if (parse_visual_component(zz[1], true, &parsed_attr) && parsed_attr)
                r_ptr->x_attr = parsed_attr;
            if (parse_visual_component(zz[2], false, &parsed_char) && parsed_char)
                r_ptr->x_char = (char)parsed_char;
            return 0;
        }
    }

    else if (buf[0] == 'B')
    {
        if (2 == tokenize(buf + 2, 2, zz))
        {
            add_autoinscription(strtol(zz[0], NULL, 0), zz[1]);
            return 0;
        }
    }

    else if (buf[0] == 'Q')
    {
        i = tokenize(buf + 2, 4, zz);
        if (i == 2)
        {
            n1 = strtol(zz[0], NULL, 0);
            n2 = strtol(zz[1], NULL, 0);
            squelch_level[n1] = n2;
            return 0;
        }
        else if (i == 4)
        {
            i = strtol(zz[0], NULL, 0);
            n1 = strtol(zz[1], NULL, 0);
            n2 = strtol(zz[2], NULL, 0);
            sq = strtol(zz[3], NULL, 0);
            if ((k_info[i].tval == n1) && (k_info[i].sval == n2))
            {
                k_info[i].squelch = sq;
                return 0;
            }
            else
            {
                for (i = 1; i < z_info->k_max; i++)
                {
                    if ((k_info[i].tval == n1) && (k_info[i].sval == n2))
                    {
                        k_info[i].squelch = sq;
                        return 0;
                    }
                }
            }
        }
    }

    else if (buf[0] == 'K')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            object_kind* k_ptr;
            byte parsed_attr, parsed_char;
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= (long)z_info->k_max))
                return 1;
            k_ptr = &k_info[i];
            if (parse_visual_component(zz[1], true, &parsed_attr) && parsed_attr)
                k_ptr->x_attr = parsed_attr;
            if (parse_visual_component(zz[2], false, &parsed_char) && parsed_char)
                k_ptr->x_char = (char)parsed_char;
            return 0;
        }
    }

    else if (buf[0] == 'F')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            feature_type* f_ptr;
            byte parsed_attr, parsed_char;
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= (long)z_info->f_max))
                return 1;
            f_ptr = &f_info[i];
            if (parse_visual_component(zz[1], true, &parsed_attr) && parsed_attr)
                f_ptr->x_attr = parsed_attr;
            if (parse_visual_component(zz[2], false, &parsed_char) && parsed_char)
                f_ptr->x_char = (char)parsed_char;
            return 0;
        }
    }

    else if (buf[0] == 'L')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            flavor_type* flavor_ptr;
            byte parsed_attr, parsed_char;
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= (long)z_info->flavor_max))
                return 1;
            flavor_ptr = &flavor_info[i];
            if (parse_visual_component(zz[1], true, &parsed_attr) && parsed_attr)
                flavor_ptr->x_attr = parsed_attr;
            if (parse_visual_component(zz[2], false, &parsed_char) && parsed_char)
                flavor_ptr->x_char = (char)parsed_char;
            return 0;
        }
    }

    else if (buf[0] == 'S')
    {
        if (tokenize(buf + 2, 3, zz) == 3)
        {
            i = strtol(zz[0], NULL, 0);
            n1 = strtol(zz[1], NULL, 0);
            n2 = strtol(zz[2], NULL, 0);
            if ((i < 0) || (i >= (long)N_ELEMENTS(misc_to_attr)))
                return 1;
            misc_to_attr[i] = (byte)n1;
            misc_to_char[i] = (char)n2;
            return 0;
        }
    }

    else if (buf[0] == 'E')
    {
        if (tokenize(buf + 2, 2, zz) == 2)
        {
            i = strtol(zz[0], NULL, 0) % 128;
            n1 = strtol(zz[1], NULL, 0);
            if ((i < 0) || (i >= (long)N_ELEMENTS(tval_to_attr)))
                return 1;
            if (n1)
                tval_to_attr[i] = (byte)n1;
            return 0;
        }
    }

    else if (buf[0] == 'A')
    {
        text_to_ascii(macro_buffer, sizeof(macro_buffer), buf + 2);
        return 0;
    }

    else if (buf[0] == 'P')
    {
        char tmp[1024];
        text_to_ascii(tmp, sizeof(tmp), buf + 2);
        macro_add(tmp, macro_buffer);
        return 0;
    }

    else if (buf[0] == 'C')
    {
        long mode;
        char tmp[1024];

        if (tokenize(buf + 2, 2, zz) != 2)
            return 1;

        mode = strtol(zz[0], NULL, 0);
        if ((mode < 0) || (mode >= KEYMAP_MODES))
            return 1;

        text_to_ascii(tmp, sizeof(tmp), zz[1]);
        if (!tmp[0] || tmp[1])
            return 1;
        i = (long)tmp[0];

        str_free(keymap_act[mode][i]);
        keymap_act[mode][i] = str_dup(macro_buffer);

        return 0;
    }

    else if (buf[0] == 'V')
    {
        if (tokenize(buf + 2, 5, zz) == 5)
        {
            i = strtol(zz[0], NULL, 0);
            if ((i < 0) || (i >= 256))
                return 1;
            angband_color_table[i][0] = (byte)strtol(zz[1], NULL, 0);
            angband_color_table[i][1] = (byte)strtol(zz[2], NULL, 0);
            angband_color_table[i][2] = (byte)strtol(zz[3], NULL, 0);
            angband_color_table[i][3] = (byte)strtol(zz[4], NULL, 0);
            return 0;
        }
    }

    else if (buf[0] == 'T')
    {
        int tok;

        tok = tokenize(buf + 2, MAX_MACRO_MOD + 2, zz);

        if (tok >= 4)
        {
            int j;
            int num;

            macro_trigger_free();

            if (*zz[0] == '\0')
                return 0;

            num = strlen(zz[1]);

            if (num + 2 != tok)
                return 1;

            macro_template = str_dup(zz[0]);
            macro_modifier_chr = str_dup(zz[1]);

            for (j = 0; j < num; j++)
                macro_modifier_name[j] = str_dup(zz[2 + j]);
        }
        else if (tok >= 2)
        {
            char* trigger_buf;
            cptr s;
            char* t;

            if (max_macrotrigger >= MAX_MACRO_TRIGGER)
            {
                msg_print("Too many macro triggers!");
                return 1;
            }

            trigger_buf = mem_alloc_array(strlen(zz[0]) + 1, char);

            s = zz[0];
            t = trigger_buf;

            while (*s)
            {
                if ('\\' == *s)
                    s++;
                *t++ = *s++;
            }

            *t = '\0';

            macro_trigger_name[max_macrotrigger] = str_dup(trigger_buf);
            mem_free_null(trigger_buf);

            macro_trigger_keycode[0][max_macrotrigger] = str_dup(zz[1]);

            if (tok == 3)
                macro_trigger_keycode[1][max_macrotrigger] = str_dup(zz[2]);
            else
                macro_trigger_keycode[1][max_macrotrigger] = str_dup(zz[1]);

            max_macrotrigger++;
        }

        return 0;
    }

    else if (buf[0] == 'X')
    {
        for (i = 0; i < OPT_ADULT; i++)
        {
            if (option_text[i] && streq(option_text[i], buf + 2))
            {
                op_ptr->opt[i] = false;
                return 0;
            }
        }

        return 0;
    }

    else if (buf[0] == 'Y')
    {
        for (i = 0; i < OPT_ADULT; i++)
        {
            if (option_text[i] && streq(option_text[i], buf + 2))
            {
                op_ptr->opt[i] = true;
                return 0;
            }
        }

        return 0;
    }

    else if (buf[0] == 'W')
    {
        long win, flag, value;

        if (tokenize(buf + 2, 3, zz) == 3)
        {
            win = strtol(zz[0], NULL, 0);
            flag = strtol(zz[1], NULL, 0);
            value = strtol(zz[2], NULL, 0);

            if ((win <= 0) || (win >= ANGBAND_TERM_MAX))
                return 1;

            if ((flag < 0) || (flag >= 32))
                return 1;

            if (window_flag_desc[flag])
            {
                if (value)
                    op_ptr->window_flag[win] |= (1L << flag);
                else
                    op_ptr->window_flag[win] &= ~(1L << flag);
            }

            return 0;
        }
    }

    else if (buf[0] == 'M')
    {
        if (tokenize(buf + 2, 2, zz) == 2)
        {
            long type = strtol(zz[0], NULL, 0);
            int color = color_char_to_attr(zz[1][0]);

            if (color < 0)
                return 1;

            return message_color_define((u16b)type, (byte)color);
        }
    }

    return 1;
}

static cptr process_pref_file_expr(char** sp, char* fp)
{
    cptr v;
    char* b;
    char* s;
    char b1 = '[';
    char b2 = ']';
    char f = ' ';

    s = (*sp);

    while (isspace((unsigned char)*s))
        s++;

    b = s;
    v = "?o?o?";

    if (*s == b1)
    {
        const char* p;
        const char* t;

        s++;
        t = process_pref_file_expr(&s, &f);

        if (!*t)
        {
        }
        else if (streq(t, "IOR"))
        {
            v = "0";
            while (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
                if (*t && !streq(t, "0"))
                    v = "1";
            }
        }
        else if (streq(t, "AND"))
        {
            v = "1";
            while (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
                if (*t && streq(t, "0"))
                    v = "0";
            }
        }
        else if (streq(t, "NOT"))
        {
            v = "1";
            while (*s && (f != b2))
            {
                t = process_pref_file_expr(&s, &f);
                if (*t && !streq(t, "0"))
                    v = "0";
            }
        }
        else if (streq(t, "EQU"))
        {
            v = "1";
            if (*s && (f != b2))
                t = process_pref_file_expr(&s, &f);
            while (*s && (f != b2))
            {
                p = t;
                t = process_pref_file_expr(&s, &f);
                if (*t && !streq(p, t))
                    v = "0";
            }
        }
        else if (streq(t, "LEQ"))
        {
            v = "1";
            if (*s && (f != b2))
                t = process_pref_file_expr(&s, &f);
            while (*s && (f != b2))
            {
                p = t;
                t = process_pref_file_expr(&s, &f);
                if (*t && (strcmp(p, t) >= 0))
                    v = "0";
            }
        }
        else if (streq(t, "GEQ"))
        {
            v = "1";
            if (*s && (f != b2))
                t = process_pref_file_expr(&s, &f);
            while (*s && (f != b2))
            {
                p = t;
                t = process_pref_file_expr(&s, &f);
                if (*t && (strcmp(p, t) <= 0))
                    v = "0";
            }
        }
        else
        {
            while (*s && (f != b2))
                t = process_pref_file_expr(&s, &f);
        }

        if (f != b2)
            v = "?x?x?";

        if ((f = *s) != '\0')
            *s++ = '\0';
    }
    else
    {
        while (isprint((unsigned char)*s) && !strchr(" []", *s))
            ++s;

        if ((f = *s) != '\0')
            *s++ = '\0';

        if (*b == '$')
        {
            if (streq(b + 1, "SYS"))
                v = ANGBAND_SYS;
            else if (streq(b + 1, "GRAF"))
                v = ANGBAND_GRAF;
            else if (streq(b + 1, "RACE"))
                v = p_name + rp_ptr->name;
            else if (streq(b + 1, "nameless"))
                v = op_ptr->base_name;
            else if (streq(b + 1, "VERSION"))
                v = VERSION_STRING;
        }
        else
        {
            v = b;
        }
    }

    (*fp) = f;
    (*sp) = s;

    return v;
}

static errr process_pref_file_aux(cptr name)
{
    SDL_IOStream* fp;
    char buf[1024];
    char old[1024];
    int line = -1;
    errr err = 0;
    bool bypass = false;

    log_debug("Processing preference file: %s", name);

    fp = sdl_fopen(name, "r");

    if (!fp)
    {
        log_debug("Preference file '%s' not found or could not be opened", name);
        return -1;
    }

    while (0 == sdl_fgets(fp, buf, sizeof(buf)))
    {
        line++;

        if (!buf[0])
            continue;

        if (isspace((unsigned char)buf[0]))
            continue;

        if (buf[0] == '#')
            continue;

        SDL_strlcpy(old, buf, sizeof(old));

        if ((buf[0] == '?') && (buf[1] == ':'))
        {
            char f;
            cptr v;
            char* s = buf + 2;

            v = process_pref_file_expr(&s, &f);
            bypass = streq(v, "0") ? true : false;
            continue;
        }

        if (bypass)
            continue;

        if (buf[0] == '%')
        {
            (void)process_pref_file(buf + 2);
            continue;
        }

        err = process_pref_file_command(buf);

        if (err)
            break;
    }

    if (err)
    {
        msg_format("Error %d in line %d of file '%s'.", err, line, name);
        msg_format("Parsing '%s'", old);
        message_flush();
    }

    log_debug("Successfully processed preference file '%s' (%d lines)", name, line + 1);

    sdl_fclose(fp);

    return err;
}

errr process_pref_file(cptr name)
{
    char buf[1024];
    errr err;

    path_build(buf, sizeof(buf), ANGBAND_DIR_PREF, name);
    err = process_pref_file_aux(buf);

    if (err < 1)
    {
        path_build(buf, sizeof(buf), ANGBAND_DIR_USER, name);
        err = process_pref_file_aux(buf);
    }

    return err;
}
