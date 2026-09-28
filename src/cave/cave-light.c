#include "angband.h"
#include "externs.h"
#include "cave-light.h"

/* The raw field is a render cache, rebuilt by update_view, not AI knowledge.
 * Observation below requires both a computed cell and the observer's LOS.
 * Keeping pre-clamp values makes overlapping shadows marginally correct. */
static s16b raw_light[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s16b raw_darkness_delta[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

int cave_light_contribution(int light, bool glow, int sy, int sx, int y, int x)
{
    int d, radius = ABS(light);
    if (!radius || !in_bounds(sy, sx) || !in_bounds(y, x))
        return 0;
    d = distance(sy, sx, y, x);
    if (d > radius || !los(sy, sx, y, x))
        return 0;
    /* GLOW replaces this emitter's centre, including negative emitters. */
    if (glow && !d)
        return 1;
    return light > 0 ? radius + 1 - d : -(radius + 1 - d);
}

void cave_light_remember_raw(int y, int x, int value, int darkness_delta)
{
    if (in_bounds(y, x))
    {
        raw_light[y][x] = value;
        raw_darkness_delta[y][x] = darkness_delta;
    }
}

void cave_light_remember_darkening(int y, int x, int darkness_delta)
{
    if (in_bounds(y, x))
        raw_darkness_delta[y][x] += darkness_delta;
}

static int adjusted_light(int raw, int delta)
{
    return delta ? MAX(-5, raw - delta) : raw;
}

bool cave_light_observed_change(const monster_type* observer, int y, int x,
    int old_contribution, int new_contribution, int* before, int* after)
{
    if (!observer || !in_bounds(y, x)
        || !(cave_info[y][x] & CAVE_VIEW)
        || !los(observer->fy, observer->fx, y, x))
        return false;
    *before = adjusted_light(raw_light[y][x], raw_darkness_delta[y][x]);
    *after = adjusted_light(raw_light[y][x] - old_contribution + new_contribution,
        raw_darkness_delta[y][x]);
    return true;
}
