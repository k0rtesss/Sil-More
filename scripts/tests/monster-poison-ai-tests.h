/* Included by monster-ai-test.c after its map fixtures. */
#include "melee/melee-movement.h"

static void poison_corridor(int length)
{
    for (int y = 1; y < 6; y++)
        for (int x = 1; x < 10; x++)
            tile(y, x, y == 3 ? FEAT_FLOOR : FEAT_WALL_PERM);
    for (int x = 3; x < 3 + length; x++) tile(3, x, FEAT_POISON);
}

static void test_poison_ai(void)
{
    bool bash = false;
    int y, x;
    monster_type* m = reset_map(7, 11, 3, 2, 3, 8);
    races[1].flags2 = 0;
    tile(3, 3, FEAT_POISON);
    CHECK(cave_exist_mon(&races[1], 3, 3, false, false));
    CHECK(cave_passable_mon(m, 3, 3, &bash) == 100);
    CHECK(monster_step_cost(m, 3, 2, 3, 3) == 4);
    CHECK(monster_poison_step_damage(m, 3, 2, 3, 3) == 12);
    CHECK(monster_terrain_penalty(m, 3, 3) > 0);
    update_flow(3, 8, 1);
    y = m->fy; x = m->fx;
    get_move_advance(m, &y, &x);
    CHECK(features[y][x] != FEAT_POISON); /* Ordinary monsters detour. */

    races[1].flags3 = RF3_RES_POIS;
    m->hp = 1;
    CHECK(monster_step_cost(m, 3, 2, 3, 3) == 1);
    CHECK(monster_terrain_penalty(m, 3, 3) == 0);
    races[1].flags3 = 0; races[1].flags2 = RF2_FLYING;
    CHECK(monster_step_cost(m, 3, 2, 3, 3) == 1);
    CHECK(monster_poison_step_damage(m, 3, 2, 3, 3) == 0);
    races[1].flags2 = 0;
    CHECK(!cave_passable_mon(m, 3, 3, &bash));
    m->confused = 5;
    CHECK(cave_passable_mon(m, 3, 3, &bash) == 100);
    m->confused = 0;

    m = reset_map(7, 11, 3, 2, 3, 8);
    poison_corridor(2);
    m->hp = 19;
    update_flow(3, 8, 1);
    CHECK(flow_dist(1, 3, 2) < FLOW_MAX_DIST); /* Two tiles cost 18 total. */
    y = m->fy; x = m->fx;
    get_move_advance(m, &y, &x);
    CHECK(y == 3 && x == 3);
    m->hp = 18;
    update_flow(3, 8, 1);
    CHECK(flow_dist(1, 3, 2) == FLOW_MAX_DIST);
    m->hp = 25; m->poisoned = 7;
    update_flow(3, 8, 1);
    CHECK(flow_dist(1, 3, 2) == FLOW_MAX_DIST); /* Existing debt counts. */

    m->hp = 16; m->poisoned = 9; /* HP19, two doses, then a ceil(12/5) tick. */
    move_monster(m, 3, 3);
    CHECK(monster_poison_step_damage(m, 3, 3, 3, 4) == 6);
    CHECK(get_move_escape_poison(m, &y, &x));
    CHECK(y == 3 && x == 4); /* Continue survivable crossing, no oscillation. */
    m->hp = 7; m->poisoned = 10;
    CHECK(get_move_escape_poison(m, &y, &x));
    CHECK(y == 3 && x == 2); /* Doomed creatures can still reach land. */

    m = reset_map(9, 11, 4, 4, 4, 5);
    races[1].flags2 = 0; /* No SMART or pack flag needed. */
    tile(4, 4, FEAT_POISON);
    CHECK(get_move_escape_poison(m, &y, &x));
    CHECK(features[y][x] != FEAT_POISON && occupants[y][x] == 0);
    races[1].flags2 = RF2_MINDLESS;
    CHECK(get_move_escape_poison(m, &y, &x));
    races[1].flags2 = 0; races[1].freq_ranged = 100;
    m->min_range = m->best_range = 4;
    CHECK(get_move_escape_poison(m, &y, &x));
    races[1].flags1 = RF1_NEVER_MOVE;
    CHECK(!get_move_escape_poison(m, &y, &x));
    races[1].flags1 = 0; races[1].flags3 = RF3_RES_POIS;
    CHECK(!get_move_escape_poison(m, &y, &x));
    races[1].flags3 = 0; m->confused = 1;
    CHECK(!get_move_escape_poison(m, &y, &x));

    m = reset_map(11, 11, 5, 3, 5, 5);
    tile(5, 4, FEAT_POISON);
    tactical_context context = { .actor = m, .py = p_ptr->py,
        .px = p_ptr->px, .original_distance = m->cdis };
    CHECK(tactical_position_score(&context, 5, 4)
        < tactical_position_score(&context, 4, 4));
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(features[y][x] != FEAT_POISON);
}
