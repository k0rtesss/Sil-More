/* File: app-scene-birth.c */
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

#include "app-scene-birth.h"

#include "blitz.h"
#include "log/log.h"
#include "platform-config.h"
#include "player/killer.h"
#include "ui/ui-information-scene.h"

static NavResult player_birth_aux(void)
{
    NavResult result = NAV_OK;
    ui_information_scene_scope semantic_scope;

    log_debug("Initializing character data and history");
    birth_set_assignment_review_pending(true);

    SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name,
        sizeof(op_ptr->full_name));
    process_player_name(true);
    p_ptr->history[0] = '\0';
    SDL_strlcat(p_ptr->history, c_text + c_info[p_ptr->pcharacter].text,
        sizeof(p_ptr->history));

    p_ptr->wt = 0;
    p_ptr->ht = 0;
    p_ptr->age = 0;

    if (!ui_information_scene_enter(&semantic_scope))
    {
        log_error("player_birth_aux: semantic assignment scene unavailable");
        return NAV_TO_MAIN;
    }

    if (run_mode_is_blitz() && !blitz_oaths_enabled())
    {
        p_ptr->oath_type = 0;
    }
    else
    {
        log_debug("Entering oath selection");
        result = birth_select_oath();
        if (result != NAV_OK)
            goto cleanup_semantic_birth_ui;
        log_debug("Oath selection completed");
    }

    if (run_mode_is_blitz())
    {
        result = birth_blitz_configure_effects();
        if (result != NAV_OK)
            goto cleanup_semantic_birth_ui;
    }

    if (blitz_auto_allocates_stats())
    {
        result = birth_blitz_auto_build_character();
        if (result != NAV_OK)
            goto cleanup_semantic_birth_ui;
    }
    else
    {
        for (;;)
        {
            NavResult stat_result;

            log_debug("Entering stats allocation");
            stat_result = birth_run_stats_allocation();
            if (stat_result == NAV_OK)
            {
                log_debug("Stats accepted, entering skills allocation");
                result = gain_skills();
                if (result != NAV_OK)
                    break;
                log_debug("Skills allocation completed");
                break;
            }
            if (stat_result == NAV_BACK)
            {
                result = NAV_BACK;
                break;
            }
            if (stat_result == NAV_TO_MAIN)
            {
                result = NAV_TO_MAIN;
                break;
            }
            if (stat_result == NAV_QUIT)
            {
                result = NAV_QUIT;
                break;
            }
        }
    }

    p_ptr->artefacts = 0;

    log_trace("Final character stats: Str=%d Dex=%d Con=%d Gra=%d",
        p_ptr->stat_base[A_STR], p_ptr->stat_base[A_DEX],
        p_ptr->stat_base[A_CON], p_ptr->stat_base[A_GRA]);

cleanup_semantic_birth_ui:
    ui_information_scene_leave(&semantic_scope);
    return result;
}

NavResult blitz_character_creation(void)
{
    blitz_runtime_reset();

    if (birth_blitz_setup_menu() != NAV_OK)
        return NAV_TO_MAIN;

    if (blitz_current_setup()->character_mode == BLITZ_CHARACTER_SELECTED)
        return character_creation();

    birth_blitz_pick_random_race_and_character();
    birth_finalize_character_creation_selection();
    return NAV_OK;
}

NavResult character_creation(void)
{
    return birth_run_character_creation_menu();
}

NavResult player_birth(void)
{
    int i;
    char raw_date[25];
    char clean_date[25];
    char month[4];
    time_t ct = time((time_t*)0);

    log_info("Starting character creation process");
    killer_reset();

    while (1)
    {
        NavResult result = player_birth_aux();

        if (result == NAV_OK)
            break;
        if (result == NAV_BACK)
            return NAV_BACK;
        if (result == NAV_TO_MAIN)
            return NAV_TO_MAIN;
        if (result == NAV_QUIT)
            return NAV_QUIT;
    }

    for (i = 0; i < NOTES_LENGTH; i++)
        notes_buffer[i] = '\0';

    (void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));

    sprintf(month, "%.2s", raw_date + 5);
    atomonth(atoi(month), month);

    if (*(raw_date + 7) == '0')
    {
        sprintf(clean_date, "%.1s %.3s %.4s", raw_date + 8, month,
            raw_date + 1);
    }
    else
    {
        sprintf(clean_date, "%.2s %.3s %.4s", raw_date + 7, month,
            raw_date + 1);
    }

    SDL_strlcat(notes_buffer,
        format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name),
        sizeof(notes_buffer));
    SDL_strlcat(notes_buffer, format("Entered Angband on %s\n", clean_date),
        sizeof(notes_buffer));
    SDL_strlcat(notes_buffer, "\n   Turn     Depth   Note\n\n",
        sizeof(notes_buffer));

    message_add(" ", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add("====================", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add(" ", MSG_GENERIC);

    birth_player_outfit();

    if (!run_mode_is_blitz())
        metarun_load_persistent_settings();

    platform_load_app_options();

    log_info("Character creation completed: %s the %s", op_ptr->full_name,
        p_name + rp_ptr->name);

    return NAV_OK;
}
