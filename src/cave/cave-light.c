#include "angband.h"
#include "cave-light.h"
#include "cave.h"
#include "cave-bridge.h"
#include "level-generation/level-state.h"

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

/* Private scratch reproduces the ordinary noise field's door costs and
 * expansion order. weapon_glows() itself also changes a live flow and checks
 * the previous player's view, neither of which belongs in a borrowed view. */
static bool cached_weapon_glows(const level_image* floor, const object_type* object)
{
    static _Thread_local byte cost[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static _Thread_local int queue[2][MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int cy = object->iy, cx = object->ix, count = 1, cycle = 0, hate = 0;
    u32b f1, f2, f3, f4;
    if (wield_slot(object) != INVEN_WIELD) return false;
    object_flags4(object, &f1, &f2, &f3, &f4);
    if (!(f1 & (TR1_SLAY_WOLF | TR1_SLAY_SPIDER | TR1_SLAY_UNDEAD
            | TR1_SLAY_RAUKO | TR1_SLAY_ORC | TR1_SLAY_TROLL | TR1_SLAY_DRAGON))
        && !(f4 & (TR4_SLAY_SERPENT | TR4_SLAY_VAMPIRE | TR4_SLAY_HORROR
            | TR4_SLAY_CAT | TR4_SLAY_GIANT))) return false;
    SDL_memset(cost, FLOW_MAX_DIST, sizeof(cost));
    cost[cy][cx] = 0; queue[0][0] = cy * MAX_DUNGEON_WID + cx;
    for (int step = 1; step <= FLOW_MAX_DIST && count; step++) {
        int next_count = 0, next_cycle = 1 - cycle;
        for (int i = 0; i < count; i++) {
            int cell = queue[cycle][i], y = cell / MAX_DUNGEON_WID, x = cell % MAX_DUNGEON_WID;
            if (cost[y][x] >= step) { queue[next_cycle][next_count++] = cell; continue; }
            for (int d = 0; d < 8; d++) {
                int yy = y + ddy_ddd[d], xx = x + ddx_ddd[d];
                if (yy < 0 || xx < 0 || yy >= floor->hgt || xx >= floor->wid
                    || cost[yy][xx] < FLOW_MAX_DIST) continue;
                int feat = floor->feat[yy][xx];
                if (FEAT_IS_WALL(feat) && feat != FEAT_SECRET) continue;
                bool door = (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
                    || feat == FEAT_SECRET || feat == FEAT_WARDED || feat == FEAT_WARDED2;
                if (step + (door ? 5 : 0) >= FLOW_MAX_DIST) continue;
                cost[yy][xx] = step + (door ? 5 : 0);
                queue[next_cycle][next_count++] = yy * MAX_DUNGEON_WID + xx;
            }
        }
        cycle = next_cycle; count = next_count;
    }
    for (int i = 1; i < floor->mon_max; i++) {
        const monster_type* monster = &floor->monsters[i];
        if (!monster->r_idx
            || monster->fy >= floor->hgt || monster->fx >= floor->wid) continue;
        int multiplier = weapon_glow_multiplier(f1, f4, &r_info[monster->r_idx]);
        hate += 50 * multiplier / MAX(1, cost[monster->fy][monster->fx]);
        if (hate >= 15) return true;
    }
    if (f1 & TR1_SLAY_SPIDER)
        for (int y = MAX(0, cy - 2); y <= MIN(floor->hgt - 1, cy + 2); y++)
            for (int x = MAX(0, cx - 2); x <= MIN(floor->wid - 1, cx + 2); x++)
                if (floor->feat[y][x] == FEAT_TRAP_WEB) hate += 50 / MAX(1, cost[y][x]);
    return hate >= 15;
}

int cave_light_cached_sample(const level_image* floor, int y, int x)
{
    if (!floor || y < 0 || x < 0 || y >= floor->hgt || x >= floor->wid) return 0;
    int value = (floor->info[y][x] & CAVE_GLOW) ? 1 : 0, lava = 0;
    if (floor->info[y][x] & CAVE_CHASM_AREA) value -= 4;
    for (int sy = MAX(0, y - 2); sy <= MIN(floor->hgt - 1, y + 2); sy++)
        for (int sx = MAX(0, x - 2); sx <= MIN(floor->wid - 1, x + 2); sx++) {
            const environment_cell* cell = &floor->cells[sy][sx];
            int feat = floor->feat[sy][sx];
            int underlay = floor->environment.ready && (cell->flags & ENV_BRIDGE)
                && cell->integrity && (FEAT_IS_BRIDGE(feat) || feat == cell->bridge_feat)
                ? cell->underlay : cave_bridge_underlay(feat);
            int d = distance(sy, sx, y, x);
            if (underlay == FEAT_LAVA && d <= 2 && cave_los_info(floor->info, sy, sx, y, x))
                lava = MAX(lava, 3 - d);
        }
    value += lava;
    for (int i = 1; i < floor->mon_max; i++) {
        const monster_type* monster = &floor->monsters[i];
        if (!monster->r_idx
            || monster->fy >= floor->hgt || monster->fx >= floor->wid) continue;
        const monster_race* race = &r_info[monster->r_idx];
        int radius = ABS(race->light), d = distance(monster->fy, monster->fx, y, x);
        if (!radius || d > radius || !cave_los_info(floor->info, monster->fy, monster->fx, y, x)) continue;
        value += (race->flags2 & RF2_GLOW) && !d ? 1
            : race->light > 0 ? radius + 1 - d : -(radius + 1 - d);
    }
    for (int i = 1; i < floor->o_max; i++) {
        const object_type* object = &floor->objects[i];
        if (!object->k_idx || object->held_m_idx || object->iy <= 0 || object->ix <= 0
            || object->iy >= floor->hgt - 1 || object->ix >= floor->wid - 1) continue;
        int radius = (object->name1 >= ART_MORGOTH_1 && object->name1 <= ART_MORGOTH_3)
            ? object->pval : 0;
        int d = distance(object->iy, object->ix, y, x);
        if (d > MAX(1, radius) || !cave_los_info(floor->info, object->iy, object->ix, y, x)) continue;
        if (cached_weapon_glows(floor, object)) radius++;
        if (radius > 0 && d <= radius) value += radius + 1 - d;
    }
    return value;
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
