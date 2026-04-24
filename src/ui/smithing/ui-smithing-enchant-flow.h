/* File: ui-smithing-enchant-flow.h */
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

/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static bool smith_ui_enchant_build_scene(app_ui_scene* scene,
    const smith_ui_enchant_menu_state* state, int highlight)
{
    app_ui_panel* panel;

    if (!scene || !state)
        return false;

    if (highlight < 1 || highlight > state->entry_count)
        highlight = 1;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_CRAFTING;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        state->selecting_prefix ? "Enchant: choose prefix"
                                : "Enchant: choose suffix");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 selects, 8/2 moves, a-z jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Select");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-z", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        state->entry_count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    for (int i = 0; i < state->entry_count; i++)
    {
        char key[APP_UI_KEY_MAX];
        char label[APP_UI_LABEL_MAX];
        byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE : state->row_attr[i];

        if (i == 0)
            SDL_strlcpy(label, "(none)", sizeof(label));
        else
            ego_name_for_enchant_menu(state->choice[i], label, sizeof(label));
        key[0] = '\0';
        if (i < 26)
            strnfmt(key, sizeof(key), "%c", (char)('a' + i));
        if (!app_ui_panel_add_row(panel, (s16b)(i + 1), row_attr,
                state->valid[i], i + 1 == highlight, key, label, ""))
        {
            return false;
        }
    }

    return smith_ui_enchant_add_selected_detail(panel, state, highlight);
}

static bool smith_ui_enchant_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int prefix_highlight = 1;
    int suffix_highlight = 1;
    bool completed = false;
    bool leave_menu = false;
    int selected_prefix;
    int selected_suffix;
    bool show_prefix_step;
    bool show_suffix_step;
    bool selecting_prefix;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return false;

    smith_o_ptr->name1 = 0;
    smith2_o_ptr->name1 = 0;

    selected_prefix = (int)object_ego_prefix(smith_o_ptr);
    selected_suffix = (int)object_ego_suffix(smith_o_ptr);
    show_prefix_step = enchant_menu_has_applicable_affix(
        smith2_o_ptr, 0, selected_suffix, true) || (selected_prefix != 0);
    show_suffix_step = enchant_menu_has_applicable_affix(
        smith2_o_ptr, selected_prefix, 0, false) || (selected_suffix != 0);

    if (!show_prefix_step && !show_suffix_step)
    {
        smith_ui_snapshot_scene_close(&scope);
        return false;
    }

    selecting_prefix = show_prefix_step;

    while (!leave_menu)
    {
        app_ui_scene scene;
        smith_ui_enchant_menu_state state;
        int choice;
        char ch;

        smith_ui_enchant_build_state(&state, selecting_prefix,
            selecting_prefix ? 0 : selected_prefix,
            selecting_prefix ? selected_suffix : 0, smith2_o_ptr);
        if (selecting_prefix)
        {
            if (prefix_highlight < 1 || prefix_highlight > state.entry_count)
                prefix_highlight = 1;
            if (!smith_ui_enchant_build_scene(&scene, &state, prefix_highlight)
                || !smith_ui_snapshot_scene_present(&scope, &scene))
            {
                log_warn("smithing snapshot enchant menu: failed to build or publish prefix scene");
                break;
            }
        }
        else
        {
            if (suffix_highlight < 1 || suffix_highlight > state.entry_count)
                suffix_highlight = 1;
            if (!smith_ui_enchant_build_scene(&scene, &state, suffix_highlight)
                || !smith_ui_snapshot_scene_present(&scope, &scene))
            {
                log_warn("smithing snapshot enchant menu: failed to build or publish suffix scene");
                break;
            }
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.entry_count);
        if (choice > 0)
        {
            if (selecting_prefix)
                prefix_highlight = choice;
            else
                suffix_highlight = choice;
        }
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            int* highlight_ptr = selecting_prefix ? &prefix_highlight
                                                  : &suffix_highlight;

            if (*highlight_ptr > 1)
                (*highlight_ptr)--;
            else
                *highlight_ptr = state.entry_count;
            continue;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            int* highlight_ptr = selecting_prefix ? &prefix_highlight
                                                  : &suffix_highlight;

            if (*highlight_ptr < state.entry_count)
                (*highlight_ptr)++;
            else
                *highlight_ptr = 1;
            continue;
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            if (selecting_prefix)
            {
                completed = false;
                leave_menu = true;
            }
            else
            {
                create_special(selected_prefix, selected_suffix);
                selecting_prefix = true;
            }
            continue;
        }
        else if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            /* Use current highlight */
        }
        else
        {
            continue;
        }

        if (selecting_prefix)
            create_special(state.choice[prefix_highlight - 1], selected_suffix);
        else
            create_special(selected_prefix, state.choice[suffix_highlight - 1]);

        if (selecting_prefix)
        {
            selected_prefix = (int)object_ego_prefix(smith_o_ptr);
            if (show_suffix_step)
            {
                selecting_prefix = false;
                continue;
            }
            completed = true;
            leave_menu = true;
        }
        else
        {
            selected_suffix = (int)object_ego_suffix(smith_o_ptr);
            completed = true;
            leave_menu = true;
        }
    }

    smith_ui_snapshot_scene_close(&scope);
    return completed;
}
