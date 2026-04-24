/* File: spell-visuals.c */
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
#include "spell/spell-damage.h"

static byte spell_color(int type)
{
    switch (type)
    {
    case GF_ARROW:
        return (TERM_L_UMBER);
    case GF_BOULDER:
        return (TERM_SLATE);
    case GF_ACID:
        return (TERM_SLATE);
    case GF_ELEC:
        return (TERM_BLUE);
    case GF_FIRE:
        return (TERM_RED);
    case GF_COLD:
        return (TERM_WHITE);
    case GF_POIS:
        return (TERM_GREEN);
    case GF_CONFUSION:
        return (TERM_L_UMBER);
    case GF_SOUND:
        return (TERM_L_WHITE);
    case GF_LIGHT:
        return (TERM_WHITE);
    case GF_DARK_WEAK:
        return (TERM_L_DARK);
    case GF_DARK:
        return (TERM_L_DARK);
    case GF_IDENTIFY:
        return (TERM_WHITE);
    case GF_EARTHQUAKE:
        return (TERM_SLATE);
    case GF_WEB:
        return (TERM_L_UMBER);
    }

    return (TERM_L_WHITE);
}

u16b bolt_pict(int y, int x, int ny, int nx, int typ)
{
    int base;
    byte k;
    byte a;
    char c;

    if ((ny == y) && (nx == x))
        base = 0x30;
    else if (nx == x)
        base = 0x40;
    else if (ny == y)
        base = 0x50;
    else if ((ny - y) == (x - nx))
        base = 0x60;
    else if ((ny - y) == (nx - x))
        base = 0x70;
    else
        base = 0x30;

    if (typ == GF_LIGHT && use_graphics == GRAPHICS_MICROCHASM)
    {
        a = misc_to_attr[ICON_GLOW];
        c = misc_to_char[ICON_GLOW];
    }
    else
    {
        k = spell_color(typ);
        a = misc_to_attr[base + k];
        c = misc_to_char[base + k];
    }

    return (PICT(a, c));
}
