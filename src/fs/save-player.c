/* File: save-player.c */
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
#include "blitz.h"
#include "fs/path.h"
#include "fs/save-internal.h"
#include "log/log.h"
#include "metarun/metarun-meta-state.h"
#include <stdio.h>

static void updatecharinfoS(void)
{
    char tmp_Path[1024];
    char parsed_dir_user[1024];

    int curDepth = p_ptr->max_depth * 50;

    log_debug("Creating character output file");

    if (!path_parse(parsed_dir_user, sizeof(parsed_dir_user), ANGBAND_DIR_USER))
    {
        log_warn("updatecharinfoS: unable to resolve user directory");
        return;
    }

    if (!path_build(tmp_Path, sizeof(tmp_Path), parsed_dir_user,
            "CharOutput.txt"))
    {
        log_warn("updatecharinfoS: unable to build character output path");
        return;
    }

    FILE* oFile = fopen(tmp_Path, "w");
    if (!oFile)
    {
        log_warn("Failed to open character output file: %s", tmp_Path);
        return;
    }

    fprintf(oFile, "{\n");

    const char* race_str = p_name + p_info[p_ptr->prace].name;
    fprintf(oFile, "race: \"%s\",\n", race_str);

    const char* class_str = c_name + c_info[p_ptr->pcharacter].name;
    fprintf(oFile, "class: \"%s\",\n", class_str);

    fprintf(oFile, "mDepth: \"%i\",\n", curDepth);
    fprintf(oFile, "isDead: \"%i\",\n", p_ptr->is_dead);
    fprintf(oFile, "killedBy: \"%s\",\n", p_ptr->died_from);
    fprintf(oFile, "onTheRun: \"%i\",\n", p_ptr->on_the_run);
    fprintf(oFile, "morgothDead: \"%i\"\n", p_ptr->morgoth_slain);

    fprintf(oFile, "}");
    fclose(oFile);
    log_debug("Character output file written successfully: %s", tmp_Path);
}

void wr_extra(void)
{
    int i, j;

    wr_string(op_ptr->full_name);

    wr_string(p_ptr->died_from);

    wr_string(p_ptr->history);

    wr_byte(p_ptr->prace);
    wr_byte(p_ptr->pcharacter);
    wr_byte(p_ptr->unused1);

    wr_s16b(p_ptr->game_type);

    wr_s16b(p_ptr->age);
    wr_s16b(p_ptr->ht);
    wr_s16b(p_ptr->wt);

    for (i = 0; i < A_MAX; ++i)
        wr_s16b(p_ptr->stat_base[i]);
    for (i = 0; i < A_MAX; ++i)
        wr_s16b(p_ptr->stat_drain[i]);

    for (i = 0; i < S_MAX; ++i)
        wr_s16b(p_ptr->skill_base[i]);

    for (i = 0; i < S_MAX; ++i)
    {
        for (j = 0; j < ABILITIES_MAX; ++j)
        {
            wr_byte(p_ptr->innate_ability[i][j]);
            wr_byte(p_ptr->active_ability[i][j]);
            wr_byte(p_ptr->have_ability[i][j]);

            if (i == S_SPC && p_ptr->have_ability[i][j] != 0)
            {
                log_trace("Save: Special ability %d has value %d", j,
                    p_ptr->have_ability[i][j]);
            }
        }
    }

    ability_log_sync_missing();
    u16b ability_events = p_ptr->ability_timeline_count;
    if (ability_events > ABILITY_TIMELINE_MAX)
        ability_events = ABILITY_TIMELINE_MAX;
    wr_u16b(ability_events);
    for (u16b idx = 0; idx < ability_events; idx++)
    {
        wr_byte(p_ptr->ability_timeline_skill[idx]);
        wr_byte(p_ptr->ability_timeline_ability[idx]);
        wr_u32b(p_ptr->ability_timeline_turn[idx]);
        wr_s16b(p_ptr->ability_timeline_depth[idx]);
    }

    wr_s16b(p_ptr->last_attack_m_idx);
    wr_s16b(p_ptr->consecutive_attacks);
    wr_s16b(p_ptr->bane_type);

    for (i = 0; i < ACTION_MAX; ++i)
        wr_byte(p_ptr->previous_action[i]);
    wr_byte(p_ptr->focused);

    wr_s32b(p_ptr->new_exp);
    wr_s32b(p_ptr->exp);

    wr_s32b(p_ptr->encounter_exp);
    wr_s32b(p_ptr->kill_exp);
    wr_s32b(p_ptr->descent_exp);
    wr_s32b(p_ptr->ident_exp);

    wr_s16b(p_ptr->mhp);
    wr_s16b(p_ptr->chp);
    wr_u16b(p_ptr->chp_frac);

    wr_s16b(p_ptr->msp);
    wr_s16b(p_ptr->csp);
    wr_u16b(p_ptr->csp_frac);

    wr_s16b(p_ptr->max_depth);

    wr_u16b(p_ptr->staircasiness);

    wr_s16b(p_ptr->morgoth_state);

    wr_byte(p_ptr->song1);
    wr_byte(p_ptr->song2);
    wr_s16b(p_ptr->song_duration);
    wr_s16b(p_ptr->song_target_idx);
    wr_byte(p_ptr->song_target_song);
    wr_byte(p_ptr->song_lockout_timer);
    wr_byte(p_ptr->song_contest_player_stacks);
    wr_byte(p_ptr->song_duel_pad);
    wr_s32b(p_ptr->song_contest_last_turn);
    wr_s16b(p_ptr->vengeance);
    wr_s16b(p_ptr->blind);
    wr_s16b(p_ptr->entranced);
    wr_s16b(p_ptr->confused);
    wr_s16b(p_ptr->food);
    wr_u16b(p_ptr->stairs_taken);
    wr_u16b(p_ptr->fixed_forge_count);
    wr_u16b(p_ptr->forge_count);
    wr_s16b(p_ptr->energy);
    wr_s16b(p_ptr->fast);
    wr_s16b(p_ptr->slow);
    wr_s16b(p_ptr->afraid);
    wr_s16b(p_ptr->cut);
    wr_s16b(p_ptr->stun);
    wr_s16b(p_ptr->poisoned);
    wr_s16b(p_ptr->image);
    wr_s16b(p_ptr->rage);
    wr_s16b(p_ptr->tmp_str);
    wr_s16b(p_ptr->tmp_dex);
    wr_s16b(p_ptr->tmp_con);
    wr_s16b(p_ptr->tmp_gra);
    wr_s16b(p_ptr->tim_invis);
    wr_s16b(p_ptr->tmp_per);
    wr_s16b(p_ptr->darkened);
    wr_s16b(p_ptr->oppose_fire);
    wr_s16b(p_ptr->oppose_cold);
    wr_s16b(p_ptr->oppose_pois);

    wr_s16b(p_ptr->song_challenge_effect);
    wr_s16b(p_ptr->song_elbereth_effect);

    wr_byte(p_ptr->stealth_mode);
    wr_byte(p_ptr->self_made_arts);

    meta_monster_sync_player_state();

    wr_byte(p_ptr->climbing);

    wr_byte(p_ptr->morgoth_hall_entered ? 1 : 0);
    wr_byte(p_ptr->morgoth_second_wind ? 1 : 0);
    wr_byte(p_ptr->discovery_lore_flags);
    wr_s16b(p_ptr->lamp_oil);
    wr_u16b(0U);
    wr_u32b(0L);
    wr_u32b(0L);

    for (i = 0; i < LEGACY_ITEM_QUALITY_BYTES; i++)
        wr_byte(0);

    wr_string(g_vault_name);

    wr_u16b(z_info->e_max);

    for (i = 0; i < z_info->e_max; i++)
    {
        ego_item_type* e_ptr = &e_info[i];
        byte tmp8u = 0;

        if (e_ptr->everseen)
            tmp8u |= 0x02;
        if (e_ptr->aware)
            tmp8u |= 0x04;

        wr_byte(tmp8u);
    }

    wr_u16b(0);

    for (i = 0; i < MAX_GREATER_VAULTS; i++)
        wr_s16b(p_ptr->greater_vaults[i]);

    wr_u32b(RANDART_VERSION);
    wr_u32b(seed_randart);
    wr_u32b(seed_flavor);

    wr_u16b(p_ptr->panic_save);
    wr_byte(p_ptr->truce);
    wr_byte(p_ptr->morgoth_hits);
    wr_byte(p_ptr->crown_hint);
    wr_byte(p_ptr->crown_shatter);
    wr_byte(p_ptr->cursed);
    wr_byte(p_ptr->on_the_run);
    wr_byte(p_ptr->morgoth_slain);
    wr_u16b(p_ptr->escaped);
    wr_u16b(p_ptr->noscore);
    wr_u16b(p_ptr->smithing_leftover);
    wr_byte(p_ptr->unique_forge_made ? 1 : 0);
    wr_byte(p_ptr->unique_forge_seen ? 1 : 0);

    wr_byte(p_ptr->is_dead ? 1 : 0);
    log_trace("Player is dead: %d", p_ptr->is_dead);

    wr_byte(feeling);
    wr_byte(do_feeling);

    wr_s32b(turn);
    log_trace("Current turn: %d", turn);

    wr_s32b(playerturn);
    log_trace("Player turn: %d", playerturn);

    wr_byte(p_ptr->crown_shatter_sil2);
    wr_byte(p_ptr->crown_shatter_sil3);

    wr_byte(p_ptr->killed_enemy_with_arrow ? 1 : 0);
    wr_byte(p_ptr->orome_bow_hit_streak);
    wr_byte(p_ptr->orome_spear_ready);

    wr_byte(p_ptr->oath_type);
    wr_byte(p_ptr->oaths_broken);
    log_info(
        "SAVE: About to write quest marker 0x51, oath_type=%d, oaths_broken=%d",
        p_ptr->oath_type, p_ptr->oaths_broken);

#if (VERSION_MAJOR > 0) || (VERSION_MAJOR == 0 && (VERSION_MINOR > 8 || (VERSION_MINOR == 8 && VERSION_PATCH >= 6)))
    log_trace("Writing quest block marker 0x51 (version %s)", VERSION_STRING);
    wr_byte(0x51);
    wr_byte(p_ptr->tulkas_quest);
    wr_s16b(p_ptr->tulkas_target_r_idx);
    wr_s16b(p_ptr->tulkas_prize_a_idx);
    wr_byte(p_ptr->tulkas_quest_complete);
    wr_s16b(p_ptr->tulkas_stronghold_level);
    wr_byte(p_ptr->tulkas_stronghold_placed);
    wr_byte(p_ptr->tulkas_second_roll_done);
    wr_byte(p_ptr->tulkas_orc_mask);
    wr_byte(p_ptr->tulkas_orc_restricted);
    wr_byte(p_ptr->tulkas_second_spawn_pending);
    wr_byte(p_ptr->tulkas_morgoth_progress);
    wr_byte(p_ptr->aule_quest);
    wr_byte(p_ptr->aule_forge_y);
    wr_byte(p_ptr->aule_forge_x);
    wr_byte(p_ptr->aule_reserved);
    wr_s16b(p_ptr->aule_level);
    wr_s16b(p_ptr->aule_last_object_diff);
    wr_byte(p_ptr->mandos_quest);
    wr_byte(p_ptr->mandos_vault_y);
    wr_byte(p_ptr->mandos_vault_x);
    wr_byte(p_ptr->mandos_monsters_remaining);
    wr_s16b(p_ptr->mandos_level);
    wr_s16b(p_ptr->mandos_reserved);
    wr_byte(p_ptr->mandos_resurrection_primed);
    wr_byte(p_ptr->mandos_resurrection_used);
    wr_byte(p_ptr->niena_quest);
    wr_byte(p_ptr->niena_monsters_seen);
    wr_byte(p_ptr->niena_monsters_killed);
    wr_byte(p_ptr->niena_reserved);
    wr_s16b(p_ptr->niena_level);
    wr_s16b(p_ptr->niena_reserved2);
    wr_byte(p_ptr->orome_quest);
    wr_byte(p_ptr->orome_target_type);
    wr_s16b(p_ptr->orome_target_count);
    wr_s16b(p_ptr->orome_killed_count);
    wr_s16b(p_ptr->orome_wolves_killed);
    wr_s16b(p_ptr->orome_spiders_killed);
    wr_s16b(p_ptr->orome_serpents_killed);
    wr_s16b(p_ptr->orome_vampires_killed);
    wr_s16b(p_ptr->orome_dragons_killed);
    wr_byte(p_ptr->orome_great_hunt_mask);
    wr_byte(p_ptr->varda_quest);
    wr_byte(p_ptr->varda_vault_ready);
    wr_byte(p_ptr->varda_vault_placed);
    wr_byte(p_ptr->varda_shadow_restricted);
    wr_s16b(p_ptr->varda_level);
    wr_byte(p_ptr->varda_shadow_ready);
    wr_byte(p_ptr->varda_shadow_placed);
    wr_byte(p_ptr->varda_shadow_pad);
    wr_s16b(p_ptr->varda_shadow_level);
    for (i = 0; i < VALA_MAX; i++)
        wr_byte(p_ptr->vala_quest_stage2[i]);
    for (i = 0; i < VALA_MAX; i++)
        wr_byte(p_ptr->vala_quest_stage3[i]);
    wr_byte(p_ptr->quest_vault_used);
    for (i = 0; i < (int)N_ELEMENTS(p_ptr->quest_reserved); i++)
        wr_byte(p_ptr->quest_reserved[i]);
#endif

    {
        skeleton_note_state_save sn_state;
        skeleton_note_get_state(&sn_state);
        wr_byte(0x52);
        wr_s16b(sn_state.level_depth);
        wr_s16b(sn_state.note_cap);
        wr_s16b(sn_state.notes_shown);
        wr_s16b(sn_state.map_wid);
        wr_s16b(sn_state.map_hgt);
        wr_u32b(sn_state.hint_used_mask);
        wr_byte(sn_state.seen_count);
        for (i = 0; i < SKELETON_NOTE_SEEN_MAX; i++)
            wr_s16b(sn_state.seen_ids[i]);
    }

    {
        partition_meta_save pm;
        level_partition_meta_get(&pm);
        wr_byte(0x53);
        wr_s16b(pm.grid_rows);
        wr_s16b(pm.grid_cols);
        wr_s16b(pm.partition_count);
        for (i = 0; i < PARTITION_META_MAX; ++i)
            wr_byte(pm.modes[i]);
        for (i = 0; i < PARTITION_META_MAX; ++i)
            wr_byte(pm.big_cave_types[i]);
    }

    {
        wr_byte(0x54);
        wr_s16b(hint_messages_level_depth_for_save());
        wr_s16b(hint_messages_map_wid_for_save());
        wr_s16b(hint_messages_map_hgt_for_save());

        byte count = hint_messages_count_for_save();
        wr_byte(count);
        for (i = 0; i < count; ++i)
        {
            hint_message_meta meta;
            byte line_count = hint_messages_message_line_count(i);
            wr_byte(line_count);
            for (int li = 0; li < line_count; ++li)
                wr_string(hint_messages_message_line(i, li));

            hint_messages_message_meta(i, &meta);
            wr_s16b(meta.source_y);
            wr_s16b(meta.source_x);
            wr_byte(meta.cue_count);
            for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
            {
                wr_string(meta.cue_dists[cue]);
                wr_string(meta.cue_dirs[cue]);
            }
        }
    }

    wr_byte(0x55);
    wr_byte((byte)run_mode_current());
    {
        int8_t *stacks = blitz_runtime_curse_stacks();
        u64b seen = *blitz_runtime_curses_seen();
        for (i = 0; i < METAR_CURSE_SLOTS; ++i)
            wr_byte((byte)(stacks ? stacks[i] : 0));
        wr_u32b((u32b)(seen & 0xFFFFFFFFULL));
        wr_u32b((u32b)(seen >> 32));
    }

    wr_s32b(min_depth_counter);
    log_info("SAVE: min_depth_counter=%d, current depth=%d, calculated min_depth()=%d",
        min_depth_counter, p_ptr->depth, min_depth());

    log_debug("Updating character info output file");
    updatecharinfoS();
}

void wr_randarts(void)
{
    int i, begin;

    if (adult_rand_artefacts)
        begin = 0;
    else
        begin = z_info->art_norm_max;

    wr_u16b(begin);
    wr_u16b(z_info->art_max);
    wr_u16b(z_info->art_norm_max);

    for (i = begin; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        wr_string(a_ptr->name);
        wr_u32b(a_ptr->guid.hi);
        wr_u32b(a_ptr->guid.lo);

        wr_byte(a_ptr->tval);
        wr_byte(a_ptr->sval);
        wr_s16b(a_ptr->pval);

        wr_s16b(a_ptr->att);
        wr_byte(a_ptr->dd);
        wr_byte(a_ptr->ds);
        wr_s16b(a_ptr->evn);
        wr_byte(a_ptr->pd);
        wr_byte(a_ptr->ps);

        wr_s16b(a_ptr->weight);
        wr_s32b(a_ptr->cost);

        wr_u32b(a_ptr->flags1);
        wr_u32b(a_ptr->flags2);
        wr_u32b(a_ptr->flags3);
        wr_u32b(a_ptr->flags4);

        wr_byte(a_ptr->level);
        wr_byte(a_ptr->rarity);

        wr_byte(a_ptr->activation);
        wr_u16b(a_ptr->time);
        wr_u16b(a_ptr->randtime);

        for (int bi = 0; bi < A_MAX; bi++)
            wr_s16b(a_ptr->stat_bonus[bi]);
        for (int bi = 0; bi < S_MAX; bi++)
            wr_s16b(a_ptr->skill_bonus[bi]);
        for (int bi = 0; bi < A_MAX; bi++)
            wr_byte(a_ptr->stat_bonus_set[bi] ? 1 : 0);
        for (int bi = 0; bi < S_MAX; bi++)
            wr_byte(a_ptr->skill_bonus_set[bi] ? 1 : 0);
    }
}
