/* File: ui-smithing-numbers-detail.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

static bool smith_ui_numbers_add_selected_detail(app_ui_panel* panel,
    const smith_ui_numbers_menu_state* state, int highlight)
{
    char buf[APP_UI_TEXT_MAX];

    if (!panel || !state)
        return false;

    if (highlight < 1 || highlight > SMT_NUM_MENU_MAX)
        highlight = SMT_NUM_MENU_I_ATT;

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        smith_ui_numbers_action_label(highlight));

    if (!state->valid[highlight - 1])
    {
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "This action is unavailable for the current design."))
        {
            return false;
        }
        return smith_ui_main_menu_add_current_item_detail(panel);
    }

    switch (highlight)
    {
    case SMT_NUM_MENU_I_ATT:
    case SMT_NUM_MENU_D_ATT:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Change the item's attack bonus within its legal range."))
        {
            return false;
        }
        strnfmt(buf, sizeof(buf), "Current attack: %+d", smith_o_ptr->att);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        strnfmt(buf, sizeof(buf), "Range: %+d to %+d", att_min(), att_max());
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        break;

    case SMT_NUM_MENU_I_DS:
    case SMT_NUM_MENU_D_DS:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Change the item's damage sides within its legal range."))
        {
            return false;
        }
        strnfmt(buf, sizeof(buf), "Current damage sides: %d", smith_o_ptr->ds);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        strnfmt(buf, sizeof(buf), "Range: %d to %d", ds_min(), ds_max());
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        break;

    case SMT_NUM_MENU_I_EVN:
    case SMT_NUM_MENU_D_EVN:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Change the item's evasion bonus within its legal range."))
        {
            return false;
        }
        strnfmt(buf, sizeof(buf), "Current evasion: %+d", smith_o_ptr->evn);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        strnfmt(buf, sizeof(buf), "Range: %+d to %+d", evn_min(), evn_max());
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        break;

    case SMT_NUM_MENU_I_PS:
    case SMT_NUM_MENU_D_PS:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Step through the legal protection values for this item."))
        {
            return false;
        }
        smith_ui_format_protection_value(buf, sizeof(buf), smith_o_ptr);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Protection range follows the item's current smithing limits."))
        {
            return false;
        }
        break;

    case SMT_NUM_MENU_I_WGT:
    case SMT_NUM_MENU_D_WGT:
    {
        char min_buf[32];
        char max_buf[32];

        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Change the item's base weight in half-pound steps."))
        {
            return false;
        }
        smith_ui_format_weight_lb(buf, sizeof(buf), smith_o_ptr->weight);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        smith_ui_format_weight_lb(min_buf, sizeof(min_buf), wgt_min());
        smith_ui_format_weight_lb(max_buf, sizeof(max_buf), wgt_max());
        strnfmt(buf, sizeof(buf), "Range: %s to %s", min_buf, max_buf);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        break;
    }

    case SMT_NUM_MENU_ALLOY_CYCLE:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Cycle between no alloy, mithril, and star iron."))
        {
            return false;
        }
        strnfmt(buf, sizeof(buf), "Current alloy: %s",
            smith_ui_alloy_name(smith_alloy.type));
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        if (!state->has_alloy_mastery)
        {
            if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                    "Requires the Alloy Mastery ability."))
            {
                return false;
            }
        }
        else if (!state->alloy_applicable)
        {
            if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                    "This item cannot take an alloy bonus."))
            {
                return false;
            }
        }
        else
        {
            strnfmt(buf, sizeof(buf),
                "Needs %d.%d lb metal (mithril %d.%d, star iron %d.%d).",
                state->alloy_weight / 10, state->alloy_weight % 10,
                state->mithril_have / 10, state->mithril_have % 10,
                state->star_iron_have / 10, state->star_iron_have % 10);
            if (!smith_ui_panel_try_add_detail_line(panel,
                    state->can_afford[highlight - 1] ? TERM_SLATE : TERM_L_DARK,
                    buf))
            {
                return false;
            }
        }
        break;

    case SMT_NUM_MENU_ALLOY_CLEAR:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Remove the currently active alloy bonus."))
        {
            return false;
        }
        strnfmt(buf, sizeof(buf), "Current alloy: %s",
            smith_ui_alloy_name(smith_alloy.type));
        if (!smith_ui_panel_try_add_detail_line(panel,
                (smith_alloy.type != SMITH_ALLOY_NONE) ? TERM_SLATE
                                                       : TERM_L_DARK,
                buf))
        {
            return false;
        }
        break;

    case SMT_NUM_MENU_EDIT_BONUSES:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Adjust pval-driven stat, skill, damage, and tunneling bonuses."))
        {
            return false;
        }
        strnfmt(buf, sizeof(buf), "Current pval: %+d", smith_o_ptr->pval);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
            return false;
        break;
    }

    if (!state->can_afford[highlight - 1]
        && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "The modified design would exceed your current resources."))
    {
        return false;
    }

    return smith_ui_main_menu_add_current_item_detail(panel);
}
