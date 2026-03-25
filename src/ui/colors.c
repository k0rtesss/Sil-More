/* ui/colors.c - Color name and attribute utilities */

#include "../angband.h"
#include "colors.h"

static bool g_use_background_colors = false;

/* Short color names for base colors */
static char* short_color_names[MAX_BASE_COLORS] = {
    "Dark", "White", "Slate", "Orange",
    "Red", "Green", "Blue", "Umber",
    "L.Dark", "L.Slate", "Violet", "Yellow",
    "L.Red", "L.Green", "L.Blue", "L.Umber"
};

/*
 * Extract a textual representation of an attribute.
 * Returns the base color name, optionally with a shade suffix.
 */
cptr attr_to_text(byte a)
{
    char* base;

    base = short_color_names[GET_BASE_COLOR(a)];

#if DO_YOU_WANT_THIS_IN_MONSTER_SPOILERS_Q

    if (GET_SHADE(a) > 0)
    {
        static char buf[25];

        strnfmt(buf, sizeof(buf), "%s%d", base, GET_SHADE(a));

        return (buf);
    }

#endif

    return (base);
}

bool ui_colors_use_backgrounds(void)
{
    return g_use_background_colors;
}

void ui_colors_set_backgrounds(bool enabled)
{
    g_use_background_colors = enabled;
}
