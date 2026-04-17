/* File: ui-smithing-numbers-flow.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static bool smith_ui_numbers_build_scene(app_ui_scene* scene,
    const smith_ui_numbers_menu_state* state, int highlight)
{
    app_ui_panel* panel;
    int choice;

    if (!scene || !state)
        return false;

    if (highlight < 1 || highlight > SMT_NUM_MENU_MAX)
        highlight = SMT_NUM_MENU_I_ATT;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Numbers");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 applies, 8/2 moves, a-m jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Apply");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Apply");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-m", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        SMT_NUM_MENU_MAX, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    for (choice = 1; choice <= SMT_NUM_MENU_MAX; choice++)
    {
        char key[APP_UI_KEY_MAX];
        byte row_attr = (choice == highlight) ? TERM_L_BLUE
                                              : state->row_attr[choice - 1];

        strnfmt(key, sizeof(key), "%c", (char)('a' + choice - 1));
        if (!app_ui_panel_add_row(panel, (s16b)choice, row_attr,
                state->valid[choice - 1], choice == highlight, key,
                smith_ui_numbers_action_label(choice), ""))
        {
            return false;
        }
    }

    return smith_ui_numbers_add_selected_detail(panel, state, highlight);
}

static void smith_ui_numbers_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_numbers_menu_state state;
        int choice;
        char ch;

        smith_ui_numbers_build_state(&state);
        if (highlight < 1 || highlight > SMT_NUM_MENU_MAX)
            highlight = SMT_NUM_MENU_I_ATT;

        if (!smith_ui_numbers_build_scene(&scene, &state, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot numbers menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, SMT_NUM_MENU_MAX);
        if (choice > 0)
        {
            highlight = choice;
            if (state.valid[highlight - 1])
            {
                if (highlight == SMT_NUM_MENU_EDIT_BONUSES)
                {
                    smith_ui_snapshot_begin_nested_transition();
                    smith_ui_snapshot_scene_close(&scope);
                    smith_bonus_menu();
                    smith_ui_snapshot_end_nested_transition();
                    if (!smith_ui_snapshot_scene_enter(&scope))
                        return;
                }
                else
                {
                    modify_numbers(highlight);
                }
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
            if (state.valid[highlight - 1])
            {
                if (highlight == SMT_NUM_MENU_EDIT_BONUSES)
                {
                    smith_ui_snapshot_begin_nested_transition();
                    smith_ui_snapshot_scene_close(&scope);
                    smith_bonus_menu();
                    smith_ui_snapshot_end_nested_transition();
                    if (!smith_ui_snapshot_scene_enter(&scope))
                        return;
                }
                else
                {
                    modify_numbers(highlight);
                }
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
            if (highlight > 1)
                highlight--;
            else
                highlight = SMT_NUM_MENU_MAX;
            continue;
        }

        if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (highlight < SMT_NUM_MENU_MAX)
                highlight++;
            else
                highlight = 1;
            continue;
        }
    }

    smith_ui_snapshot_scene_close(&scope);
}
