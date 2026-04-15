/* File: cmd-ui-settings-visuals.c */

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
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "ui/ui-information-scene.h"

static int visuals_menu(int* highlight)
{
    const settings_choice_entry entries[] = {
        { 1, '1', "1) Load a user pref file", false },
#ifdef ALLOW_VISUALS
        { 2, '2', "2) Dump monster attr/chars", false },
        { 3, '3', "3) Dump object attr/chars", false },
        { 4, '4', "4) Dump feature attr/chars", false },
        { 5, '5', "5) Dump flavor attr/chars", false },
        { 6, '6', "6) Change monster attr/chars", false },
        { 7, '7', "7) Change object attr/chars", false },
        { 8, '8', "8) Change feature attr/chars", false },
        { 9, '9', "9) Change flavor attr/chars", false },
#endif
        { 10, '0', "0) Reset visuals", false },
    };

    return settings_choice_menu("Interact with Visuals", entries,
        (int)N_ELEMENTS(entries), highlight, 0);
}

static int colors_menu(int* highlight)
{
    const settings_choice_entry entries[] = {
        { 1, '1', "1) Load a user pref file", false },
#ifdef ALLOW_COLORS
        { 2, '2', "2) Dump colors", false },
        { 3, '3', "3) Modify colors", false },
#endif
    };

    return settings_choice_menu("Interact with Colors", entries,
        (int)N_ELEMENTS(entries), highlight, 0);
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

static void settings_color_editor_adjust_view(int total_rows, int visible_rows,
    int current, int* scroll)
{
    int max_scroll;

    if (!scroll)
        return;

    if (visible_rows < 1)
        visible_rows = 1;

    if (current < *scroll)
        *scroll = current;
    else if (current >= *scroll + visible_rows)
        *scroll = current - visible_rows + 1;

    max_scroll = total_rows - visible_rows;
    if (max_scroll < 0)
        max_scroll = 0;
    if (*scroll > max_scroll)
        *scroll = max_scroll;
    if (*scroll < 0)
        *scroll = 0;
}

static void settings_color_format_label(char* buf, size_t buflen, int idx)
{
    strnfmt(buf, buflen, "%2d  %s", idx, get_ext_color_name(idx));
}

static void settings_color_format_meta(char* buf, size_t buflen, int idx)
{
    strnfmt(buf, buflen, "K %3d  RGB %3d,%3d,%3d",
        angband_color_table[idx][0], angband_color_table[idx][1],
        angband_color_table[idx][2], angband_color_table[idx][3]);
}

static bool settings_color_editor_present_ui_scene(int idx, int scroll)
{
    app_ui_scene scene;
    app_ui_panel* panel;

    panel = settings_browser_scene_begin_ex(&scene, "Modify Colors",
        "8/2 move  4/6 jump  k/r/g/b adjust  c copy  v exact  Esc back",
        1100, 2200);
    if (!panel)
        return false;

    if (scroll > 0)
        app_ui_panel_set_row_offset(panel, (s16b)scroll);

    for (int i = 0; i < MAX_COLORS; i++)
    {
        char label_buf[APP_UI_LABEL_MAX];
        char meta_buf[APP_UI_META_MAX];
        byte row_attr = (i == idx) ? TERM_L_BLUE : TERM_WHITE;

        settings_color_format_label(label_buf, sizeof(label_buf), i);
        settings_color_format_meta(meta_buf, sizeof(meta_buf), i);
        if (!settings_browser_add_icon_pair_row(panel, (s16b)i, row_attr,
                TERM_SLATE, settings_color_sample_attr((byte)i),
                settings_color_sample_char((byte)i), true, i == idx,
                label_buf, meta_buf))
        {
            return false;
        }
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Use k/K, r/R, g/G, and b/B to adjust values.");
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Use c to copy from another color or v to enter exact values.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true, "8/2",
        "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true, "4/6",
        "Jump");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true, "k/r/g/b",
        "Adjust");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true, "c",
        "Copy");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true, "v",
        "Exact");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true, "Esc",
        "Back");
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->focus_id = (s16b)idx;
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
    int i;
    ang_file* fff;
    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Interact until done */
    while (1)
    {
        choice = visuals_menu(&highlight);
        if (choice == 0)
            break;

        /* Load a user pref file */
        if (choice == 1)
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(15);
        }

#ifdef ALLOW_VISUALS

        /* Dump monster attr/chars */
        else if (choice == 2)
        {
            static cptr mark = "Monster attr/chars";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Visuals",
                    "Enter the file name for the monster visual dump.", "",
                    ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_monsters: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = ang_file_open_compat(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            ang_file_printf_compat(fff, "\n\n");

            /* Start dumping */
            ang_file_printf_compat(fff, "# Monster attr/char definitions\n\n");

            /* Dump monsters */
            for (i = 0; i < z_info->r_max; i++)
            {
                monster_race* r_ptr = &r_info[i];

                /* Skip non-entries */
                if (!r_ptr->name)
                    continue;

                /* Dump a comment */
                ang_file_printf_compat(fff, "# %s\n", (r_name + r_ptr->name));

                /* Dump the monster attr/char info */
                dump_visual_pair(fff, "R", i, r_ptr->x_attr, (byte)r_ptr->x_char);
            }

            /* All done */
            ang_file_printf_compat(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            (void)ang_file_close_compat(fff);

            /* Message */
            msg_print("Dumped monster attr/chars.");
        }

        /* Dump object attr/chars */
        else if (choice == 3)
        {
            static cptr mark = "Object attr/chars";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Visuals",
                    "Enter the file name for the object visual dump.", "",
                    ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_objects: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = ang_file_open_compat(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            ang_file_printf_compat(fff, "\n\n");

            /* Start dumping */
            ang_file_printf_compat(fff, "# Object attr/char definitions\n\n");

            /* Dump objects */
            for (i = 0; i < z_info->k_max; i++)
            {
                object_kind* k_ptr = &k_info[i];

                /* Skip non-entries */
                if (!k_ptr->name)
                    continue;

                /* Dump a comment */
                ang_file_printf_compat(fff, "# %s\n", (k_name + k_ptr->name));

                /* Dump the object attr/char info */
                dump_visual_pair(
                    fff, "K", i, k_ptr->x_attr, (byte)k_ptr->x_char);
            }

            /* All done */
            ang_file_printf_compat(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            (void)ang_file_close_compat(fff);

            /* Message */
            msg_print("Dumped object attr/chars.");
        }

        /* Dump feature attr/chars */
        else if (choice == 4)
        {
            static cptr mark = "Feature attr/chars";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Visuals",
                    "Enter the file name for the feature visual dump.", "",
                    ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_features: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = ang_file_open_compat(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            ang_file_printf_compat(fff, "\n\n");

            /* Start dumping */
            ang_file_printf_compat(fff, "# Feature attr/char definitions\n\n");

            /* Dump features */
            for (i = 0; i < z_info->f_max; i++)
            {
                feature_type* f_ptr = &f_info[i];

                /* Skip non-entries */
                if (!f_ptr->name)
                    continue;

                /* Dump a comment */
                ang_file_printf_compat(fff, "# %s\n", (f_name + f_ptr->name));

                /* Dump the feature attr/char info */
                dump_visual_pair(
                    fff, "F", i, f_ptr->x_attr, (byte)f_ptr->x_char);
            }

            /* All done */
            ang_file_printf_compat(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            (void)ang_file_close_compat(fff);

            /* Message */
            msg_print("Dumped feature attr/chars.");
        }

        /* Dump flavor attr/chars */
        else if (choice == 5)
        {
            static cptr mark = "Flavor attr/chars";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Visuals",
                    "Enter the file name for the flavor visual dump.", "",
                    ftmp, sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_flavors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old attr/chars */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = ang_file_open_compat(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            ang_file_printf_compat(fff, "\n\n");

            /* Start dumping */
            ang_file_printf_compat(fff, "# Flavor attr/char definitions\n\n");

            /* Dump flavors */
            for (i = 0; i < z_info->flavor_max; i++)
            {
                flavor_type* flavor_ptr = &flavor_info[i];

                /* Dump a comment */
                ang_file_printf_compat(fff, "# %s\n", (flavor_text + flavor_ptr->text));

                /* Dump the flavor attr/char info */
                dump_visual_pair(
                    fff, "L", i, flavor_ptr->x_attr, (byte)flavor_ptr->x_char);
            }

            /* All done */
            ang_file_printf_compat(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            (void)ang_file_close_compat(fff);

            /* Message */
            msg_print("Dumped flavor attr/chars.");
        }

        /* Modify monster attr/chars */
        else if (choice == 6)
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
        else if (choice == 7)
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
        else if (choice == 8)
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
        else if (choice == 9)
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
 * Asks to the user for specific color values.
 * Returns true if the color was modified.
 */
static bool askfor_color_values(int idx)
{
    char str[10];

    int k, r, g, b;

    /* Get the default value */
    strnfmt(str, sizeof(str), "%d", angband_color_table[idx][1]);

    /* Query, check for ESCAPE */
    if (!settings_ui_prompt_string("Modify Colors",
            "Enter red (0-255).", "Current channel value.", str,
            sizeof(str)))
        return false;

    /* Convert to number */
    r = atoi(str);

    /* Check bounds */
    if (r < 0)
        r = 0;
    if (r > 255)
        r = 255;

    /* Get the default value */
    strnfmt(str, sizeof(str), "%d", angband_color_table[idx][2]);

    /* Query, check for ESCAPE */
    if (!settings_ui_prompt_string("Modify Colors",
            "Enter green (0-255).", "Current channel value.", str,
            sizeof(str)))
        return false;

    /* Convert to number */
    g = atoi(str);

    /* Check bounds */
    if (g < 0)
        g = 0;
    if (g > 255)
        g = 255;

    /* Get the default value */
    strnfmt(str, sizeof(str), "%d", angband_color_table[idx][3]);

    /* Query, check for ESCAPE */
    if (!settings_ui_prompt_string("Modify Colors",
            "Enter blue (0-255).", "Current channel value.", str,
            sizeof(str)))
        return false;

    /* Convert to number */
    b = atoi(str);

    /* Check bounds */
    if (b < 0)
        b = 0;
    if (b > 255)
        b = 255;

    /* Get the default value */
    strnfmt(str, sizeof(str), "%d", angband_color_table[idx][0]);

    /* Query, check for ESCAPE */
    if (!settings_ui_prompt_string("Modify Colors",
            "Enter extra (0-255).", "Current channel value.", str,
            sizeof(str)))
        return false;

    /* Convert to number */
    k = atoi(str);

    /* Check bounds */
    if (k < 0)
        k = 0;
    if (k > 255)
        k = 255;

    /* Do nothing if the color is not modified */
    if ((k == angband_color_table[idx][0]) && (r == angband_color_table[idx][1])
        && (g == angband_color_table[idx][2])
        && (b == angband_color_table[idx][3]))
        return false;

    /* Modify the color table */
    angband_color_table[idx][0] = k;
    angband_color_table[idx][1] = r;
    angband_color_table[idx][2] = g;
    angband_color_table[idx][3] = b;

    /* Notify the changes */
    return true;
}

/* These two are used to place elements in the grid */
#define COLOR_X(idx) (((idx) / MAX_BASE_COLORS) * 5 + 1)
#define COLOR_Y(idx) ((idx) % MAX_BASE_COLORS + 6)

/* Hack - Note the cast to "int" to prevent overflow */
#define IS_BLACK(idx)                                                          \
    ((int)angband_color_table[idx][1] + (int)angband_color_table[idx][2]       \
            + (int)angband_color_table[idx][3]                                 \
        == 0)

/* We show black as dots to see the shape of the grid */
#define BLACK_SAMPLE "..."

/*
 * The screen used to modify the color table. Only 128 colors can be modified.
 * The remaining entries of the color table are reserved for graphic mode.
 */
static void modify_colors(void)
{
    int idx = 0;
    int scroll = 0;
    char ch;

    while (1)
    {
        settings_ui_layout layout = settings_ui_read_layout();
        int visible_rows = settings_ui_list_visible_rows(&layout, 0, 8, 1);
        int dir;

        settings_color_editor_adjust_view(MAX_COLORS, visible_rows, idx,
            &scroll);

        if (!settings_color_editor_present_ui_scene(idx, scroll))
        {
            return;
        }
        ch = settings_ui_read_key(false);

        dir = target_dir(ch);
        if ((dir == 2) || (dir == 4) || (dir == 6) || (dir == 8))
            ch = I2D(dir);

        switch (ch)
        {
        case ESCAPE:
            return;

        /* Down */
        case '2':
        {
            /* Check bounds */
            if (idx + 1 >= MAX_COLORS)
                break;

            /* Get the new position */
            ++idx;
            break;
        }

        /* Up */
        case '8':
        {
            /* Check bounds */
            if (idx - 1 < 0)
                break;

            /* Get the new position */
            --idx;
            break;
        }

        /* Left */
        case '4':
        {
            /* Check bounds */
            if (idx - 16 < 0)
                break;

            /* Get the new position */
            idx -= MAX_BASE_COLORS;
            break;
        }

            /* Right */
        case '6':
        {
            /* Check bounds */
            if (idx + MAX_BASE_COLORS >= MAX_COLORS)
                break;

            /* Get the new position */
            idx += MAX_BASE_COLORS;
            break;
        }

            /* Copy from color */
        case 'c':
        {
            char str[10];
            int src;

            /* Get the default value, the base color */
            sprintf(str, "%d", GET_BASE_COLOR(idx));

            /* Query, check for ESCAPE */
            if (!settings_ui_prompt_string("Modify Colors",
                    format("Copy from color (0-%d, def. base).",
                        MAX_COLORS - 1),
                    "Enter the source color index.", str, sizeof(str)))
                break;

            /* Convert to number */
            src = atoi(str);

            /* Check bounds */
            if (src < 0)
                src = 0;
            if (src >= MAX_COLORS)
                src = MAX_COLORS - 1;

            /* Do nothing if the colors are the same */
            if (src == idx)
                break;

            /* Modify the color table */
            angband_color_table[idx][0] = angband_color_table[src][0];
            angband_color_table[idx][1] = angband_color_table[src][1];
            angband_color_table[idx][2] = angband_color_table[src][2];
            angband_color_table[idx][3] = angband_color_table[src][3];
            break;
        }

        /* Increase the extra value */
        case 'k':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr + 1);
            break;
        }

        /* Decrease the extra value */
        case 'K':
        {
            /* Get a pointer to the proper value */
            byte* k_ptr = &angband_color_table[idx][0];

            /* Modify the value */
            *k_ptr = (byte)(*k_ptr - 1);
            break;
        }

        /* Increase the red value */
        case 'r':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr + 1);
            break;
        }

        /* Decrease the red value */
        case 'R':
        {
            /* Get a pointer to the proper value */
            byte* r_ptr = &angband_color_table[idx][1];

            /* Modify the value */
            *r_ptr = (byte)(*r_ptr - 1);
            break;
        }

            /* Increase the green value */
        case 'g':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr + 1);
            break;
        }

            /* Decrease the green value */
        case 'G':
        {
            /* Get a pointer to the proper value */
            byte* g_ptr = &angband_color_table[idx][2];

            /* Modify the value */
            *g_ptr = (byte)(*g_ptr - 1);
            break;
        }

            /* Increase the blue value */
        case 'b':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr + 1);
            break;
        }

        /* Decrease the blue value */
        case 'B':
        {
            /* Get a pointer to the proper value */
            byte* b_ptr = &angband_color_table[idx][3];

            /* Modify the value */
            *b_ptr = (byte)(*b_ptr - 1);
            break;
        }

        /* Ask for specific values */
        case 'v':
            (void)askfor_color_values(idx);
            break;
        }
    }
}

/*
 * Interact with "colors"
 */
void do_cmd_colors(void)
{
    int choice = 0;
    int highlight = 1;
    int i;
    ang_file* fff;
    char buf[1024];

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Interact until done */
    while (1)
    {
        choice = colors_menu(&highlight);
        if (choice == 0)
            break;

        /* Load a user pref file */
        if (choice == 1)
        {
            /* Ask for and load a user pref file */
            do_cmd_pref_file_hack(8);
            do_cmd_redraw();
        }

#ifdef ALLOW_COLORS

        /* Dump colors */
        else if (choice == 2)
        {
            static cptr mark = "Colors";
            char ftmp[80];

            /* Default filename */
            strnfmt(ftmp, sizeof(ftmp), "%s.prf", op_ptr->base_name);

            /* Get a filename */
            if (!settings_ui_prompt_string("Interact with Colors",
                    "Enter the file name for the color dump.", "", ftmp,
                    sizeof(ftmp)))
                continue;

            /* Build the filename */
            if (!path_build(buf, sizeof(buf), ANGBAND_DIR_USER, ftmp))
            {
                log_error("dump_colors: failed to build path for '%s'", ftmp);
                continue;
            }

            /* Remove old colors */
            remove_old_dump(buf, mark);

            /* Append to the file */
            fff = ang_file_open_compat(buf, "a");

            /* Failure */
            if (!fff)
                continue;

            /* Output header */
            pref_header(fff, mark);

            /* Skip some lines */
            ang_file_printf_compat(fff, "\n\n");

            /* Start dumping */
            ang_file_printf_compat(fff, "# Color redefinitions\n\n");

            /* Dump colors */
            for (i = 0; i < 256; i++)
            {
                int kv = angband_color_table[i][0];
                int rv = angband_color_table[i][1];
                int gv = angband_color_table[i][2];
                int bv = angband_color_table[i][3];

                cptr name = "unknown";

                /* Skip non-entries */
                if (!kv && !rv && !gv && !bv)
                    continue;

                /* Extract the color name */
                if (i < 16)
                    name = color_names[i];

                /* Dump a comment */
                ang_file_printf_compat(fff, "# Color '%s'\n", name);

                /* Dump the monster attr/char info */
                ang_file_printf_compat(fff,
                    "V:%d:0x%02X:0x%02X:0x%02X:0x%02X\n\n", i, kv, rv,
                    gv, bv);
            }

            /* All done */
            ang_file_printf_compat(fff, "\n\n\n\n");

            /* Output footer */
            pref_footer(fff, mark);

            /* Close */
            (void)ang_file_close_compat(fff);

            /* Message */
            msg_print("Dumped color redefinitions.");
        }

        /* Edit colors */
        else if (choice == 3)
        {
            modify_colors();
        }

#endif /* ALLOW_COLORS */

        /* Unknown option */
        else
        {
            bell("Illegal command for colors!");
        }

        /* Flush messages */
        message_flush();
    }
}


