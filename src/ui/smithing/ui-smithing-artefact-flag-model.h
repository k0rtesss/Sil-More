/* File: ui-smithing-artefact-flag-model.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static void smith_ui_artefact_flag_build_state(
    smith_ui_artefact_flag_menu_state* state, int category)
{
    int i;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    prepare_artefact();

    for (i = 0; smithing_flag_types[i].flag != 0
        && state->count < (int)N_ELEMENTS(state->flags); i++)
    {
        if (category != smithing_flag_types[i].category)
            continue;
        if ((smithing_flag_types[i].flagset == 1)
            && (smithing_flag_types[i].flag == TR1_SHARPNESS2)
            && !(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
        {
            continue;
        }

        state->flags[state->count] = smithing_flag_types[i].flag;
        state->flagsets[state->count] = smithing_flag_types[i].flagset;
        state->labels[state->count] = smithing_flag_types[i].desc;
        if (((state->flagsets[state->count] == 1)
                && (smith2_a_ptr->flags1 & state->flags[state->count]))
            || ((state->flagsets[state->count] == 2)
                && (smith2_a_ptr->flags2 & state->flags[state->count]))
            || ((state->flagsets[state->count] == 3)
                && (smith2_a_ptr->flags3 & state->flags[state->count]))
            || ((state->flagsets[state->count] == 4)
                && (smith2_a_ptr->flags4 & state->flags[state->count])))
        {
            state->present[state->count] = true;
            state->valid[state->count] = true;
            state->affordable[state->count] = true;
        }
        else if (applicable_flag(state->flags[state->count],
                state->flagsets[state->count], smith_o_ptr))
        {
            state->valid[state->count] = true;
            add_artefact_flag(state->flags[state->count],
                state->flagsets[state->count]);
            state->affordable[state->count] = affordable(smith_o_ptr);
        }

        state->row_attr[state->count] = state->present[state->count]
            ? TERM_BLUE
            : (state->valid[state->count]
                ? (state->affordable[state->count] ? TERM_WHITE : TERM_SLATE)
                : TERM_L_DARK);
        state->count++;
    }

    prepare_artefact();
}

static bool smith_ui_artefact_flag_add_selected_detail(app_ui_panel* panel,
    const smith_ui_artefact_flag_menu_state* state, int highlight)
{
    if (!panel || !state || state->count <= 0)
        return false;

    if (highlight < 1 || highlight > state->count)
        highlight = 1;

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        state->labels[highlight - 1]);
    if (state->present[highlight - 1])
    {
        remove_artefact_flag(state->flags[highlight - 1],
            state->flagsets[highlight - 1]);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Selecting this entry removes the flag from the artefact."))
        {
            return false;
        }
    }
    else if (state->valid[highlight - 1])
    {
        add_artefact_flag(state->flags[highlight - 1],
            state->flagsets[highlight - 1]);
        if (!smith_ui_panel_try_add_detail_line(panel,
                state->affordable[highlight - 1] ? TERM_SLATE : TERM_L_DARK,
                state->affordable[highlight - 1]
                    ? "Selecting this entry adds the flag to the artefact."
                    : "That flag exceeds your current resources."))
        {
            return false;
        }
    }
    else
    {
        prepare_artefact();
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "That flag cannot be added to this artefact."))
        {
            return false;
        }
    }

    return smith_ui_main_menu_add_current_item_detail(panel);
}
