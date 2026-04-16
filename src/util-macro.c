#include "angband.h"
#include "externs.h"

s16b macro__num;
cptr* macro__pat;
cptr* macro__act;

int max_macrotrigger = 0;
cptr macro_template = NULL;
cptr macro_modifier_chr;
cptr macro_modifier_name[MAX_MACRO_MOD];
cptr macro_trigger_name[MAX_MACRO_TRIGGER];
cptr macro_trigger_keycode[2][MAX_MACRO_TRIGGER];

char macro_buffer[1024];
cptr keymap_act[KEYMAP_MODES][256];

/*
 * The "macro" package
 *
 * Functions are provided to manipulate a collection of macros, each
 * of which has a trigger pattern string and a resulting action string
 * and a small set of flags.
 */

/*
 * Determine if any macros have ever started with a given character.
 */
static bool macro__use[256];

/*
 * Find the macro (if any) which exactly matches the given pattern
 */
int macro_find_exact(cptr pat)
{
    int i;

    /* Nothing possible */
    if (!macro__use[(byte)(pat[0])])
    {
        return (-1);
    }

    /* Scan the macros */
    for (i = 0; i < macro__num; ++i)
    {
        /* Skip macros which do not match the pattern */
        if (!streq(macro__pat[i], pat))
            continue;

        /* Found one */
        return (i);
    }

    /* No matches */
    return (-1);
}

/*
 * Find the first macro (if any) which contains the given pattern
 */
int macro_find_check(cptr pat)
{
    int i;

    /* Nothing possible */
    if (!macro__use[(byte)(pat[0])])
    {
        return (-1);
    }

    /* Scan the macros */
    for (i = 0; i < macro__num; ++i)
    {
        /* Skip macros which do not contain the pattern */
        if (!prefix(macro__pat[i], pat))
            continue;

        /* Found one */
        return (i);
    }

    /* Nothing */
    return (-1);
}

/*
 * Find the first macro (if any) which contains the given pattern and more
 */
int macro_find_maybe(cptr pat)
{
    int i;

    /* Nothing possible */
    if (!macro__use[(byte)(pat[0])])
    {
        return (-1);
    }

    /* Scan the macros */
    for (i = 0; i < macro__num; ++i)
    {
        /* Skip macros which do not contain the pattern */
        if (!prefix(macro__pat[i], pat))
            continue;

        /* Skip macros which exactly match the pattern XXX XXX */
        if (streq(macro__pat[i], pat))
            continue;

        /* Found one */
        return (i);
    }

    /* Nothing */
    return (-1);
}

/*
 * Find the longest macro (if any) which starts with the given pattern
 */
int macro_find_ready(cptr pat)
{
    int i, t, n = -1, s = -1;

    /* Nothing possible */
    if (!macro__use[(byte)(pat[0])])
    {
        return (-1);
    }

    /* Scan the macros */
    for (i = 0; i < macro__num; ++i)
    {
        /* Skip macros which are not contained by the pattern */
        if (!prefix(pat, macro__pat[i]))
            continue;

        /* Obtain the length of this macro */
        t = strlen(macro__pat[i]);

        /* Only track the "longest" pattern */
        if ((n >= 0) && (s > t))
            continue;

        /* Track the entry */
        n = i;
        s = t;
    }

    /* Result */
    return (n);
}

/*
 * Add a macro definition (or redefinition).
 */
errr macro_add(cptr pat, cptr act)
{
    int n;

    /* Paranoia -- require data */
    if (!pat || !act)
        return (-1);

    /* Look for any existing macro */
    n = macro_find_exact(pat);

    /* Replace existing macro */
    if (n >= 0)
    {
        /* Free the old macro action */
        str_free(macro__act[n]);
    }

    /* Create a new macro */
    else
    {
        /* Get a new index */
        n = macro__num++;

        /* Boundary check */
        if (macro__num >= MACRO_MAX)
            quit("Too many macros!");

        /* Save the pattern */
        macro__pat[n] = str_dup(pat);
    }

    /* Save the action */
    macro__act[n] = str_dup(act);

    /* Efficiency */
    macro__use[(byte)(pat[0])] = true;

    /* Success */
    return (0);
}

/*
 * Initialize the "macro" package
 */
errr macro_init(void)
{
    /* Macro patterns */
    macro__pat = mem_alloc_array(MACRO_MAX, cptr);

    /* Macro actions */
    macro__act = mem_alloc_array(MACRO_MAX, cptr);

    /* Success */
    return (0);
}

/*
 * Free the macro package
 */
errr macro_free(void)
{
    int i, j;

    /* Free the macros */
    for (i = 0; i < macro__num; ++i)
    {
        str_free(macro__pat[i]);
        str_free(macro__act[i]);
    }

    mem_free_null(macro__pat);
    mem_free_null(macro__act);

    /* Free the keymaps */
    for (i = 0; i < KEYMAP_MODES; ++i)
    {
        for (j = 0; j < (int)N_ELEMENTS(keymap_act[i]); ++j)
        {
            str_free(keymap_act[i][j]);
            keymap_act[i][j] = NULL;
        }
    }

    /* Success */
    return (0);
}

/*
 * Free the macro trigger package
 */
errr macro_trigger_free(void)
{
    int i;
    int num;

    if (macro_template != NULL)
    {
        /* Free the template */
        str_free(macro_template);
        macro_template = NULL;

        /* Free the trigger names and keycodes */
        for (i = 0; i < max_macrotrigger; i++)
        {
            str_free(macro_trigger_name[i]);

            str_free(macro_trigger_keycode[0][i]);
            str_free(macro_trigger_keycode[1][i]);
        }

        /* No more macro triggers */
        max_macrotrigger = 0;

        /* Count modifier-characters */
        num = strlen(macro_modifier_chr);

        /* Free modifier names */
        for (i = 0; i < num; i++)
        {
            str_free(macro_modifier_name[i]);
        }

        /* Free modifier chars */
        str_free(macro_modifier_chr);
        macro_modifier_chr = NULL;
    }

    /* Success */
    return (0);
}
