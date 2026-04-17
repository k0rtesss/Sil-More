/* File: ui-smithing-reforge-flow.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static bool smith_ui_reforge_build_scene(app_ui_scene* scene,
    const smith_ui_reforge_menu_state* state, int highlight)
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

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Reforge: choose prefix");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 selects, 8/2 moves, a-z jumps, Esc/4 cancels.");
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

    if (state->entry_count <= 0)
    {
        if (!app_ui_panel_add_row(panel, 0, TERM_SLATE, true, false, "",
                "Nothing available.", ""))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < state->entry_count; i++)
        {
            char key[APP_UI_KEY_MAX];
            char label[APP_UI_LABEL_MAX];
            byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE : state->row_attr[i];

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
    }

    return smith_ui_reforge_add_selected_detail(panel, state, highlight);
}

static int smith_ui_reforge_prefix_snapshot_menu(const object_type* source)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!source || !source->k_idx)
        return 0;
    if (!smith_ui_snapshot_scene_enter(&scope))
        return 0;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_reforge_menu_state state;
        int choice;
        char ch;

        smith_ui_reforge_build_state(&state, source);
        if (state.entry_count > 0 && highlight > state.entry_count)
            highlight = state.entry_count;
        if (highlight < 1)
            highlight = 1;

        if (!smith_ui_reforge_build_scene(&scene, &state, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot reforge menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.entry_count);
        if (choice > 0)
            highlight = choice;
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (state.entry_count > 0)
            {
                if (highlight > 1)
                    highlight--;
                else
                    highlight = state.entry_count;
            }
            continue;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (state.entry_count > 0)
            {
                if (highlight < state.entry_count)
                    highlight++;
                else
                    highlight = 1;
            }
            continue;
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            break;
        }
        else if (!((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            ))
        {
            continue;
        }

        if ((highlight <= state.entry_count) && state.valid[highlight - 1])
        {
            int result = state.choice[highlight - 1];

            smith_ui_snapshot_scene_close(&scope);
            return result;
        }

        bell("You cannot afford that reforge.");
    }

    smith_ui_snapshot_scene_close(&scope);
    return 0;
}
