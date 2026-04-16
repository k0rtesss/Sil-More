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
#include "drop_system.h"
#include "cave/cave-state.h"
#include "fs/pref-files.h"
#include "fs/pref-time.h"
#include "fs/savefile-name.h"
#include "game-event.h"
#include "init/init-data.h"
#include "init/init-paths.h"
#include "level-generation/level-generation.h"
#include "melee/melee.h"
#include "monster/monster-state.h"
#include "object/object-state.h"
#include "object/object-slot.h"
#include "object/object-ui-select.h"
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
#include "ui/ui-character-name.h"
#include "ui/ui-character-screen.h"
#include "ui/colors.h"
#include "ui/ui-death.h"
#include "ui/ui-file-viewer.h"
#include "ui/ui-help.h"
#include "ui/ui-look-sidebar.h"
#include "ui/ui-story.h"
#include "ui/story_font.h"
// extern FILE *log_file;

/* variable.c */
extern s16b object_level;
extern bool use_sound;
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
extern autoinscription* inscriptions;
extern u16b inscriptionsCount;

/*
 * Rage and labyrinth partitions both suppress remembered-grid information.
 * When inactive, remembered information remains available to look/target UI.
 */
#ifndef GRID_INFO_VISIBILITY_HELPERS_DEFINED
#define GRID_INFO_VISIBILITY_HELPERS_DEFINED
static inline bool player_suppresses_unseen_grid_info(void)
{
    return (!p_ptr->is_dead) && (p_ptr->rage || g_labyrinth_view_active);
}

static inline bool grid_info_is_available(int y, int x)
{
    return !player_suppresses_unseen_grid_info() || player_can_see_bold(y, x);
}
#endif

#ifndef GENERATION_DEPTH_HELPERS_DEFINED
#define GENERATION_DEPTH_HELPERS_DEFINED
static inline int generation_depth_for_level(int depth)
{
    if (depth == 0)
        return MORGOTH_DEPTH;
    if (depth < 1)
        return 1;
    return depth;
}

static inline int player_generation_depth(void)
{
    if (!p_ptr)
        return 1;
    return generation_depth_for_level(p_ptr->depth);
}
#endif

/*
 * Automatically generated "function declarations"
 */

/* birth.c */
extern NavResult player_birth(void);
extern NavResult gain_skills(void);
extern NavResult character_creation(void);
extern NavResult blitz_character_creation(void);
void player_wipe(void);

/* cave.c */
extern int distance(int y1, int x1, int y2, int x2);
extern int distance_squared(int y1, int x1, int y2, int x2);
extern bool los(int y1, int x1, int y2, int x2);
extern void random_unseen_floor(int* ry, int* rx);
extern bool no_light(void);
extern bool seen_by_keen_senses(int y, int x);
extern bool cave_valid_bold(int y, int x);
extern bool feat_supports_lighting(int feat);
extern void map_info(int y, int x, byte* ap, char* cp, byte* tap, char* tcp);
extern void map_info_default(int y, int x, byte* ap, char* cp);
extern int player_tile_offset(void);
extern void dungeon_mark_map_for_redraw(void);
extern void dungeon_note_cursor_relative(int y, int x);
extern void dungeon_sync_cursor_state(void);
extern void note_spot(int y, int x);
extern bool build_overhead_subwindow_ui_scene(app_ui_scene* scene);
extern void do_cmd_view_map(void);
extern errr vinfo_init(void);
extern void forget_view(void);
extern void update_view(void);
extern int flow_dist(int which_flow, int y, int x);
extern void update_flow(int cy, int cx, int which_flow);
extern void update_smell(void);
extern void map_feature(int y, int x);
extern void map_area(void);
extern void map_area_radius(int radius);
extern void wiz_light(void);
extern void wiz_dark(void);
extern void gates_illuminate(bool daytime);
extern void cave_set_feat(int y, int x, int feat);
extern void cave_set_feat_with_color(int y, int x, int feat, int color);
extern byte get_depth_color(int depth);
extern void reset_depth_color_cache(void);
/* Style-weight APIs */
extern void styles_init_for_level(void);
extern void styles_begin_vault(int extra_sidx, int extra_weight);
extern void styles_end_vault(void);
extern void styles_reset_level_weights(void);
extern void styles_add_level_weight(int sidx, int weight);
extern void styles_reset_vault_weights(void);
extern void styles_add_vault_weight(int sidx, int weight);
extern void styles_add_vault_from_level(int factor);
extern void styles_set_vault_avoid_style(int sidx);
extern void styles_default_vault_clear(void);
extern void styles_default_vault_add(int sidx_or_star, int weight);
extern void styles_apply_vault_list(const int* sidx, const int* weight, int count);
extern void styles_vault_rules_clear(void);
extern void styles_set_vault_rule(int depth, const int* sidx, const int* weight, int count);
extern void styles_apply_vault_default_for_depth(int depth);
extern void styles_partition_rules_clear(void);
extern void styles_add_partition_rule(int depth, int kind, const int* sidx, const int* weight, int count);
extern int styles_pick_partition_style(int depth, int kind);
extern int styles_get_level_primary_style(void);
extern int styles_get_vault_primary_style(void);
extern void styles_select_vault_primary(void);
extern int styles_pick_random_from_level(void);
extern int styles_decode_color_style(byte color_value);
extern void styles_rules_clear(void);
extern void styles_add_level_rule(int min_depth, int max_depth, const int* sidx, const int* weight, int count);
/* Narrative text: from style.txt (S:/M1:/M2: lines) */
extern const char* styles_get_style_display(int sidx);
extern const char* styles_get_style_short_desc(int sidx);
extern const char* styles_get_style_m1(int sidx);
extern const char* styles_get_style_m2(int sidx);
extern void clear_active_narrative_banner(void);
extern bool dungeon_active_narrative_banner_animating(u64b now_ms);
extern bool dungeon_query_active_narrative_banner(u64b now_ms, char* text,
    size_t text_size, u64b* started_ms, u32b* hold_ms);
extern void styles_reload_messages_from_text(void);
extern void styles_clear_display_messages(void);
extern int p_ptr_depth_proxy(void);
extern void styles_set_loaded_level_primary(int sidx);
/* Persisted door-style variant choices for consistency across save/load */
extern int styles_get_choice_capacity(void);
extern void styles_copy_level_door_choices(byte* out_buf, int max_n);
extern void styles_load_level_door_choices(const byte* in_buf, int n);
extern int project_path(
    u16b* gp, int range, int y1, int x1, int* y2, int* x2, u32b flg);
extern byte projectable(int y1, int x1, int y2, int x2, u32b flg);
extern void scatter(int* yp, int* xp, int y, int x, int d, int m);
extern void health_track(int m_idx);
extern void monster_race_track(int r_idx);
extern void object_kind_track(int k_idx);
extern void disturb(int stop_stealth, int unused_flag);

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
extern void new_wandering_flow(monster_type* m_ptr, int y, int x);
extern void new_wandering_destination(
    monster_type* m_ptr, monster_type* leader_ptr);
extern void drop_iron_crown(monster_type* m_ptr, const char* msg);
extern void perceive(void);
extern int success_chance(int sides, int skill, int difficulty);
extern int light_penalty(const monster_type* m_ptr);
extern bool check_hit(int power, bool display_roll);
extern int hit_roll(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll);
extern int total_player_attack(monster_type* m_ptr, int base);
extern int total_player_evasion(monster_type* m_ptr, bool archery);
extern int total_monster_attack(monster_type* m_ptr, int base);
extern int total_monster_evasion(monster_type* m_ptr, bool archery);
extern int stealth_melee_bonus(const monster_type* m_ptr, bool allow_unseen);
extern int overwhelming_att_mod(monster_type* m_ptr);
extern int crit_bonus(int hit_result, int weight, const monster_race* r_ptr,
    int skill_type, bool thrown, monster_type* attacker,
    const object_type* o_ptr);
extern void ident_on_wield(object_type* o_ptr);
extern void ident_resist(u32b flag);
extern void ident_passive(void);
extern void ident_see_invisible(const monster_type* m_ptr);
extern void ident_haunted(void);
extern void ident_hunger(void);
extern void ident_f2(u32b flag, object_type* supplied_object);
extern void ident_f3(u32b flag, object_type* supplied_object);
extern void ident_weapon_by_use(
    object_type* o_ptr, const monster_type* m_ptr, u32b flag);
extern void ident_bow_arrow_by_use(object_type* j_ptr, object_type* i_ptr,
    object_type* o_ptr, const monster_type* m_ptr, u32b bow_flag,
    u32b arrow_flag);
extern void apply_weapon_combat_effects(object_type* o_ptr,
    monster_type* m_ptr, int skill_type, int net_dam, bool fatal_blow,
    cptr armor_shatter_noun);
extern int slay_bonus(
    const object_type* o_ptr, const monster_type* m_ptr, u32b* noticed_flag);
extern int prt_after_sharpness(const object_type* o_ptr, u32b* noticed_flag);
extern void search(void);
extern void do_cmd_pickup_from_pile(void);
extern void py_pickup_aux(int o_idx);
extern void py_pickup(void);
extern void chest_release_contents(object_type* o_ptr, int y, int x,
    int destroy_typ);
extern bool smith_oath_forbids_object(const object_type* o_ptr);
extern bool smith_oath_confirm_break(void);
extern void hit_trap(int y, int x);
extern void display_hit(
    int y, int x, int net_dam, int dam_type, bool fatal_blow);
extern int concentration_bonus(int y, int x);
extern int focused_attack_bonus(void);
extern int master_hunter_bonus(monster_type* m_ptr);
extern bool knock_back(int y1, int x1, int y2, int x2);
extern bool abort_for_mercy(monster_type* m_ptr);
extern bool abort_for_valorous(monster_type* m_ptr);
extern bool cowardly_attack(monster_type* m_ptr);
extern bool is_aoe_attack_type(int attack_type);
extern void break_mercy_oath(monster_type* m_ptr, int damage);
extern void break_valorous_oath(monster_type* m_ptr, int damage, int attack_type, int damage_source);
extern void attack_punctuation(
    char* punctuation, int net_dam, int crit_bonus_dice);
extern int count_open_adjacent_squares(int y, int x);
extern void py_attack_aux(int y, int x, int attack_type);
extern void py_attack(int y, int x, int attack_type);
extern void flanking_or_retreat(int y, int x);
extern void move_player(int dir);

/* cmd2.c */
extern int min_depth(void);
extern void min_depth_timer_status(int* base_increment, int* additional_increment,
    int* total_increment, int* progress, int* threshold);
extern void note_lost_greater_vault(void);
extern void do_cmd_go_up(void);
extern void do_cmd_go_down(void);
extern void do_cmd_search(void);
extern void do_cmd_toggle_stealth(void);
extern bool do_cmd_open_aux(int y, int x);
extern void do_cmd_open(void);
extern void do_cmd_close(void);
extern void do_cmd_exchange(void);
extern void do_cmd_fletchery(void);
extern void finish_fletching(int);
extern void do_cmd_tunnel(void);
extern bool break_free_of_web(void);
extern void do_cmd_disarm(void);
extern void do_cmd_bash(void);
extern void do_cmd_steal(void);
extern void do_cmd_alter(void);
extern void do_cmd_spike(void);
extern bool do_cmd_walk_test(int y, int x);
extern void do_cmd_walk(void);
extern void do_cmd_jump(void);
extern void do_cmd_run(void);
extern void do_cmd_hold(void);
extern void do_cmd_pickup(void);
extern void do_cmd_rest(void);
extern int archery_range(const object_type* j_ptr);
extern int throwing_range(const object_type* i_ptr);
extern void attacks_of_opportunity(int neutralized_y, int neutralized_x);
extern void do_cmd_fire(int quiver);
extern void do_cmd_throw(bool automatic);
extern void do_cmd_throw_from_slot(int slot);
extern bool throw_slot_menu_active;
extern bool throw_slot_enabled[INVEN_TOTAL];

/* cmd3.c */
extern void do_cmd_use_item_by_index(int item);
extern void do_cmd_use_item(void);
extern void do_cmd_use_item_enhanced(void);
extern void do_cmd_inven(void);
extern void do_cmd_inven_direct(void);
extern void do_cmd_equip(void);
extern void do_cmd_equip_direct(void);
extern void do_cmd_wield(object_type* default_o_ptr, int default_item);
extern void do_cmd_wield_wrapper(void);
extern void do_cmd_wield_enhanced(void);
extern void do_cmd_takeoff(object_type* default_o_ptr, int default_item);
extern void do_cmd_drop_item_by_index(int item);
extern void do_cmd_drop(void);
extern bool open_supplies_menu_with_context(supply_menu_action default_action, int default_group, bool default_focus, bool default_hotkey);
extern void do_cmd_destroy(void);
extern void do_cmd_observe(void);
extern void do_cmd_observe_enhanced(void);
extern void do_cmd_uninscribe(void);
extern void do_cmd_inscribe(void);
extern void do_cmd_refuel_lamp(object_type* default_o_ptr, int default_item);
extern void do_cmd_refuel_torch(
    object_type* default_o_ptr, int default_item, bool is_mallorn);
extern void do_cmd_refuel(void);
extern void do_cmd_target(void);
extern void do_cmd_look(void);
extern void do_cmd_look_at(int y, int x);
extern void do_cmd_unified_look(void);
extern void do_cmd_locate(void);
extern void do_cmd_query_symbol(void);
extern void do_cmd_view_monsters(void);
extern void do_cmd_view_objects(void);
extern void highlight_entity_on_map(int y, int x, bool highlight);
extern void highlight_entity_on_map_type(int y, int x, bool highlight, int entity_type);
extern bool ang_sort_comp_hook(const void* u, const void* v, int a, int b);
extern void ang_sort_swap_hook(void* u, void* v, int a, int b);
extern void py_steal(int y, int x);

/* cmd4.c */
extern void do_cmd_redraw(void);
extern void options_birth_menu(bool adult);
extern void do_cmd_character_sheet(void);
extern void do_cmd_change_song(void);
extern void show_songs_with_highlight(int highlight);
extern void wipe_screen_from(int col);
extern void do_cmd_ability_screen(void);
extern void do_cmd_smithing_screen(void);
extern void do_cmd_main_menu(void);
extern void do_cmd_message_one(void);
extern void do_cmd_messages(void);
extern void do_cmd_options_aux(int page, cptr info);
extern void do_cmd_options(void);
extern void do_cmd_pane_settings(void);
extern void do_cmd_macros(void);
extern void do_cmd_keybinds(void);
extern void do_cmd_visuals(void);
extern void do_cmd_colors(void);
extern void do_cmd_version(void);
extern void do_cmd_feeling(void);
extern void do_cmd_knowledge_notes(void);
extern void do_cmd_knowledge_oaths(void);
extern void do_cmd_knowledge_artefacts(void);
extern void do_cmd_knowledge_monsters(void);
extern bool do_cmd_knowledge_supplies(const supply_menu_request* request);
extern void do_cmd_knowledge_objects(void);
extern void do_cmd_knowledge_kills(void);
#define KNOWLEDGE_PAGE_ARTEFACTS 0
#define KNOWLEDGE_PAGE_OBJECTS 1
#define KNOWLEDGE_PAGE_MONSTERS 2
#define KNOWLEDGE_PAGE_CURSES 3
extern void do_cmd_knowledge_browser_page(int page);
extern void ghost_challenge(void);
extern void desc_art_fake(int a_idx);
extern void apply_magic_fake(object_type* o_ptr);
extern void do_cmd_knowledge(void);
extern void add_random_curse(object_type *o_ptr);

/* cmd5.c */
extern bool build_object_kind_recall_ui_scene(app_ui_scene* scene, int k_idx,
    cptr prompt, bool overlay_dungeon);

/* cmd6.c */
extern void do_cmd_eat_food(object_type* default_o_ptr, int default_item);
extern void do_cmd_quaff_potion(object_type* default_o_ptr, int default_item);
extern void do_cmd_use_gem(object_type* default_o_ptr, int default_item);
extern void do_cmd_activate_staff(object_type* default_o_ptr, int default_item);
extern void do_cmd_play_instrument(
    object_type* default_o_ptr, int default_item);
extern void do_cmd_activate(void);

/* dungeon.c */
extern bool can_be_pseudo_ided(const object_type* o_ptr);
extern int value_check_aux1(const object_type* o_ptr);
extern void land(void);
extern void pseudo_id(object_type* o_ptr);
extern void pseudo_id_everything(void);
extern void id_known_specials(void);
extern void id_everything(void);
extern PlayResult play_game(void);
extern void death_spectator_view(void);
extern bool death_spectator_active(void);
extern void reset_dungeon_state(void);

/* files.c */
extern void safe_setuid_drop(void);
extern void safe_setuid_grab(void);
extern errr file_character(cptr name, bool full);
extern void do_cmd_escape(int);
extern void do_cmd_suicide(void);
extern int meta_write(const metarun*);
extern errr meta_read(metarun*);
extern int meta_seek(int i);
extern int meta_fill(bool);

/* init2.c */
extern void init_file_paths(char* path);
extern void display_introduction(void);
extern void init_angband(void);
extern void autoinscribe_clean(void);
extern void autoinscribe_init(void);
extern void re_init_some_things(void);
extern NavResult initial_menu(bool *start_new);
extern void cleanup_angband(void);

/* load.c */
extern bool load_player(void);
extern bool load_meta(void);

/* monster1.c */
extern void describe_monster(
    int r_idx, bool spoilers, const monster_type* m_ptr);
extern bool build_monster_recall_ui_scene(app_ui_scene* scene, int r_idx,
    const monster_type* m_ptr, cptr prompt, bool overlay_dungeon);

/* monster2.c */
extern s16b poly_r_idx(const monster_type* m_ptr);
extern void delete_monster_idx(int i);
extern void delete_monster(int y, int x);
extern void compact_monsters(int size);
extern void wipe_mon_list(void);
extern s16b mon_pop(void);
extern errr get_mon_num_prep(void);
extern s16b get_mon_num(
    int level, bool special, bool allow_non_smart, bool vault);
extern bool build_monlist_subwindow_ui_scene(app_ui_scene* scene);
extern void lore_probe_aux(int r_idx);
extern void lore_treasure(int m_idx, int num_item);
extern int monster_stat(monster_type* m_ptr, int stat_type);
extern void listen_hint_new_player_turn(void);
extern bool listen_hint_overlay(int m_idx, byte* a, char* c);
extern s16b monster_carry(int m_idx, object_type* j_ptr);
extern int monster_base_armour_sides(const monster_type* m_ptr);
extern int monster_song_hp_loss(const monster_type* m_ptr);
extern void monster_swap(int y1, int x1, int y2, int x2);
extern s16b player_place(int y, int x);
extern s16b monster_place(int y, int x, monster_type* n_ptr);
extern void calc_monster_speed(int y, int x);
extern void set_monster_haste(s16b m_idx, s16b counter, bool message);
extern void set_monster_slow(s16b m_idx, s16b counter, bool message);
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
extern bool place_monster_one(
    int y, int x, int r_idx, bool slp, bool ingnore_depth, monster_type* m_ptr);
extern bool place_monster_aux(int y, int x, int r_idx, bool slp, bool grp);
extern bool place_monster(int y, int x, bool slp, bool grp, bool vault);
extern bool quest_monster_spawn_okay(int r_idx);
extern bool alloc_monster(bool on_stairs, bool force_undead);
extern bool summon_specific(int y1, int x1, int lev, int type);
extern bool reproduce_monster(int old_m_idx, int new_r_idx);
extern void message_pain(int m_idx, int dam);

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
extern bool is_smithed_by_player(const object_type* o_ptr);
extern bool upgrade_broken_item(int slot);
extern bool reveal_random_artifact(void);
extern bool elemental_attack_destroys_object(int attack_type,
    const object_type* o_ptr);
extern void sound_dam(int raw_dam, int min_raw, int max_raw, int hp_dam);

extern bool make_object(
    object_type* j_ptr, drop_quality quality, int objecttype);
extern bool make_object_with_profile(object_type* j_ptr, drop_quality quality,
    int objecttype, const drop_profile* profile);
extern bool make_guaranteed_artefact(
    object_type* j_ptr, drop_quality quality, int objecttype);
extern bool make_guaranteed_artefact_with_profile(object_type* j_ptr,
    drop_quality quality, int objecttype, const drop_profile* profile);
extern bool prep_object_theme(int themetype);
extern s16b floor_carry(int y, int x, object_type* j_ptr);
extern s16b drop_near(object_type* j_ptr, int chance, int y, int x);
extern void acquirement(int y1, int x1, int num, drop_quality quality);
extern void place_object(int y, int x, drop_quality quality, int droptype,
    bool allow_artefacts);
extern void place_trap(int y, int x);
extern void place_random_door(int y, int x);
extern void place_forge(int y, int x);
extern void inven_item_charges(int item);
extern void inven_item_describe(int item);
extern void inven_item_increase(int item, int num);
extern void inven_item_optimize(int item);
extern void floor_item_charges(int item);
extern void floor_item_describe(int item);
extern void floor_item_increase(int item, int num);
extern void floor_item_optimize(int item);
extern void check_pack_overflow(void);
extern bool inven_carry_okay(const object_type* o_ptr);
extern bool inven_carry_okay_after_removing(
    const object_type* o_ptr, int remove_item, int remove_amt);
extern bool inven_carry_limit_failed(void);
extern cptr inven_carry_limit_label(void);
extern int inven_carry_limit_value(void);
extern bool inven_carry_limit_can_replace(const object_type* o_ptr);
extern int object_stack_limit(const object_type* o_ptr);
extern s16b inven_carry(object_type* o_ptr, bool combine_ammo);
extern s16b inven_takeoff(int item, int amt);
extern void inven_drop(int item, int amt);
extern void inven_enforce_current_pack_limits(void);
extern void combine_pack(void);
extern void reorder_pack(bool display_message);
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

/* squelch.c */
extern byte squelch_level[SQUELCH_BYTES];
extern int do_cmd_autoinscribe_item(s16b k_idx);
extern void do_cmd_squelch_autoinsc(void);
extern int squelch_itemp(object_type* o_ptr, byte feeling, bool fullid);
extern int do_squelch_item(int squelch, int item, object_type* o_ptr);
extern void rearrange_stack(int y, int x);
extern void do_squelch_pile(int y, int x);
extern int get_autoinscription_index(s16b k_idx);
extern void obliterate_autoinscription(s16b kind);
extern void autoinscribe_ground(void);
extern void autoinscribe_pack(void);
extern int remove_autoinscription(s16b kind);
extern int add_autoinscription(s16b kind, cptr inscription);
extern int apply_autoinscription(object_type* o_ptr);
extern char* squelch_to_label(int squelch);

/*use-obj.c*/
extern int consumable_healing_points(const object_type* o_ptr);
extern bool use_object(object_type* o_ptr, bool* ident);
extern bool use_sanctity_gem_on(object_type* target_o_ptr, bool* ident);

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
extern void input_clear_pending(void);
extern errr input_byte_unshift(int key);
extern errr input_byte_enqueue(int key);
extern void input_byte_queue_clear(void);
extern bool input_byte_queue_pending(void);
extern bool inkey_cursor_hidden(void);
extern void inkey_set_cursor_hidden(bool hidden);
extern bool inkey_can_consume_immediately(void);
extern bool input_submit_movement_command(
    const app_movement_command* command);
extern void input_clear_movement_commands(void);
extern bool input_wait_for_movement_or_legacy(u16b context, u16b wait_reason,
    app_movement_command* out_command, char* out_ch);
extern s16b quark_add(cptr str);
extern bool parse_u64b_hex(const char* text, u64b* out);
extern s16b message_num(void);
extern cptr message_str(s16b age);
extern u16b message_type(s16b age);
extern byte message_color(s16b age);
extern errr message_color_define(u16b type, byte color);
extern void message_add(cptr str, u16b type);
extern bool build_message_subwindow_ui_scene(app_ui_scene* scene);
extern bool message_topline_snapshot(char* out_text, size_t out_text_size,
    byte* out_color, u16b* out_type, bool* out_more_pending);
extern void message_topline_override(byte color, cptr text);
extern void message_topline_clear_override(void);
extern errr messages_init(void);
extern void messages_free(void);
extern void move_cursor(int row, int col);
extern void msg_print(cptr msg);
extern void msg_format(cptr fmt, ...);
extern void msg_debug(cptr fmt, ...);
extern void message(u16b message_type, s16b extra, cptr message);
extern void message_format(u16b message_type, s16b extra, cptr fmt, ...);
extern void message_flush(void);
extern void text_out_to_file(byte attr, cptr str);
extern bool prompt_text_input(cptr prompt, cptr detail, char* buf, size_t len,
    bool allow_randomize);
extern bool askfor_aux(char* buf, size_t len);
extern bool askfor_name(char* buf, size_t len);
extern s16b get_quantity(cptr prompt, int max);
extern int get_check_other(cptr prompt, char other);
extern bool get_check(cptr prompt);
extern bool get_check_oath_multiline(cptr prompt);
extern int get_menu_choice(s16b max, char* prompt);
extern bool get_com(cptr prompt, char* command);
extern bool preconfirm_enter_morgoth_hall(void);
extern void pause_line(int row);
extern int int_exp(int base, int power);
extern int color_char_to_attr(char c);
extern int color_text_to_attr(cptr name);

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
extern void cnv_stat(int val, char* out_val);
extern int health_level(int current, int max);
extern bool get_alertness_text(
    monster_type* m_ptr, int text_size, char* text, int* color);
extern byte health_attr(int current, int max);
extern byte object_display_color(const object_type* o_ptr, byte base_color);

/* xtra2.c */
extern bool saving_throw(monster_type* m_ptr, int resistance);
extern bool turin_resist_bad_effect(void);
extern bool allow_player_blind(monster_type* m_ptr);
extern bool set_blind(int v);
extern bool allow_player_confusion(monster_type* m_ptr);
extern bool set_confused(int v);
extern bool set_poisoned(int v);
extern bool allow_player_fear(monster_type* m_ptr);
extern bool set_afraid(int v);
extern bool allow_player_entrancement(monster_type* m_ptr);
extern bool set_entranced(int v);
extern bool allow_player_image(monster_type* m_ptr);
extern bool set_image(int v);
extern bool set_fast(int v);
extern bool allow_player_slow(monster_type* m_ptr);
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
extern void falling_damage(bool stun);
extern void create_chosen_artefact(byte name1, int y, int x, bool identify);
extern int drop_loot(monster_type* m_ptr);
extern void anger_morgoth(int level);
extern void ang_sort_aux(void* u, void* v, int p, int q);
extern void ang_sort(void* u, void* v, int n);
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
extern void repeat_push(int what);
extern bool repeat_pull(int* what);
extern void repeat_clear(void);
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
