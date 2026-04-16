/* File: externs.h */

/*
 * Copyright (c) 1997 Ben Harrison
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

/*
 * Note that some files have their own header files
 * (z-virt.h, rng.h)
 */

/*
 * Automatically generated "variable" declarations
 */
#include "h-basic.h"
#include "app/app-movement.h"
#include "cave/cave.h"
#include "cmd/combat/cmd-combat.h"
#include "drop_system.h"
#include "cave/cave-state.h"
#include "cmd/monster/cmd-monster.h"
#include "cmd/combat/cmd-ranged.h"
#include "cmd/item/cmd-item.h"
#include "cmd/movement/cmd-movement.h"
#include "cmd/ui/cmd-ui.h"
#include "cmd/world/cmd-world.h"
#include "cmd/world/cmd-interact-chest.h"
#include "fs/savefile-name.h"
#include "init/init-data.h"
#include "init/init-paths.h"
#include "level-generation/level-generation.h"
#include "melee/melee.h"
#include "monster/monster.h"
#include "monster/monster-state.h"
#include "object/object.h"
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
#include "ui/ui-character-name.h"
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
// extern FILE *log_file;

/* variable.c */
extern s16b image_count;
extern bool shimmer_objects;
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
extern bool (*ang_sort_comp)(const void* u, const void* v, int a, int b);
extern void (*ang_sort_swap)(void* u, void* v, int a, int b);
extern bool (*get_mon_num_hook)(int r_idx);
extern bool (*get_obj_num_hook)(int k_idx);
extern void (*object_info_out_flags)(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
extern byte squelch_level[SQUELCH_BYTES];

/*
 * Automatically generated "function declarations"
 */

/* birth.c */
extern NavResult player_birth(void);
extern NavResult character_creation(void);
extern NavResult blitz_character_creation(void);
void player_wipe(void);

/* cave.c */
/* Style-weight APIs */
/* Narrative text: from style.txt (S:/M1:/M2: lines) */
extern const char* styles_get_style_display(int sidx);
extern const char* styles_get_style_short_desc(int sidx);
extern const char* styles_get_style_m1(int sidx);
extern const char* styles_get_style_m2(int sidx);
extern void clear_active_narrative_banner(void);
extern bool dungeon_active_narrative_banner_animating(u64b now_ms);
extern bool dungeon_query_active_narrative_banner(u64b now_ms, char* text,
    size_t text_size, u64b* started_ms, u32b* hold_ms);
extern void styles_clear_display_messages(void);
extern int p_ptr_depth_proxy(void);

/* cmd1.c */
extern void apply_oath_breaking_curse(int oath_type);
extern void give_player_item(object_type * o_ptr);
extern void do_cmd_pickup_from_pile(void);
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
extern bool ang_sort_comp_hook(const void* u, const void* v, int a, int b);
extern void ang_sort_swap_hook(void* u, void* v, int a, int b);
extern void py_steal(int y, int x);

/* cmd4.c */
extern void options_birth_menu(bool adult);
extern void do_cmd_change_song(void);
extern void show_songs_with_highlight(int highlight);
extern void wipe_screen_from(int col);
extern void do_cmd_options_aux(int page, cptr info);
extern void do_cmd_pane_settings(void);
extern void do_cmd_keybinds(void);
extern void do_cmd_visuals(void);
extern void do_cmd_colors(void);
extern void do_cmd_version(void);
extern void do_cmd_feeling(void);
extern void do_cmd_knowledge_oaths(void);
extern void do_cmd_knowledge_artefacts(void);
extern void do_cmd_knowledge_monsters(void);
extern void do_cmd_knowledge_objects(void);
extern void do_cmd_knowledge_kills(void);
extern void ghost_challenge(void);
extern void do_cmd_knowledge(void);

/* cmd5.c */
/* cmd6.c */
extern void do_cmd_play_instrument(
    object_type* default_o_ptr, int default_item);
extern void do_cmd_activate(void);

/* dungeon.c */
extern bool can_be_pseudo_ided(const object_type* o_ptr);
extern int value_check_aux1(const object_type* o_ptr);
extern void land(void);
extern void pseudo_id_everything(void);
extern void id_everything(void);
extern void death_spectator_view(void);

/* files.c */
extern void safe_setuid_drop(void);
extern void safe_setuid_grab(void);
extern void do_cmd_escape(int);
extern void do_cmd_suicide(void);
extern int meta_write(const metarun*);
extern errr meta_read(metarun*);
extern int meta_seek(int i);
extern int meta_fill(bool);

/* init2.c */
extern void init_file_paths(char* path);

/* load.c */
extern bool load_player(void);
extern bool load_meta(void);

extern bool prep_object_theme(int themetype);

/* randart.c */
extern void make_random_name(char* random_name, size_t max);
extern s32b artefact_power(int a_idx);
extern void build_randart_tables(void);
extern void free_randart_tables(void);
extern errr do_randart(u32b randart_seed, bool full);
extern bool make_one_randart(
    object_type* o_ptr, int art_power, bool namechoice);
extern void artefact_wipe(int a_idx);
extern bool can_be_randart(const object_type* o_ptr);

/* save.c */
extern bool save_player(void);

/*use-obj.c*/
/* util.c */

extern bool inkey_can_consume_immediately(void);

/*
 * Hack -- conditional (or "bizarre") externs
 */

#ifdef SET_UID
#endif /* SET_UID */

#ifdef ALLOW_REPEAT
#endif /* ALLOW_REPEAT */

#ifdef ALLOW_DEBUG
/* wizard2.c */
void display_light_map(void);
void display_scent_map(void);
void display_noise_map(void);
extern void do_cmd_debug(void);
extern void do_cmd_wiz_unhide(int d);
#endif /* ALLOW_DEBUG */

#ifdef ALLOW_SPOILERS

/* wizard1.c */
extern void do_cmd_spoilers(void);

#endif /* ALLOW_SPOILERS */
extern bool make_fake_artefact(object_type* o_ptr, byte name1);

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
