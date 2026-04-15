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
#include "fs/pref-files.h"
#include "fs/pref-time.h"
#include "fs/savefile-name.h"
#include "game-event.h"
#include "level-generation/level-generation.h"
#include "melee/melee.h"
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

typedef struct app_ui_scene app_ui_scene;
#include "player/weapon_stats.h"
#include "quest/quest.h"
#include "runtime/runtime-game.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_ui.h"
#include "signals.h"
#include "smithing/smithing.h"
#include "spell/spell.h"
#include "ui/ui-character-name.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-death.h"
#include "ui/ui-file-viewer.h"
#include "ui/ui-help.h"
#include "ui/ui-look-sidebar.h"
#include "ui/ui-story.h"
#include "ui/story_font.h"
// extern FILE *log_file;
extern int max_macrotrigger;
extern cptr macro_template;
extern cptr macro_modifier_chr;
extern cptr macro_modifier_name[MAX_MACRO_MOD];
extern cptr macro_trigger_name[MAX_MACRO_TRIGGER];
extern cptr macro_trigger_keycode[2][MAX_MACRO_TRIGGER];

/* tables.c */
extern const s16b ddd[9];
extern const s16b ddx[10];
extern const s16b ddy[10];
extern const s16b ddx_ddd[9];
extern const s16b ddy_ddd[9];
extern const char hexsym[16];
extern const byte extract_energy[8];
extern const byte chest_traps[25 + 1];
extern cptr color_names[16];
extern cptr stat_names[A_MAX];
extern cptr stat_names_reduced[A_MAX];
extern cptr stat_names_full[A_MAX];
extern cptr skill_names[S_MAX];
extern cptr skill_names_full[S_MAX];
extern cptr window_flag_desc[32];
extern cptr option_text[OPT_MAX];
extern cptr option_desc[OPT_MAX];
extern const bool option_norm[OPT_MAX];
extern const byte option_page[OPT_PAGE_MAX][OPT_PAGE_PER];
extern cptr inscrip_text[MAX_INSCRIP];
extern byte spell_info_RF4[32][3];
extern byte spell_desire_RF4[32][2];

/* variable.c */
extern cptr copyright;
extern byte version_major;
extern byte version_minor;
extern byte version_patch;
extern byte version_extra;
extern byte sf_major;
extern byte sf_minor;
extern byte sf_patch;
extern byte sf_extra;
extern u32b sf_xtra;
extern u32b sf_when;
extern u16b sf_lives;
extern u16b sf_saves;
extern bool character_generated;
extern bool character_dungeon;
extern bool character_loaded;
extern bool character_loaded_dead;
extern bool character_saved;
extern s16b character_icky;
extern s16b character_xtra;
extern u32b seed_randart;
extern u32b seed_flavor;
extern s16b num_repro;
extern s16b object_level;
extern s16b monster_level;
extern char summon_kin_type;
extern s32b turn;
extern s32b playerturn;
extern s32b min_depth_counter;
extern bool use_sound;
extern int use_graphics;
extern s16b image_count;
extern bool use_bigtile;
extern s16b signal_count;
extern bool msg_flag;
extern byte object_generation_mode;
extern bool shimmer_monsters;
extern bool shimmer_objects;
extern bool repair_mflag_mark;
extern bool repair_mflag_show;
extern s16b o_max;
extern s16b o_cnt;
extern s16b mon_max;
extern s16b mon_cnt;
extern byte feeling;
extern byte do_feeling;
extern s16b rating;
extern bool good_item_flag;
extern int closing_flag;
extern int player_uid;
extern int player_euid;
extern int player_egid;
extern char savefile[1024];
extern s16b macro__num;
extern cptr* macro__pat;
extern cptr* macro__act;
extern byte angband_color_table[256][4];
extern const cptr angband_sound_name[MSG_MAX];
extern int view_n;
extern u16b* view_g;
extern int temp_n;
extern u16b* temp_g;
extern byte* temp_y;
extern byte* temp_x;
extern u16b (*cave_info)[256];
extern byte (*cave_feat)[MAX_DUNGEON_WID];
extern byte (*cave_color)[MAX_DUNGEON_WID];
extern s16b (*cave_light)[MAX_DUNGEON_WID];
extern s16b (*cave_o_idx)[MAX_DUNGEON_WID];
extern s16b (*cave_m_idx)[MAX_DUNGEON_WID];
extern u32b mon_power_ave[MAX_DEPTH][CREATURE_TYPE_MAX];

extern byte cave_cost[MAX_FLOWS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
extern byte (*cave_when)[MAX_DUNGEON_WID];
extern int scent_when;
extern byte flow_center_y[MAX_FLOWS];
extern byte flow_center_x[MAX_FLOWS];
extern byte update_center_y[MAX_FLOWS];
extern byte update_center_x[MAX_FLOWS];
extern s16b wandering_pause[MAX_FLOWS];

/* Public style color encoding base for save/load */
#ifndef COLOR_STYLE_BASE
#define COLOR_STYLE_BASE 128
#endif

extern s16b stealth_score;
extern bool player_attacked;
extern bool attacked_player;
extern maxima* z_info;
extern object_type* o_list;
extern monster_type* mon_list;
extern monster_lore* l_list;
extern object_type* inventory;
extern s16b alloc_kind_size;
extern alloc_entry* alloc_kind_table;
extern s16b alloc_ego_size;
extern alloc_entry* alloc_ego_table;
extern s16b alloc_race_size;
extern alloc_entry* alloc_race_table;
extern byte misc_to_attr[256];
extern char misc_to_char[256];
extern byte tval_to_attr[128];
extern char macro_buffer[1024];
extern cptr keymap_act[KEYMAP_MODES][256];
extern const player_race* rp_ptr;
extern character_profile* current_character_profile;
extern player_other* op_ptr;
extern player_type* p_ptr;
extern vault_type* v_info;
extern char* v_name;
extern char* v_text;
extern feature_type* f_info;
extern char* f_name;
extern char* f_text;
extern object_kind* k_info;
extern char* k_name;
extern char* k_text;
extern ability_type* b_info;
extern char* b_name;
extern char* b_text;
extern artefact_type* a_info;
extern char* a_text;
extern bool* valar_reserved_artifacts;
extern ego_item_type* e_info;
extern char* e_name;
extern char* e_text;
extern monster_race* r_info;
extern monster_race* r_base;
extern char* r_name;
extern char* r_text;
extern player_race* p_info;
extern char* p_name;
extern char* p_text;
extern character_profile* c_info;
extern char* c_name;
extern char* c_text;
extern hist_type* h_info;
extern story_type* st_info;
extern char* st_text;
extern char* st_name;
extern curse_type* cu_info;
extern char* cu_text;
extern char* cu_name;
extern major_blessing_type* mb_info;
extern char* mb_text;
extern char* mb_name;
extern quest_type* quest_info;
extern char* quest_name_text;
extern char* quest_desc_text;
extern char* q_text;
extern oath_type* oath_info;
extern char* oath_name_text;
extern char* oath_desc_text;
extern char* h_text;
extern flavor_type* flavor_info;
extern char* flavor_name;
extern char* flavor_text;
extern names_type* n_info;
extern style_type* style_info;
extern skeleton_note_template* skeleton_note_info;
extern char* skeleton_note_text;
/* Default vein tile accessors (defined in init1.c) */
byte get_default_vein_row(void);
byte get_default_vein_col(void);
bool get_overlay_key_enabled(void);
void get_overlay_key_rgb(byte* r, byte* g, byte* b);
extern char* style_name;

extern cptr ANGBAND_SYS;
extern cptr ANGBAND_GRAF;
extern cptr ANGBAND_DIR;
extern cptr ANGBAND_DIR_APEX;
extern cptr ANGBAND_DIR_METARUN;
extern cptr ANGBAND_DIR_BONE;
extern cptr ANGBAND_DIR_DATA;
extern cptr ANGBAND_DIR_EDIT;
extern cptr ANGBAND_DIR_FILE;
extern cptr ANGBAND_DIR_HELP;
extern cptr ANGBAND_DIR_INFO;
extern cptr ANGBAND_DIR_SAVE;
extern cptr ANGBAND_DIR_PREF;
extern cptr ANGBAND_DIR_USER;
extern cptr ANGBAND_DIR_XTRA;
extern cptr ANGBAND_DIR_SCRIPT;
extern bool (*ang_sort_comp)(const void* u, const void* v, int a, int b);
extern void (*ang_sort_swap)(void* u, void* v, int a, int b);
extern bool (*get_mon_num_hook)(int r_idx);
extern bool (*get_obj_num_hook)(int k_idx);
extern void (*object_info_out_flags)(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
extern ang_file* text_out_file;
extern void (*text_out_hook)(byte a, cptr str);
extern int text_out_wrap;
extern int text_out_indent;
extern bool use_transparency;
extern char notes_buffer[NOTES_LENGTH];
extern autoinscription* inscriptions;
extern u16b inscriptionsCount;
extern byte bones_selector;
extern int r_ghost;
extern char ghost_name[80];
extern bool g_labyrinth_view_active;
extern bool stop_stealth_mode;

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
/* After showing the per-style banner on level entry, count down user inputs
 * and force a full screen redraw when it reaches zero. */
extern int g_banner_force_redraw_remaining;
extern void clear_active_narrative_banner(void);
extern bool dungeon_append_active_partition_banner_ui_scene(app_ui_scene* scene);
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
extern bool graphics_are_ascii(void);
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
extern void make_alert(monster_type* m_ptr);
extern void set_alertness(monster_type* m_ptr, int alertness);
extern void perceive(void);
extern int success_chance(int sides, int skill, int difficulty);
extern int skill_check(
    monster_type* m_ptr1, int skill, int difficulty, monster_type* m_ptr2);
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
extern void ident(object_type* o_ptr);
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
extern const byte cycle[];
extern const byte chome[];
extern void run_step(int dir);

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
extern void do_cmd_note(char* note, int what_depth);
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
extern void monster_desc(
    char* desc, size_t max, const monster_type* m_ptr, int mode);
extern void monster_desc_race(char* desc, size_t max, int r_idx);
extern void lore_probe_aux(int r_idx);
extern void lore_treasure(int m_idx, int num_item);
extern int monster_skill(monster_type* m_ptr, int skill_type);
extern int monster_stat(monster_type* m_ptr, int stat_type);
extern void update_mon(int m_idx, bool full);
extern void update_monsters(bool full);
extern bool detect_monster_noise(monster_type* m_ptr, int skill);
extern void listen_hint_new_player_turn(void);
extern bool listen_hint_overlay(int m_idx, byte* a, char* c);
extern s16b monster_carry(int m_idx, object_type* j_ptr);
extern int monster_base_armour_sides(const monster_type* m_ptr);
extern int monster_song_hp_loss(const monster_type* m_ptr);
extern void monster_add_song_hp_loss(monster_type* m_ptr, int amount);
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

/* obj-info.c */
extern bool object_info_out(const object_type* o_ptr);
extern void note_info_screen(const object_type* o_ptr);
extern void object_info_screen(const object_type* o_ptr);
extern void object_info_screen_multi(const object_type** objects, const char** headings, int count);

/* object1.c */
extern bool easter_time(void);
extern void flavor_init(void);
extern void reset_visuals(bool prefs);
extern void object_flags(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
extern void object_flags4(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4);
extern void object_flags_known(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);
extern void object_flags_known4(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4);
extern bool object_grants_ability(
    const object_type* o_ptr, int skilltype, int abilitynum);
extern void strip_name(char* buf, int k_idx);
extern void object_desc(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
extern void object_desc_floor(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
extern void object_desc_spoil(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode);
extern void identify_random_gen(const object_type* o_ptr);
extern byte object_attr_graphics_override(
    const object_type* o_ptr, byte base_attr);
extern char object_char_graphics_override(
    const object_type* o_ptr, char base_char);
extern void inventory_menu_set_include_equip(bool include);
extern bool player_can_treat_as_throwing(const object_type* o_ptr);
extern bool player_can_treat_as_throwing_flags(const object_type* o_ptr, u32b f3);
extern bool weapon_is_impale_eligible(const object_type* o_ptr);
extern int get_paired_artefact(int art_idx);

/* object2.c */
extern void excise_object_idx(int o_idx);
extern void delete_object_idx(int o_idx);
extern void delete_object(int y, int x);
extern void compact_objects(int size);
extern void wipe_o_list(void);
extern s16b o_pop(void);
extern object_type* get_first_object(int y, int x);
extern object_type* get_next_object(const object_type* o_ptr);
extern void get_obj_num_prep(void);
extern s16b get_obj_num(int level);
extern void object_known(object_type* o_ptr);
extern void object_aware(object_type* o_ptr);
extern void object_tried(object_type* o_ptr);
extern bool object_has_ego_flag4(const object_type* o_ptr, u32b flag);
extern s32b object_value(const object_type* o_ptr);
extern bool object_similar(const object_type* o_ptr, const object_type* j_ptr);
extern void object_absorb(object_type* o_ptr, object_type* j_ptr);
extern s16b lookup_kind(int tval, int sval);
extern void object_wipe(object_type* o_ptr);
extern void object_copy(object_type* o_ptr, const object_type* j_ptr);
extern void object_prep(object_type* o_ptr, int k_idx);
extern void object_refresh_weight(object_type* o_ptr);
extern void object_into_artefact(object_type* o_ptr, artefact_type* a_ptr);
extern u32b object_kind_pval_flags1(const object_kind* k_ptr);
extern u32b artefact_pval_flags1(const artefact_type* a_ptr);
extern u32b ego_item_pval_flags1(const ego_item_type* e_ptr);
extern u32b object_pval_flags1(const object_type* o_ptr);
extern void object_apply_pval_delta_with_mask(object_type* o_ptr, u32b mask, int delta);
extern bool object_apply_ego_affix(object_type* o_ptr, int e_idx, bool smithing);
extern bool object_break_brass_lantern(object_type* o_ptr);
extern bool object_is_fire_broken(const object_type* o_ptr);
extern bool object_break_shafted_weapon_by_fire(object_type* o_ptr);
extern bool object_repair_fire_broken_weapon(object_type* o_ptr);
extern void object_into_special(object_type* o_ptr, int lev, bool smithing);
extern void check_artifact_visibility(void);
extern void apply_magic(object_type* o_ptr, int lev, bool okay, bool good,
    bool great, bool allow_insta);
/* thrall_quest.c */
extern bool is_alert_thrall(monster_type* m_ptr);
extern void init_thrall_quest(monster_type* m_ptr);
extern cptr get_thrall_quest_item_name(byte quest_item);
extern int player_has_thrall_quest_item(byte quest_item);
extern bool handle_thrall_interaction(monster_type* m_ptr);
extern void complete_thrall_quest(monster_type* m_ptr, int item_slot);
extern bool object_is_damaged_item(const object_type* o_ptr);
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
extern void reveal_trap(int y, int x);
extern void place_secret_door(int y, int x);
extern void place_closed_door(int y, int x);

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

/* SDL3-based file I/O operations */
extern errr sdl_fclose(ang_file* stream);
extern errr sdl_fgets(ang_file* stream, char* buf, size_t n);
extern errr sdl_fputs(ang_file* stream, cptr buf, size_t n);
extern errr sdl_read(ang_file* stream, char* buf, size_t n);
extern errr sdl_write(ang_file* stream, cptr buf, size_t n);
extern errr sdl_seek(ang_file* stream, ang_file_off_t offset);
extern ang_file_off_t sdl_tell(ang_file* stream);
extern ang_file_off_t sdl_size(ang_file* stream);

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
extern void bell(cptr reason);
extern void sound(int val);
extern s16b quark_add(cptr str);
extern cptr quark_str(s16b i);
extern bool parse_u64b_hex(const char* text, u64b* out);
extern errr quarks_init(void);
extern errr quarks_free(void);
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
extern int count_wrapped_lines(cptr str, int wrap_width, int indent);
extern void text_out(cptr str);
extern void text_out_c(byte a, cptr str);
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
extern int damroll(int num, int sides);
extern bool is_a_vowel(int ch);
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
extern void notice_stuff(void);
extern void update_stuff(void);
extern void redraw_stuff(void);
extern void window_stuff(void);
extern void handle_stuff(void);
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
extern void check_experience(void);
extern s32b adjusted_mon_exp(const monster_race* r_ptr, bool kill);
extern void gain_exp(s32b amount);
extern void lose_exp(s32b amount);
extern void scare_onlooking_friends(const monster_type* m_ptr, int amount);
extern void create_chosen_artefact(byte name1, int y, int x, bool identify);
extern int drop_loot(monster_type* m_ptr);
extern void anger_morgoth(int level);
extern void maybe_update_morgoth_state_from_hp(monster_type* m_ptr);
extern void monster_death(int m_idx);
extern bool mon_take_hit(int m_idx, int dam, cptr note, int who);
extern bool modify_panel(int wy, int wx);
extern bool adjust_panel(int y, int x);
extern bool change_panel(int dir);
extern void verify_panel(void);
extern void ang_sort_aux(void* u, void* v, int p, int q);
extern void ang_sort(void* u, void* v, int n);
extern int motion_dir(int y1, int x1, int y2, int x2);
extern int target_dir(char ch);
extern bool target_able(int m_idx);
extern bool target_okay(int range);
extern bool target_sighted(void);
extern void target_set_monster(int m_idx);
extern void target_set_location(int y, int x);
extern void get_sorted_target_list(int mode, int range);
extern bool target_set_interactive(int mode, int range);
extern int dir_from_delta(int deltay, int deltax);
extern int rough_direction(int y1, int x1, int y2, int x2);
extern bool get_aim_dir(int* dp, int range);
extern bool get_rep_dir(int* dp);
extern bool confuse_dir(int* dp);
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

extern bool g_hide_left_panel;
