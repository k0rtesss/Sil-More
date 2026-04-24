/* File: spell-elemental-materials.c */
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

typedef int (*inven_func)(const object_type*);

bool hates_acid(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    case TV_STAFF:
    case TV_CHEST:
    case TV_SKELETON:
        return true;
    }

    return false;
}

bool hates_elec(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_RING:
        return true;
    }

    return false;
}

bool hates_fire(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_CHEST:
    case TV_NOTE:
    case TV_STAFF:
        return true;

    case TV_LIGHT:
        return (o_ptr->sval == SV_LIGHT_TORCH)
            || (o_ptr->sval == SV_LIGHT_MALLORN);
    }

    return false;
}

bool hates_cold(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_POTION:
    case TV_GEM:
    case TV_FLASK:
        return true;

    case TV_LIGHT:
        return (o_ptr->sval == SV_LIGHT_LANTERN)
            || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL);
    }

    return false;
}

static int set_acid_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!hates_acid(o_ptr))
        return false;
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_IGNORE_ACID)
        return false;
    return true;
}

static int set_elec_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!hates_elec(o_ptr))
        return false;
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_IGNORE_ELEC)
        return false;
    return true;
}

static int set_fire_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!hates_fire(o_ptr))
        return false;
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_IGNORE_FIRE)
        return false;
    return true;
}

static int set_cold_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!hates_cold(o_ptr))
        return false;
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_IGNORE_COLD)
        return false;
    return true;
}

static int set_sound_destroy(const object_type* o_ptr)
{
    return hates_cold(o_ptr);
}

bool elemental_attack_destroys_object(int attack_type, const object_type* o_ptr)
{
    inven_func typ = NULL;

    switch (attack_type)
    {
    case GF_ACID:
        typ = set_acid_destroy;
        break;
    case GF_ELEC:
        typ = set_elec_destroy;
        break;
    case GF_FIRE:
        typ = set_fire_destroy;
        break;
    case GF_COLD:
        typ = set_cold_destroy;
        break;
    case GF_SOUND:
        typ = set_sound_destroy;
        break;
    }

    if (!typ || !o_ptr)
        return false;

    return (*typ)(o_ptr) ? true : false;
}
