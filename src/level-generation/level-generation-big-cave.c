/* File: level-generation-big-cave.c */

#include "angband.h"
#include "level-generation-internal.h"

void big_cave_type_rules_clear(void)
{
    for (int d = 0; d < 32; ++d)
    {
        g_big_cave_type_rule_set[d] = false;
        for (int t = 0; t < BIG_CAVE_TYPE_MAX; ++t)
            g_big_cave_type_weight[d][t] = 0;
    }
}

void big_cave_type_set_rule(int depth, int ice_weight, int fire_weight,
    int pois_weight)
{
    if (depth < 0 || depth >= 32)
        return;

    g_big_cave_type_rule_set[depth] = true;
    g_big_cave_type_weight[depth][BIG_CAVE_ICE] = MAX(0, ice_weight);
    g_big_cave_type_weight[depth][BIG_CAVE_FIRE] = MAX(0, fire_weight);
    g_big_cave_type_weight[depth][BIG_CAVE_POIS] = MAX(0, pois_weight);
}

big_cave_type_t big_cave_type_pick_for_depth(int depth)
{
    int d = depth;
    int ice_w = 1;
    int fire_w = 1;
    int pois_w = 1;
    int total;
    int r;

    if (d < 0)
        d = 0;
    if (d >= 32)
        d = 31;

    if (g_big_cave_type_rule_set[d])
    {
        ice_w = g_big_cave_type_weight[d][BIG_CAVE_ICE];
        fire_w = g_big_cave_type_weight[d][BIG_CAVE_FIRE];
        pois_w = g_big_cave_type_weight[d][BIG_CAVE_POIS];
    }

    total = ice_w + fire_w + pois_w;
    if (total <= 0)
    {
        ice_w = 1;
        fire_w = 1;
        pois_w = 1;
        total = 3;
    }

    r = rand_int(total);
    if (r < ice_w)
        return BIG_CAVE_ICE;
    r -= ice_w;
    if (r < fire_w)
        return BIG_CAVE_FIRE;

    return BIG_CAVE_POIS;
}
