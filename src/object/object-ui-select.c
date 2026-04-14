/* File: object-ui-select.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-session.h"
#include "externs.h"
#include "log/log.h"
#include "object/object-desc.h"
#include "object/object-display.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"
#include "ui/ui-information-scene.h"

#include <ctype.h>

bool item_tester_full = false;
byte item_tester_tval = 0;
bool (*item_tester_hook)(const object_type*) = NULL;

typedef struct item_selector_menu_scene_scope {
    bool active;
    app_input_capture_scope input_capture_scope;
} item_selector_menu_scene_scope;

static bool verify_item(cptr prompt, int item);
static bool get_item_allow(int item);
static bool get_item_okay(int item);
static int get_tag(int* cp, char tag);

static bool item_selector_verify_item_no_flash(cptr prompt, int item)
{
    return verify_item(prompt, item);
}

static bool item_selector_get_item_allow_no_flash(int item)
{
    return get_item_allow(item);
}

static bool item_selector_menu_scene_enter(item_selector_menu_scene_scope* scope)
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

static void item_selector_menu_scene_close(item_selector_menu_scene_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope)
        return;

    if (scope->active && session)
    {
        app_session_clear_inputs(session);
        app_session_clear_interaction(session);
        app_session_clear_dungeon_overlay_scene(session);
        app_session_pop_input_capture(session, &scope->input_capture_scope);
        (void)Term_xtra(TERM_XTRA_FRESH, 0);
    }
    scope->active = false;
}

static void item_selector_format_weight(char* buf, size_t buf_size,
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

static bool item_selector_mode_enabled(int mode, bool use_inven, bool use_equip,
    bool use_floor)
{
    if (mode == (USE_INVEN))
        return use_inven;
    if (mode == (USE_EQUIP))
        return use_equip;
    if (mode == (USE_FLOOR))
        return use_floor;

    return false;
}

static cptr item_selector_mode_name(int mode)
{
    if (mode == (USE_INVEN))
        return "Inventory";
    if (mode == (USE_EQUIP))
        return "Equipment";
    if (mode == (USE_FLOOR))
        return "Floor";

    return "Items";
}

static int item_selector_selected_entry(int current_mode, const int* floor_list,
    int vis_inven_cnt, const int* vis_inven, int vis_equip_cnt,
    const int* vis_equip, int vis_floor_cnt, const int* vis_floor,
    int highlight_row)
{
    if (highlight_row < 0)
        return -10000;

    if (current_mode == (USE_INVEN))
    {
        if (highlight_row < vis_inven_cnt)
            return vis_inven[highlight_row];
    }
    else if (current_mode == (USE_EQUIP))
    {
        if (highlight_row < vis_equip_cnt)
            return vis_equip[highlight_row];
    }
    else if (current_mode == (USE_FLOOR))
    {
        if (highlight_row < vis_floor_cnt)
            return 0 - floor_list[vis_floor[highlight_row]];
    }

    return -10000;
}

static byte item_selector_row_attr(const object_type* o_ptr, bool empty_slot)
{
    if (!o_ptr || empty_slot || !o_ptr->k_idx)
        return TERM_L_DARK;

    if (weapon_glows(o_ptr))
        return object_display_color(o_ptr, TERM_L_BLUE);

    return object_display_color(o_ptr,
        tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
}

static void item_selector_add_tabs(app_ui_panel* panel, int current_mode,
    bool use_inven, bool use_equip, bool use_floor)
{
    if (!panel)
        return;

    if (use_inven)
    {
        (void)app_ui_panel_add_tab(panel, (s16b)(USE_INVEN),
            (current_mode == (USE_INVEN)) ? TERM_L_BLUE : TERM_SLATE,
            current_mode == (USE_INVEN), "Inventory");
    }
    if (use_equip)
    {
        (void)app_ui_panel_add_tab(panel, (s16b)(USE_EQUIP),
            (current_mode == (USE_EQUIP)) ? TERM_L_BLUE : TERM_SLATE,
            current_mode == (USE_EQUIP), "Equipment");
    }
    if (use_floor)
    {
        (void)app_ui_panel_add_tab(panel, (s16b)(USE_FLOOR),
            (current_mode == (USE_FLOOR)) ? TERM_L_BLUE : TERM_SLATE,
            current_mode == (USE_FLOOR), "Floor");
    }
}

static void item_selector_add_footer_actions(app_ui_panel* panel,
    bool can_switch)
{
    if (!panel)
        return;

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Space", "Select");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "x", "Examine");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "8/2", "Move");
    if (can_switch)
    {
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            "/", "Switch");
    }
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
        "Esc", "Cancel");
}

static void item_selector_add_selected_detail(app_ui_panel* panel,
    int current_mode, const int* floor_list, int vis_inven_cnt,
    const int* vis_inven, int vis_equip_cnt, const int* vis_equip,
    int vis_floor_cnt, const int* vis_floor, int highlight_row)
{
    char buf[APP_UI_TEXT_MAX];
    int selected_entry;
    object_type* o_ptr = NULL;
    bool floor_item = false;

    if (!panel)
        return;

    selected_entry = item_selector_selected_entry(current_mode, floor_list,
        vis_inven_cnt, vis_inven, vis_equip_cnt, vis_equip, vis_floor_cnt,
        vis_floor, highlight_row);
    if (selected_entry == -10000)
        return;

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected");

    if (selected_entry == SUPPLIES_INDEX)
    {
        format_supply_summary(buf, sizeof(buf));
        (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, buf);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "Shared potion, herb, and gem cache.");
        return;
    }

    if (selected_entry < 0)
    {
        floor_item = true;
        o_ptr = &o_list[0 - selected_entry];
    }
    else
    {
        o_ptr = &inventory[selected_entry];
    }

    if (floor_item)
        object_desc_floor(buf, sizeof(buf), o_ptr, true, 3);
    else if (o_ptr->k_idx)
        object_desc(buf, sizeof(buf), o_ptr, true, 3);
    else
        SDL_strlcpy(buf, describe_empty_slot(selected_entry), sizeof(buf));
    (void)app_ui_panel_add_detail_line(panel,
        item_selector_row_attr(o_ptr, !floor_item && !o_ptr->k_idx), buf);

    if (floor_item)
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "Location: floor");
    }
    else if (selected_entry >= INVEN_WIELD)
    {
        strnfmt(buf, sizeof(buf), "Slot: %s", mention_use(selected_entry));
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }
    else
    {
        char label = index_to_label(selected_entry);

        strnfmt(buf, sizeof(buf), "Slot: %c", label ? label : '?');
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }

    if (o_ptr->k_idx && o_ptr->weight)
    {
        int weight = o_ptr->weight * o_ptr->number;

        strnfmt(buf, sizeof(buf), "Weight: %d.%d lb", weight / 10, weight % 10);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE, buf);
    }

    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
        "Press x to inspect and compare.");
}

static bool item_selector_append_inventory_rows(app_ui_panel* panel,
    int vis_inven_cnt, const int* vis_inven, int highlight_row)
{
    int i;

    if (!panel)
        return false;

    for (i = 0; i < vis_inven_cnt; i++)
    {
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        char key[APP_UI_KEY_MAX];
        byte attr = TERM_WHITE;
        byte icon_attr = 0;
        char icon_char = '\0';
        int item_index = vis_inven[i];

        label[0] = '\0';
        meta[0] = '\0';
        key[0] = '\0';
        if (item_index == SUPPLIES_INDEX)
        {
            int virtual_slot = supplies_virtual_slot();
            char tag = supplies_label_char();

            format_supply_summary(label, sizeof(label));
            attr = TERM_L_WHITE;
            if (!tag && virtual_slot >= 0)
                tag = index_to_label(virtual_slot);
            if (tag)
                strnfmt(key, sizeof(key), "%c", tag);
        }
        else
        {
            object_type* o_ptr = &inventory[item_index];

            object_desc(label, sizeof(label), o_ptr, true, 3);
            item_selector_format_weight(meta, sizeof(meta), o_ptr);
            attr = item_selector_row_attr(o_ptr, false);
            icon_attr = object_attr(o_ptr);
            icon_char = object_char(o_ptr);
            strnfmt(key, sizeof(key), "%c", index_to_label(item_index));
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)item_index, attr, attr,
                icon_attr, icon_char, true, highlight_row == i, key, label,
                meta))
        {
            return false;
        }
    }

    return true;
}

static bool item_selector_append_equipment_rows(app_ui_panel* panel,
    int vis_equip_cnt, const int* vis_equip, int highlight_row)
{
    int i;

    if (!panel)
        return false;

    for (i = 0; i < vis_equip_cnt; i++)
    {
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        char desc[80];
        char key[APP_UI_KEY_MAX];
        int item_index = vis_equip[i];
        object_type* o_ptr = &inventory[item_index];
        bool empty_slot = !o_ptr->k_idx;
        byte attr;
        byte icon_attr = 0;
        char icon_char = '\0';

        if (!empty_slot)
            object_desc(desc, sizeof(desc), o_ptr, true, 3);
        else
            SDL_strlcpy(desc, describe_empty_slot(item_index), sizeof(desc));
        strnfmt(label, sizeof(label), "%s: %s", mention_use(item_index), desc);
        item_selector_format_weight(meta, sizeof(meta), o_ptr);
        strnfmt(key, sizeof(key), "%c", index_to_label(item_index));
        attr = item_selector_row_attr(o_ptr, empty_slot);
        if (!empty_slot)
        {
            icon_attr = object_attr(o_ptr);
            icon_char = object_char(o_ptr);
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)item_index, attr,
                empty_slot ? TERM_SLATE : attr, icon_attr, icon_char, true,
                highlight_row == i, key, label, meta))
        {
            return false;
        }
    }

    return true;
}

static bool item_selector_append_floor_rows(app_ui_panel* panel,
    const int* floor_list, int vis_floor_cnt, const int* vis_floor,
    int highlight_row)
{
    int i;

    if (!panel || !floor_list)
        return false;

    for (i = 0; i < vis_floor_cnt; i++)
    {
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        char key[APP_UI_KEY_MAX];
        int floor_slot = vis_floor[i];
        int object_index = floor_list[floor_slot];
        object_type* o_ptr = &o_list[object_index];
        byte attr = item_selector_row_attr(o_ptr, false);

        object_desc_floor(label, sizeof(label), o_ptr, true, 3);
        item_selector_format_weight(meta, sizeof(meta), o_ptr);
        strnfmt(key, sizeof(key), "%c", index_to_label(floor_slot));

        if (!app_ui_panel_add_row_ex(panel, (s16b)(0 - object_index), attr,
                attr, object_attr(o_ptr), object_char(o_ptr), true,
                highlight_row == i, key, label, meta))
        {
            return false;
        }
    }

    return true;
}

static bool item_selector_build_ui_scene(app_ui_scene* scene, cptr prompt,
    int current_mode, bool use_inven, bool use_equip, bool use_floor,
    const int* floor_list, int vis_inven_cnt, const int* vis_inven,
    int vis_equip_cnt, const int* vis_equip, int vis_floor_cnt,
    const int* vis_floor, int highlight_row)
{
    app_ui_panel* panel;
    char controls[APP_UI_TEXT_MAX];
    int available_modes = 0;

    if (!scene)
        return false;

    if (use_inven)
        available_modes++;
    if (use_equip)
        available_modes++;
    if (use_floor)
        available_modes++;

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
    app_ui_panel_set_title(panel, TERM_L_WHITE,
        (prompt && prompt[0]) ? prompt : "Select item");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        item_selector_mode_name(current_mode));
    item_selector_add_tabs(panel, current_mode, use_inven, use_equip, use_floor);

    SDL_strlcpy(controls, "Enter/Space selects, x examines, 8/2 moves.",
        sizeof(controls));
    if (available_modes > 1)
        SDL_strlcat(controls, " / switches panes.", sizeof(controls));
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE, controls);
    item_selector_add_footer_actions(panel, available_modes > 1);

    if (current_mode == (USE_INVEN))
    {
        if (!item_selector_append_inventory_rows(panel, vis_inven_cnt, vis_inven,
                highlight_row))
        {
            return false;
        }
    }
    else if (current_mode == (USE_EQUIP))
    {
        if (!item_selector_append_equipment_rows(panel, vis_equip_cnt, vis_equip,
                highlight_row))
        {
            return false;
        }
    }
    else if (current_mode == (USE_FLOOR))
    {
        if (!item_selector_append_floor_rows(panel, floor_list, vis_floor_cnt,
                vis_floor, highlight_row))
        {
            return false;
        }
    }

    if (panel->row_count == 0
        && item_selector_mode_enabled(current_mode, use_inven, use_equip,
            use_floor))
    {
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "No selectable entries in this pane.");
    }

    item_selector_add_selected_detail(panel, current_mode, floor_list,
        vis_inven_cnt, vis_inven, vis_equip_cnt, vis_equip, vis_floor_cnt,
        vis_floor, highlight_row);

    return true;
}

static bool item_selector_menu_scene_present(item_selector_menu_scene_scope* scope,
    const app_ui_scene* scene)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session || !scene)
        return false;
    if (!app_session_publish_dungeon_overlay_scene(session, scene))
        return false;

    (void)Term_xtra(TERM_XTRA_FRESH, 0);
    return true;
}

typedef struct item_selector_visible_state {
    int vis_inven[INVEN_PACK + 1];
    int vis_inven_cnt;
    int vis_equip[INVEN_TOTAL - INVEN_WIELD];
    int vis_equip_cnt;
    int vis_floor[MAX_FLOOR_STACK];
    int vis_floor_cnt;
    int highlight_row;
    bool highlight_active;
} item_selector_visible_state;

static bool item_selector_mode_allows_item(int item, bool allow_inven,
    bool allow_equip, bool allow_floor)
{
    if (item == SUPPLIES_INDEX)
        return allow_inven;
    if (item < 0)
        return allow_floor;
    if (item < INVEN_WIELD)
        return allow_inven;

    return allow_equip;
}

static void item_selector_build_visible_state(
    item_selector_visible_state* state, int current_mode,
    const int* floor_list, int floor_num)
{
    int count = 0;

    if (!state)
        return;

    state->vis_inven_cnt = 0;
    state->vis_equip_cnt = 0;
    state->vis_floor_cnt = 0;

    if (!p_ptr->command_see)
    {
        state->highlight_row = -1;
        state->highlight_active = false;
        return;
    }

    if (current_mode == (USE_INVEN))
    {
        bool has_supplies = supplies_visible_for_current_filter();

        if (has_supplies && state->vis_inven_cnt < INVEN_PACK)
            state->vis_inven[state->vis_inven_cnt++] = SUPPLIES_INDEX;

        for (int ii = 0; ii < INVEN_PACK && state->vis_inven_cnt < INVEN_PACK;
             ii++)
        {
            if (inventory[ii].k_idx && get_item_okay(ii))
                state->vis_inven[state->vis_inven_cnt++] = ii;
        }

        count = state->vis_inven_cnt;
    }
    else if (current_mode == (USE_EQUIP))
    {
        for (int ii = INVEN_WIELD; ii < INVEN_TOTAL; ii++)
        {
            bool include_slot = false;

            if (inventory[ii].k_idx)
                include_slot = get_item_okay(ii);
            else if (throw_slot_menu_active && throw_slot_enabled[ii])
                include_slot = true;

            if (include_slot)
                state->vis_equip[state->vis_equip_cnt++] = ii;
        }

        count = state->vis_equip_cnt;
    }
    else if (current_mode == (USE_FLOOR))
    {
        for (int ii = 0; ii < floor_num; ii++)
        {
            int obj_idx = floor_list[ii];

            if (get_item_okay(0 - obj_idx))
                state->vis_floor[state->vis_floor_cnt++] = ii;
        }

        count = state->vis_floor_cnt;
    }

    if (count <= 0)
    {
        state->highlight_row = -1;
        state->highlight_active = false;
        return;
    }

    if (!state->highlight_active || state->highlight_row < 0)
        state->highlight_row = 0;
    else if (state->highlight_row >= count)
        state->highlight_row = count - 1;

    state->highlight_active = true;
}

static void item_selector_move_highlight(item_selector_visible_state* state,
    int current_mode, int dir)
{
    int count = 0;

    if (!state || !state->highlight_active)
        return;

    if (current_mode == (USE_INVEN))
        count = state->vis_inven_cnt;
    else if (current_mode == (USE_EQUIP))
        count = state->vis_equip_cnt;
    else if (current_mode == (USE_FLOOR))
        count = state->vis_floor_cnt;

    if (count <= 0)
        return;

    state->highlight_row = (state->highlight_row + count + dir) % count;
}

static bool item_selector_highlighted_item(const item_selector_visible_state* state,
    int current_mode, const int* floor_list, int* out_item)
{
    int item;

    if (!state || !out_item)
        return false;

    item = item_selector_selected_entry(current_mode, floor_list,
        state->vis_inven_cnt, state->vis_inven, state->vis_equip_cnt,
        state->vis_equip, state->vis_floor_cnt, state->vis_floor,
        state->highlight_row);
    if (item == -10000)
        return false;

    *out_item = item;
    return true;
}

static int item_selector_inventory_hotkey_item(
    const item_selector_visible_state* state, char which)
{
    int i;
    char target = (char)tolower((unsigned char)which);

    if (!state)
        return -10000;

    for (i = 0; i < state->vis_inven_cnt; i++)
    {
        int item_index = state->vis_inven[i];
        char key = '\0';

        if (item_index == SUPPLIES_INDEX)
        {
            int virtual_slot = supplies_virtual_slot();

            key = supplies_label_char();
            if (!key && virtual_slot >= 0)
                key = index_to_label(virtual_slot);
        }
        else if (item_index >= 0 && item_index < INVEN_WIELD)
        {
            key = index_to_label(item_index);
        }

        if (key && (char)tolower((unsigned char)key) == target)
            return item_index;
    }

    return -10000;
}

static void item_selector_format_prompt(char* out_val, size_t out_val_size,
    cptr pmt, int current_mode, bool use_inven, bool use_equip, int i1, int i2,
    int e1, int e2, int f1, int f2)
{
    if (!out_val || !out_val_size)
        return;

    if (p_ptr->command_see)
    {
        if (current_mode == (USE_INVEN))
        {
            strnfmt(out_val, out_val_size, "(Inven:%c-%c, ESC, %s) %s",
                index_to_label(i1), index_to_label(i2),
                use_equip ? "/ for Equip" : "- for floor,", pmt);
        }
        else if (current_mode == (USE_EQUIP))
        {
            strnfmt(out_val, out_val_size, "(Equip:%c-%c, ESC, %s) %s",
                index_to_label(e1), index_to_label(e2),
                use_inven ? "/ for Inven" : "- for floor,", pmt);
        }
        else
        {
            strnfmt(out_val, out_val_size, "(Floor:%c-%c, ESC, %s) %s",
                index_to_label(f1), index_to_label(f2),
                use_inven ? "/ for Inven" : use_equip ? "/ for Equip" : "",
                pmt);
        }
    }
    else
    {
        strnfmt(out_val, out_val_size, "(Items, ESC) %s", pmt);
    }
}

static bool item_selector_run_snapshot_loop(int* cp, cptr pmt, bool use_inven,
    bool use_equip, bool use_floor, bool allow_inven, bool allow_equip,
    bool allow_floor, int i1, int i2, int e1, int e2, int f1, int f2,
    const int* floor_list, int floor_num, bool* toggle, bool* out_item)
{
    item_selector_menu_scene_scope menu_scene_scope;
    item_selector_visible_state visible_state;
    bool done = false;
    bool item = false;

    memset(&menu_scene_scope, 0, sizeof(menu_scene_scope));
    memset(&visible_state, 0, sizeof(visible_state));
    visible_state.highlight_row = -1;

    if (!item_selector_menu_scene_enter(&menu_scene_scope))
        return false;

    p_ptr->command_see = true;

    while (!done)
    {
        char out_val[160];
        char which;
        int j;
        int ni = 0;
        int ne = 0;

        for (j = 0; j < ANGBAND_TERM_MAX; j++)
        {
            if (!angband_term[j])
                continue;

            if (op_ptr->window_flag[j] & (PW_INVEN))
                ni++;
            if (op_ptr->window_flag[j] & (PW_EQUIP))
                ne++;
        }

        if (((p_ptr->command_wrk == (USE_EQUIP)) && ni && !ne)
            || ((p_ptr->command_wrk == (USE_INVEN)) && !ni && ne))
        {
            toggle_inven_equip();
            if (toggle)
                *toggle = !(*toggle);
        }

        p_ptr->window |= (PW_INVEN | PW_EQUIP);
        window_stuff();

        item_selector_build_visible_state(&visible_state, p_ptr->command_wrk,
            floor_list, floor_num);
        item_selector_format_prompt(out_val, sizeof(out_val), pmt,
            p_ptr->command_wrk, use_inven, use_equip, i1, i2, e1, e2, f1, f2);

        {
            app_ui_scene scene;

            if (!item_selector_build_ui_scene(&scene, out_val,
                    p_ptr->command_wrk, use_inven, use_equip, use_floor,
                    floor_list, visible_state.vis_inven_cnt,
                    visible_state.vis_inven, visible_state.vis_equip_cnt,
                    visible_state.vis_equip, visible_state.vis_floor_cnt,
                    visible_state.vis_floor,
                    visible_state.highlight_active
                        ? visible_state.highlight_row
                        : -1)
                || !item_selector_menu_scene_present(&menu_scene_scope, &scene))
            {
                log_warn("item selector: failed to present snapshot overlay");
                item_selector_menu_scene_close(&menu_scene_scope);
                p_ptr->command_see = false;
                if (out_item)
                    *out_item = false;
                return false;
            }
        }

        which = (char)ui_information_scene_wait_key();

        switch (which)
        {
        case ESCAPE:
            done = true;
            break;

        case '*':
        case '?':
            break;

        case ' ':
        {
            int selected_item;

            if (!visible_state.highlight_active
                || !item_selector_highlighted_item(&visible_state,
                    p_ptr->command_wrk, floor_list, &selected_item))
            {
                break;
            }

            if (!item_selector_mode_allows_item(selected_item, allow_inven,
                    allow_equip, allow_floor)
                || !get_item_okay(selected_item))
            {
                break;
            }

            if (!item_selector_get_item_allow_no_flash(selected_item))
                break;

            *cp = selected_item;
            item = true;
            done = true;
            break;
        }

        case '/':
            if (use_inven && (p_ptr->command_wrk != (USE_INVEN)))
                p_ptr->command_wrk = (USE_INVEN);
            else if (use_equip && (p_ptr->command_wrk != (USE_EQUIP)))
                p_ptr->command_wrk = (USE_EQUIP);
            else
                bell("Cannot switch item selector!");
            break;

        case '-':
            if (!allow_floor)
            {
                bell("Cannot select floor!");
                break;
            }

            for (int i = 0; i < floor_num; i++)
            {
                int selected_item = 0 - floor_list[i];

                if (!get_item_okay(selected_item))
                    continue;

                if (!item_selector_get_item_allow_no_flash(selected_item))
                    continue;

                *cp = selected_item;
                item = true;
                done = true;
                break;
            }
            break;

        case 'x':
        case 'X':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
        {
            int selected_item;

            if (!visible_state.highlight_active
                || !item_selector_highlighted_item(&visible_state,
                    p_ptr->command_wrk, floor_list, &selected_item))
            {
                bell("No highlighted item to examine.");
                break;
            }

            describe_item_with_comparisons(selected_item, true);
            break;
        }

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        {
            int selected_item = 0;
            bool tag_found = get_tag(&selected_item, which);

            if (!tag_found && visible_state.highlight_active
                && (which == '2' || which == '8' || which == '6'))
            {
                if (which == '8')
                {
                    item_selector_move_highlight(&visible_state,
                        p_ptr->command_wrk, -1);
                    break;
                }
                if (which == '2')
                {
                    item_selector_move_highlight(&visible_state,
                        p_ptr->command_wrk, +1);
                    break;
                }
                if (!item_selector_highlighted_item(&visible_state,
                        p_ptr->command_wrk, floor_list, &selected_item))
                {
                    break;
                }
            }
            else if (!tag_found)
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            if (!item_selector_mode_allows_item(selected_item, allow_inven,
                    allow_equip, allow_floor))
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            if (!get_item_okay(selected_item))
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            if (!item_selector_get_item_allow_no_flash(selected_item))
            {
                done = true;
                break;
            }

            *cp = selected_item;
            item = true;
            done = true;
            break;
        }

        case '[':
        case ']':
        {
            int selected_item = 0;
            bool item_found = false;

            if (p_ptr->command_wrk == (USE_INVEN))
            {
                for (int i = INVEN_PACK; i >= 0; i--)
                {
                    if (get_item_okay(i) && ((which == '[') || !item_found))
                    {
                        selected_item = i;
                        item_found = true;
                    }
                }
            }
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
                {
                    if (get_item_okay(i) && ((which == ']') || !item_found))
                    {
                        selected_item = i;
                        item_found = true;
                    }
                }
            }

            if (!item_found)
            {
                bell("No valid items found.");
                break;
            }

            if (!item_selector_mode_allows_item(selected_item, allow_inven,
                    allow_equip, allow_floor))
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            if (!item_selector_get_item_allow_no_flash(selected_item))
            {
                done = true;
                break;
            }

            *cp = selected_item;
            item = true;
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            int selected_item = 0;

            if (visible_state.highlight_active
                && item_selector_highlighted_item(&visible_state,
                    p_ptr->command_wrk, floor_list, &selected_item))
            {
                if (!get_item_okay(selected_item))
                {
                    bell("Illegal object choice (highlight)!");
                    break;
                }

                if (!item_selector_get_item_allow_no_flash(selected_item))
                {
                    done = true;
                    break;
                }

                *cp = selected_item;
                item = true;
                done = true;
                break;
            }

            if (p_ptr->command_wrk == (USE_INVEN))
            {
                if (i1 != i2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                selected_item = i1;
            }
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                if (e1 != e2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                selected_item = e1;
            }
            else
            {
                if (f1 != f2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                selected_item = 0 - floor_list[f1];
            }

            if (!get_item_okay(selected_item))
            {
                bell("Illegal object choice (default)!");
                break;
            }

            if (!item_selector_get_item_allow_no_flash(selected_item))
            {
                done = true;
                break;
            }

            *cp = selected_item;
            item = true;
            done = true;
            break;
        }

        default:
        {
            int selected_item = 0;
            bool verify;

            verify = (isupper((unsigned char)which) ? true : false);
            which = (char)tolower((unsigned char)which);

            if (p_ptr->command_wrk == (USE_INVEN))
            {
                selected_item = item_selector_inventory_hotkey_item(
                    &visible_state, which);
                if (selected_item == -10000)
                    selected_item = label_to_inven(which);

                if (selected_item < 0)
                {
                    bell("Illegal object choice (inven)!");
                    break;
                }
            }
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                selected_item = label_to_equip(which);

                if (selected_item < 0)
                {
                    bell("Illegal object choice (equip)!");
                    break;
                }
            }
            else
            {
                int floor_slot = (islower((unsigned char)which) ? A2I(which) : -1);

                if (floor_slot < 0 || floor_slot >= floor_num)
                {
                    bell("Illegal object choice (floor)!");
                    break;
                }

                selected_item = 0 - floor_list[floor_slot];
            }

            if (!get_item_okay(selected_item))
            {
                bell("Illegal object choice (normal)!");
                break;
            }

            if (verify && !item_selector_verify_item_no_flash("Try",
                    selected_item))
            {
                done = true;
                break;
            }

            if (!item_selector_get_item_allow_no_flash(selected_item))
            {
                done = true;
                break;
            }

            *cp = selected_item;
            item = true;
            done = true;
            break;
        }
        }
    }

    item_selector_menu_scene_close(&menu_scene_scope);
    p_ptr->command_see = false;
    if (out_item)
        *out_item = item;

    return true;
}

/*
 * Flip "inven" and "equip" in any sub-windows
 */
void toggle_inven_equip(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Unused */
        if (!angband_term[j])
            continue;

        /* Flip inven to equip */
        if (op_ptr->window_flag[j] & (PW_INVEN))
        {
            /* Flip flags */
            op_ptr->window_flag[j] &= ~(PW_INVEN);
            op_ptr->window_flag[j] |= (PW_EQUIP);

            /* Window stuff */
            p_ptr->window |= (PW_EQUIP);
        }

        /* Flip inven to equip */
        else if (op_ptr->window_flag[j] & (PW_EQUIP))
        {
            /* Flip flags */
            op_ptr->window_flag[j] &= ~(PW_EQUIP);
            op_ptr->window_flag[j] |= (PW_INVEN);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN);
        }
    }
}

/*
 * Verify the choice of an item.
 *
 * The item can be negative to mean "item on floor".
 */
static bool verify_item(cptr prompt, int item)
{
    char o_name[80];
    char out_val[160];
    object_type* o_ptr;

    if (item == SUPPLIES_INDEX)
        return true;

    /* Inventory */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Floor */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    if (!o_ptr)
        return false;

    if ((item >= 0) && !o_ptr->k_idx)
    {
        if (item >= INVEN_WIELD && item < INVEN_TOTAL && item_tester_okay(o_ptr))
        {
            strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt,
                describe_empty_slot(item));
            return get_check(out_val);
        }

        return false;
    }

    /* Describe */
    if (item < 0)
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
    else
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Prompt */
    strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt, o_name);

    /* Query */
    return (get_check(out_val));
}

/*
 * Hack -- allow user to "prevent" certain choices.
 *
 * The item can be negative to mean "item on floor".
 */
static bool get_item_allow(int item)
{
    cptr s;
    object_type* o_ptr;

    if (item == SUPPLIES_INDEX)
        return true;

    /* Inventory */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Floor */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    if (!o_ptr)
        return false;

    if ((item >= 0) && !o_ptr->k_idx)
    {
        if (item >= INVEN_WIELD && item < INVEN_TOTAL && item_tester_okay(o_ptr))
            return true;

        return false;
    }

    /* No inscription */
    if (!o_ptr->obj_note)
        return (true);

    /* Find a '!' */
    s = strchr(quark_str(o_ptr->obj_note), '!');

    /* Process preventions */
    while (s)
    {
        /* Check the "restriction" */
        if ((s[1] == p_ptr->command_cmd) || (s[1] == '*'))
        {
            /* Verify the choice */
            if (!verify_item("Really try", item))
                return (false);
        }

        /* Find another '!' */
        s = strchr(s + 1, '!');
    }

    /* Allow it */
    return (true);
}

/*
 * Verify the "okayness" of a given item.
 *
 * The item can be negative to mean "item on floor".
 */
static bool get_item_okay(int item)
{
    object_type* o_ptr;

    if (item == SUPPLIES_INDEX)
        return supplies_visible_for_current_filter();

    /* Inventory */
    if (item >= 0)
    {
        o_ptr = &inventory[item];
    }

    /* Floor */
    else
    {
        o_ptr = &o_list[0 - item];
    }

    if (!o_ptr)
        return false;

    if ((item >= 0) && !o_ptr->k_idx)
    {
        if (item >= INVEN_WIELD && item < INVEN_TOTAL)
            return item_tester_okay(o_ptr);

        return false;
    }

    /* Verify the item */
    return (item_tester_okay(o_ptr));
}

/*
 * Find the "first" inventory object with the given "tag".
 *
 * A "tag" is a char "n" appearing as "@n" anywhere in the
 * inscription of an object.
 *
 * Also, the tag "@xn" will work as well, where "n" is a tag-char,
 * and "x" is the "current" p_ptr->command_cmd code.
 *
 * Also works with '[' for first valid choice and ']' for last valid choice.
 */
static int get_tag(int* cp, char tag)
{
    int i;
    cptr s;

    /* Check every object */
    for (i = 0; i < INVEN_TOTAL; ++i)
    {
        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip empty inscriptions */
        if (!o_ptr->obj_note)
            continue;

        /* Find a '@' */
        s = strchr(quark_str(o_ptr->obj_note), '@');

        /* Process all tags */
        while (s)
        {
            /* Check the normal tags */
            if (s[1] == tag)
            {
                /* Save the actual inventory ID */
                *cp = i;

                /* Success */
                return (true);
            }

            /* Check the special tags */
            if ((s[1] == p_ptr->command_cmd) && (s[2] == tag))
            {
                /* Save the actual inventory ID */
                *cp = i;

                /* Success */
                return (true);
            }

            /* Find another '@' */
            s = strchr(s + 1, '@');
        }
    }

    /* No such tag */
    return (false);
}

/*
 * Let the user select an item, save its "index"
 *
 * Return true only if an acceptable item was chosen by the user.
 *
 * The selected item must satisfy the "item_tester_hook()" function,
 * if that hook is set, and the "item_tester_tval", if that value is set.
 *
 * All "item_tester" restrictions are cleared before this function returns.
 *
 * The user is allowed to choose acceptable items from the equipment,
 * inventory, or floor, respectively, if the proper flag was given,
 * and there are any acceptable items in that location.
 *
 * The equipment or inventory are displayed (even if no acceptable
 * items are in that location) if the proper flag was given.
 *
 * If there are no acceptable items available anywhere, and "str" is
 * not NULL, then it will be used as the text of a warning message
 * before the function returns.
 *
 * Note that the user must press "-" to specify the item on the floor,
 * and there is no way to "examine" the item on the floor, while the
 * use of "capital" letters will "examine" an inventory/equipment item,
 * and prompt for its use.
 *
 * If a legal item is selected from the inventory, we save it in "cp"
 * directly (0 to 35), and return true.
 *
 * If a legal item is selected from the floor, we save it in "cp" as
 * a negative (-1 to -511), and return true.
 *
 * If no item is available, we do nothing to "cp", and we display a
 * warning message, using "str" if available, and return false.
 *
 * If no item is selected, we do nothing to "cp", and return false.
 *
 * Global "p_ptr->command_new" is used when viewing the inventory or equipment
 * to allow the user to enter a command while viewing those screens, and
 * also to induce "auto-enter" of stores, and other such stuff.
 *
 * Global "p_ptr->command_see" may be set before calling this function to start
 * out in "browse" mode.  It is cleared before this function returns.
 *
 * Global "p_ptr->command_wrk" is used to choose between equip/inven/floor
 * listings.  It is equal to USE_INVEN or USE_EQUIP or USE_FLOOR, except
 * when this function is first called, when it is equal to zero, which will
 * cause it to be set to USE_INVEN.
 *
 * We always erase the prompt when we are done, leaving a blank line,
 * or a warning message, if appropriate, if no items are available.
 *
 * Note that only "acceptable" floor objects get indexes, so between two
 * commands, the indexes of floor objects may change.  XXX XXX XXX
 */
bool get_item(int* cp, cptr pmt, cptr str, int mode)
{
    app_wait_scope wait_scope;
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i1, i2;
    int e1, e2;
    int f1, f2;

    bool done, item;
    bool oops = false;

    bool use_inven = ((mode & (USE_INVEN)) ? true : false);
    bool use_equip = ((mode & (USE_EQUIP)) ? true : false);
    bool use_floor = ((mode & (USE_FLOOR)) ? true : false);

    bool allow_inven = false;
    bool allow_equip = false;
    bool allow_floor = false;

    bool toggle = false;

    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

#ifdef ALLOW_REPEAT

    /* Get the item index */
    if (repeat_pull(cp))
    {
        /* Verify the item */
        if (get_item_okay(*cp))
        {
            /* Forget the item_tester_tval restriction */
            item_tester_tval = 0;

            /* Forget the item_tester_hook restriction */
            item_tester_hook = NULL;

            /* Success */
            return (true);
        }
        else
        {
            /* Invalid repeat - reset it */
            repeat_clear();
        }
    }

#endif /* ALLOW_REPEAT */

    /* save the mode in a global variable version */
    p_ptr->get_item_mode = mode;

    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_LIST_SELECTION, mode, 0);

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Not done */
    done = false;

    /* No item selected */
    item = false;

    /* Full inventory */
    i1 = 0;
    i2 = INVEN_PACK - 1;

    /* Forbid inventory */
    if (!use_inven)
        i2 = -1;

    /* Restrict inventory indexes */
    while ((i1 <= i2) && (!get_item_okay(i1)))
        i1++;
    while ((i1 <= i2) && (!get_item_okay(i2)))
        i2--;

    /* Accept inventory */
    if (i1 <= i2)
        allow_inven = true;

    /* Full equipment */
    e1 = INVEN_WIELD;
    e2 = INVEN_TOTAL - 1;

    /* Forbid equipment */
    if (!use_equip)
        e2 = -1;

    /* Restrict equipment indexes */
    while ((e1 <= e2) && (!get_item_okay(e1)))
        e1++;
    while ((e1 <= e2) && (!get_item_okay(e2)))
        e2--;

    /* Accept equipment */
    if (e1 <= e2)
        allow_equip = true;

    /* Scan all objects in the grid */
    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, py, px, 0x00);

    /* Full floor */
    f1 = 0;
    f2 = floor_num - 1;

    /* Forbid floor */
    if (!use_floor)
        f2 = -1;

    /* Restrict floor indexes */
    while ((f1 <= f2) && (!get_item_okay(0 - floor_list[f1])))
        f1++;
    while ((f1 <= f2) && (!get_item_okay(0 - floor_list[f2])))
        f2--;

    /* Accept floor */
    if (f1 <= f2)
        allow_floor = true;

    /* Require at least one legal choice */
    if (!allow_inven && !allow_equip && !allow_floor)
    {
        /* Cancel p_ptr->command_see */
        p_ptr->command_see = false;

        /* Oops */
        oops = true;

        /* Done */
        done = true;
    }

    /* Analyze choices */
    else
    {
        /* Hack -- Start on equipment if requested */
        if (p_ptr->command_see && (p_ptr->command_wrk == (USE_EQUIP))
            && use_equip)
        {
            p_ptr->command_wrk = (USE_EQUIP);
        }

        /* Use inventory if allowed */
        else if (use_inven)
        {
            p_ptr->command_wrk = (USE_INVEN);
        }

        /* Use equipment if allowed */
        else if (use_equip)
        {
            p_ptr->command_wrk = (USE_EQUIP);
        }

        /* Use floor if allowed */
        else if (use_floor)
        {
            p_ptr->command_wrk = (USE_FLOOR);
        }

        /* Hack -- Use (empty) inventory */
        else
        {
            p_ptr->command_wrk = (USE_INVEN);
        }
    }

    /* Option to always show a list */
    if (auto_display_lists)
    {
        p_ptr->command_see = true;
    }

    if (!done)
    {
        bool snapshot_item = false;

        if (item_selector_run_snapshot_loop(cp, pmt, use_inven, use_equip,
                use_floor, allow_inven, allow_equip, allow_floor, i1, i2, e1,
                e2, f1, f2, floor_list, floor_num, &toggle, &snapshot_item))
        {
            item = snapshot_item;
        }
        else
        {
            log_warn("item selector: snapshot interaction required; legacy selector removed");
            msg_print("Item selection requires active snapshot UI rendering.");
        }
    }

    p_ptr->command_see = false;

    app_session_pop_wait_scope(app_session_current(), &wait_scope);

    set_story_inventory_list_active(false);
    set_story_equipment_list_active(false);

    /* Forget whether inventory or equipment was being examined */
    p_ptr->command_wrk = 0;

    /* Forget whether inventory or equipment or floor or combinations were examinable */
    p_ptr->get_item_mode = 0;

    /* Forget the item_tester_tval restriction */
    item_tester_tval = 0;

    /* Forget the item_tester_hook restriction */
    item_tester_hook = NULL;

    /* Toggle again if needed */
    if (toggle)
        toggle_inven_equip();

    /* Update */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Window stuff */
    window_stuff();

    /* Clear the prompt line */
    prt("", 0, 0);

    /* Warning if needed */
    if (oops && str)
        msg_print(str);

#ifdef ALLOW_REPEAT
    /* Save item if available */
    if (item)
        repeat_push(*cp);
#endif

    /* Result */
    return (item);
}
