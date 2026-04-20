/* File: object-display.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "cJSON.h"
#include "fs/file.h"
#include "fs/resource.h"
#include "log/log.h"
#include "object/object-display.h"

static bool inventory_menu_include_equip = false;
static byte g_object_text_colors[128];

/*
 * Apply tilemode overrides for special artefacts.
 * Currently used to distinguish Morgoth's crown variants
 * once Silmarils have been removed.
 */
byte object_attr_graphics_override(const object_type* o_ptr, byte base_attr)
{
    if (!o_ptr)
        return base_attr;

    /* Only adjust if this is already a tile (high bit set). */
    if (!(base_attr & 0x80))
        return base_attr;

    byte preserved = base_attr & (GRAPHICS_GLOW_MASK | TILE_FLAG);

    if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_2))
    {
        byte base = preserved | TILE_FLAG;
        return TILE_SET_INDEX(base, 12);
    }

    return base_attr;
}

char object_char_graphics_override(const object_type* o_ptr, char base_char)
{
    if (!o_ptr)
        return base_char;

    /* Only adjust if this is already a tile (high bit set). */
    if (!(base_char & 0x80))
        return base_char;

    byte preserved = base_char & (GRAPHICS_ALERT_MASK | TILE_FLAG);
    byte column = 0;

    switch (o_ptr->name1)
    {
    case ART_MORGOTH_2:
        column = 23;
        break;
    case ART_MORGOTH_1:
        column = 24;
        break;
    case ART_MORGOTH_0:
        column = 25;
        break;
    default:
        return base_char;
    }

    byte base = preserved | TILE_FLAG;
    return (char)TILE_SET_INDEX(base, column);
}

bool object_is_unidentified_for_display(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->ident & IDENT_SPOIL)
        return false;

    if (!object_aware_p(o_ptr))
        return true;

    if (o_ptr->tval == TV_STAFF || o_ptr->tval == TV_HORN)
        return !object_known_p(o_ptr);

    if (object_uses_smithing_difficulty(o_ptr))
        return !object_known_p(o_ptr);

    return false;
}

void inventory_menu_set_include_equip(bool include)
{
    inventory_menu_include_equip = include;
}

bool inventory_menu_get_include_equip(void)
{
    return inventory_menu_include_equip;
}

static void load_object_text_colors_json(void)
{
    char path[1024];
    ang_file* f = NULL;
    char* buffer = NULL;
    cJSON* root = NULL;
    size_t read = 0;
    int loaded_entries = 0;

    for (int i = 0; i < (int)N_ELEMENTS(g_object_text_colors); i++)
        g_object_text_colors[i] = TERM_L_DARK;

    if (!resource_build_path(path, sizeof(path), RESOURCE_ROOT_PREF,
            "object_text_colors.json"))
    {
        log_warn("object text colors: unable to build config path");
        return;
    }

    f = ang_file_open(path, "rb");
    if (!f)
    {
        log_warn("object text colors: config not found at '%s'", path);
        return;
    }

    {
        Sint64 file_size = SDL_GetIOSize(f);
        if (file_size < 0 || file_size > 1024 * 1024)
        {
            log_warn("object text colors: invalid file size for '%s'", path);
            ang_file_close(f);
            return;
        }

        buffer = mem_alloc_array((size_t)file_size + 1, char);
        if (!buffer)
        {
            log_error("object text colors: out of memory");
            ang_file_close(f);
            return;
        }

        read = SDL_ReadIO(f, buffer, (size_t)file_size);
        buffer[read] = '\0';
    }

    ang_file_close(f);
    root = cJSON_Parse(buffer);
    mem_free(buffer);
    buffer = NULL;

    if (!root)
    {
        log_warn("object text colors: failed to parse '%s'", path);
        return;
    }

    {
        cJSON* default_attr = cJSON_GetObjectItemCaseSensitive(root,
            "defaultAttr");

        if (cJSON_IsNumber(default_attr))
        {
            int attr = default_attr->valueint;
            if (attr >= 0 && attr <= 255)
            {
                for (int i = 0; i < (int)N_ELEMENTS(g_object_text_colors); i++)
                    g_object_text_colors[i] = (byte)attr;
            }
        }
    }

    {
        cJSON* entries = cJSON_GetObjectItemCaseSensitive(root, "entries");
        cJSON* entry = NULL;

        if (!cJSON_IsArray(entries))
        {
            log_warn("object text colors: missing 'entries' array in '%s'",
                path);
            cJSON_Delete(root);
            return;
        }

        cJSON_ArrayForEach(entry, entries)
        {
            cJSON* tval = cJSON_GetObjectItemCaseSensitive(entry, "tval");
            cJSON* attr = cJSON_GetObjectItemCaseSensitive(entry, "attr");
            int tval_value;
            int attr_value;

            if (!cJSON_IsNumber(tval) || !cJSON_IsNumber(attr))
                continue;

            tval_value = tval->valueint;
            attr_value = attr->valueint;

            if (tval_value < 0
                || tval_value >= (int)N_ELEMENTS(g_object_text_colors))
            {
                continue;
            }
            if (attr_value < 0 || attr_value > 255)
                continue;

            g_object_text_colors[tval_value] = (byte)attr_value;
            loaded_entries++;
        }
    }

    cJSON_Delete(root);
    log_debug("object text colors: loaded %d entries from '%s'",
        loaded_entries, path);
}

/*
 * Get the display color for an object text, applying artifact shade if identified
 * This is used for TEXT color in inventory/equipment displays
 * Uses MAKE_EXTENDED_COLOR to create proper shaded colors
 */
byte object_display_color(const object_type* o_ptr, byte base_color)
{
    if (!o_ptr || !o_ptr->k_idx)
        return base_color;

    /* Category colors come from config, but state-driven overrides stay here. */
    if (unidentified_items_slate && object_is_unidentified_for_display(o_ptr))
        return TERM_SLATE;

    byte color_to_use = base_color;

    /* Bows are light umber by default, but allow artifact coloring to override */
    if (o_ptr->tval == TV_BOW)
    {
        color_to_use = TERM_L_UMBER;
    }

    /* Check for artifact-specific color (works in both modes) */
    if (o_ptr->name1 && a_info[o_ptr->name1].d_attr)
    {
        color_to_use = a_info[o_ptr->name1].d_attr;
    }

    /* Apply special handling when artifact_unique_color option is enabled */
    if (artifact_unique_color)
    {
        /* Identified artifacts are yellow (including artifact rings) */
        if (artefact_p(o_ptr) && object_known_p(o_ptr))
        {
            return TERM_YELLOW;
        }

        /* Non-artifact rings are orange when option is enabled */
        if (o_ptr->tval == TV_RING)
        {
            return TERM_ORANGE;
        }
    }
    else
    {
        /* Default mode: use shade 3 for identified artifacts */
        if (artefact_p(o_ptr) && object_known_p(o_ptr))
        {
            return MAKE_EXTENDED_COLOR(color_to_use, 3);
        }
    }

    if (unidentified_items_slate && color_to_use == TERM_SLATE)
        return TERM_WHITE;

    return color_to_use;
}

byte object_default_text_color(const object_type* o_ptr)
{
    byte attr;

    if (!o_ptr || !o_ptr->k_idx)
        return TERM_L_DARK;

    if (o_ptr->tval < (int)N_ELEMENTS(g_object_text_colors))
        return g_object_text_colors[o_ptr->tval];

    attr = object_attr_default(o_ptr);
    if (attr & TILE_FLAG)
        attr = k_info[o_ptr->k_idx].d_attr;

    return GET_BASE_COLOR(attr);
}

/*
 * Reset the "visual" lists
 *
 * This involves resetting various things to their "default" state.
 *
 * All content-owned visuals now come from the edit/raw data.
 * The parameter is retained only for call-site compatibility.
 */
void reset_visuals(bool unused)
{
    int i;

    /* Unused parameter */
    (void)unused;

    /* Extract default attr/char code for features */
    for (i = 0; i < z_info->f_max; i++)
    {
        feature_type* f_ptr = &f_info[i];

        /* Only reset if no tile was specified in data file (T: line) */
        if (!(f_ptr->x_attr & 0x80))
        {
            f_ptr->x_attr = f_ptr->d_attr;
            f_ptr->x_char = f_ptr->d_char;
        }
    }

    /* Extract default attr/char code for objects */
    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Only reset if no tile was specified in data file (T: line) */
        if (!(k_ptr->x_attr & 0x80))
        {
            k_ptr->x_attr = k_ptr->d_attr;
            k_ptr->x_char = k_ptr->d_char;
        }
    }

    /* Extract default attr/char code for monsters */
    for (i = 0; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];

        /* Only reset if no tile was specified in data file (T: line) */
        if (!(r_ptr->x_attr & 0x80))
        {
            r_ptr->x_attr = r_ptr->d_attr;
            r_ptr->x_char = r_ptr->d_char;
        }
    }

    /* Extract default attr/char code for flavors */
    for (i = 0; i < z_info->flavor_max; i++)
    {
        flavor_type* flavor_ptr = &flavor_info[i];

        /* Only reset if no tile was specified in data file (T: line) */
        if (!(flavor_ptr->x_attr & 0x80))
        {
            flavor_ptr->x_attr = flavor_ptr->d_attr;
            flavor_ptr->x_char = flavor_ptr->d_char;
        }
    }

    load_object_text_colors_json();
    refresh_effect_visuals_for_graphics_mode();
}
