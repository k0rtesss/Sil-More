/* File: externs.h */
/*
 * Copyright (c) 1997 Ben Harrison
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

#include "h-basic.h"
#include "app/app-movement.h"
#include "cave/cave.h"
#include "cmd/combat/cmd-combat.h"
#include "cmd/debug/cmd-debug.h"
#include "drop_system.h"
#include "cave/cave-state.h"
#include "cmd/monster/cmd-monster.h"
#include "cmd/combat/cmd-ranged.h"
#include "cmd/item/cmd-item.h"
#include "cmd/movement/cmd-movement.h"
#include "cmd/ui/cmd-ui.h"
#include "cmd/world/cmd-world.h"
#include "cmd/world/cmd-interact-chest.h"
#include "fs/load.h"
#include "fs/save.h"
#include "fs/savefile-name.h"
#include "init/init-data.h"
#include "init/init-paths.h"
#include "level-generation/level-generation.h"
#include "melee/melee.h"
#include "monster/monster.h"
#include "monster/monster-state.h"
#include "object/object.h"
#include "object/object-randart.h"
#include "object/object-state.h"
#include "object/object-slot.h"
#include "object/object-ui-select.h"
#include "object/object-use.h"
#include "player/ability_log.h"
#include "player/encumbrance.h"
#include "player/identification.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-calc.h"
#include "player/player-oaths.h"
#include "player/player-resources.h"
#include "player/player-song-disguise.h"
#include "player/player-song-duels.h"
#include "player/player-song-effects.h"
#include "player/player-songs.h"
#include "player/player-state.h"

typedef struct app_ui_scene app_ui_scene;
#include "player/weapon_stats.h"
#include "quest/quest.h"
#include "runtime/runtime-dungeon.h"
#include "runtime/runtime-game.h"
#include "runtime/runtime-state.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_ui.h"
#include "platform-signals.h"
#include "smithing/smithing.h"
#include "spell/spell.h"
#include "support/text-output.h"
#include "support/util.h"
#include "ui/ui-character-screen.h"
#include "ui/colors.h"
#include "ui/ui-death.h"
#include "ui/ui-file-viewer.h"
#include "ui/ui-help.h"
#include "ui/ui-look-sidebar.h"
#include "ui/smithing/ui-smithing-screen.h"
#include "ui/ui-story.h"
#include "ui/story_font.h"
#include "ui/targeting.h"
#include "ui/ui-status.h"
/* Transitional globals */
extern const cptr angband_sound_name[MSG_MAX];

/* Public style color encoding base for save/load */
#ifndef COLOR_STYLE_BASE
#define COLOR_STYLE_BASE 128
#endif

/* Default vein tile accessors (defined in init1.c) */
byte get_default_vein_row(void);
byte get_default_vein_col(void);
bool get_overlay_key_enabled(void);
void get_overlay_key_rgb(byte* r, byte* g, byte* b);
/*
 * Automatically generated "function declarations"
 */

/* birth.c */
extern NavResult player_birth(void);
extern NavResult character_creation(void);
extern NavResult blitz_character_creation(void);
void player_wipe(void);

/* cmd1.c */
extern int count_open_adjacent_squares(int y, int x);

/* cmd2.c */
extern void do_cmd_steal(void);
extern void do_cmd_spike(void);
extern void do_cmd_jump(void);

/* cmd3.c */
extern void do_cmd_use_item_by_index(int item);
extern void do_cmd_use_item(void);
extern void do_cmd_use_item_enhanced(void);
extern void do_cmd_inven_direct(void);
extern void do_cmd_equip_direct(void);
extern void do_cmd_wield_wrapper(void);
extern void do_cmd_wield_enhanced(void);
extern void do_cmd_destroy(void);
extern void do_cmd_observe(void);
extern void do_cmd_observe_enhanced(void);
extern void do_cmd_uninscribe(void);
extern void do_cmd_inscribe(void);
extern void py_steal(int y, int x);

/* cmd4.c */
extern void options_birth_menu(bool adult);
extern void show_songs_with_highlight(int highlight);
extern void wipe_screen_from(int col);
extern void do_cmd_visuals(void);

/* cmd5.c */
/* cmd6.c */

/* Runtime compatibility declarations */
extern void safe_setuid_drop(void);
extern void safe_setuid_grab(void);

/* init2.c */
extern bool prep_object_theme(int themetype);

/* randart.c */

// Metarun.c

extern errr load_metaruns(bool create_if_missing);
extern bool metarun_created;
extern u32b curse_flag_mask(void);
extern int curse_flag_count_rhf(u32b rhf_flag);
extern int curse_flag_count_cur(u32b cur_flag);
extern int curse_flag_delta_cur(u32b cur_flag);
extern int  any_curse_flag_active(u32b flag); /* CUR-only */

// init1.c
extern void dbg_show_active_flags(void);
