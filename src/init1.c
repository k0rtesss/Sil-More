/* File: init1.c */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "h-define.h"
#include "score/score_guid.h"
#include <SDL3/SDL.h>
#include <ctype.h>

/*
 * This file is used to initialize various variables and arrays for the
 * Angband game.  Note the use of "sdl_read()" and "sdl_write()" to bypass
 * the common limitation of "read()" and "write()" to only 32767 bytes
 * at a time.
 *
 * Several of the arrays for Angband are built from "template" files in
 * the "lib/file" directory, from which quick-load binary "image" files
 * are constructed whenever they are not present in the "lib/data"
 * directory, or if those files become obsolete, if we are allowed.
 *
 * Warning -- the "ascii" file parsers use a minor hack to collect the
 * name and text information in a single pass.  Thus, the game will not
 * be able to load any template file with more than 20K of names or 60K
 * of text, even though technically, up to 64K should be legal.
 *
 * Note that if "ALLOW_TEMPLATES" is not defined, then a lot of the code
 * in this file is compiled out, and the game will not run unless valid
 * "binary template files" already exist in "lib/data".  Thus, one can
 * compile Angband with ALLOW_TEMPLATES defined, run once to create the
 * "*.raw" files in "lib/data", and then quit, and recompile without
 * defining ALLOW_TEMPLATES, which will both save 20K and prevent people
 * from changing the ascii template files in potentially dangerous ways.
 *
 * The code could actually be removed and placed into a "stand-alone"
 * program, but that feels a little silly, especially considering some
 * of the platforms that we currently support.
 */

#ifdef ALLOW_TEMPLATES

#include "init.h"
#include "init/init-object-bonuses.h"
#include "init/init-parse-internal.h"
#include "metarun.h"

/*
 * Activation type
 */
static cptr a_info_act[ACT_MAX] = { "ILLUMINATION", "MAGIC_MAP", "CLAIRVOYANCE",
    "PROT_EVIL", "DISP_EVIL", "HEAL1", "HEAL2", "CURE_WOUNDS", "HASTE1",
    "HASTE2", "FIRE1", "FIRE2", "FIRE3", "FROST1", "FROST2", "FROST3", "FROST4",
    "FROST5", "ACID1", "RECHARGE1", "SLEEP", "LIGHTNING_BOLT", "ELEC2",
    "BANISHMENT", "MASS_BANISHMENT", "IDENTIFY_FULLY", "DRAIN_LIFE1",
    "DRAIN_LIFE2", "BIZZARE", "STAR_BALL", "RAGE_BLESS_RESIST", "PHASE",
    "TRAP_DOOR_DEST", "DETECT", "RESIST", "TELEPORT", "RESTORE_VOICE",
    "MISSILE", "ARROW", "REM_FEAR_POIS", "STINKING_CLOUD", "STONE_TO_MUD",
    "TELE_AWAY", "WOR", "CONFUSE", "PROBE", "FIREBRAND", "STARLIGHT",
    "MANA_BOLT", "BERSERKER", "RES_ACID", "RES_ELEC", "RES_FIRE", "RES_COLD",
    "RES_POIS" };
/*
 * Grab one flag in an object_kind from a textual string
 */
static errr grab_one_kind_flag(object_kind* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[TR1] = &(ptr->flags1);
    f[TR2] = &(ptr->flags2);
    f[TR3] = &(ptr->flags3);
    f[TR4] = &(ptr->flags4);
    return grab_one_flag(f, "object", what);
}

/**********************************************************************
 * Grab a single RHF and CUR flag for a curse (used by “F:” and "U:" lines in curses.txt)
 **********************************************************************/
static errr grab_one_curse_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RHF] = &(cu_ptr->flags);   /* write into the new word we added */
    return grab_one_flag(f, "curse", what);
}

static errr grab_one_curse_unique_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[CUR] = &(cu_ptr->flags_u);   /* write into the new word we added */
    return grab_one_flag(f, "curse unique", what);
}

static errr grab_one_blessing_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RHF] = &(cu_ptr->blessing_flags);
    return grab_one_flag(f, "blessing", what);
}

static errr grab_one_blessing_unique_flag(curse_type *cu_ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[CUR] = &(cu_ptr->blessing_flags_u);
    return grab_one_flag(f, "blessing unique", what);
}

/*
 * Initialize the "k_info" array, by parsing an ascii "template" file
 */
errr parse_k_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static object_kind* k_ptr = NULL;

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
        k_ptr = (object_kind*)head->info_ptr + i;

        /* Store the name */
        if (!(k_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Reset per-stat/skill bonuses. */
        for (int si = 0; si < A_MAX; si++)
        {
            k_ptr->stat_bonus[si] = 0;
            k_ptr->stat_bonus_set[si] = false;
        }
        for (int sk = 0; sk < S_MAX; sk++)
        {
            k_ptr->skill_bonus[sk] = 0;
            k_ptr->skill_bonus_set[sk] = false;
        }
    }

    /* Process 'G' for "Graphics" (one line only) */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

        /* Extract d_char */
        d_char = buf[2];

        /* If we have a longer string than expected ... */
        if (buf[5])
        {
            /* Advance "buf" on by 4 */
            buf += 4;

            /* Extract the colour */
            d_attr = color_text_to_attr(buf);
        }
        else
        {
            /* Extract the attr */
            d_attr = color_char_to_attr(buf[4]);
        }

        /* Paranoia */
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        k_ptr->d_attr = d_attr;
        k_ptr->d_char = d_char;
    }

    /* Process 'T' for "Tile" graphics (one line only) */
    else if (buf[0] == 'T')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse and set tile coordinates */
        return parse_tile_line(buf, &k_ptr->x_attr, &k_ptr->x_char);
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int tval, sval, pval;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval, &pval))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        k_ptr->tval = tval;
        k_ptr->sval = sval;
        k_ptr->pval = pval;

        /* Default max pval = base pval (no variation unless R: overrides) */
        k_ptr->max_pval = pval;

        apply_default_pval_bonuses(k_ptr->flags1, k_ptr->pval,
            k_ptr->stat_bonus, k_ptr->stat_bonus_set,
            k_ptr->skill_bonus, k_ptr->skill_bonus_set);
    }

    /* Process 'M' for per-stat/skill bonus overrides (one per line) */
    else if (buf[0] == 'M')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        cptr token = buf + 2;
        int value = atoi(s);

        if (!apply_obj_bonus_token(token, value,
                &k_ptr->flags1,
                k_ptr->stat_bonus, k_ptr->stat_bonus_set,
                k_ptr->skill_bonus, k_ptr->skill_bonus_set))
        {
            return (PARSE_ERROR_GENERIC);
        }

        apply_default_pval_bonuses(k_ptr->flags1, k_ptr->pval,
            k_ptr->stat_bonus, k_ptr->stat_bonus_set,
            k_ptr->skill_bonus, k_ptr->skill_bonus_set);
    }

    /* Process 'W' for "More Info" (one line only) */
    else if (buf[0] == 'W')
    {
        int level, extra, wgt;
        long cost;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%ld", &level, &extra, &wgt, &cost))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        k_ptr->level = level;
        k_ptr->extra = extra;
        k_ptr->weight = wgt;
        k_ptr->cost = cost;
    }

    /* Process 'A' for "Allocation" (one line only) */
    else if (buf[0] == 'A')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Reset explicit allocation count */
        k_ptr->alloc_count = 0;

        /* Read each number following a colon */
        for (s = buf + 1; s && (s[0] == ':') && s[1];)
        {
            /* Sanity check */
            if (k_ptr->alloc_count > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            int depth = atoi(s + 1);
            int rarity = 1;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            char* next = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!next || t < next))
                rarity = atoi(t + 1);

            if (rarity < 0)
                rarity = 0;

            /* Store legacy locale/chance for compatibility */
            k_ptr->locale[k_ptr->alloc_count] = (byte)depth;
            k_ptr->chance[k_ptr->alloc_count] = (byte)rarity;

            /* Store explicit allocation entries (supporting zero rarity) */
            k_ptr->alloc_depth[k_ptr->alloc_count] = (byte)depth;
            k_ptr->alloc_prob[k_ptr->alloc_count] = (byte)rarity;
            k_ptr->alloc_count++;

            /* Advance to next colon (if any) */
            s = next;
        }
    }

    /* Hack -- Process 'P' for "power" and such */
    else if (buf[0] == 'P')
    {
        int att, dd, ds, evn, pd, ps;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (6
            != sscanf(
                buf + 2, "%d:%dd%d:%d:%dd%d", &att, &dd, &ds, &evn, &pd, &ps))
            return (PARSE_ERROR_GENERIC);

        k_ptr->att = att;
        k_ptr->dd = dd;
        k_ptr->ds = ds;
        k_ptr->evn = evn;
        k_ptr->pd = pd;
        k_ptr->ps = ps;

        /* Default max values = base values (no variation unless R: overrides) */
        k_ptr->max_att = att;
        k_ptr->max_ds = ds;
        k_ptr->max_evn = evn;
        k_ptr->max_ps = ps;
    }

    /* Process 'R' for "Range" — smithing/drop maximums (one per line) */
    else if (buf[0] == 'R')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        cptr stat_name = buf + 2;
        int value = atoi(s);

        if (streq(stat_name, "ATT"))
            k_ptr->max_att = (s16b)value;
        else if (streq(stat_name, "DS"))
            k_ptr->max_ds = (byte)value;
        else if (streq(stat_name, "EVN"))
            k_ptr->max_evn = (s16b)value;
        else if (streq(stat_name, "PS"))
            k_ptr->max_ps = (byte)value;
        else if (streq(stat_name, "PVAL"))
            k_ptr->max_pval = (s16b)value;
        else
            return (PARSE_ERROR_GENERIC);
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
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
                while (*t == ' ' || *t == '|')
                    t++;
            }

            /* Parse this entry */
            if (0 != grab_one_kind_flag(k_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }

        apply_default_pval_bonuses(k_ptr->flags1, k_ptr->pval,
            k_ptr->stat_bonus, k_ptr->stat_bonus_set,
            k_ptr->skill_bonus, k_ptr->skill_bonus_set);
    }

    /* Process 'B' for "aBilities" (one line only) */
    else if (buf[0] == 'B')
    {
        int i;

        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            k_ptr->abilitynum[i] = 0;

            /* Store the skilltype */
            k_ptr->skilltype[i] = atoi(s + 1);

            /* List this ability */
            k_ptr->abilities++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    k_ptr->abilitynum[i] = abilitynum;
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current k_ptr */
        if (!k_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(k_ptr->text), head, s))
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

/*
 * Initialize the "b_info" array, by parsing an ascii "template" file
 */
errr parse_b_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    static int cur_t = 0;

    /* Current entry */
    static ability_type* b_ptr = NULL;

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
        b_ptr = (ability_type*)head->info_ptr + i;

        /* Store the name */
        if (!(b_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Start with the first of the tval indices */
        cur_t = 0;
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int skilltype, abilitynum, level;

        /* There better be a current k_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &skilltype, &abilitynum, &level))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        b_ptr->skilltype = skilltype;
        b_ptr->abilitynum = abilitynum;
        b_ptr->level = level;
    }

    /* Process 'P' for "Prerequisites" (one line only) */
    else if (buf[0] == 'P')
    {
        int i;

        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            b_ptr->prereq_abilitynum[i] = 0;

            /* Store the skilltype */
            b_ptr->prereq_skilltype[i] = atoi(s + 1);

            /* List this prerequisite */
            b_ptr->prereqs++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int prereq_abilitynum = atoi(t + 1);
                if (prereq_abilitynum > 0)
                    b_ptr->prereq_abilitynum[i] = prereq_abilitynum;
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current k_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(b_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'E' for "Effect" (mechanical description) */
    else if (buf[0] == 'E')
    {
        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the effect text */
        if (!add_text(&(b_ptr->effect), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Types allowed" (up to five lines) */
    else if (buf[0] == 'T')
    {
        int tval, sval1, sval2;

        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval1, &sval2))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        b_ptr->tval[cur_t] = (byte)tval;
        b_ptr->min_sval[cur_t] = (byte)sval1;
        b_ptr->max_sval[cur_t] = (byte)sval2;

        /* Increase counter for 'possible tval' index */
        cur_t++;

        /* Allow only a limited number of T: lines */
        if (cur_t > ABILITY_TVALS_MAX)
            return (PARSE_ERROR_GENERIC);
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

/*
 * Grab one flag in an artefact_type from a textual string
 */
static errr grab_one_artefact_flag(artefact_type* ptr, cptr what)
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
 * Grab one activation from a textual string
 */
static errr grab_one_activation(artefact_type* a_ptr, cptr what)
{
    int i;

    /* Scan activations */
    for (i = 0; i < ACT_MAX; i++)
    {
        if (streq(what, a_info_act[i]))
        {
            a_ptr->activation = i;
            return (0);
        }
    }

    /* Oops */
    msg_format("Unknown artefact activation '%s'.", what);

    /* Error */
    return (PARSE_ERROR_GENERIC);
}

/*
 * Initialize the "a_info" array, by parsing an ascii "template" file
 */
errr parse_a_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    /* Current entry */
    static artefact_type* a_ptr = NULL;

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
        a_ptr = (artefact_type*)head->info_ptr + i;

        /* Store the name */
        SDL_strlcpy(a_ptr->name, s, MAX_LEN_ART_NAME);

        /* Ignore everything */
        a_ptr->flags3 |= (TR3_IGNORE_MASK);

        /* Sil-y: paranoia: make sure that the default values are 0 */
        a_ptr->d_attr = 0;
        a_ptr->d_char = 0;

        /* Reset per-stat/skill bonuses. */
        for (int si = 0; si < A_MAX; si++)
        {
            a_ptr->stat_bonus[si] = 0;
            a_ptr->stat_bonus_set[si] = false;
        }
        for (int sk = 0; sk < S_MAX; sk++)
        {
            a_ptr->skill_bonus[sk] = 0;
            a_ptr->skill_bonus_set[sk] = false;
        }

        /* Default spawn stack size */
        a_ptr->spawn_num = 1;
    }

    /* Sil -- added this to allow for artefacts that look different to the base
     * type */
    /* Process 'G' for "Graphics" (one line only) */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

        /* Extract d_char */
        d_char = buf[2];

        /* If we have a longer string than expected ... */
        if (buf[5])
        {
            /* Advance "buf" on by 4 */
            buf += 4;

            /* Extract the colour */
            d_attr = color_text_to_attr(buf);
        }
        else
        {
            /* Extract the attr */
            d_attr = color_char_to_attr(buf[4]);
        }

        /* Paranoia */
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->d_attr = d_attr;
        a_ptr->d_char = d_char;
    }
    /* Process 'Q' for GUID */
    else if (buf[0] == 'Q')
    {
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        u64b guid;
        if (!parse_u64b_hex(buf + 2, &guid))
            return (PARSE_ERROR_GENERIC);

        a_ptr->guid = score_guid_from_u64(guid);
    }

    /* Process 'S' for spawn stack size */
    else if (buf[0] == 'S')
    {
        int spawn_num;

        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (1 != sscanf(buf + 2, "%d", &spawn_num))
            return (PARSE_ERROR_GENERIC);

        if (spawn_num < 1 || spawn_num > 255)
            return (PARSE_ERROR_GENERIC);

        a_ptr->spawn_num = (byte)spawn_num;
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int tval, sval, pval;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval, &pval))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->tval = tval;
        a_ptr->sval = sval;
        a_ptr->pval = pval;

        apply_default_pval_bonuses(a_ptr->flags1, a_ptr->pval,
            a_ptr->stat_bonus, a_ptr->stat_bonus_set,
            a_ptr->skill_bonus, a_ptr->skill_bonus_set);
    }

    /* Process 'M' for per-stat/skill bonus overrides (one per line) */
    else if (buf[0] == 'M')
    {
        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        s = strchr(buf + 2, ':');
        if (!s)
            return (PARSE_ERROR_GENERIC);

        *s++ = '\0';
        cptr token = buf + 2;
        int value = atoi(s);

        if (!apply_obj_bonus_token(token, value,
                &a_ptr->flags1,
                a_ptr->stat_bonus, a_ptr->stat_bonus_set,
                a_ptr->skill_bonus, a_ptr->skill_bonus_set))
        {
            return (PARSE_ERROR_GENERIC);
        }

        apply_default_pval_bonuses(a_ptr->flags1, a_ptr->pval,
            a_ptr->stat_bonus, a_ptr->stat_bonus_set,
            a_ptr->skill_bonus, a_ptr->skill_bonus_set);
    }

    /* Process 'W' for "More Info" (one line only) */
    else if (buf[0] == 'W')
    {
        int level, rarity, wgt;
        long cost;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%ld", &level, &rarity, &wgt, &cost))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->level = level;
        a_ptr->rarity = rarity;
        a_ptr->weight = wgt;
        a_ptr->cost = cost;
    }

    /* Process 'P' for "power" and such */
    else if (buf[0] == 'P')
    {
        int att, dd, ds, evn, pd, ps;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (6
            != sscanf(
                buf + 2, "%d:%dd%d:%d:%dd%d", &att, &dd, &ds, &evn, &pd, &ps))
            return (PARSE_ERROR_GENERIC);

        a_ptr->att = att;
        a_ptr->dd = dd;
        a_ptr->ds = ds;
        a_ptr->evn = evn;
        a_ptr->pd = pd;
        a_ptr->ps = ps;
    }

    /* Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current a_ptr */
        if (!a_ptr)
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
            if (0 != grab_one_artefact_flag(a_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }

        apply_default_pval_bonuses(a_ptr->flags1, a_ptr->pval,
            a_ptr->stat_bonus, a_ptr->stat_bonus_set,
            a_ptr->skill_bonus, a_ptr->skill_bonus_set);
    }

    /* Process 'A' for "Activation & time" */
    else if (buf[0] == 'A')
    {
        int ptime, prand;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

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

        /* Get the activation */
        if (grab_one_activation(a_ptr, buf + 2))
            return (PARSE_ERROR_GENERIC);

        /* Scan for the values */
        if (2 != sscanf(s, "%d:%d", &ptime, &prand))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        a_ptr->time = ptime;
        a_ptr->randtime = prand;
    }

    /* Process 'B' for "aBilities" (one line only) */
    /* Format: B:skilltype/abilitynum/banetype:skilltype/abilitynum/banetype:... */
    /* The banetype is optional (defaults to 0 = player choice) */
    else if (buf[0] == 'B')
    {
        int i;
        char* u;

        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum and bane_type */
            a_ptr->abilitynum[i] = 0;
            a_ptr->bane_type[i] = 0;

            /* Store the skilltype */
            a_ptr->skilltype[i] = atoi(s + 1);

            /* List this ability */
            a_ptr->abilities++;

            /* Find the first slash (abilitynum) */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int abilitynum = atoi(t + 1);
                if (abilitynum > 0)
                    a_ptr->abilitynum[i] = abilitynum;

                /* Look for a second slash (bane_type) */
                u = strchr(t + 1, '/');
                if (u && (!s || u < s))
                {
                    int banetype = atoi(u + 1);
                    if (banetype > 0)
                        a_ptr->bane_type[i] = banetype;
                }
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current a_ptr */
        if (!a_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&a_ptr->text, head, s))
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

/*
 * Add a name to the probability tables
 */
static errr build_prob(char* name, names_type* n_ptr)
{
    int c_prev, c_cur, c_next;

    while (*name && !isalpha((unsigned char)*name))
        ++name;

    if (!*name)
        return PARSE_ERROR_GENERIC;

    c_prev = c_cur = S_WORD;

    do
    {
        if (isalpha((unsigned char)*name))
        {
            c_next = A2I(tolower((unsigned char)*name));
            n_ptr->lprobs[c_prev][c_cur][c_next]++;
            n_ptr->ltotal[c_prev][c_cur]++;
            c_prev = c_cur;
            c_cur = c_next;
        }
    } while (*++name);

    n_ptr->lprobs[c_prev][c_cur][E_WORD]++;
    n_ptr->ltotal[c_prev][c_cur]++;

    return 0;
}

/*
 * Initialize the "n_info" array, by parsing an ascii "template" file
 */
errr parse_n_info(char* buf, header* head)
{
    names_type* n_ptr = head->info_ptr;

    /*
     * This function is called once, when the raw file does not exist.
     * If you want to initialize some stuff before parsing the txt file
     * you can do:
     *
     * static int do_init = 1;
     *
     * if (do_init)
     * {
     *    do_init = 0;
     *    ...
     *    do_stuff_with_n_ptr
     *    ...
     * }
     *
     */

    if (buf[0] == 'N')
    {
        return build_prob(buf + 2, n_ptr);
    }

    /*
     * If you want to do something after parsing the file you can add
     * a special directive at the end of the txt file, like:
     *
     * else
     * if (buf[0] == 'X')          (Only at the end of the txt file)
     * {
     *    ...
     *    do_something_else_with_n_ptr
     *    ...
     * }
     *
     */
    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }
}

static byte skeleton_note_parse_sval_token(const char* tok, bool* ok)
{
    if (ok)
        *ok = false;
    if (!tok || !*tok)
        return 0;
    if (streq(tok, "ELF"))
    {
        if (ok) *ok = true;
        return SV_SKELETON_ELF;
    }
    if (streq(tok, "HUMAN"))
    {
        if (ok) *ok = true;
        return SV_SKELETON_HUMAN;
    }
    if (streq(tok, "ORC"))
    {
        if (ok) *ok = true;
        return SV_SKELETON_ORC;
    }
    if (streq(tok, "ANY"))
    {
        if (ok) *ok = true;
        return SV_SKELETON_NOTE_ANY;
    }
    return 0;
}

static byte skeleton_note_parse_hint_token(const char* tok)
{
    if (!tok)
        return SKEL_HINT_NONE;
    if (streq(tok, "GREAT_VAULT"))
        return SKEL_HINT_GREAT_VAULT;
    if (streq(tok, "VAULT_ARTIFACT"))
        return SKEL_HINT_VAULT_ARTIFACT;
    if (streq(tok, "STAIRS"))
        return SKEL_HINT_STAIRS;
    if (streq(tok, "PARTITION"))
        return SKEL_HINT_PARTITION_PRESENCE;
    if (streq(tok, "FORGE"))
        return SKEL_HINT_FORGE;
    if (streq(tok, "UNIQUE"))
        return SKEL_HINT_UNIQUE_MONSTER;
    if (streq(tok, "TIP"))
        return SKEL_HINT_TIP;
    if (streq(tok, "SIZE"))
        return SKEL_HINT_LEVEL_SIZE;
    if (streq(tok, "QUEST"))
        return SKEL_HINT_QUEST;
    if (streq(tok, "LABYRINTH"))
        return SKEL_HINT_PART_LABYRINTH;
    if (streq(tok, "CHASM"))
        return SKEL_HINT_PART_CHASM;
    if (streq(tok, "CAVE"))
        return SKEL_HINT_PART_CAVE;
    if (streq(tok, "CAVE_ICE"))
        return SKEL_HINT_PART_CAVE_ICE;
    if (streq(tok, "CAVE_FIRE"))
        return SKEL_HINT_PART_CAVE_FIRE;
    if (streq(tok, "CAVE_POIS"))
        return SKEL_HINT_PART_CAVE_POIS;
    if (streq(tok, "ROOMY"))
        return SKEL_HINT_PART_ROOMY;
    if (streq(tok, "RUINED"))
        return SKEL_HINT_PART_RUINED;
    if (streq(tok, "CAVEY"))
        return SKEL_HINT_PART_CAVEY;
    return SKEL_HINT_NONE;
}

/*
 * Parse skeleton_note.txt
 *
 * Formats:
 *   O:<SVAL>:<weight>:<text>
 *   C:<SVAL>:<weight>:<text>
 *   M:<SVAL>:<HINT>:<weight>:<text>
 *
 * SVAL may be ELF/HUMAN/ORC/ANY
 * HINT may be GREAT_VAULT/VAULT_ARTIFACT/STAIRS/PARTITION/FORGE/UNIQUE/TIP/SIZE/QUEST/LABYRINTH/CHASM/CAVE/CAVE_ICE/CAVE_FIRE/CAVE_POIS/ROOMY/RUINED/CAVEY
 * Weight is optional (defaults to 100) and clamped to a byte.
 */
errr parse_skeleton_note_info(char* buf, header* head)
{
    static int next_idx = 0;
    skeleton_note_role role = SKELETON_NOTE_ROLE_NONE;
    char buf_copy[1024];

    strnfmt(buf_copy, sizeof(buf_copy), "%s", buf);

    /* Reset per-file */
    if (error_idx < 0)
        next_idx = 0;

    if (!buf[0] || buf[0] == '#')
        return 0;

    if (buf[0] == 'O')
        role = SKELETON_NOTE_ROLE_OPENING;
    else if (buf[0] == 'C')
        role = SKELETON_NOTE_ROLE_SIGNOFF;
    else if (buf[0] == 'M')
        role = SKELETON_NOTE_ROLE_HINT;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    if (next_idx >= head->info_num)
        return PARSE_ERROR_TOO_MANY_ENTRIES;

    skeleton_note_template* note = (skeleton_note_template*)head->info_ptr + next_idx;

    char* cursor = buf + 2;
    char* sval_tok = cursor;
    char* sep = strchr(cursor, ':');
    if (!sep)
    {
        log_error("skeleton_note.txt: missing sval separator on line %d (buf='%s')",
            error_line, buf_copy);
        return PARSE_ERROR_GENERIC;
    }
    *sep = '\0';
    cursor = sep + 1;

    bool valid_sval = false;
    byte sval = skeleton_note_parse_sval_token(sval_tok, &valid_sval);
    if (!valid_sval)
    {
        log_error("skeleton_note.txt: invalid sval '%s' on line %d (buf='%s')",
            sval_tok, error_line, buf_copy);
        return PARSE_ERROR_GENERIC;
    }

    byte hint = SKEL_HINT_NONE;
    if (role == SKELETON_NOTE_ROLE_HINT)
    {
        char* hint_tok = cursor;
        sep = strchr(cursor, ':');
        if (!sep)
        {
            log_error("skeleton_note.txt: missing hint separator on line %d (buf='%s')",
                error_line, buf_copy);
            return PARSE_ERROR_GENERIC;
        }
        *sep = '\0';
        cursor = sep + 1;
        hint = skeleton_note_parse_hint_token(hint_tok);
        if (hint == SKEL_HINT_NONE)
        {
            log_error("skeleton_note.txt: invalid hint '%s' on line %d (buf='%s')",
                hint_tok, error_line, buf_copy);
            return PARSE_ERROR_INVALID_FLAG;
        }
    }

    long weight = 100;
    sep = strchr(cursor, ':');
    if (sep)
    {
        *sep = '\0';
        weight = atol(cursor);
        cursor = sep + 1;
    }
    else
    {
        cursor = cursor;
    }

    if (weight < 0 || weight > 255)
    {
        log_error("skeleton_note.txt: weight out of bounds (%ld) on line %d (buf='%s')",
            weight, error_line, buf_copy);
        return PARSE_ERROR_OUT_OF_BOUNDS;
    }

    if (!cursor || cursor[0] == '\0')
    {
        log_error("skeleton_note.txt: missing text payload on line %d (buf='%s')",
            error_line, buf_copy);
        return PARSE_ERROR_GENERIC;
    }

    note->sval = sval;
    note->hint = hint;
    note->role = role;
    note->weight = (byte)weight;

    if (!add_text(&note->text, head, cursor))
        return PARSE_ERROR_OUT_OF_MEMORY;

    next_idx++;
    error_idx = next_idx;
    return 0;
}

/*
 * Grab one flag in a special item_type from a textual string
 */
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
            e_ptr->stat_bonus_min[si] = 0;
            e_ptr->stat_bonus[si] = 0;
            e_ptr->stat_bonus_set[si] = false;
        }
        for (int sk = 0; sk < S_MAX; sk++)
        {
            e_ptr->skill_bonus_min[sk] = 0;
            e_ptr->skill_bonus[sk] = 0;
            e_ptr->skill_bonus_set[sk] = false;
        }

        /* Start with the first of the tval indices */
        cur_t = 0;

        /* Reset allocation tracking */
        e_ptr->alloc_count = 0;
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

    /* Process 'T' for "Types allowed" (up to EGO_TVALS_MAX lines) */
    else if (buf[0] == 'T')
    {
        int tval, sval1, sval2;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval1, &sval2))
            return (PARSE_ERROR_GENERIC);

        /* Allow only a limited number of T: lines */
        if (cur_t >= EGO_TVALS_MAX)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        e_ptr->tval[cur_t] = (byte)tval;
        e_ptr->min_sval[cur_t] = (byte)sval1;
        e_ptr->max_sval[cur_t] = (byte)sval2;

        /* Increase counter for 'possible tval' index */
        cur_t++;
    }

    /* Hack -- Process 'C' for "creation" */
    else if (buf[0] == 'C')
    {
        int max_att, to_dd, to_ds, max_evn, to_pd, to_ps, pv;
        int min_pv = 0;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values (8th field min_pval is optional) */
        int fields = sscanf(buf + 2, "%d:%d:%d:%d:%d:%d:%d:%d", &max_att, &to_dd, &to_ds,
            &max_evn, &to_pd, &to_ps, &pv, &min_pv);
        if (fields < 7)
            return (PARSE_ERROR_GENERIC);

        e_ptr->max_att = max_att;
        e_ptr->to_dd = to_dd;
        e_ptr->to_ds = to_ds;
        e_ptr->max_evn = max_evn;
        e_ptr->to_pd = to_pd;
        e_ptr->to_ps = to_ps;
        e_ptr->max_pval = pv;
        e_ptr->min_pval = (byte)min_pv;

        /* If ego grants pval (max_pval > 0) but min_pval is 0, default to 1 */
        if (e_ptr->max_pval > 0 && e_ptr->min_pval == 0)
            e_ptr->min_pval = 1;
    }

    /* Process 'M' for per-stat/skill bonus ranges (one per line) */
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
        int min_value = 0;
        int max_value = 0;

        if (!parse_bonus_value_range(s, &min_value, &max_value))
            return (PARSE_ERROR_GENERIC);

        if (!apply_ego_bonus_token_range(token, min_value, max_value,
                &e_ptr->flags1,
                e_ptr->stat_bonus_min, e_ptr->stat_bonus, e_ptr->stat_bonus_set,
                e_ptr->skill_bonus_min, e_ptr->skill_bonus, e_ptr->skill_bonus_set))
        {
            return (PARSE_ERROR_GENERIC);
        }
    }

    /* Process 'B' for "aBilities" (one line only) */
    else if (buf[0] == 'B')
    {
        int i;

        /* There better be a current e_ptr */
        if (!e_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            e_ptr->abilitynum[i] = 0;

            /* Store the skilltype */
            e_ptr->skilltype[i] = atoi(s + 1);

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
                    e_ptr->abilitynum[i] = abilitynum;
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

/*
 * Grab one flag in a player_race from a textual string
 *
 * Sil:  these used to be the TR1, TR2 and TR3 flags,
 *       but we now use the race/character flags (RHF).
 */
static errr grab_one_race_flag(player_race* ptr, cptr what)
{
    u32b* f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));
    f[RHF] = &(ptr->flags);
    return grab_one_flag(f, "player", what);
}

/*
 * Grab one flag in a character_profile from a textual string
 *
 * Sil:  these used to be the TR1, TR2 and TR3 flags,
 *       but we now use the race/character flags (RHF).
 */
static errr grab_one_character_flag(character_profile *ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));

    f[RHF] = &(ptr->flags);

    return grab_one_flag(f, "player character", what);
}

static errr grab_one_character_uflag(character_profile *ptr, cptr what)
{
    u32b *f[MAX_FLAG_SETS];
    memset(f, 0, sizeof(f));

    f[UNQ] = &(ptr->flags_u);      /* NEW: accept unique-flag word */

    return grab_one_flag(f, "player character", what);
}

/*
 * Initialize the "p_info" array, by parsing an ascii "template" file
 */
errr parse_p_info(char* buf, header* head)
{
    int i, j;

    char *s, *t;

    /* Current entry */
    static player_race* pr_ptr = NULL;
    static int cur_equip = 0;

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
        pr_ptr = (player_race*)head->info_ptr + i;

        /* Store the name */
        if (!(pr_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        cur_equip = 0;
    }

    /* Process 'Q' for stable GUID */
    else if (buf[0] == 'Q')
    {
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        u64b guid = 0;
        if (!parse_u64b_hex(buf + 2, &guid))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->guid = score_guid_from_u64(guid);
    }

    /* Process 'S' for "Stats" (one line only) */
    else if (buf[0] == 'S')
    {
        int adj;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Start the string */
        s = buf + 1;

        /* For each stat */
        for (j = 0; j < A_MAX; j++)
        {
            /* Find the colon before the subindex */
            s = strchr(s, ':');

            /* Verify that colon */
            if (!s)
                return (PARSE_ERROR_GENERIC);

            /* Nuke the colon, advance to the subindex */
            *s++ = '\0';

            /* Get the value */
            adj = atoi(s);

            /* Save the value */
            pr_ptr->r_adj[j] = adj;

            /* Next... */
            continue;
        }
    }

    /* Hack -- Process 'I' for "info" and such */
    else if (buf[0] == 'I')
    {
        int hist, b_age, m_age;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &hist, &b_age, &m_age))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->hist = hist;
        pr_ptr->b_age = b_age;
        pr_ptr->m_age = m_age;
    }

    /* Hack -- Process 'H' for "Height" */
    else if (buf[0] == 'H')
    {
        int b_ht, m_ht;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &b_ht, &m_ht))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->b_ht = b_ht;
        pr_ptr->m_ht = m_ht;
    }

    /* Hack -- Process 'W' for "Weight" */
    else if (buf[0] == 'W')
    {
        int b_wt, m_wt;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &b_wt, &m_wt))
            return (PARSE_ERROR_GENERIC);

        pr_ptr->b_wt = b_wt;
        pr_ptr->m_wt = m_wt;
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current pr_ptr */
        if (!pr_ptr)
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
            if (0 != grab_one_race_flag(pr_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'E' for "Starting Equipment" */
    else if (buf[0] == 'E')
    {
        int tval, sval, min, max;

        start_item* e_ptr;

        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Access the item */
        e_ptr = &pr_ptr->start_items[cur_equip];

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
            return (PARSE_ERROR_GENERIC);

        if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
            return (PARSE_ERROR_INVALID_ITEM_NUMBER);

        /* Save the values */
        e_ptr->tval = tval;
        e_ptr->sval = sval;
        e_ptr->min = min;
        e_ptr->max = max;

        /* Next item */
        cur_equip++;

        /* Limit number of starting items */
        if (cur_equip > MAX_START_ITEMS)
            return (PARSE_ERROR_GENERIC);
    }

    /* Hack -- Process 'C' for character choices */
    else if (buf[0] == 'C')
    {
        /* There better be a current pr_ptr */
        if (!pr_ptr)
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

            int bit = atoi(s);   // Converts the string (e.g. "42") to an int
            if (bit >= 0 && bit < FLAG_COUNT) {
                int word = bit / 32;        // Which 32-bit slot (0 or 1)
                int shift = bit % 32;       // Which bit in that slot (0–31)
                pr_ptr->choice[word] |= (1U << shift);  // Set the bit
            } else {
                // Invalid flag index
            }

            /* Start the next entry */
            s = t;
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current pr_ptr */
        if (!pr_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(pr_ptr->text), head, s))
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

/*
 * Initialize the "c_info" array, by parsing an ascii "template" file
 */
// errr parse_c_info(char* buf, header* head)
// {
//     int i, j;
//     static int cur_equip = 0;

//     char *s, *t;

//     /* Current entry */
//     static character_profile* ph_ptr = NULL;

//     log_debug("Parsing characters");

//     /* Process 'N' for "New/Number/Name" */
//     if (buf[0] == 'N')
//     {
//         char *s;
//         int   idx, j;

//         /* Find the colon before the name */
//         s = strchr(buf + 2, ':');
//         if (!s) return (PARSE_ERROR_GENERIC);

//         /* Split and advance to the name text */
//         *s++ = '\0';
//         if (!*s) return (PARSE_ERROR_GENERIC);

//         /* Parse the index */
//         idx = atoi(buf + 2);
//         if (idx <= error_idx)            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
//         if (idx >= head->info_num)       return (PARSE_ERROR_TOO_MANY_ENTRIES);
//         error_idx = idx;

//         /* Point at this slot */
//         ph_ptr = (character_profile*)head->info_ptr + idx;

//         /* Store the name offset */
//         if (!(ph_ptr->name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);

//         /* Debug: announce new character and its name */
//         log_debug("New character #%d: \"%s\"", idx,
//                 head->name_ptr + ph_ptr->name);

//         /* Sentinel‐initialize all ability slots to “empty” */
//         for (j = 0; j < CHARACTER_ABILITY_MAX; j++)
//         {
//             ph_ptr->a_adj[j][0] = -1;
//             ph_ptr->a_adj[j][1] = -1;
//         }
//         log_debug("  a_adj slots 0..%d set to -1", CHARACTER_ABILITY_MAX - 1);
//     }

//     /* Process 'A' for "Alternate Name" */
//     else if (buf[0] == 'A')
//     {
//         /* Find the colon before the name */
//         s = strchr(buf, ':');

//         /* Verify that colon */
//         if (!s)
//             return (PARSE_ERROR_GENERIC);

//         /* Nuke the colon, advance to the name */
//         *s++ = '\0';

//         /* Paranoia -- require a name */
//         if (!*s)
//             return (PARSE_ERROR_GENERIC);

//         /* Store the name */
//         if (!(ph_ptr->alt_name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }

//     /* Process 'B' for "Short Name" */
//     else if (buf[0] == 'B')
//     {
//         /* Find the colon before the name */
//         s = strchr(buf, ':');

//         /* Verify that colon */
//         if (!s)
//             return (PARSE_ERROR_GENERIC);

//         /* Nuke the colon, advance to the name */
//         *s++ = '\0';

//         /* Paranoia -- require a name */
//         if (!*s)
//             return (PARSE_ERROR_GENERIC);

//         /* Store the name */
//         if (!(ph_ptr->short_name = add_name(head, s)))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }

//     /* Process 'S' for "Stats" (one line only) */
//     else if (buf[0] == 'S')
//     {
//         int adj;

//         /* There better be a current ph_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Start the string */
//         s = buf + 1;

//         /* For each stat */
//         for (j = 0; j < A_MAX; j++)
//         {
//             /* Find the colon before the subindex */
//             s = strchr(s, ':');

//             /* Verify that colon */
//             if (!s)
//                 return (PARSE_ERROR_GENERIC);

//             /* Nuke the colon, advance to the subindex */
//             *s++ = '\0';

//             /* Get the value */
//             adj = atoi(s);

//             /* Save the value */
//             ph_ptr->h_adj[j] = adj;

//             /* Next... */
//             continue;
//         }
//     }

//     /* Hack -- Process 'F' for flags */
//     else if (buf[0] == 'F')
//     {
//         /* There better be a current pr_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Parse every entry textually */
//         for (s = buf + 2; *s;)
//         {
//             /* Find the end of this entry */
//             for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) /* loop */
//                 ;

//             /* Nuke and skip any dividers */
//             if (*t)
//             {
//                 *t++ = '\0';
//                 while ((*t == ' ') || (*t == '|'))
//                     t++;
//             }

//             /* Parse this entry */
//             if (0 != grab_one_character_flag(ph_ptr, s))
//                 return (PARSE_ERROR_INVALID_FLAG);

//             /* Start the next entry */
//             s = t;
//         }
//     }

//     /* ------------------------------------------------------------ */
//     /* U: list of Unique flags        */
//     /* ------------------------------------------------------------ */
//     else if (buf[0] == 'U')
//     {
//         if (!ph_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

//         for (s = buf + 2; *s; )
//         {
//             /* token = [^ or |]*  */
//             for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
//             if (*t)
//             {
//                 *t++ = '\0';
//                 while ((*t == ' ') || (*t == '|')) t++;
//             }

//             if (grab_one_character_uflag(ph_ptr, s))
//                 return PARSE_ERROR_INVALID_FLAG;

//             s = t;
//         }
//     }

//         /* Process 'E' for "Starting Equipment" */
//     else if (buf[0] == 'E')
//     {
//         int tval, sval, min, max;

//         start_item* e_ptr;

//         /* There better be a current pr_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Access the item */
//         e_ptr = &ph_ptr->start_items[cur_equip];

//         /* Scan for the values */
//         if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
//             return (PARSE_ERROR_GENERIC);

//         if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
//             return (PARSE_ERROR_INVALID_ITEM_NUMBER);

//         /* Save the values */
//         e_ptr->tval = tval;
//         e_ptr->sval = sval;
//         e_ptr->min = min;
//         e_ptr->max = max;

//         /* Next item */
//         cur_equip++;

//         /* Limit number of starting items */
//         if (cur_equip > MAX_START_ITEMS)
//             return (PARSE_ERROR_GENERIC);
//     }


//     /* Process 'D' for "Description" */
//     else if (buf[0] == 'D')
//     {
//         /* There better be a current ph_ptr */
//         if (!ph_ptr)
//             return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Get the text */
//         s = buf + 2;

//         /* Store the text */
//         if (!add_text(&(ph_ptr->text), head, s))
//             return (PARSE_ERROR_OUT_OF_MEMORY);
//     }
//     /* Process 'C' for character ability entries */
//     else if (buf[0] == 'C')
//     {
//         char *t = buf + 1;
//         int   pair = 0;

//         if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

//         /* Debug: which character we’re parsing into */
//         log_debug("Parsing abilities for character \"%s\"…",
//                 head->name_ptr + ph_ptr->name);

//         /* Read up to CHARACTER_ABILITY_MAX of “:stat:ability” pairs */
//         while (pair < CHARACTER_ABILITY_MAX)
//         {
//             /* stat */
//             t = strchr(t, ':');
//             if (!t) break;
//             *t++ = '\0';
//             ph_ptr->a_adj[pair][0] = (s16b)atoi(t);

//             /* ability */
//             t = strchr(t, ':');
//             if (!t) break;
//             *t++ = '\0';
//             ph_ptr->a_adj[pair][1] = (s16b)atoi(t);

//             log_debug("  parsed slot %d -> stat=%d ability=%d",
//                     pair,
//                     ph_ptr->a_adj[pair][0],
//                     ph_ptr->a_adj[pair][1]);

//             pair++;
//         }

//         log_debug("  total %d ability pairs parsed", pair);
//     }

//     else
//     {
//         /* Oops */
//         return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
//     }

//     /* Success */
//     return (0);
// }

errr parse_c_info(char* buf, header* head)
{
    int j;
    static int cur_equip = 0;

    char *s, *t;

    /* Current entry */
    static character_profile* ph_ptr = NULL;

    log_trace("Parsing characters");

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        char *s;
        int   idx, j;

        /* Find the colon before the name */
        s = strchr(buf + 2, ':');
        if (!s) return (PARSE_ERROR_GENERIC);

        /* Split and advance to the name text */
        *s++ = '\0';
        if (!*s) return (PARSE_ERROR_GENERIC);

        /* Parse the index */
        idx = atoi(buf + 2);
        if (idx <= error_idx)            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (idx >= head->info_num)       return (PARSE_ERROR_TOO_MANY_ENTRIES);
        error_idx = idx;

        /* Point at this slot */
        ph_ptr = (character_profile*)head->info_ptr + idx;

        /* RESET equipment counter for new character */
        cur_equip = 0;

        /* Initialize power to default value 1 (average) */
        ph_ptr->power = 1;

        /* Store the name offset */
        if (!(ph_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Debug: announce new character and its name */
        log_trace("New character #%d: \"%s\"", idx,
                head->name_ptr + ph_ptr->name);

        /* Sentinel‐initialize all ability slots to "empty" */
        for (j = 0; j < CHARACTER_ABILITY_MAX; j++)
        {
            ph_ptr->a_adj[j][0] = -1;
            ph_ptr->a_adj[j][1] = -1;
        }
        log_trace("  a_adj slots 0..%d set to -1", CHARACTER_ABILITY_MAX - 1);

        /* Initialize starting items array */
        for (j = 0; j < MAX_START_ITEMS; j++)
        {
            ph_ptr->start_items[j].tval = 0;
            ph_ptr->start_items[j].sval = 0;
            ph_ptr->start_items[j].min = 0;
            ph_ptr->start_items[j].max = 0;
        }
        log_debug("  start_items array initialized");
    }

    /* Process 'Q' for stable GUID */
    else if (buf[0] == 'Q')
    {
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        u64b guid = 0;
        if (!parse_u64b_hex(buf + 2, &guid))
            return (PARSE_ERROR_GENERIC);

        ph_ptr->guid = score_guid_from_u64(guid);
    }

    /* Process 'A' for "Alternate Name" */
    else if (buf[0] == 'A')
    {
        /* Find the colon before the name */
        s = strchr(buf, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Store the name */
        if (!(ph_ptr->alt_name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'B' for "Start String" */
    else if (buf[0] == 'B')
    {
        /* Find the colon before the name */
        s = strchr(buf, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Store the name */
        if (!(ph_ptr->start_string = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'S' for "Stats" (one line only) */
    else if (buf[0] == 'S')
    {
        int adj;

        /* There better be a current ph_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Start the string */
        s = buf + 1;

        /* For each stat */
        for (j = 0; j < A_MAX; j++)
        {
            /* Find the colon before the subindex */
            s = strchr(s, ':');

            /* Verify that colon */
            if (!s)
                return (PARSE_ERROR_GENERIC);

            /* Nuke the colon, advance to the subindex */
            *s++ = '\0';

            /* Get the value */
            adj = atoi(s);

            /* Save the value */
            ph_ptr->h_adj[j] = adj;

            /* Next... */
            continue;
        }
    }

    /* Hack -- Process 'F' for flags */
    else if (buf[0] == 'F')
    {
        /* There better be a current pr_ptr */
        if (!ph_ptr)
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
            if (0 != grab_one_character_flag(ph_ptr, s))
                return (PARSE_ERROR_INVALID_FLAG);

            /* Start the next entry */
            s = t;
        }
    }

    /* ------------------------------------------------------------ */
    /* U: list of Unique flags        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'U')
    {
        if (!ph_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_character_uflag(ph_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }

        /* Process 'E' for "Starting Equipment" */
    else if (buf[0] == 'E')
    {
        int tval, sval, min, max;

        start_item* e_ptr;

        /* There better be a current pr_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Check if we've exceeded the maximum number of items */
        if (cur_equip >= MAX_START_ITEMS)
        {
            log_debug("Warning: Too many starting items for character (max %d), ignoring", MAX_START_ITEMS);
            return (PARSE_ERROR_GENERIC);
        }

        /* Access the item */
        e_ptr = &ph_ptr->start_items[cur_equip];

        /* Scan for the values */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &tval, &sval, &min, &max))
            return (PARSE_ERROR_GENERIC);

        if ((min < 0) || (max < 0) || (min > 99) || (max > 99))
            return (PARSE_ERROR_INVALID_ITEM_NUMBER);

        /* Save the values */
        e_ptr->tval = tval;
        e_ptr->sval = sval;
        e_ptr->min = min;
        e_ptr->max = max;

        /* Debug: show what we parsed */
        log_debug("  Equipment slot %d: tval=%d sval=%d min=%d max=%d", 
                cur_equip, tval, sval, min, max);

        /* Next item */
        cur_equip++;
    }


    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current ph_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(ph_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }
    /* Process 'C' for character ability entries */
    else if (buf[0] == 'C')
    {
        char *t = buf + 2; /* Skip 'C:' */
        int   pair = 0;
        char *stat_start, *ability_start;

        if (!ph_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Debug: which character we're parsing into */
        log_debug("Parsing abilities for character \"%s\" from line: %s",
                head->name_ptr + ph_ptr->name, buf);

        /* Read up to CHARACTER_ABILITY_MAX of ":stat:ability" pairs */
        while (pair < CHARACTER_ABILITY_MAX && t && *t)
        {
            /* Find first colon for stat */
            if (*t == ':') t++; /* Skip leading colon if present */
            stat_start = t;
            
            t = strchr(t, ':');
            if (!t) break;
            *t++ = '\0';
            
            if (!*stat_start) break; /* Empty stat */
            ph_ptr->a_adj[pair][0] = (s16b)atoi(stat_start);

            /* Find second colon for ability */
            ability_start = t;
            t = strchr(t, ':');
            if (t) {
                *t++ = '\0';
            }
            
            if (!*ability_start) break; /* Empty ability */
            ph_ptr->a_adj[pair][1] = (s16b)atoi(ability_start);

            log_debug("  parsed slot %d -> stat=%d ability=%d",
                    pair,
                    ph_ptr->a_adj[pair][0],
                    ph_ptr->a_adj[pair][1]);

            pair++;
            
            /* If no more colons, we're done */
            if (!t) break;
        }

        log_debug("  total %d ability pairs parsed", pair);
    }

    /* Process 'P' for "Power" */
    else if (buf[0] == 'P')
    {
        /* There better be a current ph_ptr */
        if (!ph_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse the power value */
        ph_ptr->power = (byte)atoi(buf + 2);
        
        log_debug("  power set to %d", ph_ptr->power);
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

/*
 * Initialize the "h_info" array, by parsing an ascii "template" file
 */
errr parse_h_info(char* buf, header* head)
{
    int i;

    char* s;

    /* Current entry */
    static hist_type* h_ptr = NULL;

    /* Process 'N' for "New/Number" */
    if (buf[0] == 'N')
    {
        int prv, nxt, prc, hou;

        /* Hack - get the index */
        i = error_idx + 1;

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        h_ptr = (hist_type*)head->info_ptr + i;

        /* Scan for the values */
        if (4 != sscanf(buf, "N:%d:%d:%d:%d", &prv, &nxt, &prc, &hou))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        h_ptr->chart = prv;
        h_ptr->next = nxt;
        h_ptr->roll = prc;
        h_ptr->character = hou;
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current h_ptr */
        if (!h_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&h_ptr->text, head, s))
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

/*
 * Initialize the "st_info" array, by parsing an ascii "template" file
 */
errr parse_st_info(char* buf, header* head)
{
    int i;

    char* s;

    /* Current entry */
    static story_type* st_ptr = NULL;

    /* Process 'N' for "New/Number" */
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
        st_ptr = (story_type*)head->info_ptr + i;
        memset(st_ptr, 0, sizeof(story_type));

        /* Store the name */
        if (!(st_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Sensible defaults */
        st_ptr->st_type  = 0;
        st_ptr->order    = 0;
        st_ptr->runtypes = 0;   /* 0 == ALL runtypes */
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current st_ptr */
        if (!st_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&st_ptr->text, head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }
    /* Process 'T' for type (byte) */
    else if (buf[0] == 'T')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        int t;
        if (1 != sscanf(buf + 2, "%d", &t)) return (PARSE_ERROR_GENERIC);
        if (t < 0 || t > 255)               return (PARSE_ERROR_OUT_OF_BOUNDS);
        st_ptr->st_type = (byte)t;
    }
    /* Process 'O' for order (byte) */
    else if (buf[0] == 'O')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        int o;
        if (1 != sscanf(buf + 2, "%d", &o)) return (PARSE_ERROR_GENERIC);
        if (o < 0 || o > 255)               return (PARSE_ERROR_OUT_OF_BOUNDS);
        st_ptr->order = (byte)o;
    }
    /* Process 'R' for runtypes: "*" or "i|j|k" */
    else if (buf[0] == 'R')
    {
        if (!st_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        s = buf + 2;
        while (*s == ' ' || *s == '\t') s++;

        /* "*" => all runtypes (store 0 to mean ALL) */
        if (*s == '*')
        {
            st_ptr->runtypes = 0;  /* wildcard */
        }
        else
        {
            u32b mask = 0;
            char *tok = strtok(s, "|");
            while (tok)
            {
                int bit = atoi(tok);
                if (bit < 0 || bit >= 32)
                {
                    /* silently ignore out-of-range bits */
                }
                else
                {
                    mask |= (1UL << bit);
                }
                tok = strtok(NULL, "|");
            }
            st_ptr->runtypes = mask;
        }
    }
    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

/**********************************************************************
 * Initialise the “cu_info” array by parsing curses.txt
 **********************************************************************/
errr parse_cu_info(char *buf, header *head)
{
    int   i, j;
    char *s, *t;

    /* Current entry */
    static curse_type *cu_ptr = NULL;

    /* ------------------------------------------------------------ */
    /* N: idx : name                                                */
    /* ------------------------------------------------------------ */
    if (buf[0] == 'N')
    {
        /* Find name delimiter */
        s = strchr(buf + 2, ':');
        if (!s) return PARSE_ERROR_GENERIC;

        *s++ = '\0';
        if (!*s) return PARSE_ERROR_GENERIC;

        i = atoi(buf + 2);
        if (i <= error_idx)          return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (i >= head->info_num)     return PARSE_ERROR_TOO_MANY_ENTRIES;
        error_idx = i;

        cu_ptr = ((curse_type *)head->info_ptr) + i;

        /* Reset fresh record */
        memset(cu_ptr, 0, sizeof(curse_type));       /* clears the record  */
                                                     /* flags included     */
        cu_ptr->weight = 1;      /* sensible defaults           */
        cu_ptr->max_stacks = 0;  /* 0 = unlimited               */

        if (!(cu_ptr->name = add_name(head, s)))     
            return PARSE_ERROR_OUT_OF_MEMORY;
        cu_ptr->blessing_name = 0;  /* NULL unless B: directive sets it */
    }

    /* ------------------------------------------------------------ */
    /* B: blessing name                                             */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'B')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!(cu_ptr->blessing_name = add_name(head, buf + 2)))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* C: stat adjustments  or  S: (old name kept for back-compat)  */
    /* ------------------------------------------------------------ */
    else if ((buf[0] == 'C') || (buf[0] == 'S'))
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        s = buf + 1;                     /* points to first ':' */
        for (j = 0; j < A_MAX; j++)
        {
            s = strchr(s, ':');
            if (!s) return PARSE_ERROR_GENERIC;
            *s++ = '\0';
            cu_ptr->cu_adj[j] = atoi(s);
        }
    }

    /* ------------------------------------------------------------ */
    /* F: list of RHF flags (MEL_PENALTY | SWORD_AFFINITY …)        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'F')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_curse_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }
    /* ------------------------------------------------------------ */
    /* G: list of blessing RHF flags                                */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'G')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_blessing_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }
    /* ------------------------------------------------------------ */
    /* U: list of CUR flags        */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'U')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            /* token = [^ or |]*  */
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_curse_unique_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }
    /* ------------------------------------------------------------ */
    /* Y: list of blessing CUR flags                                */
    /* ------------------------------------------------------------ */
    /* V: is reserved for version stamps in data files; do not use. */
    else if (buf[0] == 'Y')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        for (s = buf + 2; *s; )
        {
            for (t = s; *t && (*t != ' ') && (*t != '|'); ++t) ;
            if (*t)
            {
                *t++ = '\0';
                while ((*t == ' ') || (*t == '|')) t++;
            }

            if (grab_one_blessing_unique_flag(cu_ptr, s))
                return PARSE_ERROR_INVALID_FLAG;

            s = t;
        }
    }

    /* ------------------------------------------------------------ */
    /* A: weight / max_stacks   (e.g. 3/5 means weight=3, max=5)    */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'A')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        /* default is "1/0" so zero-initialised files still work    */
        cu_ptr->weight     = 1;
        cu_ptr->max_stacks = 0;

        char *s = buf + 2;
        char *t = strchr(s, '/');
        if (!t) return PARSE_ERROR_GENERIC;

        *t++ = '\0';
        cu_ptr->weight     = (byte)atoi(s);
        cu_ptr->max_stacks = (byte)atoi(t);
    }


    /* ------------------------------------------------------------ */
    /* D: description line(s)                                       */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'D')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->text), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* E: blessing description line(s)                              */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'E')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->blessing_text), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* P: power/effect description                                   */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'P')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->power), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* H: blessing power/effect description                         */
    /* ------------------------------------------------------------ */
    else if (buf[0] == 'H')
    {
        if (!cu_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;

        if (!add_text(&(cu_ptr->blessing_power), head, buf + 2))
            return PARSE_ERROR_OUT_OF_MEMORY;
    }

    /* ------------------------------------------------------------ */
    /* anything else is an error                                    */
    /* ------------------------------------------------------------ */
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return 0;
}

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
errr parse_flavor_info(char* buf, header* head)
{
    int i;

    /* Current entry */
    static flavor_type* flavor_ptr;

    /* Process 'N' for "Number" */
    if (buf[0] == 'N')
    {
        int tval, sval;
        int result;

        /* Scan the value */
        result = sscanf(buf, "N:%d:%d:%d", &i, &tval, &sval);

        /* Either two or three values */
        if ((result != 2) && (result != 3))
            return (PARSE_ERROR_GENERIC);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        flavor_ptr = (flavor_type*)head->info_ptr + i;

        /* Save the tval */
        flavor_ptr->tval = (byte)tval;

        /* Save the sval */
        if (result == 2)
        {
            /* Megahack - unknown sval */
            flavor_ptr->sval = SV_UNKNOWN;
        }
        else
            flavor_ptr->sval = (byte)sval;
    }

    /* Process 'G' for "Graphics" */
    else if (buf[0] == 'G')
    {
        char d_char;
        int d_attr;

        /* There better be a current flavor_ptr */
        if (!flavor_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);
        if (!buf[3])
            return (PARSE_ERROR_GENERIC);
        if (!buf[4])
            return (PARSE_ERROR_GENERIC);

        /* Extract d_char */
        d_char = buf[2];

        /* If we have a longer string than expected ... */
        if (buf[5])
        {
            /* Advance "buf" on by 4 */
            buf += 4;

            /* Extract the colour */
            d_attr = color_text_to_attr(buf);
        }
        else
        {
            /* Extract the attr */
            d_attr = color_char_to_attr(buf[4]);
        }

        /* Paranoia */
        if (d_attr < 0)
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        flavor_ptr->d_attr = d_attr;
        flavor_ptr->d_char = d_char;
    }

    /* Process 'T' for "Tile" graphics (one line only) */
    else if (buf[0] == 'T')
    {
        /* There better be a current flavor_ptr */
        if (!flavor_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse and set tile coordinates */
        return parse_tile_line(buf, &flavor_ptr->x_attr, &flavor_ptr->x_char);
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current flavor_ptr */
        if (!flavor_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Paranoia */
        if (!buf[1])
            return (PARSE_ERROR_GENERIC);
        if (!buf[2])
            return (PARSE_ERROR_GENERIC);

        /* Store the text */
        if (!add_text(&flavor_ptr->text, head, buf + 2))
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

/*
 * Initialize the "effect" arrays (misc_to_attr, misc_to_char),
 * by parsing an ascii "template" file
 */
errr parse_effect_info(char* buf, header* head)
{
    int i;
    char* s;

    /* Current entry index */
    static int effect_idx = -1;
    effect_glyph* glyphs = (effect_glyph*)head->info_ptr;

    /* Process 'V' for "Version" */
    if (buf[0] == 'V')
    {
        return (0);
    }

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

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i < 0 || i >= 256)
            return (PARSE_ERROR_GENERIC);

        /* Save the index */
        effect_idx = i;
    }

    /* Process 'T' for "Tile" graphics */
    else if (buf[0] == 'T')
    {
        int row, col;

        /* Must have a valid effect index */
        if (effect_idx < 0)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse row and column */
        if (2 != sscanf(buf + 2, "%d:%d", &row, &col))
            return (PARSE_ERROR_GENERIC);

        /* Validate range (0-63 for 6-bit index) */
        if (row < 0 || row > 63)
            return (PARSE_ERROR_GENERIC);
        if (col < 0 || col > 63)
            return (PARSE_ERROR_GENERIC);

        if (!glyphs)
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Store in the raw-backed table (and update globals) */
        glyphs[effect_idx].a = (byte)(0x80 | row);
        glyphs[effect_idx].c = (byte)(0x80 | col);
        misc_to_attr[effect_idx] = glyphs[effect_idx].a;
        misc_to_char[effect_idx] = (char)glyphs[effect_idx].c;
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

/*
 * Initialize the "quest_info" array, by parsing a ascii "template" file
 */
errr parse_quest_info(char* buf, header* head)
{
    int i;
    char *s;

    /* Current entry */
    static quest_type* quest_ptr = NULL;

    /* Process 'N' for "New/Number/Name" or 'Q' for "Quest" */
    if (buf[0] == 'N' || buf[0] == 'Q')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s) return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s) return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        quest_ptr = (quest_type*)head->info_ptr + i;

        quest_ptr->quest_num = (byte)i;
        quest_ptr->vala_id = 0;
        quest_ptr->sequence = 1;
        quest_ptr->quest_flags = 0;
        quest_ptr->challenge_unlock = 0;
        quest_ptr->completion_cap = 0;

        /* Store the name */
        if (!(quest_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Title text" */
    else if (buf[0] == 'T')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store title text in dedicated field */
        if (!add_text(&(quest_ptr->title_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'C' for "Challenge text" */
    else if (buf[0] == 'C')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store challenge text in dedicated field */
        if (!add_text(&(quest_ptr->challenge_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'Z' for "Vala owner" */
    else if (buf[0] == 'Z')
    {
        char vala_name[32];
        int vala_id = 0;

        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (1 == sscanf(buf + 2, "%d", &vala_id))
        {
            /* Numeric Vala id parsed directly. */
        }
        else if (1 == sscanf(buf + 2, "%31s", vala_name))
        {
            if (SDL_strcasecmp(vala_name, "Tulkas") == 0) vala_id = VALA_TULKAS;
            else if (SDL_strcasecmp(vala_name, "Aule") == 0) vala_id = VALA_AULE;
            else if (SDL_strcasecmp(vala_name, "Mandos") == 0) vala_id = VALA_MANDOS;
            else if (SDL_strcasecmp(vala_name, "Nienna") == 0 || SDL_strcasecmp(vala_name, "Niena") == 0) vala_id = VALA_NIENNA;
            else if (SDL_strcasecmp(vala_name, "Orome") == 0) vala_id = VALA_OROME;
            else if (SDL_strcasecmp(vala_name, "Varda") == 0) vala_id = VALA_VARDA;
        }

        if (vala_id < 0 || vala_id > VALA_MAX) vala_id = 0;
        quest_ptr->vala_id = (byte)vala_id;
    }

    /* Process 'J' for "sequence" within a Vala quest chain */
    else if (buf[0] == 'J')
    {
        int seq;

        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        seq = atoi(buf + 2);
        if (seq < 1) seq = 1;
        if (seq > VALA_STAGES) seq = VALA_STAGES;
        quest_ptr->sequence = (byte)seq;
    }

    /* Process 'F' for "quest Flags" */
    else if (buf[0] == 'F')
    {
        char flagbuf[128];
        char *token;

        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        quest_ptr->quest_flags = 0;
        SDL_strlcpy(flagbuf, buf + 2, sizeof(flagbuf));
        token = strtok(flagbuf, " |,:;");
        while (token)
        {
            if (SDL_strcasecmp(token, "GLOBAL") == 0)
                quest_ptr->quest_flags |= QUEST_FLAG_GLOBAL;
            else if (SDL_strcasecmp(token, "OPTIONAL_CHAIN") == 0 ||
                     SDL_strcasecmp(token, "NO_CHAIN") == 0)
                quest_ptr->quest_flags |= QUEST_FLAG_OPTIONAL_CHAIN;
            token = strtok(NULL, " |,:;");
        }
    }

    /* Process 'H' for "Challenge unlock" */
    else if (buf[0] == 'H')
    {
        char challenge_name[32];
        int challenge_id = 0;

        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        if (1 == sscanf(buf + 2, "%d", &challenge_id))
        {
            /* Numeric challenge id parsed directly. */
        }
        else if (1 == sscanf(buf + 2, "%31s", challenge_name))
        {
            if (SDL_strcasecmp(challenge_name, "DISCONNECTED") == 0 || SDL_strcasecmp(challenge_name, "DISCON") == 0)
                challenge_id = CHALLENGE_DISCONNECTED;
            else if (SDL_strcasecmp(challenge_name, "SINGLE_STAIR") == 0 || SDL_strcasecmp(challenge_name, "SINGLE") == 0)
                challenge_id = CHALLENGE_SINGLE_STAIR;
            else if (SDL_strcasecmp(challenge_name, "FIXED_50K") == 0 || SDL_strcasecmp(challenge_name, "FIXED_50K_XP") == 0 || SDL_strcasecmp(challenge_name, "FIXED_XP") == 0)
                challenge_id = CHALLENGE_FIXED_50K_XP;
            else if (SDL_strcasecmp(challenge_name, "TULKAS_BLUNT") == 0 || SDL_strcasecmp(challenge_name, "BLUNT") == 0)
                challenge_id = CHALLENGE_TULKAS_BLUNT;
            else if (SDL_strcasecmp(challenge_name, "TORCHLIGHT") == 0 || SDL_strcasecmp(challenge_name, "TORCH") == 0)
                challenge_id = CHALLENGE_TORCHLIGHT;
        }

        if (challenge_id < 0 || challenge_id > CHALLENGE_MAX_TRACKED)
            challenge_id = 0;

        quest_ptr->challenge_unlock = (byte)challenge_id;
    }

    /* Process 'L' for "completion cap" */
    else if (buf[0] == 'L')
    {
        int cap;

        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        cap = atoi(buf + 2);
        if (cap < 0) cap = 0;
        if (cap > 255) cap = 255;
        quest_ptr->completion_cap = (byte)cap;
    }

    /* Process 'Y' for "quest tYpe" */
    else if (buf[0] == 'Y')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse quest type */
        quest_ptr->quest_type = atoi(buf + 2);
    }

    /* Process 'P' for "Parametric formula" */
    else if (buf[0] == 'P')
    {
        char formula_name[32];
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse formula: P:FORMULA_TYPE:param1:param2:param3:param4:depth_min:depth_max */
        /* Example: P:LINEAR_DECAY:27:0:0:0:6:19 */
        /*          P:SCALED_RANGE:0.125:14:5:0:14:25 */
        
        /* Initialize to hardcoded by default */
        quest_ptr->formula_type = FORMULA_HARDCODED;
        quest_ptr->formula_params[0] = 0.0f;
        quest_ptr->formula_params[1] = 0.0f;
        quest_ptr->formula_params[2] = 0.0f;
        quest_ptr->formula_params[3] = 0.0f;
        quest_ptr->depth_min = 0;
        quest_ptr->depth_max = 25;
        
        /* Parse formula name */
        if (sscanf(buf + 2, "%31[^:]", formula_name) == 1) {
            char *rest = strchr(buf + 2, ':');
            if (rest) {
                rest++; /* Skip the ':' */
                
                /* Determine formula type */
                if (SDL_strcasecmp(formula_name, "LINEAR_DECAY") == 0) {
                    quest_ptr->formula_type = FORMULA_LINEAR_DECAY;
                    /* Parse: base:unused:unused:unused (depth from E: field) */
                    sscanf(rest, "%f:%f:%f:%f", 
                           &quest_ptr->formula_params[0], &quest_ptr->formula_params[1], 
                           &quest_ptr->formula_params[2], &quest_ptr->formula_params[3]);
                } else if (SDL_strcasecmp(formula_name, "SCALED_RANGE") == 0) {
                    quest_ptr->formula_type = FORMULA_SCALED_RANGE;
                    /* Parse: max_prob:start_depth:range:unused (depth from E: field) */
                    sscanf(rest, "%f:%f:%f:%f", 
                           &quest_ptr->formula_params[0], &quest_ptr->formula_params[1], 
                           &quest_ptr->formula_params[2], &quest_ptr->formula_params[3]);
                } else if (SDL_strcasecmp(formula_name, "LINEAR_INTERPOLATE") == 0) {
                    quest_ptr->formula_type = FORMULA_LINEAR_INTERPOLATE;
                    /* Parse: min_prob:max_prob:unused:unused (depth from E: field) */
                    sscanf(rest, "%f:%f:%f:%f", 
                           &quest_ptr->formula_params[0], &quest_ptr->formula_params[1], 
                           &quest_ptr->formula_params[2], &quest_ptr->formula_params[3]);
                } else if (SDL_strcasecmp(formula_name, "FIXED_PERCENT") == 0) {
                    quest_ptr->formula_type = FORMULA_FIXED_PERCENT;
                    /* Parse: percentage:unused:unused:unused (depth from E: field) */
                    sscanf(rest, "%f:%f:%f:%f", 
                           &quest_ptr->formula_params[0], &quest_ptr->formula_params[1], 
                           &quest_ptr->formula_params[2], &quest_ptr->formula_params[3]);
                }
                /* Add more formula types here as needed */
            }
        }
    }

    /* Process 'O' for "Oath" */
    else if (buf[0] == 'O')
    {
        int oath_id;
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse oath ID directly as integer */
        oath_id = atoi(buf + 2);
        
        /* Validate oath ID range dynamically using z_info->oath_max */
        if (oath_id < 0 || !z_info || oath_id >= z_info->oath_max) {
            oath_id = 0; /* Default to no oath */
        }
        
        /* Store the oath ID */
        quest_ptr->oath_id = oath_id;
    }

    /* Process 'E' for "Eligibility requirements" */
    else if (buf[0] == 'E')
    {
        char requirement_type[32];
        int value1, value2, value3;
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        log_trace("QUEST PARSE: Processing E: field for quest %d: '%s'", quest_ptr->quest_num, buf);
        log_trace("QUEST PARSE: buf+2 = '%s'", buf + 2);

        /* Initialize eligibility fields */
        quest_ptr->eligibility_type = 0; /* Default: no requirements */
        quest_ptr->eligibility_skill = 0;
        quest_ptr->eligibility_value = 0;
        quest_ptr->eligibility_depth_min = 0;
        quest_ptr->eligibility_depth_max = 0;

        /* Parse eligibility requirements */
        /* Try parsing with skill name first: E:SKILL_MIN:SMT:10 */
        char skill_name[32];
        
        /* Debug: test the sscanf pattern */
        char test_input[] = "SKILL_MIN:SMT:10";
        char test_skill[32];
        int test_value;
        int test_result = sscanf(test_input, "SKILL_MIN:%31[^:]:%d", test_skill, &test_value);
        log_trace("QUEST PARSE: sscanf test on '%s' returned %d, skill='%s', value=%d", 
                 test_input, test_result, test_skill, test_value);
        
        if (2 == sscanf(buf + 2, "SKILL_MIN:%31[^:]:%d", skill_name, &value1))
        {
            quest_ptr->eligibility_type = 1; /* skill_min */
            
            log_trace("QUEST PARSE: Successfully parsed SKILL_MIN: skill='%s', value=%d", skill_name, value1);
            
            /* Map skill names to skill types */
            if (SDL_strcasecmp(skill_name, "MEL") == 0) {
                quest_ptr->eligibility_skill = S_MEL;
            } else if (SDL_strcasecmp(skill_name, "ARC") == 0) {
                quest_ptr->eligibility_skill = S_ARC;
            } else if (SDL_strcasecmp(skill_name, "EVN") == 0) {
                quest_ptr->eligibility_skill = S_EVN;
            } else if (SDL_strcasecmp(skill_name, "STL") == 0) {
                quest_ptr->eligibility_skill = S_STL;
            } else if (SDL_strcasecmp(skill_name, "PER") == 0) {
                quest_ptr->eligibility_skill = S_PER;
            } else if (SDL_strcasecmp(skill_name, "WIL") == 0) {
                quest_ptr->eligibility_skill = S_WIL;
            } else if (SDL_strcasecmp(skill_name, "SMT") == 0) {
                quest_ptr->eligibility_skill = S_SMT;
            } else if (SDL_strcasecmp(skill_name, "SNG") == 0) {
                quest_ptr->eligibility_skill = S_SNG;
            } else {
                quest_ptr->eligibility_skill = S_MEL; /* Default to Melee */
            }
            
            quest_ptr->eligibility_value = value1; /* minimum value */
            
            log_trace("QUEST PARSE: Set eligibility_type=1, eligibility_skill=%d, eligibility_value=%d", 
                     quest_ptr->eligibility_skill, quest_ptr->eligibility_value);
        }
        /* Fall back to numeric parsing */
        else if (3 == sscanf(buf + 2, "%31[^:]:%d:%d", requirement_type, &value1, &value2))
        {
            log_trace("QUEST PARSE: Trying numeric parsing: type='%s', value1=%d, value2=%d", requirement_type, value1, value2);
            
            /* Format: E:requirement_type:value1:value2 */
            if (streq(requirement_type, "SKILL_MIN"))
            {
                quest_ptr->eligibility_type = 1; /* skill_min */
                quest_ptr->eligibility_skill = value1; /* skill type (S_SMT, etc.) */
                quest_ptr->eligibility_value = value2; /* minimum value */
                
                log_trace("QUEST PARSE: Numeric SKILL_MIN - eligibility_type=1, eligibility_skill=%d, eligibility_value=%d", 
                         quest_ptr->eligibility_skill, quest_ptr->eligibility_value);
            }
            else if (streq(requirement_type, "DEPTH_RANGE"))
            {
                quest_ptr->eligibility_type = 3; /* depth_range */
                quest_ptr->eligibility_depth_min = value1; /* minimum depth */
                quest_ptr->eligibility_depth_max = value2; /* maximum depth */
                /* Also set for P: field formula calculations */
                quest_ptr->depth_min = value1;
                quest_ptr->depth_max = value2;
            }
        }
        else if (4 == sscanf(buf + 2, "%31[^:]:%d:%d:%d", requirement_type, &value1, &value2, &value3))
        {
            /* Format: E:requirement_type:value1:value2:value3 */
            if (streq(requirement_type, "SKILL_RANGE"))
            {
                quest_ptr->eligibility_type = 2; /* skill_range */
                quest_ptr->eligibility_skill = value1; /* skill type */
                quest_ptr->eligibility_depth_min = value2; /* min depth */
                quest_ptr->eligibility_depth_max = value3; /* max depth */
                /* Also set for P: field formula calculations */
                quest_ptr->depth_min = value2;
                quest_ptr->depth_max = value3;
            }
        }
        else
        {
            log_trace("QUEST PARSE: Failed to parse E: field '%s' - leaving as NO_REQUIREMENTS", buf);
        }
    }

    /* Process 'Y' for "Yarn/story" */
    else if (buf[0] == 'Y')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Add story text to description */
        if (!add_text(&(quest_ptr->text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'A' for "Ability reward" */
    else if (buf[0] == 'A')
    {
        int ability_type, ability_id;

        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &ability_type, &ability_id))
            return (PARSE_ERROR_GENERIC);

        /* Save the values in the new ability fields */
        quest_ptr->ability_type = ability_type;
        quest_ptr->ability_id = ability_id;
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the text */
        if (!add_text(&(quest_ptr->text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'S' for "Stat bonuses" */
    else if (buf[0] == 'S')
    {
        int str_bonus, dex_bonus, con_bonus, gra_bonus;
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse stat bonuses: S:str:dex:con:gra */
        if (4 == sscanf(buf + 2, "%d:%d:%d:%d", &str_bonus, &dex_bonus, &con_bonus, &gra_bonus))
        {
            quest_ptr->stat_bonuses[0] = str_bonus;
            quest_ptr->stat_bonuses[1] = dex_bonus;
            quest_ptr->stat_bonuses[2] = con_bonus;
            quest_ptr->stat_bonuses[3] = gra_bonus;
        }
    }

    /* Process 'K' for "sKill bonuses" */
    else if (buf[0] == 'K')
    {
        char skill_name[32];
        int skill_bonus;
        
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse skill bonuses: K:skill:bonus */
        if (2 == sscanf(buf + 2, "%31[^:]:%d", skill_name, &skill_bonus))
        {
            /* Map skill names to skill types using proper constants */
            if (SDL_strcasecmp(skill_name, "MEL") == 0) {
                quest_ptr->skill_type = S_MEL; /* Melee (0) */
            } else if (SDL_strcasecmp(skill_name, "ARC") == 0) {
                quest_ptr->skill_type = S_ARC; /* Archery (1) */
            } else if (SDL_strcasecmp(skill_name, "EVN") == 0) {
                quest_ptr->skill_type = S_EVN; /* Evasion (2) */
            } else if (SDL_strcasecmp(skill_name, "STL") == 0) {
                quest_ptr->skill_type = S_STL; /* Stealth (3) */
            } else if (SDL_strcasecmp(skill_name, "PER") == 0) {
                quest_ptr->skill_type = S_PER; /* Perception (4) */
            } else if (SDL_strcasecmp(skill_name, "WIL") == 0) {
                quest_ptr->skill_type = S_WIL; /* Will (5) */
            } else if (SDL_strcasecmp(skill_name, "SMT") == 0) {
                quest_ptr->skill_type = S_SMT; /* Smithing (6) */
            } else if (SDL_strcasecmp(skill_name, "SNG") == 0) {
                quest_ptr->skill_type = S_SNG; /* Song (7) */
            } else {
                quest_ptr->skill_type = 0; /* Default to Melee if unknown */
            }
            quest_ptr->skill_bonus = skill_bonus;
        }
    }

    /* Process 'I' for "Initialization text" */
    else if (buf[0] == 'I')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the initialization text in dedicated field with newline separator */
        if (quest_ptr->init_text != 0) {
            /* Add newline separator if not first line */
            if (!add_text(&(quest_ptr->init_text), head, "\n"))
                return (PARSE_ERROR_OUT_OF_MEMORY);
        }
        
        /* Store the text (buf + 2 points after "I:") */
        if (!add_text(&(quest_ptr->init_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'W' for "Win/completion text" */
    else if (buf[0] == 'W')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the completion text in dedicated field with newline separator */
        if (quest_ptr->completion_text != 0) {
            /* Add newline separator if not first line */
            if (!add_text(&(quest_ptr->completion_text), head, "\n"))
                return (PARSE_ERROR_OUT_OF_MEMORY);
        }
        
        /* Store the completion text (buf + 2 points after "W:") */
        if (!add_text(&(quest_ptr->completion_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'R' for "vaRiable name" (quest state mapping) */
    else if (buf[0] == 'R')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the quest state variable name */
        if (!(quest_ptr->quest_state_var = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'M' for "Metarun quest ID" */
    else if (buf[0] == 'M')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the metarun quest ID string */
        if (!(quest_ptr->metarun_quest_id = add_name(head, buf + 2)))
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

/*
 * Initialize the "oath_info" array, by parsing a ascii "template" file
 */
errr parse_oath_info(char* buf, header* head)
{
    int i;
    char *s;

    /* Current entry */
    static oath_type* oath_ptr = NULL;

    /* Process 'N' for "New/Number/Name" or 'O' for "Oath" */
    if (buf[0] == 'N' || buf[0] == 'O')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s) return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s) return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        oath_ptr = (oath_type*)head->info_ptr + i;

        /* Initialize the new fields */
    oath_ptr->oath_num = i;
        oath_ptr->stat_bonuses[0] = 0;
        oath_ptr->stat_bonuses[1] = 0;
        oath_ptr->stat_bonuses[2] = 0;
        oath_ptr->stat_bonuses[3] = 0;
        oath_ptr->skill_type = 0;
        oath_ptr->skill_bonus = 0;

        /* Store the name */
        if (!(oath_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Type info" or "Title text" */
    else if (buf[0] == 'T')
    {
        int oath_num, difficulty;

        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Try to scan for numeric values first (Type info) */
        if (2 == sscanf(buf + 2, "%d:%d", &oath_num, &difficulty))
        {
            /* Save the values */
            oath_ptr->oath_num = oath_num;
            oath_ptr->difficulty = difficulty;
        }
        else
        {
            /* Ignore title text for now - not stored in oath structure */
        }
    }

    /* Process 'R' for "Reward description" */
    else if (buf[0] == 'R')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store reward text using add_name for name buffer */
        if (!(oath_ptr->reward_text = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'A' for "Ability reward" */
    else if (buf[0] == 'A')
    {
        int reward_type, reward_value;

        /* There better be a current o_ptr */
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (2 != sscanf(buf + 2, "%d:%d", &reward_type, &reward_value))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        oath_ptr->reward_type = reward_type;
        oath_ptr->reward_value = reward_value;
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the text */
        if (!add_text(&(oath_ptr->text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'P' for "Pledge text" */
    else if (buf[0] == 'P')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store pledge text using add_name for name buffer */
        if (!(oath_ptr->pledge_text = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'F' for "Forbidden action text" */
    else if (buf[0] == 'F')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store forbidden action text using add_name for name buffer */
        if (!(oath_ptr->forbidden_text = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'S' for "Stat bonuses" */
    else if (buf[0] == 'S')
    {
        int str, dex, con, gra;

        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse stat bonuses and store them */
        /* Format: S:str:dex:con:gra */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", &str, &dex, &con, &gra))
        {
            return (PARSE_ERROR_GENERIC);
        }

        /* Store the stat bonuses */
        oath_ptr->stat_bonuses[0] = str;
        oath_ptr->stat_bonuses[1] = dex;
        oath_ptr->stat_bonuses[2] = con;
        oath_ptr->stat_bonuses[3] = gra;
    }

    /* Process 'K' for "sKill bonuses" */
    else if (buf[0] == 'K')
    {
        int skill_type, skill_bonus;

        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse skill bonuses and store them */
        /* Format: K:skill:bonus */
        if (2 != sscanf(buf + 2, "%d:%d", &skill_type, &skill_bonus))
        {
            return (PARSE_ERROR_GENERIC);
        }

        /* Store the skill bonuses */
        oath_ptr->skill_type = skill_type;
        oath_ptr->skill_bonus = skill_bonus;
    }

    /* Process 'B' for "Behavioral restrictions" */
    else if (buf[0] == 'B')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Ignore behavioral restriction text for now - not essential for oath selection */
    }

    /* Process 'U' for "Unlock conditions" */
    else if (buf[0] == 'U')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Ignore unlock condition text for now - not essential for oath selection */
    }

    /* Process 'C' for "Confirmation prompt" */
    else if (buf[0] == 'C')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store confirmation prompt text using add_name for name buffer */
        if (!(oath_ptr->confirmation_prompt = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'M' for "Curse Message" */
    else if (buf[0] == 'M')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store curse message text using add_name for name buffer */
        if (!(oath_ptr->curse_message = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'E' for "pErmanent consequence message" */
    else if (buf[0] == 'E')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store permanent consequence message text using add_name for name buffer */
        if (!(oath_ptr->permanent_message = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'Q' for "Death/escape message" */
    else if (buf[0] == 'Q')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store death/escape message text using add_name for name buffer */
        if (!(oath_ptr->death_message = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'Z' for "Banned text (birth screen)" */
    else if (buf[0] == 'Z')
    {
        /* There better be a current oath_ptr */
        if (!oath_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store banned text using add_text to allow multiple lines */
        if (!add_text(&(oath_ptr->banned_text), head, buf + 2))
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

#else /* ALLOW_TEMPLATES */

#endif /* ALLOW_TEMPLATES */













