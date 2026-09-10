#include "angband.h"
#include "externs.h"
#include "monster/monster-senses.h"
#include "melee/melee-process.h"
#include "melee/melee-movement.h"
#include "melee/melee-movement-internal.h"
#include "melee/melee-util.h"
#include <stdio.h>

/* Link real sensing, perception, scent deposition, movement costs and flows.
 * Only unrelated quest boundaries, messages and character skill setup are
 * stubbed. In particular hearing runs the production alertness roll. */
static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte scents[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte rewired[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u16b info[MAX_DUNGEON_HGT][256];
static s16b occupants[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static monster_type monsters[8];
static monster_race races[8];
static monster_lore lore[8];
static character_profile profiles[1];
static int checks;
#define CHECK(test) do { checks++; if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); exit(1); } } while (0)

bool varda_quest_duruin_can_enter(const monster_type* m, int y, int x)
{ (void)m; (void)y; (void)x; return true; }
int monster_skill(monster_type* m, int skill)
{ (void)m; (void)skill; return 10; }
int monster_stat(monster_type* m, int stat)
{ (void)m; (void)stat; return 10; }
bool monster_race_is_vala(int r_idx) { (void)r_idx; return false; }
bool singing(int song) { (void)song; return false; }
int success_chance(int sides, int skill, int difficulty)
{ (void)sides; (void)skill; (void)difficulty; return 50; }
void set_alertness(monster_type* m, int alertness) { m->alertness = alertness; }
int bane_bonus(monster_type* m) { (void)m; return 0; }
int artifact_bane_bonus(monster_type* m) { (void)m; return 0; }
int elf_bane_bonus(monster_type* m) { (void)m; return 0; }
int dwarf_bane_bonus(monster_type* m) { (void)m; return 0; }
int edain_bane_bonus(monster_type* m) { (void)m; return 0; }
int ability_bonus(int skill, int ability) { (void)skill; (void)ability; return 0; }
void msg_format(cptr fmt, ...) { (void)fmt; }
int monster_ai_confidence(const monster_type* m, int feature)
{ (void)m; (void)feature; return 0; }
int health_level(int hp, int maxhp) { (void)hp; (void)maxhp; CHECK(false); return 0; }
bool similar_monsters(int y1, int x1, int y2, int x2)
{ (void)y1; (void)x1; (void)y2; (void)x2; CHECK(false); return false; }
void shriek(monster_type* m) { (void)m; CHECK(false); }
byte projectable(int y1, int x1, int y2, int x2, u32b flags)
{ (void)y1; (void)x1; (void)y2; (void)x2; (void)flags; CHECK(false); return 0; }
void monster_desc(char* out, size_t len, const monster_type* m, int mode)
{ (void)out; (void)len; (void)m; (void)mode; CHECK(false); }

static void tile(int y, int x, int feat)
{
    features[y][x] = feat;
    info[y][x] = (feat == FEAT_WALL_PERM || feat == FEAT_WALL_EXTRA) ? CAVE_WALL : 0;
}

static monster_type* reset_map(void)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(monsters, 0, sizeof(monsters)); memset(races, 0, sizeof(races));
    memset(lore, 0, sizeof(lore)); memset(occupants, 0, sizeof(occupants));
    memset(scents, 0, sizeof(scents)); memset(rewired, 0, sizeof(rewired));
    p_ptr->cur_map_hgt = 15; p_ptr->cur_map_wid = 22;
    p_ptr->py = 5; p_ptr->px = 18;
    cave_feat = features; cave_when = scents; cave_info = info;
    cave_m_idx = occupants; cave_rewired = rewired;
    mon_list = monsters; r_info = races; l_list = lore; c_info = profiles;
    mon_max = 3; playerturn = 100; scent_when = 150;
    visual_recognition = false; cheat_timestop = false; cheat_skill_rolls = false;
    for (int y = 0; y < 15; y++) for (int x = 0; x < 22; x++)
        tile(y, x, !y || !x || y == 14 || x == 21 || x == 11
            ? FEAT_WALL_PERM : FEAT_FLOOR);
    monster_type* m = &monsters[1];
    m->r_idx = 1; m->fy = 5; m->fx = 3;
    m->hp = m->maxhp = 100; m->alertness = ALERTNESS_ALERT;
    m->min_range = m->best_range = 1; m->stance = STANCE_AGGRESSIVE;
    m->cdis = distance(m->fy, m->fx, p_ptr->py, p_ptr->px);
    occupants[m->fy][m->fx] = 1; occupants[p_ptr->py][p_ptr->px] = -1;
    races[1].d_char = 'C'; races[1].flags3 = RF3_WOLF;
    return m;
}

static void move(monster_type* m, int y, int x)
{
    occupants[m->fy][m->fx] = 0; m->fy = y; m->fx = x;
    occupants[y][x] = 1;
}

static void test_capabilities_and_acquisition(void)
{
    monster_type* m = reset_map(); int y, x;
    CHECK(monster_scent_limit(&races[1]) == 80);
    races[1].flags3 = RF3_CAT; races[1].d_char = 'f';
    CHECK(monster_scent_limit(&races[1]) == 40);
    scents[5][3] = scent_when + 41;
    monster_senses_refresh(m); CHECK(m->ai.sense.kind == MON_SENSE_NONE);
    scents[5][3] = scent_when + 40;
    m->alertness = ALERTNESS_UNWARY;
    monster_senses_refresh(m); CHECK(m->ai.sense.kind == MON_SENSE_SCENT);
    CHECK(m->alertness == ALERTNESS_UNWARY);
    CHECK(monster_senses_target(m, &y, &x) && y == 5 && x == 3);
    memset(&m->ai.sense, 0, sizeof(m->ai.sense));
    m->alertness = ALERTNESS_UNWARY - 1;
    monster_senses_refresh(m); CHECK(m->ai.sense.kind == MON_SENSE_NONE);
    m->alertness = ALERTNESS_ALERT; races[1].flags3 = RF3_DRAGON;
    races[1].d_char = 'D'; scents[5][3] = scent_when + 80;
    monster_senses_refresh(m); CHECK(m->ai.sense.kind == MON_SENSE_SCENT);
    memset(&m->ai.sense, 0, sizeof(m->ai.sense));
    scents[5][3] = scent_when + 81;
    monster_senses_refresh(m); CHECK(m->ai.sense.kind == MON_SENSE_NONE);
    races[1].flags3 = RF3_SERPENT; races[1].d_char = 's';
    CHECK(monster_scent_limit(&races[1]) == 0);
    scents[5][3] = scent_when;
    monster_senses_refresh(m); CHECK(m->ai.sense.kind == MON_SENSE_NONE);
}

static void test_local_trails(void)
{
    monster_type* m = reset_map(); int y, x;
    scents[5][3] = scent_when + 8;
    scents[5][4] = scent_when + 7;
    scents[4][3] = scent_when + 9;
    monster_senses_refresh(m);
    CHECK(monster_senses_advance(m, &y, &x) && y == 5 && x == 4);
    move(m, y, x); monster_senses_refresh(m);
    CHECK(m->ai.sense.stale_decisions == 0 && m->ai.sense.anchor_x == 4);
    /* Unreachable fresh scent cannot pull the actor through a wall. */
    tile(5, 5, FEAT_WALL_PERM); scents[5][5] = scent_when;
    CHECK(monster_senses_advance(m, &y, &x));
    CHECK(!(y == 5 && x == 5));
    CHECK(m->ai.sense.kind == MON_SENSE_SEARCH);

    m = reset_map();
    for (int yy = 1; yy < 14; yy++) for (int xx = 1; xx < 11; xx++)
        scents[yy][xx] = scent_when + 10;
    monster_senses_refresh(m);
    for (int n = 0; n < 8; n++)
    {
        CHECK(monster_senses_advance(m, &y, &x));
        move(m, y, x);
        /* Player actions age this exact trail, rather than renewing it. */
        playerturn++; scent_when--;
        monster_senses_refresh(m);
    }
    CHECK(m->ai.sense.stale_decisions == 8);
    for (int n = 0; n < 9; n++)
    {
        bool found = monster_senses_advance(m, &y, &x);
        if (found)
        {
            CHECK(distance(y, x, m->ai.sense.anchor_y, m->ai.sense.anchor_x) <= 2);
            move(m, y, x);
        }
        monster_senses_refresh(m);
    }
    CHECK(m->ai.sense.kind == MON_SENSE_NONE);
    CHECK(!monster_senses_target(m, &y, &x));
}

static void test_hearing_and_recognition(void)
{
    monster_type* m = reset_map();
    update_flow(5, 3, FLOW_MONSTER_NOISE);
    monster_perception(false, false, -100);
    CHECK(m->ai.sense.kind == MON_SENSE_NONE);
    m->alertness = ALERTNESS_ALERT;
    update_flow(5, 6, FLOW_PLAYER_NOISE);
    monster_perception(true, false, -100);
    CHECK(m->ai.sense.kind == MON_SENSE_SOUND);
    CHECK(m->ai.sense.y == 5 && m->ai.sense.x == 6);
    CHECK(m->ai.sense.x != p_ptr->px);
    monster_senses_share_trace(m, 3, 3);
    CHECK(m->ai.sense.kind == MON_SENSE_SOUND && m->ai.sense.x == 6);
    memset(&m->ai.sense, 0, sizeof(m->ai.sense));
    monster_perception(true, false, 1000);
    CHECK(m->ai.sense.kind == MON_SENSE_NONE);
    for (int yy = 1; yy < 14; yy++) tile(yy, 11, FEAT_FLOOR);
    CHECK(monster_has_sight(m));
    visual_recognition = true; races[1].flags2 = RF2_SMART;
    CHECK(!monster_has_sight(m));
    p_ptr->cur_light = 30;
    CHECK(monster_has_sight(m));
    monster_senses_refresh(m); CHECK(m->ai.sense.kind == MON_SENSE_SIGHT);
    monster_senses_hear(m, 5, 6); CHECK(m->ai.sense.kind == MON_SENSE_SIGHT);
    monster_senses_share_trace(m, 3, 3); CHECK(m->ai.sense.kind == MON_SENSE_SIGHT);
    races[1].flags2 |= RF2_SHORT_SIGHTED;
    CHECK(!monster_has_sight(m));
}

static void test_known_target_pursuit(void)
{
    monster_type* m = reset_map(); int y = -1, x = -1;
    get_move_advance(m, &y, &x);
    CHECK(y == m->fy && x == m->fx);
    /* A recorded sound is investigated even after its unseen source moves. */
    monster_senses_hear(m, 5, 6);
    playerturn++; p_ptr->py = 12; p_ptr->px = 18;
    get_move_advance(m, &y, &x);
    CHECK(x > m->fx && flow_center_y[1] == 5 && flow_center_x[1] == 6);
    CHECK(m->ai.sense.y == 5 && m->ai.sense.x == 6);
    move(m, 5, 6);
    get_move_advance(m, &y, &x);
    CHECK(m->ai.sense.kind == MON_SENSE_SEARCH);
    CHECK(distance(y, x, 5, 6) <= 2);

    m = reset_map();
    scents[5][3] = scent_when + 1; scents[5][4] = scent_when;
    /* Scent points at a player square that recognition cannot identify. */
    occupants[5][18] = 0; occupants[5][4] = -1;
    p_ptr->py = 5; p_ptr->px = 4;
    visual_recognition = true; races[1].flags2 = RF2_SMART;
    p_ptr->cur_light = -20;
    get_move_advance(m, &y, &x);
    CHECK(!(y == 5 && x == 4));
    CHECK(!monster_has_sight(m));
}

static void test_deposition_and_roundtrip(void)
{
    reset_map(); p_ptr->py = 7; p_ptr->px = 6;
    tile(7, 6, FEAT_ICE); update_smell(); CHECK(get_scent(7, 6) == 0);
    tile(7, 6, FEAT_POISON); update_smell(); CHECK(get_scent(7, 6) == 0);
    tile(7, 6, FEAT_LAVA); update_smell(); CHECK(get_scent(7, 6) == 0);
    byte value = scent_export_cell(7, 6); CHECK(value == 1);
    scent_restore_begin(); CHECK(get_scent(7, 6) == -1);
    scent_restore_cell(7, 6, value); CHECK(get_scent(7, 6) == 0);
    scent_restore_cell(7, 5, 81); CHECK(get_scent(7, 5) == 80);
    scent_restore_cell(7, 4, 82); CHECK(get_scent(7, 4) == -1);
    tile(7, 5, FEAT_WATER); scent_restore_cell(7, 5, 1);
    CHECK(get_scent(7, 5) == -1);
    scent_when = 1; scents[7][4] = 1 + 20;
    p_ptr->leaping = true; update_smell(); CHECK(get_scent(7, 4) == 21);
    CHECK(get_scent(7, 6) == -1); /* old pre-wrap invalid data disappears */

    reset_map(); p_ptr->py = 7; p_ptr->px = 6;
    p_ptr->leaping = true; update_smell(); CHECK(get_scent(7, 6) == -1);
    p_ptr->leaping = false; tile(7, 6, FEAT_WATER);
    update_smell(); CHECK(get_scent(7, 6) == -1 && get_scent(7, 5) == -1);
    tile(7, 6, FEAT_FLOOR); tile(7, 7, FEAT_WATER);
    update_smell(); CHECK(get_scent(7, 8) == -1);
    CHECK(get_scent(8, 7) == -1); /* diagonal bank corner */
    p_ptr->py = 3; p_ptr->px = 18; update_smell();
    CHECK(get_scent(3, 18) == 0 && get_scent(5, 13) == -1);
}

static void test_flow_targets_and_poison_merge(void)
{
    monster_type* m = reset_map();
    m->target_y = 3; m->target_x = 4;
    monsters[2] = *m; monsters[2].fy = 5; monsters[2].fx = 6;
    monsters[2].target_y = 2; monsters[2].target_x = 7;
    occupants[5][6] = 2;
    update_flow(5, 8, FLOW_PLAYER_NOISE);
    CHECK(m->target_y == 3 && monsters[2].target_y == 2);
    update_flow(5, 8, 1);
    CHECK(m->target_x == 4 && monsters[2].target_x == 7);

    m = reset_map();
    for (int y = 1; y < 14; y++) for (int x = 1; x < 21; x++)
        tile(y, x, FEAT_WALL_PERM);
    for (int x = 2; x <= 18; x++) tile(10, x, FEAT_FLOOR);
    for (int y = 3; y <= 10; y++)
    { tile(y, 8, FEAT_FLOOR); tile(y, 18, FEAT_FLOOR); }
    for (int x = 8; x <= 18; x++) tile(3, x, FEAT_FLOOR);
    tile(10, 4, FEAT_POISON); tile(10, 5, FEAT_POISON);
    tile(10, 12, FEAT_POISON); tile(10, 13, FEAT_POISON);
    occupants[p_ptr->py][p_ptr->px] = 0; p_ptr->py = 10; p_ptr->px = 18;
    occupants[10][18] = -1; move(m, 10, 2); m->hp = 31;
    update_flow(10, 18, 1);
    CHECK(flow_dist(1, 10, 8) < FLOW_MAX_DIST);
    CHECK(flow_dist(1, 10, 2) < FLOW_MAX_DIST);
    /* Removing the dry branch makes the common prefix plus short branch lethal. */
    tile(6, 8, FEAT_WALL_PERM); update_flow(10, 18, 1);
    CHECK(flow_dist(1, 10, 2) == FLOW_MAX_DIST);
}

int main(void)
{
    test_capabilities_and_acquisition(); test_local_trails();
    test_hearing_and_recognition(); test_deposition_and_roundtrip();
    test_known_target_pursuit();
    test_flow_targets_and_poison_merge();
    printf("Monster sensing: %d production-path checks passed.\n", checks);
    return 0;
}
