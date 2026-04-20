/* File: cmd-ui-settings-visuals.c */
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
#include "app/app-command.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-time.h"
#include "sdl-config.h"
#include "sdl-main-internal.h"
#include "sound-config.h"
#include "cmd-ui-settings-internal.h"
#include "cmd-ui.h"
#include "cmd-ui-settings.h"
#include "ui/ui-information-scene.h"

static int visuals_menu(int* highlight)
{
    const settings_choice_entry entries[] = {
#ifdef ALLOW_VISUALS
        { 1, '1', "1) Change monster attr/chars", false },
        { 2, '2', "2) Change object attr/chars", false },
        { 3, '3', "3) Change feature attr/chars", false },
        { 4, '4', "4) Change flavor attr/chars", false },
#endif
        { 10, '0', "0) Reset visuals", false },
    };

    return settings_choice_menu("Interact with Visuals", entries,
        (int)N_ELEMENTS(entries), highlight, 0);
}

static int palette_presets_menu(int* highlight)
{
    settings_choice_entry entries[UI_COLOR_PRESET_MAX + 1];
    char labels[UI_COLOR_PRESET_MAX][96];
    int entry_count = 0;

    for (int i = 0; i < ui_colors_palette_preset_count()
        && entry_count < UI_COLOR_PRESET_MAX; i++)
    {
        char hotkey = (i < 9) ? (char)('1' + i) : (char)('a' + (i - 9));

        strnfmt(labels[entry_count], sizeof(labels[entry_count]), "%c) %s",
            hotkey, ui_colors_palette_preset_label(i));
        entries[entry_count] = (settings_choice_entry){ entry_count + 1,
            hotkey, labels[entry_count], false };
        entry_count++;
    }

    if (entry_count <= 0)
        return 0;

    return settings_choice_menu("Palette Presets", entries, entry_count,
        highlight, 0);
}

/*
 * Settings editor helper: add a browser row with a sample icon.
 */
static bool settings_browser_add_icon_pair_row(app_ui_panel* panel, s16b id,
    byte attr, byte meta_attr, byte icon_attr, char icon_char, bool enabled,
    bool selected, cptr label, cptr meta)
{
    return app_ui_panel_add_row_ex(panel, id, attr, meta_attr, icon_attr,
        icon_char, enabled, selected, "", label ? label : "",
        meta ? meta : "");
}

static void settings_format_attr_char_pair(char* buf, size_t buflen, byte attr,
    byte ch)
{
    strnfmt(buf, buflen, "%3u / %3u", attr, ch);
}

static bool settings_visual_present_ui_scene(cptr title, cptr kind_label,
    int index, cptr name, byte default_attr, byte default_char,
    byte current_attr, byte current_char)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char label_buf[APP_UI_LABEL_MAX];
    char meta_buf[APP_UI_META_MAX];

    panel = settings_browser_scene_begin_ex(&scene, title,
        "n/N move  a/A attr  c/C char  s shade  Esc back", 980, 2048);
    if (!panel)
        return false;

    strnfmt(label_buf, sizeof(label_buf), "%s %d", kind_label, index);
    strnfmt(meta_buf, sizeof(meta_buf), "%s", name ? name : "(unnamed)");
    if (!settings_browser_add_icon_pair_row(panel, 0, TERM_L_BLUE, TERM_SLATE,
            current_attr, (char)current_char, true, true, label_buf, meta_buf))
    {
        return false;
    }

    settings_format_attr_char_pair(meta_buf, sizeof(meta_buf), default_attr,
        default_char);
    if (!settings_browser_add_icon_pair_row(panel, 1, TERM_WHITE, TERM_SLATE,
            default_attr, (char)default_char, true, false,
            "Default attr/char", meta_buf))
    {
        return false;
    }

    settings_format_attr_char_pair(meta_buf, sizeof(meta_buf), current_attr,
        current_char);
    if (!settings_browser_add_icon_pair_row(panel, 2, TERM_WHITE, TERM_SLATE,
            current_attr, (char)current_char, true, false,
            "Current attr/char", meta_buf))
    {
        return false;
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "The sample icon previews the current glyph.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "n/N",
        "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "a/A",
        "Attr");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "c/C",
        "Char");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true, "s",
        "Shade");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true, "Esc",
        "Back");
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->focus_id = 0;
    return ui_information_scene_present_ui(&scene);
}

static bool settings_color_is_black(byte attr)
{
    return ((int)angband_color_table[attr][1]
            + (int)angband_color_table[attr][2]
            + (int)angband_color_table[attr][3])
        == 0;
}

static byte settings_color_sample_attr(byte attr)
{
    return settings_color_is_black(attr) ? TERM_WHITE : attr;
}

static char settings_color_sample_char(byte attr)
{
    return settings_color_is_black(attr) ? '.' : '#';
}

static bool settings_color_picker_present_ui_scene(cptr title, cptr subtitle,
    byte attr, cptr label)
{
    app_ui_scene scene;
    app_ui_panel* panel;
    char meta_buf[APP_UI_META_MAX];

    panel = settings_browser_scene_begin_ex(&scene, title, subtitle, 960, 1800);
    if (!panel)
        return false;

    strnfmt(meta_buf, sizeof(meta_buf), "attr %d", attr);
    if (!settings_browser_add_icon_pair_row(panel, 0, TERM_L_BLUE, TERM_SLATE,
            settings_color_sample_attr(attr), settings_color_sample_char(attr),
            true, true, label ? label : "", meta_buf))
    {
        return false;
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "4/6 changes the value. Enter accepts. Escape cancels.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "4/6",
        "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "Enter",
        "Accept");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "Esc",
        "Cancel");
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->focus_id = 0;
    return ui_information_scene_present_ui(&scene);
}

/*
 * Asks to the player for an extended color. It is done in two steps:
 * 1. Asks for the base color.
 * 2. Asks for a specific shade.
 * If the user press ESCAPE no changes are made to attr.
 */
static void askfor_shade(byte* attr, int y)
{
    byte base, shade, temp;
    bool changed = false;
    int ch;

    (void)y;

    /* Start with the given base color */
    base = GET_BASE_COLOR(*attr);

    /* 1. Query for base color */
    while (1)
    {
        if (!settings_color_picker_present_ui_scene("Shade Picker",
                "1/2 Choose base color", base, color_names[base]))
        {
            return;
        }
        ch = settings_ui_read_key(false);

        /* Cancel */
        if (ch == ESCAPE)
        {
            return;
        }

        /* Accept the current base color */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous color if possible */
        if ((ch == '4') && (base > 0))
        {
            --base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }

        /* Move to the next color if possible */
        if ((ch == '6') && (base < MAX_BASE_COLORS - 1))
        {
            ++base;
            /* Reset the shade, see below */
            changed = true;
            continue;
        }
    }

    /* The player selected a different base color, start from shade 0 */
    if (changed)
        shade = 0;
    /* We assume that the player is editing the current shade, go there */
    else
        shade = GET_SHADE(*attr);

    /* 2. Query for specific shade */
    while (1)
    {
        /* Create the real color */
        temp = MAKE_EXTENDED_COLOR(base, shade);
        if (!settings_color_picker_present_ui_scene("Shade Picker",
                "2/2 Choose shade", temp, get_ext_color_name(temp)))
        {
            return;
        }
        ch = settings_ui_read_key(false);

        /* Cancel */
        if (ch == ESCAPE)
            return;

        /* Accept the current shade */
        if ((ch == '\r') || (ch == '\n'))
            break;

        /* Move to the previous shade if possible */
        if ((ch == '4') && (shade > 0))
        {
            --shade;
            continue;
        }

        /* Move to the next shade if possible */
        if ((ch == '6') && (shade < MAX_SHADES - 1))
        {
            ++shade;
            continue;
        }
    }

    /* Assign the selected shade */
    *attr = temp;
}

/*
 * Interact with "visuals"
 */
void do_cmd_visuals(void)
{
    int choice = 0;
    int highlight = 1;

    /* Interact until done */
    while (1)
    {
        choice = visuals_menu(&highlight);
        if (choice == 0)
            break;

#ifdef ALLOW_VISUALS

        /* Modify monster attr/chars */
        if (choice == 1)
        {
            static int r = 0;

            while (1)
            {
                monster_race* r_ptr = &r_info[r];
                byte da = (byte)r_ptr->d_attr;
                byte dc = (byte)r_ptr->d_char;
                byte ca = (byte)r_ptr->x_attr;
                byte cc = (byte)r_ptr->x_char;
                int cx;

                if (!settings_visual_present_ui_scene(
                        "Monster Attr/Char", "Monster", r,
                        r_name + r_ptr->name, da, dc, ca, cc))
                {
                    break;
                }
                cx = settings_ui_read_key(false);

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    r = (r + z_info->r_max + 1) % z_info->r_max;
                if (cx == 'N')
                    r = (r + z_info->r_max - 1) % z_info->r_max;
                if (cx == 'a')
                    r_ptr->x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    r_ptr->x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    r_ptr->x_char = (byte)(cc + 1);
                if (cx == 'C')
                    r_ptr->x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&r_ptr->x_attr, 22);
                }
            }
        }

        /* Modify object attr/chars */
        else if (choice == 2)
        {
            static int k = 0;

            while (1)
            {
                object_kind* k_ptr = &k_info[k];
                byte da = (byte)k_ptr->d_attr;
                byte dc = (byte)k_ptr->d_char;
                byte ca = (byte)k_ptr->x_attr;
                byte cc = (byte)k_ptr->x_char;
                int cx;

                if (!settings_visual_present_ui_scene(
                        "Object Attr/Char", "Object", k,
                        k_name + k_ptr->name, da, dc, ca, cc))
                {
                    break;
                }
                cx = settings_ui_read_key(false);

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    k = (k + z_info->k_max + 1) % z_info->k_max;
                if (cx == 'N')
                    k = (k + z_info->k_max - 1) % z_info->k_max;
                if (cx == 'a')
                    k_info[k].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    k_info[k].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    k_info[k].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    k_info[k].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&k_info[k].x_attr, 22);
                }
            }
        }

        /* Modify feature attr/chars */
        else if (choice == 3)
        {
            static int f = 0;

            while (1)
            {
                feature_type* f_ptr = &f_info[f];
                byte da = (byte)f_ptr->d_attr;
                byte dc = (byte)f_ptr->d_char;
                byte ca = (byte)f_ptr->x_attr;
                byte cc = (byte)f_ptr->x_char;
                int cx;

                if (!settings_visual_present_ui_scene(
                        "Feature Attr/Char", "Terrain", f,
                        f_name + f_ptr->name, da, dc, ca, cc))
                {
                    break;
                }
                cx = settings_ui_read_key(false);

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->f_max + 1) % z_info->f_max;
                if (cx == 'N')
                    f = (f + z_info->f_max - 1) % z_info->f_max;
                if (cx == 'a')
                    f_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    f_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    f_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    f_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&f_info[f].x_attr, 22);
                }
            }
        }

        /* Modify flavor attr/chars */
        else if (choice == 4)
        {
            static int f = 0;

            while (1)
            {
                flavor_type* flavor_ptr = &flavor_info[f];
                byte da = (byte)flavor_ptr->d_attr;
                byte dc = (byte)flavor_ptr->d_char;
                byte ca = (byte)flavor_ptr->x_attr;
                byte cc = (byte)flavor_ptr->x_char;
                int cx;

                if (!settings_visual_present_ui_scene(
                        "Flavor Attr/Char", "Flavor", f,
                        flavor_text + flavor_ptr->text, da, dc, ca, cc))
                {
                    break;
                }
                cx = settings_ui_read_key(false);

                /* All done */
                if (cx == ESCAPE)
                    break;

                /* Analyze */
                if (cx == 'n')
                    f = (f + z_info->flavor_max + 1) % z_info->flavor_max;
                if (cx == 'N')
                    f = (f + z_info->flavor_max - 1) % z_info->flavor_max;
                if (cx == 'a')
                    flavor_info[f].x_attr = (byte)(ca + 1);
                if (cx == 'A')
                    flavor_info[f].x_attr = (byte)(ca - 1);
                if (cx == 'c')
                    flavor_info[f].x_char = (byte)(cc + 1);
                if (cx == 'C')
                    flavor_info[f].x_char = (byte)(cc - 1);
                if (cx == 's')
                {
                    askfor_shade(&flavor_info[f].x_attr, 22);
                }
            }
        }

#endif /* ALLOW_VISUALS */

        /* Reset visuals */
        else if (choice == 10)
        {
            /* Reset */
            reset_visuals(true);

            /* Message */
            msg_print("Visual attr/char tables reset.");
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for visuals!");
        }

        /* Flush messages */
        message_flush();
    }
}

/*
 * Interact with "colors"
 */
void do_cmd_colors(void)
{
    int highlight = 1;
    int choice;
    cptr current = ui_colors_current_palette_preset();
    cptr config_label = settings_sdl_config_path();
    cptr preset_id;

    if (ui_colors_palette_preset_count() <= 0)
        ui_colors_load_palette_presets();
    if (ui_colors_palette_preset_count() <= 0)
    {
        msg_print("No palette presets are available.");
        message_flush();
        return;
    }

    for (int i = 0; i < ui_colors_palette_preset_count(); i++)
    {
        if (current && streq(current, ui_colors_palette_preset_id(i)))
        {
            highlight = i + 1;
            break;
        }
    }

    choice = palette_presets_menu(&highlight);
    if (choice <= 0)
        return;

    preset_id = ui_colors_palette_preset_id(choice - 1);
    if (!preset_id || !ui_colors_apply_palette_preset(preset_id))
    {
        msg_print("Failed to apply palette preset.");
        message_flush();
        return;
    }

    SDL_strlcpy(config.palette_preset, preset_id, sizeof(config.palette_preset));
    sdl_sync_palette();
    platform_frame_react();

    if (save_pane_config_to_json())
    {
        msg_format("Palette preset saved to %s",
            (config_label && config_label[0]) ? config_label : "sil_sdl.json");
    }
    else
    {
        msg_print("Palette preset changed, but saving the SDL config failed.");
    }

    message_flush();
}

