#include "angband.h"
#include "externs.h"
#include "monster/monster-senses.h"
#include <stdio.h>

static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u16b info[MAX_DUNGEON_HGT][256];
static s16b occupants[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s16b floor_objects[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte pending[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static monster_type monsters[8];
static object_type objects[16];
static int checks, moves, mined, cleared, deaths, drops, skeleton_sval;
static bool has_target, needs_return, movement_blocked;
static int sight_range, territory_x;
static unsigned inspected_directions;

#define CHECK(test) do { checks++; if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); exit(1); } } while (0)

/* Exercise the real behaviour, replacing only its engine side effects. */
void monster_swap(int y1, int x1, int y2, int x2)
{
    CHECK(occupants[y2][x2] == 0);
    CHECK(features[y2][x2] == FEAT_FLOOR || features[y2][x2] == FEAT_WATER);
    int idx = occupants[y1][x1];
    occupants[y1][x1] = 0; occupants[y2][x2] = idx;
    monsters[idx].fy = y2; monsters[idx].fx = x2;
    moves++;
}
void cave_set_feat(int y, int x, int feat)
{
    if (FEAT_IS_QUARTZ(features[y][x]))
    {
        CHECK(feat == FEAT_RUBBLE);
        mined++;
    }
    else
    {
        CHECK(features[y][x] == FEAT_RUBBLE && feat == FEAT_FLOOR);
        cleared++;
    }
    features[y][x] = feat;
    info[y][x] = feat == FEAT_RUBBLE ? CAVE_WALL : 0;
}
int water_movement_energy(int base, int from, int to, bool flying)
{ (void)from; (void)to; (void)flying; return base; }
void monster_desc(char* out, size_t len, const monster_type* m, int mode)
{ (void)m; (void)mode; snprintf(out, len, "a thrall"); }
void object_desc(char* out, size_t len, const object_type* o, int pref, int mode)
{ (void)o; (void)pref; (void)mode; snprintf(out, len, "a gem"); }
void msg_format(cptr fmt, ...) { (void)fmt; }
void lite_spot(int y, int x)
{
    int idx = occupants[y][x];
    CHECK(idx > 0);
    inspected_directions |= 1u << monsters[idx].visual_facing_dir;
}
void monster_senses_refresh(monster_type* m) { (void)m; }
bool monster_senses_target(const monster_type* m, int* y, int* x)
{ (void)m; (void)y; (void)x; return has_target; }
int monster_sight_range(const monster_type* m) { (void)m; return sight_range; }
bool monster_routine_needs_return(const monster_type* m)
{ (void)m; return needs_return; }
bool monster_routine_allows(const monster_type* m, int y, int x)
{ (void)m; (void)y; return x <= territory_x; }
int cave_environment_pending_hazard(int y, int x) { return pending[y][x]; }
void process_move(monster_type* m, int y, int x, bool bash)
{
    CHECK(!bash);
    if (!movement_blocked) monster_swap(m->fy, m->fx, y, x);
}
void monster_sound(const monster_type* m, int action)
{ (void)m; CHECK(action == MONSTER_SOUND_DEATH); }
void delete_monster_idx(int idx)
{
    CHECK(monsters[idx].r_idx == R_IDX_HUMAN_THRALL
        || monsters[idx].r_idx == R_IDX_ELF_THRALL);
    occupants[monsters[idx].fy][monsters[idx].fx] = 0;
    memset(&monsters[idx], 0, sizeof(monsters[idx]));
    deaths++;
}
s16b lookup_kind(int tval, int sval)
{ CHECK(tval == TV_SKELETON); return sval + 1; }
void object_wipe(object_type* o) { memset(o, 0, sizeof(*o)); }
void object_prep(object_type* o, int kind)
{ o->tval = TV_SKELETON; o->sval = kind - 1; }
s16b drop_near(object_type* o, int chance, int y, int x)
{
    CHECK(o->tval == TV_SKELETON && o->pval == 1 && chance == -1);
    CHECK(occupants[y][x] == 0);
    skeleton_sval = o->sval; drops++; return 1;
}

static void tile(int y, int x, int feat)
{
    features[y][x] = feat;
    info[y][x] = feat >= FEAT_DOOR_HEAD && feat <= FEAT_WALL_TAIL
        ? CAVE_WALL : 0;
}

static monster_type* place(int idx, int race, int y, int x)
{
    monster_type* m = &monsters[idx];
    memset(m, 0, sizeof(*m));
    m->r_idx = race; m->fy = y; m->fx = x; m->hp = m->maxhp = 20;
    m->alertness = ALERTNESS_UNWARY;
    occupants[y][x] = idx;
    return m;
}

static monster_type* reset(int race)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(monsters, 0, sizeof(monsters));
    memset(occupants, 0, sizeof(occupants));
    memset(floor_objects, 0, sizeof(floor_objects));
    memset(objects, 0, sizeof(objects));
    memset(pending, 0, sizeof(pending));
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 31;
    for (int y = 0; y < 31; y++)
        for (int x = 0; x < 31; x++)
            tile(y, x, y == 0 || x == 0 || y == 30 || x == 30
                ? FEAT_WALL_PERM : FEAT_FLOOR);
    cave_feat = features; cave_info = info; cave_m_idx = occupants;
    cave_o_idx = floor_objects; o_list = objects;
    mon_list = monsters; mon_max = 8;
    moves = mined = cleared = deaths = drops = 0; has_target = false;
    needs_return = movement_blocked = false;
    sight_range = MAX_SIGHT; territory_x = 30;
    inspected_directions = 0;
    Rand_state_init(12345);
    return place(1, race, 15, 15);
}

static void turns(monster_type* m, int count)
{
    for (int i = 0; i < count; i++)
        monster_thrall_turn(m);
}

static void test_miners(void)
{
    int races[] = { R_IDX_HUMAN_THRALL, R_IDX_ELF_THRALL };
    for (int r = 0; r < 2; r++)
    {
        monster_type* m = reset(races[r]);
        /* A miner checks different kinds of adjacent wall, eventually finding
         * the quartz. Only quartz changes; other inspections cost an action. */
        for (int i = 0; i < 8; i++)
            tile(15 + ddy_ddd[i], 15 + ddx_ddd[i], FEAT_WALL_EXTRA);
        tile(15, 16, FEAT_QUARTZ);
        m->ml = true;
        for (int i = 0; i < 3000 && !mined; i++) monster_thrall_turn(m);
        CHECK(mined == 1 && moves == 0 && features[15][16] == FEAT_RUBBLE);
        for (int i = 0; i < 8; i++)
        {
            if (ddd[i] != 6)
                CHECK(features[15 + ddy_ddd[i]][15 + ddx_ddd[i]] == FEAT_WALL_EXTRA);
        }
        CHECK((p_ptr->update & (PU_UPDATE_VIEW | PU_MONSTERS))
            == (PU_UPDATE_VIEW | PU_MONSTERS));
        for (int i = 0; i < 3000 && !cleared; i++) monster_thrall_turn(m);
        CHECK(cleared == 1 && moves == 0 && features[15][16] == FEAT_FLOOR);
        for (int i = 0; i < 3000 && !moves; i++) monster_thrall_turn(m);
        CHECK(moves == 1 && m->fy == 15 && m->fx == 16);

        m = reset(races[r]);
        for (int i = 0; i < 8; i++)
            tile(15 + ddy_ddd[i], 15 + ddx_ddd[i], FEAT_WALL_EXTRA);
        m->ml = true;
        turns(m, 3000);
        for (int i = 0; i < 8; i++)
            CHECK(inspected_directions & (1u << ddd[i]));
        CHECK(mined == 0 && cleared == 0 && moves == 0);

        m = reset(races[r]);
        /* Enclosed quartz cannot be mined from a distance while wandering. */
        for (int y = 12; y <= 14; y++)
            for (int x = 14; x <= 16; x++) tile(y, x, FEAT_WALL_EXTRA);
        tile(13, 15, FEAT_QUARTZ);
        turns(m, 200);
        CHECK(mined == 0 && moves > 0 && features[13][15] == FEAT_QUARTZ);

        m = reset(races[r]);
        turns(m, 200);
        CHECK(moves > 0 && mined == 0);

        m = reset(races[r]);
        tile(15, 16, FEAT_QUARTZ);
        m->alertness = ALERTNESS_UNWARY - 1; turns(m, 100);
        m->alertness = ALERTNESS_UNWARY; m->confused = 1; turns(m, 100);
        m->confused = 0; m->stunned = 1; turns(m, 100);
        m->stunned = 0; m->skip_this_turn = true; turns(m, 100);
        CHECK(mined == 0 && moves == 0);
    }

    /* A dangerous or occupied corridor must not be crossed. */
    int barriers[] = { FEAT_LAVA, FEAT_POISON, FEAT_CHASM, FEAT_DEEP_WATER,
        FEAT_TRAP_PIT, FEAT_DOOR_HEAD, FEAT_FLOOR };
    for (int b = 0; b < (int)N_ELEMENTS(barriers); b++)
    {
        monster_type* m = reset(R_IDX_HUMAN_THRALL);
        for (int y = 1; y < 30; y++)
            for (int x = 1; x < 30; x++) tile(y, x, FEAT_WALL_EXTRA);
        tile(15, 15, FEAT_FLOOR); tile(15, 16, barriers[b]);
        tile(15, 17, FEAT_FLOOR); tile(15, 18, FEAT_QUARTZ);
        if (barriers[b] == FEAT_FLOOR) occupants[15][16] = -1;
        turns(m, 100);
        CHECK(mined == 0 && moves == 0);
    }

    /* Distant quartz must not influence the first step. Across seeds, miners
     * can head in every direction, including directly away from the vein. */
    unsigned moved_directions = 0;
    for (int seed = 1; seed <= 128; seed++)
    {
        monster_type* m = reset(R_IDX_HUMAN_THRALL);
        Rand_state_init(seed);
        for (int i = 0; i < 200 && !moves; i++) monster_thrall_turn(m);
        CHECK(moves == 1);
        int y = m->fy, x = m->fx;
        moved_directions |= 1u << rough_direction(15, 15, y, x);

        m = reset(R_IDX_HUMAN_THRALL);
        tile(15, 20, FEAT_QUARTZ);
        Rand_state_init(seed);
        for (int i = 0; i < 200 && !moves; i++) monster_thrall_turn(m);
        CHECK(moves == 1 && m->fy == y && m->fx == x && mined == 0);
    }
    for (int i = 0; i < 8; i++) CHECK(moved_directions & (1u << ddd[i]));

    /* Nearby quartz has no priority over inspecting ordinary stone. */
    int ordinary_checks = 0;
    for (int seed = 1; seed <= 128; seed++)
    {
        monster_type* m = reset(R_IDX_HUMAN_THRALL);
        for (int i = 0; i < 8; i++)
            tile(15 + ddy_ddd[i], 15 + ddx_ddd[i], FEAT_WALL_EXTRA);
        tile(15, 16, FEAT_QUARTZ);
        m->ml = true;
        Rand_state_init(seed);
        for (int i = 0; i < 200 && !inspected_directions; i++)
            monster_thrall_turn(m);
        CHECK(inspected_directions != 0);
        if (!mined) ordinary_checks++;
    }
    CHECK(ordinary_checks > 80 && ordinary_checks < 128);

    /* About 375 wandering actions in 3000 slow turns. Re-centre after each
     * move so no wall inspections obscure the existing action cadence. */
    monster_type* m = reset(R_IDX_HUMAN_THRALL);
    for (int i = 0; i < 3000; i++)
    {
        monster_thrall_turn(m);
        occupants[m->fy][m->fx] = 0;
        m->fy = m->fx = 15;
        occupants[15][15] = 1;
    }
    CHECK(moves > 290 && moves < 460 && mined == 0);
}

static object_type* floor_item(int idx, int tval, int y, int x)
{
    object_type* o = &objects[idx];
    memset(o, 0, sizeof(*o));
    o->k_idx = 1; o->tval = tval; o->number = 3; o->pval = 7;
    o->iy = y; o->ix = x; o->marked = true;
    o->next_o_idx = floor_objects[y][x]; floor_objects[y][x] = idx;
    return o;
}

static void test_collection(void)
{
    monster_type* m = reset(R_IDX_ORC_THRALLMASTER);
    floor_item(1, TV_GEM, 15, 15);
    floor_item(2, TV_FOOD, 15, 15);
    floor_item(3, TV_GEM, 15, 15)->ident = IDENT_CURSED;
    floor_item(4, TV_GEM, 15, 15)->ident = IDENT_BROKEN;
    floor_item(5, TV_GEM, 15, 15);
    CHECK(monster_thrall_turn(m));
    CHECK(moves == 0 && m->hold_o_idx == 1 && objects[1].next_o_idx == 5);
    CHECK(objects[5].next_o_idx == 0 && floor_objects[15][15] == 4);
    CHECK(objects[4].next_o_idx == 3 && objects[3].next_o_idx == 2);
    CHECK(objects[2].next_o_idx == 0);
    for (int i = 1; i <= 5; i += 4)
    {
        CHECK(objects[i].held_m_idx == 1 && objects[i].number == 3
            && objects[i].pval == 7 && !objects[i].marked);
        CHECK(objects[i].iy == 0 && objects[i].ix == 0);
    }
    CHECK(!monster_thrall_turn(m)); /* No interest in the remaining items. */

    /* All eight directions, both adjacent and several steps away. */
    for (int d = 0; d < 8; d++) for (int dist = 1; dist <= 3; dist += 2)
    {
        m = reset(R_IDX_ORC_THRALLMASTER);
        int y = 15 + dist * ddy_ddd[d], x = 15 + dist * ddx_ddd[d];
        floor_item(1, TV_GEM, y, x);
        for (int i = 0; i < 8 && !m->hold_o_idx; i++) CHECK(monster_thrall_turn(m));
        CHECK(m->hold_o_idx == 1 && m->fy == y && m->fx == x);
        CHECK(moves >= dist && floor_objects[y][x] == 0);
    }

    /* A refused/reaction-interrupted move cannot pick up remotely. */
    m = reset(R_IDX_ORC_THRALLMASTER);
    floor_item(1, TV_GEM, 15, 16); movement_blocked = true;
    CHECK(monster_thrall_turn(m));
    CHECK(moves == 0 && !m->hold_o_idx && floor_objects[15][16] == 1);

    int barriers[] = { FEAT_LAVA, FEAT_POISON, FEAT_CHASM, FEAT_DEEP_WATER,
        FEAT_TRAP_PIT, FEAT_DOOR_HEAD, FEAT_RUBBLE, FEAT_GLYPH, FEAT_WALL_EXTRA,
        FEAT_ILLUSORY_WALL };
    for (int b = 0; b < (int)N_ELEMENTS(barriers); b++)
    {
        m = reset(R_IDX_ORC_THRALLMASTER);
        for (int i = 0; i < 8; i++)
            tile(15 + ddy_ddd[i], 15 + ddx_ddd[i], FEAT_WALL_EXTRA);
        tile(15, 16, barriers[b]);
        floor_item(1, TV_GEM, 15, 16);
        floor_item(2, TV_GEM, 15, 17);
        CHECK(!monster_thrall_turn(m));
        CHECK(!m->hold_o_idx && moves == 0);
    }

    for (int guard = 0; guard < 13; guard++)
    {
        m = reset(R_IDX_ORC_THRALLMASTER);
        floor_item(1, TV_GEM, 15, 16);
        switch (guard)
        {
            case 0: has_target = true; break;
            case 1: m->stance = STANCE_FLEEING; break;
            case 2: m->confused = 1; break;
            case 3: m->stunned = 1; break;
            case 4: m->skip_this_turn = true; break;
            case 5: m->alertness = ALERTNESS_UNWARY - 1; break;
            case 6: needs_return = true; break;
            case 7: territory_x = 15; break;
            case 8: pending[15][16] = FEAT_POISON; break;
            case 9: pending[15][15] = FEAT_POISON; break;
            case 10: sight_range = 0; break;
            case 11: occupants[15][16] = -1; break;
            case 12: occupants[15][16] = 2; break;
        }
        CHECK(!monster_thrall_turn(m));
        CHECK(!m->hold_o_idx && moves == 0 && floor_objects[15][16] == 1);
    }

    m = reset(R_IDX_ORC_THRALLMASTER);
    floor_item(1, TV_GEM, 15, 21);
    CHECK(!monster_thrall_turn(m) && moves == 0); /* Beyond collection range. */
    m = reset(R_IDX_ORC_THRALLMASTER);
    floor_item(1, TV_GEM, 15, 18); sight_range = 2;
    CHECK(!monster_thrall_turn(m) && moves == 0);
    m = reset(R_IDX_ORC_THRALLMASTER);
    floor_item(1, TV_GEM, 15, 18); tile(15, 16, FEAT_WALL_EXTRA);
    CHECK(!monster_thrall_turn(m) && moves == 0); /* Hidden behind stone. */
}

static void test_overseers(void)
{
    int races[] = { R_IDX_HUMAN_THRALL, R_IDX_ELF_THRALL };
    for (int r = 0; r < 2; r++)
    {
        monster_type* m = reset(R_IDX_ORC_THRALLMASTER);
        place(2, races[r], 15, 16);
        place(3, R_IDX_ALERT_HUMAN_THRALL, 14, 15);
        place(4, R_IDX_ALERT_ELF_THRALL, 16, 15);
        turns(m, 5000);
        CHECK(deaths == 1 && drops == 1 && !monsters[2].r_idx);
        CHECK(skeleton_sval == (r ? SV_SKELETON_ELF : SV_SKELETON_HUMAN));
        CHECK(monsters[3].r_idx == R_IDX_ALERT_HUMAN_THRALL);
        CHECK(monsters[4].r_idx == R_IDX_ALERT_ELF_THRALL);
        CHECK(p_ptr->kill_exp == 0 && p_ptr->encounter_exp == 0);
        CHECK(!monster_thrall_turn(&monsters[3]));
        CHECK(!monster_thrall_turn(&monsters[4]));
    }

    monster_type* m = reset(R_IDX_ORC_THRALLMASTER);
    place(2, R_IDX_HUMAN_THRALL, 15, 16);
    has_target = true; turns(m, 5000); has_target = false;
    m->stance = STANCE_FLEEING; turns(m, 5000); m->stance = STANCE_CONFIDENT;
    m->confused = 1; turns(m, 5000); m->confused = 0;
    m->alertness = ALERTNESS_UNWARY - 1; turns(m, 5000);
    CHECK(deaths == 0 && drops == 0);

    m = reset(R_IDX_ORC_THRALLMASTER);
    place(2, R_IDX_HUMAN_THRALL, 15, 17);
    turns(m, 5000); CHECK(deaths == 0);

    m = reset(R_IDX_ORC_THRALLMASTER);
    for (int i = 0; i < 25000; i++)
    {
        place(2, R_IDX_HUMAN_THRALL, 15, 16);
        monster_thrall_turn(m);
    }
    CHECK(deaths > 50 && deaths < 150 && deaths == drops);
}

int main(void)
{
    test_miners(); test_collection(); test_overseers();
    printf("Thrall behaviour regression checks passed: %d\n", checks);
    return 0;
}
