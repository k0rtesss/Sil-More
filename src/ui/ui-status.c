/* File: ui/ui-status.c */

#include "angband.h"
#include "app/app-session.h"

#include "object/object-ui-display.h"
#include "platform-frame.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-status.h"

#include "log/log.h"
#include "metarun.h"
#include "player/player-calc.h"
#include "player/identification.h"

typedef bool (*ui_status_window_scene_build_fn)(app_ui_scene* scene);

static bool ui_status_build_placeholder_window_scene(app_ui_scene* scene,
    cptr title, cptr body)
{
    app_ui_panel* panel;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = TERM_SLATE;
    app_ui_panel_set_widths(panel, 420, 900);
    app_ui_panel_set_title(panel, TERM_WHITE, title ? title : "");
    return app_ui_panel_add_body_line(panel, TERM_SLATE,
        body ? body : " ");
}

static bool ui_status_build_inventory_window_scene(app_ui_scene* scene)
{
    return build_inventory_subwindow_ui_scene(scene);
}

static bool ui_status_build_equipment_window_scene(app_ui_scene* scene)
{
    return build_equipment_subwindow_ui_scene(scene);
}

static bool ui_status_build_player_window_scene(app_ui_scene* scene)
{
    return build_player_subwindow_ui_scene(scene);
}

static bool ui_status_build_object_window_scene(app_ui_scene* scene)
{
    if (!p_ptr || p_ptr->object_kind_idx <= 0)
    {
        return ui_status_build_placeholder_window_scene(scene, "Object",
            "No object selected.");
    }

    return build_object_kind_recall_ui_scene(scene, p_ptr->object_kind_idx,
        NULL, false);
}

static bool ui_status_build_overhead_window_scene(app_ui_scene* scene)
{
    return build_overhead_subwindow_ui_scene(scene);
}

static bool ui_status_build_monster_window_scene(app_ui_scene* scene)
{
    if (!p_ptr || p_ptr->monster_race_idx <= 0)
    {
        return ui_status_build_placeholder_window_scene(scene, "Monster",
            "No monster selected.");
    }

    return build_monster_recall_ui_scene(scene, p_ptr->monster_race_idx, NULL,
        NULL, false);
}

static bool ui_status_build_message_window_scene(app_ui_scene* scene)
{
    return build_message_subwindow_ui_scene(scene);
}

static bool ui_status_build_monlist_window_scene(app_ui_scene* scene)
{
    return build_monlist_subwindow_ui_scene(scene);
}

static bool ui_status_refresh_matching_windows(u32b flag,
    ui_status_window_scene_build_fn build_scene)
{
    int j;
    bool rendered_any = false;

    if (!build_scene)
        return false;

    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        app_ui_scene scene;
        int previous_view;

        if (!platform_frame_view_ready(j))
            continue;
        if (!(op_ptr->window_flag[j] & flag))
            continue;
        previous_view = platform_frame_active_view_index();
        platform_frame_set_active_view(j);
        if (!build_scene(&scene)
            || !platform_frame_render_ui_scene_to_view(j, &scene))
        {
            platform_frame_set_active_view(previous_view);
            log_warn("window refresh: semantic pane render failed for flag 0x%08X on term %d",
                flag, j);
            continue;
        }
        platform_frame_set_active_view(previous_view);

        rendered_any = true;
    }

    return rendered_any;
}

/*
 * Converts stat num into a two-char (right justified) string
 * Sil: rather pointless since stats no longer have and 18/XYZ format
 */
void cnv_stat(int val, char* out_val) { sprintf(out_val, "%2d", val); }

/*
 *  Represents the different levels of health.
 *  Note that it is a bit odd with fewer health levels in the SOMEWHAT_WOUNDED
 * category. This is due to a rounding off tension between the natural way to do
 * the colours (perfect having its own) and the natural way to do the stars for
 * the health bar (zero having its own). It should be unnoticeable to the
 * player.
 */
int health_level(int current, int max)
{
    int level;

    if (current == max)
    {
        level = HEALTH_UNHURT; // 100%
    }

    else
    {
        switch ((4 * current + max - 1) / max)
        {
        case 4:
            level = HEALTH_SOMEWHAT_WOUNDED;
            break; //  76% - 99%
        case 3:
            level = HEALTH_WOUNDED;
            break; //  51% - 75%
        case 2:
            level = HEALTH_BADLY_WOUNDED;
            break; //  26% - 50%
        case 1:
            level = HEALTH_ALMOST_DEAD;
            break; //   1% - 25%
        default:
            level = HEALTH_DEAD;
            break; //   0%
        }
    }

    return (level);
}

/*
 *  Assigns colours to the health levels.
 */
byte health_attr(int current, int max)
{
    byte a;

    switch (health_level(current, max))
    {
    case HEALTH_UNHURT:
        a = TERM_L_GREEN;
        break; // 100%
    case HEALTH_SOMEWHAT_WOUNDED:
        a = TERM_YELLOW;
        break; //  76% - 99%
    case HEALTH_WOUNDED:
        a = TERM_ORANGE;
        break; //  51% - 75%
    case HEALTH_BADLY_WOUNDED:
        a = TERM_L_RED;
        break; //  26% - 50%
    case HEALTH_ALMOST_DEAD:
        a = TERM_RED;
        break; //   1% - 25%
    default:
        a = TERM_RED;
        break; //   0%
    }

    return (a);
}

/*
 * Gets a text string denoting the alertness level / stance into a buffer, along
 * with the associated colour.
 */
bool get_alertness_text(
    monster_type* m_ptr, int text_size, char* text, int* color)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (m_ptr->alertness < ALERTNESS_UNWARY)
    {
        SDL_strlcpy(text, "Sleeping", text_size);
        *color = TERM_BLUE;
    }
    else if (m_ptr->alertness < ALERTNESS_ALERT)
    {
        SDL_strlcpy(text, "Unwary", text_size);
        *color = TERM_L_BLUE;
    }
    else
    {
        if (r_ptr->flags2 & (RF2_MINDLESS))
        {
            SDL_strlcpy(text, "Mindless", text_size);
            *color = TERM_L_DARK;
        }
        else
        {
            char morale_buf[8];

            if (m_ptr->stance == STANCE_FLEEING)
            {
                SDL_strlcpy(text, "Fleeing", text_size);
                *color = TERM_VIOLET;
            }
            else if (m_ptr->stance == STANCE_CONFIDENT)
            {
                SDL_strlcpy(text, "Confident", text_size);
                *color = TERM_L_WHITE;
            }
            else if (m_ptr->stance == STANCE_AGGRESSIVE)
            {
                SDL_strlcpy(text, "Aggress", text_size);
                *color = TERM_L_WHITE;
            }

            // sometimes (only in debugging?) we are looking at a monster before
            // it has a stance in this case return false so we don't print the
            // strings
            else
            {
                return false;
            }

            if (m_ptr->morale >= 0)
                sprintf(morale_buf, " %d", (m_ptr->morale + 9) / 10);
            else
                sprintf(morale_buf, " %d", m_ptr->morale / 10);

            strncat(text, morale_buf, text_size - strlen(text));
        }
    }

    return true;
}

/*
 * Calculate maximum voice.
 *
 * This function induces status messages.
 */

/*
 * Handle "p_ptr->notice"
 */
void notice_stuff(void)
{
    /* Notice stuff */
    if (!p_ptr->notice)
        return;

    /* Combine the pack */
    if (p_ptr->notice & (PN_COMBINE))
    {
        p_ptr->notice &= ~(PN_COMBINE);
        combine_pack();
    }

    /* Reorder the pack */
    if (p_ptr->notice & (PN_REORDER))
    {
        p_ptr->notice &= ~(PN_REORDER);
        reorder_pack(true);
    }

    p_ptr->notice &= ~(PN_AUTOINSCRIBE);
}

/*
 * Handle "p_ptr->update"
 */
void update_stuff(void)
{
    player_update_lore();

    /* Update stuff */
    if (!p_ptr->update) {
        // log_trace("update_stuff: no updates needed");
        return;
    }

    log_trace("update_stuff: processing updates 0x%08X", p_ptr->update);

    if (p_ptr->update & (PU_BONUS))
    {
        p_ptr->update &= ~(PU_BONUS);
        // log_trace("update_stuff: calculating bonuses");
        calc_bonuses();
    }

    if (p_ptr->update & (PU_HP))
    {
        p_ptr->update &= ~(PU_HP);
        // log_trace("update_stuff: calculating hitpoints");
        calc_hitpoints();
    }

    if (p_ptr->update & (PU_MANA))
    {
        p_ptr->update &= ~(PU_MANA);
        // log_trace("update_stuff: calculating voice/mana");
        calc_voice();
    }

    /* Character is not ready yet, no screen updates */
    if (!character_generated) {
        // log_trace("update_stuff: character not generated yet, skipping screen updates");
        return;
    }

    // log_trace("update_stuff: character_icky=%d", character_icky);

    /* Character is in "icky" mode, no screen updates */
    if (character_icky) {
        // log_trace("update_stuff: character in icky mode (value=%d), skipping screen updates", character_icky);
        return;
    }

    if (p_ptr->update & (PU_FORGET_VIEW))
    {
        p_ptr->update &= ~(PU_FORGET_VIEW);
        log_trace("update_stuff: forgetting view");
        forget_view();
    }

    if (p_ptr->update & (PU_UPDATE_VIEW))
    {
        p_ptr->update &= ~(PU_UPDATE_VIEW);
        log_trace("update_stuff: updating view");
        update_view();
        
        /* Check artifact visibility after view update */
        check_artifact_visibility();
    }

    if (p_ptr->update & (PU_DISTANCE))
    {
        p_ptr->update &= ~(PU_DISTANCE);
        p_ptr->update &= ~(PU_MONSTERS);
        log_trace("update_stuff: updating distances and monsters");
        update_monsters(true);
    }

    if (p_ptr->update & (PU_MONSTERS))
    {
        p_ptr->update &= ~(PU_MONSTERS);
        update_monsters(false);
    }

    if (p_ptr->update & (PU_PANEL))
    {
        p_ptr->update &= ~(PU_PANEL);
        verify_panel();
    }

    /* Check quest completion status for metarun tracking */
    // log_trace("update_stuff: About to call metarun_check_and_update_quests()");
    metarun_check_and_update_quests();
    // log_trace("update_stuff: Finished calling metarun_check_and_update_quests()");

    // log_trace("update_stuff: completed all updates");
}

/*
 * Handle "p_ptr->redraw"
 */
void redraw_stuff(void)
{
    const u32b chrome_redraw_mask = PR_BASIC | PR_MISC | PR_EXP | PR_STATS
        | PR_MEL | PR_ARC | PR_QUIVER | PR_ARMOR | PR_HP | PR_VOICE
        | PR_SONG | PR_DEPTH | PR_HEALTHBAR | PR_EXTRA | PR_CUT | PR_STUN
        | PR_HUNGER | PR_BLIND | PR_CONFUSED | PR_AFRAID | PR_POISONED
        | PR_STATE | PR_SPEED | PR_TERRAIN | PR_LIGHT | PR_RESIST;

    /* Redraw stuff */
    if (!p_ptr->redraw) {
        // log_trace("redraw_stuff: no redraws needed");
        return;
    }

    // log_trace("redraw_stuff: processing redraws 0x%08X", p_ptr->redraw);

    /* Character is not ready yet, no screen updates */
    if (!character_generated)
        return;

    // log_trace("redraw_stuff: character_icky=%d, character_generated=%s", 
            //   character_icky, character_generated ? "true" : "false");

    /* Character is in "icky" mode, no screen updates */
    if (character_icky && !p_ptr->is_dead) {
        // log_trace("redraw_stuff: character in icky mode (value=%d), skipping screen updates", character_icky);
        return;
    }

    if (p_ptr->redraw & (PR_MAP))
    {
        p_ptr->redraw &= ~(PR_MAP);
        log_trace("redraw_stuff: redrawing map");
    }

    /*
     * The visible main pane is scene-owned on this SDL-only build, so status
     * and chrome redraw flags no longer drive direct term rendering here.
     * They still matter to snapshot invalidation because handle_stuff()
     * preserves the original redraw mask before calling this function.
     */
    if (p_ptr->redraw & chrome_redraw_mask)
        p_ptr->redraw &= ~chrome_redraw_mask;

    // log_trace("redraw_stuff: completed all redraws");
}

static bool ui_status_refresh_window_flag(u32b* pending_mask, u32b flag,
    ui_status_window_scene_build_fn build_scene, bool refresh_quiver)
{
    bool rendered = false;

    if (!pending_mask || !(*pending_mask & flag))
        return false;

    *pending_mask &= ~flag;
    rendered = ui_status_refresh_matching_windows(flag, build_scene);

    if (refresh_quiver)
        p_ptr->redraw |= PR_QUIVER;

    return rendered;
}

void ui_status_refresh_window_mask(u32b window_mask)
{
    bool rendered_any = false;

    if (!window_mask)
        return;

    rendered_any = ui_status_refresh_window_flag(&window_mask, PW_INVEN,
        ui_status_build_inventory_window_scene, false) || rendered_any;
    rendered_any = ui_status_refresh_window_flag(&window_mask, PW_MONLIST,
        ui_status_build_monlist_window_scene, false) || rendered_any;
    rendered_any = ui_status_refresh_window_flag(&window_mask, PW_EQUIP,
        ui_status_build_equipment_window_scene, true) || rendered_any;
    rendered_any = ui_status_refresh_window_flag(&window_mask, PW_PLAYER_0,
        ui_status_build_player_window_scene, false) || rendered_any;
    rendered_any = ui_status_refresh_window_flag(&window_mask, PW_MESSAGE,
        ui_status_build_message_window_scene, false) || rendered_any;
    rendered_any = ui_status_refresh_window_flag(&window_mask, PW_OBJECT,
        ui_status_build_object_window_scene, false) || rendered_any;
    rendered_any = ui_status_refresh_window_flag(&window_mask, PW_OVERHEAD,
        ui_status_build_overhead_window_scene, false) || rendered_any;
    rendered_any = ui_status_refresh_window_flag(&window_mask, PW_MONSTER,
        ui_status_build_monster_window_scene, false) || rendered_any;

    if (rendered_any)
        platform_frame_present();
}

/*
 * Handle "p_ptr->window"
 */
void window_stuff(void)
{
    int j;
    u32b mask = 0L;
    u32b pending_window;

    /* Nothing to do */
    if (!p_ptr->window) {
        // log_trace("window_stuff: no window updates needed");
        return;
    }

    log_trace("window_stuff: processing windows 0x%08X", p_ptr->window);

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Save usable flags */
        if (platform_frame_view_ready(j))
        {
            /* Build the mask */
            mask |= op_ptr->window_flag[j];
        }
    }

    /* Apply usable flags */
    p_ptr->window &= (mask);

    /* Nothing to do */
    if (!p_ptr->window)
        return;

    pending_window = p_ptr->window;
    p_ptr->window = 0;
    ui_status_refresh_window_mask(pending_window);

    // log_trace("window_stuff: completed all window updates");
}

/*
 * Handle "p_ptr->update" and "p_ptr->redraw" and "p_ptr->window"
 */
void handle_stuff(void)
{
    u32b update_mask = p_ptr->update;
    u32b redraw_mask = p_ptr->redraw;
    u32b window_mask = p_ptr->window;

    log_trace("handle_stuff: starting (update=0x%08X, redraw=0x%08X, window=0x%08X)", 
              p_ptr->update, p_ptr->redraw, p_ptr->window);

    /* Update stuff */
    if (p_ptr->update)
        update_stuff();

    /* Redraw stuff */
    if (p_ptr->redraw)
        redraw_stuff();

    /* Window stuff */
    if (p_ptr->window)
        window_stuff();

    if (character_generated && p_ptr->playing)
    {
        app_session* session = app_session_current();

        if (session && (app_session_snapshot(session)->scene
                == APP_SCENE_KIND_DUNGEON))
        {
            (void)app_session_build_dungeon_snapshot(session, update_mask,
                redraw_mask, window_mask);
        }
    }

    log_trace("handle_stuff: completed");
}

