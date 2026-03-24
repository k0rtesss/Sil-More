/* File: init-parse-names.c */

#include "angband.h"
#include "init.h"
#include "h-define.h"

#ifdef ALLOW_TEMPLATES

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

#endif /* ALLOW_TEMPLATES */
