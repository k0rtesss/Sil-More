#include "angband.h"
#include "level-generation/level-stream.h"
#include "level-generation/level-generation-internal.h"
#include "cave/cave-water-flow.h"
#include "cave/cave-bridge.h"
#include "cave/cave-environment.h"

/* PURE_GEOGRAPHY_BEGIN
 * World geology has no allocation, mutable state, RNG, or knowledge of the
 * active window. Each 256-tile watershed contains a channel whose control
 * points are 128 tiles apart. Adjacent samples share the same control point,
 * including across zero. Material and downstream orientation belong to the
 * whole watershed, so re-entry cannot reverse a current or change its acid
 * into water. Lakes widen the channel around selected control points.
 */
static u64b stream_mix(u64b n)
{
    n = (n ^ (n >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    n = (n ^ (n >> 27)) * UINT64_C(0x94d049bb133111eb);
    return n ^ (n >> 31);
}

u64b level_stream_hash(u64b seed, int depth, s64b y, s64b x, unsigned domain)
{
    u64b h = stream_mix(seed ^ UINT64_C(0x9e3779b97f4a7c15));
    h = stream_mix(h ^ (u64b)(s64b)depth);
    h = stream_mix(h ^ (u64b)y);
    h = stream_mix(h ^ (u64b)x);
    return stream_mix(h ^ ((u64b)domain << 32) ^ domain);
}

s64b level_stream_floor_div(s64b value, int divisor)
{
    if (divisor <= 0) return 0;
    s64b quotient = value / divisor;
    return quotient - (value % divisor < 0 ? 1 : 0);
}

static int stream_material(u64b h, int depth)
{
    int roll = (int)(h % 100);
    if (depth >= 14 && roll < 18) return FEAT_LAVA;
    if (depth >= 10 && roll < 35) return FEAT_POISON;
    if (depth >= 7 && roll >= 80) return FEAT_CHASM;
    return FEAT_WATER;
}

static int stream_channel_offset(u64b seed, int depth, s64b lane, s64b node)
{
    return 128 + (int)(level_stream_hash(seed, depth, lane, node, 101) % 97) - 48;
}

static bool stream_geography_sample(u64b seed, int depth, s64b y, s64b x,
    int* feature, bool* lake, bool* vertical)
{
    bool v = (level_stream_hash(seed, depth, 0, 0, 100) & 1) != 0;
    s64b along = v ? y : x, across = v ? x : y;
    s64b lane = level_stream_floor_div(across, 256);
    s64b node = level_stream_floor_div(along, 128);
    int t = (int)(along % 128);
    int cross = (int)(across % 256);
    if (t < 0) t += 128;
    if (cross < 0) cross += 256;
    int a = stream_channel_offset(seed, depth, lane, node);
    int b = stream_channel_offset(seed, depth, lane, node + 1);
    int center = a + (b - a) * t / 128;
    u64b watershed = level_stream_hash(seed, depth, lane, 0, 102);
    int f = stream_material(watershed, depth);
    int radius = f == FEAT_CHASM ? 1 + (int)(watershed % 2)
        : 2 + (int)(watershed % 3);
    bool basin = false;
    /* The same node owns both halves of its lake. Ellipses are comfortably
     * inside the watershed, avoiding overlap with incompatible materials. */
    if (f != FEAT_CHASM)
    {
        for (int side = 0; side < 2; ++side)
        {
            s64b n = node + side;
            u64b h = level_stream_hash(seed, depth, lane, n, 103);
            if (h % 4 != 0) continue;
            int da = t - side * 128;
            int dc = cross - (side ? b : a);
            int ra = 14 + (int)((h >> 8) % 16);
            int rc = 8 + (int)((h >> 16) % 10);
            if (da * da * rc * rc + dc * dc * ra * ra <= ra * ra * rc * rc)
                basin = true;
        }
    }
    if (!basin && abs(cross - center) > radius) return false;
    *feature = basin && f == FEAT_WATER && abs(cross - center) > radius
        ? FEAT_DEEP_WATER : f;
    *lake = basin;
    *vertical = v;
    return true;
}

bool level_stream_major_feature(u64b seed, int depth, s64b y, s64b x,
    int* feature, int* direction)
{
    int f = 0, d = CAVE_WATER_FLOW_CALM;
    bool lake = false, vertical = false;
    if (!stream_geography_sample(seed, depth, y, x, &f, &lake, &vertical))
    {
        if (feature) *feature = 0;
        if (direction) *direction = CAVE_WATER_FLOW_CALM;
        return false;
    }
    if (!lake && (f == FEAT_WATER || f == FEAT_POISON))
    {
        int nf;
        bool nl, nv;
        s64b across = vertical ? x : y;
        s64b lane = level_stream_floor_div(across, 256);
        int sign = (level_stream_hash(seed, depth, lane, 0, 104) & 1) ? 1 : -1;
        /* Direction is fixed for a watershed, rather than inferred from a
         * local edge. Neighbouring watersheds can drain opposite ways. */
        bool addressable = sign > 0 ? (vertical ? y : x) != INT64_MAX
            : (vertical ? y : x) != INT64_MIN;
        if (addressable
            && stream_geography_sample(seed, depth,
                y + (vertical ? sign : 0), x + (vertical ? 0 : sign), &nf, &nl, &nv))
            d = vertical ? (sign > 0 ? CAVE_WATER_FLOW_SOUTH : CAVE_WATER_FLOW_NORTH)
                : (sign > 0 ? CAVE_WATER_FLOW_EAST : CAVE_WATER_FLOW_WEST);
        else if (addressable)
        {
            int local = (int)(across % 256);
            if (local < 0) local += 256;
            s64b along = vertical ? y : x;
            s64b node = level_stream_floor_div(along, 128);
            int t = (int)(along % 128);
            if (t < 0) t += 128;
            int a = stream_channel_offset(seed, depth, lane, node);
            int b = stream_channel_offset(seed, depth, lane, node + 1);
            int center = a + (b - a) * t / 128;
            int step = local < center ? 1 : -1;
            if (local != center)
                d = vertical ? (step > 0 ? CAVE_WATER_FLOW_EAST : CAVE_WATER_FLOW_WEST)
                    : (step > 0 ? CAVE_WATER_FLOW_SOUTH : CAVE_WATER_FLOW_NORTH);
        }
    }
    if (feature) *feature = f;
    if (direction) *direction = d;
    return true;
}
/* PURE_GEOGRAPHY_END */

bool level_stream_partition_bounds(int pi, int* y1, int* y2, int* x1, int* x2)
{
    if (pi < 0 || pi >= LEVEL_STREAM_GRID * LEVEL_STREAM_GRID) return false;
    int y = 1 + (pi / LEVEL_STREAM_GRID) * LEVEL_STREAM_PART_SIZE;
    int x = 1 + (pi % LEVEL_STREAM_GRID) * LEVEL_STREAM_PART_SIZE;
    if (y1) *y1 = y;
    if (y2) *y2 = y + LEVEL_STREAM_PART_SIZE - 1;
    if (x1) *x1 = x;
    if (x2) *x2 = x + LEVEL_STREAM_PART_SIZE - 1;
    return true;
}

static bool stream_protected(int y, int x)
{
    return (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT))
        || cave_m_idx[y][x] || cave_o_idx[y][x];
}

static void stream_geography_bounds(int y1, int y2, int x1, int x2)
{
    level_stream_frame frame = level_stream_get_frame();
    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
        {
            int feat, flow;
            if (!in_bounds_fully(y, x) || stream_protected(y, x)) continue;
            if (!level_stream_major_feature(frame.seed, p_ptr->depth,
                    frame.y + y - 1, frame.x + x - 1, &feat, &flow)) continue;
            /* A later pass can reassert the channel without removing its
             * intentional crossing or destroying stairs. */
            if (!cave_stair_bold(y, x) && !cave_feat_is_bridge(cave_feat[y][x]))
            {
                cave_set_feat(y, x, feat);
                cave_natural[y][x] = 1;
            }
            terrain_generation_reserve(y, x);
            cave_water_flow_restore_cell(y, x,
                (byte)(flow | ((feat == FEAT_DEEP_WATER
                    || (flow == 0 && (feat == FEAT_WATER || feat == FEAT_POISON))) ? 8 : 0)));
        }
}

static void stream_currents(int y1, int y2, int x1, int x2)
{
    level_stream_frame frame = level_stream_get_frame();
    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
        {
            int f, d;
            if (stream_protected(y, x) || !level_stream_major_feature(frame.seed,
                p_ptr->depth, frame.y + y - 1, frame.x + x - 1, &f, &d)) continue;
            if (d)
            {
                int ny = y + (d == CAVE_WATER_FLOW_NORTH ? -1 : d == CAVE_WATER_FLOW_SOUTH ? 1 : 0);
                int nx = x + (d == CAVE_WATER_FLOW_WEST ? -1 : d == CAVE_WATER_FLOW_EAST ? 1 : 0);
                if (in_bounds_fully(ny, nx))
                {
                    int other = cave_bridge_underlay(cave_feat[ny][nx]);
                    if (other != f && !(f == FEAT_WATER && other == FEAT_DEEP_WATER)) d = 0;
                }
            }
            cave_water_flow_restore_cell(y, x, (byte)(d
                | ((!d && (f == FEAT_WATER || f == FEAT_DEEP_WATER || f == FEAT_POISON)) ? 8 : 0)));
        }
}

void level_stream_foundation(void)
{
    if (!level_stream_enabled()) return;
    stream_geography_bounds(1, p_ptr->cur_map_hgt - 2,
        1, p_ptr->cur_map_wid - 2);
    stream_currents(1, p_ptr->cur_map_hgt - 2, 1, p_ptr->cur_map_wid - 2);
    /* restore_cell() deliberately permits updates without resetting the
     * rest of the table, which matters during strip generation. */
    cave_water_flow_restore_finish();
}

static void stream_simple_room(int y1, int y2, int x1, int x2, int style)
{
    int h = rand_range(5, 10), w = rand_range(7, 14);
    int sy = rand_range(y1 + 2, y2 - h - 2);
    int sx = rand_range(x1 + 2, x2 - w - 2);
    int ey = sy + h, ex = sx + w;
    for (int y = sy - 1; y <= ey + 1; ++y)
        for (int x = sx - 1; x <= ex + 1; ++x)
            if (stream_protected(y, x) || cave_feat[y][x] != FEAT_WALL_EXTRA) return;
    bool cross = one_in_(3);
    for (int y = sy; y <= ey; ++y)
        for (int x = sx; x <= ex; ++x)
        {
            if (cross && abs(y - (sy + ey) / 2) > 1
                && abs(x - (sx + ex) / 2) > 2) continue;
            cave_set_feat_style(y, x, FEAT_FLOOR, style);
            cave_info[y][x] |= CAVE_ROOM;
        }
    if (dun->cent_n < room_capacity_limit())
    {
        int i = dun->cent_n++;
        dun->cent[i] = (coord){(sy + ey) / 2, (sx + ex) / 2};
        dun->corner[i] = (rectangle){sy, sx, ey, ex};
    }
}

static void stream_crossing(int y, int x, bool vertical, int style)
{
    if (stream_protected(y, x)) return;
    int feat = cave_feat[y][x], under = cave_bridge_underlay(feat);
    if (cave_feat_is_bridge(feat) || cave_stair_bold(y, x)) return;
    int bridge = under == FEAT_WATER ? FEAT_BRIDGE_WATER_H
        : under == FEAT_DEEP_WATER ? FEAT_BRIDGE_DEEP_WATER_H
        : under == FEAT_POISON ? FEAT_BRIDGE_POISON_H
        : under == FEAT_LAVA ? FEAT_BRIDGE_LAVA_H
        : under == FEAT_CHASM ? FEAT_BRIDGE_CHASM_H
        : FEAT_IS_ICE(under) ? FEAT_BRIDGE_ICE_H : 0;
    if (bridge) cave_set_feat_style(y, x, bridge + (vertical ? 1 : 0), style);
    else if (feat == FEAT_WALL_EXTRA || feat == FEAT_WALL_OUTER
        || feat == FEAT_WALL_INNER || feat == FEAT_WALL_SOLID
        || FEAT_IS_QUARTZ(feat) || feat == FEAT_RUBBLE)
        cave_set_feat_style(y, x, FEAT_FLOOR, style);
}

static bool stream_route_tile(int y, int x)
{
    if (!stream_protected(y, x)) return cave_feat[y][x] != FEAT_WALL_PERM;
    /* An authored open passage is available to walking, while its cells and
     * walls remain immutable. This also connects gates inside a partially
     * resident site through its real entrance rather than cutting its wall. */
    int f = cave_feat[y][x];
    return f == FEAT_FLOOR || cave_feat_is_bridge(f) || cave_stair_bold(y, x)
        || f == FEAT_OPEN || f == FEAT_BROKEN || f == FEAT_GLYPH;
}

/* Bounded breadth-first paths avoid every authored/protected tile. They may
 * bridge ancient terrain but never overwrite it with a dry straight cut. */
static bool stream_path(int sy, int sx, int ty, int tx,
    int y1, int y2, int x1, int x2, int style)
{
    enum { SIDE = LEVEL_STREAM_PART_SIZE, AREA = SIDE * SIDE };
    s16b parent[AREA], queue[AREA];
    static const int dy[4] = {-1, 0, 1, 0};
    static const int dx[4] = {0, 1, 0, -1};
    if (sy < y1 || sy > y2 || sx < x1 || sx > x2
        || ty < y1 || ty > y2 || tx < x1 || tx > x2
        || !stream_route_tile(sy, sx) || !stream_route_tile(ty, tx)) return false;
    for (int i = 0; i < AREA; ++i) parent[i] = -1;
    int start = (sy - y1) * SIDE + sx - x1;
    int target = (ty - y1) * SIDE + tx - x1;
    int head = 0, tail = 0;
    parent[start] = start;
    queue[tail++] = start;
    while (head < tail && parent[target] < 0)
    {
        int pos = queue[head++];
        int y = y1 + pos / SIDE, x = x1 + pos % SIDE;
        for (int d = 0; d < 4; ++d)
        {
            int yy = y + dy[d], xx = x + dx[d];
            if (yy < y1 || yy > y2 || xx < x1 || xx > x2
                || !stream_route_tile(yy, xx)) continue;
            int next = (yy - y1) * SIDE + xx - x1;
            if (parent[next] >= 0) continue;
            parent[next] = pos;
            queue[tail++] = next;
        }
    }
    if (parent[target] < 0) return false;
    int pos = target;
    while (pos != start)
    {
        int prev = parent[pos];
        stream_crossing(y1 + pos / SIDE, x1 + pos % SIDE,
            prev / SIDE != pos / SIDE, style);
        pos = prev;
    }
    stream_crossing(sy, sx, true, style);
    return true;
}

static void stream_links(int pi, int y1, int y2, int x1, int x2, int style)
{
    level_stream_frame frame = level_stream_get_frame();
    s64b py = level_stream_floor_div(frame.y + y1 - 1, LEVEL_STREAM_PART_SIZE);
    s64b px = level_stream_floor_div(frame.x + x1 - 1, LEVEL_STREAM_PART_SIZE);
    int cy = (y1 + y2) / 2, cx = (x1 + x2) / 2;
    int best = 99999, middle_y = cy, middle_x = cx;
    for (int y = y1 + 1; y < y2; ++y)
        for (int x = x1 + 1; x < x2; ++x)
        {
            int score = abs(y - middle_y) + abs(x - middle_x);
            if (!stream_protected(y, x) && cave_floor_bold(y, x)
                && cave_feat[y][x] != FEAT_CHASM && score < best)
            { cy = y; cx = x; best = score; }
        }
    if (stream_protected(cy, cx)) return;
    int north = x1 + 8 + level_stream_hash(frame.seed, p_ptr->depth, py, px, 111) % 28;
    int south = x1 + 8 + level_stream_hash(frame.seed, p_ptr->depth, py + 1, px, 111) % 28;
    int west = y1 + 8 + level_stream_hash(frame.seed, p_ptr->depth, py, px, 112) % 28;
    int east = y1 + 8 + level_stream_hash(frame.seed, p_ptr->depth, py, px + 1, 112) % 28;
    stream_path(cy, cx, y1, north, y1, y2, x1, x2, style);
    stream_path(cy, cx, y2, south, y1, y2, x1, x2, style);
    stream_path(cy, cx, west, x1, y1, y2, x1, x2, style);
    stream_path(cy, cx, east, x2, y1, y2, x1, x2, style);
    for (int i = 0; i < dun->cent_n; ++i)
    {
        int y = dun->cent[i].y, x = dun->cent[i].x;
        if (y >= y1 && y <= y2 && x >= x1 && x <= x2)
            stream_path(cy, cx, y, x, y1, y2, x1, x2, style);
    }
    (void)pi;
}

static bool stream_scatter_spot(const partition_population_plan* plan, int* y, int* x)
{
    for (int tries = 0; tries < 250; ++tries)
    {
        *y = rand_range(plan->y1, plan->y2);
        *x = rand_range(plan->x1, plan->x2);
        if (!stream_protected(*y, *x)
            && partition_population_naked_bold(plan->mode, *y, *x)) return true;
    }
    return false;
}

static void stream_objects(const partition_population_plan* plan)
{
    /* The level-wide scatter helpers may choose CAVE_ICKY floor, and the
     * skeleton helper uses drop_near(). Use the same profiles/count rules
     * with exact placements so neither protected sites nor neighbours change. */
    partition_drop_profile prof = partition_drop_profile_for_mode_source_cfg(
        plan->mode, PARTITION_DROP_SOURCE_FLOOR);
    if (prof.allow_floor_drops)
        for (int set = 0; set < 2; ++set)
        {
            int count = set ? plan->corr_objects : plan->room_objects;
            for (int n = 0; n < count; ++n)
                for (int tries = 0; tries < 250; ++tries)
                {
                    int y, x;
                    if (!stream_scatter_spot(plan, &y, &x)) break;
                    bool room = (cave_info[y][x] & CAVE_ROOM) != 0;
                    if (plan->mode != QUAD_MODE_CHASM && room == (set != 0)) continue;
                    place_object_with_profile(y, x, &prof);
                    break;
                }
        }
    int metal = partition_metal_drop_target(plan->mode, plan->floor_count, p_ptr->depth);
    s16b kind = partition_metal_kind_for_mode(plan->mode);
    for (int n = 0; n < metal && kind; ++n)
    {
        int y, x;
        if (!stream_scatter_spot(plan, &y, &x)) break;
        object_type object;
        object_prep(&object, kind);
        object.number = 1;
        floor_carry(y, x, &object);
    }
    int skeletons = plan->mode == QUAD_MODE_BIG_CAVE ? MIN(8, MAX(2, plan->floor_count / 40))
        : plan->mode == QUAD_MODE_LABYRINTH ? MIN(6, MAX(2, plan->floor_count / 25))
        : plan->mode == QUAD_MODE_RUINED ? MIN(10, MAX(3, plan->floor_count / 15)) : 0;
    for (int n = 0; n < skeletons; ++n)
    {
        int y, x;
        if (!stream_scatter_spot(plan, &y, &x)) break;
        int roll = rand_int(100);
        int human = plan->mode == QUAD_MODE_RUINED ? 20 : plan->mode == QUAD_MODE_BIG_CAVE ? 35 : 30;
        int elf = plan->mode == QUAD_MODE_RUINED ? 20 : plan->mode == QUAD_MODE_BIG_CAVE ? 35 : 60;
        place_exact_skeleton_at(y, x, roll < human ? SV_SKELETON_HUMAN
            : roll < human + elf ? SV_SKELETON_ELF : SV_SKELETON_ORC);
    }
    for (int n = 0; n < plan->meta.chest_count; ++n)
        for (int tries = 0; tries < 250; ++tries)
        {
            int y, x;
            if (!stream_scatter_spot(plan, &y, &x)) break;
            if (place_partition_chest_at(plan->pi, y, x, &plan->meta.chest_recipes[n],
                    plan->mode, partition_mode_avoids_corridor_spawns(plan->mode))) break;
        }
    if (plan->mode == QUAD_MODE_CHASM)
        for (int n = 0; n < CHASM_WHISPERING_SHADOW_TARGET; ++n)
        {
            int y, x;
            if (!stream_scatter_spot(plan, &y, &x)) break;
            place_vault_monster_token('q', y, x);
        }
}

static void stream_populate(int pi, int y1, int y2, int x1, int x2)
{
    partition_population_plan plan;
    memset(&plan, 0, sizeof(plan));
    plan.pi = pi;
    plan.mode = current_partition_modes[pi];
    plan.cave_type = current_partition_big_cave_types[pi];
    plan.meta = current_partition_population_meta[pi];
    plan.y1 = y1; plan.y2 = y2; plan.x1 = x1; plan.x2 = x2;
    for (int i = 0; i < dun->cent_n; ++i)
        if (dun->cent[i].y >= y1 && dun->cent[i].y <= y2
            && dun->cent[i].x >= x1 && dun->cent[i].x <= x2) ++plan.room_centers;
    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (!stream_protected(y, x) && partition_population_floor_bold(plan.mode, y, x))
            {
                ++plan.floor_count;
                if (cave_info[y][x] & CAVE_ROOM) ++plan.room_floor_count;
                else ++plan.corridor_floor_count;
                ++plan.floor_count_non_icky;
                ++plan.floor_count_non_vault;
            }
    plan.monsters_base = partition_base_monsters_for_mode(plan.mode, plan.room_centers);
    plan.monsters_floor = partition_direct_floor_monsters(plan.mode, plan.floor_count);
    plan.monsters_depth = partition_depth_bonus_monsters(plan.mode, plan.floor_count, p_ptr->depth);
    plan.monsters_precurse = plan.monsters_base + plan.monsters_floor + plan.monsters_depth;
    plan.monsters_total = partition_apply_monster_curse_scale(plan.monsters_precurse);
    partition_object_counts_from_total_monsters(plan.mode, plan.monsters_precurse,
        &plan.room_objects, &plan.corr_objects);
    rebalance_partition_corridor_objects(&plan);
    int placed = 0;
    for (int tries = 0; tries < plan.monsters_total * 100 && placed < plan.monsters_total; ++tries)
    {
        int y, x;
        if (!choose_partition_monster_location(&plan, &y, &x)) break;
        bool ok = partition_mode_uses_monster_pools(plan.mode)
            ? place_partition_pool_monster(&plan, y, x, rand_int(100) < 80, 0)
            : place_monster(y, x, true, false, false);
        if (ok) ++placed;
    }
    stream_objects(&plan);
    /* Ordinary exploration can always find vertical routes, without rolling
     * another quest or greater vault. Existing portals are never replaced. */
    int stairs = p_ptr->depth >= UTUMNO_DEPTH ? 0
        : p_ptr->depth == 0 || p_ptr->depth == MORGOTH_DEPTH ? 1 : 2;
    for (int n = 0; n < stairs; ++n)
        for (int tries = 0; tries < 150; ++tries)
        {
            int y = rand_range(y1 + 2, y2 - 2), x = rand_range(x1 + 2, x2 - 2);
            if (cave_feat[y][x] != FEAT_FLOOR || stream_protected(y, x)) continue;
            cave_set_feat(y, x, p_ptr->depth == 0 || n > 0 ? FEAT_MORE : FEAT_LESS);
            break;
        }
}

void level_stream_connect_partition(int pi)
{
    int y1, y2, x1, x2;
    if (!level_stream_enabled() || !dun
        || !level_stream_partition_bounds(pi, &y1, &y2, &x1, &x2)) return;
    int style = current_partition_bridge_styles[pi];
    if (style < 0) style = style_at_color((y1 + y2) / 2, (x1 + x2) / 2);
    stream_links(pi, y1, y2, x1, x2, style);
}

void level_stream_generate_partition(int pi)
{
    int y1, y2, x1, x2;
    if (!level_stream_enabled() || !dun
        || !level_stream_partition_bounds(pi, &y1, &y2, &x1, &x2)) return;
    /* The caller binds a fresh dun workspace and saves the gameplay RNG.
     * Only the target's metadata and tiles are owned by this function. */
    s16b saved_monster_level = monster_level, saved_object_level = object_level;
    monster_level = object_level = p_ptr->depth;
    size_t costs_size = MAX_WANDERING_GROUPS * sizeof(*cave_cost);
    byte* saved_costs = SDL_malloc(costs_size);
    if (!saved_costs) quit("Unable to preserve neighbouring wandering flows");
    SDL_memcpy(saved_costs, &cave_cost[FLOW_WANDERING_HEAD], costs_size);
    environment_state environment = cave_environment_get_state();
    if (environment.ready)
    {
        environment_state provisional = environment;
        provisional.ready = false;
        cave_environment_restore_state(provisional);
    }
    /* cave_set_feat() clears upstream arrows in its immediate neighbours.
     * Preserve that one-cell halo, which belongs to retained partitions. */
    byte flow_n[LEVEL_STREAM_PART_SIZE], flow_s[LEVEL_STREAM_PART_SIZE];
    byte flow_w[LEVEL_STREAM_PART_SIZE], flow_e[LEVEL_STREAM_PART_SIZE];
    for (int i = 0; i < LEVEL_STREAM_PART_SIZE; ++i)
    {
        flow_n[i] = cave_water_flow_encoded_at(y1 - 1, x1 + i);
        flow_s[i] = cave_water_flow_encoded_at(y2 + 1, x1 + i);
        flow_w[i] = cave_water_flow_encoded_at(y1 + i, x1 - 1);
        flow_e[i] = cave_water_flow_encoded_at(y1 + i, x2 + 1);
    }
    bool protected = false, geology = false;
    level_stream_frame frame = level_stream_get_frame();
    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
        {
            int f, d;
            protected |= stream_protected(y, x);
            geology |= level_stream_major_feature(frame.seed, p_ptr->depth,
                frame.y + y - 1, frame.x + x - 1, &f, &d);
        }
    /* Construction sees the river first. A room may settle on either bank;
     * it cannot claim an untouched all-granite footprint across the channel. */
    stream_geography_bounds(y1, y2, x1, x2);
    int weights[QUAD_MODE_BIG_CAVE + 1], total = 0;
    for (int m = QUAD_MODE_ROOMY; m <= QUAD_MODE_BIG_CAVE; ++m)
    {
        weights[m] = mode_weight_for_depth(m, p_ptr->depth, 20, NULL, 25);
        /* The bounded legacy labyrinth carver places its own stairs. Utumno
         * owns authored branch portals and must not create random exits. */
        if (p_ptr->depth >= UTUMNO_DEPTH && m == QUAD_MODE_LABYRINTH) weights[m] = 0;
        if (geology && (m == QUAD_MODE_CHASM || m == QUAD_MODE_BIG_CAVE)) weights[m] = 0;
        total += weights[m];
    }
    int roll = rand_int(MAX(1, total));
    quadrant_mode_t mode = QUAD_MODE_ROOMY;
    for (int m = 0; m <= QUAD_MODE_BIG_CAVE; ++m)
    {
        if (roll < weights[m]) { mode = m; break; }
        roll -= weights[m];
    }
    if (protected) mode = QUAD_MODE_ROOMY;
    current_partition_modes[pi] = mode;
    current_partition_densities[pi] = (density_level_t)rand_int(3);
    current_partition_big_cave_types[pi] = BIG_CAVE_NONE;
    memset(&current_partition_population_meta[pi], 0, sizeof(current_partition_population_meta[pi]));
    int style = mode == QUAD_MODE_CAVEY
        ? styles_pick_partition_style(p_ptr->depth, PART_STYLE_CA_BLOB)
        : mode == QUAD_MODE_LABYRINTH
        ? styles_pick_partition_style(p_ptr->depth, PART_STYLE_LABYRINTH)
        : mode == QUAD_MODE_CHASM
        ? styles_pick_partition_style(p_ptr->depth, PART_STYLE_CHASM_FLOOR)
        : styles_pick_random_from_level();
    int bridge = styles_pick_partition_style(p_ptr->depth, PART_STYLE_CHASM_BRIDGE);
    current_partition_bridge_styles[pi] = bridge;
    bool carved = false;
    if (!protected)
    {
        if (mode == QUAD_MODE_LABYRINTH)
            carved = carve_labyrinth_bounds(y1 + 1, y2 - 1, x1 + 1, x2 - 1,
                current_partition_densities[pi], style);
        else if (mode == QUAD_MODE_CHASM)
            carved = carve_chasm_with_bridges(y1 + 1, y2 - 1, x1 + 1, x2 - 1, style, bridge);
        else if (mode == QUAD_MODE_BIG_CAVE)
        {
            big_cave_type_t type = big_cave_type_pick_for_depth(p_ptr->depth);
            style = styles_pick_partition_style(p_ptr->depth,
                type == BIG_CAVE_ICE ? PART_STYLE_BIG_CAVE_ICE
                : type == BIG_CAVE_FIRE ? PART_STYLE_BIG_CAVE_FIRE : PART_STYLE_BIG_CAVE_POIS);
            carved = carve_big_cave_bounds(y1 + 1, y2 - 1, x1 + 1, x2 - 1, style, type);
            if (carved) current_partition_big_cave_types[pi] = type;
        }
        else if (mode == QUAD_MODE_CAVEY || mode == QUAD_MODE_RUINED)
            for (int n = 0; n < 8; ++n)
                carved |= mode == QUAD_MODE_CAVEY
                    ? carve_ca_blob_anchor_bounds(y1 + 2, y2 - 2, x1 + 2, x2 - 2, style)
                    : carve_bsp_slice_anchor_bounds(y1 + 2, y2 - 2, x1 + 2, x2 - 2);
    }
    if (!carved && mode >= QUAD_MODE_LABYRINTH)
        current_partition_modes[pi] = mode = QUAD_MODE_ROOMY;
    if (mode == QUAD_MODE_ROOMY || mode == QUAD_MODE_CAVEY || mode == QUAD_MODE_RUINED)
        for (int n = 0; n < 14; ++n) stream_simple_room(y1, y2, x1, x2, style);
    if (!protected && (mode == QUAD_MODE_CAVEY || mode == QUAD_MODE_CHASM || mode == QUAD_MODE_BIG_CAVE))
        scatter_quartz_veins_in_bounds(y1 + 1, y2 - 1, x1 + 1, x2 - 1,
            mode == QUAD_MODE_CHASM ? CAVE_CHASM_AREA : 0);
    if (mode != QUAD_MODE_ROOMY)
    {
        int material = mode == QUAD_MODE_CAVEY ? 100 : 0;
        set_partition_chest_recipe(&current_partition_population_meta[pi], 0,
            mode == QUAD_MODE_BIG_CAVE ? 2 : 0,
            material, 100 - material, 0, PARTITION_CHEST_ANCHOR_ANY);
    }
    stream_geography_bounds(y1, y2, x1, x2);
    level_stream_connect_partition(pi);
    stream_populate(pi, y1, y2, x1, x2);
    stream_currents(y1, y2, x1, x2);
    for (int i = 0; i < LEVEL_STREAM_PART_SIZE; ++i)
    {
        cave_water_flow_restore_cell(y1 - 1, x1 + i, flow_n[i]);
        cave_water_flow_restore_cell(y2 + 1, x1 + i, flow_s[i]);
        cave_water_flow_restore_cell(y1 + i, x1 - 1, flow_w[i]);
        cave_water_flow_restore_cell(y1 + i, x2 + 1, flow_e[i]);
    }
    cave_water_flow_restore_finish();
    monster_level = saved_monster_level;
    object_level = saved_object_level;
    if (environment.ready) cave_environment_restore_state(environment);
    for (int group = 0; group < MAX_WANDERING_GROUPS; ++group)
    {
        int flow = FLOW_WANDERING_HEAD + group;
        const byte* saved = saved_costs + group * sizeof(*cave_cost);
        for (int y = 0; y < MAX_DUNGEON_HGT; ++y)
            for (int x = 0; x < MAX_DUNGEON_WID; ++x)
                if (y < y1 || y > y2 || x < x1 || x > x2
                    || (cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT)))
                    cave_cost[flow][y][x] = saved[y * MAX_DUNGEON_WID + x];
        /* A new group may have rebuilt this shared distance field during
         * placement. Its next actual move rebuilds against the finished map. */
        flow_center_y[flow] = flow_center_x[flow] = 0;
        update_center_y[flow] = update_center_x[flow] = 0;
    }
    SDL_free(saved_costs);
}
