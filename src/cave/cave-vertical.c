#include "angband.h"
#include "cave/cave-vertical.h"
#include "cave/cave-water-flow.h"
#include "cave/cave-environment.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-events.h"
#include "cave/cave-bridge.h"
#include "cave/cave-vertical-actors.h"
#include "level-generation/level-cache.h"
#include "level-generation/level-state.h"
#include "level-generation/level-generation-vertical-vaults.h"
#include "quest/quest-runtime.h"
#include "player/killer.h"

static _Thread_local cave_vertical_state current;
static _Thread_local bool new_down_allowed;
static _Thread_local bool changing_terrain;
static const cave_vertical_portal* portal_at(int y, int x, bool gate);
static void paint_basins(cave_vertical_state* state, int depth, level_image* image);
static int basin_feature(const cave_vertical_basin* b, int dy, int dx, int original, int profile);

static bool freshwater(int feature)
{
    return feature == FEAT_WATER || feature == FEAT_DEEP_WATER;
}

static bool liquid(int feature)
{
    return freshwater(feature) || feature == FEAT_LAVA || feature == FEAT_POISON;
}

static int material_of(int feature)
{
    return freshwater(feature) ? FEAT_WATER : feature;
}

static u32b region_identity(int depth, int y, int x, int material)
{
    u32b id = (u32b)(depth + 1) * 2654435761u ^ (u32b)y * 2246822519u
        ^ (u32b)x * 3266489917u ^ (u32b)material * 668265263u;
    return id ? id : 1;
}

const cave_vertical_state* cave_vertical_get(void) { return &current; }
void cave_vertical_capture(cave_vertical_state* state) { if (state) *state = current; }

void cave_vertical_reset(void)
{
    SDL_memset(&current, 0, sizeof(current));
    current.breath = 12;
    current.last_turn = turn;
    new_down_allowed = false;
    changing_terrain = false;
}

bool cave_vertical_restore(const cave_vertical_state* state)
{
    if (!state || state->portal_count > CAVE_VERTICAL_PORTALS
        || state->basin_count > CAVE_VERTICAL_BASINS || state->breath > 12
        || state->submerged > 1 || state->last_turn < 0 || state->last_turn > turn) return false;
    for (int i = 0; i < state->portal_count; i++) {
        const cave_vertical_portal* p = &state->portals[i];
        if (!p->identity || p->kind < CAVE_VERTICAL_STREAM
            || p->kind > CAVE_VERTICAL_TUBE || p->peer_depth < 1
            || p->peer_depth >= MORGOTH_DEPTH || p->y < 1 || p->x < 1
            || p->y >= MAX_DUNGEON_HGT - 1 || p->x >= MAX_DUNGEON_WID - 1
            || p->peer_y < 1 || p->peer_x < 1
            || p->peer_y >= MAX_DUNGEON_HGT - 1 || p->peer_x >= MAX_DUNGEON_WID - 1
            || p->strength > 3 || p->direction > 9 || p->flags > 63 || p->pending > 4096
            || (p->material != FEAT_WATER && p->material != FEAT_LAVA && p->material != FEAT_POISON)
            || (p->pending_material && p->pending_material != FEAT_WATER
                && p->pending_material != FEAT_LAVA && p->pending_material != FEAT_POISON)
            || p->due < 0) return false;
    }
    for (int i = 0; i < state->basin_count; i++) {
        const cave_vertical_basin* b = &state->basins[i];
        if (!b->identity || b->depth < 1 || b->depth >= MORGOTH_DEPTH
            || b->radius < 1 || b->radius > 8 || b->volume > b->capacity
            || b->normal_volume > b->capacity || b->capacity > 4096
            || b->y < 1 || b->x < 1 || b->y >= MAX_DUNGEON_HGT - 1
            || b->x >= MAX_DUNGEON_WID - 1 || b->hot > 6
            || (b->flags & ~(CAVE_VERTICAL_RISE_WARNED | CAVE_VERTICAL_OVERFLOW_WARNED
                | CAVE_VERTICAL_RESERVOIR | CAVE_VERTICAL_SOURCE | CAVE_VERTICAL_SINK
                | CAVE_VERTICAL_CHANGED))
            || (b->material != FEAT_WATER && b->material != FEAT_LAVA && b->material != FEAT_POISON)) return false;
    }
    current = *state;
    return true;
}

void cave_vertical_generation_begin(bool allow_down)
{
    cave_vertical_reset();
    new_down_allowed = allow_down && p_ptr->depth > 0
        && p_ptr->depth < MORGOTH_DEPTH - 1;
}

static cave_vertical_basin* basin_in(cave_vertical_state* state, u32b identity,
    int depth)
{
    for (int i = 0; i < state->basin_count; i++)
        if (state->basins[i].identity == identity && state->basins[i].depth == depth)
            return &state->basins[i];
    return NULL;
}

static cave_vertical_basin* add_basin(u32b identity, int depth, int y, int x,
    int material, int volume)
{
    cave_vertical_basin* basin = basin_in(&current, identity, depth);
    if (basin || current.basin_count == CAVE_VERTICAL_BASINS) return basin;
    basin = &current.basins[current.basin_count++];
    *basin = (cave_vertical_basin){ .identity = identity, .depth = depth,
        .y = y, .x = x, .volume = volume, .normal_volume = volume,
        .capacity = 144, .material = material, .radius = 3 };
    return basin;
}

void cave_vertical_generation_inherit(const cave_vertical_state* source,
    int source_depth, int target_depth)
{
    if (!source || ABS(source_depth - target_depth) != 1) return;
    for (int i = 0; i < source->portal_count && i < CAVE_VERTICAL_PORTALS; i++) {
        const cave_vertical_portal* from = &source->portals[i];
        if (from->peer_depth != target_depth || from->kind == CAVE_VERTICAL_CONTROL)
            continue;
        bool duplicate = false;
        for (int n = 0; n < current.portal_count; n++)
            duplicate |= current.portals[n].identity == from->identity
                && current.portals[n].peer_depth == source_depth
                && current.portals[n].y == from->peer_y
                && current.portals[n].x == from->peer_x;
        if (duplicate || current.portal_count == CAVE_VERTICAL_PORTALS) continue;
        cave_vertical_portal* to = &current.portals[current.portal_count++];
        *to = *from;
        to->y = from->peer_y; to->x = from->peer_x;
        to->peer_y = from->y; to->peer_x = from->x; to->peer_depth = source_depth;
        to->flags ^= CAVE_VERTICAL_DOWN;
        to->flags &= ~CAVE_VERTICAL_GATE;
        to->local_style = from->peer_style; to->peer_style = from->local_style;
        SDL_memcpy(to->local_patch, from->peer_patch, sizeof(to->local_patch));
        SDL_memcpy(to->peer_patch, from->local_patch, sizeof(to->peer_patch));
        to->pending = 0;
        to->pending_material = 0;
        for (int n = 0; n < source->basin_count; n++) {
            const cave_vertical_basin* b = &source->basins[n];
            if (b->identity != from->identity
                || (b->depth != source_depth && b->depth != target_depth)) continue;
            cave_vertical_basin* copy = add_basin(b->identity, b->depth,
                b->y, b->x, b->material, b->volume);
            if (copy) *copy = *b;
        }
        if (!basin_in(&current, to->identity, target_depth))
            add_basin(to->identity, target_depth, to->y, to->x, to->material, 72);
    }
}

void cave_vertical_generation_extent(int* height, int* width)
{
    for (int i = 0; i < current.portal_count; i++) {
        *height = MAX(*height, current.portals[i].y + 5);
        *width = MAX(*width, current.portals[i].x + 5);
    }
}

void cave_vertical_generation_reserve(void)
{
    for (int i = 0; i < current.portal_count; i++) {
        const cave_vertical_portal* p = &current.portals[i];
        for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++)
            if (p->local_patch[dy + 3][dx + 3] && in_bounds_fully(p->y + dy, p->x + dx))
                terrain_generation_reserve(p->y + dy, p->x + dx);
    }
}

bool cave_vertical_generation_fits(int y1, int x1, int y2, int x2)
{
    for (int i = 0; i < current.portal_count; i++) {
        const cave_vertical_portal* p = &current.portals[i];
        if (p->kind == CAVE_VERTICAL_CONTROL) continue;
        for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++)
            if (p->local_patch[dy + 3][dx + 3] && p->y + dy >= y1
                && p->y + dy <= y2 && p->x + dx >= x1 && p->x + dx <= x2)
                return false;
    }
    return true;
}

static void receiver_patch(cave_vertical_portal* p)
{
    for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
        int distance = ABS(dy) + ABS(dx);
        p->peer_patch[dy + 3][dx + 3] = distance <= 1
            ? (p->material == FEAT_WATER ? FEAT_DEEP_WATER : p->material)
            : distance <= 3 ? p->material : FEAT_FLOOR;
    }
}

static bool patch_safe(int y, int x)
{
    if (y < 5 || x < 5 || y >= p_ptr->cur_map_hgt - 5
        || x >= p_ptr->cur_map_wid - 5) return false;
    for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
        int yy = y + dy, xx = x + dx, feat = cave_feat[yy][xx];
        if ((cave_info[yy][xx] & CAVE_ICKY) || cave_stair_bold(yy, xx)
            || terrain_generation_reserved(yy, xx)
            || cave_forge_bold(yy, xx) || FEAT_IS_BRIDGE(feat)
            || cave_fixture_at(yy, xx) || cave_m_idx[yy][xx] != 0
            || cave_o_idx[yy][xx] || feat == FEAT_WALL_PERM) return false;
    }
    return true;
}

static cave_vertical_portal* add_portal(u32b identity, int y, int x,
    int depth, int peer_y, int peer_x, int kind, int material, int strength)
{
    for (int i = 0; i < current.portal_count; i++) {
        cave_vertical_portal* existing = &current.portals[i];
        if (existing->identity == identity && existing->y == y && existing->x == x)
            return existing;
    }
    if (current.portal_count == CAVE_VERTICAL_PORTALS) return NULL;
    cave_vertical_portal* p = &current.portals[current.portal_count++];
    *p = (cave_vertical_portal){ .identity = identity, .y = y, .x = x,
        .peer_depth = depth, .peer_y = peer_y, .peer_x = peer_x,
        .kind = kind, .material = material, .strength = strength,
        .flags = depth > p_ptr->depth ? CAVE_VERTICAL_DOWN : 0,
        .local_style = cave_color[y][x], .peer_style = cave_color[y][x] };
    if (kind == CAVE_VERTICAL_STREAM || kind == CAVE_VERTICAL_SUMP)
        p->flags |= CAVE_VERTICAL_REVERSIBLE;
    for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++)
        if (in_bounds_fully(y + dy, x + dx))
            p->local_patch[dy + 3][dx + 3] = cave_feat[y + dy][x + dx];
    receiver_patch(p);
    return p;
}

static void vault_regions(void)
{
    vertical_vault_marker site;
    for (int i = 0; vertical_vault_marker_at(i, &site); i++) {
        if (site.role == VERTICAL_VAULT_UPPER_BASIN
            || site.role == VERTICAL_VAULT_LOWER_BASIN) {
            cave_vertical_basin* local = add_basin(site.identity, p_ptr->depth,
                site.y, site.x, FEAT_WATER, 72);
            cave_vertical_basin* peer = add_basin(site.identity, site.target_depth,
                site.target_y, site.target_x, FEAT_WATER, 72);
            if (local) local->radius = 8;
            if (peer) peer->radius = 8;
        } else if (site.role == VERTICAL_VAULT_SPILLWAY) {
            bool inherited = portal_at(site.y, site.x, false) != NULL;
            cave_vertical_portal* p = add_portal(site.identity, site.y, site.x,
                site.target_depth, site.target_y, site.target_x,
                CAVE_VERTICAL_FALL, FEAT_WATER, 3);
            if (p) {
                /* Vault geometry is reconstructed from its blueprint. */
                SDL_memset(p->local_patch, 0, sizeof(p->local_patch));
                SDL_memset(p->peer_patch, 0, sizeof(p->peer_patch));
                if (!inherited) p->flags |= CAVE_VERTICAL_CLOSED;
            }
        } else if (site.role == VERTICAL_VAULT_GATE) {
            bool closed = true;
            bool broken = false;
            for (int n = 0; n < current.portal_count; n++)
                if (current.portals[n].identity == site.identity
                    && current.portals[n].kind != CAVE_VERTICAL_CONTROL)
                {
                    closed = (current.portals[n].flags & CAVE_VERTICAL_CLOSED) != 0;
                    broken = (current.portals[n].flags & CAVE_VERTICAL_BROKEN) != 0;
                }
            cave_vertical_portal* p = add_portal(site.identity, site.y, site.x,
                site.target_depth, site.target_y, site.target_x,
                CAVE_VERTICAL_CONTROL, FEAT_WATER, 0);
            if (p) {
                p->flags |= CAVE_VERTICAL_GATE;
                if (closed) p->flags |= CAVE_VERTICAL_CLOSED;
                else p->flags &= ~CAVE_VERTICAL_CLOSED;
                if (broken) p->flags |= CAVE_VERTICAL_BROKEN;
                SDL_memset(p->local_patch, 0, sizeof(p->local_patch));
                SDL_memset(p->peer_patch, 0, sizeof(p->peer_patch));
                cave_set_feat(site.y, site.x, broken ? FEAT_BROKEN
                    : closed ? FEAT_DOOR_HEAD : FEAT_OPEN);
            }
        } else if (site.role == VERTICAL_VAULT_OVERLOOK
            && site.target_depth > p_ptr->depth) {
            cave_vertical_portal* p = add_portal(site.identity ^ 0x40000000u,
                site.y, site.x, site.target_depth, site.target_y, site.target_x,
                CAVE_VERTICAL_LAKE, FEAT_WATER, 0);
            if (p) {
                SDL_memset(p->local_patch, 0, sizeof(p->local_patch));
                SDL_memset(p->peer_patch, 0, sizeof(p->peer_patch));
                cave_set_feat(site.y, site.x, FEAT_CHASM);
            }
        } else if (site.role == VERTICAL_VAULT_OUTLET && new_down_allowed
            && site.target_depth < p_ptr->depth) {
            u32b identity = site.identity;
            cave_vertical_portal* p = add_portal(identity, site.y, site.x,
                p_ptr->depth + 1, site.y + 3, site.x, CAVE_VERTICAL_STREAM, FEAT_WATER, 1);
            if (p) {
                add_basin(identity, p_ptr->depth, site.y, site.x, FEAT_WATER, 72);
                add_basin(identity, p_ptr->depth + 1, site.y + 3, site.x, FEAT_WATER, 72);
            }
        }
    }
    /* The lower-first visit must already know the openings in its planned
     * ceiling, even when the upper map has never been generated. */
    for (int i = 0; vertical_vault_peer_marker_at(i, &site); i++) {
        if (site.role != VERTICAL_VAULT_OVERLOOK || site.target_depth >= p_ptr->depth) continue;
        cave_vertical_portal* p = add_portal(site.identity ^ 0x40000000u,
            site.y, site.x, site.target_depth, site.target_y, site.target_x,
            CAVE_VERTICAL_LAKE, FEAT_WATER, 0);
        if (p) {
            SDL_memset(p->local_patch, 0, sizeof(p->local_patch));
            SDL_memset(p->peer_patch, 0, sizeof(p->peer_patch));
        }
    }
}

void cave_vertical_generation_vaults(void)
{
    changing_terrain = true;
    vault_regions();
    changing_terrain = false;
}

bool cave_vertical_generation_finish(void)
{
    changing_terrain = true;
    for (int i = 0; i < current.portal_count; i++) {
        cave_vertical_portal* p = &current.portals[i];
        for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
            int feat = p->local_patch[dy + 3][dx + 3], y = p->y + dy, x = p->x + dx;
            if (!feat) continue;
            if (in_bounds_fully(y, x) && (cave_info[y][x] & CAVE_ICKY)
                && cave_feat[y][x] == feat) continue;
            if (!in_bounds_fully(y, x) || (cave_info[y][x] & CAVE_ICKY)
                || cave_stair_bold(y, x) || cave_forge_bold(y, x)
                || cave_o_idx[y][x] || cave_m_idx[y][x] > 0) {
                changing_terrain = false;
                return false;
            }
            cave_set_feat(y, x, feat);
            cave_color[y][x] = p->local_style;
        }
    }
    vault_regions();
    if (new_down_allowed && current.portal_count < 6) {
        int count = 0;
        /* A local accent may continue down even where there is no long river.
         * Selection is bounded; each placement keeps a normal nearby shore. */
        for (int attempt = 0; attempt < 256 && count < 2; attempt++) {
            int y = rand_range(5, p_ptr->cur_map_hgt - 6);
            int x = rand_range(5, p_ptr->cur_map_wid - 6);
            int feat = cave_feat[y][x];
            if ((!liquid(feat) && feat != FEAT_CHASM) || !patch_safe(y, x)) continue;
            bool close = false;
            for (int i = 0; i < current.portal_count; i++)
                close |= distance(y, x, current.portals[i].y, current.portals[i].x) < 9;
            if (close) continue;
            int material = feat == FEAT_CHASM ? FEAT_WATER : material_of(feat);
            int kind = feat == FEAT_CHASM ? CAVE_VERTICAL_LAKE
                : material == FEAT_LAVA ? CAVE_VERTICAL_LAVA_FALL
                : feat == FEAT_DEEP_WATER && one_in_(3) ? CAVE_VERTICAL_SUMP
                : rand_range(CAVE_VERTICAL_STREAM, CAVE_VERTICAL_FALL);
            int peer_y = y, peer_x = x;
            if (kind == CAVE_VERTICAL_STREAM || kind == CAVE_VERTICAL_RAPIDS) {
                /* Inclined passages need not occupy a vertical column. */
                peer_x += x < p_ptr->cur_map_wid / 2 ? 4 : -4;
            }
            u32b id = region_identity(p_ptr->depth, y, x, material);
            cave_vertical_portal* p = add_portal(id, y, x, p_ptr->depth + 1,
                peer_y, peer_x, kind, material,
                kind == CAVE_VERTICAL_STREAM ? 1 : kind == CAVE_VERTICAL_RAPIDS ? 2 : 3);
            if (!p) break;
            cave_vertical_basin* source = add_basin(id, p_ptr->depth, y, x, material, 72);
            cave_vertical_basin* receiver = add_basin(id, p_ptr->depth + 1,
                peer_y, peer_x, material, 72);
            if (source && receiver) {
                source->flags |= CAVE_VERTICAL_SOURCE;
                receiver->flags |= CAVE_VERTICAL_SINK;
                if (material == FEAT_LAVA && one_in_(3)) {
                    /* A finite late flow drains into an older roofed conduit. */
                    source->flags = CAVE_VERTICAL_RESERVOIR;
                    source->volume = 96; source->normal_volume = 0;
                    receiver->volume = receiver->normal_volume = 0;
                    receiver->flags |= CAVE_VERTICAL_CHANGED;
                    p->kind = CAVE_VERTICAL_TUBE;
                    p->flags |= CAVE_VERTICAL_REVERSIBLE;
                    p->peer_x += x < p_ptr->cur_map_wid / 2 ? 4 : -4;
                    receiver->x = p->peer_x;
                }
            }
            count++;
        }
    }
    changing_terrain = false;
    paint_basins(&current, p_ptr->depth, NULL);
    return true;
}

static const cave_vertical_portal* portal_at(int y, int x, bool gate)
{
    for (int i = 0; i < current.portal_count; i++) {
        const cave_vertical_portal* p = &current.portals[i];
        if (p->y == y && p->x == x && ((p->kind == CAVE_VERTICAL_CONTROL) == gate))
            return p;
    }
    return NULL;
}

int cave_vertical_current(int y, int x, int* direction)
{
    if (!in_bounds_fully(y, x) || !liquid(cave_feat[y][x])
        || FEAT_IS_BRIDGE(cave_feat[y][x])) return 0;
    for (int i = 0; i < current.portal_count; i++) {
        const cave_vertical_portal* p = &current.portals[i];
        const cave_vertical_basin* basin = basin_in(&current, p->identity, p_ptr->depth);
        if (!(p->flags & CAVE_VERTICAL_DOWN)
            || p->kind == CAVE_VERTICAL_CONTROL || p->kind == CAVE_VERTICAL_LAKE
            || p->kind == CAVE_VERTICAL_TUBE
            || material_of(cave_feat[y][x]) != p->material) continue;
        bool channel = basin && ((basin->x == p->x && x == p->x
                && y >= MIN(basin->y, p->y) && y <= MAX(basin->y, p->y))
            || (basin->y == p->y && y == p->y
                && x >= MIN(basin->x, p->x) && x <= MAX(basin->x, p->x)));
        if (distance(y, x, p->y, p->x) > 3 && !channel) continue;
        if ((p->flags & CAVE_VERTICAL_CLOSED)
            || (basin && (basin->flags & CAVE_VERTICAL_RESERVOIR) && !basin->volume)
            || material_of(cave_feat[p->y][p->x]) != p->material) return 0;
        int dy = p->y > y ? 1 : p->y < y ? -1 : 0;
        int dx = !dy ? (p->x > x ? 1 : p->x < x ? -1 : 0) : 0;
        if (direction) *direction = dy > 0 ? 2 : dy < 0 ? 8 : dx > 0 ? 6 : dx < 0 ? 4 : 5;
        return p->strength;
    }
    if (freshwater(cave_feat[y][x]) && !cave_water_flow_is_lake(y, x)) {
        int flow = cave_water_flow_direction(y, x);
        int dir = flow == CAVE_WATER_FLOW_NORTH ? 8 : flow == CAVE_WATER_FLOW_SOUTH ? 2
            : flow == CAVE_WATER_FLOW_EAST ? 6 : flow == CAVE_WATER_FLOW_WEST ? 4 : 0;
        if (dir) { if (direction) *direction = dir; return 1; }
    }
    return 0;
}

int cave_vertical_movement_energy(int energy, int from_y, int from_x,
    int to_y, int to_x)
{
    int dir = 5, strength = cave_vertical_current(to_y, to_x, &dir);
    if (!strength || p_ptr->leaping || dir == 5) return energy;
    int dy = to_y - from_y, dx = to_x - from_x;
    int along = dy * ddy[dir] + dx * ddx[dir];
    /* Downstream swimming replaces the grounded deep-water slog; crossing
     * or fighting the current retains the ordinary terrain cost. */
    if (along > 0) return MAX(100, energy / (strength + 1));
    if (along < 0) return energy + strength * 50;
    return energy;
}

static bool wet_portal(const cave_vertical_portal* p)
{
    int feature = cave_feat[p->y][p->x];
    if (p->kind == CAVE_VERTICAL_TUBE) {
        const cave_vertical_basin* b = basin_in(&current, p->identity, p_ptr->depth);
        const cave_vertical_basin* peer = basin_in(&current, p->identity, p->peer_depth);
        return feature == FEAT_FLOOR && b && !b->volume && !b->hot
            && peer && !peer->volume && !peer->hot;
    }
    const cave_vertical_basin* b = basin_in(&current, p->identity, p_ptr->depth);
    if (b && (b->flags & CAVE_VERTICAL_RESERVOIR) && !b->volume) {
        if (p->kind == CAVE_VERTICAL_STREAM) return feature == FEAT_FLOOR;
        if (p->kind == CAVE_VERTICAL_FALL || p->kind == CAVE_VERTICAL_LAVA_FALL)
            return feature == FEAT_CHASM;
    }
    return p->kind == CAVE_VERTICAL_LAKE ? feature == FEAT_CHASM
        : material_of(feature) == p->material;
}

bool cave_vertical_can_travel(bool ascending)
{
    const cave_vertical_portal* p = portal_at(p_ptr->py, p_ptr->px, false);
    if (!p || !wet_portal(p)) return false;
    return ascending ? p->peer_depth < p_ptr->depth : p->peer_depth > p_ptr->depth;
}

int cave_vertical_fall_damage(int levels, int landing_feature)
{
    int damage = damroll(3 * MAX(1, levels), 4);
    /* Even a deep receiving pool leaves a substantial impact. A bridge or
     * frozen surface is hard ground; lava retains its separate contact cost. */
    if (landing_feature == FEAT_DEEP_WATER) return MAX(1, (damage + 1) / 2);
    if (landing_feature == FEAT_WATER) return MAX(1, (damage * 3 + 3) / 4);
    return damage;
}

static int receiving_feature(const cave_vertical_portal* p)
{
    const level_image* floor = level_cache_floor(p->peer_depth);
    if (floor && p->peer_y > 0 && p->peer_x > 0
        && p->peer_y < floor->hgt - 1 && p->peer_x < floor->wid - 1)
        return floor->feat[p->peer_y][p->peer_x];
    /* First entry uses the same inherited footprint as generation. A changed
     * basin overrides its initial patch, including a now drained landing. */
    int feature = p->peer_patch[3][3];
    if (!feature) feature = FEAT_FLOOR;
    const cave_vertical_basin* b = basin_in(&current, p->identity, p->peer_depth);
    if (b && ((b->flags & (CAVE_VERTICAL_RESERVOIR | CAVE_VERTICAL_CHANGED))
            || b->volume != b->normal_volume)) {
        int profile = vertical_vault_liquid_profile(vertical_vault_get(),
            p->peer_depth, p->identity, p->peer_y, p->peer_x);
        feature = basin_feature(b, p->peer_y - b->y, p->peer_x - b->x, feature, profile);
    }
    return feature;
}

static bool depart(const cave_vertical_portal* p, bool forced)
{
    /* A background destination may take several input frames to arrive. The
     * already committed fall must never charge impact or quest departure twice. */
    if (level_cache_transition_pending()) return true;
    if (!forced && (p->flags & CAVE_VERTICAL_CLOSED)) {
        msg_print("The sluice is closed."); return true;
    }
    bool ascending = p->peer_depth < p_ptr->depth;
    if (ascending && !(p->flags & CAVE_VERTICAL_REVERSIBLE)) {
        msg_print("The drop is too steep to climb here."); return true;
    }
    if (!forced && p->peer_depth < min_depth() && !silmarils_possessed()) {
        msg_print("The current passage cannot take you beyond Morgoth's call.");
        p_ptr->energy_use = 100; return true;
    }
    if (p_ptr->on_the_run && p->peer_depth >= MORGOTH_DEPTH) return true;
    if (!forced && (p->kind == CAVE_VERTICAL_FALL || p->kind == CAVE_VERTICAL_LAVA_FALL
        || p->kind == CAVE_VERTICAL_LAKE || p->kind == CAVE_VERTICAL_SUMP)) {
        cptr warning = p->kind == CAVE_VERTICAL_SUMP
            ? "Dive into the flooded passage? Air and footing will be limited. "
            : "Go over the drop? You may be injured and cannot climb back here. ";
        if (!get_check_near(p_ptr->py, p_ptr->px, warning)) return true;
    }
    int travel_energy = 100;
    if (ascending && p->kind == CAVE_VERTICAL_STREAM && liquid(cave_feat[p->y][p->x]))
        travel_energy += p->strength * 50;
    p_ptr->energy_use = MAX(travel_energy, p_ptr->energy_use);
    p_ptr->previous_action[0] = ACTION_MISC;
    if (p->kind == CAVE_VERTICAL_FALL || p->kind == CAVE_VERTICAL_LAVA_FALL
        || p->kind == CAVE_VERTICAL_LAKE) {
        int damage = cave_vertical_fall_damage(ABS(p->peer_depth - p_ptr->depth), receiving_feature(p));
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(damage, p->kind == CAVE_VERTICAL_LAKE ? "falling into a lake cavern"
            : p->kind == CAVE_VERTICAL_LAVA_FALL ? "falling down a volcanic shaft"
            : "falling down a waterfall");
        if (p_ptr->is_dead) return true;
    }
    if (p->kind == CAVE_VERTICAL_SUMP) {
        current.submerged = 1;
        current.breath = MAX(1, 12 - p_ptr->total_weight / 100);
    }
    msg_print(ascending ? "You struggle upstream into the higher passage."
        : p->kind == CAVE_VERTICAL_SUMP ? "You dive beneath the rock into the lower passage."
        : "The descending watercourse carries you deeper.");
    varda_quest_fail_if_bastion_missed();
    note_lost_greater_vault();
    quest_followup_leave(p->peer_depth);
    level_cache_travel_to(p->peer_depth, p->peer_y, p->peer_x);
    return true;
}

bool cave_vertical_travel(bool ascending)
{
    if (!cave_vertical_can_travel(ascending)) return false;
    return depart(portal_at(p_ptr->py, p_ptr->px, false), false);
}

bool cave_vertical_forced_travel(int y, int x)
{
    const cave_vertical_portal* p = portal_at(y, x, false);
    if (!p || !(p->flags & CAVE_VERTICAL_DOWN) || (p->flags & CAVE_VERTICAL_CLOSED)
        || p->kind == CAVE_VERTICAL_CONTROL || p->kind == CAVE_VERTICAL_STREAM
        || p->kind == CAVE_VERTICAL_SUMP || !wet_portal(p)) return false;
    return depart(p, true);
}

void cave_vertical_flow_build(void)
{
    for (int i = 0; i < current.portal_count; i++) {
        cave_vertical_portal* p = &current.portals[i];
        if (!(p->flags & CAVE_VERTICAL_DOWN) || p->kind == CAVE_VERTICAL_CONTROL
            || p->kind == CAVE_VERTICAL_LAKE) continue;
        const cave_vertical_basin* basin = basin_in(&current, p->identity, p_ptr->depth);
        int min_y = p->y - 3, max_y = p->y + 3, min_x = p->x - 3, max_x = p->x + 3;
        if (basin && (basin->y == p->y || basin->x == p->x)
            && distance(basin->y, basin->x, p->y, p->x) <= 16) {
            min_y = MIN(min_y, basin->y - 3); max_y = MAX(max_y, basin->y + 3);
            min_x = MIN(min_x, basin->x - 3); max_x = MAX(max_x, basin->x + 3);
        }
        p->direction = 0;
        for (int y = min_y; y <= max_y; y++) for (int x = min_x; x <= max_x; x++) {
            int direction = 5;
            if (!in_bounds_fully(y, x)) continue;
            int strength = cave_vertical_current(y, x, &direction);
            int flow = direction == 2 ? CAVE_WATER_FLOW_SOUTH
                : direction == 8 ? CAVE_WATER_FLOW_NORTH
                : direction == 6 ? CAVE_WATER_FLOW_EAST
                : direction == 4 ? CAVE_WATER_FLOW_WEST : CAVE_WATER_FLOW_CALM;
            if (freshwater(cave_feat[y][x]))
                cave_water_flow_set(y, x, strength > 0 ? flow : CAVE_WATER_FLOW_CALM);
            if (!p->direction && strength > 0 && material_of(cave_feat[y][x]) == p->material
                && distance(y, x, p->y, p->x) == 1) p->direction = direction;
        }
    }
}

void cave_vertical_end_action(void)
{
    if (!p_ptr || !p_ptr->energy_use || p_ptr->leaving || p_ptr->is_dead) return;
    p_ptr->redraw |= PR_STATE | PR_TERRAIN;
    for (int i = 0; i < current.basin_count; i++) {
        const cave_vertical_basin* b = &current.basins[i];
        if (b->depth != p_ptr->depth || !b->hot
            || distance(p_ptr->py, p_ptr->px, b->y, b->x) > b->radius
            || !los(b->y, b->x, p_ptr->py, p_ptr->px)) continue;
        int resistance = MAX(0, p_ptr->resist_fire + (p_ptr->oppose_fire ? 1 : 0));
        int damage = MAX(1, 6 / (resistance + 1));
        if (b->hot == 5) {
            int walls = 0;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
                if (in_bounds_fully(b->y + dy, b->x + dx)
                    && FEAT_IS_WALL(cave_feat[b->y + dy][b->x + dx])) walls++;
            if (walls >= 5) damage += 4;
        }
        take_hit(damage, "steam and cooling volcanic rock");
        if (p_ptr->is_dead) return;
    }
    if (current.submerged) {
        if (!player_submerged_in_deep_water()) {
            current.submerged = 0; current.breath = 12;
            msg_print("You find air above the water.");
        } else if (current.breath) {
            current.breath--;
            if (current.breath == 3) msg_print("You are running out of breath. Reach the shallows!");
        } else {
            take_hit(4, "drowning in a flooded passage");
        }
    }
    if (p_ptr->leaping) return;
    const cave_vertical_portal* p = portal_at(p_ptr->py, p_ptr->px, false);
    if (p && wet_portal(p) && (p->flags & CAVE_VERTICAL_DOWN)
        && !(p->flags & CAVE_VERTICAL_CLOSED)
        && p->kind != CAVE_VERTICAL_STREAM && p->kind != CAVE_VERTICAL_SUMP
        && p->kind != CAVE_VERTICAL_TUBE)
        depart(p, true);
    else {
        int dir = 5, strength = cave_vertical_current(p_ptr->py, p_ptr->px, &dir);
        if (strength < 2 || dir == 5) return;
        int y = p_ptr->py + ddy[dir], x = p_ptr->px + ddx[dir];
        if (in_bounds_fully(y, x) && !cave_m_idx[y][x]
            && liquid(cave_feat[y][x]) && player_passable(y, x, false)) {
            int old_feat = cave_feat[p_ptr->py][p_ptr->px];
            monster_swap(p_ptr->py, p_ptr->px, y, x);
            player_water_displaced(old_feat, cave_feat[y][x]);
            msg_print("The current sweeps you downstream.");
            /* At most one drift and one level transition per paid action. */
            p = portal_at(y, x, false);
            if (p && p->kind != CAVE_VERTICAL_STREAM && p->kind != CAVE_VERTICAL_SUMP)
                depart(p, true);
        }
    }
}

bool cave_vertical_receive(cave_vertical_state* state, int depth,
    u32b identity, int material, int amount)
{
    cave_vertical_basin* b = basin_in(state, identity, depth);
    if (!b || amount < 0 || amount > 4096) return false;
    if (material_of(b->material) != material_of(material)) {
        if ((material == FEAT_LAVA && b->material == FEAT_WATER)
            || (material == FEAT_WATER && b->material == FEAT_LAVA)) {
            if (amount - MIN(amount, b->volume) > b->capacity) return false;
            int consumed = MIN(amount, b->volume);
            b->volume -= consumed; b->hot = 6;
            b->flags |= CAVE_VERTICAL_CHANGED;
            if (amount > consumed) {
                b->material = material;
                b->volume = amount - consumed;
            }
            if (!(b->flags & CAVE_VERTICAL_SOURCE))
                for (int p = 0; p < state->portal_count; p++)
                    if (state->portals[p].identity == identity
                        && (state->portals[p].flags & CAVE_VERTICAL_DOWN))
                        state->portals[p].material = b->material;
            return true;
        }
        return false;
    }
    if (amount > b->capacity - b->volume) return false;
    b->volume += amount;
    b->flags |= CAVE_VERTICAL_CHANGED;
    return true;
}

static void advance(cave_vertical_state* state, int depth, const level_image* image)
{
    if (state->last_turn > turn) state->last_turn = turn;
    if (turn - state->last_turn < 10) return;
    int steps = MIN(256, (turn - state->last_turn) / 10);
    state->last_turn += steps * 10;
    for (int i = 0; i < state->basin_count; i++) {
        cave_vertical_basin* b = &state->basins[i];
        if (b->depth != depth) continue;
        b->hot = MAX(0, b->hot - steps);
        if (b->flags & CAVE_VERTICAL_SOURCE) {
            int supply = b->material;
            for (int p = 0; p < state->portal_count; p++)
                if (state->portals[p].identity == b->identity
                    && (state->portals[p].flags & CAVE_VERTICAL_DOWN)) {
                    supply = state->portals[p].material; break;
                }
            if (supply == b->material) b->volume = MIN(b->capacity, b->volume + 4 * steps);
            else (void)cave_vertical_receive(state, depth, b->identity, supply, 4 * steps);
        }
        if (b->flags & CAVE_VERTICAL_SINK) {
            int feature = image ? image->feat[b->y][b->x] : cave_feat[b->y][b->x];
            int old_volume = b->volume;
            if (material_of(feature) == b->material)
                b->volume -= MIN(MAX(0, b->volume - b->normal_volume), 4 * steps);
            if (old_volume && !b->volume && b->material == FEAT_LAVA) {
                b->hot = 6; b->flags |= CAVE_VERTICAL_CHANGED;
            }
        }
    }
    for (int i = 0; i < state->portal_count; i++) {
        cave_vertical_portal* p = &state->portals[i];
        if (!(p->flags & CAVE_VERTICAL_DOWN)) {
            /* An in-flight packet is handed to the receiving half before
             * its sending floor is evicted. It is credited exactly once. */
            if (p->pending && p->due <= turn
                && cave_vertical_receive(state, depth, p->identity,
                    p->pending_material ? p->pending_material : p->material, p->pending)) {
                p->pending = 0;
                p->pending_material = 0;
            }
            continue;
        }
        if (p->kind == CAVE_VERTICAL_CONTROL
            || p->kind == CAVE_VERTICAL_LAKE) continue;
        cave_vertical_basin* b = basin_in(state, p->identity, depth);
        if (p->pending && p->due <= turn
            && level_cache_vertical_receive(p->peer_depth, p->identity,
                p->pending_material ? p->pending_material : p->material, p->pending)) {
            p->pending = 0;
            p->pending_material = 0;
        }
        if (!b || p->pending || (p->flags & CAVE_VERTICAL_CLOSED)) continue;
        int feature = image ? image->feat[p->y][p->x] : cave_feat[p->y][p->x];
        if (material_of(feature) != p->material || b->material != p->material) continue;
        /* Stable natural rivers transfer their inflow through the network;
         * sluice releases spend finite stored water. No cloned flood budget. */
        int available = (b->flags & CAVE_VERTICAL_RESERVOIR) ? b->volume
            : MAX(0, b->volume - b->normal_volume);
        int capacity = (b->flags & CAVE_VERTICAL_SOURCE) ? 8 : 4 * MAX(1, p->strength);
        int amount = MIN(available, steps * capacity);
        if (!amount) continue;
        b->volume -= amount; p->pending = amount; p->due = turn + 20;
        p->pending_material = p->material;
        if (!b->volume && b->material == FEAT_LAVA) {
            b->hot = 6;
            b->flags |= CAVE_VERTICAL_CHANGED;
        }
    }
    for (int i = 0; i < state->basin_count; i++) {
        cave_vertical_basin* b = &state->basins[i];
        if (b->depth == depth && b->volume <= b->normal_volume)
            b->flags &= ~(CAVE_VERTICAL_RISE_WARNED | CAVE_VERTICAL_OVERFLOW_WARNED);
    }
}

static void drift_objects(void)
{
    for (int i = 0; i < current.portal_count; i++) {
        const cave_vertical_portal* p = &current.portals[i];
        if (!(p->flags & CAVE_VERTICAL_DOWN) || (p->flags & CAVE_VERTICAL_CLOSED)
            || p->material != FEAT_WATER || p->kind == CAVE_VERTICAL_CONTROL
            || p->kind == CAVE_VERTICAL_SUMP || p->kind == CAVE_VERTICAL_LAKE
            || !wet_portal(p)) continue;
        for (int index = cave_o_idx[p->y][p->x]; index > 0; index = o_list[index].next_o_idx) {
            const object_type* object = &o_list[index];
            if (object->name1 || object->tval == TV_NOTE
                || (object->tval != TV_ARROW && object->tval != TV_FOOD
                    && object->tval != TV_STAFF && object->tval != TV_FLASK)) continue;
            if (level_cache_transfer_object(p->peer_depth, index, p->peer_y, p->peer_x))
                if (cave_info[p->y][p->x] & CAVE_SEEN)
                    msg_print("The current carries an item into the lower passage.");
            break;
        }
    }
}

static int basin_feature(const cave_vertical_basin* b, int dy, int dx, int original, int profile)
{
    if (FEAT_IS_ICE(original) || FEAT_IS_BRIDGE(original) || FEAT_IS_WALL(original)
        || (!liquid(original) && original != FEAT_FLOOR)) return original;
    if (profile) {
        if (!b->volume) return FEAT_FLOOR;
        if (profile == 1) return b->volume > b->normal_volume * 4 / 3 ? b->material : FEAT_FLOOR;
        if (profile == 2 && b->volume <= b->normal_volume / 3) return FEAT_FLOOR;
        if (profile == 3 && b->material == FEAT_WATER && b->volume > b->normal_volume / 2)
            return FEAT_DEEP_WATER;
        return b->material;
    }
    int band = MAX(ABS(dy), ABS(dx));
    int wet_radius = b->volume > 100 ? 3 : b->volume > 40 ? 2 : b->volume > 8 ? 1 : 0;
    if (band > wet_radius || !b->volume) return FEAT_FLOOR;
    if (b->material == FEAT_WATER && band < wet_radius && b->volume > 40)
        return FEAT_DEEP_WATER;
    return b->material;
}

static void paint_basins(cave_vertical_state* state, int depth, level_image* image)
{
    changing_terrain = true;
    for (int i = 0; i < state->basin_count; i++) {
        cave_vertical_basin* b = &state->basins[i];
        if (b->depth != depth || (!(b->flags & (CAVE_VERTICAL_RESERVOIR | CAVE_VERTICAL_CHANGED))
            && b->volume == b->normal_volume && !b->hot)) continue;
        const vertical_vault_state* vaults = image ? &image->vertical_vaults : vertical_vault_get();
        bool authored = false;
        for (int v = 0; v < vaults->count; v++)
            authored |= vaults->placements[v].identity == b->identity;
        for (int dy = -b->radius; dy <= b->radius; dy++)
            for (int dx = -b->radius; dx <= b->radius; dx++) {
                int y = b->y + dy, x = b->x + dx;
                int hgt = image ? image->hgt : p_ptr->cur_map_hgt;
                int wid = image ? image->wid : p_ptr->cur_map_wid;
                if (y < 1 || x < 1 || y >= hgt - 1 || x >= wid - 1) continue;
                int old = image ? image->feat[y][x] : cave_feat[y][x];
                int profile = authored ? vertical_vault_liquid_profile(vaults, depth, b->identity, y, x) : 0;
                if (authored && !profile) continue;
                int feat = basin_feature(b, dy, dx, old, profile);
                if (feat == old) continue;
                if (image) {
                    if (image->fixtures[y][x]
                        || (image->monsters_at[y][x] > 0 && feat == FEAT_LAVA)) continue;
                    image->feat[y][x] = feat;
                    if (b->material == FEAT_LAVA && feat == FEAT_FLOOR && z_info->style_max > 62)
                        image->color[y][x] = 128 + 62;
                    image->cells[y][x].base_feat = feat;
                    image->water_flow[y][x] = 0;
                } else if (!cave_fixture_at(y, x)
                    && (cave_m_idx[y][x] <= 0 || feat != FEAT_LAVA)) {
                    cave_set_feat(y, x, feat);
                    if (b->material == FEAT_LAVA && feat == FEAT_FLOOR && z_info->style_max > 62)
                        cave_color[y][x] = 128 + 62;
                }
                int actual = image ? image->feat[y][x] : cave_feat[y][x];
                for (int p = 0; p < state->portal_count; p++) {
                    cave_vertical_portal* portal = &state->portals[p];
                    int py = y - portal->y + 3, px = x - portal->x + 3;
                    if (py >= 0 && py < 7 && px >= 0 && px < 7 && portal->local_patch[py][px])
                        portal->local_patch[py][px] = actual;
                }
            }
        for (int p = 0; p < state->portal_count; p++) {
            const cave_vertical_portal* outlet = &state->portals[p];
            if (outlet->identity != b->identity || outlet->kind == CAVE_VERTICAL_CONTROL
                || outlet->kind == CAVE_VERTICAL_LAKE
                || (outlet->y != b->y && outlet->x != b->x)) continue;
            int length = ABS(outlet->y - b->y) + ABS(outlet->x - b->x);
            if (length > 16) continue;
            int sy = (outlet->y > b->y) - (outlet->y < b->y);
            int sx = (outlet->x > b->x) - (outlet->x < b->x);
            for (int step = 0; step <= length; step++) {
                int y = b->y + sy * step, x = b->x + sx * step;
                int hgt = image ? image->hgt : p_ptr->cur_map_hgt;
                int wid = image ? image->wid : p_ptr->cur_map_wid;
                if (y < 1 || x < 1 || y >= hgt - 1 || x >= wid - 1) break;
                int old = image ? image->feat[y][x] : cave_feat[y][x];
                if (FEAT_IS_ICE(old) || FEAT_IS_WALL(old) || FEAT_IS_BRIDGE(old)
                    || (old != FEAT_FLOOR && old != FEAT_CHASM && !liquid(old))) break;
                int feat = b->volume ? (old == FEAT_DEEP_WATER && b->material == FEAT_WATER
                    ? FEAT_DEEP_WATER : b->material) : FEAT_FLOOR;
                if (!b->volume && step == length && (outlet->flags & CAVE_VERTICAL_DOWN)
                    && !(outlet->flags & CAVE_VERTICAL_CLOSED)
                    && (outlet->kind == CAVE_VERTICAL_FALL || outlet->kind == CAVE_VERTICAL_LAVA_FALL))
                    feat = FEAT_CHASM;
                if (image) {
                    if (image->fixtures[y][x] || (image->monsters_at[y][x] > 0
                        && (feat == FEAT_LAVA || feat == FEAT_CHASM))) continue;
                    image->feat[y][x] = feat; image->cells[y][x].base_feat = feat;
                    image->water_flow[y][x] = 0;
                } else if (!cave_fixture_at(y, x) && (cave_m_idx[y][x] <= 0 || feat != FEAT_LAVA)) {
                    int actor = cave_m_idx[y][x];
                    cave_set_feat(y, x, feat);
                    if (feat == FEAT_CHASM && actor > 0)
                        (void)cave_vertical_monster_fall(actor);
                }
                for (int n = 0; n < state->portal_count; n++) {
                    cave_vertical_portal* portal = &state->portals[n];
                    int py = y - portal->y + 3, px = x - portal->x + 3;
                    if (py >= 0 && py < 7 && px >= 0 && px < 7 && portal->local_patch[py][px])
                        portal->local_patch[py][px] = image ? image->feat[y][x] : cave_feat[y][x];
                }
            }
        }
    }
    changing_terrain = false;
    if (!image) vertical_vault_release_rewards();
}

void cave_vertical_process_image(level_image* image)
{
    if (!image) return;
    advance(&image->vertical, image->depth, image);
    paint_basins(&image->vertical, image->depth, image);
}

static void announce_basins(void)
{
    if (!character_dungeon || !p_ptr->playing || level_cache_in_worker()) return;
    for (int i = 0; i < current.basin_count; i++) {
        cave_vertical_basin* b = &current.basins[i];
        if (b->depth != p_ptr->depth || b->volume <= b->normal_volume) continue;
        int heard = cave_audio_distance(b->y, b->x, p_ptr->py, p_ptr->px);
        if (!(cave_info[b->y][b->x] & CAVE_SEEN)
            && (heard < 0 || heard > b->radius + 6)) continue;
        if (!(b->flags & CAVE_VERTICAL_RISE_WARNED)) {
            msg_print(b->material == FEAT_LAVA
                ? "Molten rock is rising behind the blocked conduit. Leave the low banks!"
                : "The basin is rising. Its low banks may soon flood!");
            cave_event_emit(CAVE_EVENT_WARNING, b->y, b->x, 8);
            b->flags |= CAVE_VERTICAL_RISE_WARNED;
        }
        int brim = MIN(b->capacity, MAX(b->normal_volume + 4, b->capacity * 3 / 4));
        if (b->volume >= brim && !(b->flags & CAVE_VERTICAL_OVERFLOW_WARNED)) {
            msg_print(b->material == FEAT_LAVA
                ? "The lava basin is near its brim. Overflow threatens the surrounding passage!"
                : "The basin is near its brim. Overflow threatens the surrounding passage!");
            cave_event_emit(CAVE_EVENT_WARNING, b->y, b->x, 12);
            b->flags |= CAVE_VERTICAL_OVERFLOW_WARNED;
        }
    }
}

void cave_vertical_process(void)
{
    s32b before = current.last_turn;
    advance(&current, p_ptr->depth, NULL);
    if (current.last_turn != before) {
        for (int i = 0; i < current.basin_count; i++) {
            const cave_vertical_basin* b = &current.basins[i];
            if (b->depth == p_ptr->depth && b->hot == 5) {
                cave_event_emit(CAVE_EVENT_VENT, b->y, b->x, 8);
                if (distance(p_ptr->py, p_ptr->px, b->y, b->x) <= 6)
                    msg_print("Steam billows above the newly exposed hot rock.");
            }
        }
    }
    if (current.last_turn != before) drift_objects();
    announce_basins();
    paint_basins(&current, p_ptr->depth, NULL);
    level_cache_process_vertical();
    cave_vertical_flow_build();
    if (!p_ptr->leaving && !p_ptr->is_dead && !p_ptr->leaping
        && cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM)
        (void)cave_vertical_forced_travel(p_ptr->py, p_ptr->px);
}

void cave_vertical_refresh(void)
{
    announce_basins();
    paint_basins(&current, p_ptr->depth, NULL);
    cave_vertical_flow_build();
}

bool cave_vertical_interact(int y, int x)
{
    const cave_vertical_portal* gate = portal_at(y, x, true);
    if (!gate || distance(p_ptr->py, p_ptr->px, y, x) > 1
        || p_ptr->blind || !(cave_info[y][x] & CAVE_SEEN)) return false;
    if (gate->flags & CAVE_VERTICAL_BROKEN) {
        msg_print("The broken sluice can no longer hold back the water.");
        return true;
    }
    bool opening = (gate->flags & CAVE_VERTICAL_CLOSED) != 0;
    u32b identity = gate->identity;
    for (int i = 0; i < current.portal_count; i++)
        if (current.portals[i].identity == identity) {
            if (opening) current.portals[i].flags &= ~CAVE_VERTICAL_CLOSED;
            else current.portals[i].flags |= CAVE_VERTICAL_CLOSED;
        }
    for (int i = 0; i < current.basin_count; i++)
        if (current.basins[i].identity == identity && current.basins[i].depth == p_ptr->depth)
            current.basins[i].flags |= CAVE_VERTICAL_RESERVOIR;
    level_image* peer = level_cache_mutable_floor(gate->peer_depth);
    if (peer) {
        for (int i = 0; i < peer->vertical.basin_count; i++)
            if (peer->vertical.basins[i].identity == identity
                && peer->vertical.basins[i].depth == p_ptr->depth)
                peer->vertical.basins[i].flags |= CAVE_VERTICAL_RESERVOIR;
    }
    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;
    changing_terrain = true;
    cave_set_feat(y, x, opening ? FEAT_OPEN : FEAT_DOOR_HEAD);
    changing_terrain = false;
    msg_print(opening ? "You open the sluice. Stored water begins to drain toward the lower cistern."
        : "You close the sluice. The outflow subsides.");
    cave_events_terrain_changed();
    cave_vertical_refresh();
    return true;
}

bool cave_vertical_view(int y, int x, cave_vertical_view_info* view)
{
    if (!view) return false;
    SDL_memset(view, 0, sizeof(*view));
    if (!p_ptr || p_ptr->blind || p_ptr->image || !character_dungeon
        || !in_bounds_fully(y, x) || !(cave_info[y][x] & CAVE_SEEN)) return false;
    const cave_vertical_portal* p = portal_at(y, x, false);
    int depth = p ? p->peer_depth : p_ptr->depth + 1;
    int target_y = p ? p->peer_y : y, target_x = p ? p->peer_x : x;
    if (p) {
        view->flags = p->flags & CAVE_VERTICAL_DOWN ? CAVE_VERTICAL_OUTFLOW : CAVE_VERTICAL_INFLOW;
        if (p->kind == CAVE_VERTICAL_LAKE && p->peer_depth < p_ptr->depth)
            view->flags |= CAVE_VERTICAL_CEILING;
        if ((p->kind == CAVE_VERTICAL_STREAM || p->kind == CAVE_VERTICAL_SUMP
            || p->kind == CAVE_VERTICAL_TUBE) && wet_portal(p)
            && !(p->flags & CAVE_VERTICAL_CLOSED)) {
            view->flags |= CAVE_VERTICAL_PASSAGE;
            if (p->kind == CAVE_VERTICAL_SUMP) view->flags |= CAVE_VERTICAL_DIVE;
            if (p->kind == CAVE_VERTICAL_TUBE) view->flags |= CAVE_VERTICAL_DRY_TUBE;
        }
        view->fluid_feature = p->material; view->direction = p->direction;
        if (p->kind == CAVE_VERTICAL_STREAM || p->kind == CAVE_VERTICAL_SUMP
            || p->kind == CAVE_VERTICAL_TUBE || p->kind == CAVE_VERTICAL_LAKE
            || p->kind == CAVE_VERTICAL_CONTROL || (p->flags & CAVE_VERTICAL_CLOSED))
            view->fluid_feature = 0;
    } else if (cave_feat[y][x] != FEAT_CHASM
        && cave_bridge_underlay(cave_feat[y][x]) != FEAT_CHASM) return false;
    view->relative_depth = depth - p_ptr->depth;
    const level_image* below = level_cache_floor(depth);
    bool open = cave_feat[y][x] == FEAT_CHASM
        || cave_bridge_underlay(cave_feat[y][x]) == FEAT_CHASM
        || (p && (p->kind == CAVE_VERTICAL_FALL || p->kind == CAVE_VERTICAL_LAVA_FALL
            || p->kind == CAVE_VERTICAL_LAKE));
    if (!below || !open || depth <= p_ptr->depth
        || target_y < 1 || target_x < 1 || target_y >= below->hgt - 1
        || target_x >= below->wid - 1) return p != NULL;
    if (below->light[target_y][target_x] <= 0
        && !(below->info[target_y][target_x] & CAVE_GLOW)
        && !(p_ptr->cur_light > 0 && distance(p_ptr->py, p_ptr->px, y, x) <= 2))
        return p != NULL;
    int feature = below->feat[target_y][target_x];
    if (f_info[feature].mimic) feature = f_info[feature].mimic;
    if (FEAT_IS_TRAP(feature) && (below->info[target_y][target_x] & CAVE_HIDDEN))
        feature = FEAT_FLOOR;
    view->feature = feature;
    view->attr = f_info[feature].x_attr; view->chr = f_info[feature].x_char;
    if ((feature == FEAT_FLOOR || FEAT_IS_GRANITE(feature)) && style_info) {
        int color = below->color[target_y][target_x];
        int style = color >= 128 ? (color - 128) & 63 : below->primary_style;
        if (style >= 0 && style < z_info->style_max) {
            const style_type* s = &style_info[style];
            if (FEAT_IS_GRANITE(feature)) {
                view->attr = (byte)(s->wall_row | 0x80);
                view->chr = (char)(s->wall_col | 0x80);
            } else {
                int choice = below->floor_choices[style];
                if (s->floor_tiled && s->floor_count > 1) {
                    u32b hash = (u32b)target_x * 0x9e3779b9u ^ (u32b)target_y * 0x85ebca6bu;
                    hash ^= (u32b)depth * 0xc2b2ae35u;
                    hash ^= hash >> 16; hash *= 0x7feb352du; hash ^= hash >> 15;
                    choice = hash % s->floor_count;
                }
                if (below->color[target_y][target_x] & 64) choice = 0;
                if (choice >= s->floor_count) choice = 0;
                view->attr = (byte)((s->floor_count ? s->floor_rowv[choice] : s->floor_row) | 0x80);
                view->chr = (char)((s->floor_count ? s->floor_colv[choice] : s->floor_col) | 0x80);
            }
        }
    }
    view->flags |= CAVE_VERTICAL_BELOW_VISIBLE;
    return true;
}

bool cave_vertical_describe(int y, int x, char* text, size_t size)
{
    if (!text || !size) return false;
    text[0] = '\0';
    if (!p_ptr || !in_bounds_fully(y, x) || p_ptr->blind || p_ptr->image
        || ((p_ptr->rage || g_labyrinth_view_active) && !(cave_info[y][x] & CAVE_SEEN))
        || !(cave_info[y][x] & (CAVE_MARK | CAVE_SEEN))) return false;
    const cave_vertical_portal* p = portal_at(y, x, true);
    if (p) {
        if (p->flags & CAVE_VERTICAL_BROKEN) {
            strnfmt(text, size, "Broken cistern sluice. Water drains freely into the lower basin.");
            return true;
        }
        strnfmt(text, size, "Cistern sluice: %s. Open or close it from an adjacent square; water reaches the lower basin after a delay.",
            p->flags & CAVE_VERTICAL_CLOSED ? "closed" : "open");
        return true;
    }
    p = portal_at(y, x, false);
    if (!p) {
        cave_vertical_view_info view;
        if (!cave_vertical_view(y, x, &view)
            || !(view.flags & CAVE_VERTICAL_BELOW_VISIBLE)) return false;
        strnfmt(text, size, "Open drop. Visible %d ft below: %s. The lower surface is on another floor.",
            view.relative_depth * 50, f_name + f_info[view.feature].name);
        return true;
    }
    const char* names[] = { "", "Descending stream", "Rapids", "Waterfall",
        "Submerged passage", "Lava fall", "Lake cavern opening", "Sluice", "Lava tube" };
    if (p->kind == CAVE_VERTICAL_TUBE) {
        const cave_vertical_basin* b = basin_in(&current, p->identity, p_ptr->depth);
        const cave_vertical_basin* peer = basin_in(&current, p->identity, p->peer_depth);
        strnfmt(text, size, "Lava tube. %s", peer && (peer->volume || peer->hot)
            ? "The adjoining conduit is still active or hot; wait for the whole passage to drain and cool."
            : b && b->volume
            ? b->material == FEAT_WATER ? "Water still fills the flooded passage."
                : "Molten rock still fills the passage."
            : b && b->hot ? "The drained rock is still dangerously hot."
            : "The passage is drained and cool; use descend or ascend here.");
        return true;
    }
    const level_image* peer = level_cache_floor(p->peer_depth);
    strnfmt(text, size, "%s. %s %d ft %s. %s%s", names[p->kind],
        p->peer_depth > p_ptr->depth ? "Continues" : "Arrives from", ABS(p->peer_depth - p_ptr->depth) * 50,
        p->peer_depth > p_ptr->depth ? "below" : "above",
        peer && peer->visited ? "The adjoining passage is explored. " : "The adjoining passage is unexplored. ",
        p->flags & CAVE_VERTICAL_CLOSED ? "The sluice is closed."
        : p->flags & CAVE_VERTICAL_REVERSIBLE ? "Use descend/ascend here; upstream travel is slower."
        : "Dangerous drop; there is no climb back here.");
    if (current.submerged && y == p_ptr->py && x == p_ptr->px) {
        size_t used = SDL_strlen(text);
        strnfmt(text + used, size > used ? size - used : 0, " Air remaining: %d actions.", current.breath);
    }
    return true;
}

void cave_vertical_terrain_changed(int y, int x, int old_feature)
{
    if (changing_terrain) return;
    int feature = cave_feat[y][x];
    const cave_vertical_portal* gate = portal_at(y, x, true);
    if (gate && (feature == FEAT_BROKEN || feature == FEAT_FLOOR || liquid(feature))
        && ((old_feature >= FEAT_DOOR_HEAD && old_feature <= FEAT_DOOR_TAIL)
            || old_feature == FEAT_OPEN)) {
        u32b identity = gate->identity;
        for (int i = 0; i < current.portal_count; i++)
            if (current.portals[i].identity == identity) {
                current.portals[i].flags &= ~CAVE_VERTICAL_CLOSED;
                current.portals[i].flags |= CAVE_VERTICAL_BROKEN;
            }
        for (int i = 0; i < current.basin_count; i++)
            if (current.basins[i].identity == identity && current.basins[i].depth == p_ptr->depth)
                current.basins[i].flags |= CAVE_VERTICAL_RESERVOIR;
        cave_events_terrain_changed();
    }
    for (int i = 0; i < current.portal_count; i++) {
        cave_vertical_portal* p = &current.portals[i];
        int dy = y - p->y, dx = x - p->x;
        if (ABS(dy) <= 3 && ABS(dx) <= 3 && p->local_patch[dy + 3][dx + 3])
            p->local_patch[dy + 3][dx + 3] = cave_feat[y][x];
    }
    if (!character_dungeon || !p_ptr->playing || level_cache_in_worker()) return;
    for (int i = 0; i < current.basin_count; i++) {
        cave_vertical_basin* b = &current.basins[i];
        if (b->depth != p_ptr->depth || distance(y, x, b->y, b->x) > b->radius) continue;
        /* External floods/eruption fronts already paid their own finite
         * supply. Transfer only the volume represented by the changed cell. */
        if (liquid(feature) && material_of(feature) != material_of(old_feature)
            && !(freshwater(feature) && FEAT_IS_ICE(old_feature)))
            (void)cave_vertical_receive(&current, b->depth, b->identity, material_of(feature), 4);
        else if (liquid(old_feature) && feature == FEAT_FLOOR) {
            b->volume -= MIN(4, b->volume);
            b->flags |= CAVE_VERTICAL_CHANGED;
            if (old_feature == FEAT_LAVA) b->hot = 6;
        }
    }
}
