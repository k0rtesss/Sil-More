/* File: ui-smithing-artefact-root.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static int smith_ui_artefact_root_entry_count(void)
{
    return MAX_CATS + (S_MAX - 1) + 1;
}

static int smith_ui_artefact_root_skill(int entry)
{
    int display_idx = 0;

    if (entry <= MAX_CATS)
        return -1;
    if (entry == smith_ui_artefact_root_entry_count())
        return -1;

    for (int skill = 0; skill < S_MAX; skill++)
    {
        if (skill == S_SPC)
            continue;
        display_idx++;
        if (MAX_CATS + display_idx == entry)
            return skill;
    }

    return -1;
}

static cptr smith_ui_artefact_root_label(int entry)
{
    int skill = smith_ui_artefact_root_skill(entry);

    if (entry >= 1 && entry <= MAX_CATS)
        return smithing_flag_cats[entry - 1].desc;
    if (skill >= 0)
        return skill_names_full[skill];
    if (entry == smith_ui_artefact_root_entry_count())
        return "Name Artefact";

    return "Artefact";
}

static bool smith_ui_artefact_root_add_selected_detail(app_ui_panel* panel,
    int highlight)
{
    int skill = smith_ui_artefact_root_skill(highlight);

    if (!panel)
        return false;

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        smith_ui_artefact_root_label(highlight));

    if (highlight >= 1 && highlight <= MAX_CATS)
    {
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Toggle individual artefact flags in this category."))
        {
            return false;
        }
    }
    else if (skill >= 0)
    {
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Toggle individual artefact abilities for this skill."))
        {
            return false;
        }
    }
    else
    {
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Rename the current custom artefact."))
        {
            return false;
        }
    }

    return smith_ui_main_menu_add_current_item_detail(panel);
}

static bool smith_ui_artefact_root_build_scene(app_ui_scene* scene, int highlight)
{
    app_ui_panel* panel;
    int count = smith_ui_artefact_root_entry_count();

    if (!scene)
        return false;
    if (highlight < 1 || highlight > count)
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
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Artifice");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 opens, 8/2 moves, a-o jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Open");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Open");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-o", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    for (int i = 1; i <= count; i++)
    {
        char key[APP_UI_KEY_MAX];
        byte attr = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

        strnfmt(key, sizeof(key), "%c", (char)('a' + i - 1));
        if (!app_ui_panel_add_row(panel, (s16b)i, attr, true, i == highlight,
                key, smith_ui_artefact_root_label(i), ""))
        {
            return false;
        }
    }

    return smith_ui_artefact_root_add_selected_detail(panel, highlight);
}
