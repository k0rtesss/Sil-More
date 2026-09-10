#include "angband.h"
#include "melee/melee-util.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

/* Exercise production decisions, including the private positional scores.
 * Rendering, quest boundaries and stat rolls are outside these map fixtures. */
#include "../../src/melee/melee-movement-tactics.c"

static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte rewired[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte scents[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u16b info[MAX_DUNGEON_HGT][256];
static s16b occupants[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static monster_type monsters[8];
static monster_race races[R_IDX_MORGOTH + 1];
static monster_lore lore[R_IDX_MORGOTH + 1];
static character_profile profiles[1];
static int checks;

#define CHECK(test) do { checks++; if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); exit(1); } } while (0)

bool varda_quest_duruin_can_enter(const monster_type* m, int y, int x)
{ (void)m; (void)y; (void)x; return true; }
int monster_skill(monster_type* m, int skill)
{ (void)m; (void)skill; return 10; }
int success_chance(int sides, int skill, int difficulty)
{ (void)sides; (void)skill; (void)difficulty; return 50; }
int monster_stat(monster_type* m, int stat)
{ (void)m; (void)stat; return 10; }
bool monster_race_is_vala(int r_idx)
{ return r_idx == R_IDX_MORGOTH; }
bool singing(int song)
{ (void)song; return false; }

/* These effects belong to retreat/territorial behavior. Advancing along a
 * reachable flow must never call them in the fixtures below. */
void set_alertness(monster_type* m, int alertness)
{ (void)m; (void)alertness; CHECK(false); }
void shriek(monster_type* m) { (void)m; CHECK(false); }
void calc_morale(monster_type* m) { (void)m; CHECK(false); }
void calc_stance(monster_type* m) { (void)m; CHECK(false); }
byte projectable(int y1, int x1, int y2, int x2, u32b flags)
{ (void)flags; return los(y1, x1, y2, x2); }
void monster_desc(char* out, size_t len, const monster_type* m, int mode)
{ (void)out; (void)len; (void)m; (void)mode; CHECK(false); }
void msg_format(cptr fmt, ...) { (void)fmt; CHECK(false); }

static void tile(int y, int x, int feat)
{
    features[y][x] = feat;
    info[y][x] = CAVE_VIEW;
    if (feat == FEAT_WALL_PERM || feat == FEAT_WALL_EXTRA
        || feat == FEAT_DOOR_HEAD)
        info[y][x] |= CAVE_WALL;
}

static monster_type* reset_map(int h, int w, int my, int mx, int py, int px)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(monsters, 0, sizeof(monsters));
    memset(races, 0, sizeof(races));
    memset(occupants, 0, sizeof(occupants));
    memset(rewired, 0, sizeof(rewired));
    memset(scents, 0, sizeof(scents));
    visual_recognition = false;
    p_ptr->cur_map_hgt = h; p_ptr->cur_map_wid = w;
    p_ptr->py = py; p_ptr->px = px;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            tile(y, x, y == 0 || x == 0 || y == h-1 || x == w-1
                ? FEAT_WALL_PERM : FEAT_FLOOR);
    cave_feat = features; cave_info = info; cave_m_idx = occupants;
    cave_rewired = rewired;
    cave_when = scents;
    mon_list = monsters; r_info = races; l_list = lore; c_info = profiles;
    mon_max = 3;
    monsters[1].r_idx = 1; monsters[1].fy = my; monsters[1].fx = mx;
    monsters[1].hp = monsters[1].maxhp = 100;
    monsters[1].min_range = monsters[1].best_range = 1;
    monsters[1].alertness = ALERTNESS_ALERT;
    monsters[1].stance = STANCE_AGGRESSIVE;
    monsters[1].cdis = distance(my, mx, py, px);
    occupants[my][mx] = 1; occupants[py][px] = -1;
    races[1].flags2 = RF2_SMART;
    races[1].level = 10;
    return &monsters[1];
}

static void move_monster(monster_type* m, int y, int x)
{
    occupants[m->fy][m->fx] = 0;
    m->fy = y; m->fx = x;
    m->cdis = distance(y, x, p_ptr->py, p_ptr->px);
    occupants[y][x] = 1;
}

static void test_terrain(void)
{
    monster_type* m = reset_map(11, 11, 5, 2, 5, 8);
    bool bash = false;
    tile(5, 3, FEAT_WATER);
    CHECK(monster_step_cost(m, 5, 2, 5, 3) == 2);
    CHECK(monster_step_cost(m, 5, 3, 5, 4) == 2);
    CHECK(monster_terrain_penalty(m, 5, 3) == 0);
    races[1].flags2 |= RF2_FLYING;
    CHECK(monster_step_cost(m, 5, 2, 5, 3) == 1);
    CHECK(monster_step_cost(m, 5, 3, 5, 4) == 1);
    tile(5, 3, FEAT_LAVA);
    CHECK(cave_exist_mon(&races[1], 5, 3, false, false));
    CHECK(!cave_passable_mon(m, 5, 3, &bash));
    CHECK(!monster_step_cost(m, 5, 2, 5, 3));
    races[1].flags2 &= ~RF2_FLYING;
    CHECK(!cave_exist_mon(&races[1], 5, 3, false, false));
    races[1].flags3 = RF3_RES_FIRE;
    CHECK(monster_step_cost(m, 5, 2, 5, 3) == 1);
    CHECK(monster_terrain_penalty(m, 5, 3) == 0);
    tile(5, 3, FEAT_CHASM);
    CHECK(!monster_step_cost(m, 5, 2, 5, 3));
    races[1].flags2 |= RF2_FLYING;
    CHECK(monster_step_cost(m, 5, 2, 5, 3) == 1);
    tile(5, 3, FEAT_ICE);
    CHECK(monster_terrain_penalty(m, 5, 3) == 0);
    races[1].flags2 &= ~RF2_FLYING;
    races[1].flags3 = RF3_RES_COLD;
    CHECK(monster_terrain_penalty(m, 5, 3) == ICE_ATTACK_PENALTY);
    CHECK(monster_step_cost(m, 5, 2, 5, 3) == 1);
    CHECK(cave_passable_mon(m, 5, 2, &bash) == 100);
    tile(5, 8, FEAT_LAVA);
    CHECK(monster_step_cost(m, 5, 7, 5, 8) == 1);
    races[1].flags1 |= RF1_NEVER_BLOW;
    CHECK(!monster_step_cost(m, 5, 7, 5, 8));
    CHECK(!monster_step_cost(m, 0, 0, -1, 0));
}

/* Independent repeated relaxation oracle: compare every reachable floor to
 * the bucket flow on a small map with asymmetric wet/door edge costs. */
static void check_flow_oracle(monster_type* m)
{
    int expected[16][16];
    int h = p_ptr->cur_map_hgt, w = p_ptr->cur_map_wid;
    CHECK(h <= 16 && w <= 16);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) expected[y][x] = FLOW_MAX_DIST;
    expected[p_ptr->py][p_ptr->px] = 0;
    for (int pass = 0; pass < h*w; pass++)
    {
        bool changed = false;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++)
        {
            if (y == p_ptr->py && x == p_ptr->px) continue;
            for (int d = 0; d < 8; d++)
            {
                int yy = y + ddy_ddd[d], xx = x + ddx_ddd[d];
                if (!in_bounds(yy, xx)) continue;
                int step = yy == p_ptr->py && xx == p_ptr->px ? 1
                    : monster_step_cost(m, y, x, yy, xx);
                if (step && expected[yy][xx] + step < expected[y][x])
                { expected[y][x] = expected[yy][xx] + step; changed = true; }
            }
        }
        if (!changed) break;
    }
    update_flow(p_ptr->py, p_ptr->px, 1);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++)
        CHECK(flow_dist(1, y, x) == expected[y][x]);
}

static void test_flows(void)
{
    monster_type* m = reset_map(11, 11, 5, 2, 5, 8);
    for (int y = 1; y < 10; y++) tile(y, 5, FEAT_LAVA);
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 2) == FLOW_MAX_DIST);
    races[1].flags2 |= RF2_FLYING;
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 2) == FLOW_MAX_DIST);
    races[1].flags3 = RF3_RES_FIRE;
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 2) == 6);
    races[1].flags3 = 0;
    for (int y = 1; y < 10; y++) tile(y, 5, FEAT_CHASM);
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 2) == 6);
    races[1].flags2 &= ~RF2_FLYING;
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 2) == FLOW_MAX_DIST);
    move_monster(m, 5, 5); tile(5, 5, FEAT_LAVA);
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 5) == 3); /* Escape, not traverse. */

    m = reset_map(11, 11, 5, 2, 5, 8);
    for (int y = 1; y < 10; y++) tile(y, 5, FEAT_WATER);
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 2) == 8);
    tile(5, 5, FEAT_ICE); /* A freshly frozen crossing changes the route. */
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 2) == 6);
    races[1].flags1 |= RF1_NEVER_BLOW;
    update_flow(5, 8, 1);
    CHECK(flow_dist(1, 5, 2) == 6);
    CHECK(!monster_step_cost(m, 5, 7, 5, 8));
    tile(2, 2, FEAT_LAVA);
    update_flow(2, 2, 1);
    CHECK(flow_dist(1, 2, 3) == FLOW_MAX_DIST);

    m = reset_map(13, 13, 6, 2, 6, 10);
    races[1].flags2 |= RF2_OPEN_DOOR;
    for (int y = 1; y < 12; y++) for (int x = 3; x < 10; x++)
        if ((x * 13 + y * 7) % 5 == 0) tile(y, x, FEAT_WATER);
    tile(6, 7, FEAT_DOOR_HEAD); tile(5, 7, FEAT_WALL_PERM);
    tile(7, 7, FEAT_LAVA);
    check_flow_oracle(m);
    races[1].flags2 |= RF2_FLYING;
    check_flow_oracle(m);
    update_flow(6, 10, FLOW_PLAYER_NOISE);
    int noise = flow_dist(FLOW_PLAYER_NOISE, 6, 2);
    races[1].flags2 = 0; races[1].flags3 = RF3_RES_FIRE;
    update_flow(6, 10, FLOW_PLAYER_NOISE);
    CHECK(flow_dist(FLOW_PLAYER_NOISE, 6, 2) == noise);
}

static void test_tactics(void)
{
    int y, x;
    monster_type* m = reset_map(11, 11, 5, 4, 5, 5);
    CHECK(!get_move_tactical(m, &y, &x)); /* No purposeless circling. */
    tile(5, 4, FEAT_ICE);
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(features[y][x] == FEAT_FLOOR);
    move_monster(m, y, x);
    CHECK(!get_move_tactical(m, &y, &x));
    move_monster(m, 5, 4); races[1].flags2 |= RF2_FLYING;
    CHECK(!get_move_tactical(m, &y, &x));
    races[1].flags2 &= ~RF2_FLYING; races[1].flags3 = RF3_RES_COLD;
    CHECK(get_move_tactical(m, &y, &x));

    m = reset_map(11, 11, 5, 4, 5, 5);
    races[1].flags2 |= RF2_KNOCK_BACK;
    tile(4, 6, FEAT_LAVA);
    CHECK(tactical_knockback(m, 5, 4) == 0); /* Straight safe wins. */
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(y == 6 && x == 4);
    move_monster(m, y, x);
    CHECK(tactical_knockback(m, y, x) == 100);
    CHECK(!get_move_tactical(m, &y, &x));
    occupants[4][6] = 2;
    CHECK(tactical_knockback(m, 6, 4) == 0);
    occupants[4][6] = 0;
    races[1].flags2 &= ~RF2_KNOCK_BACK;
    CHECK(!get_move_tactical(m, &y, &x));

    m = reset_map(11, 11, 5, 4, 5, 5);
    races[1].flags2 |= RF2_KNOCK_BACK;
    tile(5, 6, FEAT_WALL_PERM); tile(4, 6, FEAT_CHASM);
    CHECK(tactical_knockback(m, 5, 4) == 75 / 2);
    tile(6, 6, FEAT_WALL_PERM);
    CHECK(tactical_knockback(m, 5, 4) == 75);
    tile(4, 6, FEAT_TRAP_PIT);
    CHECK(tactical_knockback(m, 5, 4) == 40);
    info[4][6] |= CAVE_HIDDEN;
    CHECK(tactical_knockback(m, 5, 4) == 0);
    info[4][6] &= ~CAVE_HIDDEN; rewired[4][6] = 20;
    CHECK(tactical_knockback(m, 5, 4) == 40); /* No secret tampering knowledge. */
    m->confused = 1; CHECK(!get_move_tactical(m, &y, &x)); m->confused = 0;
    m->stance = STANCE_FLEEING; CHECK(!get_move_tactical(m, &y, &x));
    m->stance = STANCE_AGGRESSIVE;
    /* Ranged profiles now share the local terrain/lane evaluator. */
    m->min_range = m->best_range = 4;
    if (get_move_tactical(m, &y, &x)) CHECK(distance(y, x, 5, 5) > 1);
    m->min_range = m->best_range = 1;
    races[1].flags2 |= RF2_MINDLESS; CHECK(!get_move_tactical(m, &y, &x));

    m = reset_map(11, 11, 4, 5, 5, 5);
    monsters[2] = monsters[1]; monsters[2].fy = 4; monsters[2].fx = 6;
    occupants[4][6] = 2;
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(y == 5 && x == 4); /* Spread opposite the allied attacker. */
    move_monster(m, y, x);
    CHECK(!get_move_tactical(m, &y, &x));
    m = reset_map(7, 7, 1, 1, 1, 2);
    tile(1, 3, FEAT_CHASM); races[1].flags2 |= RF2_KNOCK_BACK;
    (void)get_move_tactical(m, &y, &x); /* Border neighborhood remains bounded. */
}

static void test_advance(void)
{
    monster_type* m = reset_map(11, 11, 5, 2, 5, 8);
    /* The direct route costs 10, but a dry detour costs 6. */
    for (int x = 3; x <= 5; x++) tile(5, x, FEAT_WATER);
    for (int step = 0; step < 5; step++)
    {
        int y = m->fy, x = m->fx;
        update_flow(5, 8, 1);
        int before = flow_dist(1, m->fy, m->fx);
        get_move_advance(m, &y, &x);
        CHECK(distance(m->fy, m->fx, y, x) == 1);
        CHECK(features[y][x] != FEAT_WATER);
        CHECK(monster_step_cost(m, m->fy, m->fx, y, x)
            + flow_dist(1, y, x) == before);
        move_monster(m, y, x);
    }
    CHECK(m->cdis == 1);
    /* Remembered destinations retain their behavior. */
    m->target_y = 2; m->target_x = 2;
    int y = m->fy, x = m->fx;
    get_move_advance(m, &y, &x);
    CHECK(y == 2 && x == 2);

    m = reset_map(11, 11, 5, 2, 5, 8);
    races[1].flags1 |= RF1_NEVER_BLOW;
    tile(5, 3, FEAT_LAVA);
    update_flow(5, 8, 1);
    y = m->fy; x = m->fx;
    get_move_advance(m, &y, &x);
    CHECK(features[y][x] != FEAT_LAVA);
    CHECK(flow_dist(1, y, x) < flow_dist(1, m->fy, m->fx));
}

static void test_full_map(void)
{
    reset_map(MAX_DUNGEON_HGT, MAX_DUNGEON_WID, 1, 1,
        MAX_DUNGEON_HGT/2, MAX_DUNGEON_WID/2);
    clock_t start = clock();
    for (int i = 0; i < 100; i++) update_flow(p_ptr->py, p_ptr->px, 1);
    CHECK(flow_dist(1, 1, 1) == MAX_DUNGEON_HGT/2 - 1);
    printf("100 maximum-map monster flows: %.3f seconds\n",
        (double)(clock() - start) / CLOCKS_PER_SEC);
}

#include "monster-poison-ai-tests.h"
#include "monster-tactics-ai-tests.h"

int main(void)
{
    test_terrain(); test_flows(); test_tactics(); test_advance(); test_full_map();
    test_poison_ai();
    test_tactical_extension();
    printf("Monster AI regression checks passed: %d\n", checks);
    return 0;
}
