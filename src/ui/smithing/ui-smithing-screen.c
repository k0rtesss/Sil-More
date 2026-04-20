/* File: ui-smithing-screen.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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

#include "angband.h"
#include "app/app-session.h"
#include "object/object-ui-select.h"
#include "platform-frame.h"
#include "ui/ui-information-scene.h"
#include "ui/smithing/ui-smithing-screen.h"
#include "ui/smithing/ui-smithing-internal.h"
#include "log/log.h"

static bool smith_ui_snapshot_active(void);
static bool smith_ui_base_item_snapshot_menu(void);
static void smith_ui_numbers_snapshot_menu(void);
static void smith_ui_bonus_snapshot_menu(void);
static void smith_ui_melt_snapshot_menu(void);
static bool smith_ui_enchant_snapshot_menu(void);
static void smith_ui_artefact_snapshot_menu(void);
static void smith_ui_artefact_flag_snapshot_menu(int category);
static void smith_ui_artefact_ability_snapshot_menu(int skill);
static int smith_ui_reforge_prefix_snapshot_menu(const object_type* source);

static char smith_ui_inkey_with_wait_reason(void)
{
    return (char)ui_information_scene_wait_key_hidden_with_wait_reason(
        APP_WAIT_REASON_LIST_SELECTION);
}

static void create_tval_menu(void)
{
    (void)smith_ui_base_item_snapshot_menu();
    enchant_then_numbers = false;
}

static void smith_bonus_menu(void)
{
    smith_ui_bonus_snapshot_menu();
}

/*
 * Displays a menu for modifying numerical bonuses and weight of an item.
 */
static void numbers_menu(void)
{
    if (object_has_ego(smith_o_ptr))
        enchant_then_numbers = true;

    smith_ui_numbers_snapshot_menu();
}

static void ego_name_for_enchant_menu(int e_idx, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;
    buf[0] = '\0';
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return;

    ego_item_type* e_ptr = &e_info[e_idx];
    const char* raw = e_name + e_ptr->name;
    if (!raw || !raw[0])
        return;

    if (ego_name_is_prefix(raw))
    {
        size_t len = strlen(raw);
        size_t copy_len = (len >= 2) ? (len - 2) : 0;
        if (copy_len >= buflen)
            copy_len = buflen - 1;
        if (copy_len > 0)
        {
            memcpy(buf, raw + 1, copy_len);
            buf[copy_len] = '\0';
        }
        return;
    }

    SDL_strlcpy(buf, raw, buflen);
}

static int reforge_prefix_menu(const object_type* source)
{
    if (!source || !source->k_idx)
        return 0;

    return smith_ui_reforge_prefix_snapshot_menu(source);
}

static bool enchant_menu(void)
{
    return smith_ui_enchant_snapshot_menu();
}

static void smith_ui_artefact_backup_current_state(void)
{
    artefact_copy(smith2_a_ptr, smith_a_ptr);
    object_copy(smith2_o_ptr, smith_o_ptr);
    smith2_alloy = smith_alloy;
}

/*
 * Allows the player to choose a new name for an artefact.
 */
static void rename_artefact(void)
{
    char tmp[20];
    char old_name[20];
    bool name_selected = false;

    // Clear the names
    tmp[0] = '\0';
    old_name[0] = '\0';

    // use old name as a default
    SDL_strlcpy(tmp, smith2_a_ptr->name, sizeof(tmp));

    // save a copy too
    SDL_strlcpy(old_name, op_ptr->full_name, sizeof(old_name));

    while (!name_selected)
    {
        if (askfor_name(tmp, sizeof(tmp)))
        {
            SDL_strlcpy(smith2_a_ptr->name, tmp, MAX_LEN_ART_NAME);
            p_ptr->redraw |= (PR_MISC);
        }
        else
        {
            SDL_strlcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
            return;
        }

        if (tmp[0] != '\0')
            name_selected = true;
        else
            SDL_strlcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
    }

    // retrieve a backup of the artefact (all the modifications were done to
    // this backup copy)
    artefact_copy(smith_a_ptr, smith2_a_ptr);
}

static void artefact_menu(void)
{
    smith_ui_artefact_snapshot_menu();
}

static void melt_menu(void)
{
    smith_ui_melt_snapshot_menu();
}

static bool smith_item_tester_hook_reforge_target(const object_type* o_ptr)
{
    return object_can_repair_damage(o_ptr) || object_can_reforge_prefix(o_ptr);
}

static bool smith_reforge_item(void)
{
    int slot = -1;
    int prefix_idx = 0;
    char old_name[80];
    char new_name[80];
    object_type smith_backup;
    object_type smith2_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smith_alloy_state alloy2_backup = smith2_alloy;

    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        msg_print("You can only reforge items at a forge.");
        return false;
    }

    if (forge_uses(p_ptr->py, p_ptr->px) <= 0)
    {
        msg_print("This forge has no resources left.");
        return false;
    }

    if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
    {
        bell("You do not know how to reforge gear.");
        return false;
    }

    item_tester_hook = smith_item_tester_hook_reforge_target;
    if (!get_item(&slot, "Reforge which item? ",
            "You have nothing to repair or reforge.", (USE_EQUIP | USE_INVEN)))
    {
        item_tester_hook = NULL;
        return false;
    }
    item_tester_hook = NULL;

    if (slot < 0)
        return false;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(&smith2_backup, smith2_o_ptr);

    if (object_can_repair_damage(&inventory[slot]))
    {
        if (!repair_damaged_item(slot))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot repair that item.");
            return false;
        }

        cave_feat[p_ptr->py][p_ptr->px] -= 1;
        dungeon_mark_map_for_redraw();

        object_desc(new_name, sizeof(new_name), &inventory[slot], true, 0);
        msg_format("You repair %s.", new_name);
    }
    else
    {
        reforge_preview_type preview;

        if (!object_can_reforge_prefix(&inventory[slot]))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot reforge that item.");
            return false;
        }

        prefix_idx = reforge_prefix_menu(&inventory[slot]);
        if (!prefix_idx)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            return false;
        }

        if (!reforge_preview_build(&inventory[slot], prefix_idx, &preview)
            || !preview.affordable)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot afford that reforge.");
            return false;
        }

        object_desc(old_name, sizeof(old_name), &inventory[slot], true, 0);
        object_set_ego_prefix(&inventory[slot], prefix_idx);
        if (!object_apply_ego_affix(&inventory[slot], prefix_idx, true))
        {
            object_set_ego_prefix(&inventory[slot], 0);
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot reforge that item.");
            return false;
        }

        pay_smithing_cost_struct(&preview.cost);
        inventory[slot].unused1 = 2;
        object_aware(&inventory[slot]);
        object_known(&inventory[slot]);
        object_desc(new_name, sizeof(new_name), &inventory[slot], true, 0);
        msg_format("You reforge %s into %s.", old_name, new_name);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }

    object_copy(smith_o_ptr, &smith_backup);
    object_copy(smith2_o_ptr, &smith2_backup);
    smith_alloy = alloy_backup;
    smith2_alloy = alloy2_backup;

    p_ptr->redraw |= PR_BASIC;
    return true;
}

typedef ui_information_scene_scope smith_ui_snapshot_scope;

static int smith_ui_nested_transition_depth = 0;

static void smith_ui_snapshot_reset_nested_transitions(void)
{
    smith_ui_nested_transition_depth = 0;
}

typedef struct smith_ui_main_menu_state
{
    bool valid[SMT_MENU_MAX];
    byte row_attr[SMT_MENU_MAX];
} smith_ui_main_menu_state;

static bool smith_ui_snapshot_active(void)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!app_session_interactions_enabled(session) || !session)
        return false;

    snapshot = app_session_snapshot(session);
    return snapshot && snapshot->scene == APP_SCENE_KIND_DUNGEON;
}

static void smith_ui_snapshot_begin_nested_transition(void)
{
    smith_ui_nested_transition_depth++;
}

static void smith_ui_snapshot_end_nested_transition(void)
{
    if (smith_ui_nested_transition_depth > 0)
        smith_ui_nested_transition_depth--;
}

static bool smith_ui_snapshot_scene_enter(smith_ui_snapshot_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope || !session || !smith_ui_snapshot_active())
        return false;

    app_session_clear_interaction(session);
    app_session_clear_dungeon_overlay_scene(session);
    return ui_information_scene_claim_input(scope, APP_WAIT_REASON_NONE);
}

static void smith_ui_snapshot_scene_close(smith_ui_snapshot_scope* scope)
{
    app_session* session = app_session_current();
    bool refresh_enabled = true;

    if (!scope || !scope->active || !session)
        return;

    app_session_clear_interaction(session);
    if (smith_ui_nested_transition_depth > 0)
        refresh_enabled = ui_information_scene_set_refresh_enabled(false);
    ui_information_scene_leave(scope);
    if (smith_ui_nested_transition_depth > 0)
        (void)ui_information_scene_set_refresh_enabled(refresh_enabled);
}

static bool smith_ui_snapshot_scene_present(smith_ui_snapshot_scope* scope,
    const app_ui_scene* scene)
{
    return ui_information_scene_present_overlay(scope, scene);
}

static bool smith_ui_panel_try_add_detail_line(app_ui_panel* panel, byte attr,
    cptr text)
{
    if (!panel || !text || !text[0])
        return false;
    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;

    return app_ui_panel_add_detail_line(panel, attr, text);
}

static cptr smith_ui_main_menu_label(int choice)
{
    switch (choice)
    {
    case SMT_MENU_CREATE:
        return "Base Item";
    case SMT_MENU_ENCHANT:
        return "Enchant";
    case SMT_MENU_ARTEFACT:
        return "Artifice";
    case SMT_MENU_NUMBERS:
        return "Numbers";
    case SMT_MENU_MELT:
        return "Melt";
    case SMT_MENU_REPAIR:
        return "Reforge";
    case SMT_MENU_ACCEPT:
        return (p_ptr->smithing_leftover > 0) ? "Resume" : "Accept";
    default:
        return "Smithing";
    }
}

static void smith_ui_main_menu_build_state(smith_ui_main_menu_state* state)
{
    byte attr;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));

    state->valid[SMT_MENU_CREATE - 1] = true;
    state->valid[SMT_MENU_ENCHANT - 1] = (!smith_o_ptr->name1)
        && (!enchant_then_numbers) && (smith_o_ptr->tval != 0)
        && (smith_o_ptr->tval != TV_HORN)
        && !((smith_o_ptr->tval == TV_DIGGING)
            && (smith_o_ptr->sval == SV_SHOVEL));
    state->valid[SMT_MENU_ARTEFACT - 1] = (!object_has_ego(smith_o_ptr))
        && (smith_o_ptr->tval != 0) && (smith_o_ptr->tval != TV_HORN)
        && (p_ptr->self_made_arts
            < z_info->art_self_made_max - z_info->art_rand_max - 2);
    state->valid[SMT_MENU_NUMBERS - 1] = (smith_o_ptr->tval != 0);
    state->valid[SMT_MENU_MELT - 1]
        = meltable_metal_items_carried() && cave_forge_bold(p_ptr->py, p_ptr->px);
    state->valid[SMT_MENU_REPAIR - 1]
        = cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0)
        && p_ptr->active_ability[S_SMT][SMT_REPAIR]
        && (find_reforge_target_item() >= 0);
    state->valid[SMT_MENU_ACCEPT - 1] = affordable(smith_o_ptr)
        && cave_forge_bold(p_ptr->py, p_ptr->px)
        && (forge_uses(p_ptr->py, p_ptr->px) > 0);

    attr = (p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
               || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
               || p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        ? TERM_WHITE
        : TERM_RED;
    state->row_attr[SMT_MENU_CREATE - 1]
        = state->valid[SMT_MENU_CREATE - 1] ? attr : TERM_L_DARK;

    attr = p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] ? TERM_WHITE : TERM_RED;
    state->row_attr[SMT_MENU_ENCHANT - 1]
        = state->valid[SMT_MENU_ENCHANT - 1] ? attr : TERM_L_DARK;

    attr = p_ptr->active_ability[S_SMT][SMT_ARTEFACT] ? TERM_WHITE : TERM_RED;
    state->row_attr[SMT_MENU_ARTEFACT - 1]
        = state->valid[SMT_MENU_ARTEFACT - 1] ? attr : TERM_L_DARK;

    state->row_attr[SMT_MENU_NUMBERS - 1]
        = state->valid[SMT_MENU_NUMBERS - 1] ? TERM_WHITE : TERM_L_DARK;
    state->row_attr[SMT_MENU_MELT - 1]
        = state->valid[SMT_MENU_MELT - 1] ? TERM_WHITE : TERM_L_DARK;

    attr = p_ptr->active_ability[S_SMT][SMT_REPAIR] ? TERM_WHITE : TERM_RED;
    state->row_attr[SMT_MENU_REPAIR - 1]
        = state->valid[SMT_MENU_REPAIR - 1] ? attr : TERM_L_DARK;

    state->row_attr[SMT_MENU_ACCEPT - 1]
        = state->valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK;
}

static bool smith_ui_main_menu_add_selected_detail(app_ui_panel* panel,
    int highlight)
{
    if (!panel)
        return false;

    switch (highlight)
    {
    case SMT_MENU_CREATE:
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Start with a new base item.");

    case SMT_MENU_ENCHANT:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Choose a special enchantment to add to the base item."))
        {
            return false;
        }
        if (smith_o_ptr->name1
            && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "(not compatible with Artifice)"))
        {
            return false;
        }
        if (enchant_then_numbers
            && (!smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                    "(Enchantment cannot be changed")
                || !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                    "after using the Numbers menu)")))
        {
            return false;
        }
        return true;

    case SMT_MENU_ARTEFACT:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Design your own artefact."))
        {
            return false;
        }
        if (object_has_ego(smith_o_ptr)
            && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "(not compatible with Enchant)"))
        {
            return false;
        }
        return true;

    case SMT_MENU_NUMBERS:
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Change the item's key numbers.");

    case SMT_MENU_MELT:
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Choose a mithril or star-iron item to melt down.");

    case SMT_MENU_REPAIR:
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Repair damaged gear or add a prefix to a found item at the forge.")
            || !smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Reforging uses 1.5x the difficulty delta."))
        {
            return false;
        }
        if (!p_ptr->active_ability[S_SMT][SMT_REPAIR]
            && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "(requires the Reforging ability)"))
        {
            return false;
        }
        if (p_ptr->active_ability[S_SMT][SMT_REPAIR]
            && (find_reforge_target_item() < 0)
            && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
                "(you carry nothing to reforge)"))
        {
            return false;
        }
        return true;

    case SMT_MENU_ACCEPT:
        if (forge_uses(p_ptr->py, p_ptr->px) > 0)
        {
            return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "Create the item you have designed. Press Escape to cancel instead.");
        }
        if (cave_forge_bold(p_ptr->py, p_ptr->px))
        {
            return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
                "This forge has no resources left, so you cannot create items. Press Escape to exit.");
        }
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "You are not at a forge and thus cannot create items. Press Escape to exit.");
    }

    return true;
}

static bool smith_ui_main_menu_add_current_item_detail(app_ui_panel* panel)
{
    char buf[APP_UI_TEXT_MAX];
    char o_desc[80];
    int dif;
    int turn_multiplier = 10;
    byte attr;
    bool can_afford = true;

    if (!panel)
        return false;

    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, " "))
        return false;
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_BLUE,
            "Current design"))
    {
        return false;
    }

    if (p_ptr->smithing_leftover > 0)
    {
        strnfmt(buf, sizeof(buf), "In progress: %d turns left",
            p_ptr->smithing_leftover);
        if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_BLUE, buf))
            return false;
    }

    if (smith_o_ptr->tval == 0)
    {
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "No base item selected yet.");
    }

    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, smith_o_ptr->number > 1, 2);
    strnfmt(buf, sizeof(buf), "%s   %d.%d lb", o_desc,
        smith_o_ptr->weight * smith_o_ptr->number / 10,
        (smith_o_ptr->weight * smith_o_ptr->number) % 10);
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_L_WHITE, buf))
        return false;

    if (too_difficult(smith_o_ptr))
        attr = TERM_L_DARK;
    else
        attr = TERM_SLATE;

    dif = object_difficulty(smith_o_ptr);
    if ((smithing_cost.drain > 0)
        && (smithing_cost.drain <= p_ptr->skill_base[S_SMT]))
    {
        attr = TERM_BLUE;
    }

    strnfmt(buf, sizeof(buf), "Difficulty: %d (max %d)", dif,
        p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
    if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
        return false;

    if (smithing_cost.uses > 0 && forge_uses(p_ptr->py, p_ptr->px) < smithing_cost.uses)
        can_afford = false;
    if (smithing_cost.drain > 0
        && smithing_cost.drain > p_ptr->skill_base[S_SMT])
    {
        can_afford = false;
    }
    if (smithing_cost.mithril > 0 && smithing_cost.mithril > mithril_carried())
        can_afford = false;
    if (smithing_cost.star_iron > 0
        && smithing_cost.star_iron > star_iron_carried())
    {
        can_afford = false;
    }
    if (smithing_cost.str > 0
        && p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
            - smithing_cost.str
            < -5)
    {
        can_afford = false;
    }
    if (smithing_cost.dex > 0
        && p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
            - smithing_cost.dex
            < -5)
    {
        can_afford = false;
    }
    if (smithing_cost.con > 0
        && p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
            - smithing_cost.con
            < -5)
    {
        can_afford = false;
    }
    if (smithing_cost.gra > 0
        && p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
            - smithing_cost.gra
            < -5)
    {
        can_afford = false;
    }
    if (smithing_cost.exp > 0 && p_ptr->new_exp < smithing_cost.exp)
        can_afford = false;

    if (!smith_ui_panel_try_add_detail_line(panel,
            can_afford ? TERM_SLATE : TERM_L_DARK, "Cost:"))
    {
        return false;
    }

    if (smithing_cost.weaponsmith
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Weaponsmith"))
    {
        return false;
    }
    if (smithing_cost.armoursmith
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Armoursmith"))
    {
        return false;
    }
    if (smithing_cost.jeweller
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Jeweller"))
    {
        return false;
    }
    if (smithing_cost.enchantment
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Enchantment"))
    {
        return false;
    }
    if (smithing_cost.artifice
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED, "Artifice"))
    {
        return false;
    }
    if (smithing_cost.alloy_mastery
        && !smith_ui_panel_try_add_detail_line(panel, TERM_RED,
            "Alloy Mastery"))
    {
        return false;
    }
    if (smithing_cost.uses > 0)
    {
        attr = (forge_uses(p_ptr->py, p_ptr->px) >= smithing_cost.uses)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d/%d uses", smithing_cost.uses,
            forge_uses(p_ptr->py, p_ptr->px));
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.drain > 0)
    {
        attr = (smithing_cost.drain <= p_ptr->skill_base[S_SMT])
            ? TERM_BLUE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Smithing", smithing_cost.drain);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.mithril > 0)
    {
        attr = (smithing_cost.mithril <= mithril_carried()) ? TERM_SLATE
                                                             : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d.%d lb Mithril", smithing_cost.mithril / 10,
            smithing_cost.mithril % 10);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.star_iron > 0)
    {
        attr = (smithing_cost.star_iron <= star_iron_carried()) ? TERM_SLATE
                                                                 : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d.%d lb Star Iron",
            smithing_cost.star_iron / 10, smithing_cost.star_iron % 10);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.str > 0)
    {
        attr = (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
                >= -5)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Str", smithing_cost.str);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.dex > 0)
    {
        attr = (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
                >= -5)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Dex", smithing_cost.dex);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.con > 0)
    {
        attr = (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
                >= -5)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Con", smithing_cost.con);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.gra > 0)
    {
        attr = (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
                >= -5)
            ? TERM_SLATE
            : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Gra", smithing_cost.gra);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }
    if (smithing_cost.exp > 0)
    {
        attr = (p_ptr->new_exp >= smithing_cost.exp) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Exp", smithing_cost.exp);
        if (!smith_ui_panel_try_add_detail_line(panel, attr, buf))
            return false;
    }

    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
        turn_multiplier /= 2;
    strnfmt(buf, sizeof(buf), "%d Turns", MAX(10, dif * turn_multiplier));
    return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf);
}

#define SMITH_UI_BROWSER_ROW_WINDOW 16

static int smith_ui_base_item_hotkey_choice(char ch, int max_choice)
{
    int max_hotkey;

    if (ch >= 'A' && ch <= 'Z')
        ch = (char)(ch - 'A' + 'a');
    if (max_choice <= 0)
        return 0;

    max_hotkey = MIN(max_choice, 26);
    if (ch >= 'a' && ch < (char)('a' + max_hotkey))
        return (int)(ch - 'a') + 1;

    return 0;
}

static int smith_ui_row_scroll_offset(int count, int highlight, int window)
{
    int offset;

    if (count <= 0 || window <= 0 || count <= window)
        return 0;

    if (highlight < 1)
        highlight = 1;
    if (highlight > count)
        highlight = count;

    offset = (highlight - 1) - (window / 2);
    if (offset < 0)
        offset = 0;
    if (offset > count - window)
        offset = count - window;

    return offset;
}

static byte smith_ui_base_item_tval_attr(int highlight)
{
    int category;

    if (highlight < 1 || highlight > MAX_SMITHING_TVALS)
        return TERM_L_DARK;

    category = smithing_tvals[highlight - 1].category;
    if (category == CAT_WEAPON)
    {
        return p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH] ? TERM_WHITE
                                                              : TERM_RED;
    }
    if (category == CAT_ARMOUR)
    {
        return p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH] ? TERM_WHITE
                                                              : TERM_RED;
    }
    if (category == CAT_JEWELRY)
    {
        return p_ptr->active_ability[S_SMT][SMT_JEWELLER] ? TERM_WHITE
                                                           : TERM_RED;
    }

    return TERM_WHITE;
}

static cptr smith_ui_base_item_category_name(int category)
{
    switch (category)
    {
    case CAT_WEAPON:
        return "Weapons";
    case CAT_ARMOUR:
        return "Armour";
    case CAT_JEWELRY:
        return "Jewelry";
    default:
        return "Base Item";
    }
}

static cptr smith_ui_base_item_category_skill_name(int category)
{
    switch (category)
    {
    case CAT_WEAPON:
        return "Weaponsmith";
    case CAT_ARMOUR:
        return "Armoursmith";
    case CAT_JEWELRY:
        return "Jeweller";
    default:
        return "";
    }
}

static bool smith_ui_base_item_category_skill_ready(int category)
{
    switch (category)
    {
    case CAT_WEAPON:
        return p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH];
    case CAT_ARMOUR:
        return p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH];
    case CAT_JEWELRY:
        return p_ptr->active_ability[S_SMT][SMT_JEWELLER];
    default:
        return true;
    }
}

static int smith_ui_base_item_count(int tval)
{
    int count = 0;

    for (int i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (k_ptr->tval != tval)
            continue;
        if (!smith_base_item_kind_allowed(k_ptr))
            continue;

        count++;
    }

    return count;
}

static bool smith_ui_base_item_set_preview(int tval, int choice, char* name,
    size_t name_size, bool* affordable_out)
{
    int count = 0;

    if (name && name_size)
        name[0] = '\0';
    if (affordable_out)
        *affordable_out = false;

    for (int i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (k_ptr->tval != tval)
            continue;
        if (!smith_base_item_kind_allowed(k_ptr))
            continue;

        count++;
        if (count != choice)
            continue;

        if (name && name_size)
            strip_name(name, i);
        create_base_object(tval, k_ptr->sval);
        if (affordable_out)
            *affordable_out = affordable(smith_o_ptr);
        return true;
    }

    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);
    return false;
}

static bool smith_ui_base_item_build_category_scene(app_ui_scene* scene,
    int highlight)
{
    app_ui_panel* panel;
    const smithing_tval_desc* desc;
    char buf[APP_UI_TEXT_MAX];
    byte detail_attr;
    cptr skill_name;

    if (!scene)
        return false;

    if (highlight < 1 || highlight > MAX_SMITHING_TVALS)
        highlight = 1;

    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    desc = &smithing_tvals[highlight - 1];
    skill_name = smith_ui_base_item_category_skill_name(desc->category);

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "Base Item: choose a family");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 opens, 8/2 moves, a-q jumps, Esc/4 cancels.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Open");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Open");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-q", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Cancel");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        MAX_SMITHING_TVALS, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    for (int i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        char key[APP_UI_KEY_MAX];
        byte attr = (i + 1 == highlight) ? TERM_L_BLUE
                                         : smith_ui_base_item_tval_attr(i + 1);

        strnfmt(key, sizeof(key), "%c", (char)('a' + i));
        if (!app_ui_panel_add_row(panel, (s16b)(i + 1), attr, true,
                i + 1 == highlight, key, smithing_tvals[i].desc, ""))
        {
            return false;
        }
    }

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, desc->desc);
    strnfmt(buf, sizeof(buf), "%s family", smith_ui_base_item_category_name(
        desc->category));
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;

    detail_attr = smith_ui_base_item_category_skill_ready(desc->category)
        ? TERM_SLATE
        : TERM_RED;
    strnfmt(buf, sizeof(buf), "Ability: %s", skill_name);
    if (!smith_ui_panel_try_add_detail_line(panel, detail_attr, buf))
        return false;

    return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
        "Pick a specific base item next.");
}

static bool smith_ui_base_item_build_item_scene(app_ui_scene* scene, int tval,
    int highlight, int count)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    char selected_name[APP_UI_TITLE_MAX];
    const smithing_tval_desc* desc = NULL;
    bool selected_affordable = false;

    if (!scene)
        return false;
    if (count < 0)
        count = 0;
    if (highlight < 1)
        highlight = 1;
    if (count > 0 && highlight > count)
        highlight = count;

    for (int i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        if (smithing_tvals[i].tval == tval)
        {
            desc = &smithing_tvals[i];
            break;
        }
    }

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    strnfmt(subtitle, sizeof(subtitle), "Base Item: %s",
        desc ? desc->desc : "Select");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 selects, 8/2 moves, a-z jumps, Esc/4 goes back.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Select");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-z", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    if (count <= 0)
    {
        object_wipe(smith_o_ptr);
        smith_clear_alloy_state(&smith_alloy);
        if (!app_ui_panel_add_row(panel, 0, TERM_SLATE, true, false, "",
                "Nothing available.", ""))
        {
            return false;
        }
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "No Base Items");
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "No smithable base items are available in this family.");
    }

    selected_name[0] = '\0';
    for (int i = 1, row = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        char key[APP_UI_KEY_MAX];
        char name[APP_UI_LABEL_MAX];
        bool can_afford;
        byte attr;

        if (k_ptr->tval != tval)
            continue;
        if (!smith_base_item_kind_allowed(k_ptr))
            continue;

        row++;
        name[0] = '\0';
        strip_name(name, i);
        create_base_object(tval, k_ptr->sval);
        can_afford = affordable(smith_o_ptr);
        attr = (row == highlight) ? TERM_L_BLUE
                                  : (can_afford ? TERM_WHITE : TERM_SLATE);

        key[0] = '\0';
        if (row <= 26)
            strnfmt(key, sizeof(key), "%c", (char)('a' + row - 1));

        if (!app_ui_panel_add_row(panel, (s16b)row, attr, true,
                row == highlight, key, name, ""))
        {
            return false;
        }

        if (row == highlight)
        {
            SDL_strlcpy(selected_name, name, sizeof(selected_name));
            selected_affordable = can_afford;
        }
    }

    if (!smith_ui_base_item_set_preview(tval, highlight, NULL, 0,
            &selected_affordable))
    {
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Base Item");
        return smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Unable to build the selected base item preview.");
    }

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        selected_name[0] ? selected_name : "Base Item");
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE,
            "Select this base item to start the design."))
    {
        return false;
    }
    if (!selected_affordable
        && !smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "Current costs exceed your resources, but you can still choose it."))
    {
        return false;
    }

    return smith_ui_main_menu_add_current_item_detail(panel);
}

static bool smith_ui_base_item_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    bool choosing_tval = true;
    int tval_highlight = 1;
    int sval_highlight = 1;
    int current_tval = 0;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return false;

    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    while (true)
    {
        app_ui_scene scene;
        int choice_count;
        int choice;
        char ch;

        if (choosing_tval)
        {
            if (tval_highlight < 1 || tval_highlight > MAX_SMITHING_TVALS)
                tval_highlight = 1;
            if (!smith_ui_base_item_build_category_scene(&scene, tval_highlight)
                || !smith_ui_snapshot_scene_present(&scope, &scene))
            {
                log_warn("smithing snapshot base-item menu: failed to publish category scene");
                break;
            }
            choice_count = MAX_SMITHING_TVALS;
        }
        else
        {
            choice_count = smith_ui_base_item_count(current_tval);
            if (choice_count <= 0)
                sval_highlight = 1;
            else if (sval_highlight < 1 || sval_highlight > choice_count)
                sval_highlight = 1;

            if (!smith_ui_base_item_build_item_scene(&scene, current_tval,
                    sval_highlight, choice_count)
                || !smith_ui_snapshot_scene_present(&scope, &scene))
            {
                log_warn("smithing snapshot base-item menu: failed to publish item scene");
                break;
            }
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, choice_count);
        if (choice > 0)
        {
            if (choosing_tval)
            {
                tval_highlight = choice;
                current_tval = smithing_tvals[tval_highlight - 1].tval;
                sval_highlight = 1;
                choosing_tval = false;
                continue;
            }

            sval_highlight = choice;
            if (smith_ui_base_item_set_preview(current_tval, sval_highlight,
                    NULL, 0, NULL))
            {
                smith_ui_snapshot_scene_close(&scope);
                return true;
            }
            bell("Invalid choice.");
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (choosing_tval)
            {
                current_tval = smithing_tvals[tval_highlight - 1].tval;
                if (smith_ui_base_item_count(current_tval) <= 0)
                {
                    bell("Invalid choice.");
                    continue;
                }
                sval_highlight = 1;
                choosing_tval = false;
                continue;
            }

            if (smith_ui_base_item_set_preview(current_tval, sval_highlight,
                    NULL, 0, NULL))
            {
                smith_ui_snapshot_scene_close(&scope);
                return true;
            }

            bell("Invalid choice.");
            continue;
        }

        if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            if (choosing_tval)
                break;

            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);
            choosing_tval = true;
            continue;
        }

        if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (choosing_tval)
            {
                if (tval_highlight > 1)
                    tval_highlight--;
                else
                    tval_highlight = MAX_SMITHING_TVALS;
            }
            else if (choice_count > 0)
            {
                if (sval_highlight > 1)
                    sval_highlight--;
                else
                    sval_highlight = choice_count;
            }
            continue;
        }

        if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (choosing_tval)
            {
                if (tval_highlight < MAX_SMITHING_TVALS)
                    tval_highlight++;
                else
                    tval_highlight = 1;
            }
            else if (choice_count > 0)
            {
                if (sval_highlight < choice_count)
                    sval_highlight++;
                else
                    sval_highlight = 1;
            }
            continue;
        }
    }

    smith_ui_snapshot_scene_close(&scope);
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);
    return false;
}

/*
 * Wave 1A lane split: keep these lane-local menu modules in the smithing UI
 * folder while the integrator retains the build-list merge point.
 */
#include "ui/smithing/ui-smithing-numbers-model.h"
#include "ui/smithing/ui-smithing-numbers-detail.h"
#include "ui/smithing/ui-smithing-numbers-flow.h"
#include "ui/smithing/ui-smithing-bonus-model.h"
#include "ui/smithing/ui-smithing-bonus-flow.h"
#include "ui/smithing/ui-smithing-melt-model.h"
#include "ui/smithing/ui-smithing-melt-flow.h"
#include "ui/smithing/ui-smithing-enchant-model.h"
#include "ui/smithing/ui-smithing-enchant-flow.h"
#include "ui/smithing/ui-smithing-reforge-model.h"
#include "ui/smithing/ui-smithing-reforge-flow.h"
#include "ui/smithing/ui-smithing-artefact-defs.h"
#include "ui/smithing/ui-smithing-artefact-root.h"
#include "ui/smithing/ui-smithing-artefact-flag-model.h"
#include "ui/smithing/ui-smithing-artefact-flag-flow.h"
#include "ui/smithing/ui-smithing-artefact-ability-model.h"
#include "ui/smithing/ui-smithing-artefact-ability-flow.h"
#include "ui/smithing/ui-smithing-artefact-flow.h"

static bool smith_ui_main_menu_build_scene(app_ui_scene* scene,
    const smith_ui_main_menu_state* state, int highlight)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    int choice;

    if (!scene || !state)
        return false;

    if (highlight < 1 || highlight > SMT_MENU_MAX)
        highlight = SMT_MENU_CREATE;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");

    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        SDL_strlcpy(subtitle, "Exploration mode: smithing requires a forge.",
            sizeof(subtitle));
    }
    else if (forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        SDL_strlcpy(subtitle,
            "Exploration mode: this forge has no resources left.",
            sizeof(subtitle));
    }
    else if (p_ptr->smithing_leftover > 0)
    {
        strnfmt(subtitle, sizeof(subtitle),
            "Current work can be resumed with %d turns left.",
            p_ptr->smithing_leftover);
    }
    else
    {
        SDL_strlcpy(subtitle, "Choose an action for the current design.",
            sizeof(subtitle));
    }
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 selects, 8/2 moves, a-g jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Select");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-g", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");

    for (choice = 1; choice <= SMT_MENU_MAX; choice++)
    {
        char key[APP_UI_KEY_MAX];
        byte row_attr;

        strnfmt(key, sizeof(key), "%c", (char)('a' + choice - 1));
        row_attr = (choice == highlight) ? TERM_L_BLUE : state->row_attr[choice - 1];
        if (!app_ui_panel_add_row(panel, (s16b)choice, row_attr,
                state->valid[choice - 1], choice == highlight, key,
                smith_ui_main_menu_label(choice), ""))
        {
            return false;
        }
    }

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        smith_ui_main_menu_label(highlight));
    return smith_ui_main_menu_add_selected_detail(panel, highlight)
        && smith_ui_main_menu_add_current_item_detail(panel);
}

static int smith_ui_main_menu_hotkey_choice(char ch)
{
    if (ch >= 'A' && ch <= 'G')
        ch = (char)(ch - 'A' + 'a');
    if (ch >= 'a' && ch <= (char)('a' + SMT_MENU_MAX - 1))
        return (int)(ch - 'a') + 1;

    return 0;
}

static int smithing_menu_snapshot(int* highlight)
{
    smith_ui_snapshot_scope scope;

    if (!highlight)
        return -1;
    if (!smith_ui_snapshot_scene_enter(&scope))
        return -1;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_main_menu_state state;
        int choice;
        char ch;

        if (*highlight < 1 || *highlight > SMT_MENU_MAX)
            *highlight = SMT_MENU_CREATE;

        smith_ui_main_menu_build_state(&state);
        if (!smith_ui_main_menu_build_scene(&scene, &state, *highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot hub: failed to build or publish semantic scene");
            smith_ui_snapshot_scene_close(&scope);
            return -1;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_main_menu_hotkey_choice(ch);
        if (choice > 0)
        {
            *highlight = choice;
            if (state.valid[*highlight - 1])
            {
                smith_ui_snapshot_begin_nested_transition();
                smith_ui_snapshot_scene_close(&scope);
                return *highlight;
            }
            bell("Invalid choice.");
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (state.valid[*highlight - 1])
            {
                smith_ui_snapshot_begin_nested_transition();
                smith_ui_snapshot_scene_close(&scope);
                return *highlight;
            }
            bell("Invalid choice.");
            continue;
        }

        if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            smith_ui_snapshot_scene_close(&scope);
            return -1;
        }

        if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (*highlight > 1)
                (*highlight)--;
            else
                *highlight = SMT_MENU_MAX;
            continue;
        }

        if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (*highlight < SMT_MENU_MAX)
                (*highlight)++;
            else
                *highlight = 1;
            continue;
        }
    }
}

/*
 * Brings up a screen for making new items (only works at a forge).
 * Leads to many submenus which help to determine the item's attributes.
 */
void do_cmd_smithing_screen(void)
{
    app_wait_scope wait_scope;
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool create = false;

    smith_ui_snapshot_reset_nested_transitions();
    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_LIST_SELECTION, 0, 0);

    // if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    //{
    //	msg_print("You can only create items at a forge.");
    //	return;
    //}

    if (cave_forge_bold(p_ptr->py, p_ptr->px)
        && forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        msg_print("The resources of this forge are exhausted.");
        msg_print(
            "You will be able to browse the options but not make new things.");
    }

    // Hack: flag that we are in the middle of smithing
    p_ptr->smithing = 1;

    // deal with previous interruptions
    if (p_ptr->smithing_leftover > 0)
    {
        // default to 'resume' if an item is already in progress
        highlight = SMT_MENU_ACCEPT;

        // and backup the smithing item
        object_copy(smith2_o_ptr, smith_o_ptr);
        smith2_alloy = smith_alloy;
    }

    // otherwise wipe the smithing item
    else
    {
        object_wipe(smith_o_ptr);
        smith_clear_alloy_state(&smith_alloy);
    }

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = smithing_menu_snapshot(&highlight);

        // if an action has been selected...
        switch (actiontype)
        {
        case SMT_MENU_CREATE:
        {
            // this is not a resumption of smithing an item
            p_ptr->smithing_leftover = 0;

            create_tval_menu();

            // backup the smithing object
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            break;
        }
        case SMT_MENU_ENCHANT:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                if (!enchant_menu())
                {
                    // restore the smithing object
                    object_copy(smith_o_ptr, smith2_o_ptr);
                    smith_alloy = smith2_alloy;
                }
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_ARTEFACT:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                artefact_menu();
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_NUMBERS:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                numbers_menu();

                // backup the smithing object
                object_copy(smith2_o_ptr, smith_o_ptr);
                smith2_alloy = smith_alloy;
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_MELT:
        {
            if (meltable_metal_items_carried())
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                melt_menu();
            }
            else
            {
                bell("You don't have any mithril or star-iron items.");
            }

            break;
        }
        case SMT_MENU_REPAIR:
        {
            smith_reforge_item();
            break;
        }
        case SMT_MENU_ACCEPT:
        {
            if (smithing_cost.drain > 0)
            {
                char buf[80];

                sprintf(buf,
                    "This will drain your smithing skill by %d points. "
                    "Proceed? ",
                    smithing_cost.drain);
                if (!get_check(buf))
                    break;
            }

            create = true;
            leave_menu = true;
            break;
        }
        case -1:
        {
            leave_menu = true;
            break;
        }
        }

        if (actiontype > 0)
            smith_ui_snapshot_end_nested_transition();
    }

    if (create)
    {
        int turn_multiplier = 10;

        if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
        {
            turn_multiplier /= 2;
        }

        // Display a message
        msg_print("You begin your work.");

        // add the details to the artefact type if applicable
        if (smith_o_ptr->name1)
            add_artefact_details();

        /* Cancel stealth mode */
        p_ptr->stealth_mode = false;

        // Allow the resumption of interrupted smithing
        if (p_ptr->smithing_leftover > 0)
        {
            p_ptr->smithing = p_ptr->smithing_leftover;
        }
        else
        {
            // Set smithing counter
            p_ptr->smithing
                = MAX(10, object_difficulty(smith_o_ptr) * turn_multiplier);

            // Also set the smithing leftover counter (to allow you to resume if
            // interrupted)
            p_ptr->smithing_leftover = p_ptr->smithing;
        }

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Handle stuff */
        handle_stuff();
    }

    else
    {
        if (p_ptr->smithing_leftover == 0)
        {
            /* Wipe the smithing object */
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);
        }

        // Hack: flag that we are done with smithing
        p_ptr->smithing = 0;
    }

    app_session_clear_dungeon_overlay_scene(app_session_current());
    app_session_clear_interaction(app_session_current());
    smith_ui_snapshot_reset_nested_transitions();
    platform_frame_present();
    app_session_pop_wait_scope(app_session_current(), &wait_scope);
}
