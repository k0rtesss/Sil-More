/* File: runtime/runtime-dungeon-session.c */
/*
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
#include "app/app-scene-birth.h"
#include "blitz.h"
#include "drop_system.h"
#include "fs/load.h"
#include "log/log.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-frame.h"
#include "runtime/runtime-cli.h"
#include "runtime/runtime-dungeon-internal.h"
#include "runtime/runtime-game.h"
#include "score/score_io.h"

#include <string.h>
#include <time.h>

static void snapshot_run_history(const char* reason)
{
    if (!character_generated || !p_ptr || p_ptr->is_dead)
        return;

    time_t now = time(NULL);
    if (now == (time_t)-1)
        now = 0;

    if (!score_refresh_live_snapshot(now, reason))
    {
        log_warn("run snapshot failed (%s)", reason ? reason : "unspecified");
    }
    else if (reason)
    {
        log_trace("run snapshot recorded (%s)", reason);
    }
}

bool enter_wizard_mode(void)
{
    if (!(p_ptr->noscore & 0x0008) && !p_ptr->is_dead)
    {
        msg_print("You can only enter wizard mode from within debug mode.");
        log_debug("Wizard mode denied - must be in debug mode first");
        return false;
    }

    p_ptr->noscore |= 0x0002;

    log_info("Entering wizard mode - savefile marked (noscore=0x%04X, savefile='%s')",
        (unsigned)p_ptr->noscore, savefile);
    return true;
}

#ifdef ALLOW_DEBUG
bool verify_debug_mode(void)
{
    char buf[80] = "It is not mellon";

    if (!(p_ptr->noscore & 0x0008))
    {
        msg_print(
            "You are about to use the dangerous, unsupported, debug commands!");
        msg_print(
            "Your machine may crash, and your savefile may become corrupted!");
        message_flush();

        if (!get_check("Are you sure you want to use the debug commands? "))
            return false;

        if (DEPLOYMENT)
        {
            if (prompt_text_input("Password:",
                    "Enter accepts, Esc cancels, Backspace erases.", buf,
                    sizeof(buf), false)
                && strcmp(buf, "Gondolin") == 0)
            {
                p_ptr->noscore |= 0x0008;
                return true;
            }

            msg_print("Incorrect password.");
            return false;
        }
    }

    p_ptr->noscore |= 0x0008;

    log_info("Debug mode enabled (noscore=0x%04X, savefile='%s')",
        (unsigned)p_ptr->noscore, savefile);
    return true;
}
#endif /* ALLOW_DEBUG */

/*
 * Hack - Know inventory upon death
 */
PlayResult play_game(void)
{
    bool new_game = false;

    log_info("play_game: FUNCTION ENTERED");

    if (character_icky != 0)
    {
        log_info("play_game: Fixing character_icky imbalance - was %d, resetting to 0",
            character_icky);
        character_icky = 0;
    }

    character_icky++;
    log_debug("play_game: character_icky incremented to %d", character_icky);

    if (!platform_frame_main_view_ready())
        quit("main window does not exist");

    {
        const int min_hgt = platform_current_min_terminal_rows();
        const int min_wid = platform_current_min_terminal_cols();
        const int main_hgt = platform_frame_main_grid_rows();
        const int main_wid = platform_frame_main_grid_cols();

        if ((main_hgt < min_hgt) || (main_wid < min_wid))
        {
#if defined(__ANDROID__) || defined(SIL_IOS)
            log_error("main window too small on mobile: %dx%d (need at least %dx%d)",
                main_wid, main_hgt, min_wid, min_hgt);
#else
            log_error("main window too small: %dx%d (need at least %dx%d)",
                main_wid, main_hgt, min_wid, min_hgt);
#endif
            quit("main window is too small");
        }
    }

    if (!op_ptr->base_name[0])
        SDL_strlcpy(op_ptr->base_name, "nameless", sizeof(op_ptr->base_name));

    run_mode_activate_pending();
    runtime_dungeon_show_startup_presentations();

    character_loaded = false;
    character_loaded_dead = false;
    if (autoload_alive_from_scores() && character_loaded)
    {
        log_info("Auto-loaded alive character from scores; skipping selection");
        new_game = false;
    }

    log_info("Starting new game session");

    if (!character_loaded)
    {
        character_dungeon = false;
        character_loaded = false;
        character_loaded_dead = false;
    }

    for (;;)
    {
        if (character_loaded)
            break;

        player_wipe();

        log_info("Choosing character");
        {
            NavResult cr = run_mode_is_blitz() ? blitz_character_creation()
                                               : character_creation();

            if (cr == NAV_TO_MAIN)
            {
                log_info("Returning to main menu from character creation");
                platform_music_stop_main();
                platform_music_stop_ambient();
                return PLAY_DONE;
            }
            if (cr == NAV_QUIT)
            {
                log_info("Quitting from character creation");
                return PLAY_QUIT;
            }
        }

        SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name,
            sizeof(op_ptr->full_name));
        process_player_name(true);
        log_debug("Player name set to: %s (character %d), savefile: %s",
            op_ptr->full_name, p_ptr->pcharacter, savefile);

        (void)load_player();

        if (character_loaded_dead)
        {
            log_info("Loaded dead character from '%s' - wiping for fresh restart",
                savefile);

            player_wipe();
            rp_ptr = &p_info[p_ptr->prace];
            current_character_profile = &c_info[p_ptr->pcharacter];
            character_loaded_dead = false;
        }

        log_info(character_loaded ? "Character loaded"
                                  : (character_loaded_dead
                                      ? "Character loaded dead"
                                      : "Character creation started"));

        new_game = !character_loaded;

        if (new_game)
        {
            u64b seed = (u64b)time(NULL);

            log_info("Starting new game - initializing character");
#ifdef SET_UID
            seed ^= ((seed >> 3) * (getpid() << 1));
#endif

            Rand_state_init(seed);
            log_debug("RNG initialized with seed: %llu",
                (unsigned long long)seed);

            log_info("Rolling up a new character");
            log_trace("Character creation phase: setting up dungeon state");
            character_dungeon = false;

            seed_flavor = rand_int(0x10000000);
            seed_randart = rand_int(0x10000000);

            log_debug("Game seeds initialized - flavor: %u, randart: %u",
                seed_flavor, seed_randart);

            {
                NavResult br = player_birth();

                if (br == NAV_BACK)
                {
                    log_debug("Returning to character selection from birth");
                    continue;
                }
                if (br == NAV_TO_MAIN)
                {
                    log_info("Returning to main menu from character birth");
                    platform_music_stop_main();
                    platform_music_stop_ambient();
                    return PLAY_DONE;
                }
                if (br == NAV_QUIT)
                {
                    log_info("Quitting from character birth");
                    return PLAY_QUIT;
                }
            }

            if (!character_loaded)
            {
                turn = 1;
                playerturn = 0;
                min_depth_counter = 0;
                p_ptr->depth = 1;
                log_debug("New game state initialized - starting at depth 1, turn 1");
            }
        }

        break;
    }

    if (savefile[0])
        process_player_name(false);
    else
        process_player_name(true);

    runtime_dungeon_show_opening_story_if_needed();

    log_debug("Game initialization complete, starting main game loop");
    log_trace("QUEST DEBUG: Quest states loaded - Aule: %d, Mandos: %d, Tulkas: %d",
        p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->tulkas_quest);
    log_trace("QUEST DEBUG: Special abilities - have_ability[S_SPC][SPC_MANDOS]=%d, have_ability[S_SPC][SPC_AULE]=%d",
        p_ptr->have_ability[S_SPC][SPC_MANDOS],
        p_ptr->have_ability[S_SPC][SPC_AULE]);
    log_trace("QUEST DEBUG: Special abilities - active_ability[S_SPC][SPC_MANDOS]=%d, active_ability[S_SPC][SPC_AULE]=%d",
        p_ptr->active_ability[S_SPC][SPC_MANDOS],
        p_ptr->active_ability[S_SPC][SPC_AULE]);

    validate_tulkas_quest_on_load();

    if (runtime_cli_wizard() && enter_wizard_mode())
    {
        p_ptr->wizard = true;
        log_debug("Wizard mode activated");
    }

    flavor_init();
    drop_system_init();
    reset_visuals(true);

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    p_ptr->window |= (PW_MONSTER | PW_MESSAGE);
    window_stuff();

    if (runtime_cli_force_original())
        hjkl_movement = false;
    if (runtime_cli_force_roguelike())
        hjkl_movement = true;

    reset_visuals(true);

    if (!character_dungeon)
    {
        log_info("Generating initial dungeon level");
        runtime_dungeon_reset_level_entry_tracking();
        generate_cave();
        log_debug("Initial dungeon level generated successfully");
    }

    character_generated = true;
    log_debug("play_game: character_generated set to true - character creation complete");
    ability_log_sync_missing();
    snapshot_run_history("character start");

    if (p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE
        && p_ptr->tulkas_quest_complete == 1)
    {
        int y;
        int x;
        bool spawned = false;

        log_trace("Spawning Tulkas for auto-completed quest on load");

        for (y = p_ptr->py - 3; y <= p_ptr->py + 3 && !spawned; y++)
        {
            for (x = p_ptr->px - 3; x <= p_ptr->px + 3 && !spawned; x++)
            {
                if (in_bounds(y, x) && cave_floor_bold(y, x)
                    && cave_m_idx[y][x] == 0
                    && distance(p_ptr->py, p_ptr->px, y, x) >= 2
                    && place_monster_one(y, x, R_IDX_TULKAS, true, true, NULL))
                {
                    msg_print("Upon loading, you recall that your quest target has already fallen!");
                    msg_print("Tulkas Unclad materializes nearby with a booming laugh, ready to reward your valor!");
                    spawned = true;
                    log_trace("Tulkas spawned at (%d, %d) for auto-completed quest",
                        y, x);
                }
            }
        }

        if (!spawned)
            log_trace("Failed to spawn Tulkas near player, will retry on next level");
    }

    object_generation_mode = OB_GEN_MODE_NORMAL;
    p_ptr->playing = true;
    metarun_created = false;

    if (p_ptr->chp <= 0)
        p_ptr->is_dead = true;

    runtime_dungeon_begin_active_run_presentation();

    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    turns_since_combat = 0;
    p_ptr->leaping = false;
    p_ptr->knocked_back = false;

    character_icky--;
    log_debug("play_game: character_icky decremented to %d (entering main game loop)",
        character_icky);

    while (true)
    {
        log_trace("Starting dungeon level processing loop");
        runtime_dungeon_run_level();

        if (p_ptr->notice)
            notice_stuff();
        if (p_ptr->update)
            update_stuff();
        if (p_ptr->redraw)
            redraw_stuff();
        if (p_ptr->window)
            window_stuff();

        target_set_monster(0);
        health_track(0);
        forget_view();

        if (!p_ptr->playing && !p_ptr->is_dead)
        {
            log_info("Player quit and saved - exiting game loop");
            platform_music_stop_main();
            platform_music_stop_ambient();
            break;
        }

        if (!p_ptr->is_dead)
        {
            log_trace("Cleaning up level data for transition");
            wipe_o_list();
            wipe_mon_list();
        }

        message_flush();

        if (p_ptr->playing && p_ptr->is_dead)
        {
            log_info("Player '%s' died at level %d, turn %d.",
                op_ptr->base_name, p_ptr->depth, turn);
            if ((p_ptr->wizard || (p_ptr->noscore & 0x0008) || cheat_live)
                && !get_check("Die? "))
            {
                log_debug("Player cheated death - restoring to full health");
                p_ptr->noscore |= 0x0001;

                msg_print("You invoke wizard mode and cheat death.");
                message_flush();

                p_ptr->is_dead = false;
                p_ptr->chp = p_ptr->mhp;
                p_ptr->chp_frac = 0;
                p_ptr->csp = p_ptr->msp;
                p_ptr->csp_frac = 0;
                (void)set_blind(0);
                (void)set_confused(0);
                (void)set_poisoned(0);
                (void)set_afraid(0);
                (void)set_entranced(0);
                (void)set_image(0);
                (void)set_stun(0);
                (void)set_cut(0);
                (void)res_stat(A_STR, 20);
                (void)res_stat(A_CON, 20);
                (void)res_stat(A_DEX, 20);
                (void)res_stat(A_GRA, 20);
                (void)set_food(PY_FOOD_FULL - 1);
                SDL_strlcpy(p_ptr->died_from, "Cheating death",
                    sizeof(p_ptr->died_from));
                p_ptr->leaving = true;
            }
        }

        if (p_ptr->is_dead)
            runtime_dungeon_prepare_death_presentation();

        if (p_ptr->is_dead)
        {
            log_info("Character '%s' died - ending game session",
                op_ptr->base_name);
            break;
        }

        log_info("Generating new dungeon level at depth %d", p_ptr->depth);
        runtime_dungeon_reset_level_entry_tracking();
        generate_cave();
        log_debug("New dungeon level generated successfully");
    }

    log_info("Player '%s' has left the game.", op_ptr->base_name);

    character_icky--;
    log_debug("play_game: character_icky decremented to %d (function exit)",
        character_icky);

    close_game();
    if (!p_ptr->is_dead && !p_ptr->playing)
    {
        if (p_ptr->quit_to_menu)
        {
            p_ptr->quit_to_menu = false;
            return PLAY_DONE;
        }

        return PLAY_QUIT;
    }

    return PLAY_DONE;
}
