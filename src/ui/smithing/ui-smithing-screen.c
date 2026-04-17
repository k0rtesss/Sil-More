/* File: ui-smithing-screen.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
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

/*
 * A list of tvals and their textual names
 */
typedef struct smithing_flag_cat
{
    int category;
    cptr desc;
} smithing_flag_cat;

#define CAT_STAT 1
#define CAT_SUST 2
#define CAT_SKILL 3
#define CAT_MEL 4
#define CAT_SLAY 5
#define CAT_RES 6
#define CAT_MISC 7

#define MAX_CATS 7

#define MAX_SMITHING_FLAGS (32 * 4)

static const smithing_flag_cat smithing_flag_cats[]
    = { { CAT_STAT, "Stat bonuses" }, { CAT_SUST, "Sustains" },
          { CAT_SKILL, "Skill bonuses" }, { CAT_MEL, "Melee powers" },
          { CAT_SLAY, "Slays" }, { CAT_RES, "Resistances" },
          { CAT_MISC, "Misc" } };

/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_desc
{
    int category;
    u32b flag;
    int flagset;
    cptr desc;
} smithing_flag_desc;

/*
 * A list of tvals and their textual names
 */
static const smithing_flag_desc smithing_flag_types[] = { { CAT_STAT, TR1_STR,
                                                              1, "Str bonus" },
    { CAT_STAT, TR1_DEX, 1, "Dex bonus" },
    { CAT_STAT, TR1_CON, 1, "Con bonus" },
    { CAT_STAT, TR1_GRA, 1, "Gra bonus" },
    { CAT_STAT, TR1_NEG_STR, 1, "Str penalty" },
    { CAT_STAT, TR1_NEG_DEX, 1, "Dex penalty" },
    { CAT_STAT, TR1_NEG_CON, 1, "Con penalty" },
    { CAT_STAT, TR1_NEG_GRA, 1, "Gra penalty" },
    { CAT_SKILL, TR1_ARC, 1, "Archery" }, { CAT_SKILL, TR1_STL, 1, "Stealth" },
    { CAT_SKILL, TR1_PER, 1, "Perception" }, { CAT_SKILL, TR1_WIL, 1, "Will" },
    { CAT_SKILL, TR1_SMT, 1, "Smithing" }, { CAT_SKILL, TR1_SNG, 1, "Song" },
    { CAT_MISC, TR1_DAMAGE_SIDES, 1, "Damage bonus" },
    { CAT_MISC, TR2_LIGHT, 2, "Light" },
    { CAT_MISC, TR2_SLOW_DIGEST, 2, "Sustenance" },
    { CAT_MISC, TR2_REGEN, 2, "Regeneration" },
    { CAT_MISC, TR2_SEE_INVIS, 2, "See Invisible" },
    { CAT_MISC, TR2_FREE_ACT, 2, "Free Action" },
    { CAT_MISC, TR2_SPEED, 2, "Speed" },
    { CAT_MISC, TR2_RADIANCE, 2, "Radiance" },
    { CAT_MISC, TR3_CHEAT_DEATH, 3, "Cheat Death" },
    { CAT_MISC, TR3_STAND_FAST, 3, "Stand Fast" },
    { CAT_MISC, TR3_AVOID_TRAPS, 3, "Avoid Traps" },
    { CAT_MISC, TR3_MEDIC, 3, "Medicine Bonus" },
    { CAT_MISC, TR4_PROT_FIRE, 4, "Protection vs Fire" },
    { CAT_MISC, TR4_PROT_COLD, 4, "Protection vs Cold" },
    { CAT_MISC, TR4_PROT_POIS, 4, "Protection vs Poison" },
    { CAT_MISC, TR4_PROT_DARK, 4, "Protection vs Darkness" },
    { CAT_MEL, TR1_TUNNEL, 1, "Tunneling Bonus" },
    { CAT_MEL, TR1_SHARPNESS, 1, "Sharpness" },
    { CAT_MEL, TR1_SHARPNESS2, 1, "Sharpness2" },
    { CAT_MEL, TR1_VAMPIRIC, 1, "Vampiric" },
    { CAT_MEL, TR3_ACCURATE, 3, "Accurate" },
    { CAT_SLAY, TR1_SLAY_ORC, 1, "Slay Orc" },
    { CAT_SLAY, TR1_SLAY_TROLL, 1, "Slay Troll" },
    { CAT_SLAY, TR1_SLAY_WOLF, 1, "Slay Wolf" },
    { CAT_SLAY, TR1_SLAY_SPIDER, 1, "Slay Spider" },
    { CAT_SLAY, TR1_SLAY_UNDEAD, 1, "Slay Undead" },
    { CAT_SLAY, TR1_SLAY_RAUKO, 1, "Slay Rauko" },
    { CAT_SLAY, TR1_SLAY_DRAGON, 1, "Slay Dragon" },
    { CAT_SLAY, TR4_SLAY_SERPENT, 4, "Slay Serpent" },
    { CAT_SLAY, TR4_SLAY_VAMPIRE, 4, "Slay Vampire" },
    { CAT_SLAY, TR4_SLAY_HORROR, 4, "Slay Horror" },
    { CAT_SLAY, TR4_SLAY_CAT, 4, "Slay Cat" },
    { CAT_SLAY, TR4_SLAY_GIANT, 4, "Slay Giant" },
    { CAT_SLAY, TR1_BRAND_COLD, 1, "Brand with Cold" },
    { CAT_SLAY, TR1_BRAND_FIRE, 1, "Brand with Fire" },
    { CAT_SLAY, TR1_BRAND_POIS, 1, "Brand with Poison" },
    { CAT_SUST, TR2_SUST_STR, 2, "Sustain Str" },
    { CAT_SUST, TR2_SUST_DEX, 2, "Sustain Dex" },
    { CAT_SUST, TR2_SUST_CON, 2, "Sustain Con" },
    { CAT_SUST, TR2_SUST_GRA, 2, "Sustain Gra" },
    { CAT_RES, TR2_RES_COLD, 2, "Resist Cold" },
    { CAT_RES, TR2_RES_FIRE, 2, "Resist Fire" },
    { CAT_RES, TR2_RES_POIS, 2, "Resist Poison" },
    { CAT_RES, TR2_RES_BLEED, 2, "Resist Bleeding" },
    { CAT_RES, TR2_RES_FEAR, 2, "Resist Fear" },
    { CAT_RES, TR2_RES_BLIND, 2, "Resist Blindness" },
    { CAT_RES, TR2_RES_CONFU, 2, "Resist Confusion" },
    { CAT_RES, TR2_RES_STUN, 2, "Resist Stunning" },
    { CAT_RES, TR2_RES_HALLU, 2, "Resist Hallucination" }, { 0, 0, 0, "" } };

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

static bool smith_ui_numbers_build_scene(app_ui_scene* scene,
    const smith_ui_numbers_menu_state* state, int highlight)
{
    app_ui_panel* panel;
    int choice;

    if (!scene || !state)
        return false;

    if (highlight < 1 || highlight > SMT_NUM_MENU_MAX)
        highlight = SMT_NUM_MENU_I_ATT;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Numbers");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 applies, 8/2 moves, a-m jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Apply");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Apply");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-m", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        SMT_NUM_MENU_MAX, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    for (choice = 1; choice <= SMT_NUM_MENU_MAX; choice++)
    {
        char key[APP_UI_KEY_MAX];
        byte row_attr = (choice == highlight) ? TERM_L_BLUE
                                              : state->row_attr[choice - 1];

        strnfmt(key, sizeof(key), "%c", (char)('a' + choice - 1));
        if (!app_ui_panel_add_row(panel, (s16b)choice, row_attr,
                state->valid[choice - 1], choice == highlight, key,
                smith_ui_numbers_action_label(choice), ""))
        {
            return false;
        }
    }

    return smith_ui_numbers_add_selected_detail(panel, state, highlight);
}

static void smith_ui_numbers_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_numbers_menu_state state;
        int choice;
        char ch;

        smith_ui_numbers_build_state(&state);
        if (highlight < 1 || highlight > SMT_NUM_MENU_MAX)
            highlight = SMT_NUM_MENU_I_ATT;

        if (!smith_ui_numbers_build_scene(&scene, &state, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot numbers menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, SMT_NUM_MENU_MAX);
        if (choice > 0)
        {
            highlight = choice;
            if (state.valid[highlight - 1])
            {
                if (highlight == SMT_NUM_MENU_EDIT_BONUSES)
                {
                    smith_ui_snapshot_begin_nested_transition();
                    smith_ui_snapshot_scene_close(&scope);
                    smith_bonus_menu();
                    smith_ui_snapshot_end_nested_transition();
                    if (!smith_ui_snapshot_scene_enter(&scope))
                        return;
                }
                else
                {
                    modify_numbers(highlight);
                }
            }
            else
            {
                bell("Invalid choice.");
            }
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (state.valid[highlight - 1])
            {
                if (highlight == SMT_NUM_MENU_EDIT_BONUSES)
                {
                    smith_ui_snapshot_begin_nested_transition();
                    smith_ui_snapshot_scene_close(&scope);
                    smith_bonus_menu();
                    smith_ui_snapshot_end_nested_transition();
                    if (!smith_ui_snapshot_scene_enter(&scope))
                        return;
                }
                else
                {
                    modify_numbers(highlight);
                }
            }
            else
            {
                bell("Invalid choice.");
            }
            continue;
        }

        if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            break;
        }

        if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (highlight > 1)
                highlight--;
            else
                highlight = SMT_NUM_MENU_MAX;
            continue;
        }

        if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (highlight < SMT_NUM_MENU_MAX)
                highlight++;
            else
                highlight = 1;
            continue;
        }
    }

    smith_ui_snapshot_scene_close(&scope);
}

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

static bool smith_ui_bonus_build_scene(app_ui_scene* scene,
    const smith_ui_bonus_menu_state* state, int highlight)
{
    app_ui_panel* panel;

    if (!scene || !state)
        return false;

    if (highlight < 1)
        highlight = 1;
    if (state->action_count > 0 && highlight > state->action_count)
        highlight = state->action_count;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Numbers: special bonuses");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 applies, 8/2 moves, a-z jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Apply");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Apply");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-z", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        state->action_count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    if (state->action_count <= 0)
    {
        if (!app_ui_panel_add_row(panel, 0, TERM_SLATE, true, false, "",
                "Nothing editable.", ""))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < state->action_count; i++)
        {
            char key[APP_UI_KEY_MAX];
            char label[APP_UI_LABEL_MAX];
            char meta[APP_UI_META_MAX];
            byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE
                                                 : state->row_attr[i];

            key[0] = '\0';
            if (i < 26)
                strnfmt(key, sizeof(key), "%c", (char)('a' + i));
            smith_ui_bonus_action_build_label(&state->actions[i], label,
                sizeof(label), meta, sizeof(meta));
            if (!app_ui_panel_add_row_ex(panel, (s16b)(i + 1), row_attr,
                    row_attr, 0, '\0', state->valid[i], i + 1 == highlight,
                    key, label, meta))
            {
                return false;
            }
        }
    }

    return smith_ui_bonus_add_selected_detail(panel, state, highlight);
}

static void smith_ui_bonus_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_bonus_menu_state state;
        int choice;
        char ch;

        smith_ui_bonus_build_state(&state);
        if (state.action_count > 0 && highlight > state.action_count)
            highlight = state.action_count;
        if (highlight < 1)
            highlight = 1;

        if (!smith_ui_bonus_build_scene(&scene, &state, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot bonus menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.action_count);
        if (choice > 0)
        {
            highlight = choice;
            if ((highlight <= state.action_count) && state.valid[highlight - 1])
            {
                (void)smith_adjust_bonus_entry(&state.actions[highlight - 1].entry,
                    state.actions[highlight - 1].delta);
            }
            else
            {
                bell("Invalid choice.");
            }
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if ((highlight <= state.action_count) && state.valid[highlight - 1])
            {
                (void)smith_adjust_bonus_entry(&state.actions[highlight - 1].entry,
                    state.actions[highlight - 1].delta);
            }
            else
            {
                bell("Invalid choice.");
            }
            continue;
        }

        if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            break;
        }

        if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (state.action_count > 0)
            {
                if (highlight > 1)
                    highlight--;
                else
                    highlight = state.action_count;
            }
            continue;
        }

        if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (state.action_count > 0)
            {
                if (highlight < state.action_count)
                    highlight++;
                else
                    highlight = 1;
            }
            continue;
        }
    }

    smith_ui_snapshot_scene_close(&scope);
}

typedef struct smith_ui_melt_menu_state
{
    int count;
    int slots[INVEN_TOTAL];
} smith_ui_melt_menu_state;

typedef struct smith_ui_enchant_menu_state
{
    bool selecting_prefix;
    int entry_count;
    int choice[26];
    bool valid[26];
    byte row_attr[26];
    int fixed_prefix;
    int fixed_suffix;
    const object_type* base_o_ptr;
} smith_ui_enchant_menu_state;

typedef struct smith_ui_reforge_menu_state
{
    int entry_count;
    int choice[26];
    bool valid[26];
    byte row_attr[26];
    reforge_preview_type previews[26];
    const object_type* source;
} smith_ui_reforge_menu_state;

typedef struct smith_ui_artefact_flag_menu_state
{
    int count;
    u32b flags[MAX_SMITHING_FLAGS];
    int flagsets[MAX_SMITHING_FLAGS];
    const char* labels[MAX_SMITHING_FLAGS];
    bool present[MAX_SMITHING_FLAGS];
    bool valid[MAX_SMITHING_FLAGS];
    bool affordable[MAX_SMITHING_FLAGS];
    byte row_attr[MAX_SMITHING_FLAGS];
} smith_ui_artefact_flag_menu_state;

typedef struct smith_ui_artefact_ability_menu_state
{
    int count;
    int ability_nums[64];
    const char* labels[64];
    bool present[64];
    bool valid[64];
    bool affordable[64];
    byte row_attr[64];
} smith_ui_artefact_ability_menu_state;

static void smith_ui_melt_build_state(smith_ui_melt_menu_state* state)
{
    int item;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];
        u32b f1, f2, f3;

        object_flags(o_ptr, &f1, &f2, &f3);
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            state->slots[state->count++] = item;
        }
    }
}

static bool smith_ui_melt_add_selected_detail(app_ui_panel* panel,
    const smith_ui_melt_menu_state* state, int highlight)
{
    char buf[APP_UI_TEXT_MAX];
    char desc[80];
    int slot;
    object_type* o_ptr;
    u32b f1, f2, f3;
    cptr metal_name;

    if (!panel || !state)
        return false;

    if (state->count <= 0)
    {
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Melt");
        return smith_ui_panel_try_add_detail_line(panel, TERM_L_DARK,
            "You are not carrying any meltable mithril or star-iron items.");
    }

    if (highlight < 1 || highlight > state->count)
        highlight = 1;

    slot = state->slots[highlight - 1];
    o_ptr = &inventory[slot];
    object_flags(o_ptr, &f1, &f2, &f3);
    metal_name = (f3 & TR3_STAR_IRON) ? "Star Iron" : "Mithril";
    object_desc(desc, sizeof(desc), o_ptr, false, 2);

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, desc);
    strnfmt(buf, sizeof(buf), "Returns %d.%d lb of %s.", o_ptr->weight / 10,
        o_ptr->weight % 10, metal_name);
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;

    if (slot >= INVEN_WIELD)
        strnfmt(buf, sizeof(buf), "Location: %s", mention_use(slot));
    else
        strnfmt(buf, sizeof(buf), "Location: inventory slot %c",
            index_to_label(slot));
    if (!smith_ui_panel_try_add_detail_line(panel, TERM_SLATE, buf))
        return false;

    return smith_ui_panel_try_add_detail_line(panel, TERM_WHITE,
        "Selecting this item opens the normal melt confirmation prompt.");
}

static bool smith_ui_melt_build_scene(app_ui_scene* scene,
    const smith_ui_melt_menu_state* state, int highlight)
{
    app_ui_panel* panel;

    if (!scene || !state)
        return false;

    if (highlight < 1)
        highlight = 1;
    if (state->count > 0 && highlight > state->count)
        highlight = state->count;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 920, 1380);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Smithing");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Melt");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 melts, 8/2 moves, a-z jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Melt");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Melt");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-z", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        state->count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    if (state->count <= 0)
    {
        if (!app_ui_panel_add_row(panel, 0, TERM_SLATE, true, false, "",
                "Nothing available.", ""))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < state->count; i++)
        {
            char key[APP_UI_KEY_MAX];
            char label[APP_UI_LABEL_MAX];
            char meta[APP_UI_META_MAX];
            object_type* o_ptr = &inventory[state->slots[i]];
            byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE : TERM_WHITE;

            key[0] = '\0';
            if (i < 26)
                strnfmt(key, sizeof(key), "%c", (char)('a' + i));
            object_desc(label, sizeof(label), o_ptr, false, 2);
            strnfmt(meta, sizeof(meta), "%d.%d lb", o_ptr->weight / 10,
                o_ptr->weight % 10);
            if (!app_ui_panel_add_row_ex(panel, (s16b)(i + 1), row_attr,
                    row_attr, 0, '\0', true, i + 1 == highlight, key, label,
                    meta))
            {
                return false;
            }
        }
    }

    return smith_ui_melt_add_selected_detail(panel, state, highlight);
}

static void smith_ui_melt_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_melt_menu_state state;
        int choice;
        char ch;

        smith_ui_melt_build_state(&state);
        if (state.count > 0 && highlight > state.count)
            highlight = state.count;
        if (highlight < 1)
            highlight = 1;

        if (!smith_ui_melt_build_scene(&scene, &state, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot melt menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.count);
        if (choice > 0)
        {
            highlight = choice;
            smith_ui_snapshot_scene_close(&scope);
            if (melt_metal_item(highlight))
                return;
            if (!smith_ui_snapshot_scene_enter(&scope))
                return;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (state.count > 0)
            {
                smith_ui_snapshot_scene_close(&scope);
                if (melt_metal_item(highlight))
                    return;
                if (!smith_ui_snapshot_scene_enter(&scope))
                    return;
            }
            else
            {
                bell("Invalid choice.");
            }
            continue;
        }

        if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            break;
        }

        if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (state.count > 0)
            {
                if (highlight > 1)
                    highlight--;
                else
                    highlight = state.count;
            }
            continue;
        }

        if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (state.count > 0)
            {
                if (highlight < state.count)
                    highlight++;
                else
                    highlight = 1;
            }
            continue;
        }
    }

    smith_ui_snapshot_scene_close(&scope);
}

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

static bool smith_ui_enchant_build_scene(app_ui_scene* scene,
    const smith_ui_enchant_menu_state* state, int highlight)
{
    app_ui_panel* panel;

    if (!scene || !state)
        return false;

    if (highlight < 1 || highlight > state->entry_count)
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
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        state->selecting_prefix ? "Enchant: choose prefix"
                                : "Enchant: choose suffix");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 selects, 8/2 moves, a-z jumps, Esc/4 backs out.");
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
        state->entry_count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    for (int i = 0; i < state->entry_count; i++)
    {
        char key[APP_UI_KEY_MAX];
        char label[APP_UI_LABEL_MAX];
        byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE : state->row_attr[i];

        if (i == 0)
            SDL_strlcpy(label, "(none)", sizeof(label));
        else
            ego_name_for_enchant_menu(state->choice[i], label, sizeof(label));
        key[0] = '\0';
        if (i < 26)
            strnfmt(key, sizeof(key), "%c", (char)('a' + i));
        if (!app_ui_panel_add_row(panel, (s16b)(i + 1), row_attr,
                state->valid[i], i + 1 == highlight, key, label, ""))
        {
            return false;
        }
    }

    return smith_ui_enchant_add_selected_detail(panel, state, highlight);
}

static bool smith_ui_enchant_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int prefix_highlight = 1;
    int suffix_highlight = 1;
    bool completed = false;
    bool leave_menu = false;
    int selected_prefix;
    int selected_suffix;
    bool show_prefix_step;
    bool show_suffix_step;
    bool selecting_prefix;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return false;

    smith_o_ptr->name1 = 0;
    smith2_o_ptr->name1 = 0;

    selected_prefix = (int)object_ego_prefix(smith_o_ptr);
    selected_suffix = (int)object_ego_suffix(smith_o_ptr);
    show_prefix_step = enchant_menu_has_applicable_affix(
        smith2_o_ptr, 0, selected_suffix, true) || (selected_prefix != 0);
    show_suffix_step = enchant_menu_has_applicable_affix(
        smith2_o_ptr, selected_prefix, 0, false) || (selected_suffix != 0);

    if (!show_prefix_step && !show_suffix_step)
    {
        smith_ui_snapshot_scene_close(&scope);
        return false;
    }

    selecting_prefix = show_prefix_step;

    while (!leave_menu)
    {
        app_ui_scene scene;
        smith_ui_enchant_menu_state state;
        int choice;
        char ch;

        smith_ui_enchant_build_state(&state, selecting_prefix,
            selecting_prefix ? 0 : selected_prefix,
            selecting_prefix ? selected_suffix : 0, smith2_o_ptr);
        if (selecting_prefix)
        {
            if (prefix_highlight < 1 || prefix_highlight > state.entry_count)
                prefix_highlight = 1;
            if (!smith_ui_enchant_build_scene(&scene, &state, prefix_highlight)
                || !smith_ui_snapshot_scene_present(&scope, &scene))
            {
                log_warn("smithing snapshot enchant menu: failed to build or publish prefix scene");
                break;
            }
        }
        else
        {
            if (suffix_highlight < 1 || suffix_highlight > state.entry_count)
                suffix_highlight = 1;
            if (!smith_ui_enchant_build_scene(&scene, &state, suffix_highlight)
                || !smith_ui_snapshot_scene_present(&scope, &scene))
            {
                log_warn("smithing snapshot enchant menu: failed to build or publish suffix scene");
                break;
            }
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.entry_count);
        if (choice > 0)
        {
            if (selecting_prefix)
                prefix_highlight = choice;
            else
                suffix_highlight = choice;
        }
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            int* highlight_ptr = selecting_prefix ? &prefix_highlight
                                                  : &suffix_highlight;

            if (*highlight_ptr > 1)
                (*highlight_ptr)--;
            else
                *highlight_ptr = state.entry_count;
            continue;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            int* highlight_ptr = selecting_prefix ? &prefix_highlight
                                                  : &suffix_highlight;

            if (*highlight_ptr < state.entry_count)
                (*highlight_ptr)++;
            else
                *highlight_ptr = 1;
            continue;
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            if (selecting_prefix)
            {
                completed = false;
                leave_menu = true;
            }
            else
            {
                create_special(selected_prefix, selected_suffix);
                selecting_prefix = true;
            }
            continue;
        }
        else if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            /* Use current highlight */
        }
        else
        {
            continue;
        }

        if (selecting_prefix)
            create_special(state.choice[prefix_highlight - 1], selected_suffix);
        else
            create_special(selected_prefix, state.choice[suffix_highlight - 1]);

        if (selecting_prefix)
        {
            selected_prefix = (int)object_ego_prefix(smith_o_ptr);
            if (show_suffix_step)
            {
                selecting_prefix = false;
                continue;
            }
            completed = true;
            leave_menu = true;
        }
        else
        {
            selected_suffix = (int)object_ego_suffix(smith_o_ptr);
            completed = true;
            leave_menu = true;
        }
    }

    smith_ui_snapshot_scene_close(&scope);
    return completed;
}

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

static bool smith_ui_reforge_build_scene(app_ui_scene* scene,
    const smith_ui_reforge_menu_state* state, int highlight)
{
    app_ui_panel* panel;

    if (!scene || !state)
        return false;

    if (highlight < 1 || highlight > state->entry_count)
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
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Reforge: choose prefix");
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 selects, 8/2 moves, a-z jumps, Esc/4 cancels.");
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
        state->entry_count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    if (state->entry_count <= 0)
    {
        if (!app_ui_panel_add_row(panel, 0, TERM_SLATE, true, false, "",
                "Nothing available.", ""))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < state->entry_count; i++)
        {
            char key[APP_UI_KEY_MAX];
            char label[APP_UI_LABEL_MAX];
            byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE : state->row_attr[i];

            ego_name_for_enchant_menu(state->choice[i], label, sizeof(label));
            key[0] = '\0';
            if (i < 26)
                strnfmt(key, sizeof(key), "%c", (char)('a' + i));
            if (!app_ui_panel_add_row(panel, (s16b)(i + 1), row_attr,
                    state->valid[i], i + 1 == highlight, key, label, ""))
            {
                return false;
            }
        }
    }

    return smith_ui_reforge_add_selected_detail(panel, state, highlight);
}

static int smith_ui_reforge_prefix_snapshot_menu(const object_type* source)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!source || !source->k_idx)
        return 0;
    if (!smith_ui_snapshot_scene_enter(&scope))
        return 0;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_reforge_menu_state state;
        int choice;
        char ch;

        smith_ui_reforge_build_state(&state, source);
        if (state.entry_count > 0 && highlight > state.entry_count)
            highlight = state.entry_count;
        if (highlight < 1)
            highlight = 1;

        if (!smith_ui_reforge_build_scene(&scene, &state, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot reforge menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.entry_count);
        if (choice > 0)
            highlight = choice;
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (state.entry_count > 0)
            {
                if (highlight > 1)
                    highlight--;
                else
                    highlight = state.entry_count;
            }
            continue;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (state.entry_count > 0)
            {
                if (highlight < state.entry_count)
                    highlight++;
                else
                    highlight = 1;
            }
            continue;
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            break;
        }
        else if (!((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            ))
        {
            continue;
        }

        if ((highlight <= state.entry_count) && state.valid[highlight - 1])
        {
            int result = state.choice[highlight - 1];

            smith_ui_snapshot_scene_close(&scope);
            return result;
        }

        bell("You cannot afford that reforge.");
    }

    smith_ui_snapshot_scene_close(&scope);
    return 0;
}

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

static bool smith_ui_artefact_flag_build_scene(app_ui_scene* scene,
    const smith_ui_artefact_flag_menu_state* state, int category, int highlight)
{
    app_ui_panel* panel;

    if (!scene || !state)
        return false;
    if (highlight < 1 || highlight > state->count)
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
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        smithing_flag_cats[category - 1].desc);
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 toggles, 8/2 moves, a-z jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Toggle");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Toggle");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-z", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        state->count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    if (state->count <= 0)
    {
        if (!app_ui_panel_add_row(panel, 0, TERM_SLATE, true, false, "",
                "Nothing available.", ""))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < state->count; i++)
        {
            char key[APP_UI_KEY_MAX];
            byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE
                                                 : state->row_attr[i];

            key[0] = '\0';
            if (i < 26)
                strnfmt(key, sizeof(key), "%c", (char)('a' + i));
            if (!app_ui_panel_add_row(panel, (s16b)(i + 1), row_attr,
                    state->valid[i], i + 1 == highlight, key,
                    state->labels[i], ""))
            {
                return false;
            }
        }
    }

    return smith_ui_artefact_flag_add_selected_detail(panel, state, highlight);
}

static void smith_ui_artefact_flag_snapshot_menu(int category)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_artefact_flag_menu_state state;
        int choice;
        char ch;

        smith_ui_artefact_flag_build_state(&state, category);
        if (state.count > 0 && highlight > state.count)
            highlight = state.count;
        if (highlight < 1)
            highlight = 1;

        if (!smith_ui_artefact_flag_build_scene(&scene, &state, category,
                highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot artefact flag menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.count);
        if (choice > 0)
            highlight = choice;
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (state.count > 0)
            {
                if (highlight > 1)
                    highlight--;
                else
                    highlight = state.count;
            }
            continue;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (state.count > 0)
            {
                if (highlight < state.count)
                    highlight++;
                else
                    highlight = 1;
            }
            continue;
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            prepare_artefact();
            break;
        }
        else if (!((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            ))
        {
            continue;
        }

        if (state.count > 0 && state.valid[highlight - 1])
        {
            if (state.present[highlight - 1])
                remove_artefact_flag(state.flags[highlight - 1],
                    state.flagsets[highlight - 1]);
            else
                add_artefact_flag(state.flags[highlight - 1],
                    state.flagsets[highlight - 1]);
            smith_ui_artefact_backup_current_state();
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    smith_ui_snapshot_scene_close(&scope);
}

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

static bool smith_ui_artefact_ability_build_scene(app_ui_scene* scene,
    const smith_ui_artefact_ability_menu_state* state, int skill, int highlight)
{
    app_ui_panel* panel;

    if (!scene || !state)
        return false;
    if (highlight < 1 || highlight > state->count)
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
    app_ui_panel_set_subtitle(panel, TERM_SLATE, skill_names_full[skill]);
    (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
        "Enter/Space/6 toggles, 8/2 moves, a-z jumps, Esc/4 backs out.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Toggle");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Toggle");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "a-z", "Jump");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Back");
    app_ui_panel_set_row_offset(panel, (s16b)smith_ui_row_scroll_offset(
        state->count, highlight, SMITH_UI_BROWSER_ROW_WINDOW));

    if (state->count <= 0)
    {
        if (!app_ui_panel_add_row(panel, 0, TERM_SLATE, true, false, "",
                "Nothing available.", ""))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < state->count; i++)
        {
            char key[APP_UI_KEY_MAX];
            byte row_attr = (i + 1 == highlight) ? TERM_L_BLUE
                                                 : state->row_attr[i];

            key[0] = '\0';
            if (i < 26)
                strnfmt(key, sizeof(key), "%c", (char)('a' + i));
            if (!app_ui_panel_add_row(panel, (s16b)(i + 1), row_attr,
                    state->valid[i], i + 1 == highlight, key,
                    state->labels[i], ""))
            {
                return false;
            }
        }
    }

    return smith_ui_artefact_ability_add_selected_detail(panel, state, skill,
        highlight);
}

static void smith_ui_artefact_ability_snapshot_menu(int skill)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;

    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    while (true)
    {
        app_ui_scene scene;
        smith_ui_artefact_ability_menu_state state;
        int choice;
        char ch;

        smith_ui_artefact_ability_build_state(&state, skill);
        if (state.count > 0 && highlight > state.count)
            highlight = state.count;
        if (highlight < 1)
            highlight = 1;

        if (!smith_ui_artefact_ability_build_scene(&scene, &state, skill,
                highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot artefact ability menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, state.count);
        if (choice > 0)
            highlight = choice;
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (state.count > 0)
            {
                if (highlight > 1)
                    highlight--;
                else
                    highlight = state.count;
            }
            continue;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (state.count > 0)
            {
                if (highlight < state.count)
                    highlight++;
                else
                    highlight = 1;
            }
            continue;
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            prepare_artefact();
            break;
        }
        else if (!((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            ))
        {
            continue;
        }

        if (state.count > 0 && state.valid[highlight - 1])
        {
            if (state.present[highlight - 1])
                remove_artefact_ability(skill, state.ability_nums[highlight - 1]);
            else
                add_artefact_ability(skill, state.ability_nums[highlight - 1]);
            smith_ui_artefact_backup_current_state();
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    smith_ui_snapshot_scene_close(&scope);
}

static void smith_ui_artefact_snapshot_menu(void)
{
    smith_ui_snapshot_scope scope;
    int highlight = 1;
    bool leave_menu = false;
    char buf[36];

    log_info("Player opened artifact creation menu");
    if (!smith_ui_snapshot_scene_enter(&scope))
        return;

    if (!smith_o_ptr->name1)
    {
        log_debug("Initializing new artifact creation");
        artefact_wipe(smith_a_name);
        artefact_wipe(smith2_a_name);
        smith2_a_ptr->flags3 |= (TR3_IGNORE_MASK);

        if (smith_o_ptr->tval == TV_RING)
        {
            create_base_object(TV_RING, SV_RING_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
        }
        if (smith_o_ptr->tval == TV_AMULET)
        {
            create_base_object(TV_AMULET, SV_AMULET_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
            smith2_o_ptr->pd = 1;
        }
    }

    if (strlen(smith2_a_ptr->name) == 0)
    {
        sprintf(buf, "of %s", op_ptr->full_name);
        SDL_strlcpy(smith2_a_ptr->name, buf, MAX_LEN_ART_NAME);
    }

    prepare_artefact();
    smith_ui_artefact_backup_current_state();

    while (!leave_menu)
    {
        app_ui_scene scene;
        int count = smith_ui_artefact_root_entry_count();
        int choice;
        char ch;

        prepare_artefact();
        if (highlight < 1 || highlight > count)
            highlight = 1;
        if (!smith_ui_artefact_root_build_scene(&scene, highlight)
            || !smith_ui_snapshot_scene_present(&scope, &scene))
        {
            log_warn("smithing snapshot artefact menu: failed to build or publish semantic scene");
            break;
        }

        ch = smith_ui_inkey_with_wait_reason();
        choice = smith_ui_base_item_hotkey_choice(ch, count);
        if (choice > 0)
            highlight = choice;
        else if ((ch == '8')
#ifdef ARROW_UP
            || (ch == ARROW_UP)
#endif
            )
        {
            if (highlight > 1)
                highlight--;
            else
                highlight = count;
            continue;
        }
        else if ((ch == '2')
#ifdef ARROW_DOWN
            || (ch == ARROW_DOWN)
#endif
            )
        {
            if (highlight < count)
                highlight++;
            else
                highlight = 1;
            continue;
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            leave_menu = true;
            continue;
        }
        else if (!((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            ))
        {
            continue;
        }

        if (highlight == count)
        {
            rename_artefact();
            smith_ui_artefact_backup_current_state();
        }
        else if (highlight <= MAX_CATS)
        {
            smith_ui_snapshot_begin_nested_transition();
            smith_ui_snapshot_scene_close(&scope);
            smith_ui_artefact_flag_snapshot_menu(highlight);
            smith_ui_snapshot_end_nested_transition();
            if (!smith_ui_snapshot_scene_enter(&scope))
                return;
        }
        else
        {
            int skill = smith_ui_artefact_root_skill(highlight);

            if (skill >= 0)
            {
                smith_ui_snapshot_begin_nested_transition();
                smith_ui_snapshot_scene_close(&scope);
                smith_ui_artefact_ability_snapshot_menu(skill);
                smith_ui_snapshot_end_nested_transition();
                if (!smith_ui_snapshot_scene_enter(&scope))
                    return;
            }
        }
    }

    prepare_artefact();
    smith_ui_snapshot_scene_close(&scope);
}

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

