/* File: ui-smithing-bonus-model.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

typedef struct smith_ui_bonus_menu_state
{
    int action_count;
    smith_bonus_action actions[26];
    bool valid[26];
    bool can_afford[26];
    byte row_attr[26];
} smith_ui_bonus_menu_state;

static const char* smith_bonus_stat_name(int stat)
{
    switch (stat)
    {
    case A_STR:
        return "Strength";
    case A_DEX:
        return "Dexterity";
    case A_CON:
        return "Constitution";
    case A_GRA:
        return "Grace";
    default:
        return "Unknown";
    }
}

static const char* smith_bonus_special_name(int special)
{
    switch (special)
    {
    case SMT_BONUS_SPECIAL_DAMAGE_SIDES:
        return "Damage bonus";
    case SMT_BONUS_SPECIAL_TUNNEL:
        return "Tunneling";
    default:
        return "Unknown";
    }
}
static cptr smith_ui_bonus_action_name(const smith_bonus_action* action)
{
    if (!action)
        return "Bonus";

    if (action->entry.kind == SMT_BONUS_ENTRY_STAT)
        return smith_bonus_stat_name(action->entry.index);
    if (action->entry.kind == SMT_BONUS_ENTRY_SKILL)
        return skill_names_full[action->entry.index];

    return smith_bonus_special_name(action->entry.index);
}

static int smith_ui_bonus_action_current_value(const smith_bonus_action* action)
{
    if (!action)
        return 0;

    if (action->entry.kind == SMT_BONUS_ENTRY_STAT)
        return smith_o_ptr->stat_bonus[action->entry.index];
    if (action->entry.kind == SMT_BONUS_ENTRY_SKILL)
        return smith_o_ptr->skill_bonus[action->entry.index];

    return smith_o_ptr->pval;
}

static void smith_ui_bonus_action_build_label(const smith_bonus_action* action,
    char* label, size_t label_size, char* meta, size_t meta_size)
{
    cptr verb;
    cptr name;

    if (label && label_size)
        label[0] = '\0';
    if (meta && meta_size)
        meta[0] = '\0';
    if (!action)
        return;

    verb = (action->delta > 0) ? "Increase" : "Decrease";
    name = smith_ui_bonus_action_name(action);
    if (label && label_size)
        strnfmt(label, label_size, "%s %s", verb, name);
    if (meta && meta_size)
    {
        strnfmt(meta, meta_size, "now %+d",
            smith_ui_bonus_action_current_value(action));
    }
}

static void smith_ui_bonus_build_state(smith_ui_bonus_menu_state* state)
{
    object_type snapshot;
    smith_alloy_state alloy_snapshot = smith_alloy;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    state->action_count = smith_collect_bonus_actions(state->actions,
        (int)N_ELEMENTS(state->actions));
    object_copy(&snapshot, smith_o_ptr);

    for (int i = 0; i < state->action_count; i++)
    {
        if (smith_adjust_bonus_entry(&state->actions[i].entry,
                state->actions[i].delta))
        {
            state->valid[i] = true;
            state->can_afford[i] = affordable(smith_o_ptr);
        }

        object_copy(smith_o_ptr, &snapshot);
        smith_alloy = alloy_snapshot;
        state->row_attr[i] = state->valid[i]
            ? (state->can_afford[i] ? TERM_WHITE : TERM_SLATE)
            : TERM_L_DARK;
    }

    object_copy(smith_o_ptr, &snapshot);
    smith_alloy = alloy_snapshot;
}

static bool smith_ui_bonus_add_selected_detail(app_ui_panel* panel,
    const smith_ui_bonus_menu_state* state, int highlight)
{
    char buf[APP_UI_TEXT_MAX];
    char title[APP_UI_LABEL_MAX];

    if (!panel || !state)
        return false;

    if (state->action_count <= 0)
    {
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Special Bonuses");
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "No editable special bonuses are available for this item."))
        {
            return false;
        }
        return smith_ui_main_menu_add_current_item_detail(panel);
    }

    if (highlight < 1 || highlight > state->action_count)
        highlight = 1;

    smith_ui_bonus_action_build_label(&state->actions[highlight - 1], title,
        sizeof(title), NULL, 0);
    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, title);

    strnfmt(buf, sizeof(buf), "Current bonus: %+d",
        smith_ui_bonus_action_current_value(&state->actions[highlight - 1]));
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Pval limits, ego floors, and smithing affordability still apply."))
    {
        return false;
    }
    if (!state->valid[highlight - 1]
        && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "This adjustment is unavailable for the current item."))
    {
        return false;
    }
    if (state->valid[highlight - 1] && !state->can_afford[highlight - 1]
        && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "Applying this change would exceed your current resources."))
    {
        return false;
    }

    return smith_ui_main_menu_add_current_item_detail(panel);
}
