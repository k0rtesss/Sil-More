/* File: util.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "fs/path.h"
#include "log/log.h"
#include "reliability-checks.h"
#include <sys/stat.h>

bool no_light(void)
{
    if (!p_ptr)
        return true;

    if (p_ptr->cur_light > 0)
        return false;

    if (p_ptr->py < 0 || p_ptr->py >= MAX_DUNGEON_HGT || p_ptr->px < 0
        || p_ptr->px >= MAX_DUNGEON_WID)
        return true;

    return (cave_info[p_ptr->py][p_ptr->px] & CAVE_GLOW) == 0;
}

/*
 * Parse a hexadecimal string (optional separators) into an unsigned 64-bit value.
 * Accepts optional "0x" prefix and ignores '-', '_' or whitespace separators.
 */
bool parse_u64b_hex(const char* text, u64b* out)
{
    if (!text || !out)
        return false;

    u64b value = 0;
    int digits = 0;

    while (*text)
    {
        char c = *text++;

        if (c == '-' || c == '_' || c == ' ')
            continue;

        if (digits == 0 && c == '0' && (*text == 'x' || *text == 'X'))
        {
            text++;
            continue;
        }

        int nibble;
        if (c >= '0' && c <= '9')
            nibble = c - '0';
        else if (c >= 'a' && c <= 'f')
            nibble = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F')
            nibble = 10 + (c - 'A');
        else
            return false;

        if (digits >= 16)
            return false;

        value = (value << 4) | (u64b)nibble;
        digits++;
    }

    if (digits == 0)
        return false;

    *out = value;
    return true;
}

#ifdef SET_UID

#ifndef HAVE_USLEEP

/*
 * For those systems that don't have "usleep()" but need it.
 *
 * Fake "usleep()" function grabbed from the inl netrek server -cba
 */
int usleep(unsigned long usecs)
{
    struct timeval Timer;

    int nfds = 0;

#ifdef FD_SET
    fd_set* no_fds = NULL;
#else
    int* no_fds = NULL;
#endif

    /* Paranoia -- No excessive sleeping */
    if (usecs > 4000000L)
        core("Illegal usleep() call");

    /* Wait for it */
    Timer.tv_sec = (usecs / 1000000L);
    Timer.tv_usec = (usecs % 1000000L);

    /* Wait for it */
    if (select(nfds, no_fds, no_fds, no_fds, &Timer) < 0)
    {
        /* Hack -- ignore interrupts */
        if (errno != EINTR)
            return -1;
    }

    /* Success */
    return 0;
}

#endif /* HAVE_USLEEP */

/*
 * Find a default user name from the system.
 */
void user_name(char* buf, size_t len, int id)
{
    struct passwd* pw = NULL;

    /* Look up the user name */
    if ((pw = getpwuid(id)))
    {
        /* Get the first 15 characters of the user name */
        SDL_strlcpy(buf, pw->pw_name, len);

#ifdef CAPITALIZE_USER_NAME
        /* Hack -- capitalize the user name */
        if (islower((unsigned char)buf[0]))
            buf[0] = toupper((unsigned char)buf[0]);
#endif /* CAPITALIZE_USER_NAME */

        return;
    }

    /* Oops.  Hack -- default to "nameless" */
    SDL_strlcpy(buf, "nameless", len);
}

#endif /* SET_UID */

#ifdef CHECK_MODIFICATION_TIME

/* SDL3-compatible modification time check */
errr check_modification_date_sdl(cptr raw_path, cptr txt_path)
{
    struct stat txt_info;
    struct stat raw_info;

    /* Get info for text file */
    if (stat(txt_path, &txt_info) != 0)
    {
        /* No text file or error - continue with raw */
        log_debug("check_modification_date: Cannot get info for txt file '%s'",
            txt_path);
        return (0);
    }

    /* Get info for raw file */
    if (stat(raw_path, &raw_info) != 0)
    {
        /* No raw file - need to regenerate */
        log_info("check_modification_date: No raw file '%s' - regenerating",
            raw_path);
        return (-1);
    }

    /* Ensure text file is not newer than raw file */
    if (txt_info.st_mtime > raw_info.st_mtime)
    {
        /* Text file is newer - reprocess */
        log_info("check_modification_date: txt file newer (txt=%lld, raw=%lld) - regenerating '%s'",
            (long long)txt_info.st_mtime, (long long)raw_info.st_mtime,
            txt_path);
        return (-1);
    }

    log_info("check_modification_date: raw file is up to date (txt=%lld, raw=%lld) for '%s'",
        (long long)txt_info.st_mtime, (long long)raw_info.st_mtime,
        txt_path);
    return (0);
}

#endif /* CHECK_MODIFICATION_TIME */

/*
 *  Simple exponential function for integers with non-negative powers
 */
int int_exp(int base, int power)
{
    int i;
    int result = 1;

    for (i = 0; i < power; i++)
    {
        result *= base;
    }

    return (result);
}

/*
 * Generates damage for "2d6" style dice rolls
 */
int damroll(int num, int sides)
{
    int i;
    int sum = 0;

    /* Dice with no sides always come up zero */
    if (sides <= 0)
        return (0);

    /* Roll the dice */
    for (i = 0; i < num; i++)
    {
        sum += dieroll(sides);
    }

    return (sum);
}

/*
 * Check a char for "vowel-hood"
 */
bool is_a_vowel(int ch)
{
    switch (ch)
    {
    case 'a':
    case 'e':
    case 'i':
    case 'o':
    case 'u':
    case 'A':
    case 'E':
    case 'I':
    case 'O':
    case 'U':
        return (true);
    }

    return (false);
}

/*
 * Convert a "color letter" into an "actual" color
 * The colors are: dwsorgbuDWvyRGBU, as shown below
 */
int color_char_to_attr(char c)
{
    switch (c)
    {
    case 'd':
        return (TERM_DARK);
    case 'w':
        return (TERM_WHITE);
    case 's':
        return (TERM_SLATE);
    case 'o':
        return (TERM_ORANGE);
    case 'r':
        return (TERM_RED);
    case 'g':
        return (TERM_GREEN);
    case 'b':
        return (TERM_BLUE);
    case 'u':
        return (TERM_UMBER);

    case 'D':
        return (TERM_L_DARK);
    case 'W':
        return (TERM_L_WHITE);
    case 'v':
        return (TERM_VIOLET);
    case 'y':
        return (TERM_YELLOW);
    case 'R':
        return (TERM_L_RED);
    case 'G':
        return (TERM_L_GREEN);
    case 'B':
        return (TERM_L_BLUE);
    case 'U':
        return (TERM_L_UMBER);
    }

    return (-1);
}

#ifdef ALLOW_REPEAT

#define REPEAT_MAX 20

/* Number of chars saved */
static int repeat__cnt = 0;

/* Current index */
static int repeat__idx = 0;

/* Saved "stuff" */
static int repeat__key[REPEAT_MAX];

/*
 * Push data.
 */
void repeat_push(int what)
{
    /* Too many keys */
    if (repeat__cnt == REPEAT_MAX)
        return;

    /* Push the "stuff" */
    repeat__key[repeat__cnt++] = what;

    /* Prevents us from pulling keys */
    ++repeat__idx;
}

/*
 * Pull data.
 */
bool repeat_pull(int* what)
{
    /* All out of keys */
    if (repeat__idx == repeat__cnt)
        return (false);

    /* Grab the next key, advance */
    *what = repeat__key[repeat__idx++];

    /* Success */
    return (true);
}

void repeat_clear(void)
{
    /* Start over from the failed pull */
    if (repeat__idx)
        repeat__cnt = --repeat__idx;
    /* Paranoia */
    else
        repeat__cnt = repeat__idx;

    return;
}

/*
 * Repeat previous command, or begin memorizing new command.
 */
void repeat_check(void)
{
    int what;

    /* Ignore some commands */
    if (p_ptr->command_cmd == ESCAPE)
        return;
    if (p_ptr->command_cmd == ' ')
        return;
    if (p_ptr->command_cmd == '\n')
        return;
    if (p_ptr->command_cmd == '\r')
        return;

    /* Repeat Last Command */
    if (p_ptr->command_cmd == 'n')
    {
        /* Reset */
        repeat__idx = 0;

        /* Get the command */
        if (repeat_pull(&what))
        {
            /* Save the command */
            p_ptr->command_cmd = what;
        }
    }

    /* Start saving new command */
    else
    {
        /* Reset */
        repeat__cnt = 0;
        repeat__idx = 0;

        /* Get the current command */
        what = p_ptr->command_cmd;

        /* Save this command */
        repeat_push(what);
    }
}

#endif /* ALLOW_REPEAT */

#ifdef SUPPORT_GAMMA

/* Table of gamma values */
byte gamma_table[256];

/* Table of ln(x / 256) * 256 for x going from 0 -> 255 */
static const s16b gamma_helper[256] = { 0, -1420, -1242, -1138, -1065, -1007,
    -961, -921, -887, -857, -830, -806, -783, -762, -744, -726, -710, -694,
    -679, -666, -652, -640, -628, -617, -606, -596, -586, -576, -567, -577,
    -549, -541, -532, -525, -517, -509, -502, -495, -488, -482, -475, -469,
    -463, -457, -451, -455, -439, -434, -429, -423, -418, -413, -408, -403,
    -398, -394, -389, -385, -380, -376, -371, -367, -363, -359, -355, -351,
    -347, -343, -339, -336, -332, -328, -325, -321, -318, -314, -311, -308,
    -304, -301, -298, -295, -291, -288, -285, -282, -279, -276, -273, -271,
    -268, -265, -262, -259, -257, -254, -251, -248, -246, -243, -241, -238,
    -236, -233, -231, -228, -226, -223, -221, -219, -216, -214, -212, -209,
    -207, -205, -203, -200, -198, -196, -194, -192, -190, -188, -186, -184,
    -182, -180, -178, -176, -174, -172, -170, -168, -166, -164, -162, -160,
    -158, -156, -155, -153, -151, -149, -147, -146, -144, -142, -140, -139,
    -137, -135, -134, -132, -130, -128, -127, -125, -124, -122, -120, -119,
    -117, -116, -114, -112, -111, -109, -108, -106, -105, -103, -102, -100,
    -99, -97, -96, -95, -93, -92, -90, -89, -87, -86, -85, -83, -82, -80,
    -79, -78, -76, -75, -74, -72, -71, -70, -68, -67, -66, -65, -63, -62,
    -61, -59, -58, -57, -56, -54, -53, -52, -51, -50, -48, -47, -46, -45,
    -44, -42, -41, -40, -39, -38, -37, -35, -34, -33, -32, -31, -30, -29,
    -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -14, -13,
    -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1 };

/*
 * Build the gamma table so that floating point isn't needed.
 *
 * Note gamma goes from 0->256.  The old value of 100 is now 128.
 */
void build_gamma_table(int gamma)
{
    int i, n;

    /*
     * value is the current sum.
     * diff is the new term to add to the series.
     */
    long value, diff;

    /* Hack - convergence is bad in these cases. */
    gamma_table[0] = 0;
    gamma_table[255] = 255;

    for (i = 1; i < 255; i++)
    {
        /*
         * Initialise the Taylor series
         *
         * value and diff have been scaled by 256
         */
        n = 1;
        value = 256L * 256L;
        diff = ((long)gamma_helper[i]) * (gamma - 256);

        while (diff)
        {
            value += diff;
            n++;

            /*
             * Use the following identiy to calculate the gamma table.
             * exp(x) = 1 + x + x^2/2 + x^3/(2*3) + x^4/(2*3*4) +...
             */
            diff = (((diff / 256) * gamma_helper[i]) * (gamma - 256))
                / (256 * n);
        }

        /*
         * Store the value in the table so that the
         * floating point pow function isn't needed.
         */
        gamma_table[i] = ((long)(value / 256) * i) / 256;
    }
}

#endif /* SUPPORT_GAMMA */

/*
 * Returns a string which contains the name of a extended color.
 * Examples: "Dark", "Red1", "Yellow5", etc.
 * IMPORTANT: the returned string is statically allocated so it must *not* be
 * freed and its value changes between calls to this function.
 */
cptr get_ext_color_name(byte ext_color)
{
    static char buf[25];

    if (GET_SHADE(ext_color) > 0)
    {
        strnfmt(buf, sizeof(buf), "%s%d",
            color_names[GET_BASE_COLOR(ext_color)], GET_SHADE(ext_color));
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%s", color_names[GET_BASE_COLOR(ext_color)]);
    }

    return buf;
}

/*
 * Converts a string to a terminal color byte.
 */
int color_text_to_attr(cptr name)
{
    int i, len, base, shade;

    /* Optimize name searching. See below */
    static byte len_names[MAX_BASE_COLORS];

    /* Separate the color name and the shade number */
    /* Only letters can be part of the name */
    for (i = 0; isalpha((unsigned char)name[i]); i++)
        ;

    /* Store the start of the shade number */
    len = i;

    /* Check for invalid characters in the shade part */
    while (name[i])
    {
        /* No digit, exit */
        if (!isdigit((unsigned char)name[i]))
            return (-1);
        ++i;
    }

    /* Initialize the shade */
    shade = 0;

    /* Only analyze the shade if there is one */
    if (name[len])
    {
        /* Convert to number */
        shade = atoi(name + len);

        /* Check bounds */
        if ((shade < 0) || (shade > MAX_SHADES - 1))
            return (-1);
    }

    /* Extra, allow the use of strings like "r1", "U5", etc. */
    if (len == 1)
    {
        /* Convert one character, check sanity */
        if ((base = color_char_to_attr(name[0])) == -1)
            return (-1);

        /* Build the extended color */
        return (MAKE_EXTENDED_COLOR(base, shade));
    }

    /* Hack - Initialize the length array once */
    if (!len_names[0])
    {
        for (base = 0; base < MAX_BASE_COLORS; base++)
        {
            /* Store the length of each color name */
            len_names[base] = (byte)strlen(color_names[base & 0x0F]);
        }
    }

    /* Find the name */
    for (base = 0; base < MAX_BASE_COLORS; base++)
    {
        /* Somewhat optimize the search */
        if (len != len_names[base])
            continue;

        /* Compare only the found name */
        if (SDL_strncasecmp(name, color_names[base & 0x0F], (size_t)len) == 0)
        {
            /* Build the extended color */
            return (MAKE_EXTENDED_COLOR(base, shade));
        }
    }

    /* We can not find it */
    return (-1);
}
