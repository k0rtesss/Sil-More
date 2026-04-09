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
#include "object/object-desc.h"
#include "object/object-slot.h"
#include "object/object-ui-display.h"
#include "object/object-ui-enhanced.h"
#include "object/object-ui-select.h"
#include "ui/story_font.h"

#include <ctype.h>

bool item_tester_full = false;
byte item_tester_tval = 0;
bool (*item_tester_hook)(const object_type*) = NULL;

typedef struct item_selector_menu_scene_scope {
    bool active;
    app_snapshot previous_snapshot;
} item_selector_menu_scene_scope;

static bool item_selector_snapshot_active(void)
{
    return app_session_interactions_enabled(app_session_current());
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

    scope->previous_snapshot = *snapshot;
    scope->active = true;
    return true;
}

static bool item_selector_menu_scene_present(item_selector_menu_scene_scope* scope)
{
    app_session* session = app_session_current();
    const app_interaction_state* interaction;
    app_ui_scene scene;
    app_ui_panel* panel;

    if (!scope || !scope->active || !session)
        return false;

    interaction = app_session_interaction(session);
    if (!interaction || interaction->kind == APP_INTERACTION_KIND_NONE)
        return false;

    if (!app_ui_scene_from_interaction(&scene, interaction))
        return false;

    scene.flags |= APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = (scene.panel_count > 0) ? &scene.panels[0] : NULL;
    if (!panel)
        return false;
    panel->min_width_px = MAX(panel->min_width_px, 340);

    if (!app_session_publish_menu_scene(session, &scene))
        return false;

    (void)Term_xtra(TERM_XTRA_FRESH, 0);
    return true;
}

static void item_selector_menu_scene_restore(
    item_selector_menu_scene_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session)
        return;

    app_session_set_snapshot(session, &scope->previous_snapshot);
    (void)Term_xtra(TERM_XTRA_FRESH, 0);
}

static void item_selector_suspend_snapshot_ui(
    item_selector_menu_scene_scope* scope)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot;

    item_selector_menu_scene_restore(scope);
    if (session)
    {
        app_session_clear_interaction(session);
        snapshot = app_session_snapshot(session);
        if (scope && scope->active && snapshot
            && snapshot->scene == APP_SCENE_KIND_DUNGEON)
        {
            scope->previous_snapshot = *snapshot;
        }
    }
}

static void item_selector_menu_scene_close(item_selector_menu_scene_scope* scope)
{
    if (!scope)
        return;

    item_selector_suspend_snapshot_ui(scope);
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

static void item_selector_sync_snapshot(cptr prompt, int current_mode,
    const int* floor_list, int vis_inven_cnt, const int* vis_inven,
    int vis_equip_cnt, const int* vis_equip, int vis_floor_cnt,
    const int* vis_floor, int highlight_row)
{
    app_session* session = app_session_current();
    char detail[APP_INTERACTION_TEXT_MAX];
    int i;

    if (!session)
        return;

    app_session_begin_interaction(session, APP_INTERACTION_KIND_LIST,
        APP_WAIT_REASON_LIST_SELECTION,
        APP_INTERACTION_FLAG_CAN_CONFIRM
            | APP_INTERACTION_FLAG_CAN_CANCEL
            | APP_INTERACTION_FLAG_SHOW_OPTIONS);
    app_session_set_interaction_prompt(session, TERM_WHITE, prompt);
    SDL_strlcpy(detail,
        "Enter/Space selects, x examines, / switches panes, - selects floor, Esc cancels.",
        sizeof(detail));
    app_session_set_interaction_detail(session, TERM_SLATE, detail);

    if (current_mode == (USE_INVEN))
    {
        for (i = 0; i < vis_inven_cnt; i++)
        {
            char label[APP_INTERACTION_LABEL_MAX];
            char meta[APP_INTERACTION_META_MAX];
            char tag = 'a';
            int item_index = vis_inven[i];

            label[0] = '\0';
            meta[0] = '\0';
            if (item_index == SUPPLIES_INDEX)
            {
                int virtual_slot = supplies_virtual_slot();

                format_supply_summary(label, sizeof(label));
                tag = supplies_label_char();
                if (!tag && virtual_slot >= 0)
                    tag = index_to_label(virtual_slot);
                if (!tag)
                    tag = 'a';
            }
            else
            {
                object_type* o_ptr = &inventory[item_index];

                object_desc(label, sizeof(label), o_ptr, true, 3);
                tag = index_to_label(item_index);
                item_selector_format_weight(meta, sizeof(meta), o_ptr);
            }

            (void)app_session_add_interaction_option(session, TERM_WHITE, tag,
                true, highlight_row == i, label, meta);
        }
    }
    else if (current_mode == (USE_EQUIP))
    {
        for (i = 0; i < vis_equip_cnt; i++)
        {
            char label[APP_INTERACTION_LABEL_MAX];
            char meta[APP_INTERACTION_META_MAX];
            char desc[80];
            int item_index = vis_equip[i];
            object_type* o_ptr = &inventory[item_index];

            if (o_ptr->k_idx)
                object_desc(desc, sizeof(desc), o_ptr, true, 3);
            else
                SDL_strlcpy(desc, "(empty slot)", sizeof(desc));
            strnfmt(label, sizeof(label), "%s: %s", mention_use(item_index),
                desc);
            item_selector_format_weight(meta, sizeof(meta), o_ptr);

            (void)app_session_add_interaction_option(session, TERM_WHITE,
                index_to_label(item_index), true, highlight_row == i, label,
                meta);
        }
    }
    else if (current_mode == (USE_FLOOR))
    {
        for (i = 0; i < vis_floor_cnt; i++)
        {
            char label[APP_INTERACTION_LABEL_MAX];
            char meta[APP_INTERACTION_META_MAX];
            int floor_slot = vis_floor[i];
            int object_index = floor_list[floor_slot];
            object_type* o_ptr = &o_list[object_index];

            object_desc_floor(label, sizeof(label), o_ptr, true, 3);
            item_selector_format_weight(meta, sizeof(meta), o_ptr);

            (void)app_session_add_interaction_option(session, TERM_WHITE,
                index_to_label(floor_slot), true, highlight_row == i, label,
                meta);
        }
    }

    app_session_set_interaction_selected(session,
        (highlight_row >= 0) ? (s16b)highlight_row : -1);
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

    char which;

    int i, j;
    int k = INVEN_WIELD; /* a default value to soothe compilation warnings */

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

    char out_val[160];

    int floor_list[MAX_FLOOR_STACK];
    int floor_num;
    bool snapshot_interaction = item_selector_snapshot_active();
    item_selector_menu_scene_scope menu_scene_scope;

    memset(&menu_scene_scope, 0, sizeof(menu_scene_scope));

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

    if (snapshot_interaction)
        p_ptr->command_see = true;

    if (snapshot_interaction)
        (void)item_selector_menu_scene_enter(&menu_scene_scope);

    /* Start out in "display" mode */
    if (p_ptr->command_see && !snapshot_interaction)
    {
        /* Save screen */
        screen_save();
    }

    /* Repeat until done */
    /* Row-based display mappings (built when list is visible) */
    int vis_inven[INVEN_PACK + 1];
    int vis_inven_cnt = 0;
    int vis_equip[INVEN_TOTAL - INVEN_WIELD];
    int vis_equip_cnt = 0;
    int vis_floor[MAX_FLOOR_STACK];
    int vis_floor_cnt = 0;

    int highlight_row = -1;
    bool highlight_active = false;

    /* Helper lambdas (C89 substitute: static inline style) defined as macros */
#define DRAW_HIGHLIGHT_STORY_VARS()                                                 \
        bool highlight_story_font = false;                                          \
        int highlight_story_w = 0;
#define DRAW_HIGHLIGHT_STORY_UPDATE()                                               \
        if (p_ptr->command_wrk == (USE_INVEN) || p_ptr->command_wrk == (USE_FLOOR)) \
            highlight_story_font = get_story_inventory_list_active();               \
        else if (p_ptr->command_wrk == (USE_EQUIP))                                 \
            highlight_story_font = get_story_equipment_list_active();               \
        if (highlight_story_font)                                                   \
        {                                                                           \
            int story_term_h = 0;                                                   \
            Term_get_size(&highlight_story_w, &story_term_h);                       \
        }
#define DRAW_HIGHLIGHT_IF_STORY(code)                                               \
    if (highlight_story_font) {                                                     \
        story_font_term_state highlight_story_state;                                \
        story_font_term_push(true, false, &highlight_story_state);                  \
        code;                                                                       \
        story_font_term_pop(&highlight_story_state);                                \
    } else
/* Build mapping arrays for currently selected list when visible */                 \
#define BUILD_VISIBLE_LIST()                                                         \
    do {                                                                            \
        if (!p_ptr->command_see) break;                                            \
        if (p_ptr->command_wrk == (USE_INVEN)) {                                    \
            vis_inven_cnt = 0;                                                      \
            bool has_supplies = supplies_visible_for_current_filter();              \
            if (has_supplies && vis_inven_cnt < INVEN_PACK) {                       \
                vis_inven[vis_inven_cnt++] = SUPPLIES_INDEX;                        \
            }                                                                       \
            for (int ii = 0; ii < INVEN_PACK && vis_inven_cnt < INVEN_PACK; ++ii) { \
                if (inventory[ii].k_idx && get_item_okay(ii)) {                     \
                    if (has_supplies && vis_inven_cnt >= INVEN_PACK) break;         \
                    vis_inven[vis_inven_cnt++] = ii;                                \
                }                                                                   \
            }                                                                       \
            if (!highlight_active && vis_inven_cnt > 0) {                           \
                highlight_row = 0; highlight_active = true;                         \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_EQUIP)) {                             \
            vis_equip_cnt = 0;                                                      \
            for (int ii = INVEN_WIELD; ii < INVEN_TOTAL; ++ii) {                    \
                bool include_slot = false;                                          \
                if (inventory[ii].k_idx) {                                          \
                    include_slot = get_item_okay(ii);                               \
                } else if (throw_slot_menu_active && throw_slot_enabled[ii]) {      \
                    include_slot = true;                                            \
                }                                                                   \
                if (include_slot) {                                                 \
                    vis_equip[vis_equip_cnt++] = ii;                                \
                }                                                                   \
            }                                                                       \
            if (!highlight_active && vis_equip_cnt > 0) {                           \
                highlight_row = 0; highlight_active = true;                         \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_FLOOR)) {                             \
            vis_floor_cnt = 0;                                                      \
            for (int ii = 0; ii < floor_num; ++ii) {                                \
                int obj_idx = floor_list[ii];                                       \
                if (get_item_okay(0 - obj_idx)) {                                   \
                    vis_floor[vis_floor_cnt++] = ii;                                \
                }                                                                   \
            }                                                                       \
            if (!highlight_active && vis_floor_cnt > 0) {                           \
                highlight_row = 0; highlight_active = true;                         \
            }                                                                       \
        }                                                                           \
    } while (0)
#define MOVE_HIGHLIGHT(dir)                                                          \
    do {                                                                            \
        if (!highlight_active) break;                                               \
        if (p_ptr->command_wrk == (USE_INVEN) && vis_inven_cnt > 0) {               \
            highlight_row = (highlight_row + (vis_inven_cnt) + (dir)) % vis_inven_cnt; \
        } else if (p_ptr->command_wrk == (USE_EQUIP) && vis_equip_cnt > 0) {       \
            highlight_row = (highlight_row + (vis_equip_cnt) + (dir)) % vis_equip_cnt; \
        } else if (p_ptr->command_wrk == (USE_FLOOR) && vis_floor_cnt > 0) {       \
            highlight_row = (highlight_row + (vis_floor_cnt) + (dir)) % vis_floor_cnt; \
        }                                                                           \
    } while (0)

    /* Draw highlight: re-render the line with reversed attr marker */
#define DRAW_HIGHLIGHT()                                                             \
    do {                                                                            \
        if (!highlight_active || !p_ptr->command_see) break;                        \
        byte attr = TERM_L_BLUE;                                                    \
        int col = 0;                                                                \
        int term_wid = menu_term_width();                                           \
        int weight_col = menu_weight_col_for_width(term_wid);                       \
        int label_col_base = menu_label_col_for_width(term_wid, show_weights);      \
        int len = 29;                                                               \
        int lim = term_wid - 3;                                                     \
        char tmp[80];                                                               \
        DRAW_HIGHLIGHT_STORY_VARS()                                                 \
        DRAW_HIGHLIGHT_STORY_UPDATE()                                               \
        if (show_weights && lim > (weight_col - 1)) lim = weight_col - 1;          \
        if (p_ptr->command_wrk == (USE_EQUIP)) { lim -= (14 + 2); }                 \
        if (lim < 0) lim = 0;                                                       \
        if (p_ptr->command_wrk == (USE_INVEN)) {                                    \
            for (int r=0;r<vis_inven_cnt;r++){                                      \
                int entry = vis_inven[r];                                           \
                if (entry == SUPPLIES_INDEX) {                                      \
                    format_supply_summary(tmp, sizeof(tmp));                         \
                } else {                                                            \
                    object_type* o_ptr=&inventory[entry];                           \
                    object_desc(tmp, sizeof(tmp), o_ptr, true, 3);                  \
                }                                                                   \
                tmp[lim]='\0';                                                      \
                int ltmp = (entry == SUPPLIES_INDEX)                                \
                    ? menu_inventory_row_width(tmp, NULL, show_weights)             \
                    : menu_inventory_row_width(tmp, &inventory[entry], show_weights); \
                if (ltmp>len) len=ltmp;                                             \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_EQUIP)) {                             \
            for (int r=0;r<vis_equip_cnt;r++){                                      \
                object_type* o_ptr=&inventory[vis_equip[r]];                        \
                object_desc(tmp, sizeof(tmp), o_ptr, true, 3);                      \
                tmp[lim]='\0';                                                      \
                int ltmp = menu_equipment_row_width(tmp,                            \
                    o_ptr->k_idx ? o_ptr : NULL, show_weights);                     \
                if (ltmp>len) len=ltmp;                                             \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_FLOOR)) {                             \
            for (int r=0;r<vis_floor_cnt;r++){                                      \
                object_type* o_ptr=&o_list[floor_list[vis_floor[r]]];               \
                object_desc_floor(tmp, sizeof(tmp), o_ptr, true, 3);                \
                tmp[lim]='\0';                                                      \
                int ltmp = menu_inventory_row_width(tmp, o_ptr, show_weights);      \
                if (ltmp>len) len=ltmp;                                             \
            }                                                                       \
        }                                                                           \
        col = menu_center_col_for_len(term_wid, len);                               \
        int row=-1; int item_index=0; int floor_slot=-1;                            \
        if (p_ptr->command_wrk == (USE_INVEN) && highlight_row < vis_inven_cnt) {   \
            row = highlight_row; item_index = vis_inven[highlight_row];             \
            prt("", row + 1, menu_overlay_clear_col(col));                          \
            int label_col = label_col_base;                                         \
            if (item_index == SUPPLIES_INDEX) {                                     \
                char label = supplies_label_char();                                 \
                int slot = supplies_virtual_slot();                                 \
                if (!label && slot >= 0) label = index_to_label(slot);              \
                if (!label) label = 'a';                                            \
                format_supply_summary(tmp, sizeof(tmp));                            \
                tmp[lim]='\0';                                                      \
                DRAW_HIGHLIGHT_IF_STORY({                                           \
                    char lab[8]; sprintf(lab, "(%c)", label);                       \
                    char wbuf[16]; cptr wptr = NULL;                                \
                    if (show_weights) {                                             \
                        int wgt = supplies_total_weight();                          \
                        strnfmt(wbuf, sizeof(wbuf), "%2d.%1d lb", wgt / 10, wgt % 10); \
                        wptr = wbuf;                                                \
                    }                                                               \
                    story_render_inventory_entry(row + 1, col, label_col, tmp, attr, \
                        show_weights, wptr, attr, lab, attr, NULL, true, highlight_story_w); \
                })                                                                  \
                {                                                                   \
                    c_put_str(attr,tmp,row+1,col);                                  \
                    if (show_weights){ int wgt = supplies_total_weight(); char w[16]; strnfmt(w, sizeof(w), "%2d.%1d lb", wgt / 10, wgt % 10); c_put_str(attr,w,row+1,weight_col);} \
                    { char lab[8]; sprintf(lab, " (%c)", label); c_put_str(attr,lab,row+1,label_col); } \
                }                                                                   \
            } else {                                                                \
                object_type* o_ptr=&inventory[item_index];                          \
                object_desc(tmp,sizeof(tmp),o_ptr,true,3); tmp[lim]='\0';           \
                DRAW_HIGHLIGHT_IF_STORY({                                           \
                    char lab[8]; sprintf(lab, "(%c)", index_to_label(item_index));  \
                    char wbuf[16]; cptr wptr = NULL;                                \
                    if (show_weights){ int wgt= o_ptr->weight*o_ptr->number; strnfmt(wbuf, sizeof(wbuf), "%2d.%1d lb", wgt / 10, wgt % 10); wptr = wbuf; } \
                    story_render_inventory_entry(row + 1, col, label_col, tmp, attr, \
                        show_weights, wptr, attr, lab, attr, o_ptr, true, highlight_story_w); \
                })                                                                  \
                {                                                                   \
                    int text_col = draw_item_tile(col, row+1, o_ptr);               \
                    c_put_str(attr,tmp,row+1,text_col);                             \
                    if (show_weights){ int wgt= o_ptr->weight*o_ptr->number; char w[16]; strnfmt(w, sizeof(w), "%2d.%1d lb", wgt / 10, wgt % 10); c_put_str(attr,w,row+1,weight_col);} \
                    { char lab[8]; sprintf(lab, " (%c)", index_to_label(item_index)); c_put_str(attr,lab,row+1,label_col); } \
                }                                                                   \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row < vis_equip_cnt){ \
            row = highlight_row; item_index = vis_equip[highlight_row];             \
            object_type* o_ptr=&inventory[item_index];                              \
            object_desc(tmp,sizeof(tmp),o_ptr,true,3); tmp[lim]='\0';               \
            prt("", row + 1, menu_overlay_clear_col(col));                          \
            { char usebuf[32]; strnfmt(usebuf,sizeof(usebuf),"%-12s: ", mention_use(item_index)); \
              DRAW_HIGHLIGHT_IF_STORY({                                             \
                  char lab[8]; sprintf(lab, "(%c)", index_to_label(item_index));    \
                  char wbuf[16]; cptr wptr = NULL;                                  \
                  if (show_weights && o_ptr->weight) {                              \
                      int wgt=o_ptr->weight*o_ptr->number;                          \
                      sprintf(wbuf,"%2d.%1d lb",wgt/10,wgt%10);                     \
                      wptr = wbuf;                                                  \
                  }                                                                 \
                  story_render_equipment_entry(row + 1, col, item_index, usebuf, attr, tmp, attr, \
                      show_weights, wptr, attr, lab, attr, o_ptr, true, highlight_story_w); \
              })                                                                    \
              {                                                                     \
                  c_put_str(attr,usebuf,row+1,col);                                 \
                  int text_col = draw_item_tile(col+12+2, row+1, o_ptr);            \
                  c_put_str(attr,tmp,row+1,text_col);                               \
                  if (show_weights && o_ptr->weight){ int wgt=o_ptr->weight*o_ptr->number; char w[16]; sprintf(w,"%2d.%1d lb",wgt/10,wgt%10); c_put_str(attr,w,row+1,weight_col);} \
                  { char lab[8]; sprintf(lab, " (%c)", index_to_label(item_index)); int label_col = label_col_base; c_put_str(attr,lab,row+1,label_col); } \
              }                                                                     \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row < vis_floor_cnt){ \
            row = highlight_row; floor_slot = vis_floor[highlight_row];             \
            int obj_idx = floor_list[floor_slot];                                   \
            object_type* o_ptr=&o_list[obj_idx];                                    \
            object_desc_floor(tmp,sizeof(tmp),o_ptr,true,3); tmp[lim]='\0';         \
            prt("", row+1, col);                                                    \
            int label_col = label_col_base;                                         \
            DRAW_HIGHLIGHT_IF_STORY({                                               \
                char lab[8]; sprintf(lab, "(%c)", index_to_label(floor_slot));      \
                char wbuf[16]; cptr wptr = NULL;                                    \
                if (show_weights){ int wgt=o_ptr->weight*o_ptr->number; strnfmt(wbuf, sizeof(wbuf), "%2d.%1d lb", wgt / 10, wgt % 10); wptr = wbuf; } \
                story_render_inventory_entry(row + 1, col, label_col, tmp, attr,    \
                    show_weights, wptr, attr, lab, attr, o_ptr, true, highlight_story_w); \
            })                                                                      \
            {                                                                       \
                int text_col = draw_item_tile(col, row+1, o_ptr);                   \
                c_put_str(attr,tmp,row+1,text_col);                                 \
                if (show_weights){ int wgt=o_ptr->weight*o_ptr->number; char w[16]; strnfmt(w, sizeof(w), "%2d.%1d lb", wgt / 10, wgt % 10); c_put_str(attr,w,row+1,weight_col);} \
                { char lab[8]; sprintf(lab, " (%c)", index_to_label(floor_slot)); c_put_str(attr,lab,row+1,label_col); } \
            }                                                                       \
        }                                                                           \
    } while (0)

    while (!done)
    {
        int ni = 0;
        int ne = 0;

        /* Scan windows */
        for (j = 0; j < ANGBAND_TERM_MAX; j++)
        {
            /* Unused */
            if (!angband_term[j])
                continue;

            /* Count windows displaying inven */
            if (op_ptr->window_flag[j] & (PW_INVEN))
                ni++;

            /* Count windows displaying equip */
            if (op_ptr->window_flag[j] & (PW_EQUIP))
                ne++;
        }

        /* Toggle if needed */
        if (((p_ptr->command_wrk == (USE_EQUIP)) && ni && !ne)
            || ((p_ptr->command_wrk == (USE_INVEN)) && !ni && ne))
        {
            /* Toggle */
            toggle_inven_equip();

            /* Track toggles */
            toggle = !toggle;
        }

        /* Update */
        p_ptr->window |= (PW_INVEN | PW_EQUIP);

        /* Redraw windows */
        window_stuff();

        /* Build visible list mappings for current pane */
        BUILD_VISIBLE_LIST();

        /* Viewing inventory */
        if (p_ptr->command_see && !snapshot_interaction)
        {
            /* Inventory screen */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                /* Show the inventory */
                show_inven();
            }

            /* Equipment screen */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                /* Show the equipment */
                show_equip();
            }

            /* Floor screen */
            else if (p_ptr->command_wrk == (USE_FLOOR))
            {
                /* Show the floor */
                show_floor(floor_list, floor_num);
            }
        }

        if (p_ptr->command_see && !snapshot_interaction)
        {
            DRAW_HIGHLIGHT();
        }

        /* Build the prompt */
        if (p_ptr->command_see)
        {
            /* Viewing inventory */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                strnfmt(out_val, sizeof(out_val), 
                    "(Inven:%c-%c, ESC, %s) %s",
                    index_to_label(i1), index_to_label(i2),
                    use_equip ? "/ for Equip" : "- for floor,", pmt);
            }

            /* Viewing equipment */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                strnfmt(out_val, sizeof(out_val), 
                    "(Equip:%c-%c, ESC, %s) %s",
                    index_to_label(e1), index_to_label(e2),
                    use_inven ? "/ for Inven" : "- for floor,", pmt);
            }

            /* Viewing floor */
            else
            {
                strnfmt(out_val, sizeof(out_val), 
                    "(Floor:%c-%c, ESC, %s) %s",
                    index_to_label(f1), index_to_label(f2),
                    use_inven ? "/ for Inven" : use_equip ? "/ for Equip" : "",
                    pmt);
            }
        }

        /* Not viewing inventory */
        else
        {
            /* Prompt */
            strnfmt(out_val, sizeof(out_val), "(Items, ESC) %s", pmt);
        }

        if (snapshot_interaction)
        {
            item_selector_sync_snapshot(out_val, p_ptr->command_wrk,
                floor_list, vis_inven_cnt, vis_inven, vis_equip_cnt, vis_equip,
                vis_floor_cnt, vis_floor, highlight_active ? highlight_row : -1);
            (void)item_selector_menu_scene_present(&menu_scene_scope);
        }
        else
            put_str(out_val, 0, 0);

        /* Get a key */
        which = inkey();

        switch (which)
        {
        case ESCAPE:
            done = true;
            break;

        case '*':
        case '?':
        case ' ':
        {
            bool handled_space = false;

            /* Space confirms current highlighted selection if list visible */
            if (which == ' ' && p_ptr->command_see && highlight_active)
            {
                bool have_selection = false;

                if (p_ptr->command_wrk == (USE_INVEN) && highlight_row >= 0
                    && highlight_row < vis_inven_cnt)
                {
                    k = vis_inven[highlight_row];
                    have_selection = true;
                }
                else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row >= 0
                    && highlight_row < vis_equip_cnt)
                {
                    k = vis_equip[highlight_row];
                    have_selection = true;
                }
                else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row >= 0
                    && highlight_row < vis_floor_cnt)
                {
                    int obj_idx = floor_list[vis_floor[highlight_row]];
                    k = 0 - obj_idx;
                    have_selection = true;
                }

                if (have_selection)
                {
                    if (snapshot_interaction)
                        item_selector_suspend_snapshot_ui(&menu_scene_scope);

                    if ((k >= 0 && k < INVEN_WIELD && !allow_inven) ||
                        (k >= INVEN_WIELD && k < INVEN_TOTAL && !allow_equip) ||
                        (k < 0 && !allow_floor) ||
                        !get_item_okay(k) ||
                        !get_item_allow(k))
                    {
                        have_selection = false;
                    }
                }

                if (have_selection)
                {
                    (*cp) = k;
                    item = true;
                    done = true;
                    handled_space = true;
                }
            }

            if (handled_space)
                break;

            if (snapshot_interaction)
                break;

            /* Hide the list */
            if (p_ptr->command_see)
            {
                /* Flip flag */
                p_ptr->command_see = false;

                /* Load screen */
                screen_load();
            }

            /* Show the list */
            else
            {
                /* Save screen */
                screen_save();

                /* Flip flag */
                p_ptr->command_see = true;
            }

            break;
        }

        case '/':
        {
            /* Toggle to inventory */
            if (use_inven && (p_ptr->command_wrk != (USE_INVEN)))
            {
                p_ptr->command_wrk = (USE_INVEN);
            }

            /* Toggle to equipment */
            else if (use_equip && (p_ptr->command_wrk != (USE_EQUIP)))
            {
                p_ptr->command_wrk = (USE_EQUIP);
            }

            /* No toggle allowed */
            else
            {
                bell("Cannot switch item selector!");
                break;
            }

            /* Hack -- Fix screen */
            if (p_ptr->command_see && !snapshot_interaction)
            {
                /* Load screen */
                screen_load();

                /* Save screen */
                screen_save();
            }

            /* Need to redraw */
            break;
        }

        case '-':
        {
            /* Paranoia */
            if (!allow_floor)
            {
                bell("Cannot select floor!");
                break;
            }

            /* Check each legal object */
            for (i = 0; i < floor_num; ++i)
            {
                /* Special index */
                k = 0 - floor_list[i];

                /* Skip non-okay objects */
                if (!get_item_okay(k))
                    continue;

                /* Allow player to "refuse" certain actions */
                if (snapshot_interaction)
                    item_selector_suspend_snapshot_ui(&menu_scene_scope);

                if (!get_item_allow(k))
                    continue;

                /* Accept that choice */
                (*cp) = k;
                item = true;
                done = true;
                break;
            }

            break;
        }

        case 'x':
        case 'X':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
        {
            if (p_ptr->command_see && highlight_active)
            {
                int examine_index = 0;
                bool have_selection = false;

                if (p_ptr->command_wrk == (USE_INVEN) && highlight_row >= 0 && highlight_row < vis_inven_cnt)
                {
                    examine_index = vis_inven[highlight_row];
                    have_selection = true;
                }
                else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row >= 0 && highlight_row < vis_equip_cnt)
                {
                    examine_index = vis_equip[highlight_row];
                    have_selection = true;
                }
                else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row >= 0 && highlight_row < vis_floor_cnt)
                {
                    int obj_idx = floor_list[vis_floor[highlight_row]];
                    examine_index = 0 - obj_idx;
                    have_selection = true;
                }

                if (have_selection)
                {
                    if (snapshot_interaction)
                        item_selector_suspend_snapshot_ui(&menu_scene_scope);
                    describe_item_with_comparisons(examine_index, true);
                }
                else
                {
                    bell("Nothing is selected to examine.");
                }
            }
            else
            {
                bell("No highlighted item to examine.");
            }

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
            bool tag_found = get_tag(&k, which);
            if (!tag_found && p_ptr->command_see && highlight_active
                && (which == '2' || which == '8' || which == '6'))
            {
                /* Numpad navigation mode like main menu */
                if (which == '8')
                {
                    MOVE_HIGHLIGHT(-1);
                    break;
                }
                if (which == '2')
                {
                    MOVE_HIGHLIGHT(+1);
                    break;
                }
                if (which == '6')
                {
                    /* map row to actual item */
                    if (p_ptr->command_wrk == (USE_INVEN) && highlight_row < vis_inven_cnt)
                    {
                        k = vis_inven[highlight_row];
                    }
                    else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row < vis_equip_cnt)
                    {
                        k = vis_equip[highlight_row];
                    }
                    else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row < vis_floor_cnt)
                    {
                        int obj_idx = floor_list[vis_floor[highlight_row]];
                        k = 0 - obj_idx;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else if (!tag_found)
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Hack -- Validate the item */
            if ((k < INVEN_WIELD) ? !allow_inven : !allow_equip)
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (snapshot_interaction)
                item_selector_suspend_snapshot_ui(&menu_scene_scope);

            if (!get_item_allow(k))
            {
                done = true;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = true;
            done = true;
            break;
        }

        case '[':
        case ']':
        {
            bool item_found = false;

            /* Convert letter to inventory index */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                for (i = INVEN_PACK; i >= 0; i--)
                {
                    if (get_item_okay(i) && ((which == '[') || !item_found))
                    {
                        k = i;
                        item_found = true;
                    }
                }
            }

            /* Convert letter to equipment index */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
                {
                    if (get_item_okay(i) && ((which == ']') || !item_found))
                    {
                        k = i;
                        item_found = true;
                    }
                }
            }

            /* Hack -- Validate the item */
            if ((k < INVEN_WIELD) ? !allow_inven : !allow_equip)
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Validate the item */
            if (!item_found)
            {
                bell("No valid items found.");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (snapshot_interaction)
                item_selector_suspend_snapshot_ui(&menu_scene_scope);

            if (!get_item_allow(k))
            {
                done = true;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = true;
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            /* If we have an active highlight, use it like main menu selection */
            if (highlight_active)
            {
                if (p_ptr->command_wrk == (USE_INVEN) && highlight_row < vis_inven_cnt)
                {
                    k = vis_inven[highlight_row];
                }
                else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row < vis_equip_cnt)
                {
                    k = vis_equip[highlight_row];
                }
                else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row < vis_floor_cnt)
                {
                    int obj_idx = floor_list[vis_floor[highlight_row]];
                    k = 0 - obj_idx;
                }
                else
                {
                    break;
                }
                if (!get_item_okay(k)) { bell("Illegal object choice (highlight)!"); break; }
                if (snapshot_interaction) {
                    item_selector_suspend_snapshot_ui(&menu_scene_scope);
                }
                if (!get_item_allow(k)) { done = true; break; }
                (*cp)=k; item=true; done=true; break;
            }

            /* Choose "default" inventory item */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                if (i1 != i2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = i1;
            }

            /* Choose "default" equipment item */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                if (e1 != e2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = e1;
            }

            /* Choose "default" floor item */
            else
            {
                if (f1 != f2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = 0 - floor_list[f1];
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (default)!");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (snapshot_interaction)
                item_selector_suspend_snapshot_ui(&menu_scene_scope);

            if (!get_item_allow(k))
            {
                done = true;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = true;
            done = true;
            break;
        }

        default:
        {
            bool verify;

            /* Allow numpad navigation keys here too if list visible */
            if (p_ptr->command_see && highlight_active) {
                if (which == '8') { MOVE_HIGHLIGHT(-1); break; }
                if (which == '2') { MOVE_HIGHLIGHT(+1); break; }
                if (which == '6') { /* select */
                    if (p_ptr->command_wrk == (USE_INVEN) && highlight_row < vis_inven_cnt)
                        k = vis_inven[highlight_row];
                    else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row < vis_equip_cnt)
                        k = vis_equip[highlight_row];
                    else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row < vis_floor_cnt) {
                        int obj_idx = floor_list[vis_floor[highlight_row]];
                        k = 0 - obj_idx;
                    }
                    else break;

                    if (!get_item_okay(k)) {
                        bell("Illegal object choice (highlight)!");
                        break;
                    }
                    if (snapshot_interaction) {
                        item_selector_suspend_snapshot_ui(&menu_scene_scope);
                    }
                    if (!get_item_allow(k)) {
                        done=true;
                        break;
                    }
                    (*cp)=k;
                    item=true;
                    done=true;
                    break;
                }
            }

            /* Note verify */
            verify = (isupper((unsigned char)which) ? true : false);

            /* Lowercase */
            which = tolower((unsigned char)which);

            /* Convert letter to inventory index */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                k = label_to_inven(which);

                if (k < 0)
                {
                    bell("Illegal object choice (inven)!");
                    break;
                }
            }

            /* Convert letter to equipment index */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                k = label_to_equip(which);

                if (k < 0)
                {
                    bell("Illegal object choice (equip)!");
                    break;
                }
            }

            /* Convert letter to floor index */
            else
            {
                k = (islower((unsigned char)which) ? A2I(which) : -1);

                if (k < 0 || k >= floor_num)
                {
                    bell("Illegal object choice (floor)!");
                    break;
                }

                /* Special index */
                k = 0 - floor_list[k];
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (normal)!");
                break;
            }

            /* Verify the item */
            if (snapshot_interaction)
                item_selector_suspend_snapshot_ui(&menu_scene_scope);

            if (verify && !verify_item("Try", k))
            {
                done = true;
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (snapshot_interaction)
                item_selector_suspend_snapshot_ui(&menu_scene_scope);

            if (!get_item_allow(k))
            {
                done = true;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = true;
            done = true;
            break;
        }
        }
    }

#undef BUILD_VISIBLE_LIST
#undef MOVE_HIGHLIGHT
#undef DRAW_HIGHLIGHT
#undef DRAW_HIGHLIGHT_STORY_VARS
#undef DRAW_HIGHLIGHT_STORY_UPDATE
#undef DRAW_HIGHLIGHT_IF_STORY

    /* Fix the screen if necessary */
    if (p_ptr->command_see)
    {
        /* Load screen */
        if (!snapshot_interaction)
            screen_load();

        /* Hack -- Cancel "display" */
        p_ptr->command_see = false;
    }

    if (snapshot_interaction)
        item_selector_menu_scene_close(&menu_scene_scope);
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
