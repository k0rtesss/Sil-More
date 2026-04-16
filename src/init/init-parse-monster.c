/* File: init-parse-monster.c */

#include "angband.h"
#include "init.h"
#include "init/init-parse-internal.h"

#ifdef ALLOW_TEMPLATES

/*
 * Monster Blow Methods
 */
static cptr r_info_blow_method[]
    = { "", "HIT", "TOUCH", "XXX", "XXX", "CLAW", "BITE", "STING", "PECK",
          "WHIP", "XXX", "CRUSH", "ENGULF", "CRAWL", "THORN", "XXX", "XXX",
          "XXX", "XXX", "SPORE", "XXX", "XXX", "XXX", "XXX", "XXX", NULL };

/*
 * Monster Blow Effects
 */
static cptr r_info_blow_effect[] = { "", "HURT", "WOUND", "BATTER", "SHATTER",
    "UN_BONUS", "UN_POWER", "LOSE_MANA", "SLOW", "EAT_ITEM", "EAT_FOOD", "DARK",
    "HUNGER", "POISON", "ACID", "ELEC", "FIRE", "COLD", "BLIND", "CONFUSE",
    "TERRIFY", "ENTRANCE", "HALLU", "DISEASE", "LOSE_STR", "LOSE_DEX",
    "LOSE_CON", "LOSE_GRA", "LOSE_STR_CON", "LOSE_ALL", "DISARM", NULL };

/*
 * Grab one (basic) flag in a monster_race from a textual string.
 */
static errr grab_one_basic_flag(monster_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RF1] = &(ptr->flags1);
    f[RF2] = &(ptr->flags2);
    f[RF3] = &(ptr->flags3);
    return grab_one_flag(f, "monster", what);
}

/*
 * Grab one (spell) flag in a monster_race from a textual string.
 */
static errr grab_one_spell_flag(monster_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RF4] = &(ptr->flags4);
    return grab_one_flag(f, "monster", what);
}

/*
 * Initialize the "r_info" array, by parsing an ascii "template" file.
 */
errr parse_r_info(char* buf, header* head)
{
    int i;
    char* s;
    char* t;
    static monster_race* r_ptr = NULL;

    if (buf[0] == 'N')
    {
        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        i = atoi(buf + 2);
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        error_idx = i;
        r_ptr = (monster_race*)head->info_ptr + i;

        if (!(r_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    else if (buf[0] == 'D')
    {
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = buf + 2;
        if (!add_text(&(r_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    else if (buf[0] == 'Q')
    {
        u64b guid = 0;

        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (!parse_u64b_hex(buf + 2, &guid))
            return (PARSE_ERROR_GENERIC);

        r_ptr->guid = guid;
    }

    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);
        if (!buf[2] || !buf[3] || !buf[4])
            return (PARSE_ERROR_GENERIC);

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
            return (PARSE_ERROR_GENERIC);

        r_ptr->d_attr = d_attr;
        r_ptr->d_char = d_char;
    }

    else if (buf[0] == 'T')
    {
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        return parse_tile_line(buf, &r_ptr->x_attr, &r_ptr->x_char);
    }

    else if (buf[0] == 'I')
    {
        int spd, hp1, hp2, light;

        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);
        if (4 != sscanf(buf + 2, "%d:%dd%d:%d", &spd, &hp1, &hp2, &light))
            return (PARSE_ERROR_GENERIC);

        r_ptr->speed = spd;
        r_ptr->hdice = hp1;
        r_ptr->hside = hp2;
        r_ptr->light = light;
    }

    else if (buf[0] == 'W')
    {
        int lev, rar;

        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);
        if (2 != sscanf(buf + 2, "%d:%d", &lev, &rar))
            return (PARSE_ERROR_GENERIC);

        r_ptr->level = lev;
        r_ptr->rarity = rar;
    }

    else if (buf[0] == 'A')
    {
        int sleep, per, stl, wil;

        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &sleep, &per, &stl, &wil))
            return (PARSE_ERROR_GENERIC);

        r_ptr->sleep = sleep;
        r_ptr->per = per;
        r_ptr->stl = stl;
        r_ptr->wil = wil;
    }

    else if (buf[0] == 'P')
    {
        int evn, pd = 0, ps = 0, n;

        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        n = sscanf(buf + 2, "[%d,%dd%d]", &evn, &pd, &ps);
        if ((n != 1) && (n != 3))
            return (PARSE_ERROR_GENERIC);

        r_ptr->evn = evn;
        r_ptr->pd = pd;
        r_ptr->ps = ps;
    }

    else if (buf[0] == 'B')
    {
        int n1, n2, n;
        int att, dd, ds;

        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        for (i = 0; i < MONSTER_BLOW_MAX; i++)
            if (!r_ptr->blow[i].method)
                break;

        if (i == MONSTER_BLOW_MAX)
            return (PARSE_ERROR_GENERIC);

        for (s = t = buf + 2; *t && (*t != ':'); t++)
            ;

        if (*t == ':')
            *t++ = '\0';

        for (n1 = 0; r_info_blow_method[n1]; n1++)
        {
            if (streq(s, r_info_blow_method[n1]))
                break;
        }

        if (!r_info_blow_method[n1])
            return (PARSE_ERROR_GENERIC);

        for (s = t; *t && (*t != ':'); t++)
            ;

        if (*t == ':')
            *t++ = '\0';

        for (n2 = 0; r_info_blow_effect[n2]; n2++)
        {
            if (streq(s, r_info_blow_effect[n2]))
                break;
        }

        if (!r_info_blow_effect[n2])
            return (PARSE_ERROR_GENERIC);

        dd = 0;
        ds = 0;

        n = sscanf(t, "(%d,%dd%d)", &att, &dd, &ds);
        if ((n != 1) && (n != 3))
            return (PARSE_ERROR_GENERIC);

        r_ptr->blow[i].method = n1;
        r_ptr->blow[i].effect = n2;
        r_ptr->blow[i].att = att;
        r_ptr->blow[i].dd = dd;
        r_ptr->blow[i].ds = ds;
    }

    else if (buf[0] == 'F')
    {
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

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

            if (0 != grab_one_basic_flag(r_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            s = t;
        }
    }

    else if (buf[0] == 'S')
    {
        if (!r_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        for (s = buf + 2; *s;)
        {
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t)
                ;

            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|'))
                    t++;
            }

            if ((r_ptr->freq_ranged == 0)
                && (1 == sscanf(s, "SPELL_PCT_%d", &i)))
            {
                if ((i < 1) || (i > 100))
                    return (PARSE_ERROR_INVALID_SPELL_FREQ);

                r_ptr->freq_ranged = i;
                s = t;
                continue;
            }

            if ((r_ptr->spell_power == 0) && (1 == sscanf(s, "POW_%d", &i)))
            {
                r_ptr->spell_power = i;
                s = t;
                continue;
            }

            if (0 != grab_one_spell_flag(r_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            s = t;
        }
    }

    else
    {
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    return (0);
}

#endif /* ALLOW_TEMPLATES */
