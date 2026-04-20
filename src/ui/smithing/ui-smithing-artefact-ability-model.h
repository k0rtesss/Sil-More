/* File: ui-smithing-artefact-ability-model.h */
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

static void smith_ui_artefact_ability_build_state(
    smith_ui_artefact_ability_menu_state* state, int skill)
{
    int i;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    prepare_artefact();

    for (i = 0; i < z_info->b_max && state->count < (int)N_ELEMENTS(state->ability_nums); i++)
    {
        ability_type* b_ptr = &b_info[i];

        if (!b_ptr->name || b_ptr->skilltype != skill || !ability_can_be_smithed(b_ptr))
            continue;

        state->ability_nums[state->count] = b_ptr->abilitynum;
        state->labels[state->count] = b_name + b_ptr->name;
        if (has_ability(smith2_a_ptr, skill, b_ptr->abilitynum))
        {
            state->present[state->count] = true;
            state->valid[state->count] = true;
            state->affordable[state->count] = true;
        }
        else if (applicable_ability(b_ptr, smith_o_ptr))
        {
            state->valid[state->count] = true;
            add_artefact_ability(skill, b_ptr->abilitynum);
            if (has_ability(smith_a_ptr, skill, b_ptr->abilitynum))
                state->affordable[state->count] = affordable(smith_o_ptr);
            else
                state->valid[state->count] = false;
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

static bool smith_ui_artefact_ability_add_selected_detail(app_ui_panel* panel,
    const smith_ui_artefact_ability_menu_state* state, int skill, int highlight)
{
    if (!panel || !state || state->count <= 0)
        return false;

    if (highlight < 1 || highlight > state->count)
        highlight = 1;

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        state->labels[highlight - 1]);
    if (state->present[highlight - 1])
    {
        remove_artefact_ability(skill, state->ability_nums[highlight - 1]);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Selecting this entry removes the ability from the artefact."))
        {
            return false;
        }
    }
    else if (state->valid[highlight - 1])
    {
        add_artefact_ability(skill, state->ability_nums[highlight - 1]);
        if (!smith_ui_panel_try_add_detail_line(panel,
                state->affordable[highlight - 1] ? TERM_SLATE : TERM_L_DARK,
                state->affordable[highlight - 1]
                    ? "Selecting this entry adds the ability to the artefact."
                    : "That ability exceeds your current resources."))
        {
            return false;
        }
    }
    else
    {
        prepare_artefact();
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "That ability cannot be added to this artefact."))
        {
            return false;
        }
    }

    return smith_ui_main_menu_add_current_item_detail(panel);
}
