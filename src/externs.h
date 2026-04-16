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
#include "fs/pref-time.h"
#include "fs/savefile-name.h"
#include "game-event.h"
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
#include "signals.h"
#include "smithing/smithing.h"
#include "spell/spell.h"
#include "support/macro-state.h"
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
extern s16b object_level;
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
/* Persisted door-style variant choices for consistency across save/load */
extern byte projectable(int y1, int x1, int y2, int x2, u32b flg);
extern void scatter(int* yp, int* xp, int y, int x, int d, int m);

/* cmd1.c */
extern void apply_oath_breaking_curse(int oath_type);
extern void give_player_item(object_type * o_ptr);
extern bool player_auto_identifies_object(const object_type* o_ptr);
extern void player_mark_object_experienced(object_type* o_ptr);
extern bool player_try_identify_smithing_object(
    object_type* o_ptr, bool is_equipped, int bonus);
extern bool player_try_identify_smithing_object_on_examine(
    object_type* o_ptr, bool is_equipped);
extern bool player_auto_identify_smithing_object(
    object_type* o_ptr, bool ignore_distance_penalty);
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
extern void do_cmd_macros(void);
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
extern void apply_magic_fake(object_type* o_ptr);
extern void do_cmd_knowledge(void);
extern void add_random_curse(object_type *o_ptr);

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

/* monster1.c */
extern void describe_monster(
    int r_idx, bool spoilers, const monster_type* m_ptr);

/* monster2.c */
extern s16b poly_r_idx(const monster_type* m_ptr);
extern void delete_monster(int y, int x);
extern void compact_monsters(int size);
extern void wipe_mon_list(void);
extern s16b mon_pop(void);
extern errr get_mon_num_prep(void);
extern s16b get_mon_num(
    int level, bool special, bool allow_non_smart, bool vault);
extern void lore_probe_aux(int r_idx);
extern void listen_hint_new_player_turn(void);
extern bool listen_hint_overlay(int m_idx, byte* a, char* c);
extern s16b monster_carry(int m_idx, object_type* j_ptr);
extern int monster_song_hp_loss(const monster_type* m_ptr);
extern void monster_swap(int y1, int x1, int y2, int x2);
extern s16b player_place(int y, int x);
extern s16b monster_place(int y, int x, monster_type* n_ptr);
extern void calc_monster_speed(int y, int x);
extern void set_monster_haste(s16b m_idx, s16b counter, bool message);
extern void produce_cloud(monster_type* m_ptr);
extern s16b monster_lookup_guid(u64b guid);
extern s16b monster_lookup_guid_text(const char* text);
extern bool place_monster_by_guid(
    int y, int x, u64b guid, bool slp, bool ignore_depth, monster_type* summoner);
extern void monster_special_vault_debug_context(
    int* build_vault_type, bool* exact_token);
extern void log_live_special_vault_only_monsters(const char* reason);
extern bool monster_special_vault_selection_allowed(void);
extern bool monster_special_vault_only_allowed_at(int y, int x);
extern bool place_monster_aux(int y, int x, int r_idx, bool slp, bool grp);
extern bool place_monster(int y, int x, bool slp, bool grp, bool vault);
extern bool quest_monster_spawn_okay(int r_idx);
extern bool alloc_monster(bool on_stairs, bool force_undead);
extern bool reproduce_monster(int old_m_idx, int new_r_idx);

/* thrall_quest.c */
extern bool is_alert_thrall(monster_type* m_ptr);
extern void init_thrall_quest(monster_type* m_ptr);
extern cptr get_thrall_quest_item_name(byte quest_item);
extern int player_has_thrall_quest_item(byte quest_item);
extern bool handle_thrall_interaction(monster_type* m_ptr);
extern void complete_thrall_quest(monster_type* m_ptr, int item_slot);
extern bool object_is_damaged_item(const object_type* o_ptr);
extern bool object_can_repair_damage(const object_type* o_ptr);
extern int find_broken_item_to_upgrade(void);
extern bool repair_damaged_item(int slot);
extern bool upgrade_broken_item(int slot);
extern bool reveal_random_artifact(void);
extern bool elemental_attack_destroys_object(int attack_type,
    const object_type* o_ptr);
extern void sound_dam(int raw_dam, int min_raw, int max_raw, int hp_dam);

extern bool prep_object_theme(int themetype);
extern void acquirement(int y1, int x1, int num, drop_quality quality);
extern void place_object(int y, int x, drop_quality quality, int droptype,
    bool allow_artefacts);
extern void place_trap(int y, int x);
extern void place_random_door(int y, int x);
extern void place_forge(int y, int x);
extern void steal_object_from_monster(int y, int x);

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

/* Legacy - still used by some systems */
extern errr check_modification_date(int fd, cptr template_file);
extern errr check_modification_date_sdl(cptr raw_path, cptr txt_path);

extern void text_to_ascii(char* buf, size_t len, cptr str);
extern void ascii_to_text(char* buf, size_t len, cptr str);
extern int macro_find_exact(cptr pat);
extern errr macro_add(cptr pat, cptr act);
extern errr macro_init(void);
extern errr macro_free(void);
extern errr macro_trigger_free(void);
extern errr input_byte_enqueue(int key);
extern bool inkey_can_consume_immediately(void);
extern bool input_submit_movement_command(
    const app_movement_command* command);
extern void input_clear_movement_commands(void);
extern bool input_wait_for_movement_or_legacy(u16b context, u16b wait_reason,
    app_movement_command* out_command, char* out_ch);
extern u16b message_type(s16b age);
extern errr message_color_define(u16b type, byte color);
extern void message_add(cptr str, u16b type);
extern bool message_topline_snapshot(char* out_text, size_t out_text_size,
    byte* out_color, u16b* out_type, bool* out_more_pending);
extern void move_cursor(int row, int col);
extern void msg_debug(cptr fmt, ...);
extern void text_out_to_file(byte attr, cptr str);
extern int get_menu_choice(s16b max, char* prompt);
extern void pause_line(int row);
extern int int_exp(int base, int power);

#ifdef SUPPORT_GAMMA
extern void build_gamma_table(int gamma);
extern byte gamma_table[256];
#endif /* SUPPORT_GAMMA */

extern byte get_angle_to_grid[41][41];
extern int get_angle_to_target(int y0, int x0, int y1, int x1, int dir);
extern void get_grid_using_angle(int angle, int y0, int x0, int* ty, int* tx);
extern void editing_buffer_init(
    editing_buffer* eb_ptr, const char* buf, size_t max_size);
extern void editing_buffer_destroy(editing_buffer* eb_ptr);
extern int editing_buffer_put_chr(editing_buffer* eb_ptr, char ch);
extern int editing_buffer_set_position(editing_buffer* eb_ptr, size_t new_pos);
extern int editing_buffer_delete(editing_buffer* eb_ptr);
extern void editing_buffer_clear(editing_buffer* eb_ptr);
extern void editing_buffer_get_all(
    editing_buffer* eb_ptr, char buf[], size_t max_size);
extern int editing_buffer_put_str(
    editing_buffer* eb_ptr, const char* str, int n);
extern cptr get_ext_color_name(byte ext_color);

/* xtra1.c */
extern byte object_display_color(const object_type* o_ptr, byte base_color);

/* xtra2.c */
extern bool set_blind(int v);
extern bool allow_player_confusion(monster_type* m_ptr);
extern bool set_confused(int v);
extern bool set_poisoned(int v);
extern bool set_afraid(int v);
extern bool allow_player_entrancement(monster_type* m_ptr);
extern bool set_entranced(int v);
extern bool allow_player_image(monster_type* m_ptr);
extern bool set_image(int v);
extern bool set_fast(int v);
extern bool set_slow(int v);
extern bool set_shield(int v);
extern bool set_blessed(int v);
extern bool set_hero(int v);
extern bool set_rage(int v);
extern bool set_tmp_str(int v);
extern bool set_tmp_dex(int v);
extern bool set_tmp_con(int v);
extern bool set_tmp_gra(int v);
extern bool set_protevil(int v);
extern bool set_tmp_per(int v);
extern bool set_tim_invis(int v);
extern bool set_darkened(int v);
extern bool set_oppose_fire(int v);
extern bool set_oppose_cold(int v);
extern bool set_oppose_pois(int v);
extern bool allow_player_stun(monster_type* m_ptr);
extern bool set_stun(int v);
extern bool set_cut(int v);
extern bool set_food(int v);
extern int drop_loot(monster_type* m_ptr);
/*
 * Hack -- conditional (or "bizarre") externs
 */

#ifdef SET_UID
#ifndef HAVE_USLEEP
/* util.c */
extern int usleep(unsigned long usecs);
#endif /* HAVE_USLEEP */
extern void user_name(char* buf, size_t len, int id);
#endif /* SET_UID */

#ifdef ALLOW_REPEAT
extern void repeat_check(void);
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
