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

#ifndef INCLUDED_RUNTIME_DUNGEON_INTERNAL_H
#define INCLUDED_RUNTIME_DUNGEON_INTERNAL_H

#include "runtime/runtime-dungeon.h"

bool runtime_dungeon_confirm_enter_morgoth_hall(void);
void runtime_dungeon_begin_active_run_presentation(void);
void runtime_dungeon_begin_level_vault_tracking(void);
bool runtime_dungeon_death_spectator_command_allowed(int command);
void runtime_dungeon_handle_vault_transition(void);
bool enter_wizard_mode(void);
#ifdef ALLOW_DEBUG
bool verify_debug_mode(void);
#endif
void runtime_dungeon_handle_partition_entry(bool force_message);
void runtime_dungeon_maybe_show_blitz_unlock_screen(void);
void runtime_dungeon_prepare_death_knowledge(void);
void runtime_dungeon_prepare_death_presentation(void);
bool runtime_dungeon_prepare_level_presentation(void);
void runtime_dungeon_publish_runtime_snapshot(u32b update_mask,
    u32b redraw_mask, u32b window_mask);
void runtime_dungeon_process_world(void);
void runtime_dungeon_print_story_intro(void);
void runtime_dungeon_regenhp(int regen_multiplier);
void runtime_dungeon_regenmana(int regen_multiplier);
void process_command(void);
void runtime_dungeon_reset_presentation_state(void);
void runtime_dungeon_reset_level_entry_tracking(void);
void runtime_dungeon_scan_artifacts_near_player(void);
void runtime_dungeon_reset_vault_transition_state(void);
void runtime_dungeon_run_level(void);
void runtime_dungeon_show_opening_story_if_needed(void);
void runtime_dungeon_show_initial_partition_banner(void);
void runtime_dungeon_show_startup_presentations(void);
void runtime_dungeon_update_labyrinth_view_state(bool handle_now);

#endif /* INCLUDED_RUNTIME_DUNGEON_INTERNAL_H */
