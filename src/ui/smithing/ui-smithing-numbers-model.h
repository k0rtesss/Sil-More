/* File: ui-smithing-numbers-model.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

typedef struct smith_ui_numbers_menu_state
{
    bool valid[SMT_NUM_MENU_MAX];
    bool can_afford[SMT_NUM_MENU_MAX];
    byte row_attr[SMT_NUM_MENU_MAX];
    bool alloy_applicable;
    bool has_alloy_mastery;
    int alloy_weight;
    int mithril_have;
    int star_iron_have;
} smith_ui_numbers_menu_state;

static cptr smith_ui_alloy_name(smith_alloy_type type)
{
    switch (type)
    {
    case SMITH_ALLOY_MITHRIL:
        return "Mithril";
    case SMITH_ALLOY_STAR_IRON:
        return "Star Iron";
    default:
        return "None";
    }
}

static void smith_ui_format_weight_lb(char* buf, size_t buf_size, int weight)
{
    if (!buf || !buf_size)
        return;

    strnfmt(buf, buf_size, "%d.%d lb", weight / 10, ABS(weight % 10));
}

static void smith_ui_format_protection_value(char* buf, size_t buf_size,
    const object_type* o_ptr)
{
    if (!buf || !buf_size)
        return;

    if (!o_ptr)
    {
        SDL_strlcpy(buf, "Protection: n/a", buf_size);
        return;
    }

    if (smithing_variable_protection_dice(o_ptr))
    {
        strnfmt(buf, buf_size, "Protection: %dd%d", o_ptr->pd, o_ptr->ps);
        return;
    }

    strnfmt(buf, buf_size, "Protection: %d", o_ptr->ps);
}

static cptr smith_ui_numbers_action_label(int choice)
{
    switch (choice)
    {
    case SMT_NUM_MENU_I_ATT:
        return "Increase attack bonus";
    case SMT_NUM_MENU_D_ATT:
        return "Decrease attack bonus";
    case SMT_NUM_MENU_I_DS:
        return "Increase damage sides";
    case SMT_NUM_MENU_D_DS:
        return "Decrease damage sides";
    case SMT_NUM_MENU_I_EVN:
        return "Increase evasion bonus";
    case SMT_NUM_MENU_D_EVN:
        return "Decrease evasion bonus";
    case SMT_NUM_MENU_I_PS:
        return "Increase protection";
    case SMT_NUM_MENU_D_PS:
        return "Decrease protection";
    case SMT_NUM_MENU_I_WGT:
        return "Increase weight";
    case SMT_NUM_MENU_D_WGT:
        return "Decrease weight";
    case SMT_NUM_MENU_ALLOY_CYCLE:
        return "Cycle alloy";
    case SMT_NUM_MENU_ALLOY_CLEAR:
        return "Remove alloy bonus";
    case SMT_NUM_MENU_EDIT_BONUSES:
        return "Adjust special bonuses";
    default:
        return "Numbers";
    }
}

static void smith_ui_numbers_build_state(smith_ui_numbers_menu_state* state)
{
    int i;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));

    state->valid[SMT_NUM_MENU_I_ATT - 1]
        = att_valid() && (smith_o_ptr->att < att_max());
    state->valid[SMT_NUM_MENU_D_ATT - 1]
        = att_valid() && (smith_o_ptr->att > att_min());
    state->valid[SMT_NUM_MENU_I_DS - 1]
        = ds_valid() && (smith_o_ptr->ds < ds_max());
    state->valid[SMT_NUM_MENU_D_DS - 1]
        = ds_valid() && (smith_o_ptr->ds > ds_min());
    state->valid[SMT_NUM_MENU_I_EVN - 1]
        = evn_valid() && (smith_o_ptr->evn < evn_max());
    state->valid[SMT_NUM_MENU_D_EVN - 1]
        = evn_valid() && (smith_o_ptr->evn > evn_min());
    state->valid[SMT_NUM_MENU_I_PS - 1]
        = ps_valid() && smithing_can_increase_protection(smith_o_ptr);
    state->valid[SMT_NUM_MENU_D_PS - 1]
        = ps_valid() && smithing_can_decrease_protection(smith_o_ptr);
    state->valid[SMT_NUM_MENU_I_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight + 5) <= wgt_max());
    state->valid[SMT_NUM_MENU_D_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight - 5) >= wgt_min());
    {
        u32b f1, f2, f3;

        object_flags(smith_o_ptr, &f1, &f2, &f3);
        state->valid[SMT_NUM_MENU_EDIT_BONUSES - 1]
            = (f1 & (TR1_STR | TR1_NEG_STR | TR1_DEX | TR1_NEG_DEX | TR1_CON
                     | TR1_NEG_CON | TR1_GRA | TR1_NEG_GRA | TR1_MEL
                     | TR1_ARC | TR1_STL | TR1_PER | TR1_WIL | TR1_SMT
                     | TR1_SNG | TR1_DAMAGE_SIDES | TR1_TUNNEL))
            != 0;
    }

    state->alloy_applicable = smith_alloy_applicable(smith_o_ptr);
    state->has_alloy_mastery = p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY];
    state->alloy_weight = state->alloy_applicable
        ? smith_alloy_weight_required(smith_o_ptr)
        : 0;
    state->mithril_have = mithril_carried();
    state->star_iron_have = star_iron_carried();
    state->valid[SMT_NUM_MENU_ALLOY_CYCLE - 1]
        = state->alloy_applicable && state->has_alloy_mastery;
    state->valid[SMT_NUM_MENU_ALLOY_CLEAR - 1]
        = (smith_alloy.type != SMITH_ALLOY_NONE);

    object_copy(smith3_o_ptr, smith_o_ptr);
    smith3_alloy = smith_alloy;

    for (i = 0; i < SMT_NUM_MENU_MAX; i++)
    {
        if (i == SMT_NUM_MENU_ALLOY_CYCLE - 1)
        {
            bool has_any_metal = (state->mithril_have >= state->alloy_weight)
                || (state->star_iron_have >= state->alloy_weight);

            state->can_afford[i] = has_any_metal;
            state->row_attr[i] = state->valid[i]
                ? (has_any_metal ? TERM_WHITE : TERM_SLATE)
                : TERM_L_DARK;
            continue;
        }

        if ((i == SMT_NUM_MENU_ALLOY_CLEAR - 1)
            || (i == SMT_NUM_MENU_EDIT_BONUSES - 1))
        {
            state->can_afford[i] = state->valid[i];
            state->row_attr[i] = state->valid[i] ? TERM_WHITE : TERM_L_DARK;
            continue;
        }

        if (state->valid[i])
        {
            modify_numbers(i + 1);
            state->can_afford[i] = affordable(smith_o_ptr);
        }

        object_copy(smith_o_ptr, smith3_o_ptr);
        smith_alloy = smith3_alloy;
        state->row_attr[i] = state->valid[i]
            ? (state->can_afford[i] ? TERM_WHITE : TERM_SLATE)
            : TERM_L_DARK;
    }

    object_copy(smith_o_ptr, smith3_o_ptr);
    smith_alloy = smith3_alloy;
}
