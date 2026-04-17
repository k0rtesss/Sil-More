/* File: ui-smithing-enchant-model.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static void smith_ui_enchant_build_state(smith_ui_enchant_menu_state* state,
    bool selecting_prefix, int fixed_prefix, int fixed_suffix,
    const object_type* base_o_ptr)
{
    int i;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    state->selecting_prefix = selecting_prefix;
    state->fixed_prefix = fixed_prefix;
    state->fixed_suffix = fixed_suffix;
    state->base_o_ptr = base_o_ptr;

    state->choice[state->entry_count] = 0;
    state->valid[state->entry_count] = true;
    state->row_attr[state->entry_count] = TERM_WHITE;
    state->entry_count++;

    if (selecting_prefix && ego_forbids_prefix_combo(fixed_suffix))
        return;

    for (i = 1; i < z_info->e_max && state->entry_count < (int)N_ELEMENTS(state->choice); i++)
    {
        if (smith_ego_can_apply_to_object(base_o_ptr, i, fixed_prefix,
                fixed_suffix, selecting_prefix))
        {
            if (selecting_prefix)
                create_special(i, fixed_suffix);
            else
                create_special(fixed_prefix, i);

            state->choice[state->entry_count] = i;
            state->valid[state->entry_count] = affordable(smith_o_ptr);
            state->row_attr[state->entry_count] = state->valid[state->entry_count]
                ? TERM_WHITE
                : TERM_SLATE;
            state->entry_count++;
        }
    }

    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;
}

static bool smith_ui_enchant_add_selected_detail(app_ui_panel* panel,
    const smith_ui_enchant_menu_state* state, int highlight)
{
    char buf[APP_UI_TEXT_MAX];
    char label[64];
    int selected_choice;

    if (!panel || !state || state->entry_count <= 0)
        return false;

    if (highlight < 1 || highlight > state->entry_count)
        highlight = 1;

    selected_choice = state->choice[highlight - 1];
    if (selected_choice == 0)
        SDL_strlcpy(label, "(none)", sizeof(label));
    else
        ego_name_for_enchant_menu(selected_choice, label, sizeof(label));

    if (state->selecting_prefix)
        create_special(selected_choice, state->fixed_suffix);
    else
        create_special(state->fixed_prefix, selected_choice);

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, label);
    strnfmt(buf, sizeof(buf), "Selecting a %s.",
        state->selecting_prefix ? "prefix applies it before the suffix step"
                                : "suffix finalizes the enchantment");
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;

    if (state->selecting_prefix && ego_forbids_prefix_combo(state->fixed_suffix))
    {
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "This suffix forbids any prefix."))
        {
            return false;
        }
    }
    if (!state->valid[highlight - 1]
        && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "That enchantment exceeds your current resources."))
    {
        return false;
    }

    return smith_ui_main_menu_add_current_item_detail(panel);
}
