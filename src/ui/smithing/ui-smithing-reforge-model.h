/* File: ui-smithing-reforge-model.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static void smith_ui_reforge_build_state(smith_ui_reforge_menu_state* state,
    const object_type* source)
{
    int i;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    state->source = source;
    if (!source || !source->k_idx)
        return;

    for (i = 1; i < z_info->e_max && state->entry_count < (int)N_ELEMENTS(state->choice); i++)
    {
        if (!ego_prefix_can_apply_to_object(source, i))
            continue;
        if (!reforge_preview_build(source, i, &state->previews[state->entry_count]))
            continue;

        state->choice[state->entry_count] = i;
        state->valid[state->entry_count]
            = state->previews[state->entry_count].affordable;
        state->row_attr[state->entry_count] = state->valid[state->entry_count]
            ? TERM_WHITE
            : TERM_L_DARK;
        state->entry_count++;
    }
}

static bool smith_ui_reforge_add_selected_detail(app_ui_panel* panel,
    const smith_ui_reforge_menu_state* state, int highlight)
{
    char buf[APP_UI_TEXT_MAX];
    char prefix_label[64];
    char source_name[80];
    char result_name[80];
    object_type preview_object;

    if (!panel || !state)
        return false;

    if (state->entry_count <= 0)
    {
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Reforge");
        return smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "No legal prefixes are available for this item.");
    }

    if (highlight < 1 || highlight > state->entry_count)
        highlight = 1;

    ego_name_for_enchant_menu(state->choice[highlight - 1], prefix_label,
        sizeof(prefix_label));
    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, prefix_label);

    object_desc(source_name, sizeof(source_name), state->source, true, 0);
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, source_name))
        return false;

    object_copy(&preview_object, state->source);
    object_set_ego_prefix(&preview_object, state->choice[highlight - 1]);
    if (object_apply_ego_affix(&preview_object, state->choice[highlight - 1], true))
    {
        object_desc(result_name, sizeof(result_name), &preview_object, true, 0);
        strnfmt(buf, sizeof(buf), "Result: %s", result_name);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_WHITE, buf))
            return false;
    }

    strnfmt(buf, sizeof(buf), "Difficulty: %d (+%d raw)",
        state->previews[highlight - 1].scaled_difficulty,
        state->previews[highlight - 1].raw_delta_difficulty);
    if (!smith_ui_panel_try_add_detail_line(panel,
            state->valid[highlight - 1] ? TERM_SLATE : TERM_L_DARK, buf))
    {
        return false;
    }
    if (state->previews[highlight - 1].cost.uses > 0)
    {
        strnfmt(buf, sizeof(buf), "%d/%d uses",
            state->previews[highlight - 1].cost.uses,
            forge_uses(p_ptr->py, p_ptr->px));
        if (!smith_ui_panel_try_add_detail_line(panel,
                (forge_uses(p_ptr->py, p_ptr->px)
                    >= state->previews[highlight - 1].cost.uses)
                    ? TERM_SLATE
                    : TERM_L_DARK,
                buf))
        {
            return false;
        }
    }
    if (state->previews[highlight - 1].cost.drain > 0)
    {
        strnfmt(buf, sizeof(buf), "%d Smithing",
            state->previews[highlight - 1].cost.drain);
        if (!smith_ui_panel_try_add_detail_line(panel,
                (state->previews[highlight - 1].cost.drain
                    <= p_ptr->skill_base[S_SMT])
                    ? TERM_BLUE
                    : TERM_L_DARK,
                buf))
        {
            return false;
        }
    }
    strnfmt(buf, sizeof(buf), "%d Turns", state->previews[highlight - 1].turns);
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;

    if (!state->valid[highlight - 1]
        && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "You cannot currently afford this reforge."))
    {
        return false;
    }

    return true;
}
