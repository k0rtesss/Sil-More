/* Runs after check_new_monsters' isolated template/engine initialization. */
#include "angband.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-environment.h"
#include "achievements.h"

extern void cave_feature_visual(const feature_type*, byte*, char*);
extern void update_labyrinth_view_state(bool);

extern int fixture_liquid(int y, int x);
extern int fixture_fixture(int y, int x);
extern bool fixture_material(int y, int x);
extern bool fixture_chasm(int y, int x);
extern bool fixture_focus(int y, int x);

static int failures, checks, map_opened, map_blocked;
static int scenario;
static const char* scenario_name;
static u64b saved_rng;

#define CHECK(condition) do { checks++; if (!(condition)) { \
    fprintf(stderr, "%s line %d: %s\n", scenario_name, __LINE__, #condition); \
    failures++; } } while (0)

void __wrap_msg_print(cptr text)
{
    if (text && strstr(text, "labyrinth confounds")) map_blocked++;
}

/* Exercise real view, monster, lore and upkeep; omit only pane painting. */
void __wrap_handle_stuff(void)
{
    update_stuff();
    p_ptr->redraw = p_ptr->window = 0;
}

char __wrap_inkey(void) { return ESCAPE; }
void __wrap_sdl_minimap_begin(void) {}
void __wrap_sdl_minimap_end(void) {}
bool __wrap_sdl_display_pixel_map(int* cy, int* cx)
{
    (void)cy; (void)cx;
    map_opened++;
    return false; /* Use the production terminal overview. */
}

static void check_terrain_at(int y, int x)
{
    byte a, ta, none_a;
    char c, tc, none_c;
    cave_feature_visual(&f_info[FEAT_NONE], &none_a, &none_c);
    map_info(y, x, &a, &c, &ta, &tc);
    CHECK(ta != none_a || tc != none_c);
    CHECK(grid_info_is_available(y, x));
}

static bool target_list_contains(int mode, int y, int x)
{
    get_sorted_target_list(mode, 0);
    for (int i = 0; i < temp_n; i++)
        if (temp_y[i] == y && temp_x[i] == x) return true;
    return false;
}

void __wrap_request_command(void)
{
    CHECK(death_spectator_active());
    CHECK(!p_ptr->is_dead && p_ptr->playing && !p_ptr->leaving);
    CHECK(character_icky == 0);
    CHECK(!sil_achievements_eligible());
    update_labyrinth_view_state(false);
    CHECK(!player_can_see_bold(24, 25)); /* Remote, behind a solid wall. */
    CHECK(player_can_see_bold(10, 11));
    check_terrain_at(24, 25);
    CHECK(fixture_material(24, 25));
    CHECK(fixture_chasm(24, 25));
    CHECK(fixture_liquid(24, 26) == FEAT_DEEP_WATER);
    CHECK(fixture_fixture(24, 27) == CAVE_FIXTURE_WALL_TORCH);
    CHECK(fixture_focus(24, 25));
    p_ptr->wy = 10;
    CHECK(target_list_contains(TARGET_LIST_MONSTER, 24, 28));
    CHECK(target_list_contains(TARGET_LIST_OBJECT, 24, 26));
    CHECK(object_is_visible(&o_list[1]));
    if (scenario == 8)
    {
        CHECK(cave_environment_known_feature(24, 29) == FEAT_POISON);
        CHECK(cave_environment_display_underlay(24, 29) == FEAT_POISON);
        CHECK(fixture_liquid(24, 29) == FEAT_POISON);
    }
    CHECK(p_ptr->monster_race_idx != 11);
    monster_race_track(11);
    CHECK(p_ptr->monster_race_idx == 11);
    do_cmd_view_map();
    CHECK(map_opened == 1 && map_blocked == 0);

    /* Redraw/view rebuilds must retain the reveal and consume no game turn. */
    for (int i = 0; i < 2; i++)
    {
        p_ptr->update |= PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS;
        __wrap_handle_stuff();
        check_terrain_at(24, 25);
        CHECK(object_is_visible(&o_list[1]));
    }
    CHECK(p_ptr->exp == 5800 && p_ptr->encounter_exp == 73);
    CHECK(p_ptr->ident_exp == 17);
    CHECK(p_ptr->niena_monsters_seen == 2);
    CHECK(!mon_list[1].encountered && !mon_list[2].encountered);
    CHECK(l_list[11].psights == 0 && l_list[11].tsights == 0);
    CHECK(!sil_achievement_unlocked_at(SIL_ACH_WHERE_GROND_WAS_WROUGHT));
    CHECK(Rand_state_export() == saved_rng);
    CHECK(turn == 8591 && playerturn == 817 && p_ptr->energy_use == 0);
    death_spectator_request_exit();
}

static void setup_final_look(int mode)
{
    reset_map(9);
    cave_environment_reset();
    partition_meta_save partitions = {0};
    partitions.grid_rows = partitions.grid_cols = partitions.partition_count = 1;
    partitions.modes[0] = mode == 1 ? QUAD_MODE_LABYRINTH : QUAD_MODE_ROOMY;
    level_partition_meta_set(&partitions);
    memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
    if (mode == 7)
    {
        for (int i = 0; i < z_info->cu_max; i++)
            if (cu_info[i].flags_u & CUR_LIGHTP) metar.curse_stacks[i] = 3;
        CHECK(curse_flag_delta_cur(CUR_LIGHTP) > 0);
    }
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 32;
    p_ptr->py = p_ptr->px = 10;
    cave_fixtures_clear();
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    memset(l_list, 0, z_info->r_max * sizeof(*l_list));
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++)
    {
        bool wall = y == 0 || x == 0 || y == 31 || x == 31 || y == 18;
        cave_set_feat(y, x, wall ? FEAT_WALL_EXTRA : FEAT_FLOOR);
        cave_info[y][x] = wall ? CAVE_WALL : 0;
        if (mode == 5) cave_info[y][x] |= CAVE_CHASM_AREA;
        cave_light[y][x] = 0;
    }
    cave_m_idx[10][10] = -1;
    if (mode == 9)
    {
        p_ptr->depth = UTUMNO_FORGE_DEPTH;
        cave_set_feat(10, 11, FEAT_FORGE_UNIQUE_HEAD);
    }
    cave_set_feat(24, 26, FEAT_DEEP_WATER);
    cave_set_feat(24, 27, FEAT_WALL_EXTRA);
    cave_fixture_set(24, 27, CAVE_FIXTURE_WALL_TORCH);
    object_prep(&o_list[1], lookup_kind(TV_POTION, SV_POTION_HEALING));
    o_list[1].iy = 24; o_list[1].ix = 26;
    cave_o_idx[24][26] = 1; o_max = 2; o_cnt = 1;
    for (int i = 1; i <= 2; i++)
    {
        monster_type* m = &mon_list[i];
        m->r_idx = 11; m->fy = i == 1 ? 10 : 24;
        m->fx = i == 1 ? 12 : 28;
        m->hp = m->maxhp = 20; m->cdis = distance(10, 10, m->fy, m->fx);
        m->alertness = ALERTNESS_ALERT;
        cave_m_idx[m->fy][m->fx] = i;
    }
    mon_max = 3; mon_cnt = 2;
    p_ptr->is_dead = true; p_ptr->playing = false; p_ptr->leaving = true;
    p_ptr->blind = mode == 2 ? 12 : 0;
    p_ptr->rage = mode == 3 ? 15 : 0;
    p_ptr->image = mode == 4 ? 20 : 0;
    g_labyrinth_view_active = mode == 1;
    p_ptr->cur_light = mode == 6 ? -3 : 0;
    if (mode == 6) r_info[11].light = -3;
    else r_info[11].light = 0;
    p_ptr->skill_use[S_PER] = 0;
    p_ptr->monster_race_idx = 0;
    p_ptr->niena_quest = NIENA_QUEST_ACTIVE; p_ptr->niena_monsters_seen = 2;
    p_ptr->exp = 5800; p_ptr->encounter_exp = 73; p_ptr->ident_exp = 17;
    p_ptr->update = p_ptr->redraw = p_ptr->window = 0;
    p_ptr->energy_use = 0; turn = 8591; playerturn = 817;
    character_generated = character_dungeon = true;
    character_icky = 2; character_xtra = false;
    map_opened = map_blocked = 0;
    view_n = 0;
    if (mode == 8)
    {
        cave_info[24][29] = CAVE_MARK;
        cave_set_feat(24, 29, FEAT_WATER);
        cave_environment_seed();
        cave_set_feat(24, 29, FEAT_POISON);
    }
    saved_rng = Rand_state_export();
}

static int check_final_look(void)
{
    const char* names[] = {"ordinary", "labyrinth", "blindness", "rage",
        "hallucination", "chasm darkness", "player and monster darkness",
        "darkness curse", "stale environmental memory", "terrain discovery"};
    ANGBAND_DIR_APEX = ANGBAND_DIR_USER;
    sil_achievements_init();
    int old_monster_light = r_info[11].light;
    for (scenario = 0; scenario < (int)N_ELEMENTS(names); scenario++)
    {
        scenario_name = names[scenario];
        setup_final_look(scenario);
        s16b blind = p_ptr->blind, rage = p_ptr->rage, image = p_ptr->image;
        bool labyrinth = g_labyrinth_view_active;
        int before = failures;
        death_spectator_view();
        CHECK(p_ptr->is_dead && !p_ptr->playing && p_ptr->leaving);
        CHECK(p_ptr->blind == blind && p_ptr->rage == rage && p_ptr->image == image);
        CHECK(g_labyrinth_view_active == labyrinth);
        CHECK(character_icky == 2 && !death_spectator_active());
        printf("%s: %s\n", scenario_name, failures == before ? "PASS" : "FAIL");
    }
    /* Actual living restrictions still apply on the same revealed map. */
    scenario_name = "living controls";
    p_ptr->is_dead = false; p_ptr->playing = true; p_ptr->leaving = false;
    character_icky = 0;
    p_ptr->blind = p_ptr->rage = p_ptr->image = 0;
    g_labyrinth_view_active = true;
    CHECK(!grid_info_is_available(24, 25));
    CHECK(!fixture_focus(24, 25));
    CHECK(!object_is_visible(&o_list[1]));
    CHECK(!fixture_liquid(24, 26));
    CHECK(!fixture_fixture(24, 27));
    g_labyrinth_view_active = false;
    cave_info[24][26] |= CAVE_SEEN;
    p_ptr->skill_use[S_PER] = 200;
    CHECK(object_is_visible(&o_list[1]));
    p_ptr->blind = 1;
    CHECK(!object_is_visible(&o_list[1]));
    p_ptr->blind = 0;
    p_ptr->niena_quest = 0;
    update_mon(1, true);
    CHECK(mon_list[1].encountered && p_ptr->encounter_exp > 73);
    p_ptr->is_dead = true;
    mon_list[1].encountered = mon_list[1].ml = false;
    s32b living_exp = p_ptr->exp, living_encounter_exp = p_ptr->encounter_exp;
    update_mon(1, true);
    CHECK(mon_list[1].ml && !mon_list[1].encountered);
    CHECK(p_ptr->exp == living_exp && p_ptr->encounter_exp == living_encounter_exp);
    r_info[11].light = old_monster_light;
    printf("Final Look: %d checks, %d failures\n", checks, failures);
    SDL_Quit();
    return failures ? 1 : 0;
}
