/* File: cmd-ui-query.c */

/*
 * Copyright (c) 2001 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"

/*
 * The table of "symbol info" -- each entry is a string of the form
 * "X:desc" where "X" is the trigger, and "desc" is the "info".
 */
static cptr ident_info[]
    = { " :A dark grid", "!:A potion (or oil)", "\":An amulet", "#:A wall",
          /* "$:unused", */
          "%:A quartz vein", "&:A plant", "':An open door", "(:Soft armour",
          "):A shield", "*:A gem (or unseen monster)", "+:A closed door",
          ",:Food", "-:Arrows", ".:Floor", "/:An axe or polearm", "0:A forge",
          /* "1:unused", */
          /* "2:unused", */
          /* "3:unused", */
          /* "4:unused", */
          /* "5:unused", */
          /* "6:unused", */
          /* "7:unused", */
          /* "8:unused", */
          /* "9:unused", */
          "::Rubble", ";:A glyph of warding", "<:A staircase up", "=:A ring",
          ">:A staircase down", "?:An instrument", "@:Elf, Dwarf, or Man",
          /* "A:unused", */
          /* "B:unused", */
          "C:Canine", "D:Dragon",
          /* "E:unused", */
          /* "F:unused", */
          "G:Giant", "H:Horror", "I:Insect",
          /* "J:unused", */
          /* "K:unused", */
          /* "L:unused", */
          "M:Spider", "N:Nameless Thing",
          /* "O:unused", */
          "P:Giant",
          /* "Q:unused", */
          "R:Rauko", "S:Ancient Serpent", "T:Troll",
          /* "U:unused", */
          "V:Valar", "W:Wight/Wraith",
          /* "X:unused", */
          /* "Y:unused", */
          /* "Z:unused", */
          "[:Mail", "\\:A blunt weapon (or digger)", "]:Misc. armour",
          "^:A trap", "_:A staff",
          /* "`:unused", */
          /* "a:unused", */
          "b:Bat/Bird",
          /* "c:unused", */
          "d:Dragon",
          /* "e:unused", */
          "f:Feline",
          /* "g:unused", */
          /* "h:unused", */
          /* "i:unused", */
          /* "j:unused", */
          /* "k:unused", */
          /* "l:unused", */
          "m:Young Spider",
          /* "n:unused", */
          "o:Orc",
          /* "p:unused", */
          /* "q:unused", */
          /* "r:unused", */
          "s:Serpent",
          /* "t:unused", */
          /* "u:unused", */
          "v:Vampire", "w:Creeping Shadow",
          /* "x:unused", */
          /* "y:unused", */
          /* "z:unused", */
          /* "{:unused", */
          "|:An edged weapon (sword/dagger/etc)", "}:A bow",
          "~:A tool (or miscellaneous item)", NULL };

/*
 * Sorting hook -- Comp function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform on "u".
 */
bool ang_sort_comp_hook(const void* u, const void* v, int a, int b)
{
    u16b* who = (u16b*)(u);

    u16b* why = (u16b*)(v);

    int w1 = who[a];
    int w2 = who[b];

    int z1, z2;

    /* Sort by player kills */
    if (*why >= 4)
    {
        /* Extract player kills */
        z1 = l_list[w1].pkills;
        z2 = l_list[w2].pkills;

        /* Compare player kills */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by total kills */
    if (*why >= 3)
    {
        /* Extract total kills */
        z1 = l_list[w1].tkills;
        z2 = l_list[w2].tkills;

        /* Compare total kills */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by monster level */
    if (*why >= 2)
    {
        /* Extract levels */
        z1 = r_info[w1].level;
        z2 = r_info[w2].level;

        /* Compare levels */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Sort by monster depth */
    if (*why >= 1)
    {
        /* Extract experience */
        z1 = r_info[w1].level;
        z2 = r_info[w2].level;

        /* Compare experience */
        if (z1 < z2)
            return (true);
        if (z1 > z2)
            return (false);
    }

    /* Compare indexes */
    return (w1 <= w2);
}

/*
 * Sorting hook -- Swap function -- see below
 *
 * We use "u" to point to array of monster indexes,
 * and "v" to select the type of sorting to perform.
 */
void ang_sort_swap_hook(void* u, void* v, int a, int b)
{
    u16b* who = (u16b*)(u);

    u16b holder;

    /* Unused parameter */
    (void)v;

    /* Swap */
    holder = who[a];
    who[a] = who[b];
    who[b] = holder;
}

/*
 * Identify a character, allow recall of monsters
 *
 * Several "special" responses recall "multiple" monsters:
 *   ^A (all monsters)
 *   ^U (all unique monsters)
 *   ^N (all non-unique monsters)
 *
 * The responses may be sorted in several ways, see below.
 *
 *
 */
void do_cmd_query_symbol(void)
{
    int i, n, r_idx;
    char sym, query;
    char buf[128];

    bool all = false;
    bool uniq = false;
    bool norm = false;

    bool recall = false;

    u16b why = 0;
    u16b* who;

    /* Get a character, or abort */
    if (!get_com("Enter character to be identified: ", &sym))
        return;

    /* Find that character info, and describe it */
    for (i = 0; ident_info[i]; ++i)
    {
        if (sym == ident_info[i][0])
            break;
    }

    /* Describe */
    if (sym == KTRL('A'))
    {
        all = true;
        SDL_strlcpy(buf, "Full monster list.", sizeof(buf));
    }
    else if (sym == KTRL('U'))
    {
        all = uniq = true;
        SDL_strlcpy(buf, "Unique monster list.", sizeof(buf));
    }
    else if (sym == KTRL('N'))
    {
        all = norm = true;
        SDL_strlcpy(buf, "Non-unique monster list.", sizeof(buf));
    }
    else if (ident_info[i])
    {
        strnfmt(buf, sizeof(buf), "%c - %s.", sym, ident_info[i] + 2);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%c - %s.", sym, "Unknown Symbol");
    }

    /* Display the result */
    prt(buf, 0, 0);

    /* Allocate the "who" array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Nothing to recall */
        if (!cheat_know && !l_ptr->tsights && !know_monster_info)
            continue;

        /* Require non-unique monsters if needed */
        if (norm && (r_ptr->flags1 & (RF1_UNIQUE)))
            continue;

        /* Require unique monsters if needed */
        if (uniq && !(r_ptr->flags1 & (RF1_UNIQUE)))
            continue;

        // Ignore monsters that can't be generated
        if (r_ptr->level > 25)
            continue;

        /* Collect "appropriate" monsters */
        if (all || (r_ptr->d_char == sym))
            who[n++] = i;
    }

    /* Nothing to recall */
    if (!n)
    {
        /* XXX XXX Free the "who" array */
        who = mem_free(who);

        return;
    }

    /* Prompt */
    put_str("Recall details? (k/p/y/n): ", 0, 40);

    /* Query */
    query = inkey();

    /* Restore */
    prt(buf, 0, 0);

    /* Sort by kills (and level) */
    if (query == 'k')
    {
        why = 4;
        query = 'y';
    }

    /* Sort by level */
    if (query == 'p')
    {
        why = 2;
        query = 'y';
    }

    /* Catch "escape" */
    if (query != 'y')
    {
        /* XXX XXX Free the "who" array */
        who = mem_free(who);

        return;
    }

    /* Sort if needed */
    if (why)
    {
        /* Select the sort method */
        ang_sort_comp = ang_sort_comp_hook;
        ang_sort_swap = ang_sort_swap_hook;

        /* Sort the array */
        ang_sort(who, &why, n);
    }

    /* Start at the end */
    i = n - 1;

    /* Scan the monster memory */
    while (1)
    {
        /* Extract a race */
        r_idx = who[i];

        /* Hack -- Auto-recall */
        monster_race_track(r_idx);

        /* Hack -- Handle stuff */
        handle_stuff();

        /* Hack -- Begin the prompt */
        roff_top(r_idx);

        /* Hack -- Complete the prompt */
        Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");

        /* Interact */
        while (1)
        {
            /* Recall (raging players don't get recall) */
            if (recall)
            {
                /* Save screen */
                screen_save();

                /* Recall on screen */
                screen_roff(who[i], NULL);

                /* Hack -- Complete the prompt (again) */
                Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");
            }

            /* Command */
            query = inkey();

            /* Unrecall */
            if (recall)
            {
                /* Load screen */
                screen_load();
            }

            /* Normal commands */
            if (query != 'r')
                break;

            /* Toggle recall */
            recall = !recall;
        }

        /* Stop scanning */
        if (query == ESCAPE)
            break;

        /* Move to "prev" monster */
        if (query == '-')
        {
            if (++i == n)
            {
                i = 0;
            }
        }

        /* Move to "next" monster */
        else
        {
            if (i-- == 0)
            {
                i = n - 1;
            }
        }
    }

    /* Re-display the identity */
    prt(buf, 0, 0);

    /* Free the "who" array */
    who = mem_free(who);
}
