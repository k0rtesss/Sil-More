#include "angband.h"
#include "externs.h"
#include "monster/monster-senses.h"
#include <stdio.h>

static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u16b info[MAX_DUNGEON_HGT][256];
static s16b occupants[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static monster_type monsters[8];
static int checks, moves, mined, deaths, drops, skeleton_sval;
static bool has_target;

#define CHECK(test) do { checks++; if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); exit(1); } } while (0)

/* Exercise the real behaviour, replacing only its engine side effects. */
void monster_swap(int y1, int x1, int y2, int x2)
{
    CHECK(occupants[y2][x2] == 0);
    CHECK(features[y2][x2] == FEAT_FLOOR);
    int idx = occupants[y1][x1];
    occupants[y1][x1] = 0; occupants[y2][x2] = idx;
    monsters[idx].fy = y2; monsters[idx].fx = x2;
    moves++;
}
void cave_set_feat(int y, int x, int feat)
{
    CHECK(features[y][x] == FEAT_QUARTZ && feat == FEAT_RUBBLE);
    features[y][x] = feat;
    mined++;
}
int water_movement_energy(int base, int from, int to, bool flying)
{ (void)from; (void)to; (void)flying; return base; }
void monster_desc(char* out, size_t len, const monster_type* m, int mode)
{ (void)m; (void)mode; snprintf(out, len, "a thrall"); }
void msg_format(cptr fmt, ...) { (void)fmt; }
void monster_senses_refresh(monster_type* m) { (void)m; }
bool monster_senses_target(const monster_type* m, int* y, int* x)
{ (void)m; (void)y; (void)x; return has_target; }
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
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 31;
    for (int y = 0; y < 31; y++)
        for (int x = 0; x < 31; x++)
            tile(y, x, y == 0 || x == 0 || y == 30 || x == 30
                ? FEAT_WALL_PERM : FEAT_FLOOR);
    cave_feat = features; cave_info = info; cave_m_idx = occupants;
    mon_list = monsters; mon_max = 8;
    moves = mined = deaths = drops = 0; has_target = false;
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
        tile(15, 16, FEAT_QUARTZ);
        turns(m, 100);
        CHECK(mined == 1 && moves == 0 && features[15][16] == FEAT_RUBBLE);
        CHECK((p_ptr->update & (PU_UPDATE_VIEW | PU_MONSTERS))
            == (PU_UPDATE_VIEW | PU_MONSTERS));

        m = reset(races[r]);
        /* Nearest quartz is sealed off. Walk round a wall to the usable vein. */
        for (int y = 12; y <= 14; y++)
            for (int x = 14; x <= 16; x++) tile(y, x, FEAT_WALL_EXTRA);
        tile(13, 15, FEAT_QUARTZ);
        for (int y = 14; y <= 17; y++) tile(y, 17, FEAT_WALL_EXTRA);
        tile(15, 19, FEAT_QUARTZ);
        turns(m, 200);
        CHECK(mined == 1 && moves > 0);
        CHECK(features[13][15] == FEAT_QUARTZ && features[15][19] == FEAT_RUBBLE);

        m = reset(races[r]);
        turns(m, 200);
        CHECK(moves == 0 && mined == 0);

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
        FEAT_TRAP_PIT, FEAT_DOOR_HEAD, FEAT_RUBBLE, FEAT_FLOOR };
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

    monster_type* m = reset(R_IDX_HUMAN_THRALL);
    tile(15, 29, FEAT_QUARTZ); /* Beyond the local search. */
    turns(m, 100);
    CHECK(moves == 0 && mined == 0);

    /* About 375 work actions in 3000 slow turns (6000 normal player turns). */
    m = reset(R_IDX_HUMAN_THRALL);
    for (int i = 0; i < 3000; i++)
    {
        tile(15, 16, FEAT_QUARTZ);
        monster_thrall_turn(m);
    }
    CHECK(mined > 290 && mined < 460);
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
    test_miners(); test_overseers();
    printf("Thrall behaviour regression checks passed: %d\n", checks);
    return 0;
}
