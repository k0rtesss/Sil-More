/* File: ui-smithing-bonus-flow.h */
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

static bool smith_ui_bonus_build_scene(app_ui_scene* scene,
    const smith_ui_bonus_menu_state* state, int highlight)
{
    app_ui_panel* panel;

    if (!scene || !state)
        return false;

    if (highlight < 1)
        highlight = 1;
    if (state->action_count > 0 && highlight > state->action_count)
        highlight = state->action_count;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_CRAFTING;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Numbers: special bonuses");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 applies, 8/2 moves, a-z jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Apply");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Apply");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-z", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        state->action_count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    if (state->action_count <= 0)
    {
        if (!app_ui_panel_add_row(panel, 0, TERM_SLATE, true, false, "",
                "Nothing editable.", ""))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < state->action_count; i++)
        {
            char key[APP_UI_KEY_MAX];
            char label[APP_UI_LABEL_MAX];
            char meta[APP_UI_META_MAX];
            byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE
                                                 : state->row_attr[i];

            key[0] = '\0';
            if (i < 26)
                strnfmt(key, sizeof(key), "%c", (char)('a' + i));
            smith_ui_bonus_action_build_label(&state->actions[i], label,
                sizeof(label), meta, sizeof(meta));
            if (!app_ui_panel_add_row_ex(panel, (s16b)(i + 1), row_attr,
                    row_attr, 0, '\0', state->valid[i], i + 1 == highlight,
                    key, label, meta))
            {
                return false;
            }
        }
    }

    return smith_ui_bonus_add_selected_detail(panel, state, highlight);
}

static void smith_ui_bonus_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_bonus_menu_state state;
        int choice;
        char ch;

        smith_ui_bonus_build_state(&state);
        if (state.action_count > 0 && highlight > state.action_count)
            highlight = state.action_count;
        if (highlight < 1)
            highlight = 1;

        if (!smith_ui_bonus_build_scene(&scene, &state, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot bonus menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.action_count);
        if (choice > 0)
        {
            highlight = choice;
            if ((highlight <= state.action_count) && state.valid[highlight - 1])
            {
                (void)smith_adjust_bonus_entry(&state.actions[highlight - 1].entry,
                    state.actions[highlight - 1].delta);
            }
            else
            {
                bell("Invalid choice.");
            }
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if ((highlight <= state.action_count) && state.valid[highlight - 1])
            {
                (void)smith_adjust_bonus_entry(&state.actions[highlight - 1].entry,
                    state.actions[highlight - 1].delta);
            }
            else
            {
                bell("Invalid choice.");
            }
            continue;
        }

        if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            break;
        }

        if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (state.action_count > 0)
            {
                if (highlight > 1)
                    highlight--;
                else
                    highlight = state.action_count;
            }
            continue;
        }

        if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (state.action_count > 0)
            {
                if (highlight < state.action_count)
                    highlight++;
                else
                    highlight = 1;
            }
            continue;
        }
    }

    smith_ui_snapshot_scene_close(&scope);
}
