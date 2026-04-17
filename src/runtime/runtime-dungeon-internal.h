#ifndef INCLUDED_RUNTIME_DUNGEON_INTERNAL_H
#define INCLUDED_RUNTIME_DUNGEON_INTERNAL_H

#include "runtime/runtime-dungeon.h"

bool runtime_dungeon_confirm_enter_morgoth_hall(void);
void runtime_dungeon_describe_greater_vault_entry(cptr vault_name);
void runtime_dungeon_handle_partition_entry(bool force_message);
void runtime_dungeon_maybe_show_blitz_unlock_screen(void);
void runtime_dungeon_prepare_death_knowledge(void);
void runtime_dungeon_print_story_intro(void);
void runtime_dungeon_reset_level_entry_tracking(void);
void runtime_dungeon_show_initial_partition_banner(void);
void runtime_dungeon_update_labyrinth_view_state(bool handle_now);

#endif /* INCLUDED_RUNTIME_DUNGEON_INTERNAL_H */
