/* File: cmd-ui-look.c */

/*
 * Copyright (c) 2001 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-session.h"
#include "externs.h"
#include "log/log.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "metarun.h"
#include "object/object-ui-enhanced.h"
#include "ui/ui-information-scene.h"
#include "ui/ui-look-sidebar.h"

void do_cmd_target(void)
{
    /* Target set */
    if (target_set_interactive(TARGET_KILL, 0))
    {
        msg_print("Target Selected.");
    }

    /* Target aborted */
    else
    {
        msg_print("Target Aborted.");
    }
}

/*
 * Calculate the bounding box of explored areas, detected monsters, and detected objects.
 * Returns true if any explored area or detected entity found, false otherwise
 * 
 * This function includes positions of monsters detected by items like the
 * Gem of Foes (which have MFLAG_MARK set) in the scrollable bounds, and
 * positions of marked objects (e.g., from Gem of Treasures / detection).
 */
static bool get_explored_bounds(int* min_y, int* max_y, int* min_x, int* max_x)
{
    int y, x, i;
    
    *min_x = p_ptr->cur_map_wid;
    *max_x = 0;
    *min_y = p_ptr->cur_map_hgt;
    *max_y = 0;

    /* Check explored grids */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Check if this grid has been seen */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                if (x < *min_x) *min_x = x;
                if (x > *max_x) *max_x = x;
                if (y < *min_y) *min_y = y;
                if (y > *max_y) *max_y = y;
            }
        }
    }

    /* Also include detected monsters (e.g., from Gem of Foes) */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        
        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;
        
        /* Check if monster is detected (MFLAG_MARK set by detection spells/items) */
        if (m_ptr->mflag & (MFLAG_MARK))
        {
            int my = m_ptr->fy;
            int mx = m_ptr->fx;
            
            if (mx < *min_x) *min_x = mx;
            if (mx > *max_x) *max_x = mx;
            if (my < *min_y) *min_y = my;
            if (my > *max_y) *max_y = my;
        }
    }

    /* Also include marked objects (e.g., from Gem of Treasures / object detection) */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip held objects */
        if (o_ptr->held_m_idx)
            continue;

        /* Only include marked (detected/memorized) objects */
        if (!o_ptr->marked)
            continue;

        int oy = o_ptr->iy;
        int ox = o_ptr->ix;
        if (!in_bounds_fully(oy, ox))
            continue;

        if (ox < *min_x) *min_x = ox;
        if (ox > *max_x) *max_x = ox;
        if (oy < *min_y) *min_y = oy;
        if (oy > *max_y) *max_y = oy;
    }

    /* Check if any explored area or detected entity was found */
    return (*min_x <= *max_x && *min_y <= *max_y);
}

/*
 * Look command
 */
static bool g_unified_look_has_start = false;
static int g_unified_look_start_y = 0;
static int g_unified_look_start_x = 0;
static char g_unified_look_snapshot_prompt[APP_INTERACTION_TEXT_MAX];
static int g_unified_look_snapshot_prompt_cols = 0;

static bool unified_look_snapshot_active(void)
{
    return app_session_interactions_enabled(app_session_current());
}

static void unified_look_snapshot_clear(void)
{
    g_unified_look_snapshot_prompt[0] = '\0';
    g_unified_look_snapshot_prompt_cols = 0;

    if (!unified_look_snapshot_active())
        return;

    app_session_clear_interaction(app_session_current());
}

static bool unified_look_snapshot_publish_overlay(int overlay_rows,
    int overlay_cols)
{
    app_session* session = app_session_current();
    app_raw_cell_snapshot captured[APP_INTERACTION_PANEL_ROW_MAX]
                                  [APP_INTERACTION_PANEL_COL_MAX];
    int rows;
    int cols;
    int y;
    int x;

    if (!unified_look_snapshot_active() || !session || !Term || !Term->scr)
        return false;

    app_session_begin_interaction(session, APP_INTERACTION_KIND_LOOK,
        APP_WAIT_REASON_TARGETING, APP_INTERACTION_FLAG_CAN_CANCEL);
    if (g_unified_look_snapshot_prompt_cols > 0
        && g_unified_look_snapshot_prompt[0])
    {
        app_session_set_interaction_prompt(session, TERM_WHITE,
            g_unified_look_snapshot_prompt);
    }

    rows = MIN(overlay_rows, (int)APP_INTERACTION_PANEL_ROW_MAX);
    cols = MIN(overlay_cols, (int)APP_INTERACTION_PANEL_COL_MAX);
    if (rows <= 0 || cols <= 0)
        return true;

    memset(captured, 0, sizeof(captured));
    for (y = 0; y < rows; y++)
    {
        int term_row = y + 1;

        if (term_row < 0 || term_row >= Term->hgt)
            break;

        for (x = 0; x < cols; x++)
        {
            captured[y][x].attr = Term->scr->a[term_row][x];
            captured[y][x].story = Term->scr->story[term_row][x];
            captured[y][x].ch = Term->scr->c[term_row][x];
        }
    }

    return app_session_set_interaction_panel(session, 1, 0, (u16b)rows,
        (u16b)cols, &captured[0][0], APP_INTERACTION_PANEL_COL_MAX);
}

static char unified_look_inkey_with_wait_reason(void)
{
    app_wait_scope scope;
    app_session* session = app_session_current();
    char ch;

    app_session_push_wait_scope(session, &scope,
        APP_WAIT_REASON_TARGETING, 0, 0);
    ch = inkey();
    app_session_pop_wait_scope(session, &scope);
    return ch;
}

static void unified_look_wait_for_information(void)
{
    app_wait_scope wait_scope;

    app_session_push_wait_scope(app_session_current(), &wait_scope,
        APP_WAIT_REASON_INFORMATIONAL_PAUSE, 0, 0);
    (void)inkey();
    app_session_pop_wait_scope(app_session_current(), &wait_scope);
}

static void unified_look_show_monster_recall(const monster_type* m_ptr)
{
    if (!m_ptr)
        return;

    if (unified_look_snapshot_active())
    {
        ui_information_scene_scope info_scope;

        unified_look_snapshot_clear();
        if (!ui_information_scene_enter_mirror(&info_scope))
        {
            log_error("unified-look: snapshot recall scene could not enter "
                "mirror scope");
            bell("Monster recall screen unavailable.");
            return;
        }

        screen_roff(m_ptr->r_idx, m_ptr);
        if (!ui_information_scene_present_term())
        {
            ui_information_scene_leave(&info_scope);
            log_error("unified-look: snapshot recall scene could not "
                "capture/present");
            bell("Monster recall screen unavailable.");
            return;
        }

        (void)ui_information_scene_wait_key();
        ui_information_scene_leave(&info_scope);
        return;
    }

    screen_save();
    screen_roff(m_ptr->r_idx, m_ptr);
    unified_look_wait_for_information();
    screen_load();
}

static void unified_look_show_object_info(const object_type* o_ptr)
{
    if (!o_ptr)
        return;

    if (wield_slot(o_ptr) >= INVEN_WIELD && wield_slot(o_ptr) < INVEN_TOTAL)
    {
        int slot = wield_slot(o_ptr);
        const object_type* compare_objects[2];
        const char* compare_headings[2];
        char selected_heading[32];
        char equipped_heading[32];

        strnfmt(selected_heading, sizeof(selected_heading), "Selected item");
        strnfmt(equipped_heading, sizeof(equipped_heading), "%s",
            mention_use(slot));

        compare_objects[0] = o_ptr;
        compare_headings[0] = selected_heading;
        compare_headings[1] = equipped_heading;
        compare_objects[1] = inventory[slot].k_idx ? &inventory[slot] : NULL;

        object_info_screen_multi(compare_objects, compare_headings, 2);
        return;
    }

    object_info_screen(o_ptr);
}

void do_cmd_look_at(int y, int x)
{
    if (y < 0 || y >= p_ptr->cur_map_hgt || x < 0 || x >= p_ptr->cur_map_wid)
    {
        do_cmd_look();
        return;
    }

    g_unified_look_has_start = true;
    g_unified_look_start_y = y;
    g_unified_look_start_x = x;
    do_cmd_look();
    g_unified_look_has_start = false;
}

void do_cmd_look(void)
{
    /* Block when hallucinating */
    if (p_ptr->image)
    {
        msg_print("Your vision is too distorted to examine things carefully.");
        return;
    }

    /* Use the new unified look system */
    do_cmd_unified_look();
}

/*
 * Unified look command - combines look, scroll, and view functionality
 */

static int unified_sidebar_object_group(const object_type* o_ptr)
{
    if (!o_ptr)
        return LOOK_GROUP_OTHER;

    if (artefact_p(o_ptr))
        return LOOK_GROUP_ARTIFACT;

    switch (o_ptr->tval)
    {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOW:
    case TV_DIGGING:
    case TV_ARROW:
        return LOOK_GROUP_WEAPON;

    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return LOOK_GROUP_ARMOUR;

    case TV_RING:
    case TV_AMULET:
    case TV_HORN:
    case TV_STAFF:
        return LOOK_GROUP_JEWELRY;

    case TV_EASTER:
        return LOOK_GROUP_HERBS;

    case TV_POTION:
        return LOOK_GROUP_POTIONS;

    case TV_GEM:
        return LOOK_GROUP_GEMS;

    case TV_FOOD:
        if (o_ptr->sval < SV_FOOD_MIN_FOOD)
            return LOOK_GROUP_CONSUMABLE;
        break;
    }

    return LOOK_GROUP_OTHER;
}

static bool unified_look_can_show_monster_at(int y, int x)
{
    int m_idx = cave_m_idx[y][x];

    return (m_idx > 0) && mon_list[m_idx].ml && grid_info_is_available(y, x);
}

static bool unified_look_can_show_marked_object_at(int y, int x)
{
    int o_idx = cave_o_idx[y][x];

    return (o_idx > 0) && o_list[o_idx].marked && grid_info_is_available(y, x);
}

static int unified_look_count_visible_entities(unified_look_state* state)
{
    int total_entities = 0;
    int i;

    if (state->show_monsters)
    {
        get_sorted_target_list(TARGET_LIST_MONSTER, 0);

        for (i = 0; i < temp_n; i++)
        {
            int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];

            if (!m_idx) continue;
            if (!unified_look_can_show_monster_at(temp_y[i], temp_x[i])) continue;

            total_entities++;
        }
    }

    if (state->show_objects)
    {
        int group_counts[LOOK_GROUP_COUNT] = {0};

        get_sorted_target_list(TARGET_LIST_OBJECT, 0);

        for (i = 0; i < temp_n; i++)
        {
            int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
            if (!o_idx)
                continue;

            if (!grid_info_is_available(temp_y[i], temp_x[i]))
                continue;

            object_type* o_ptr = &o_list[o_idx];

            /* Only count marked (memorized) objects (matches sidebar display) */
            if (!o_ptr->marked)
                continue;

            if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
                continue;

            int group = unified_sidebar_object_group(o_ptr);
            if (state->object_group_filter >= 0 && group != state->object_group_filter)
                continue;
            if (state->limit_objects_top_five && group_counts[group] >= 5)
                continue;

            group_counts[group]++;
            total_entities++;
        }
    }

    return total_entities;
}

static int unified_look_count_visible_objects_for_group(unified_look_state* state, int group_filter)
{
    int total_objects = 0;
    int i;

    if (!state)
        return 0;

    int group_counts[LOOK_GROUP_COUNT] = {0};

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        if (!o_idx)
            continue;

        if (!grid_info_is_available(temp_y[i], temp_x[i]))
            continue;

        object_type* o_ptr = &o_list[o_idx];

        /* Only count marked (memorized) objects (matches sidebar display) */
        if (!o_ptr->marked)
            continue;

        if ((o_ptr->tval == TV_ARROW) && (o_ptr->number < 10))
            continue;

        int group = unified_sidebar_object_group(o_ptr);
        if (group_filter >= 0 && group != group_filter)
            continue;

        if (state->limit_objects_top_five && group_counts[group] >= 5)
            continue;

        group_counts[group]++;
        total_objects++;
    }

    return total_objects;
}

static void unified_look_sync_cursor_selection(unified_look_state* state)
{
    int new_selection;

    if (!state)
        return;

    if ((state->look_mode != 0) || state->in_sidebar_mode)
        return;

    new_selection = unified_look_find_cursor_selection(state, state->cursor_y,
        state->cursor_x);

    if (state->highlighted_y >= 0 && state->highlighted_x >= 0)
    {
        if ((new_selection < 0)
            || (state->highlighted_y != state->cursor_y)
            || (state->highlighted_x != state->cursor_x))
        {
            highlight_entity_on_map(state->highlighted_y, state->highlighted_x,
                false);
            state->highlighted_y = -1;
            state->highlighted_x = -1;
            state->highlighted_entity_type = 0;
        }
    }

    state->selected_entity = new_selection;
}

static void unified_look_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static void unified_look_print_prompt(cptr full_text, cptr compact_text)
{
    int term_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    char buf[192];
    cptr selected = full_text;

    if (compact_text && term_wid < (int)strlen(full_text) + 1)
        selected = compact_text;

    SDL_strlcpy(buf, selected, sizeof(buf));

    if ((int)strlen(buf) >= term_wid && term_wid > 4)
    {
        int cut = term_wid - 4;
        if (cut < 0)
            cut = 0;
        buf[cut] = '\0';
        SDL_strlcat(buf, "...", sizeof(buf));
    }

    SDL_strlcpy(g_unified_look_snapshot_prompt, buf,
        sizeof(g_unified_look_snapshot_prompt));
    g_unified_look_snapshot_prompt_cols = (int)strlen(buf);
    prt(buf, 0, 0);
}

void do_cmd_unified_look(void)
{
    unified_look_state state;
    int y, x;
    char query;
    bool done = false;
    bool need_redraw = true;
    int original_wy, original_wx; /* Store original viewport */
    
    /* Clear entry level banner when using look command */
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }
    
    /* Enable story font for unified look if the setting is on */
    bool use_story_font = story_look_enabled();
    if (use_story_font)
    {
        log_debug("do_cmd_unified_look: Enabling story font");
        sdl_story_font_enable();
    }

    if (unified_look_snapshot_active())
        unified_look_snapshot_clear();
    
    log_trace("=== UNIFIED LOOK STARTED ===");
    
    /* Store original viewport */
    original_wy = p_ptr->wy;
    original_wx = p_ptr->wx;
    
    log_trace("Original viewport: (%d,%d)", original_wy, original_wx);
    
    /* Initialize state */
    state.cursor_y = p_ptr->py;
    state.cursor_x = p_ptr->px;
    if (g_unified_look_has_start
        && g_unified_look_start_y >= 0 && g_unified_look_start_y < p_ptr->cur_map_hgt
        && g_unified_look_start_x >= 0 && g_unified_look_start_x < p_ptr->cur_map_wid)
    {
        state.cursor_y = g_unified_look_start_y;
        state.cursor_x = g_unified_look_start_x;

        if (!panel_contains(state.cursor_y, state.cursor_x))
        {
            int max_wy = MAX(p_ptr->cur_map_hgt - SCREEN_HGT, 0);
            int max_wx = MAX(p_ptr->cur_map_wid - SCREEN_WID, 0);
            int new_wy = state.cursor_y - SCREEN_HGT / 2;
            int new_wx = state.cursor_x - SCREEN_WID / 2;

            p_ptr->wy = MIN(MAX(new_wy, 0), max_wy);
            p_ptr->wx = MIN(MAX(new_wx, 0), max_wx);
            p_ptr->redraw |= PR_MAP;
            p_ptr->window |= PW_OVERHEAD;
            handle_stuff();
        }
    }
    state.selected_entity = -1;
    state.show_monsters = true;
    state.show_objects = true;
    state.object_group_filter = -1;
    state.limit_objects_top_five = false;
    state.display_mode = 0; /* 0 = manual, 1 = entity */
    state.highlighted_y = -1;
    state.highlighted_x = -1;
    state.highlighted_entity_type = 0; /* 0 = none, 1 = monster, 2 = object */
    state.in_sidebar_mode = false;
    state.look_mode = 0; /* 0 = normal unified look, 1 = L-style scrolling */
    state.current_square_entity = 0; /* 0 = monster, 1 = object */
    state.square_cycling_mode = false; /* Start in normal sidebar cycling mode */
    
    /* Track monster health at initial cursor position for left sidebar display */
    int initial_m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
    if ((initial_m_idx > 0)
        && unified_look_can_show_monster_at(state.cursor_y, state.cursor_x))
    {
        /* Track this monster for health display */
        health_track(initial_m_idx);
    }
    else
    {
        /* Clear health tracking when not starting on a visible monster */
        health_track(0);
    }
    
    /* Process redraw flags to update health bar immediately */
    handle_stuff();
    
    /* Main interaction loop */
    while (!done)
    {
        bool screen_saved = false;
        int overlay_rows = 0;
        int overlay_cols = 0;
        
        if (need_redraw)
        {
            unified_look_sync_cursor_selection(&state);
            g_unified_look_snapshot_prompt[0] = '\0';
            g_unified_look_snapshot_prompt_cols = 0;

            /* Save screen to preserve underlying display */
            screen_save();
            screen_saved = true;
            
            /* Show unified sidebar */
            overlay_rows = show_unified_sidebar(&state, &overlay_cols);
            
            /* Track monster health at current cursor position for left sidebar display */
            /* This handles Tab cycling and any other cursor position updates */
            int cursor_m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
            if ((cursor_m_idx > 0)
                && unified_look_can_show_monster_at(state.cursor_y, state.cursor_x))
            {
                /* Track this monster for health display */
                health_track(cursor_m_idx);
            }
            else
            {
                /* Clear health tracking when cursor is not on a visible monster */
                health_track(0);
            }
            
            /* Process redraw flags to update health bar immediately */
            handle_stuff();

            /* Show cursor position info */
            y = state.cursor_y;
            x = state.cursor_x;
            
            /* Display entity name in left sidebar if cursor is on something */
            {
                char out_val[256];
                int cursor_m_idx = cave_m_idx[y][x];
                int cursor_o_idx = cave_o_idx[y][x];
                int feat = cave_feat[y][x];
                bool has_visible_monster = unified_look_can_show_monster_at(y, x);
                bool has_marked_object = unified_look_can_show_marked_object_at(y, x);
                bool has_known_feature = false;
                cptr feature_name = NULL;
                
                /* Check for known/revealed features (traps, doors, stairs, shafts) */
                if (grid_info_is_available(y, x) && (cave_info[y][x] & (CAVE_MARK)))
                {
                    /* Traps */
                    if (feat >= FEAT_TRAP_HEAD && feat <= FEAT_TRAP_TAIL)
                    {
                        has_known_feature = true;
                        feature_name = f_name + f_info[feat].name;
                    }
                    /* Doors (closed, locked, jammed) */
                    else if (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
                    {
                        has_known_feature = true;
                        feature_name = f_name + f_info[feat].name;
                    }
                    /* Open door */
                    else if (feat == FEAT_OPEN)
                    {
                        has_known_feature = true;
                        feature_name = "open door";
                    }
                    /* Broken door */
                    else if (feat == FEAT_BROKEN)
                    {
                        has_known_feature = true;
                        feature_name = "broken door";
                    }
                    /* Stairs up */
                    else if (feat == FEAT_LESS)
                    {
                        has_known_feature = true;
                        feature_name = "up staircase";
                    }
                    /* Stairs down */
                    else if (feat == FEAT_MORE)
                    {
                        has_known_feature = true;
                        feature_name = "down staircase";
                    }
                    /* Shaft up */
                    else if (feat == FEAT_LESS_SHAFT)
                    {
                        has_known_feature = true;
                        feature_name = "up shaft";
                    }
                    /* Shaft down */
                    else if (feat == FEAT_MORE_SHAFT)
                    {
                        has_known_feature = true;
                        feature_name = "down shaft";
                    }
                }
                
                /* Priority: monster first, then object (only if marked), then feature */
                if (has_visible_monster)
                {
                    monster_type* m_ptr = &mon_list[cursor_m_idx];
                    char m_name[80];
                    
                    /* Get the monster name with indefinite article */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0x08);
                    
                    /* Display "You see <monster name>" in left sidebar */
                    strnfmt(out_val, sizeof(out_val), "You see %s.", m_name);
                    unified_look_print_prompt(out_val, NULL);
                }
                else if (has_marked_object)
                {
                    object_type* o_ptr = &o_list[cursor_o_idx];
                    char o_name[80];
                    char smith_buf[20];
                    
                    /* Get the object name with indefinite article */
                    object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);

                    smith_buf[0] = '\0';
                    if (op_ptr->opt[OPT_show_smithing_difficulty_look]
                        && object_known_p(o_ptr)
                        && object_uses_smithing_difficulty(o_ptr))
                    {
                        int depth = (p_ptr && p_ptr->depth > 0) ? p_ptr->depth : 1;
                        int sd = object_smithing_difficulty(o_ptr);
                        int wr = object_weight_rarity(o_ptr, depth);
                        strnfmt(smith_buf, sizeof(smith_buf), " {%d,%d}", sd, wr);
                    }
                    
                    /* Display "You see <object name>" in left sidebar */
                    strnfmt(out_val, sizeof(out_val), "You see %s%s.", o_name,
                        smith_buf);
                    unified_look_print_prompt(out_val, NULL);
                }
                else if (has_known_feature)
                {
                    /* Display "You see <feature name>" in left sidebar */
                    strnfmt(out_val, sizeof(out_val), "You see %s.",
                        feature_name);
                    unified_look_print_prompt(out_val, NULL);
                }
                else
                {
                    /* Display help text based on current mode */
                    if (state.look_mode == 0)
                    {
                        if (steamdeck_controls_active()) {
                            char prev_label[16];
                            char next_label[16];
                            char exam_label[16];
                            char target_label[16];
                            char obj_label[16];
                            char pan_label[16];
                            char back_label[16];
                            char prompt_buf[160];

                            unified_look_prompt_label('e', "L1", prev_label, sizeof(prev_label));
                            unified_look_prompt_label('i', "R1", next_label, sizeof(next_label));
                            unified_look_prompt_label(' ', "A", exam_label, sizeof(exam_label));
                            unified_look_prompt_label('f', "B", target_label, sizeof(target_label));
                            unified_look_prompt_label('u', "X", obj_label, sizeof(obj_label));
                            unified_look_prompt_label('s', "Y", pan_label, sizeof(pan_label));
                            unified_look_prompt_label(ESCAPE, "ESC", back_label, sizeof(back_label));

                            strnfmt(prompt_buf, sizeof(prompt_buf),
                                "[%s/%s]=Select [%s]=Exam [%s]=Target [%s]=Obj [%s]=Pan [%s]=Back",
                                next_label, prev_label, exam_label, target_label, obj_label, pan_label, back_label);
                            unified_look_print_prompt(prompt_buf,
                                "[R1/L1] Sel [A] Exam [B] Targ [X] Obj [Y] Pan [ESC]");
                        } else {
                            unified_look_print_prompt(
                                "[Tab/q]=Select [Space]=Exam [t]=Target [l]=Disp [m]=Monst [o]=ObjCat [T]=Top5 [s]=Pan [ESC]",
                                "[Tab/q] Sel [Space] Exam [t] Targ [l] Disp [m] Mon [o] Obj [T] Top5 [s] Pan [ESC]");
                        }
                    }
                    else
                    {
                        if (steamdeck_controls_active()) {
                            char prev_label[16];
                            char next_label[16];
                            char exam_label[16];
                            char target_label[16];
                            char obj_label[16];
                            char cursor_label[16];
                            char back_label[16];
                            char prompt_buf[160];

                            unified_look_prompt_label('e', "L1", prev_label, sizeof(prev_label));
                            unified_look_prompt_label('i', "R1", next_label, sizeof(next_label));
                            unified_look_prompt_label(' ', "A", exam_label, sizeof(exam_label));
                            unified_look_prompt_label('f', "B", target_label, sizeof(target_label));
                            unified_look_prompt_label('u', "X", obj_label, sizeof(obj_label));
                            unified_look_prompt_label('s', "Y", cursor_label, sizeof(cursor_label));
                            unified_look_prompt_label(ESCAPE, "ESC", back_label, sizeof(back_label));

                            strnfmt(prompt_buf, sizeof(prompt_buf),
                                "[%s/%s]=Select [%s]=Exam [%s]=Target [%s]=Obj [%s]=Curs [%s]=Back",
                                next_label, prev_label, exam_label, target_label, obj_label, cursor_label, back_label);
                            unified_look_print_prompt(prompt_buf,
                                "[R1/L1] Sel [A] Exam [B] Targ [X] Obj [Y] Curs [ESC]");
                        } else {
                            unified_look_print_prompt(
                                "[Tab/q]=Select [Space]=Exam [t]=Target [l]=Disp [m]=Monst [o]=ObjCat [T]=Top5 [s]=Curs [ESC]",
                                "[Tab/q] Sel [Space] Exam [t] Targ [l] Disp [m] Mon [o] Obj [T] Top5 [s] Curs [ESC]");
                        }
                    }
                }
            }
            
            /* Move cursor to position */
            move_cursor_relative(state.cursor_y, state.cursor_x);

            if (unified_look_snapshot_active())
                (void)unified_look_snapshot_publish_overlay(overlay_rows,
                    overlay_cols);
            
            need_redraw = false;
        }
        
        /* Get input */
        query = unified_look_inkey_with_wait_reason();
        log_trace("Unified look key input: '%c' (%d) [char: %c, isupper: %d]", 
                 query, (int)query, (query >= 32 && query <= 126) ? query : '?', 
                 (query >= 'A' && query <= 'Z') ? 1 : 0);
        
        /* Restore screen after input if we saved it */
        if (screen_saved)
        {
            screen_load();
        }
        
        /* Update health bar display after screen restore */
        handle_stuff();
        
        /* Analyze input */
        log_trace("Processing key: '%c' (%d), backtick is %d", query, (int)query, (int)'`');
        switch (query)
        {
            case 'T':
            {
                state.limit_objects_top_five = !state.limit_objects_top_five;
                log_trace("'T' key pressed - top five toggle now %d", state.limit_objects_top_five ? 1 : 0);

                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }

                handle_stuff();
                need_redraw = true;
                continue;
            }

            /* Handle capital letters - most are now ignored since we use arrows for scrolling */
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
            case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
            case 'O': case 'P': case 'R': case 'S': case 'U':
            case 'V': case 'W': case 'X': case 'Y': case 'Z':
            {
                /* Capital letters are now ignored - use 'l' to switch to panel scroll mode */
                log_trace("Capital letter ignored: '%c' (%d) - use 'l' to switch modes", query, (int)query);
                break;
            }
            
            case ESCAPE:
            case 'Q':
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                done = true;
                break;
                
            /* Common menu keys - exit unified look and let them be processed normally */
            case '/':            /* Identify symbol */
            case '?':            /* Help */
            case 's':
            {
                /* Switch between cursor mode and panel scrolling mode */
                state.look_mode = (state.look_mode + 1) % 2;
                log_trace("'s' key pressed - look mode changed to: %d", state.look_mode);
                
                /* Update help text based on mode */
                need_redraw = true;
                break;
            }
            
            case 'x':            /* Examine/Look - show description */
            {
                log_trace("EXAMINATION: 'x' key pressed for description");
                
                /* Disable story font for info screens */
                if (use_story_font)
                    sdl_story_font_disable();

                unified_look_snapshot_clear();
                
                /* Same logic as Space/Enter for examination */
                log_trace("EXAMINATION: state.in_sidebar_mode=%d, state.selected_entity=%d", 
                         state.in_sidebar_mode, state.selected_entity);
                log_trace("EXAMINATION: state.highlighted_y=%d, state.highlighted_x=%d", 
                         state.highlighted_y, state.highlighted_x);
                
                if (state.in_sidebar_mode && state.selected_entity >= 0 && 
                    state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions met");
                    
                    int cursor_m_idx = cave_m_idx[state.highlighted_y][state.highlighted_x];
                    int cursor_o_idx = cave_o_idx[state.highlighted_y][state.highlighted_x];
                    
                    log_trace("EXAMINATION: At highlighted position (%d,%d) - m_idx=%d, o_idx=%d, entity_type=%d", 
                             state.highlighted_y, state.highlighted_x, cursor_m_idx, cursor_o_idx, state.highlighted_entity_type);
                    
                    /* Examine the entity based on what was highlighted in the sidebar */
                    /* Entity type: 1 = monster, 2 = object */
                    if (state.highlighted_entity_type == 1 && cursor_m_idx > 0)
                    {
                        /* Monster was highlighted - examine monster */
                        log_trace("EXAMINATION: Highlighted entity is monster, examining monster %d", cursor_m_idx);
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            unified_look_show_monster_recall(m_ptr);
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else if ((state.highlighted_entity_type == 2)
                        && unified_look_can_show_marked_object_at(
                            state.highlighted_y, state.highlighted_x))
                    {
                        /* Object was highlighted - examine object */
                        log_trace("EXAMINATION: Highlighted entity is object, examining object %d", cursor_o_idx);
                        /* Object examination */
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        log_trace("EXAMINATION: Showing object info screen");
                        unified_look_show_object_info(o_ptr);
                        log_trace("EXAMINATION: Object examination completed");
                    }
                    else if (cursor_m_idx > 0)
                    {
                        log_trace("EXAMINATION: Found monster, examining monster %d", cursor_m_idx);
                        /* Monster examination */
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            unified_look_show_monster_recall(m_ptr);
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else
                    {
                        log_trace("EXAMINATION: No entities found at highlighted position");
                    }
                }
                else
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions NOT met - using cursor position examination");
                    /* Examine cursor position */
                    y = state.cursor_y;
                    x = state.cursor_x;
                    
                    int cursor_m_idx = cave_m_idx[y][x];
                    int cursor_o_idx = cave_o_idx[y][x];
                    bool has_visible_monster = unified_look_can_show_monster_at(y, x);
                    bool has_object = unified_look_can_show_marked_object_at(y, x);
                    
                    log_trace("EXAMINATION: Cursor position (%d,%d) - has_visible_monster=%d, has_object=%d", 
                             y, x, has_visible_monster, has_object);
                    
                    /* Prioritize OBJECT first, then visible monster */
                    if (has_object)
                    {
                        log_trace("EXAMINATION: Examining object at cursor position");
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        unified_look_show_object_info(o_ptr);
                    }
                    else if (has_visible_monster)
                    {
                        log_trace("EXAMINATION: Examining visible monster at cursor position");
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        unified_look_show_monster_recall(m_ptr);
                    }
                    else
                    {
                        log_trace("EXAMINATION: No visible entities at cursor position");
                    }
                }
                need_redraw = true;
                break;
            }
            
            case '[':            /* View monsters */
            case ']':            /* View objects */
            case 'w':            /* Wield/Wear */
            case 'd':            /* Drop */
            case 'k':            /* Destroy */
            case 'r':            /* Read scroll */
            case 'a':            /* Activate */
            case 'z':            /* Zap rod */
            case '.':            /* Run */
            case ',':            /* Stay */
            case '<':            /* Go up stairs */
            case '>':            /* Go down stairs */
            case 'g':            /* Get/Pickup */
            case 'c':            /* Close */
            case 'j':            /* Jam */
            case '+':            /* Alter */
            case '*':            /* Target */
            case '@':            /* Center map */
            case '(':            /* Dungeon history */
            case '|':            /* Screenshots */
            case '~':            /* Various things */
            case '!':            /* OS command */
command_key:
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                done = true;
                /* Don't consume the key - let it be processed by the main game loop */
                Term_keypress(query);
                break;
                
            case '2':
            case '8':
            case '4':
            case '6':
            case '1':
            case '3':
            case '7':
            case '9':
            {
                /* Arrow key behavior depends on current mode */
                if (state.look_mode == 0)
                {
                    /* Mode 0: Normal unified look - manual cursor scrolling */
                    int dir = target_dir(query);
                    if (dir)
                    {
                        int new_cursor_y = state.cursor_y + ddy[dir];
                        int new_cursor_x = state.cursor_x + ddx[dir];
                        
                        /* Calculate explored bounds */
                        int min_y, max_y, min_x, max_x;
                        bool has_explored = get_explored_bounds(&min_y, &max_y, &min_x, &max_x);
                        
                        if (has_explored)
                        {
                            /* Constrain cursor to explored area */
                            if (new_cursor_y < min_y) new_cursor_y = min_y;
                            if (new_cursor_y > max_y) new_cursor_y = max_y;
                            if (new_cursor_x < min_x) new_cursor_x = min_x;
                            if (new_cursor_x > max_x) new_cursor_x = max_x;
                        }
                        else
                        {
                            /* No explored area, constrain to full map */
                            if (new_cursor_y < 0) new_cursor_y = 0;
                            if (new_cursor_y >= p_ptr->cur_map_hgt) new_cursor_y = p_ptr->cur_map_hgt - 1;
                            if (new_cursor_x < 0) new_cursor_x = 0;
                            if (new_cursor_x >= p_ptr->cur_map_wid) new_cursor_x = p_ptr->cur_map_wid - 1;
                        }
                        
                        state.cursor_y = new_cursor_y;
                        state.cursor_x = new_cursor_x;
                        
                        /* Handle viewport scrolling when cursor reaches screen edge */
                        if (!panel_contains(state.cursor_y, state.cursor_x))
                        {
                            /* Log viewport scrolling */
                            log_trace("Viewport scroll: cursor at (%d,%d), panel (%d,%d)", 
                                     state.cursor_y, state.cursor_x, p_ptr->wy, p_ptr->wx);
                            
                            /* Center the viewport on the cursor */
                            int new_wy = state.cursor_y - SCREEN_HGT / 2;
                            int new_wx = state.cursor_x - SCREEN_WID / 2;
                            
                            /* Constrain viewport to explored bounds if available */
                            if (has_explored)
                            {
                                int explored_min_wy = min_y;
                                int explored_max_wy = max_y - SCREEN_HGT + 1;
                                int explored_min_wx = min_x;
                                int explored_max_wx = max_x - SCREEN_WID + 1;
                                
                                /* Ensure min <= max */
                                if (explored_max_wy < explored_min_wy) explored_max_wy = explored_min_wy;
                                if (explored_max_wx < explored_min_wx) explored_max_wx = explored_min_wx;
                                
                                if (new_wy < explored_min_wy) new_wy = explored_min_wy;
                                if (new_wy > explored_max_wy) new_wy = explored_max_wy;
                                if (new_wx < explored_min_wx) new_wx = explored_min_wx;
                                if (new_wx > explored_max_wx) new_wx = explored_max_wx;
                            }
                            
                            /* Use proper panel management function */
                            if (modify_panel(new_wy, new_wx))
                            {
                                /* Handle viewport updates immediately */
                                handle_stuff();
                            }
                            
                            log_trace("New viewport: (%d,%d)", p_ptr->wy, p_ptr->wx);
                        }
                        
                        state.in_sidebar_mode = false;
                        state.selected_entity = -1;
                        state.square_cycling_mode = false;
                        state.current_square_entity = 0;
                        
                        /* Clear old highlighting */
                        if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                        {
                            highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                            state.highlighted_y = -1;
                            state.highlighted_x = -1;
                            state.highlighted_entity_type = 0;
                        }
                        
                        /* Track monster health at cursor position for left sidebar display */
                        int m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
                        if ((m_idx > 0)
                            && unified_look_can_show_monster_at(state.cursor_y,
                                state.cursor_x))
                        {
                            /* Track this monster for health display */
                            health_track(m_idx);
                        }
                        else
                        {
                            /* Clear health tracking when not on a visible monster */
                            health_track(0);
                        }
                        
                        /* Process redraw flags to update health bar immediately */
                        handle_stuff();
                        
                        need_redraw = true;
                    }
                }
                else
                {
                    /* Mode 1: Panel scrolling - arrows scroll whole panels like capital letters */
                    int dir = target_dir(query);
                    if (dir)
                    {
                        log_trace("Panel scrolling mode: key='%c', dir=%d", query, dir);
                        
                        int old_wy = p_ptr->wy;
                        int old_wx = p_ptr->wx;
                        
                        log_trace("Old viewport: (%d,%d)", old_wy, old_wx);
                        
                        /* Apply the motion by full panels */
                        int new_wy = p_ptr->wy + (ddy[dir] * PANEL_HGT);
                        int new_wx = p_ptr->wx + (ddx[dir] * PANEL_WID);
                        
                        /* Calculate explored bounds for viewport constraint */
                        int min_y, max_y, min_x, max_x;
                        int explored_min_wy, explored_max_wy;
                        int explored_min_wx, explored_max_wx;
                        
                        if (get_explored_bounds(&min_y, &max_y, &min_x, &max_x))
                        {
                            /* Calculate viewport bounds based on explored area */
                            explored_min_wy = min_y;
                            explored_max_wy = max_y - SCREEN_HGT + 1;
                            explored_min_wx = min_x;
                            explored_max_wx = max_x - SCREEN_WID + 1;
                            
                            /* Ensure min <= max */
                            if (explored_max_wy < explored_min_wy) explored_max_wy = explored_min_wy;
                            if (explored_max_wx < explored_min_wx) explored_max_wx = explored_min_wx;
                        }
                        else
                        {
                            /* No explored area, use full map */
                            explored_min_wy = 0;
                            explored_max_wy = p_ptr->cur_map_hgt - SCREEN_HGT;
                            explored_min_wx = 0;
                            explored_max_wx = p_ptr->cur_map_wid - SCREEN_WID;
                        }
                        
                        /* Constrain viewport to explored boundaries */
                        if (new_wy < explored_min_wy) new_wy = explored_min_wy;
                        if (new_wx < explored_min_wx) new_wx = explored_min_wx;
                        if (new_wy > explored_max_wy) new_wy = explored_max_wy;
                        if (new_wx > explored_max_wx) new_wx = explored_max_wx;
                        
                        /* Additional safety checks */
                        if (new_wy < 0) new_wy = 0;
                        if (new_wx < 0) new_wx = 0;
                            
                        log_trace("Constrained viewport: (%d,%d)", new_wy, new_wx);

                        /* Use proper panel management function */
                        if (modify_panel(new_wy, new_wx))
                        {
                            /* Update cursor to same relative position */
                            state.cursor_y = state.cursor_y + (p_ptr->wy - old_wy);
                            state.cursor_x = state.cursor_x + (p_ptr->wx - old_wx);
                            
                            /* Boundary check cursor */
                            if (state.cursor_y < 0) state.cursor_y = 0;
                            if (state.cursor_y >= p_ptr->cur_map_hgt) state.cursor_y = p_ptr->cur_map_hgt - 1;
                            if (state.cursor_x < 0) state.cursor_x = 0;
                            if (state.cursor_x >= p_ptr->cur_map_wid) state.cursor_x = p_ptr->cur_map_wid - 1;
                            
                            log_trace("New cursor: (%d,%d)", state.cursor_y, state.cursor_x);

                            /* Handle viewport updates immediately */
                            handle_stuff();
                            
                            /* Track monster health at cursor position for left sidebar display */
                            int m_idx = cave_m_idx[state.cursor_y][state.cursor_x];
                            if ((m_idx > 0)
                                && unified_look_can_show_monster_at(state.cursor_y,
                                    state.cursor_x))
                            {
                                /* Track this monster for health display */
                                health_track(m_idx);
                            }
                            else
                            {
                                /* Clear health tracking when not on a visible monster */
                                health_track(0);
                            }
                            
                            /* Process redraw flags to update health bar immediately */
                            handle_stuff();
                            
                            need_redraw = true;
                        }
                        else
                        {
                            log_trace("Viewport unchanged");
                        }
                    }
                }
                break;
            }
            
            case '\t': /* Tab key */
            case 'i':  /* I key - forward cycling (Steam Deck) */
            {
                if (query == 'i' && !steamdeck_controls_active())
                    goto command_key;
                log_trace("Tab key pressed - cycling entities");
                
                /* Global sidebar cycling only - no square cycling */
                state.in_sidebar_mode = true;
                state.square_cycling_mode = false; /* Always disable square cycling */
                
                /* Count total VISIBLE entities using same logic as sidebar */
                int total_entities = unified_look_count_visible_entities(&state);
                
                log_trace("Total visible entities: %d", total_entities);
                
                if (total_entities > 0)
                {
                    /* Clear previous highlighting */
                    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                    {
                        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    }
                    
                    /* Advance selection */
                    int old_selection = state.selected_entity;
                    state.selected_entity++;
                    if (state.selected_entity >= total_entities)
                        state.selected_entity = 0;
                        
                    log_trace("Entity selection: %d -> %d", old_selection, state.selected_entity);
                }
                
                need_redraw = true;
                break;
            }
            
            case '`': /* Backtick key - reverse Tab cycling */
            case 'q': /* Q key - reverse Tab cycling */
            case 'e': /* E key - reverse Tab cycling (Steam Deck) */
            {
                if (query == 'e' && !steamdeck_controls_active())
                    goto command_key;
                log_trace("REVERSE CYCLING: Key handler reached - cycling entities backward");
                
                /* Global sidebar cycling only - no square cycling */
                state.in_sidebar_mode = true;
                state.square_cycling_mode = false; /* Always disable square cycling */
                
                /* Count total VISIBLE entities using same logic as sidebar */
                int total_entities = unified_look_count_visible_entities(&state);
                
                log_trace("Total visible entities: %d", total_entities);
                
                if (total_entities > 0)
                {
                    /* Clear previous highlighting */
                    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                    {
                        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    }
                    
                    /* Move backward in selection */
                    int old_selection = state.selected_entity;
                    state.selected_entity--;
                    if (state.selected_entity < 0)
                        state.selected_entity = total_entities - 1;
                        
                    log_trace("Entity selection (backward): %d -> %d", old_selection, state.selected_entity);
                }
                
                need_redraw = true;
                break;
            }
            
            case '\r': /* Enter key */
            case ' ':
            {
                log_trace("EXAMINATION: Enter/Space key pressed for examination");
                
                /* Disable story font for info screens */
                if (use_story_font)
                    sdl_story_font_disable();

                unified_look_snapshot_clear();
                
                /* Examine current target */
                log_trace("EXAMINATION: state.in_sidebar_mode=%d, state.selected_entity=%d", 
                         state.in_sidebar_mode, state.selected_entity);
                log_trace("EXAMINATION: state.highlighted_y=%d, state.highlighted_x=%d", 
                         state.highlighted_y, state.highlighted_x);
                
                if (state.in_sidebar_mode && state.selected_entity >= 0 && 
                    state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions met");
                    
                    int cursor_m_idx = cave_m_idx[state.highlighted_y][state.highlighted_x];
                    int cursor_o_idx = cave_o_idx[state.highlighted_y][state.highlighted_x];
                    
                    log_trace("EXAMINATION: At highlighted position (%d,%d) - m_idx=%d, o_idx=%d, entity_type=%d", 
                             state.highlighted_y, state.highlighted_x, cursor_m_idx, cursor_o_idx, state.highlighted_entity_type);
                    
                    /* Examine the entity based on what was highlighted in the sidebar */
                    /* Entity type: 1 = monster, 2 = object */
                    if (state.highlighted_entity_type == 1 && cursor_m_idx > 0)
                    {
                        /* Monster was highlighted - examine monster */
                        log_trace("EXAMINATION: Highlighted entity is monster, examining monster %d", cursor_m_idx);
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            unified_look_show_monster_recall(m_ptr);
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else if ((state.highlighted_entity_type == 2)
                        && unified_look_can_show_marked_object_at(
                            state.highlighted_y, state.highlighted_x))
                    {
                        /* Object was highlighted - examine object */
                        log_trace("EXAMINATION: Highlighted entity is object, examining object %d", cursor_o_idx);
                        /* Object examination */
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        log_trace("EXAMINATION: Showing object info screen");
                        unified_look_show_object_info(o_ptr);
                        log_trace("EXAMINATION: Object examination completed");
                    }
                    else if (cursor_m_idx > 0)
                    {
                        log_trace("EXAMINATION: Found monster, examining monster %d", cursor_m_idx);
                        /* Monster examination */
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        log_trace("EXAMINATION: Monster ml=%d", m_ptr->ml);
                        if (unified_look_can_show_monster_at(state.highlighted_y,
                                state.highlighted_x))
                        {
                            log_trace("EXAMINATION: Showing monster recall");
                            unified_look_show_monster_recall(m_ptr);
                            log_trace("EXAMINATION: Monster recall completed");
                        }
                        else
                        {
                            log_trace("EXAMINATION: Monster not visible (ml=0), skipping examination");
                        }
                    }
                    else
                    {
                        log_trace("EXAMINATION: No entities found at highlighted position");
                    }
                }
                else
                {
                    log_trace("EXAMINATION: Sidebar mode examination conditions NOT met - using cursor position examination");
                    /* Examine cursor position */
                    y = state.cursor_y;
                    x = state.cursor_x;
                    
                    int cursor_m_idx = cave_m_idx[y][x];
                    int cursor_o_idx = cave_o_idx[y][x];
                    bool has_visible_monster = unified_look_can_show_monster_at(y, x);
                    bool has_object = unified_look_can_show_marked_object_at(y, x);
                    
                    log_trace("EXAMINATION: Cursor position (%d,%d) - has_visible_monster=%d, has_object=%d", 
                             y, x, has_visible_monster, has_object);
                    
                    /* Prioritize OBJECT first, then visible monster */
                    if (has_object)
                    {
                        log_trace("EXAMINATION: Examining object at cursor position");
                        object_type* o_ptr = &o_list[cursor_o_idx];
                        unified_look_show_object_info(o_ptr);
                    }
                    else if (has_visible_monster)
                    {
                        log_trace("EXAMINATION: Examining visible monster at cursor position");
                        monster_type* m_ptr = &mon_list[cursor_m_idx];
                        unified_look_show_monster_recall(m_ptr);
                    }
                    else
                    {
                        log_trace("EXAMINATION: No visible entities at cursor position");
                    }
                }
                
                /* Re-enable story font */
                if (use_story_font)
                    sdl_story_font_enable();
                
                need_redraw = true;
                break;
            }
            
            case 'm':
            {
                log_trace("'m' key pressed - cycling monster display");
                /* Cycle monsters: monsters -> nothing -> monsters */
                if (state.show_monsters)
                {
                    /* From showing monsters to hiding monsters */
                    state.show_monsters = false;
                    log_trace("Mode changed to: monsters hidden");
                }
                else
                {
                    /* From hiding monsters to showing monsters */
                    state.show_monsters = true;
                    log_trace("Mode changed to: monsters shown");
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'m' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 'u':
                if (!steamdeck_controls_active())
                    goto command_key;
                /* fallthrough */
            case 'o':
            {
                log_trace("'o' key pressed - cycling object categories");

                /* Cycle: all -> weapons -> armour -> artifacts -> herbs -> potions -> gems -> consumables -> other -> hidden */
                static const int object_filter_cycle[] = {
                    LOOK_GROUP_ARTIFACT,
                    LOOK_GROUP_WEAPON,
                    LOOK_GROUP_ARMOUR,
                    LOOK_GROUP_JEWELRY,
                    LOOK_GROUP_HERBS,
                    LOOK_GROUP_POTIONS,
                    LOOK_GROUP_GEMS,
                    LOOK_GROUP_CONSUMABLE,
                    LOOK_GROUP_OTHER,
                };

                if (!state.show_objects)
                {
                    state.show_objects = true;
                    state.object_group_filter = -1;
                    log_trace("Object display: shown (ALL)");
                }
                else if (state.object_group_filter < 0)
                {
                    /* Skip empty categories */
                    int next_group = -1;
                    for (size_t idx = 0; idx < N_ELEMENTS(object_filter_cycle); ++idx)
                    {
                        int group = object_filter_cycle[idx];
                        if (unified_look_count_visible_objects_for_group(&state, group) > 0)
                        {
                            next_group = group;
                            break;
                        }
                    }

                    if (next_group >= 0)
                    {
                        state.object_group_filter = next_group;
                        log_trace("Object display: filtered (group=%d)", state.object_group_filter);
                    }
                    else
                    {
                        state.show_objects = false;
                        state.object_group_filter = -1;
                        log_trace("Object display: hidden (no non-empty categories)");
                    }
                }
                else
                {
                    int next_group = -1;
                    for (size_t idx = 0; idx < N_ELEMENTS(object_filter_cycle); ++idx)
                    {
                        if (object_filter_cycle[idx] != state.object_group_filter)
                            continue;

                        /* Skip empty categories */
                        for (size_t j = idx + 1; j < N_ELEMENTS(object_filter_cycle); ++j)
                        {
                            int group = object_filter_cycle[j];
                            if (unified_look_count_visible_objects_for_group(&state, group) > 0)
                            {
                                next_group = group;
                                break;
                            }
                        }
                        break;
                    }

                    if (next_group >= 0)
                    {
                        state.object_group_filter = next_group;
                        log_trace("Object display: filtered (group=%d)", state.object_group_filter);
                    }
                    else
                    {
                        state.show_objects = false;
                        state.object_group_filter = -1;
                        log_trace("Object display: hidden");
                    }
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'o' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 'l':
            {
                log_trace("'l' key pressed - cycling through display modes");
                /* Cycle display modes: monsters+objects -> objects -> nothing -> monsters+objects */
                if (state.show_monsters && state.show_objects)
                {
                    /* From both to objects only */
                    state.show_monsters = false;
                    state.show_objects = true;
                    log_trace("Mode changed to: objects only");
                }
                else if (!state.show_monsters && state.show_objects)
                {
                    /* From objects only to nothing */
                    state.show_monsters = false;
                    state.show_objects = false;
                    log_trace("Mode changed to: nothing (all hidden)");
                }
                else
                {
                    /* From nothing (or monsters only) to both */
                    state.show_monsters = true;
                    state.show_objects = true;
                    log_trace("Mode changed to: both monsters and objects");
                }
                
                /* Reset selection when changing display */
                state.selected_entity = -1;
                state.in_sidebar_mode = false;
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                }
                
                /* Force a complete redraw */
                handle_stuff();
                need_redraw = true;
                log_trace("'l' key: set need_redraw=true, continuing to redraw immediately");
                /* Continue to top of loop to process redraw immediately */
                continue;
            }
            
            case 'f':
                if (!steamdeck_controls_active())
                    goto command_key;
                /* fallthrough */
            case 't':
            {
                /* Target monster at cursor position or selected position */
                int target_y = state.cursor_y;
                int target_x = state.cursor_x;
                
                /* Use highlighted position if in sidebar mode */
                if (state.in_sidebar_mode && state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    target_y = state.highlighted_y;
                    target_x = state.highlighted_x;
                }
                
                int m_idx = cave_m_idx[target_y][target_x];
                if ((m_idx > 0) && unified_look_can_show_monster_at(target_y, target_x))
                {
                    /* Set target to the monster */
                    target_set_monster(m_idx);
                    
                    /* Get monster description for message */
                    char m_name[80];
                    monster_desc(m_name, sizeof(m_name), &mon_list[m_idx], 0x80);
                    msg_format("Target set to %s.", m_name);
                    
                    /* Exit unified look after targeting */
                    done = true;
                }
                else
                {
                    msg_print("No monster at cursor position.");
                }
                break;
            }
            
            case 'p':
            {
                /* Return to player position */
                state.cursor_y = p_ptr->py;
                state.cursor_x = p_ptr->px;
                state.in_sidebar_mode = false;
                state.selected_entity = -1;
                need_redraw = true;
                break;
            }
            
            default:
            {
                /* Unhandled key - exit like ESC */
                log_trace("Unhandled key in unified look: '%c' (%d) - exiting", query, (int)query);
                /* Clear any highlighting before exit */
                if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
                {
                    highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
                    state.highlighted_y = -1;
                    state.highlighted_x = -1;
                    state.highlighted_entity_type = 0;
                }
                done = true;
                break;
            }
        }
    }
    
    /* Clear any highlighting */
    if (state.highlighted_y >= 0 && state.highlighted_x >= 0)
    {
        highlight_entity_on_map(state.highlighted_y, state.highlighted_x, false);
        state.highlighted_y = -1;
        state.highlighted_x = -1;
        state.highlighted_entity_type = 0;
    }
    
    log_trace("=== UNIFIED LOOK ENDED ===");

    unified_look_snapshot_clear();
    
    /* Clear health tracking before exiting look command */
    health_track(0);
    
    /* Disable story font if it was enabled */
    if (use_story_font)
    {
        log_debug("do_cmd_unified_look: Disabling story font");
        sdl_story_font_disable();
    }
    
    /* Restore original viewport */
    if (p_ptr->wy != original_wy || p_ptr->wx != original_wx)
    {
        p_ptr->wy = original_wy;
        p_ptr->wx = original_wx;
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
        handle_stuff();
    }

}

/*
 * Highlight an entity on the map 
 */
void highlight_entity_on_map(int y, int x, bool highlight)
{
    highlight_entity_on_map_type(y, x, highlight, 0); /* Default: auto-detect */
}

void highlight_entity_on_map_type(int y, int x, bool highlight, int entity_type)
{
    if (highlight)
    {
        /* Get the original character and color, but show with blue background */
        char display_char;
        byte display_attr;
        
        /* Determine what to display based on entity_type preference */
        /* entity_type: 0=auto-detect, 1=prefer monster, 2=prefer object */
        
        if (entity_type == 2 && cave_o_idx[y][x] > 0)
        {
            /* Prefer object display */
            object_type* o_ptr = &o_list[cave_o_idx[y][x]];
            display_char = object_char(o_ptr);
            display_attr = object_attr(o_ptr); /* Keep original object color */
            log_trace("Highlighting object '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (entity_type == 1 && cave_m_idx[y][x] > 0)
        {
            /* Prefer monster display */
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            display_char = monster_char(r_ptr);
            display_attr = monster_attr(r_ptr); /* Keep original monster color */
            log_trace("Highlighting monster '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (cave_m_idx[y][x] > 0)
        {
            /* Auto-detect: For monsters, show normal appearance (no color change) */
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            display_char = monster_char(r_ptr);
            display_attr = monster_attr(r_ptr); /* Keep original monster color */
            log_trace("Highlighting monster '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else if (cave_o_idx[y][x] > 0)
        {
            /* Auto-detect: For objects, show normal appearance (no color change) */
            object_type* o_ptr = &o_list[cave_o_idx[y][x]];
            display_char = object_char(o_ptr);
            display_attr = object_attr(o_ptr); /* Keep original object color */
            log_trace("Highlighting object '%c' at (%d,%d) -> showing normal color", 
                     display_char, y, x);
        }
        else
        {
            /* Empty space - use a cursor */
            display_char = '+';
            display_attr = TERM_L_BLUE;
            log_trace("Highlighting empty space at (%d,%d) -> showing blue cursor", y, x);
        }
        
        /* Draw highlighted character */
        print_rel(display_char, display_attr, y, x);
        log_trace("Applied blue highlighting: char='%c', attr=%d", 
                 display_char, display_attr);
    }
    else
    {
        /* Restore original display */
        lite_spot(y, x);
        log_trace("Restored original display at (%d,%d)", y, x);
    }
}

/*
 * Allow the player to examine other sectors on the map
 */
void do_cmd_locate(void)
{
    int dir, y1, x1, y2, x2;
    int min_y, max_y, min_x, max_x;
    int explored_min_wy, explored_max_wy;
    int explored_min_wx, explored_max_wx;

    /* Block when hallucinating */
    if (p_ptr->image)
    {
        msg_print("Your vision is too distorted to map your location.");
        return;
    }

    /* Clear entry level banner when using L command */
    if (g_banner_force_redraw_remaining > 0)
    {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Calculate explored bounds */
    if (get_explored_bounds(&min_y, &max_y, &min_x, &max_x))
    {
        /* Calculate viewport bounds based on explored area */
        explored_min_wy = min_y;
        explored_max_wy = max_y - SCREEN_HGT + 1;
        explored_min_wx = min_x;
        explored_max_wx = max_x - SCREEN_WID + 1;
        
        /* Ensure min <= max */
        if (explored_max_wy < explored_min_wy) explored_max_wy = explored_min_wy;
        if (explored_max_wx < explored_min_wx) explored_max_wx = explored_min_wx;
    }
    else
    {
        /* No explored area, use full map */
        explored_min_wy = 0;
        explored_max_wy = p_ptr->cur_map_hgt - SCREEN_HGT;
        explored_min_wx = 0;
        explored_max_wx = p_ptr->cur_map_wid - SCREEN_WID;
    }

    /* Start at current panel */
    y2 = y1 = p_ptr->wy;
    x2 = x1 = p_ptr->wx;

    /* Show panels until done */
    while (true)
    {
        /* Assume no direction */
        dir = 0;

        /* Get a direction */
        while (!dir)
        {
            char command;

            /* Get a command (or Cancel) */
            if (!get_com("Shift viewpoint in which direction? ", &command))
                break;

            /* Extract direction */
            dir = target_dir(command);

            /* Error */
            if (!dir)
                bell("Illegal direction for look (around dungeon)!");
        }

        /* No direction */
        if (!dir)
            break;

        /* Apply the motion */
        y2 += (ddy[dir] * PANEL_HGT);
        x2 += (ddx[dir] * PANEL_WID);

        /* Constrain to explored bounds */
        if (y2 > explored_max_wy)
            y2 = explored_max_wy;
        if (y2 < explored_min_wy)
            y2 = explored_min_wy;

        if (x2 > explored_max_wx)
            x2 = explored_max_wx;
        if (x2 < explored_min_wx)
            x2 = explored_min_wx;

        /* Handle "changes" */
        if ((p_ptr->wy != y2) || (p_ptr->wx != x2))
        {
            /* Update panel */
            p_ptr->wy = y2;
            p_ptr->wx = x2;

            /* Redraw map */
            p_ptr->redraw |= (PR_MAP);

            /* Window stuff */
            p_ptr->window |= (PW_OVERHEAD);

            /* Handle stuff */
            handle_stuff();
        }
    }

    /* Verify panel */
    p_ptr->update |= (PU_PANEL);

    /* Handle stuff */
    handle_stuff();
}
