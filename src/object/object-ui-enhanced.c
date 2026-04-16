/* File: object-ui-enhanced.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-session.h"
#include "platform-frame.h"
#include "platform-input.h"
#include "object/object-desc.h"
#include "object/object-display.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "object/object-ui-enhanced.h"
#include "player/identification.h"
#include "supplies.h"
#include "ui/ui-information-scene.h"

#define ENHANCED_MAX_LIST 80
#define MAX_COMPARE_LINES 2

static void inventory_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static bool death_spectator_allow_menu_action(void)
{
    if (!death_spectator_active())
        return true;

    msg_print("You can no longer take that action.");
    return false;
}

int enhanced_menu_action = ENHANCED_ACTION_NONE;
int enhanced_inventory_selected_item = -1;
char current_menu_command = 0;
int current_menu_state = 0;
int enhanced_equip_action = ENHANCED_ACTION_NONE;
int enhanced_equipment_selected_item = -1;

static void append_compare_slot(int* slots, int* count, int slot);

typedef struct enhanced_item_snapshot_scope {
    bool active;
    app_input_capture_scope input_capture_scope;
} enhanced_item_snapshot_scope;

typedef struct enhanced_item_snapshot_entry {
    int item_index;
    bool is_floor;
    bool is_supply;
    byte attr;
    byte meta_attr;
    byte icon_attr;
    char icon_char;
    char key[APP_UI_KEY_MAX];
    char label[APP_UI_LABEL_MAX];
    char meta[APP_UI_META_MAX];
} enhanced_item_snapshot_entry;

typedef struct enhanced_item_snapshot_state {
    int mode;
    int entry_count;
    bool allow_compare;
    bool has_floor_items;
    int armour_weight;
    enhanced_item_snapshot_entry entries[ENHANCED_MAX_LIST];
} enhanced_item_snapshot_state;

static bool enhanced_item_snapshot_active(void)
{
    return app_session_interactions_enabled(app_session_current());
}

static bool enhanced_item_snapshot_scene_enter(
    enhanced_item_snapshot_scope* scope)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    if (!scope || !session)
        return false;

    memset(scope, 0, sizeof(*scope));
    snapshot = app_session_snapshot(session);
    if (!snapshot || snapshot->scene != APP_SCENE_KIND_DUNGEON)
        return false;

    app_session_clear_interaction(session);
    app_session_clear_dungeon_overlay_scene(session);
    app_session_push_input_capture(session, &scope->input_capture_scope);
    app_session_clear_inputs(session);
    scope->active = true;
    return true;
}

static void enhanced_item_snapshot_scene_suspend(
    enhanced_item_snapshot_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session)
        return;

    app_session_clear_inputs(session);
    app_session_clear_interaction(session);
    app_session_clear_dungeon_overlay_scene(session);
    app_session_pop_input_capture(session, &scope->input_capture_scope);
}

static void enhanced_item_snapshot_scene_close(
    enhanced_item_snapshot_scope* scope)
{
    if (!scope)
        return;

    enhanced_item_snapshot_scene_suspend(scope);
    scope->active = false;
}

static bool enhanced_item_snapshot_scene_present(
    enhanced_item_snapshot_scope* scope, const app_ui_scene* scene)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session || !scene)
        return false;
    if (!app_session_publish_dungeon_overlay_scene(session, scene))
        return false;

    platform_frame_present();
    return true;
}

static void enhanced_item_set_result(int mode, int action, int item_index)
{
    if (mode == (USE_EQUIP))
    {
        enhanced_equip_action = action;
        enhanced_equipment_selected_item = item_index;
    }
    else
    {
        enhanced_menu_action = action;
        enhanced_inventory_selected_item = item_index;
    }
}

static object_type* enhanced_item_object_for_index(int item_index,
    bool* floor_item)
{
    if (floor_item)
        *floor_item = false;

    if (item_index == SUPPLIES_INDEX)
        return NULL;
    if (item_index < 0)
    {
        int floor_index = 0 - item_index;

        if (floor_item)
            *floor_item = true;
        if (floor_index <= 0 || floor_index >= o_max)
            return NULL;
        return &o_list[floor_index];
    }
    if (item_index < 0 || item_index >= INVEN_TOTAL)
        return NULL;

    return &inventory[item_index];
}

static byte enhanced_item_row_attr(const object_type* o_ptr, bool empty_slot)
{
    if (!o_ptr || empty_slot || !o_ptr->k_idx)
        return TERM_L_DARK;

    if (weapon_glows(o_ptr))
        return object_display_color(o_ptr, TERM_L_BLUE);

    return object_display_color(o_ptr, object_default_text_color(o_ptr));
}

static void enhanced_item_format_weight(char* buf, size_t buf_size,
    const object_type* o_ptr)
{
    int weight = 0;

    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (!show_weights || !o_ptr || !o_ptr->k_idx)
        return;

    weight = o_ptr->weight * o_ptr->number;
    strnfmt(buf, buf_size, "%2d.%1d lb", weight / 10, weight % 10);
}

static void enhanced_item_format_total_weight(char* buf, size_t buf_size,
    int total_weight)
{
    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (!show_weights)
        return;

    strnfmt(buf, buf_size, "%2d.%1d lb", total_weight / 10,
        total_weight % 10);
}

static int enhanced_item_clamp_highlight(
    const enhanced_item_snapshot_state* state, int highlight_row)
{
    if (!state || state->entry_count <= 0)
        return -1;
    if (highlight_row < 0)
        return 0;
    if (highlight_row >= state->entry_count)
        return state->entry_count - 1;

    return highlight_row;
}

static int enhanced_item_find_hotkey_entry(
    const enhanced_item_snapshot_state* state, char which)
{
    int i;
    char target = (char)tolower((unsigned char)which);

    if (!state)
        return -1;

    for (i = 0; i < state->entry_count; i++)
    {
        const enhanced_item_snapshot_entry* entry = &state->entries[i];

        if (!entry->key[0])
            continue;
        if ((char)tolower((unsigned char)entry->key[0]) == target)
            return i;
    }

    return -1;
}

static bool enhanced_item_hotkey_selectable(
    const enhanced_item_snapshot_state* state, int entry_index)
{
    int item_index;

    if (!state || entry_index < 0 || entry_index >= state->entry_count)
        return false;

    item_index = state->entries[entry_index].item_index;
    if (state->mode != (USE_EQUIP))
        return true;
    if (item_index < INVEN_WIELD || item_index >= INVEN_TOTAL)
        return false;
    if (inventory[item_index].k_idx)
        return true;

    return throw_slot_menu_active && throw_slot_enabled[item_index];
}

static void enhanced_item_format_meta_with_tag(char* buf, size_t buf_size,
    cptr weight_text, cptr tag_text)
{
    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (weight_text && weight_text[0] && tag_text && tag_text[0])
    {
        strnfmt(buf, buf_size, "%s  %s", weight_text, tag_text);
    }
    else if (weight_text && weight_text[0])
    {
        SDL_strlcpy(buf, weight_text, buf_size);
    }
    else if (tag_text && tag_text[0])
    {
        SDL_strlcpy(buf, tag_text, buf_size);
    }
}

static void enhanced_item_build_prompt_line(char* buf, size_t buf_size,
    int mode)
{
    const bool portable_controls = portable_controls_active();
    const bool inventory_mode = (mode == (USE_INVEN));

    if (!buf || !buf_size)
        return;

    buf[0] = '\0';
    if (portable_controls)
    {
        char confirm_label[16];
        char desc_label[16];
        char cycle_label[16];

        inventory_prompt_label(' ', "A", confirm_label,
            sizeof(confirm_label));
        inventory_prompt_label('x', "RS Right", desc_label,
            sizeof(desc_label));
        if (current_menu_command == 'u')
        {
            inventory_prompt_label('u', "X", cycle_label,
                sizeof(cycle_label));
            strnfmt(buf, buf_size, inventory_mode
                ? "%s-use  %s-desc  <- drop  %s-cycle  (Inventory)"
                : "%s-remove  %s-desc  <- drop  %s-cycle  (Equipment)",
                confirm_label, desc_label, cycle_label);
        }
        else if (current_menu_command == 'x')
        {
            inventory_prompt_label('x', "RS Right", cycle_label,
                sizeof(cycle_label));
            strnfmt(buf, buf_size, inventory_mode
                ? "%s-examine  %s-desc  <- drop  %s-cycle  (Inventory)"
                : "%s-remove  %s-desc  <- drop  %s-cycle  (Equipment)",
                confirm_label, desc_label, cycle_label);
        }
        else
        {
            strnfmt(buf, buf_size, inventory_mode
                ? "%s-%s  %s-desc  <- drop  (Inventory)"
                : "%s-remove  %s-desc  <- drop  (Equipment)",
                confirm_label, (current_menu_command == 'w') ? "wield" : "use",
                desc_label);
        }
        return;
    }

    if (inventory_mode)
    {
        if (current_menu_command == 'u')
        {
            strnfmt(buf, buf_size,
                "Space-Use, -> description, %c again-cycle  (Inventory)",
                current_menu_command);
        }
        else if (current_menu_command == 'x')
        {
            strnfmt(buf, buf_size,
                "Space-Examine, -> description, %c again-cycle  (Inventory)",
                current_menu_command);
        }
        else
        {
            SDL_strlcpy(buf, (current_menu_command == 'w')
                ? "Space-Wield, -> description, <- drop  (Inventory)"
                : "Space-Use, -> description, <- drop  (Inventory)",
                buf_size);
        }
    }
    else
    {
        if (current_menu_command == 'u' || current_menu_command == 'x')
        {
            strnfmt(buf, buf_size,
                "Space-Remove, %c again-cycle  (Equipment)",
                current_menu_command);
        }
        else
        {
            SDL_strlcpy(buf,
                "Space-Remove, -> description, <- drop  (Equipment)",
                buf_size);
        }
    }
}

static bool enhanced_item_append_compare_overlay_rows(app_ui_panel* panel,
    int item_index)
{
    int slot_candidates[MAX_COMPARE_LINES];
    int slot_count = 0;
    int i;
    bool floor_item = false;
    object_type* base_obj = enhanced_item_object_for_index(item_index,
        &floor_item);

    if (!panel || !base_obj || !base_obj->k_idx)
        return true;

    append_compare_slot(slot_candidates, &slot_count, wield_slot(base_obj));
    if (base_obj->tval == TV_RING)
    {
        append_compare_slot(slot_candidates, &slot_count, INVEN_LEFT);
        append_compare_slot(slot_candidates, &slot_count, INVEN_RIGHT);
    }
    else if (base_obj->tval == TV_ARROW)
    {
        append_compare_slot(slot_candidates, &slot_count, INVEN_QUIVER1);
        append_compare_slot(slot_candidates, &slot_count, INVEN_QUIVER2);
    }

    for (i = 0; i < slot_count; i++)
    {
        char desc[80];
        char label[APP_UI_LABEL_MAX];
        char weight_buf[16];
        char meta[APP_UI_META_MAX];
        char tag[8];
        int slot = slot_candidates[i];
        object_type* compare_obj = &inventory[slot];
        bool empty_slot = !compare_obj->k_idx;
        byte attr = enhanced_item_row_attr(compare_obj, empty_slot);
        byte meta_attr = attr;
        byte icon_attr = 0;
        char icon_char = '\0';

        if (!empty_slot)
        {
            object_desc(desc, sizeof(desc), compare_obj, true, 3);
            icon_attr = object_attr(compare_obj);
            icon_char = object_char(compare_obj);
        }
        else
        {
            SDL_strlcpy(desc, describe_empty_slot(slot), sizeof(desc));
        }

        strnfmt(label, sizeof(label), "%s: %s", mention_use(slot), desc);
        strnfmt(tag, sizeof(tag), "(%c)", index_to_label(slot));
        enhanced_item_format_weight(weight_buf, sizeof(weight_buf), compare_obj);
        if (show_weights && empty_slot)
            weight_buf[0] = '\0';
        enhanced_item_format_meta_with_tag(meta, sizeof(meta), weight_buf, tag);

        if (!app_ui_panel_add_row_ex(panel, (s16b)(-3000 - slot), attr,
                meta_attr, icon_attr, icon_char, true, false, "", label, meta))
        {
            return false;
        }
    }

    return true;
}

static bool enhanced_item_append_armour_total_rows(app_ui_panel* panel,
    const enhanced_item_snapshot_state* state)
{
    char buf[APP_UI_LABEL_MAX];

    if (!panel || !state || state->mode != (USE_EQUIP)
        || state->armour_weight <= 0)
    {
        return true;
    }

    if (!app_ui_panel_add_row(panel, (s16b)-4001, TERM_L_DARK, true, false, "",
            "--------", ""))
    {
        return false;
    }

    strnfmt(buf, sizeof(buf), "armour: %3d.%1d lb",
        state->armour_weight / 10, state->armour_weight % 10);
    return app_ui_panel_add_row(panel, (s16b)-4002, TERM_SLATE, true, false,
        "", buf, "");
}

static void enhanced_item_build_inventory_snapshot_state(
    enhanced_item_snapshot_state* state)
{
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;
    int i;
    int z = 0;
    bool include_supplies = !inventory_menu_get_include_equip()
        && supplies_visible_for_current_filter();

    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    state->mode = (USE_INVEN);
    state->allow_compare = (current_menu_command == 'u'
        || current_menu_command == 'x');

    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px,
        0x00);
    for (i = 0; i < floor_num && state->entry_count < ENHANCED_MAX_LIST; i++)
    {
        enhanced_item_snapshot_entry* entry;
        object_type* o_ptr = &o_list[floor_list[i]];
        char weight_buf[16];

        if (!item_tester_okay(o_ptr))
            continue;

        entry = &state->entries[state->entry_count++];
        memset(entry, 0, sizeof(*entry));
        entry->item_index = 0 - floor_list[i];
        entry->is_floor = true;
        entry->attr = enhanced_item_row_attr(o_ptr, false);
        entry->meta_attr = entry->attr;
        entry->icon_attr = object_attr(o_ptr);
        entry->icon_char = object_char(o_ptr);
        SDL_strlcpy(entry->key, "-", sizeof(entry->key));
        object_desc_floor(entry->label, sizeof(entry->label), o_ptr, true, 3);
        enhanced_item_format_weight(weight_buf, sizeof(weight_buf), o_ptr);
        enhanced_item_format_meta_with_tag(entry->meta, sizeof(entry->meta),
            weight_buf, "(-)");
        state->has_floor_items = true;
    }

    for (i = 0; i < INVEN_PACK; i++)
    {
        if (inventory[i].k_idx)
            z = i + 1;
    }

    if (include_supplies)
    {
        int max_items = INVEN_PACK - 1;

        if (z > max_items)
            z = max_items;
    }

    if (include_supplies && state->entry_count < ENHANCED_MAX_LIST)
    {
        enhanced_item_snapshot_entry* entry = &state->entries[state->entry_count++];
        char hotkey = supplies_label_char();
        int slot = supplies_virtual_slot();
        char weight_buf[16];
        char tag_buf[8];

        memset(entry, 0, sizeof(*entry));
        entry->item_index = SUPPLIES_INDEX;
        entry->is_supply = true;
        entry->attr = TERM_L_WHITE;
        entry->meta_attr = TERM_L_WHITE;
        if (!hotkey && slot >= 0)
            hotkey = index_to_label(slot);
        if (hotkey)
        {
            strnfmt(entry->key, sizeof(entry->key), "%c", hotkey);
            strnfmt(tag_buf, sizeof(tag_buf), "(%c)", hotkey);
        }
        else
        {
            tag_buf[0] = '\0';
        }
        format_supply_summary(entry->label, sizeof(entry->label));
        enhanced_item_format_total_weight(weight_buf, sizeof(weight_buf),
            supplies_total_weight());
        enhanced_item_format_meta_with_tag(entry->meta, sizeof(entry->meta),
            weight_buf, tag_buf);
    }

    for (i = 0; i < z && state->entry_count < ENHANCED_MAX_LIST; i++)
    {
        enhanced_item_snapshot_entry* entry;
        object_type* o_ptr = &inventory[i];
        char weight_buf[16];
        char tag_buf[8];

        if (!item_tester_okay(o_ptr))
            continue;

        entry = &state->entries[state->entry_count++];
        memset(entry, 0, sizeof(*entry));
        entry->item_index = i;
        entry->attr = enhanced_item_row_attr(o_ptr, false);
        entry->meta_attr = entry->attr;
        entry->icon_attr = object_attr(o_ptr);
        entry->icon_char = object_char(o_ptr);
        strnfmt(entry->key, sizeof(entry->key), "%c", index_to_label(i));
        object_desc(entry->label, sizeof(entry->label), o_ptr, true, 3);
        enhanced_item_format_weight(weight_buf, sizeof(weight_buf), o_ptr);
        strnfmt(tag_buf, sizeof(tag_buf), "(%c)", index_to_label(i));
        enhanced_item_format_meta_with_tag(entry->meta, sizeof(entry->meta),
            weight_buf, tag_buf);
    }
}

static void enhanced_item_build_equipment_snapshot_state(
    enhanced_item_snapshot_state* state)
{
    int i;

    if (!state)
        return;

    memset(state, 0, sizeof(*state));
    state->mode = (USE_EQUIP);

    for (i = INVEN_WIELD; i < INVEN_TOTAL && state->entry_count < ENHANCED_MAX_LIST; i++)
    {
        enhanced_item_snapshot_entry* entry;
        object_type* o_ptr = &inventory[i];
        bool empty_slot;
        char weight_buf[16];
        char tag_buf[8];

        if (!item_tester_okay(o_ptr))
            continue;

        entry = &state->entries[state->entry_count++];
        memset(entry, 0, sizeof(*entry));
        entry->item_index = i;
        empty_slot = !o_ptr->k_idx;
        entry->attr = enhanced_item_row_attr(o_ptr, empty_slot);
        entry->meta_attr = entry->attr;
        if (!empty_slot)
        {
            entry->icon_attr = object_attr(o_ptr);
            entry->icon_char = object_char(o_ptr);
        }
        strnfmt(entry->key, sizeof(entry->key), "%c", index_to_label(i));
        strnfmt(tag_buf, sizeof(tag_buf), "(%c)", index_to_label(i));
        if (!empty_slot)
        {
            char desc[80];

            object_desc(desc, sizeof(desc), o_ptr, true, 3);
            strnfmt(entry->label, sizeof(entry->label), "%s: %s",
                mention_use(i), desc);
            enhanced_item_format_weight(weight_buf, sizeof(weight_buf), o_ptr);
            enhanced_item_format_meta_with_tag(entry->meta, sizeof(entry->meta),
                weight_buf, tag_buf);
            if (show_weights && o_ptr->weight && i >= INVEN_BODY
                && i <= INVEN_FEET)
            {
                entry->meta_attr = TERM_SLATE;
                state->armour_weight += o_ptr->weight * o_ptr->number;
            }
        }
        else
        {
            char desc[96];

            SDL_strlcpy(desc, describe_empty_slot(i), sizeof(desc));
            if (i == INVEN_QUIVER2)
            {
                SDL_strlcat(desc, " (keeps passive bonuses)", sizeof(desc));
            }
            strnfmt(entry->label, sizeof(entry->label), "%s: %s",
                mention_use(i), desc);
            enhanced_item_format_meta_with_tag(entry->meta, sizeof(entry->meta),
                "", tag_buf);
        }
    }
}

static bool enhanced_item_build_snapshot_scene(app_ui_scene* scene,
    const enhanced_item_snapshot_state* state, int highlight_row)
{
    app_ui_panel* panel;
    char prompt[APP_UI_TEXT_MAX];
    int i;

    if (!scene || !state)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_TRANSIENT);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel,
        (state->mode == (USE_EQUIP)) ? 960 : 820, 1600);

    enhanced_item_build_prompt_line(prompt, sizeof(prompt), state->mode);
    if (prompt[0])
        (void)app_ui_panel_add_body_line(panel, TERM_WHITE, prompt);

    if (state->entry_count <= 0)
    {
        return app_ui_panel_add_row(panel, (s16b)-2000, TERM_SLATE, true, false,
            "", "Nothing available.", "");
    }

    for (i = 0; i < state->entry_count; i++)
    {
        const enhanced_item_snapshot_entry* entry = &state->entries[i];
        bool selected = (highlight_row == i);
        byte attr = selected ? TERM_L_BLUE : entry->attr;
        byte meta_attr = selected ? TERM_L_BLUE : entry->meta_attr;

        if (!app_ui_panel_add_row_ex(panel, (s16b)entry->item_index, attr,
                meta_attr, entry->icon_attr, entry->icon_char, true, selected,
                "",
                entry->label, entry->meta))
        {
            return false;
        }

        if (state->mode == (USE_INVEN) && state->allow_compare && selected
            && !entry->is_supply)
        {
            if (!enhanced_item_append_compare_overlay_rows(panel,
                    entry->item_index))
            {
                return false;
            }
        }
    }

    return enhanced_item_append_armour_total_rows(panel, state);
}

static int enhanced_item_selected_action(
    const enhanced_item_snapshot_state* state, int entry_index,
    bool examine_action)
{
    const enhanced_item_snapshot_entry* entry;

    if (!state || entry_index < 0 || entry_index >= state->entry_count)
        return ENHANCED_ACTION_NONE;

    entry = &state->entries[entry_index];
    if (entry->is_supply)
        return ENHANCED_ACTION_SUPPLIES;

    return examine_action ? ENHANCED_ACTION_EXAMINE : ENHANCED_ACTION_USE;
}

static bool enhanced_item_run_snapshot_menu(int mode)
{
    enhanced_item_snapshot_scope scene_scope;
    enhanced_item_snapshot_state state;
    app_wait_scope wait_scope;
    int highlight_row = -1;
    bool done = false;

    if (!enhanced_item_snapshot_active())
        return false;
    if (!enhanced_item_snapshot_scene_enter(&scene_scope))
        return false;

    enhanced_item_set_result(mode, ENHANCED_ACTION_NONE, -1);
    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_LIST_SELECTION, 0, 0);

    while (!done)
    {
        app_ui_scene scene;
        int which;

        if (mode == (USE_EQUIP))
            enhanced_item_build_equipment_snapshot_state(&state);
        else
            enhanced_item_build_inventory_snapshot_state(&state);
        highlight_row = enhanced_item_clamp_highlight(&state, highlight_row);

        if (enhanced_item_build_snapshot_scene(&scene, &state, highlight_row))
            (void)enhanced_item_snapshot_scene_present(&scene_scope, &scene);

        which = ui_information_scene_wait_key();
        switch (which)
        {
        case ESCAPE:
            enhanced_item_set_result(mode, ENHANCED_ACTION_NONE, -1);
            done = true;
            break;

        case '/':
        case KTRL('I'):
        case KTRL('E'):
            enhanced_item_set_result(mode, ENHANCED_ACTION_SWITCH, -1);
            done = true;
            break;

        case 'u':
            if (current_menu_command == which)
            {
                enhanced_item_set_result(mode, ENHANCED_ACTION_SWITCH, -1);
                done = true;
            }
            break;

        case 'x':
            if (current_menu_command == which)
            {
                enhanced_item_set_result(mode, ENHANCED_ACTION_SWITCH, -1);
                done = true;
            }
            else if (highlight_row >= 0 && highlight_row < state.entry_count)
            {
                int action = enhanced_item_selected_action(&state,
                    highlight_row, true);

                if (action != ENHANCED_ACTION_EXAMINE
                    && !death_spectator_allow_menu_action())
                {
                    break;
                }
                enhanced_item_set_result(mode, action,
                    state.entries[highlight_row].item_index);
                done = true;
            }
            break;

#ifdef ARROW_UP
        case ARROW_UP:
#endif
        case '8':
            if (state.entry_count > 0)
            {
                highlight_row = (highlight_row + state.entry_count - 1)
                    % state.entry_count;
            }
            break;

#ifdef ARROW_DOWN
        case ARROW_DOWN:
#endif
        case '2':
            if (state.entry_count > 0)
                highlight_row = (highlight_row + 1) % state.entry_count;
            break;

#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
        case '6':
            if (highlight_row >= 0 && highlight_row < state.entry_count)
            {
                int action = enhanced_item_selected_action(&state,
                    highlight_row, true);

                if (action != ENHANCED_ACTION_EXAMINE
                    && !death_spectator_allow_menu_action())
                {
                    break;
                }
                enhanced_item_set_result(mode, action,
                    state.entries[highlight_row].item_index);
                done = true;
            }
            break;

#ifdef ARROW_LEFT
        case ARROW_LEFT:
#endif
        case '4':
            if (highlight_row >= 0 && highlight_row < state.entry_count)
            {
                if (state.entries[highlight_row].is_floor)
                {
                    bell("Cannot drop floor items!");
                    break;
                }
                if (!death_spectator_allow_menu_action())
                    break;
                enhanced_item_set_result(mode, ENHANCED_ACTION_DROP,
                    state.entries[highlight_row].item_index);
                done = true;
            }
            break;

        case ' ':
        case '\n':
        case '\r':
            if (highlight_row >= 0 && highlight_row < state.entry_count)
            {
                int action = enhanced_item_selected_action(&state,
                    highlight_row, current_menu_command == 'x');

                if (action != ENHANCED_ACTION_EXAMINE
                    && !death_spectator_allow_menu_action())
                {
                    break;
                }
                enhanced_item_set_result(mode, action,
                    state.entries[highlight_row].item_index);
                done = true;
            }
            break;

        default:
            if ((which >= 'a' && which <= 'z')
                || (which >= 'A' && which <= 'Z') || which == '-')
            {
                int entry_index = enhanced_item_find_hotkey_entry(&state,
                    (char)which);

                if (entry_index < 0
                    || !enhanced_item_hotkey_selectable(&state, entry_index))
                {
                    bell("Illegal object choice!");
                    break;
                }

                if (current_menu_command != 0)
                {
                    int action = enhanced_item_selected_action(&state,
                        entry_index, current_menu_command == 'x');

                    if (action != ENHANCED_ACTION_EXAMINE
                        && !death_spectator_allow_menu_action())
                    {
                        break;
                    }
                    enhanced_item_set_result(mode, action,
                        state.entries[entry_index].item_index);
                }
                else if (state.entries[entry_index].is_floor
                    || state.entries[entry_index].is_supply)
                {
                    int action = enhanced_item_selected_action(&state,
                        entry_index, false);

                    if (!death_spectator_allow_menu_action())
                        break;
                    enhanced_item_set_result(mode, action,
                        state.entries[entry_index].item_index);
                }
                else
                {
                    p_ptr->command_new = which;
                    p_ptr->command_see = true;
                }

                done = true;
            }
            else
            {
                bell("Invalid command!");
            }
            break;
        }
    }

    enhanced_item_snapshot_scene_close(&scene_scope);
    app_session_pop_wait_scope(app_session_current(), &wait_scope);
    return true;
}

static void append_compare_slot(int* slots, int* count, int slot)
{
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return;

    for (int i = 0; i < *count; i++)
    {
        if (slots[i] == slot)
            return;
    }

    if (*count < MAX_COMPARE_LINES)
        slots[(*count)++] = slot;
}

void describe_item_with_comparisons(int item_index, bool include_comparisons)
{
    const object_type* objects[MAX_COMPARE_LINES + 1];
    const char* headings[MAX_COMPARE_LINES + 1];
    char heading_texts[MAX_COMPARE_LINES + 1][64];
    int count = 0;
    object_type* base_obj;
    bool is_floor = (item_index < 0);

    if (item_index == -1)
        return;

    if (is_floor)
    {
        int floor_idx = 0 - item_index;
        if (floor_idx <= 0 || floor_idx >= o_max)
            return;
        base_obj = &o_list[floor_idx];
    }
    else
    {
        if (item_index < 0 || item_index >= INVEN_TOTAL)
            return;
        base_obj = &inventory[item_index];
    }

    if (!base_obj->k_idx)
        return;

    if (object_uses_smithing_difficulty(base_obj) && !object_known_p(base_obj))
    {
        bool is_equipped = (!is_floor && item_index >= INVEN_WIELD);
        (void)player_try_identify_smithing_object_on_examine(base_obj,
            is_equipped);
    }

    strnfmt(heading_texts[count], sizeof(heading_texts[count]), "%s:",
        is_floor ? "Selected item (floor)" : "Selected item");
    headings[count] = heading_texts[count];
    objects[count++] = base_obj;

    if (include_comparisons)
    {
        int slots[MAX_COMPARE_LINES];
        int slot_count = 0;

        append_compare_slot(slots, &slot_count, wield_slot(base_obj));

        if (base_obj->tval == TV_RING)
        {
            append_compare_slot(slots, &slot_count, INVEN_LEFT);
            append_compare_slot(slots, &slot_count, INVEN_RIGHT);
        }
        else if (base_obj->tval == TV_ARROW)
        {
            append_compare_slot(slots, &slot_count, INVEN_QUIVER1);
            append_compare_slot(slots, &slot_count, INVEN_QUIVER2);
        }

        for (int i = 0; i < slot_count && count < MAX_COMPARE_LINES + 1; i++)
        {
            int slot = slots[i];
            object_type* equip_obj;

            if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
                continue;

            equip_obj = &inventory[slot];
            strnfmt(heading_texts[count], sizeof(heading_texts[count]), "%s:",
                mention_use(slot));
            headings[count] = heading_texts[count];
            objects[count++] = equip_obj->k_idx ? equip_obj : NULL;
        }
    }

    object_info_screen_multi(objects, headings, count);
}

void show_inven_enhanced(void)
{
    /* Legacy entry points now forward to the semantic snapshot menu path. */
    run_inven_enhanced_menu();
}

void show_equip_enhanced(void)
{
    run_equip_enhanced_menu();
}

void run_inven_enhanced_menu(void)
{
    if (!enhanced_item_run_snapshot_menu((USE_INVEN)))
    {
        enhanced_menu_action = ENHANCED_ACTION_NONE;
        enhanced_inventory_selected_item = -1;
        msg_print("Inventory overlay requires snapshot UI.");
    }
}

void run_equip_enhanced_menu(void)
{
    if (!enhanced_item_run_snapshot_menu((USE_EQUIP)))
    {
        enhanced_equip_action = ENHANCED_ACTION_NONE;
        enhanced_equipment_selected_item = -1;
        msg_print("Equipment overlay requires snapshot UI.");
    }
}

#undef MAX_COMPARE_LINES
#undef ENHANCED_MAX_LIST
