/* File: cmd-ui-knowledge-supplies.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "platform-input.h"
#include "supplies.h"
#include "cmd-ui-knowledge.h"

static cptr supply_group_text[SUPPLY_GROUP_MAX + 1] = {
    "Herbs",
    "Food",
    "Potions",
    "Gems",
    "Lights",
    NULL
};

static bool supplies_menu_use_entry(supply_list_entry* entry)
{
    object_type* o_ptr;

    if (!entry || entry->supply_idx < 0)
        return false;

    o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    supplies_begin_action(entry->supply_idx);

    switch (o_ptr->tval)
    {
    case TV_FOOD:
        do_cmd_eat_food(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_POTION:
        do_cmd_quaff_potion(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_STAFF:
        do_cmd_activate_staff(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_GEM:
        do_cmd_use_gem(o_ptr, SUPPLIES_INDEX);
        break;
    default:
        supplies_end_action();
        bell("Cannot use that item here!");
        msg_print("Cannot use that item here.");
        return false;
    }

    supplies_end_action();
    return true;
}

static bool supplies_menu_drop_entry(supply_list_entry* entry)
{
    object_type* o_ptr;
    int max_amt;
    int actual_amt;
    bool dropped;

    if (!entry || entry->supply_idx < 0)
        return false;

    o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    max_amt = o_ptr->number;
    if (max_amt <= 0)
        return false;

    actual_amt = get_quantity(NULL, max_amt);
    if (actual_amt <= 0)
        return false;

    supplies_begin_action(entry->supply_idx);
    dropped = supplies_drop_amount(entry->supply_idx, actual_amt);
    supplies_end_action();

    if (dropped)
        handle_stuff();

    return dropped;
}

static bool supply_kind_matches(int group, int tval, int sval)
{
    return supplies_group_matches_kind(group, tval, sval);
}

static bool supply_item_matches(int group, const object_type* o_ptr)
{
    return supplies_group_matches_object(group, o_ptr);
}

static void append_supply_item_weight(char* buf, size_t len,
    const object_type* o_ptr, bool each)
{
    char weight_buf[32];

    if (!buf || len == 0 || !o_ptr || o_ptr->weight <= 0)
        return;

    strnfmt(weight_buf, sizeof(weight_buf), " [%d.%1d lb%s]",
        o_ptr->weight / 10, o_ptr->weight % 10, each ? " each" : "");
    SDL_strlcat(buf, weight_buf, len);
}

static int supply_group_uniform_weight(int group_idx)
{
    int weight = -1;
    int i;

    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (!k_ptr->name)
            continue;
        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (weight < 0)
            weight = k_ptr->weight;
        else if (weight != k_ptr->weight)
            return -1;
    }

    return weight;
}

static void describe_supply_group_status(int group_idx, char* buf, size_t len)
{
    int weight;

    if (!buf || len == 0)
        return;

    buf[0] = '\0';

    switch (group_idx)
    {
    case SUPPLY_GROUP_HERBS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
        {
            strnfmt(buf, len, "All herbs weigh %d.%1d lb each.", weight / 10,
                weight % 10);
        }
        break;

    case SUPPLY_GROUP_FOOD:
        SDL_strlcpy(buf, "Food weight varies; each row shows per-item weight.",
            len);
        break;

    case SUPPLY_GROUP_POTIONS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
        {
            strnfmt(buf, len, "All potions weigh %d.%1d lb each.", weight / 10,
                weight % 10);
        }
        break;

    case SUPPLY_GROUP_GEMS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
        {
            strnfmt(buf, len, "All gems weigh %d.%1d lb each.", weight / 10,
                weight % 10);
        }
        break;

    case SUPPLY_GROUP_LIGHTS:
        SDL_strlcpy(buf,
            "Each light row shows item weight; light total above includes oil.",
            len);
        break;
    }
}

static void compute_supply_group_totals(int totals[SUPPLY_GROUP_MAX])
{
    int i;

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
        totals[i] = 0;

    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        if (supply_kind_matches(SUPPLY_GROUP_HERBS, o_ptr->tval, o_ptr->sval))
            totals[SUPPLY_GROUP_HERBS] += o_ptr->number;
        else if (supply_kind_matches(SUPPLY_GROUP_FOOD, o_ptr->tval, o_ptr->sval))
            totals[SUPPLY_GROUP_FOOD] += o_ptr->number;
        else if (o_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += o_ptr->number;
        else if (o_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += o_ptr->number;
    }

    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);

        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if (supply_kind_matches(SUPPLY_GROUP_HERBS, s_ptr->tval, s_ptr->sval))
            totals[SUPPLY_GROUP_HERBS] += s_ptr->number;
        else if (supply_kind_matches(SUPPLY_GROUP_FOOD, s_ptr->tval, s_ptr->sval))
            totals[SUPPLY_GROUP_FOOD] += s_ptr->number;
        else if (s_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += s_ptr->number;
        else if (s_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += s_ptr->number;
        else if (supplies_is_light_object(s_ptr))
            totals[SUPPLY_GROUP_LIGHTS] += s_ptr->number;
    }

    {
        object_type* light_ptr = &inventory[INVEN_LITE];

        if (supplies_is_light_object(light_ptr))
            totals[SUPPLY_GROUP_LIGHTS] += MAX(light_ptr->number, 1);
    }
}

static bool supply_kind_is_known(const object_kind* k_ptr)
{
    if (!k_ptr)
        return false;

    if (cheat_know || p_ptr->wizard)
        return true;

    return k_ptr->aware || k_ptr->everseen || k_ptr->tried;
}

static int collect_supply_entries(int group_idx, supply_list_entry entries[])
{
    int count = 0;
    int capacity = z_info->k_max;
    int i;

    if (!entries)
        return 0;

    memset(entries, 0, sizeof(supply_list_entry) * capacity);

    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        int j;
        int value;

        if (!o_ptr->k_idx)
            continue;

        if (!supply_item_matches(group_idx, o_ptr))
            continue;

        value = o_ptr->number;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == o_ptr->k_idx)
            {
                entries[j].total += value;
                if (entries[j].item_idx < 0)
                    entries[j].item_idx = i;
                break;
            }
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = o_ptr->k_idx;
            entries[count].item_idx = i;
            entries[count].total = value;
            entries[count].supply_idx = -1;
            count++;
        }
    }

    if (group_idx == SUPPLY_GROUP_LIGHTS)
    {
        object_type* light_ptr = &inventory[INVEN_LITE];
        int j;

        if (light_ptr->k_idx && supply_item_matches(group_idx, light_ptr))
        {
            int value = MAX(light_ptr->number, 1);

            for (j = 0; j < count; j++)
            {
                if (entries[j].k_idx == light_ptr->k_idx)
                {
                    entries[j].total += value;
                    if (entries[j].item_idx < 0)
                        entries[j].item_idx = INVEN_LITE;
                    break;
                }
            }

            if ((j == count) && (count < capacity))
            {
                entries[count].k_idx = light_ptr->k_idx;
                entries[count].item_idx = INVEN_LITE;
                entries[count].total = value;
                entries[count].supply_idx = -1;
                count++;
            }
        }
    }

    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        int j;
        int value;

        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if (!supply_item_matches(group_idx, s_ptr))
            continue;

        value = s_ptr->number;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == s_ptr->k_idx)
            {
                entries[j].total += value;
                if (entries[j].item_idx < 0)
                    entries[j].item_idx = SUPPLIES_INDEX;
                entries[j].supply_idx = i;
                break;
            }
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = s_ptr->k_idx;
            entries[count].item_idx = SUPPLIES_INDEX;
            entries[count].total = value;
            entries[count].supply_idx = i;
            count++;
        }
    }

    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        int j;

        if (!k_ptr->name)
            continue;

        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (!supply_kind_is_known(k_ptr))
            continue;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == i)
                break;
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = i;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            count++;
        }
    }

    if (count < capacity)
    {
        entries[count].k_idx = -1;
        entries[count].item_idx = -1;
        entries[count].total = 0;
        entries[count].supply_idx = -1;
    }

    return count;
}

static byte get_supply_item_color(int k_idx, bool aware)
{
    object_kind* k_ptr;

    if (k_idx < 0 || k_idx >= z_info->k_max)
        return TERM_WHITE;

    k_ptr = &k_info[k_idx];

    if (!aware)
        return TERM_SLATE;

    switch (k_ptr->tval)
    {
    case TV_FOOD:
        switch (k_ptr->sval)
        {
        case SV_FOOD_RAGE:
            return TERM_RED;
        case SV_FOOD_SUSTENANCE:
            return TERM_GREEN;
        case SV_FOOD_TERROR:
            return TERM_VIOLET;
        case SV_FOOD_HEALING:
            return TERM_L_GREEN;
        case SV_FOOD_RESTORATION:
            return TERM_BLUE;
        case SV_FOOD_HUNGER:
            return TERM_UMBER;
        case SV_FOOD_VISIONS:
            return TERM_L_UMBER;
        case SV_FOOD_ENTRANCEMENT:
            return TERM_VIOLET;
        case SV_FOOD_WEAKNESS:
            return TERM_SLATE;
        case SV_FOOD_SICKNESS:
            return TERM_L_DARK;
        default:
            return TERM_WHITE;
        }

    case TV_POTION:
        switch (k_ptr->sval)
        {
        case SV_POTION_MIRUVOR:
            return TERM_WHITE;
        case SV_POTION_ORCISH_LIQUOR:
            return TERM_UMBER;
        case SV_POTION_ESGALDUIN:
            return TERM_VIOLET;
        case SV_POTION_CLARITY:
            return TERM_L_UMBER;
        case SV_POTION_HEALING:
            return TERM_L_GREEN;
        case SV_POTION_VOICE:
            return TERM_L_BLUE;
        case SV_POTION_true_SIGHT:
            return TERM_BLUE;
        case SV_POTION_ANTIDOTE:
            return TERM_GREEN;
        case SV_POTION_QUICKNESS:
            return TERM_ORANGE;
        case SV_POTION_ELEM_RESISTANCE:
            return TERM_L_BLUE;
        case SV_POTION_STR:
            return TERM_RED;
        case SV_POTION_DEX:
            return TERM_GREEN;
        case SV_POTION_CON:
            return TERM_L_RED;
        case SV_POTION_GRA:
            return TERM_BLUE;
        case SV_POTION_SLOWNESS:
        case SV_POTION_CONFUSION:
        case SV_POTION_DEC_DEX:
        case SV_POTION_DEC_GRA:
            return TERM_SLATE;
        case SV_POTION_POISON:
        case SV_POTION_BLINDNESS:
            return TERM_L_DARK;
        default:
            return TERM_WHITE;
        }

    case TV_GEM:
        switch (k_ptr->sval)
        {
        case SV_GEM_FREEDOM:
            return TERM_WHITE;
        case SV_GEM_LIGHT:
            return TERM_ORANGE;
        case SV_GEM_SANCTITY:
            return TERM_L_UMBER;
        case SV_GEM_UNDERSTANDING:
            return TERM_BLUE;
        case SV_GEM_REVELATIONS:
            return TERM_L_BLUE;
        case SV_GEM_TREASURES:
            return TERM_ORANGE;
        case SV_GEM_FOES:
            return TERM_RED;
        case SV_GEM_SELF_KNOWLEDGE:
            return TERM_GREEN;
        case SV_GEM_WARDING:
            return TERM_VIOLET;
        case SV_GEM_RECHARGING:
            return TERM_BLUE;
        case SV_GEM_SHADOWS:
            return TERM_L_DARK;
        default:
            return TERM_WHITE;
        }

    case TV_LIGHT:
        switch (k_ptr->sval)
        {
        case SV_LIGHT_TORCH:
            return TERM_YELLOW;
        case SV_LIGHT_MALLORN:
            return TERM_L_GREEN;
        case SV_LIGHT_LANTERN:
            return TERM_UMBER;
        case SV_LIGHT_LESSER_JEWEL:
            return TERM_L_BLUE;
        case SV_LIGHT_FEANORIAN:
            return TERM_WHITE;
        default:
            return TERM_WHITE;
        }

    default:
        return TERM_WHITE;
    }
}

static void knowledge_scene_add_supply_footer_actions(app_ui_panel* panel)
{
    char recall_key[APP_UI_KEY_MAX];
    char use_key[APP_UI_KEY_MAX];
    char confirm_key[APP_UI_KEY_MAX];
    char drop_key[APP_UI_KEY_MAX];
    char back_key[APP_UI_KEY_MAX];
    char use_confirm_key[APP_UI_KEY_MAX];

    if (!panel)
        return;

    if (steamdeck_controls_active())
    {
        controller_prompt_label(steamdeck_info_key(), "RS", recall_key,
            sizeof(recall_key));
        controller_prompt_label(steamdeck_alt_action_key(), "X", use_key,
            sizeof(use_key));
        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_key,
            sizeof(confirm_key));
        controller_prompt_label('d', "d", drop_key, sizeof(drop_key));
        controller_prompt_label(steamdeck_back_key(), "B", back_key,
            sizeof(back_key));
        strnfmt(use_confirm_key, sizeof(use_confirm_key), "%s/%s", use_key,
            confirm_key);

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "D-pad", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            recall_key, "Recall");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            use_confirm_key, "Use");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            drop_key, "Drop");
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            back_key, "Back");
        return;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "4/6", "Group");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "r", "Recall");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "u/Space", "Use");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "d", "Drop");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
        "Esc", "Back");
}

static void knowledge_scene_add_supply_group_detail_lines(app_ui_panel* panel,
    int grp_idx[], int grp_cnt, int grp_cur, int grp_top, int group_totals[])
{
    int start;
    int i;

    if (!panel || !grp_idx || !group_totals || grp_cnt <= 0)
        return;

    panel->flags |= APP_UI_PANEL_FLAG_DETAIL_LEADING;
    app_ui_panel_set_detail_title(panel, TERM_SLATE, "Group");

    start = grp_top;
    if (start < 0)
        start = 0;
    if (start >= grp_cnt)
        start = grp_cnt - 1;
    if ((start + (int)APP_UI_DETAIL_LINE_MAX) > grp_cnt)
        start = MAX(0, grp_cnt - (int)APP_UI_DETAIL_LINE_MAX);

    for (i = start;
        i < grp_cnt && panel->detail_line_count < APP_UI_DETAIL_LINE_MAX; i++)
    {
        int grp = grp_idx[i];
        byte attr;
        char buf[APP_UI_TEXT_MAX];

        switch (grp)
        {
        case SUPPLY_GROUP_HERBS:
            attr = TERM_GREEN;
            break;
        case SUPPLY_GROUP_FOOD:
            attr = TERM_L_GREEN;
            break;
        case SUPPLY_GROUP_POTIONS:
            attr = TERM_VIOLET;
            break;
        case SUPPLY_GROUP_GEMS:
            attr = TERM_BLUE;
            break;
        case SUPPLY_GROUP_LIGHTS:
            attr = TERM_YELLOW;
            break;
        default:
            attr = TERM_WHITE;
            break;
        }

        if (i == grp_cur)
            attr = TERM_L_WHITE;
        else if (group_totals[grp] == 0)
            attr = TERM_L_DARK;

        strnfmt(buf, sizeof(buf), "%-8s %3d", supply_group_text[grp],
            group_totals[grp]);
        (void)app_ui_panel_add_detail_line(panel, attr, buf);
    }
}

static void knowledge_scene_append_supply_rows(app_ui_panel* panel,
    supply_list_entry entries[], int entry_cnt, int entry_cur, bool focus_rows,
    int current_group)
{
    int i;

    if (!panel || !entries || entry_cnt <= 0)
        return;

    for (i = 0; i < entry_cnt; i++)
    {
        supply_list_entry* entry = &entries[i];
        object_type* o_ptr;
        object_type fake;
        object_kind* k_ptr;
        bool aware;
        byte base_attr;
        byte cursor_attr;
        byte attr;
        char name[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];

        if (entry->k_idx < 0 || entry->k_idx >= z_info->k_max)
            continue;

        k_ptr = &k_info[entry->k_idx];
        aware = k_ptr->aware;

        if (entry->total == 0)
        {
            base_attr = TERM_L_DARK;
            cursor_attr = TERM_SLATE;
        }
        else
        {
            base_attr = get_supply_item_color(entry->k_idx, aware);
            cursor_attr = aware ? TERM_L_WHITE : TERM_WHITE;
        }

        attr = (focus_rows && i == entry_cur) ? cursor_attr : base_attr;

        if ((entry->item_idx >= 0) && (entry->item_idx < INVEN_PACK))
        {
            o_ptr = &inventory[entry->item_idx];
        }
        else if (entry->item_idx == INVEN_LITE)
        {
            o_ptr = &inventory[INVEN_LITE];
        }
        else
        {
            object_wipe(&fake);
            object_prep(&fake, entry->k_idx);
            if (aware)
                fake.ident |= IDENT_KNOWN;
            fake.number = (entry->total > 0) ? entry->total : 1;
            o_ptr = &fake;
        }

        object_desc(name, sizeof(name), o_ptr, true, 3);
        if (current_group == SUPPLY_GROUP_FOOD)
            append_supply_item_weight(name, sizeof(name), o_ptr,
                entry->total > 1);
        else if (current_group == SUPPLY_GROUP_LIGHTS)
            append_supply_item_weight(name, sizeof(name), o_ptr, false);
        if ((current_group == SUPPLY_GROUP_LIGHTS)
            && (entry->item_idx == INVEN_LITE))
        {
            SDL_strlcat(name, " [equipped]", sizeof(name));
        }
        strnfmt(meta, sizeof(meta), "x%-3d", entry->total);

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, attr, attr,
                object_attr(o_ptr), object_char(o_ptr), true, false, "", name,
                meta))
        {
            break;
        }
    }
}

static bool knowledge_build_supplies_browser_scene(app_ui_scene* scene,
    int grp_idx[], int grp_cnt, int grp_cur, int grp_top, int group_totals[],
    supply_list_entry entries[], int entry_cnt, int entry_top, int entry_cur,
    int column, cptr weight_text)
{
    app_ui_panel* panel;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_WHITE;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, TERM_L_WHITE,
        "Supplies - Herbs, Food, Potions, Gems, Lights");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Name / Qty / Sym");
    if (weight_text && weight_text[0])
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE, weight_text);
    {
        char status_buf[96];

        describe_supply_group_status(grp_idx[grp_cur], status_buf,
            sizeof(status_buf));
        if (status_buf[0])
            (void)app_ui_panel_add_body_line(panel, TERM_L_BLUE, status_buf);
    }

    knowledge_scene_add_supply_group_detail_lines(panel, grp_idx, grp_cnt,
        grp_cur, grp_top, group_totals);
    knowledge_scene_append_supply_rows(panel, entries, entry_cnt, entry_cur,
        column == 1, grp_idx[grp_cur]);
    app_ui_panel_set_row_offset(panel, (s16b)entry_top);
    knowledge_scene_add_supply_footer_actions(panel);

    if (column == 0 && panel->detail_line_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_DETAIL;
    }
    else if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = panel->rows[MIN(entry_cur, (int)panel->row_count - 1)].id;
    }

    return true;
}

bool do_cmd_knowledge_supplies(const supply_menu_request* request)
{
    ui_information_scene_scope info_scope;
    int grp_cnt = SUPPLY_GROUP_MAX;
    int grp_idx[SUPPLY_GROUP_MAX + 1];
    int group_totals[SUPPLY_GROUP_MAX];
    supply_list_entry* entries;
    int grp_cur = 0;
    int grp_top = 0;
    int entry_cur = 0;
    int entry_top = 0;
    int column = 0;
    bool flag = false;
    supply_menu_action forced_action = SUPPLY_MENU_ACTION_NONE;
    bool hotkey_mode = false;
    bool acted = false;
    bool refresh_after_close = false;
    int i;

    if (request)
    {
        forced_action = request->action;
        hotkey_mode = request->hotkey_mode;
        if (request->focus_group && request->group >= 0
            && request->group < SUPPLY_GROUP_MAX)
        {
            grp_cur = request->group;
        }
        if (forced_action != SUPPLY_MENU_ACTION_NONE)
            column = 1;
    }

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
        grp_idx[i] = i;
    grp_idx[grp_cnt] = -1;

    if (!knowledge_enter_information_scene_or_report(&info_scope,
            "knowledge supplies",
            "Supplies screen unavailable."))
    {
        return false;
    }

    entries = mem_alloc_array(z_info->k_max, supply_list_entry);

    while (!flag)
    {
        int entry_cnt;
        int used_weight;
        int light_item_weight;
        int light_oil_weight;
        int light_weight;
        int lamp_oil;
        int max_weight;
        char weight_buf[128];
        char ch;

        compute_supply_group_totals(group_totals);
        used_weight = supplies_limit_weight();
        light_item_weight = supplies_carried_light_item_weight();
        light_oil_weight = player_lamp_oil_weight();
        light_weight = light_item_weight + light_oil_weight;
        lamp_oil = player_lamp_oil();
        max_weight = supplies_current_weight_cap();
        strnfmt(weight_buf, sizeof(weight_buf),
            "Supply: %d.%1d/%d.%1d lb  Light: %d.%1d lb (%d.%1d items + %d.%1d oil)  Oil: %d/%d",
            used_weight / 10, used_weight % 10, max_weight / 10,
            max_weight % 10, light_weight / 10, light_weight % 10,
            light_item_weight / 10, light_item_weight % 10,
            light_oil_weight / 10, light_oil_weight % 10, lamp_oil,
            PLAYER_LAMP_OIL_MAX);

        if (grp_cur >= grp_cnt)
            grp_cur = grp_cnt - 1;
        if (grp_cur < 0)
            grp_cur = 0;

        entry_cnt = collect_supply_entries(grp_idx[grp_cur], entries);

        if (entry_cnt == 0)
        {
            entry_cur = 0;
            entry_top = 0;
            if (column)
                column = 0;
        }
        else
        {
            if (entry_cur >= entry_cnt)
                entry_cur = entry_cnt - 1;
            if (entry_cur < 0)
                entry_cur = 0;

            if (entry_cur < entry_top)
                entry_top = entry_cur;
            if (entry_cur >= entry_top + KNOWLEDGE_BROWSER_ROWS)
                entry_top = entry_cur - KNOWLEDGE_BROWSER_ROWS + 1;
            if (entry_top < 0)
                entry_top = 0;
        }

        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + KNOWLEDGE_BROWSER_ROWS)
            grp_top = grp_cur - KNOWLEDGE_BROWSER_ROWS + 1;
        if (grp_top < 0)
            grp_top = 0;

        {
            app_ui_scene scene;

            if (!knowledge_present_ui_scene_or_abort(&info_scope,
                    knowledge_build_supplies_browser_scene(&scene, grp_idx,
                        grp_cnt, grp_cur, grp_top, group_totals, entries,
                        entry_cnt, entry_top, entry_cur, column, weight_buf),
                    &scene, "knowledge supplies browser",
                    "Supplies screen unavailable."))
            {
                goto cleanup;
            }
        }

        ch = (char)ui_information_scene_wait_key();
        if (steamdeck_controls_active() && ch == steamdeck_back_key())
            ch = ESCAPE;

        if ((ch == '\r' || ch == '\n'
                || (steamdeck_controls_active()
                    && ch == steamdeck_confirm_key()))
            && column && entry_cnt)
        {
            if (forced_action == SUPPLY_MENU_ACTION_USE)
                ch = 'u';
            else if (forced_action == SUPPLY_MENU_ACTION_DROP)
                ch = 'd';
        }

        switch (ch)
        {
        case ESCAPE:
            flag = true;
            break;

        case 'R':
        case 'r':
        case 'X':
        case 'x':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];

                if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    if (!knowledge_pause_information_scene(&info_scope))
                        goto cleanup;
                    (void)player_try_identify_smithing_object_on_examine(
                        &inventory[entry->item_idx], false);
                    object_info_screen(&inventory[entry->item_idx]);
                    if (!knowledge_resume_information_scene(&info_scope))
                        goto cleanup;
                }
                else if (entry->k_idx >= 0)
                {
                    object_kind* k_ptr = &k_info[entry->k_idx];

                    if (k_ptr->aware)
                    {
                        if (!knowledge_pause_information_scene(&info_scope))
                            goto cleanup;
                        knowledge_desc_obj_fake(entry->k_idx);
                        if (!knowledge_resume_information_scene(&info_scope))
                            goto cleanup;
                    }
                    else
                    {
                        bell("You have not identified that yet.");
                        msg_print("You have not identified that yet.");
                    }
                }
            }
            break;

        case 'u':
        case 'U':
        case ' ':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool handled = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    handled = supplies_menu_use_entry(entry);
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    object_type* o_ptr = &inventory[entry->item_idx];

                    switch (o_ptr->tval)
                    {
                    case TV_FOOD:
                        do_cmd_eat_food(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_POTION:
                        do_cmd_quaff_potion(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_STAFF:
                        do_cmd_activate_staff(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_GEM:
                        do_cmd_use_gem(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    default:
                        bell("Cannot use that item here!");
                        break;
                    }

                    if (handled)
                        handle_stuff();
                }
                else
                {
                    bell("You do not have any of that item.");
                    msg_print("You do not have any of that item.");
                }

                if (handled)
                {
                    acted = true;
                    refresh_after_close = true;
                    if (hotkey_mode || forced_action == SUPPLY_MENU_ACTION_USE)
                        flag = true;
                }
            }
            break;

        case 'd':
        case 'D':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool dropped = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    dropped = supplies_menu_drop_entry(entry);
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    do_cmd_drop_item_by_index(entry->item_idx);
                    dropped = true;
                }
                else
                {
                    bell("Nothing to drop here.");
                    msg_print("Nothing to drop here.");
                }

                if (dropped)
                {
                    acted = true;
                    handle_stuff();
                    refresh_after_close = true;
                    if (hotkey_mode || forced_action == SUPPLY_MENU_ACTION_DROP)
                        flag = true;
                }
            }
            break;

        default:
            knowledge_browser_cursor_with_rows(ch, &column, &grp_cur, grp_cnt,
                &entry_cur, entry_cnt, KNOWLEDGE_BROWSER_ROWS);
            break;
        }
    }

cleanup:
    mem_free_null(entries);
    if (info_scope.active)
        ui_information_scene_leave(&info_scope);

    if (refresh_after_close)
    {
        p_ptr->redraw |= PR_MAP;
        p_ptr->window |= PW_MESSAGE;
        handle_stuff();
    }

    return acted;
}
