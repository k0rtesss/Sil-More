/* File: cmd-pickup.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-ui.h"
#include "app/app-session.h"
#include "externs.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"
#include "log/log.h"
#include "player/killer.h"
#include "runtime-cli.h"
#include "metarun.h"
#include <math.h>

void give_player_item(object_type * o_ptr)
{
    char o_name[80];
    object_type copy = *o_ptr;
    int source_y = o_ptr ? o_ptr->iy : -1;
    int source_x = o_ptr ? o_ptr->ix : -1;

    int slot = inven_carry(o_ptr, true);

    if (slot == SUPPLIES_INDEX)
    {
        object_desc(o_name, sizeof(o_name), &copy, true, 3);
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("You add %s to your supplies (%c).", o_name, label);
        sound(MSG_PICK);
        return;
    }

    if (slot < 0)
        return;
    
    /* Play pickup sound */
    sound(MSG_PICK);

    /* reset the pointer to the new location to pick up the count of the item
       in the inventory */
    o_ptr = &inventory[slot];

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    msg_format("You have %s (%c).", o_name, index_to_label(slot));
    app_session_note_animation(app_session_current(),
        APP_ANIMATION_HINT_OBJECT_TRANSFER, copy.k_idx,
        APP_PACK_COORD(source_y, source_x), slot, copy.number,
        APP_SNAPSHOT_INVALIDATE_MAP | APP_SNAPSHOT_INVALIDATE_STATUS);

    /* Update quiver display if this was a throwing weapon or arrow */
    if ((slot == INVEN_QUIVER1 || slot == INVEN_QUIVER2) ||
        (copy.tval == TV_ARROW))
    {
        p_ptr->redraw |= (PR_QUIVER);
    }
}

bool is_weapon_or_armor(const object_type* o_ptr)
{
    /* Check if it's a weapon */
    if (o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM || 
        o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_BOW)
        return true;
        
    /* Check if it's armor */
    if (o_ptr->tval == TV_SOFT_ARMOR || o_ptr->tval == TV_MAIL || 
        o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_CLOAK || 
        o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return true;
        
    return false;
}

bool smith_oath_forbids_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return chosen_oath(OATH_SMITH) && !oath_invalid(OATH_SMITH)
        && is_weapon_or_armor(o_ptr) && !is_smithed_by_player(o_ptr);
}

bool smith_oath_confirm_break(void)
{
    char* prompt;

    if (!chosen_oath(OATH_SMITH) || oath_invalid(OATH_SMITH))
        return true;

    prompt = oath_confirmation_prompt(OATH_SMITH);
    if (!prompt || !prompt[0])
        prompt = "Are you certain you wish to break your Oath of the Smith?";

    if (!get_check_oath_multiline(prompt))
        return false;

    p_ptr->oaths_broken |= OATH_SMITH_FLAG;
    apply_oath_breaking_curse(OATH_SMITH);
    return true;
}

/*
 * Check if an object was smithed by the player
 */
static const object_type* replacement_filter_incoming = NULL;
static bool item_tester_limit_group(const object_type* o_ptr);

static bool pack_item_matches_replacement_type(const object_type* incoming,
                                               const object_type* candidate)
{
    if (!incoming || !candidate || !candidate->k_idx)
        return false;

    if (incoming->tval == candidate->tval)
        return true;

    int incoming_slot = wield_slot(incoming);
    if (incoming_slot >= INVEN_WIELD && incoming_slot < INVEN_TOTAL)
    {
        int candidate_slot = wield_slot(candidate);
        if (candidate_slot == incoming_slot)
            return true;
    }

    return false;
}

static void format_staff_prompt_name(char* buf, size_t max,
                                     const object_type* o_ptr, bool pref)
{
    char full[80];
    const char* staff_of;

    if (!buf || max == 0)
        return;

    buf[0] = '\0';

    if (!o_ptr || !o_ptr->k_idx)
        return;

    object_desc(full, sizeof(full), o_ptr, pref, 0);

    if (o_ptr->tval != TV_STAFF)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    staff_of = strstr(full, "Staff of ");
    if (!staff_of)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    if (!pref)
    {
        SDL_strlcpy(buf, staff_of, max);
        return;
    }

    if (!strncmp(full, "The ", 4))
        strnfmt(buf, max, "The %s", staff_of);
    else if (!strncmp(full, "no more ", 8))
        strnfmt(buf, max, "no more %s", staff_of);
    else
        strnfmt(buf, max, "a %s", staff_of);
}

typedef struct pickup_pile_scene_scope {
    bool active;
    app_snapshot previous_snapshot;
} pickup_pile_scene_scope;

typedef struct pickup_pile_snapshot_entry {
    int floor_slot;
    int floor_o_idx;
    byte attr;
    byte icon_attr;
    char icon_char;
    char key[APP_UI_KEY_MAX];
    char label[APP_UI_LABEL_MAX];
    char meta[APP_UI_META_MAX];
} pickup_pile_snapshot_entry;

typedef struct pickup_pile_snapshot_state {
    pickup_pile_snapshot_entry entries[MAX_FLOOR_STACK];
    int entry_count;
} pickup_pile_snapshot_state;

static byte pickup_pile_row_attr(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return TERM_L_DARK;

    if (weapon_glows(o_ptr))
        return object_display_color(o_ptr, TERM_L_BLUE);

    return object_display_color(o_ptr,
        tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
}

static void pickup_pile_format_weight(char* buf, size_t buf_size,
                                      const object_type* o_ptr)
{
    int wgt;

    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (!show_weights || !o_ptr || !o_ptr->k_idx || !o_ptr->weight)
        return;

    wgt = o_ptr->weight * o_ptr->number;
    strnfmt(buf, buf_size, "%2d.%1d lb", wgt / 10, wgt % 10);
}

static void pickup_pile_format_meta(char* buf, size_t buf_size,
                                    const object_type* o_ptr, int floor_slot)
{
    char weight_buf[16];
    char tag_buf[8];

    if (!buf || !buf_size)
        return;

    pickup_pile_format_weight(weight_buf, sizeof(weight_buf), o_ptr);
    strnfmt(tag_buf, sizeof(tag_buf), "(%c)", index_to_label(floor_slot));

    if (weight_buf[0])
        strnfmt(buf, buf_size, "%s  %s", weight_buf, tag_buf);
    else
        SDL_strlcpy(buf, tag_buf, buf_size);
}

static bool pickup_pile_prompt_for_floor_item(cptr prompt, int floor_o_idx)
{
    char o_name[80];
    char out_val[160];
    object_type* o_ptr;

    if (floor_o_idx <= 0)
        return false;

    o_ptr = &o_list[floor_o_idx];
    object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
    strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt, o_name);
    return get_check(out_val);
}

static bool pickup_pile_allow_floor_item(int floor_o_idx)
{
    cptr note_text;
    cptr marker;
    object_type* o_ptr;

    if (floor_o_idx <= 0)
        return false;

    o_ptr = &o_list[floor_o_idx];
    if (!o_ptr->obj_note)
        return true;

    note_text = quark_str(o_ptr->obj_note);
    marker = strchr(note_text, '!');

    while (marker)
    {
        if ((marker[1] == p_ptr->command_cmd) || (marker[1] == '*'))
        {
            if (!pickup_pile_prompt_for_floor_item("Really try", floor_o_idx))
                return false;
        }

        marker = strchr(marker + 1, '!');
    }

    return true;
}

static bool pickup_pile_scene_enter(pickup_pile_scene_scope* scope)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!scope)
        return false;

    memset(scope, 0, sizeof(*scope));
    if (!runtime_cli_snapshot_renderer() || !session)
        return false;

    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return false;

    app_session_clear_interaction(session);
    scope->previous_snapshot = *snapshot;
    scope->active = true;
    return true;
}

static bool pickup_pile_scene_present(pickup_pile_scene_scope* scope,
                                      const app_ui_scene* scene)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !scene || !session)
        return false;
    if (!app_session_publish_menu_scene(session, scene))
        return false;

    (void)Term_xtra(TERM_XTRA_FRESH, 0);
    return true;
}

static void pickup_pile_scene_restore(pickup_pile_scene_scope* scope,
                                      bool refresh)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session)
        return;

    app_session_set_snapshot(session, &scope->previous_snapshot);
    if (refresh)
        (void)Term_xtra(TERM_XTRA_FRESH, 0);
}

static void pickup_pile_scene_capture(pickup_pile_scene_scope* scope)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!scope || !scope->active || !session)
        return;

    snapshot = app_session_snapshot(session);
    if (snapshot && snapshot->scene == APP_SCENE_KIND_DUNGEON)
        scope->previous_snapshot = *snapshot;
}

static void pickup_pile_scene_suspend(pickup_pile_scene_scope* scope)
{
    app_session* session = app_session_current();

    pickup_pile_scene_restore(scope, false);
    if (session)
        app_session_clear_interaction(session);
}

static void pickup_pile_scene_close(pickup_pile_scene_scope* scope)
{
    if (!scope)
        return;

    pickup_pile_scene_restore(scope, true);
    scope->active = false;
}

static void pickup_pile_build_snapshot_state(
    pickup_pile_snapshot_state* state, const int* floor_list, int floor_num)
{
    int i;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    if (!floor_list)
        return;

    for (i = 0; i < floor_num && state->entry_count < MAX_FLOOR_STACK; i++)
    {
        pickup_pile_snapshot_entry* entry;
        object_type* o_ptr = &o_list[floor_list[i]];

        if (!item_tester_okay(o_ptr))
            continue;

        entry = &state->entries[state->entry_count++];
        memset(entry, 0, sizeof(*entry));
        entry->floor_slot = i;
        entry->floor_o_idx = floor_list[i];
        entry->attr = pickup_pile_row_attr(o_ptr);
        entry->icon_attr = object_attr(o_ptr);
        entry->icon_char = object_char(o_ptr);
        strnfmt(entry->key, sizeof(entry->key), "%c", index_to_label(i));
        object_desc_floor(entry->label, sizeof(entry->label), o_ptr, true, 3);
        pickup_pile_format_meta(entry->meta, sizeof(entry->meta), o_ptr, i);
    }
}

static int pickup_pile_clamp_highlight(const pickup_pile_snapshot_state* state,
                                       int highlight_row)
{
    if (!state || state->entry_count <= 0)
        return -1;

    if (highlight_row < 0 || highlight_row >= state->entry_count)
        return 0;

    return highlight_row;
}

static bool pickup_pile_build_ui_scene(app_ui_scene* scene,
                                       const pickup_pile_snapshot_state* state,
                                       int highlight_row)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];

    if (!scene || !state)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 760, 1220);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Pick Up Objects");
    strnfmt(subtitle, sizeof(subtitle), "%d object%s on the floor",
        state->entry_count, (state->entry_count == 1) ? "" : "s");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, subtitle);
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Enter/Space picks up, x examines, 8/2 moves.");
    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Pick Up");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Pick Up");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "x", "Examine");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "Esc", "Cancel");

    if (state->entry_count <= 0)
    {
        return app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "No pickable objects remain on this tile.");
    }

    for (int i = 0; i < state->entry_count; i++)
    {
        const pickup_pile_snapshot_entry* entry = &state->entries[i];

        if (!app_ui_panel_add_row_ex(panel, (s16b)(0 - entry->floor_o_idx),
                (highlight_row == i) ? TERM_L_BLUE : entry->attr,
                (highlight_row == i) ? TERM_L_BLUE : entry->attr,
                entry->icon_attr, entry->icon_char, true,
                highlight_row == i, entry->key, entry->label, entry->meta))
        {
            return false;
        }
    }

    if (highlight_row >= 0 && highlight_row < state->entry_count)
    {
        const pickup_pile_snapshot_entry* entry = &state->entries[highlight_row];
        object_type* o_ptr = &o_list[entry->floor_o_idx];
        char detail[APP_UI_TEXT_MAX];
        char weight_buf[16];

        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, entry->label);
        strnfmt(detail, sizeof(detail), "Hotkey: %c",
            index_to_label(entry->floor_slot));
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, detail);

        pickup_pile_format_weight(weight_buf, sizeof(weight_buf), o_ptr);
        if (weight_buf[0])
        {
            strnfmt(detail, sizeof(detail), "Weight: %s", weight_buf);
            (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, detail);
        }

        if (supplies_is_supply_object(o_ptr))
        {
            (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
                "Supply items can merge directly into your cache.");
        }

        if (smith_oath_forbids_object(o_ptr))
        {
            (void)app_ui_panel_add_detail_line(panel, TERM_L_RED,
                "Picking this up will break your Oath of the Smith.");
        }

        if (p_ptr->total_weight + o_ptr->weight > weight_limit() * 3 / 2)
        {
            (void)app_ui_panel_add_detail_line(panel, TERM_L_RED,
                "You cannot lift it at your current carrying weight.");
        }

        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "Use x to inspect the item without picking it up.");
    }

    return true;
}

static int pickup_pile_find_hotkey_entry(
    const pickup_pile_snapshot_state* state, char which)
{
    int i;

    if (!state)
        return -1;

    for (i = 0; i < state->entry_count; i++)
    {
        if (index_to_label(state->entries[i].floor_slot) == which)
            return i;
    }

    return -1;
}

bool is_smithed_by_player(const object_type* o_ptr)
{
    return (o_ptr->unused1 != 0);
}

static bool pickup_choice_is_valid_pack_item(int item)
{
    if ((item < 0) || (item >= INVEN_PACK))
    {
        bell("Illegal object choice!");
        return false;
    }

    if (!inventory[item].k_idx)
    {
        bell("That slot is empty.");
        return false;
    }

    return true;
}

static bool pickup_select_pack_item(int* item, cptr prompt, cptr empty_prompt,
                                    bool (*hook)(const object_type*),
                                    const object_type* incoming)
{
    bool selected;
    bool old_item_tester_full = item_tester_full;
    byte old_item_tester_tval = item_tester_tval;
    bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
    const object_type* old_filter = replacement_filter_incoming;

    if (!item)
        return false;

    if (hook)
    {
        replacement_filter_incoming = incoming;
        item_tester_tval = 0;
        item_tester_hook = hook;
        item_tester_full = false;
    }

    selected = get_item(item, prompt, empty_prompt, USE_INVEN);

    replacement_filter_incoming = old_filter;
    item_tester_hook = old_item_tester_hook;
    item_tester_tval = old_item_tester_tval;
    item_tester_full = old_item_tester_full;

    if (!selected)
        return false;

    return pickup_choice_is_valid_pack_item(*item);
}

/*
 * Prompt the player to drop an inventory item so a new object can be picked up.
 * Returns true if an item was dropped, false if the player declined or nothing was dropped.
 */
static bool prompt_replace_pack_item(const object_type* incoming)
{
    char incoming_name[80];
    char prompt[160];
    int item;

    object_desc(incoming_name, sizeof(incoming_name), incoming, true, 3);
    msg_format("No room for %s.", incoming_name);
    msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt), "Replace which item to pick up %s? ", incoming_name);

    if (!pickup_select_pack_item(&item, prompt,
            "You have nothing to replace.", NULL, NULL))
    {
        return false;
    }

    object_type* drop_ptr = &inventory[item];

    inven_drop(item, drop_ptr->number);

    /* Let inventory housekeeping run before we attempt the pickup again */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    notice_stuff();

    return true;
}

typedef enum
{
    PICKUP_FAILURE_ABORT = 0,
    PICKUP_FAILURE_RETRY,
    PICKUP_FAILURE_EQUIPPED
} pickup_failure_result;

static pickup_failure_result resolve_pickup_failure(object_type* incoming,
                                                    int floor_o_idx,
                                                    const char* incoming_name,
                                                    bool attempted_replacement);

/*
 * Helper routine for py_pickup() and py_pickup_floor().
 *
 * Add the given dungeon object to the character's inventory.
 *
 * Delete the object afterwards.
 */

void py_pickup_aux(int o_idx)
{
    object_type* o_ptr;
    char o_name[120];
    
    o_ptr = &o_list[o_idx];
    // Remember the floor position even if give_player_item wipes the object
    int pickup_y = o_ptr->iy;
    int pickup_x = o_ptr->ix;

    /*hack - don't pickup &nothings*/
    if (o_ptr->k_idx)
    {
        /* Check for Oath of the Smith violation */
        if (smith_oath_forbids_object(o_ptr))
        {
            if (!smith_oath_confirm_break())
                return;
        }

        /* Check for supply items with partial pickup option */
        if (supplies_is_supply_object(o_ptr) && o_ptr->number > 1)
        {
            int max_qty = supplies_max_absorbable_quantity(o_ptr);
            
            /* If we can't absorb all of it but can absorb some, offer partial pickup */
            if (max_qty > 0 && max_qty < o_ptr->number)
            {
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                
                char prompt[160];
                strnfmt(prompt, sizeof(prompt), 
                        "Your supply cache can only hold %d of %d. Pick up how many? (0-%d): ",
                        max_qty, o_ptr->number, max_qty);
                
                int qty = get_quantity(prompt, max_qty);
                
                if (qty <= 0)
                {
                    msg_print("You leave it on the ground.");
                    return;
                }
                
                /* Create a partial object to pick up */
                object_type partial;
                object_copy(&partial, o_ptr);
                partial.number = qty;
                
                give_player_item(&partial);
                
                /* Reduce the floor object */
                o_ptr->number -= qty;
                
                /* Break the truce if creatures see */
                break_truce(false);
                
                return;
            }
        }
        
        give_player_item(o_ptr);

        // Break the truce if creatures see
        break_truce(false);

        if (!o_ptr->k_idx || o_ptr->number <= 0)
        {
            if (!o_ptr->k_idx)
            {
                o_ptr->iy = pickup_y;
                o_ptr->ix = pickup_x;
            }
            delete_object_idx(o_idx);
        }

        return;
    }

    /* Delete the object */
    o_ptr->iy = pickup_y;
    o_ptr->ix = pickup_x;
    delete_object_idx(o_idx);
}

static bool pickup_try_channel_floor_staff(object_type* o_ptr, int floor_o_idx)
{
    int target_slot = -1;
    object_type* target = NULL;

    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_STAFF || o_ptr->pval <= 0
        || !p_ptr->active_ability[S_WIL][WIL_CHANNELING])
    {
        return false;
    }

    object_type* wielded = &inventory[INVEN_STAFF];
    if (wielded->k_idx && wielded->k_idx == o_ptr->k_idx)
    {
        target = wielded;
        target_slot = INVEN_STAFF;
    }

    if (!target)
    {
        for (int i = 0; i < INVEN_PACK; i++)
        {
            object_type* pack_obj = &inventory[i];

            if (!pack_obj->k_idx)
                continue;
            if (pack_obj->tval != TV_STAFF)
                continue;
            if (pack_obj->k_idx != o_ptr->k_idx)
                continue;

            target = pack_obj;
            target_slot = i;
            break;
        }
    }

    if (target)
    {
        int mult = CHANNELING_CHARGE_MULTIPLIER;
        int existing_raw = MAX(target->pval, 0);
        int donor_raw = MAX(o_ptr->pval, 0);
        int existing_uses = existing_raw / mult;
        int donor_uses = donor_raw / mult;

        if (donor_uses > 0)
        {
            double existing_term = pow((double)existing_uses, 1.5);
            double donor_term = pow((double)donor_uses, 1.5);
            double combined_uses_raw = 0.0;
            double sum_terms = existing_term + donor_term;

            if (sum_terms > 0.0)
                combined_uses_raw = pow(sum_terms, 2.0 / 3.0);

            int combined_uses = (int)(combined_uses_raw + 0.5);
            long combined_pval = (long)combined_uses * mult;
            long max_pval = (long)(32767 / mult) * mult;

            if (combined_pval > max_pval)
                combined_pval = max_pval;

            combined_uses = (int)(combined_pval / mult);

            int gain_uses = combined_uses - existing_uses;
            if (gain_uses > 0)
            {
                char target_name[80];
                char donor_name[80];
                char prompt[120];

                format_staff_prompt_name(
                    target_name, sizeof(target_name), target, false);
                format_staff_prompt_name(
                    donor_name, sizeof(donor_name), o_ptr, true);

                log_debug("Channeling: donor floor staff k_idx=%d pval=%d number=%d, target inv slot %d k_idx=%d pval=%d number=%d",
                    o_ptr->k_idx, o_ptr->pval, o_ptr->number, target_slot,
                    target->k_idx, target->pval, target->number);

                strnfmt(prompt, sizeof(prompt),
                    "Channel %s into your %s (%d charges)?", donor_name,
                    target_name, combined_uses);
                if (get_check(prompt))
                {
                    target->pval = (s16b)combined_pval;
                    target->ident &= ~(IDENT_EMPTY);
                    o_ptr->pval = 0;
                    o_ptr->ident |= IDENT_EMPTY;

                    log_debug("Channeling complete: target now has pval=%d number=%d, donor has pval=%d number=%d",
                        target->pval, target->number, o_ptr->pval,
                        o_ptr->number);

                    if (target_slot >= 0 && target_slot < INVEN_TOTAL)
                        inven_item_charges(target_slot);
                    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
                    p_ptr->window |= (PW_EQUIP | PW_PLAYER_0 | PW_INVEN);
                    msg_format("You channel %d charge%s into your %s (now %d).",
                        gain_uses, (gain_uses == 1) ? "" : "s", target_name,
                        combined_uses);
                    delete_object_idx(floor_o_idx);

                    log_debug("Channeling: deleted floor object idx %d",
                        floor_o_idx);

                    p_ptr->previous_action[0] = ACTION_MISC;
                    p_ptr->energy_use = 100;
                    return true;
                }
            }
        }
    }

    return false;
}

static bool pickup_handle_floor_object(int floor_o_idx,
                                       bool consume_pickup_turn)
{
    object_type* o_ptr = &o_list[floor_o_idx];
    char o_name[80];
    bool attempted_replacement = false;

    if (!o_ptr->k_idx)
        return false;

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    if (pickup_try_channel_floor_staff(o_ptr, floor_o_idx))
        return true;

    while (!inven_carry_okay(o_ptr))
    {
        pickup_failure_result failure = resolve_pickup_failure(
            o_ptr, floor_o_idx, o_name, attempted_replacement);

        if (failure == PICKUP_FAILURE_RETRY)
        {
            attempted_replacement = true;
            continue;
        }

        return (failure == PICKUP_FAILURE_EQUIPPED);
    }

    if (p_ptr->total_weight + o_ptr->weight > weight_limit() * 3 / 2)
    {
        msg_format("You cannot lift %s.", o_name);
        return false;
    }

    if (consume_pickup_turn)
    {
        p_ptr->previous_action[0] = ACTION_MISC;
        p_ptr->energy_use = 100;
    }

    py_pickup_aux(floor_o_idx);
    return true;
}

static bool pickup_run_snapshot_pile_menu(bool* out_picked_up_item)
{
    pickup_pile_scene_scope scene_scope;
    app_wait_scope wait_scope;
    bool picked_up_item = false;
    int highlight_row = -1;

    if (out_picked_up_item)
        *out_picked_up_item = false;

    if (!pickup_pile_scene_enter(&scene_scope))
        return false;

    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_LIST_SELECTION, 0, 0);

    while (true)
    {
        int floor_list[MAX_FLOOR_STACK];
        int floor_num;
        pickup_pile_snapshot_state state;
        app_ui_scene scene;
        char command = '\0';

        handle_stuff();
        pickup_pile_scene_capture(&scene_scope);

        floor_num = scan_floor(
            floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);
        pickup_pile_build_snapshot_state(&state, floor_list, floor_num);

        if (state.entry_count < 1)
        {
            if (picked_up_item)
                msg_print("There are no more objects where you are standing.");
            else
                msg_print("There are no objects where you are standing.");
            break;
        }

        highlight_row = pickup_pile_clamp_highlight(&state, highlight_row);
        if (!pickup_pile_build_ui_scene(&scene, &state, highlight_row))
        {
            log_warn("pickup pile: failed to build snapshot UI scene");
            app_session_pop_wait_scope(app_session_current(), &wait_scope);
            pickup_pile_scene_close(&scene_scope);
            return false;
        }
        if (!pickup_pile_scene_present(&scene_scope, &scene))
        {
            log_warn("pickup pile: failed to present snapshot UI scene");
            app_session_pop_wait_scope(app_session_current(), &wait_scope);
            pickup_pile_scene_close(&scene_scope);
            return false;
        }

        if (!get_com("Pick up command: ", &command))
            break;

        switch (command)
        {
        case '8':
#ifdef ARROW_UP
        case ARROW_UP:
#endif
            if (state.entry_count > 0)
            {
                highlight_row = (highlight_row + state.entry_count - 1)
                    % state.entry_count;
            }
            break;

        case '2':
#ifdef ARROW_DOWN
        case ARROW_DOWN:
#endif
            if (state.entry_count > 0)
                highlight_row = (highlight_row + 1) % state.entry_count;
            break;

        case 'x':
        case 'X':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
            if (highlight_row >= 0 && highlight_row < state.entry_count)
            {
                pickup_pile_scene_suspend(&scene_scope);
                describe_item_with_comparisons(
                    0 - state.entries[highlight_row].floor_o_idx, true);
            }
            else
            {
                bell("No highlighted item to examine.");
            }
            break;

        case ' ':
        case '\n':
        case '\r':
        case '-':
            if (highlight_row >= 0 && highlight_row < state.entry_count)
            {
                int floor_o_idx = state.entries[highlight_row].floor_o_idx;

                pickup_pile_scene_suspend(&scene_scope);
                if (!pickup_pile_allow_floor_item(floor_o_idx))
                    break;
                if (pickup_handle_floor_object(floor_o_idx, false))
                    picked_up_item = true;
            }
            else
            {
                bell("No highlighted item to pick up.");
            }
            break;

        default:
            if (isalpha((unsigned char)command))
            {
                bool verify = isupper((unsigned char)command) ? true : false;
                int entry_index = pickup_pile_find_hotkey_entry(&state,
                    (char)tolower((unsigned char)command));

                if (entry_index < 0)
                {
                    bell("Illegal object choice!");
                    break;
                }

                highlight_row = entry_index;
                pickup_pile_scene_suspend(&scene_scope);
                if (verify
                    && !pickup_pile_prompt_for_floor_item("Try",
                        state.entries[entry_index].floor_o_idx))
                {
                    break;
                }
                if (!pickup_pile_allow_floor_item(
                        state.entries[entry_index].floor_o_idx))
                {
                    break;
                }
                if (pickup_handle_floor_object(
                        state.entries[entry_index].floor_o_idx, false))
                {
                    picked_up_item = true;
                }
            }
            else
            {
                bell("Invalid command!");
            }
            break;
        }
    }

    app_session_pop_wait_scope(app_session_current(), &wait_scope);
    pickup_pile_scene_close(&scene_scope);

    if (out_picked_up_item)
        *out_picked_up_item = picked_up_item;

    return true;
}

/*
 * Allow the player to sort through items in a pile and
 * pickup what they want.  This command does not use
 * any energy because it costs a player no extra energy
 * to walk into a grid and automatically pick up items
 */
void do_cmd_pickup_from_pile(void)
{
    bool picked_up_item = false;

    if (!pickup_run_snapshot_pile_menu(&picked_up_item))
    {
        log_warn("pickup pile: snapshot menu required; legacy fallback removed");
        msg_print("Pile pickup requires active snapshot UI rendering.");
        return;
    }

    /* Combine / Reorder the pack */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);

    /* Just be sure all inventory management is done. */
    notice_stuff();
}

static void report_pack_limit_failure(const char* o_name, bool still)
{
    if (inven_carry_limit_failed())
    {
        cptr label = inven_carry_limit_label();
        int limit = inven_carry_limit_value();

        if (label)
        {
            /* Special message for supply weight limit */
            if (strcmp(label, "supply weight") == 0)
            {
                msg_format("Your supply cache cannot carry any more weight (limit %d lbs).",
                           limit);
                return;
            }

            if (still)
                msg_format("Your pack still cannot hold more %s (limit %d).", label,
                           limit);
            else
                msg_format("Your pack cannot hold more %s (limit %d).", label, limit);
            return;
        }
    }

    if (still)
        msg_format("You still have no room for %s.", o_name);
    else
        msg_format("You have no room for %s.", o_name);
}

static bool item_tester_limit_group(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (replacement_filter_incoming
        && !pack_item_matches_replacement_type(replacement_filter_incoming, o_ptr))
        return false;

    return inven_carry_limit_can_replace(o_ptr);
}

static bool pack_has_limit_candidates(const object_type* incoming)
{
    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* j_ptr = &inventory[i];

        if (!j_ptr->k_idx)
            continue;

        if (!inven_carry_limit_can_replace(j_ptr))
            continue;

        if (!pack_item_matches_replacement_type(incoming, j_ptr))
            continue;

        return true;
    }

    return false;
}

static bool prompt_replace_pack_item_limit(const object_type* incoming,
                                           const char* incoming_name)
{
    char prompt[160];
    cptr label = inven_carry_limit_label();
    int limit = inven_carry_limit_value();
    int item;

    if (label)
        msg_format("You already carry %s (limit %d).", label, limit);
    else
        msg_print("You cannot carry any more of those.");

    msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt),
            "Replace which item to pick up %s? ", incoming_name);

    if (!pickup_select_pack_item(&item, prompt,
            "You have nothing to replace.", item_tester_limit_group, incoming))
    {
        return false;
    }

    object_type* drop_ptr = &inventory[item];

    if (!inven_carry_limit_can_replace(drop_ptr))
    {
        msg_print("That will not make enough room.");
        return false;
    }

    inven_drop(item, drop_ptr->number);

    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    notice_stuff();

    return true;
}

static pickup_failure_result handle_zero_limit_pickup(object_type* incoming,
                                                      int floor_o_idx,
                                                      const char* incoming_name)
{
    int slot = wield_slot(incoming);

    msg_format("You cannot carry %s in your pack.", incoming_name);

    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
    {
        msg_print("It does not fit anywhere on your body.");
        return PICKUP_FAILURE_ABORT;
    }

    object_type* equip_ptr = &inventory[slot];

    if (!equip_ptr->k_idx)
    {
        if (get_check("Wear it now? "))
        {
            do_cmd_wield(incoming, 0 - floor_o_idx);
            return PICKUP_FAILURE_EQUIPPED;
        }

        msg_print("You leave it on the ground.");
        return PICKUP_FAILURE_ABORT;
    }

    if (cursed_p(equip_ptr))
    {
        char equipped_name[80];
        object_desc(equipped_name, sizeof(equipped_name), equip_ptr, true, 3);
        msg_format("You cannot remove %s.", equipped_name);
        return PICKUP_FAILURE_ABORT;
    }

    char equipped_name[80];
    object_desc(equipped_name, sizeof(equipped_name), equip_ptr, true, 3);

    char prompt[160];
    strnfmt(prompt, sizeof(prompt), "Replace %s with %s? ", equipped_name,
            incoming_name);

    if (get_check(prompt))
    {
        do_cmd_wield(incoming, 0 - floor_o_idx);
        return PICKUP_FAILURE_EQUIPPED;
    }

    msg_print("You decide to keep your current equipment.");
    return PICKUP_FAILURE_ABORT;
}

static pickup_failure_result handle_group_limit_pickup(object_type* incoming,
                                                       const char* incoming_name)
{
    if (!pack_has_limit_candidates(incoming))
        return PICKUP_FAILURE_ABORT;

    if (!prompt_replace_pack_item_limit(incoming, incoming_name))
        return PICKUP_FAILURE_ABORT;

    return PICKUP_FAILURE_RETRY;
}

static pickup_failure_result resolve_pickup_failure(object_type* incoming,
                                                    int floor_o_idx,
                                                    const char* incoming_name,
                                                    bool attempted_replacement)
{
    if (inven_carry_limit_failed())
    {
        if (inven_carry_limit_value() <= 0)
            return handle_zero_limit_pickup(incoming, floor_o_idx,
                                            incoming_name);

        pickup_failure_result limit_result =
            handle_group_limit_pickup(incoming, incoming_name);

        if (limit_result == PICKUP_FAILURE_ABORT)
            report_pack_limit_failure(incoming_name, attempted_replacement);

        return limit_result;
    }

    if (prompt_replace_pack_item(incoming))
        return PICKUP_FAILURE_RETRY;

    report_pack_limit_failure(incoming_name, attempted_replacement);
    return PICKUP_FAILURE_ABORT;
}

void py_pickup(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    bool done_pickup = false;

    s16b this_o_idx, next_o_idx = 0;

    object_type* o_ptr;

    /* Automatically destroy squelched items in pile if necessary */
    do_squelch_pile(py, px);

    /* Scan the pile of objects */
    for (this_o_idx = cave_o_idx[py][px]; this_o_idx; this_o_idx = next_o_idx)
    {
        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Hack -- disturb */
        disturb(0, 0);

        /* End loop if squelched stuff reached */
        if ((k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS)
            && (k_info[o_ptr->k_idx].aware))
        {
            next_o_idx = 0;
            continue;
        }

        if (pickup_handle_floor_object(this_o_idx, true))
            done_pickup = true;
    }

    if (!done_pickup)
    {
        p_ptr->previous_action[0] = ACTION_NOTHING;
        p_ptr->energy_use = 0;
    }
}

/*
 * Determine if a trap affects the player.
 * Based on player's evasion.
 */
