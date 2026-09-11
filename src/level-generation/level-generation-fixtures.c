#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "cave/cave-fixtures.h"

/* Decorative fixtures are static light sources.  Keep their gameplay light
 * radius separate from the player's carried torch radius. */
#define FIXTURE_CANDIDATE_COUNT 2
#define VAULT_FIXTURE_LIMIT 6
#define VAULT_FIXTURE_SPACING 4

static u32b fixture_hash(int y, int x, u32b salt)
{
    u32b hash = salt ^ (u32b)y * 0x9e3779b9u ^ (u32b)x * 0x85ebca6bu;

    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return hash;
}

static bool fixture_wall_feature(int feat)
{
    return feat >= FEAT_WALL_EXTRA && feat <= FEAT_WALL_SOLID;
}

static bool fixture_source_feature(int feat)
{
    return feat != FEAT_CHASM && feat != FEAT_RUBBLE
        && feat != FEAT_WATER && feat != FEAT_ICE
        && feat != FEAT_LAVA && feat != FEAT_POISON;
}

static bool fixture_light_tile_ok(int y, int x, bool allow_room_tiles)
{
    if (!in_bounds_fully(y, x))
        return false;
    if (!cave_floor_bold(y, x) && !fixture_wall_feature(cave_feat[y][x]))
        return false;
    if (!allow_room_tiles && (cave_info[y][x] & (CAVE_ROOM | CAVE_ICKY)))
        return false;
    return true;
}

static bool fixture_source_ok(int y, int x, bool allow_icky)
{
    if (!in_bounds_fully(y, x) || !cave_floor_bold(y, x))
        return false;
    if (!fixture_source_feature(cave_feat[y][x]))
        return false;
    if (!allow_icky && (cave_info[y][x] & CAVE_ICKY))
        return false;
    return true;
}

static bool fixture_is_room_light(int wall_y, int wall_x)
{
    static const int dy[4] = { -1, 1, 0, 0 };
    static const int dx[4] = { 0, 0, -1, 1 };

    if (cave_info[wall_y][wall_x] & (CAVE_ROOM | CAVE_ICKY))
        return true;

    for (int i = 0; i < 4; ++i)
    {
        int y = wall_y + dy[i];
        int x = wall_x + dx[i];
        if (in_bounds_fully(y, x) && cave_floor_bold(y, x)
            && (cave_info[y][x] & (CAVE_ROOM | CAVE_ICKY)))
            return true;
    }

    return false;
}

static int fixture_light_radius(byte kind)
{
    return kind == CAVE_FIXTURE_BRAZIER
        ? CAVE_FIXTURE_BRAZIER_LIGHT_RADIUS
        : CAVE_FIXTURE_TORCH_LIGHT_RADIUS;
}

/* Fixture radii are grid radii, so cardinal and diagonal directions use the
 * same number of squares. The general distance() helper intentionally uses a
 * different approximation for gameplay range calculations. */
static int fixture_grid_distance(int y1, int x1, int y2, int x2)
{
    return MAX(ABS(y1 - y2), ABS(x1 - x2));
}

/* Apply the light field from a fixture's walkable side.  LOS prevents a wall
 * fixture from lighting through the wall behind it, while the endpoint wall
 * itself is explicitly included so the flame remains visible. */
void apply_fixture_light_area(
    int source_y, int source_x, int radius, bool allow_room_tiles)
{
    if (!in_bounds_fully(source_y, source_x) || radius < 0)
        return;

    for (int y = source_y - radius; y <= source_y + radius; ++y)
    {
        for (int x = source_x - radius; x <= source_x + radius; ++x)
        {
            if (!in_bounds_fully(y, x)
                || fixture_grid_distance(source_y, source_x, y, x) > radius
                || !fixture_light_tile_ok(y, x, allow_room_tiles))
                continue;
            if ((y != source_y || x != source_x)
                && !los(source_y, source_x, y, x))
                continue;
            cave_info[y][x] |= CAVE_GLOW;
        }
    }
}

void apply_cave_fixture_glow(
    int wall_y, int wall_x, int source_y, int source_x,
    bool allow_room_tiles)
{
    if (!in_bounds_fully(wall_y, wall_x)
        || !fixture_wall_feature(cave_feat[wall_y][wall_x]))
        return;

    cave_info[wall_y][wall_x] |= CAVE_GLOW;
    apply_fixture_light_area(source_y, source_x,
        fixture_light_radius(cave_fixture_at(wall_y, wall_x)),
        allow_room_tiles);
}

/* Rebuild fixture light after the natural-cave/labyrinth glow policy has
 * cleared permanent room lighting.  Fixture identity is saved, but the
 * walkable side is intentionally derived from the final terrain. */
void reapply_cave_fixture_glow(void)
{
    static const int dy[4] = { -1, 1, 0, 0 };
    static const int dx[4] = { 0, 0, -1, 1 };

    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!cave_fixture_at(y, x))
                continue;

            bool allow_room_tiles = fixture_is_room_light(y, x);
            int source_y = y;
            int source_x = x;
            bool found_source = false;

            for (int i = 0; i < 4; ++i)
            {
                int sy = y + dy[i];
                int sx = x + dx[i];
                if (!fixture_source_ok(sy, sx, allow_room_tiles))
                    continue;
                source_y = sy;
                source_x = sx;
                found_source = true;
                break;
            }

            if (found_source)
            {
                apply_cave_fixture_glow(y, x, source_y, source_x,
                    allow_room_tiles);
            }
            else
            {
                /* Keep an orphaned-but-valid wall fixture visible. */
                cave_info[y][x] |= CAVE_GLOW;
            }
        }
    }
}

static bool wall_fixture_candidate(
    int y, int x, bool allow_icky, bool natural_cave, bool inner_wall_only,
    coord* source)
{
    static const int dy[4] = { -1, 1, 0, 0 };
    static const int dx[4] = { 0, 0, -1, 1 };

    if (!in_bounds_fully(y, x) || !fixture_wall_feature(cave_feat[y][x]))
        return false;
    if (cave_fixture_at(y, x))
        return false;
    if (inner_wall_only && cave_feat[y][x] != FEAT_WALL_INNER)
        return false;
    if ((cave_info[y][x] & CAVE_G_VAULT) && !allow_icky)
        return false;
    if (!allow_icky && (cave_info[y][x] & CAVE_ICKY))
        return false;
    if (natural_cave && (cave_info[y][x] & CAVE_CHASM_AREA))
        return false;

    for (int i = 0; i < 4; ++i)
    {
        int sy = y + dy[i];
        int sx = x + dx[i];
        if (!fixture_source_ok(sy, sx, allow_icky))
            continue;
        if (!(cave_info[sy][sx] & (CAVE_ROOM | CAVE_ICKY)))
            continue;
        if (natural_cave && (cave_info[sy][sx] & CAVE_ICKY))
            continue;
        source->y = (byte)sy;
        source->x = (byte)sx;
        return true;
    }

    return false;
}

/* Procedural caves and rooms retain their two-position reservoir. */
static int sample_wall_fixture_candidates(
    rectangle bounds, bool allow_icky, bool natural_cave,
    bool inner_wall_only,
    coord walls[FIXTURE_CANDIDATE_COUNT],
    coord sources[FIXTURE_CANDIDATE_COUNT])
{
    int y1 = MAX(1, bounds.y1 - 1);
    int x1 = MAX(1, bounds.x1 - 1);
    int y2 = MIN(p_ptr->cur_map_hgt - 2, bounds.y2 + 1);
    int x2 = MIN(p_ptr->cur_map_wid - 2, bounds.x2 + 1);
    int sampled = 0;
    int seen = 0;

    for (int y = y1; y <= y2; ++y)
    {
        for (int x = x1; x <= x2; ++x)
        {
            coord source;
            if (!wall_fixture_candidate(y, x, allow_icky, natural_cave,
                    inner_wall_only, &source))
                continue;

            ++seen;
            if (sampled < FIXTURE_CANDIDATE_COUNT)
            {
                walls[sampled] = (coord){ (byte)y, (byte)x };
                sources[sampled] = source;
                ++sampled;
            }
            else if (rand_int(seen) < FIXTURE_CANDIDATE_COUNT)
            {
                int replace = rand_int(FIXTURE_CANDIDATE_COUNT);
                walls[replace] = (coord){ (byte)y, (byte)x };
                sources[replace] = source;
            }
        }
    }

    return sampled;
}

static byte random_room_fixture_kind(int brazier_percent)
{
    if (rand_int(100) < brazier_percent)
        return CAVE_FIXTURE_BRAZIER;
    return rand_int(2) ? CAVE_FIXTURE_WALL_TORCH_2
        : CAVE_FIXTURE_WALL_TORCH;
}

static int count_fixtures(rectangle bounds)
{
    int count = 0;
    int y1 = MAX(1, bounds.y1 - 1);
    int x1 = MAX(1, bounds.x1 - 1);
    int y2 = MIN(p_ptr->cur_map_hgt - 2, bounds.y2 + 1);
    int x2 = MIN(p_ptr->cur_map_wid - 2, bounds.x2 + 1);

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (cave_fixture_at(y, x) != CAVE_FIXTURE_NONE)
                ++count;
    return count;
}

static byte deterministic_room_fixture_kind(
    coord wall, u32b salt, int brazier_percent)
{
    u32b hash = fixture_hash(wall.y, wall.x, salt);
    if (hash % 100 < (u32b)brazier_percent)
        return CAVE_FIXTURE_BRAZIER;
    return (hash >> 8) & 1
        ? CAVE_FIXTURE_WALL_TORCH_2 : CAVE_FIXTURE_WALL_TORCH;
}

static int place_sampled_fixtures(
    const coord* walls, const coord* sources, int candidate_count,
    int target, bool allow_room_tiles, bool rear_only, u32b salt,
    int brazier_percent, bool deterministic)
{
    int placed = 0;
    target = MIN(target, candidate_count);

    for (int i = 0; i < target; ++i)
    {
        int y = walls[i].y;
        int x = walls[i].x;
        byte kind = rear_only ? CAVE_FIXTURE_WALL_TORCH_2
            : (deterministic
                ? deterministic_room_fixture_kind(
                    walls[i], salt, brazier_percent)
                : random_room_fixture_kind(brazier_percent));
        cave_fixture_set(y, x, kind);
        apply_cave_fixture_glow(y, x, sources[i].y, sources[i].x,
            allow_room_tiles);
        ++placed;
    }

    return placed;
}

static int place_cave_fixtures(rectangle bounds)
{
    coord walls[FIXTURE_CANDIDATE_COUNT];
    coord sources[FIXTURE_CANDIDATE_COUNT];
    int existing = count_fixtures(bounds);
    int target;

    if (existing >= FIXTURE_CANDIDATE_COUNT)
        return 0;

    /* Cave fixtures are deliberately uncommon, but a chosen cave may have
     * one or two rear torches. */
    if (!one_in_(3))
        return 0;

    target = 1 + one_in_(2);
    target = MIN(target, FIXTURE_CANDIDATE_COUNT - existing);
    int candidates = sample_wall_fixture_candidates(
        bounds, false, true, false, walls, sources);
    return place_sampled_fixtures(walls, sources, candidates, target, true,
        true, 0, 0, false);
}

static int room_brazier_percent(rectangle bounds)
{
    int height = bounds.y2 - bounds.y1 + 1;
    int width = bounds.x2 - bounds.x1 + 1;
    int area = MAX(0, height) * MAX(0, width);

    if (area >= 180)
        return 55;
    if (area >= 90)
        return 35;
    return 15;
}

static int vault_brazier_percent(int vault_type)
{
    if (vault_type >= 8)
        return 70;
    if (vault_type == 7)
        return 45;
    return 20;
}

static int place_room_fixtures(rectangle bounds)
{
    coord walls[FIXTURE_CANDIDATE_COUNT];
    coord sources[FIXTURE_CANDIDATE_COUNT];

    /* Keep ordinary-room fixtures sparse; larger rooms can get a second one.
     * Ordinary rooms may use braziers as well as torches. */
    if (!one_in_(3))
        return 0;

    int target = 1 + one_in_(4);
    int candidates = sample_wall_fixture_candidates(
        bounds, false, false, false, walls, sources);
    return place_sampled_fixtures(walls, sources, candidates, target, true,
        false, 0, room_brazier_percent(bounds), false);
}

/* Invert build_vault's mirrors and optional transpose. Looking up the actual
 * template also excludes padded holes and neighboring rooms inside the box. */
static char vault_fixture_symbol(int y, int x, int y0, int x0,
    const vault_type* v_ptr, bool flip_v, bool flip_h, bool flip_d)
{
    int ay = (flip_d ? x - x0 : y - y0) + v_ptr->hgt / 2;
    int ax = (flip_d ? y - y0 : x - x0) + v_ptr->wid / 2;

    if (ay < 0 || ay >= v_ptr->hgt || ax < 0 || ax >= v_ptr->wid)
        return ' ';
    if (flip_v) ay = v_ptr->hgt - 1 - ay;
    if (flip_h) ax = v_ptr->wid - 1 - ax;
    return v_text[v_ptr->text + ay * v_ptr->wid + ax];
}

int place_vault_template_fixtures(int y0, int x0, const vault_type* v_ptr,
    bool flip_v, bool flip_h, bool flip_d)
{
    coord walls[VAULT_FIXTURE_LIMIT];
    coord sources[VAULT_FIXTURE_LIMIT];
    int candidates = 0;
    int floor_area = 0;

    if (!v_ptr || !v_text || !(v_ptr->flags & VLT_TORCHES)
        || (v_ptr->flags & (VLT_QUEST | VLT_SURFACE))
        || v_ptr->typ < 6 || v_ptr->typ > 8
        || !v_ptr->hgt || !v_ptr->wid)
        return 0;

    int height = flip_d ? v_ptr->wid : v_ptr->hgt;
    int width = flip_d ? v_ptr->hgt : v_ptr->wid;
    int y1 = MAX(1, y0 - height / 2);
    int x1 = MAX(1, x0 - width / 2);
    int y2 = MIN(p_ptr->cur_map_hgt - 2, y0 - height / 2 + height - 1);
    int x2 = MIN(p_ptr->cur_map_wid - 2, x0 - width / 2 + width - 1);

    for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x)
            if (cave_feat[y][x] == FEAT_FLOOR
                && vault_fixture_symbol(y, x, y0, x0, v_ptr,
                    flip_v, flip_h, flip_d) != ' ')
                ++floor_area;

    int target = v_ptr->typ == 8 ? VAULT_FIXTURE_LIMIT
        : v_ptr->typ == 7 ? (floor_area < 90 ? 3 : floor_area < 180 ? 4 : 5)
        : (floor_area < 20 ? 1 : floor_area < 90 ? 2 : 3);

    /* Vault flags are authored data, so choose the wall positions and fixture
     * style deterministically. This adds no generation RNG noise to the
     * monsters, objects, or later vault selection. */
    u32b salt = v_ptr->name ^ (v_ptr->text * 0x9e3779b9u)
        ^ ((u32b)v_ptr->typ << 24);

    /* At most six full scans keep storage bounded and let rejected nearby
     * candidates yield to well-spaced walls elsewhere in the vault. */
    while (candidates < target)
    {
        bool found = false;
        u32b best_score = 0;
        coord best_wall = { 0, 0 }, best_source = { 0, 0 };
        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                coord source;
                if (vault_fixture_symbol(y, x, y0, x0, v_ptr,
                        flip_v, flip_h, flip_d) != '#'
                    || !wall_fixture_candidate(y, x, true, false, true, &source)
                    || cave_feat[source.y][source.x] != FEAT_FLOOR
                    || vault_fixture_symbol(source.y, source.x, y0, x0, v_ptr,
                        flip_v, flip_h, flip_d) != '.')
                    continue;

                bool separated = true;
                for (int i = 0; i < candidates; ++i)
                    if (distance(source.y, source.x, sources[i].y, sources[i].x)
                        < VAULT_FIXTURE_SPACING)
                    {
                        separated = false;
                        break;
                    }
                if (!separated)
                    continue;

                u32b score = fixture_hash(y, x, salt);
                if (!found || score < best_score)
                {
                    best_score = score;
                    best_wall = (coord){ (byte)y, (byte)x };
                    best_source = source;
                    found = true;
                }
            }
        }
        if (!found)
            break;
        walls[candidates] = best_wall;
        sources[candidates++] = best_source;
    }

    int placed = place_sampled_fixtures(walls, sources, candidates, target,
        true, false, salt, vault_brazier_percent(v_ptr->typ), true);
    if (placed)
    {
        log_debug("Vault '%s': placed %d decorative fixture(s)",
            v_name + v_ptr->name, placed);
    }
    return placed;
}

int place_generation_fixtures(void)
{
    int caves = 0;
    int rooms = 0;

    for (int i = 0; i < dun->cent_n; ++i)
    {
        rectangle bounds = dun->corner[i];
        int placed;

        if (room_anchor_kind[i] == LAYOUT_ANCHOR_CA_BLOB)
        {
            placed = place_cave_fixtures(bounds);
            caves += placed;
        }
        else if (!room_kind_is_vault(dun->kind[i])
            && (room_anchor_kind[i] == LAYOUT_ANCHOR_ROOM
                || room_anchor_kind[i] == LAYOUT_ANCHOR_NONE))
        {
            placed = place_room_fixtures(bounds);
            rooms += placed;
        }
    }

    if (caves || rooms)
    {
        log_debug("Generation fixtures: %d cave rear torches, %d room fixtures",
            caves, rooms);
    }

    return caves + rooms;
}
