/* File: level-generation-connectivity.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "level-generation/level-generation.h"
#include "log/log.h"
#include "level-generation/gen-log.h"
#include "metarun.h"
#include "level-generation/level-generation-internal.h"
#include <string.h>

/* Dungeon streamer generation values */
#define DUN_STR_DEN 5 /* Density of streamers */
#define DUN_STR_RNG 2 /* Width of streamers */
#define DUN_STR_QUA 4 /* Number of quartz streamers */

int choose_up_stairs(void);
int choose_down_stairs(void);
bool build_streamer(int feat);
void build_chasms(void);
bool alloc_stairs(int feat, int num);

bool feature_is_any_door(int feat)
{
    return (feat == FEAT_SECRET) || (feat == FEAT_OPEN) || (feat == FEAT_BROKEN)
        || ((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL));
}

/* Organic cave/blob anchors should meet corridors as open floor, not doors.
 * Chasm anchors reuse the same kind marker for shaping, so exclude them. */
static bool room_prefers_floor_thresholds(int room_idx)
{
    int cy, cx;

    if (room_idx < 0 || room_idx >= dun->cent_n || room_idx >= CENT_MAX)
        return false;
    if (room_anchor_kind[room_idx] != LAYOUT_ANCHOR_CA_BLOB)
        return false;

    cy = dun->cent[room_idx].y;
    cx = dun->cent[room_idx].x;
    if (!in_bounds_fully(cy, cx))
        return false;

    return ((cave_info[cy][cx] & CAVE_CHASM_AREA) == 0);
}

static bool tunnel_prefers_floor_thresholds(int r1, int r2)
{
    return room_prefers_floor_thresholds(r1)
        || room_prefers_floor_thresholds(r2);
}

static void carve_floor_threshold(
    int y, int x, int r1, int r2, bool mark_escape)
{
    cave_set_feat(y, x, FEAT_FLOOR);
    cave_corridor1[y][x] = r1;
    cave_corridor2[y][x] = r2;
    if (mark_escape)
        mark_generation_escape_tunnel(y, x);
}

/* Collapse adjacent doors outside vaults to avoid double-door seams */
int squash_double_doors(void)
{
    int removed = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
    {
        for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
        {
            if (!feature_is_any_door(cave_feat[y][x])) continue;
            if (cave_info[y][x] & (CAVE_ICKY)) continue;

            /* Only clear east/south neighbors to keep at most one door */
            int ny = y, nx = x + 1;
            if ((nx < p_ptr->cur_map_wid - 1) &&
                !(cave_info[ny][nx] & (CAVE_ICKY)) &&
                feature_is_any_door(cave_feat[ny][nx]))
            {
                cave_set_feat(ny, nx, FEAT_FLOOR);
                removed++;
            }
            ny = y + 1; nx = x;
            if ((ny < p_ptr->cur_map_hgt - 1) &&
                !(cave_info[ny][nx] & (CAVE_ICKY)) &&
                feature_is_any_door(cave_feat[ny][nx]))
            {
                cave_set_feat(ny, nx, FEAT_FLOOR);
                removed++;
            }
        }
    }
    log_trace("squash_double_doors: converted %d adjacent doors to floor", removed);
    return removed;
}

/* determines whether the player can pass through a given feature */
/* icky locations (inside vaults) are all considered passable.    */
bool player_passable(int y, int x, bool ignore_rubble_and_chasms)
{
    if (!in_bounds_fully(y, x)) return false;

    byte feature = cave_feat[y][x];
    bool icky_interior = (cave_info[y][x] & (CAVE_ICKY))
        && (cave_info[y][x - 1] & (CAVE_ICKY))
        && (cave_info[y][x + 1] & (CAVE_ICKY))
        && (cave_info[y - 1][x] & (CAVE_ICKY))
        && (cave_info[y + 1][x] & (CAVE_ICKY));

    if ((feature < FEAT_WALL_HEAD) || (feature > FEAT_WALL_TAIL))
    {
        return !((feature == FEAT_CHASM) && !ignore_rubble_and_chasms);
    }
    else
    {
        return (feature == FEAT_SECRET)
            || ((feature >= FEAT_DOOR_HEAD) && (feature <= FEAT_DOOR_TAIL))
            || ((feature == FEAT_RUBBLE) && ignore_rubble_and_chasms)
            || icky_interior;
    }
}

/* floodfills access through the dungeon, marking all accessible squares with
 * true */
void flood_access(int y, int x,
    int access_array[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    bool ignore_rubble_and_chasms)
{
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int flood_ddy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int flood_ddx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int head = 0;
    int tail = 0;

    if (!in_bounds_fully(y, x)) return;
    if (access_array[y][x]) return;
    if (!player_passable(y, x, ignore_rubble_and_chasms)) return;

    access_array[y][x] = true;
    queue[tail++] = y * MAX_DUNGEON_WID + x;

    while (head < tail)
    {
        int idx = queue[head++];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;

        for (int d = 0; d < 8; ++d)
        {
            int ny = cy + flood_ddy[d];
            int nx = cx + flood_ddx[d];

            if (!in_bounds_fully(ny, nx)) continue;
            if (access_array[ny][nx]) continue;
            if (!player_passable(ny, nx, ignore_rubble_and_chasms)) continue;

            access_array[ny][nx] = true;
            if (tail < (int)N_ELEMENTS(queue))
            {
                queue[tail++] = ny * MAX_DUNGEON_WID + nx;
            }
        }
    }
}

void label_rooms(void)
{
    int i;

    for (i = 0; i < dun->cent_n; i++)
    {
        // cave_feat[dun->corner[i].y1][dun->corner[i].x1] = 5 + 1;
        // cave_feat[dun->corner[i].y1][dun->corner[i].x2] = 5 + 1;
        // cave_feat[dun->corner[i].y2][dun->corner[i].x1] = 5 + 1;
        // cave_feat[dun->corner[i].y2][dun->corner[i].x2] = 5 + 1;

        cave_feat[dun->cent[i].y][dun->cent[i].x] = 5 + (i % 10);
        if (i > 9)
        {
            cave_feat[dun->cent[i].y][dun->cent[i].x - 1] = 5 + ((i / 10) % 10);
        }
        if (i > 99)
        {
            cave_feat[dun->cent[i].y][dun->cent[i].x - 1]
                = 5 + ((i / 100) % 10);
        }
    }
}

// floodfills access through the *graph* of the dungeon
static void flood_piece(int n, int piece_num)
{
    int i;

    dun->piece[n] = piece_num;

    for (i = 0; i < dun->cent_n; i++)
    {
        if (dun->connection[n][i] && (dun->piece[i] == 0))
        {
            flood_piece(i, piece_num);
        }
    }
    return;
}

int dungeon_pieces(void)
{
    int piece_num;
    int i;

    // first reset the pieces
    for (i = 0; i < dun->cent_n; i++)
    {
        dun->piece[i] = 0;
    }

    for (piece_num = 1; piece_num <= dun->cent_n; piece_num++)
    {
        // find the next room that doesn't belong to a piece
        for (i = 0; (i < dun->cent_n) && (dun->piece[i] != 0); i++)
            ;

        if (i == dun->cent_n)
        {
            break;
        }
        else
        {
            flood_piece(i, piece_num);
        }
    }

    return (piece_num - 1);
}


static bool h_tunnel_ok(
    int x1, int x2, int y, bool tentative, int desired_changes)
{
    int x, x_lo, x_hi, changes;

    x_lo = MIN(x1, x2);
    x_hi = MAX(x1, x2);
    changes = 0;

    /* Don't dig corridors ending at a room's outer wall (can happen at corners
     * of L-corridors) */
    if ((cave_feat[y][x1] == FEAT_WALL_OUTER)
        || (cave_feat[y][x2] == FEAT_WALL_OUTER))
        return (false);
    /* Don't dig L-corridors when the corner is too close to non-room empty space.
     * But allow corners near CAVE_ROOM floor (from caves, chasms, etc.) */
    if (!(cave_info[y][x_lo] & (CAVE_ROOM)))
    {
        bool blocked_lo = false;
        if (cave_feat[y - 1][x_lo - 1] == FEAT_FLOOR && !(cave_info[y - 1][x_lo - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (cave_feat[y + 1][x_lo - 1] == FEAT_FLOOR && !(cave_info[y + 1][x_lo - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (blocked_lo)
            return (false);
    }
    if (!(cave_info[y][x_hi] & (CAVE_ROOM)))
    {
        bool blocked_hi = false;
        if (cave_feat[y - 1][x_hi + 1] == FEAT_FLOOR && !(cave_info[y - 1][x_hi + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (cave_feat[y + 1][x_hi + 1] == FEAT_FLOOR && !(cave_info[y + 1][x_hi + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (blocked_hi)
            return (false);
    }

    /* test each location in the corridor */
    for (x = x_lo; x <= x_hi; x++)
    {
        /* count the number of times it enters or leaves a room */
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER) && // to outside
            (cave_floor_bold(y, x - 1)
                || (cave_feat[y][x - 1] == FEAT_WALL_INNER))) // from inside
        {
            changes++;
        }
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && // from outside
            (cave_floor_bold(y, x)
                || (cave_feat[y][x] == FEAT_WALL_INNER))) // to inside
        {
            changes++;
        }

        /* abort if the tunnel would go through two adjacent squares of the
         * outside wall of a room */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_OUTER))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall to a door */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }
        /* abort if the tunnel would go from a door to an outside wall */
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x - 1] == FEAT_DOOR_HEAD))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall into an inside wall
         */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_INNER))
        {
            return (false);
        }
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x - 1] == FEAT_WALL_INNER))
        {
            return (false);
        }

        /* abort if the tunnel would directly enter a vault without going
         * through a designated square */
        if ((x > x_lo) && (cave_feat[y][x - 1] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }
        if ((x > x_lo) && (cave_feat[y][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x - 1)
                || (cave_feat[y][x - 1] == FEAT_WALL_INNER)))
        {
            return (false);
        }

        /* abort if the tunnel would go through or adjacent to an existing door
         * (except in vaults) */
        if (cave_known_closed_door_bold(y - 1, x)
            && !(cave_info[y - 1][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x)
            && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y + 1, x)
            && !(cave_info[y + 1][x] & (CAVE_ICKY)))
        {
            return (false);
        }

        /* abort if the tunnel would have floor beside it at some point outside
         * a room, UNLESS that adjacent floor is part of a CAVE_ROOM (cave edges) */
        if (!(cave_info[y][x] & (CAVE_ROOM)))
        {
            bool has_non_room_floor_adj = false;
            if (cave_feat[y + 1][x] == FEAT_FLOOR && !(cave_info[y + 1][x] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (cave_feat[y - 1][x] == FEAT_FLOOR && !(cave_info[y - 1][x] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (has_non_room_floor_adj)
            {
                return (false);
            }
        }
    }
    if (tentative && (changes != desired_changes))
    {
        return (false);
    }
    else
    {
        return (true);
    }
}

static bool v_tunnel_ok(
    int y1, int y2, int x, bool tentative, int desired_changes)
{
    int y, y_lo, y_hi, changes;

    y_lo = MIN(y1, y2);
    y_hi = MAX(y1, y2);
    changes = 0;

    /* Don't dig corridors ending at a room's outer wall (can happen at corners
     * of L-corridors) */
    if ((cave_feat[y1][x] == FEAT_WALL_OUTER)
        || (cave_feat[y2][x] == FEAT_WALL_OUTER))
        return (false);
    /* Don't dig L-corridors when the corner is too close to non-room empty space.
     * But allow corners near CAVE_ROOM floor (from caves, chasms, etc.) */
    if (!(cave_info[y_lo][x] & (CAVE_ROOM)))
    {
        bool blocked_lo = false;
        if (cave_feat[y_lo - 1][x - 1] == FEAT_FLOOR && !(cave_info[y_lo - 1][x - 1] & CAVE_ROOM))
            blocked_lo = true;
        if (cave_feat[y_lo - 1][x + 1] == FEAT_FLOOR && !(cave_info[y_lo - 1][x + 1] & CAVE_ROOM))
            blocked_lo = true;
        if (blocked_lo)
            return (false);
    }
    if (!(cave_info[y_hi][x] & (CAVE_ROOM)))
    {
        bool blocked_hi = false;
        if (cave_feat[y_hi + 1][x - 1] == FEAT_FLOOR && !(cave_info[y_hi + 1][x - 1] & CAVE_ROOM))
            blocked_hi = true;
        if (cave_feat[y_hi + 1][x + 1] == FEAT_FLOOR && !(cave_info[y_hi + 1][x + 1] & CAVE_ROOM))
            blocked_hi = true;
        if (blocked_hi)
            return (false);
    }

    /* test each location in the corridor */
    for (y = y_lo; y <= y_hi; y++)
    {
        /* count the number of times it enters or leaves a room */
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_floor_bold(y - 1, x)
                || (cave_feat[y - 1][x] == FEAT_WALL_INNER)))
        {
            changes++;
        }
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            changes++;
        }

        /* abort if the tunnel would go through two adjacent squares of the
         * outside wall of a room */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_OUTER))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall to a door */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }
        /* abort if the tunnel would go from a door to an outside wall */
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y - 1][x] == FEAT_DOOR_HEAD))
        {
            return (false);
        }

        /* abort if the tunnel would go from an outside wall into an inside wall
         */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_OUTER)
            && (cave_feat[y][x] == FEAT_WALL_INNER))
        {
            return (false);
        }
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_OUTER)
            && (cave_feat[y - 1][x] == FEAT_WALL_INNER))
        {
            return (false);
        }

        /* abort if the tunnel would directly enter a vault without going
         * through a designated square */
        if ((y > y_lo) && (cave_feat[y - 1][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }
        if ((y > y_lo) && (cave_feat[y][x] == FEAT_WALL_EXTRA)
            && (cave_floor_bold(y - 1, x)
                || (cave_feat[y - 1][x] == FEAT_WALL_INNER)))
        {
            return (false);
        }

        /* abort if the tunnel would go through, or adjacent to an existing
         * (non-vault) door */
        if (cave_known_closed_door_bold(y, x - 1)
            && !(cave_info[y][x - 1] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x)
            && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            return (false);
        }
        if (cave_known_closed_door_bold(y, x + 1)
            && !(cave_info[y][x + 1] & (CAVE_ICKY)))
        {
            return (false);
        }

        /* abort if the tunnel would have floor beside it at some point outside
         * a room, UNLESS that adjacent floor is part of a CAVE_ROOM (cave edges) */
        if (!(cave_info[y][x] & (CAVE_ROOM)))
        {
            bool has_non_room_floor_adj = false;
            if (cave_feat[y][x + 1] == FEAT_FLOOR && !(cave_info[y][x + 1] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (cave_feat[y][x - 1] == FEAT_FLOOR && !(cave_info[y][x - 1] & CAVE_ROOM))
                has_non_room_floor_adj = true;
            if (has_non_room_floor_adj)
            {
                return (false);
            }
        }
    }
    if (tentative && (changes != desired_changes))
    {
        return (false);
    }
    else
    {
        return (true);
    }
}

typedef enum {
    TUNNEL_TREAT_NONE = 0,
    TUNNEL_TREAT_NICHES,
    TUNNEL_TREAT_PILLARS
} tunnel_treatment;

typedef struct tunnel_profile {
    byte width;          /* 1 = normal, 2 = offset double, 3 = grand hall */
    int side_bias;       /* -1/0/1: which side to favour when width == 2 */
    tunnel_treatment treatment;
} tunnel_profile;

static const tunnel_profile TUNNEL_PROFILE_NORMAL = {1, 0, TUNNEL_TREAT_NONE};

static tunnel_profile choose_tunnel_profile(bool tentative)
{
    tunnel_profile profile = TUNNEL_PROFILE_NORMAL;

    /* On shallow branches, fall back to narrow connectors */
    if (tentative)
    {
        /* allow style variation even on tentative digs */
    }

    int depth = p_ptr->depth;
    int sidx = styles_get_level_primary_style();
    byte style_group = (sidx >= 0 && style_info) ? style_info[sidx].group : 0;
    bool style_grand = (style_group >= 4); /* warmer/darker palettes get a bump */

    /* Variable tunnel widths at any depth, probability scales with depth */
    /* Base rarity values (lower = more common) */
    int medium_rarity, grand_rarity;
    
    if (depth >= 20)
    {
        medium_rarity = style_grand ? 5 : 7;
        grand_rarity = style_grand ? 8 : 12;
    }
    else if (depth >= 12)
    {
        medium_rarity = style_grand ? 7 : 10;
        grand_rarity = style_grand ? 11 : 16;
    }
    else if (depth >= 7)
    {
        medium_rarity = style_grand ? 10 : 14;
        grand_rarity = style_grand ? 16 : 22;
    }
    else
    {
        /* Even early levels can have occasional wider corridors */
        medium_rarity = style_grand ? 16 : 20;
        grand_rarity = style_grand ? 25 : 30;
    }

    if (one_in_(grand_rarity))
    {
        profile.width = 3;
        profile.treatment = one_in_(3) ? TUNNEL_TREAT_PILLARS : TUNNEL_TREAT_NICHES;
    }
    else if (one_in_(medium_rarity))
    {
        profile.width = one_in_(4) ? 3 : 2;
        profile.side_bias = one_in_(2) ? 1 : -1;
        profile.treatment = one_in_(3) ? TUNNEL_TREAT_NICHES : TUNNEL_TREAT_NONE;
    }

    return profile;
}

static void apply_tunnel_niche_torch_glow(int niche_y, int niche_x, int front_dy, int front_dx)
{
    if (!in_bounds_fully(niche_y, niche_x))
        return;

    /* "Torch" effect (radius 1) biased into the corridor:
     * - light the niche floor itself
     * - light the two wall tiles flanking the niche (along the corridor axis)
     * - light the 3 corridor floor tiles directly in front of the niche
     */
    int axis_dy = (front_dx != 0) ? 1 : 0;
    int axis_dx = (front_dy != 0) ? 1 : 0;

    if (cave_floor_bold(niche_y, niche_x)
        && !(cave_info[niche_y][niche_x] & (CAVE_ROOM | CAVE_ICKY)))
    {
        cave_info[niche_y][niche_x] |= (CAVE_GLOW);
    }

    for (int i = -RADIUS_TORCH; i <= RADIUS_TORCH; i += 2 * RADIUS_TORCH)
    {
        int wy = niche_y + axis_dy * i;
        int wx = niche_x + axis_dx * i;
        if (!in_bounds_fully(wy, wx))
            continue;
        if (cave_info[wy][wx] & (CAVE_ROOM | CAVE_ICKY))
            continue;
        if (cave_wall_bold(wy, wx))
            cave_info[wy][wx] |= (CAVE_GLOW);
    }

    int entry_y = niche_y + front_dy;
    int entry_x = niche_x + front_dx;
    for (int i = -RADIUS_TORCH; i <= RADIUS_TORCH; ++i)
    {
        int fy = entry_y + axis_dy * i;
        int fx = entry_x + axis_dx * i;
        if (!in_bounds_fully(fy, fx))
            continue;
        if (!cave_floor_bold(fy, fx))
            continue;
        if (cave_info[fy][fx] & (CAVE_ROOM | CAVE_ICKY))
            continue;
        cave_info[fy][fx] |= (CAVE_GLOW);
    }
}

static void apply_v_tunnel_treatment(
    int r1, int r2, int y_lo, int y_hi, int x, bool widen_west, bool widen_east,
    const tunnel_profile* profile, bool mark_escape)
{
    if (!profile)
        return;

    /* Side niches sit just outside the carved width */
    if (profile->treatment == TUNNEL_TREAT_NICHES)
    {
        int offset = (profile->width >= 3) ? 2 : 1;
        int side = 0;
        if (widen_west && widen_east)
            side = one_in_(2) ? -offset : offset;
        else if (widen_west)
            side = -offset;
        else if (widen_east)
            side = offset;
        else
            side = one_in_(2) ? -offset : offset;

        int y = y_lo + 2 + rand_int(3);
        while (y < y_hi - 1)
        {
            int nx = x + side;
            if (in_bounds_fully(y, nx) && cave_feat[y][nx] == FEAT_WALL_EXTRA
                && !(cave_info[y][nx] & (CAVE_ROOM | CAVE_ICKY)))
            {
                cave_set_feat(y, nx, FEAT_FLOOR);
                cave_corridor1[y][nx] = r1;
                cave_corridor2[y][nx] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y, nx);

                int dir = (side > 0) ? 1 : -1;
                apply_tunnel_niche_torch_glow(y, nx, 0, -dir);
            }
            y += 3 + rand_int(3);
            side = -side; /* alternate sides */
        }
    }

    /* Pillar lines break up wide halls without blocking flow */
    if (profile->treatment == TUNNEL_TREAT_PILLARS && profile->width >= 3)
    {
        int y = y_lo + 2 + rand_int(2);
        while (y <= y_hi - 2)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
            {
                cave_set_feat(y, x, FEAT_WALL_EXTRA);
                cave_corridor1[y][x] = -1;
                cave_corridor2[y][x] = -1;
            }
            y += 3 + rand_int(2);
        }
    }
}

static void apply_h_tunnel_treatment(
    int r1, int r2, int x_lo, int x_hi, int y, bool widen_north, bool widen_south,
    const tunnel_profile* profile, bool mark_escape)
{
    if (!profile)
        return;

    if (profile->treatment == TUNNEL_TREAT_NICHES)
    {
        int offset = (profile->width >= 3) ? 2 : 1;
        int side = 0;
        if (widen_north && widen_south)
            side = one_in_(2) ? -offset : offset;
        else if (widen_north)
            side = -offset;
        else if (widen_south)
            side = offset;
        else
            side = one_in_(2) ? -offset : offset;

        int x = x_lo + 2 + rand_int(3);
        while (x < x_hi - 1)
        {
            int ny = y + side;
            if (in_bounds_fully(ny, x) && cave_feat[ny][x] == FEAT_WALL_EXTRA
                && !(cave_info[ny][x] & (CAVE_ROOM | CAVE_ICKY)))
            {
                cave_set_feat(ny, x, FEAT_FLOOR);
                cave_corridor1[ny][x] = r1;
                cave_corridor2[ny][x] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(ny, x);

                int dir = (side > 0) ? 1 : -1;
                apply_tunnel_niche_torch_glow(ny, x, -dir, 0);
            }
            x += 3 + rand_int(3);
            side = -side;
        }
    }

    if (profile->treatment == TUNNEL_TREAT_PILLARS && profile->width >= 3)
    {
        int x = x_lo + 2 + rand_int(2);
        while (x <= x_hi - 2)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
            {
                cave_set_feat(y, x, FEAT_WALL_EXTRA);
                cave_corridor1[y][x] = -1;
                cave_corridor2[y][x] = -1;
            }
            x += 3 + rand_int(2);
        }
    }
}

static void build_v_tunnel(
    int r1, int r2, int y1, int y2, int x, const tunnel_profile* profile)
{
    int y, y_lo, y_hi;
    tunnel_profile local = profile ? *profile : TUNNEL_PROFILE_NORMAL;
    int width = MAX(1, MIN(local.width, 3));
    bool mark_escape = tunnel_should_mark_escape(r1, r2);
    bool floor_thresholds = tunnel_prefers_floor_thresholds(r1, r2);
    bool short_span = (ABS(y2 - y1) < 4);
    if (short_span)
        local.treatment = TUNNEL_TREAT_NONE;
    if (short_span && width > 2)
        width = 2;

    bool widen_west = (width >= 3) || (width == 2 && local.side_bias < 0);
    bool widen_east = (width >= 3) || (width == 2 && local.side_bias > 0);

    y_lo = MIN(y1, y2);
    y_hi = MAX(y1, y2);

    for (y = y_lo; y <= y_hi; y++)
    {
        if (cave_feat[y][x] == FEAT_WALL_OUTER)
        {
            if (floor_thresholds)
            {
                carve_floor_threshold(y, x, r1, r2, mark_escape);
            }
            else
            {
                /* all doors get randomised later */
                cave_set_feat(y, x, FEAT_DOOR_HEAD);
            }
        }
        else if (cave_feat[y][x] == FEAT_WALL_EXTRA)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
            if (mark_escape)
                mark_generation_escape_tunnel(y, x);
        }

        /* thicken corridors when requested by carving adjacent granite only */
        if (width > 1)
        {
            if (widen_east && x + 1 < MAX_DUNGEON_WID
                && cave_feat[y][x + 1] == FEAT_WALL_EXTRA
                && in_bounds_fully(y, x + 1)
                && !(cave_info[y][x + 1] & (CAVE_ROOM)))
            {
                cave_set_feat(y, x + 1, FEAT_FLOOR);
                cave_corridor1[y][x + 1] = r1;
                cave_corridor2[y][x + 1] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y, x + 1);
            }
            if (widen_west && x - 1 > 0 && cave_feat[y][x - 1] == FEAT_WALL_EXTRA
                && in_bounds_fully(y, x - 1)
                && !(cave_info[y][x - 1] & (CAVE_ROOM)))
            {
                cave_set_feat(y, x - 1, FEAT_FLOOR);
                cave_corridor1[y][x - 1] = r1;
                cave_corridor2[y][x - 1] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y, x - 1);
            }
        }
    }

    apply_v_tunnel_treatment(r1, r2, y_lo, y_hi, x, widen_west, widen_east,
        &local, mark_escape);
}

static void build_h_tunnel(
    int r1, int r2, int x1, int x2, int y, const tunnel_profile* profile)
{
    int x, x_lo, x_hi;
    tunnel_profile local = profile ? *profile : TUNNEL_PROFILE_NORMAL;
    int width = MAX(1, MIN(local.width, 3));
    bool mark_escape = tunnel_should_mark_escape(r1, r2);
    bool floor_thresholds = tunnel_prefers_floor_thresholds(r1, r2);
    bool short_span = (ABS(x2 - x1) < 4);
    if (short_span)
        local.treatment = TUNNEL_TREAT_NONE;
    if (short_span && width > 2)
        width = 2;

    bool widen_south = (width >= 3) || (width == 2 && local.side_bias > 0);
    bool widen_north = (width >= 3) || (width == 2 && local.side_bias < 0);

    x_lo = MIN(x1, x2);
    x_hi = MAX(x1, x2);

    for (x = x_lo; x <= x_hi; x++)
    {
        if (cave_feat[y][x] == FEAT_WALL_OUTER)
        {
            if (floor_thresholds)
            {
                carve_floor_threshold(y, x, r1, r2, mark_escape);
            }
            else
            {
                /* all doors get randomised later */
                cave_set_feat(y, x, FEAT_DOOR_HEAD);
            }
        }
        else if (cave_feat[y][x] == FEAT_WALL_EXTRA)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
            cave_corridor1[y][x] = r1;
            cave_corridor2[y][x] = r2;
            if (mark_escape)
                mark_generation_escape_tunnel(y, x);
        }

        /* thicken corridors when requested by carving adjacent granite only */
        if (width > 1)
        {
            if (widen_south && y + 1 < MAX_DUNGEON_HGT
                && cave_feat[y + 1][x] == FEAT_WALL_EXTRA
                && in_bounds_fully(y + 1, x)
                && !(cave_info[y + 1][x] & (CAVE_ROOM)))
            {
                cave_set_feat(y + 1, x, FEAT_FLOOR);
                cave_corridor1[y + 1][x] = r1;
                cave_corridor2[y + 1][x] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y + 1, x);
            }
            if (widen_north && y - 1 > 0 && cave_feat[y - 1][x] == FEAT_WALL_EXTRA
                && in_bounds_fully(y - 1, x)
                && !(cave_info[y - 1][x] & (CAVE_ROOM)))
            {
                cave_set_feat(y - 1, x, FEAT_FLOOR);
                cave_corridor1[y - 1][x] = r1;
                cave_corridor2[y - 1][x] = r2;
                if (mark_escape)
                    mark_generation_escape_tunnel(y - 1, x);
            }
        }
    }

    apply_h_tunnel_treatment(r1, r2, x_lo, x_hi, y, widen_north, widen_south,
        &local, mark_escape);
}

static bool build_tunnel(
    int r1, int r2, int y1, int x1, int y2, int x2, bool tentative)
{
    tunnel_profile profile = choose_tunnel_profile(tentative);

    /* build a vertical tunnel */
    if (x1 == x2)
    {
        if (!v_tunnel_ok(y1, y2, x1, tentative, 2))
        {
            return (false);
        }
        build_v_tunnel(r1, r2, y1, y2, x1, &profile);
    }

    /* build a horizontal tunnel */
    else if (y1 == y2)
    {
        if (!h_tunnel_ok(x1, x2, y1, tentative, 2))
        {
            return (false);
        }
        build_h_tunnel(r1, r2, x1, x2, y1, &profile);
    }

    /* build an L-shaped tunnel */
    else
    {
        /* build an h-v tunnel */
        if (one_in_(2))
        {
            if (!h_tunnel_ok(x1, x2, y1, tentative, 1)
                || !v_tunnel_ok(y1, y2, x2, tentative, 1))
            {
                return (false);
            }
            build_h_tunnel(r1, r2, x1, x2, y1, &profile);
            build_v_tunnel(r1, r2, y1, y2, x2, &profile);
        }

        /* build a v-h tunnel */
        else
        {
            if (!h_tunnel_ok(x1, x2, y2, tentative, 1)
                || !v_tunnel_ok(y1, y2, x1, tentative, 1))
            {
                return (false);
            }
            build_v_tunnel(r1, r2, y1, y2, x1, &profile);
            build_h_tunnel(r1, r2, x1, x2, y2, &profile);
        }
    }

    return (true);
}

bool connect_two_rooms(int r1, int r2, bool tentative, bool desperate)
{
    int x, y;
    int r1y, r1x, r1y1, r1x1, r1y2, r1x2;
    int r2y, r2x, r2y1, r2x1, r2y2, r2x2;
    bool success;
    int morgoth_margin = 1;

    /* Allow long corridor spans across 3x3 partitions on 15x15 block maps */
    int base_limit_x = MAX(50, (p_ptr->cur_map_wid * 2) / 3); /* ~110 on 165x165 */
    int base_limit_y = MAX(35, (p_ptr->cur_map_hgt * 2) / 3); /* ~110 on 165x165 */
    int distance_limitx = desperate ? base_limit_x + base_limit_x / 2 : base_limit_x;
    int distance_limity = desperate ? base_limit_y + base_limit_y / 2 : base_limit_y;

    r1y = dun->cent[r1].y;
    r1x = dun->cent[r1].x;
    r1y1 = dun->corner[r1].y1;
    r1x1 = dun->corner[r1].x1;
    r1y2 = dun->corner[r1].y2;
    r1x2 = dun->corner[r1].x2;

    r2y = dun->cent[r2].y;
    r2x = dun->cent[r2].x;
    r2y1 = dun->corner[r2].y1;
    r2x1 = dun->corner[r2].x1;
    r2y2 = dun->corner[r2].y2;
    r2x2 = dun->corner[r2].x2;

    if (morgoth_region_active())
    {
        /* Skip any corridor that would cross the throne room partition */
        if (morgoth_segment_blocked(r1y, r1x, r2y, r2x, morgoth_margin))
            return false;
    }

    /* if the rooms are too far apart, then just give up immediately */
    // look at total distance of room centres
    if ((ABS(r1y - r2y) > distance_limity * 3)
        || (ABS(r1x - r2x) > distance_limitx * 3))
    {
        return (false);
    }
    // then look at distance of relevant room edges
    if ((r1x < r2x) && (r2x1 - r1x2 > distance_limitx))
    {
        return (false);
    }
    if ((r2x < r1x) && (r1x1 - r2x2 > distance_limitx))
    {
        return (false);
    }
    if ((r1y < r2y) && (r2y1 - r1y2 > distance_limity))
    {
        return (false);
    }
    if ((r2y < r1y) && (r1y1 - r2y2 > distance_limity))
    {
        return (false);
    }

    /* if we have vertical or horizontal overlap, connect a straight tunnel */
    /* at a random point where they overlap */

    /* if vertical overlap */
    if ((r1x1 <= r2x2) && (r2x1 <= r1x2))
    {
        /* unless careful, there will be too many vertical tunnels */
        /* since rooms are wider than they are tall                */
        if (tentative && one_in_(2))
        {
            return (false);
        }
        x = rand_range(MAX(r1x1, r2x1),
            MIN(r1x2,
                r2x2)); // Sil-x: one of these two lines has somehow caused a
                        // crash:
                        // http://angband.oook.cz/ladder-show.php?id=13070

        if (morgoth_segment_blocked(r1y, x, r2y, x, morgoth_margin))
            return false;
        success = build_tunnel(r1, r2, r1y, x, r2y, x, tentative);
    }
    /* if horizontal overlap */
    else if ((r1y1 <= r2y2) && (r2y1 <= r1y2))
    {
        y = rand_range(MAX(r1y1, r2y1),
            MIN(r1y2,
                r2y2)); // Sil-x: one of these two lines has somehow caused a
                        // crash

        if (morgoth_segment_blocked(y, r1x, y, r2x, morgoth_margin))
            return false;
        success = build_tunnel(r1, r2, y, r1x, y, r2x, tentative);
    }

    /* otherwise, make an L shaped corridor between their centres */
    else
    {
        // this must fail if any of the tunnels would be too long
        if (MIN(ABS(r2x - r1x1), ABS(r2x - r1x2)) > distance_limitx - 2)
            return (false);
        if (MIN(ABS(r1x - r2x1), ABS(r1x - r2x2)) > distance_limitx - 2)
            return (false);
        if (MIN(ABS(r2y - r1y1), ABS(r2y - r1y2)) > distance_limity - 2)
            return (false);
        if (MIN(ABS(r1y - r2y1), ABS(r1y - r2y2)) > distance_limity - 2)
            return (false);

        if (morgoth_segment_blocked(r1y, r1x, r1y, r2x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r1y, r2x, r2y, r2x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r1y, r1x, r2y, r1x, morgoth_margin))
            return false;
        if (morgoth_segment_blocked(r2y, r1x, r2y, r2x, morgoth_margin))
            return false;

        success = build_tunnel(r1, r2, r1y, r1x, r2y, r2x, tentative);
    }

    if (success)
    {
        dun->connection[r1][r2] = true;
        dun->connection[r2][r1] = true;
    }

    return (success);
}

static bool connect_room_to_corridor(int r)
{
    int length = 10;
    int x;
    int y;
    int delta;
    int ry, rx, r1, r2;
    bool success = false;
    bool done = false;

    ry = dun->cent[r].y;
    rx = dun->cent[r].x;

    y = ry;
    x = rx;

    // go down/right half the time, up/left the other half
    if (one_in_(2))
        delta = 1;
    else
        delta = -1;

    // go horizontal half the time, vertical the other half
    if (one_in_(2))
    {
        while (!done)
        {
            y += delta;

            // abort if the tunnel leaves the map or passes through a door
            if (!in_bounds(y, x) || (ABS(y - ry) > length)
                || cave_any_closed_door_bold(y, x))
            {
                success = false;
                done = true;
            }
            else if (coord_in_morgoth_region(y, x, 1))
            {
                success = false;
                done = true;
            }

            // it has intercepted a tunnel!
            else if ((cave_feat[y][x] == FEAT_FLOOR)
                && !(cave_info[y][x] & (CAVE_ROOM)))
            {
                r1 = cave_corridor1[y][x];
                r2 = cave_corridor2[y][x];

                // make sure that the tunnel intercepts only connects rooms that
                // aren't connected to this room
                if ((r1 < 0) || (r2 < 0)
                    || (!(dun->connection[r][r1]) && !(dun->connection[r][r2])))
                {
                    if (v_tunnel_ok(ry, y - (delta * 2), x, true, 1))
                    {
                        build_v_tunnel(r, r1, ry, y, x, &TUNNEL_PROFILE_NORMAL);

                        // mark the new room connections
                        dun->connection[r][r1] = true;
                        dun->connection[r1][r] = true;
                        dun->connection[r][r2] = true;
                        dun->connection[r2][r] = true;
                        success = true;
                    }
                }

                done = true;
            }
        }
    }

    // do the vertical case (very similar to the horizontal one!)
    else
    {
        while (!done)
        {
            x += delta;

            // abort if the tunnel leaves the map or passes through a door
            if (!in_bounds(y, x) || (ABS(x - rx) > length)
                || cave_any_closed_door_bold(y, x))
            {
                success = false;
                done = true;
            }
            else if (coord_in_morgoth_region(y, x, 1))
            {
                success = false;
                done = true;
            }

            // it has intercepted a tunnel!
            else if ((cave_feat[y][x] == FEAT_FLOOR)
                && !(cave_info[y][x] & (CAVE_ROOM)))
            {
                r1 = cave_corridor1[y][x];
                r2 = cave_corridor2[y][x];

                // make sure that the tunnel intercepts only connects rooms that
                // aren't connected to this room
                if ((r1 < 0) || (r2 < 0)
                    || (!(dun->connection[r][r1]) && !(dun->connection[r][r2])))
                {
                    if (h_tunnel_ok(rx, x - (delta * 2), y, true, 1))
                    {
                        build_h_tunnel(r, r1, rx, x, y, &TUNNEL_PROFILE_NORMAL);

                        // mark the new room connections
                        dun->connection[r][r1] = true;
                        dun->connection[r1][r] = true;
                        dun->connection[r][r2] = true;
                        dun->connection[r2][r] = true;
                        success = true;
                    }
                }

                done = true;
            }
        }
    }

    return (success);
}

static bool connectivity_rescue_traversable(int ry, int rx)
{
    if (!in_bounds_fully(ry, rx))
        return false;

    if (cave_feat[ry][rx] == FEAT_WALL_PERM)
        return false;
    if (cave_feat[ry][rx] == FEAT_CHASM)
        return false;

    bool is_wall = (cave_feat[ry][rx] >= FEAT_WALL_HEAD)
        && (cave_feat[ry][rx] <= FEAT_WALL_TAIL)
        && (cave_feat[ry][rx] != FEAT_SECRET);

    /* Never carve through Morgoth's vault walls: require using the forced doors. */
    if (morgoth_level_active && (cave_info[ry][rx] & CAVE_G_VAULT) && is_wall)
        return false;

    /* Avoid carving new routes inside the sealed Morgoth region: only traverse
     * existing vault/tunnel squares there (and don't cross permanent walls). */
    if (coord_in_morgoth_region(ry, rx, 0)
        && !(cave_info[ry][rx] & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
        && is_wall)
    {
        return false;
    }

    return true;
}

static int connectivity_unreachable_component(
    int start_y, int start_x,
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID])
{
    static const int ddy8[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int ddx8[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int head = 0;
    int tail = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            component[y][x] = 0;
        }
    }

    if (!in_bounds_fully(start_y, start_x))
        return 0;
    if (cave_access[start_y][start_x])
        return 0;
    if (!player_passable(start_y, start_x, true))
        return 0;

    component[start_y][start_x] = 1;
    component_cells[tail++] = start_y * MAX_DUNGEON_WID + start_x;

    while (head < tail)
    {
        int cur = component_cells[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 8; ++d)
        {
            int ny = cy + ddy8[d];
            int nx = cx + ddx8[d];
            int nidx;

            if (!in_bounds_fully(ny, nx))
                continue;
            if (component[ny][nx])
                continue;
            if (cave_access[ny][nx])
                continue;
            if (!player_passable(ny, nx, true))
                continue;

            component[ny][nx] = 1;
            nidx = ny * MAX_DUNGEON_WID + nx;
            if (tail < MAX_DUNGEON_HGT * MAX_DUNGEON_WID)
                component_cells[tail++] = nidx;
        }
    }

    return tail;
}

static bool connectivity_component_boundary_cell(
    int y, int x,
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID])
{
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};

    for (int d = 0; d < 4; ++d)
    {
        int ny = y + ddy4[d];
        int nx = x + ddx4[d];

        if (!in_bounds_fully(ny, nx))
            continue;
        if (component[ny][nx])
            continue;
        if (!connectivity_rescue_traversable(ny, nx))
            continue;

        return true;
    }

    return false;
}

static bool connectivity_rescue_component(
    byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID],
    int component_count,
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID],
    int *out_source_y, int *out_source_x,
    int *out_target_y, int *out_target_x,
    int *out_carve_count, int *out_boundary_sources)
{
    static int prev[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int queue[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    static const int ddy4[4] = {-1, 1, 0, 0};
    static const int ddx4[4] = {0, 0, -1, 1};
    int head = 0;
    int tail = 0;
    int found_y = -1;
    int found_x = -1;
    int source_y = -1;
    int source_x = -1;
    int carve_count = 0;
    int boundary_sources = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            prev[y][x] = -1;
        }
    }

    for (int i = 0; i < component_count; ++i)
    {
        int idx = component_cells[i];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;
        prev[cy][cx] = -2;
    }

    for (int i = 0; i < component_count; ++i)
    {
        int idx = component_cells[i];
        int cy = idx / MAX_DUNGEON_WID;
        int cx = idx % MAX_DUNGEON_WID;

        if (cave_feat[cy][cx] == FEAT_CHASM)
            continue;
        if (!connectivity_component_boundary_cell(cy, cx, component))
            continue;

        prev[cy][cx] = idx;
        if (tail < (int)N_ELEMENTS(queue))
            queue[tail++] = idx;
        boundary_sources++;
    }

    if (out_boundary_sources)
        *out_boundary_sources = boundary_sources;

    if (boundary_sources == 0)
        return false;

    while (head < tail)
    {
        int cur = queue[head++];
        int cy = cur / MAX_DUNGEON_WID;
        int cx = cur % MAX_DUNGEON_WID;

        for (int d = 0; d < 4; ++d)
        {
            int ny = cy + ddy4[d];
            int nx = cx + ddx4[d];
            int nidx;

            if (!in_bounds_fully(ny, nx))
                continue;
            if (prev[ny][nx] != -1)
                continue;
            if (!connectivity_rescue_traversable(ny, nx))
                continue;

            nidx = ny * MAX_DUNGEON_WID + nx;
            prev[ny][nx] = cur;
            if (tail < (int)N_ELEMENTS(queue))
                queue[tail++] = nidx;

            if (cave_access[ny][nx]
                && player_passable(ny, nx, true)
                && !coord_in_morgoth_region(ny, nx, 1))
            {
                found_y = ny;
                found_x = nx;
                head = tail;
                break;
            }
        }
    }

    if (found_y < 0 || found_x < 0)
        return false;

    {
        int cur = found_y * MAX_DUNGEON_WID + found_x;
        int safety = 0;

        while (safety++ < (int)N_ELEMENTS(queue))
        {
            int cy = cur / MAX_DUNGEON_WID;
            int cx = cur % MAX_DUNGEON_WID;

            if (cave_feat[cy][cx] != FEAT_WALL_PERM)
            {
                bool in_morgoth = coord_in_morgoth_region(cy, cx, 0);
                bool allow_morgoth = (cave_info[cy][cx] & CAVE_MORGOTH_TUNNEL) != 0;

                if (!in_morgoth || allow_morgoth)
                {
                    if (!cave_floor_bold(cy, cx)
                        && (cave_feat[cy][cx] < FEAT_DOOR_HEAD
                            || cave_feat[cy][cx] > FEAT_DOOR_TAIL))
                    {
                        cave_set_feat(cy, cx, FEAT_FLOOR);
                        carve_count++;
                    }

                    if (!(cave_info[cy][cx] & CAVE_ROOM))
                        mark_generation_escape_tunnel(cy, cx);
                }
            }

            if (prev[cy][cx] == cur)
            {
                source_y = cy;
                source_x = cx;
                break;
            }

            if (prev[cy][cx] < 0)
                break;
            cur = prev[cy][cx];
        }
    }

    if (out_source_y)
        *out_source_y = source_y;
    if (out_source_x)
        *out_source_x = source_x;
    if (out_target_y)
        *out_target_y = found_y;
    if (out_target_x)
        *out_target_x = found_x;
    if (out_carve_count)
        *out_carve_count = carve_count;

    return (source_y >= 0 && source_x >= 0);
}

/*
 *  Make sure that the level is sufficiently connected.
 */

bool check_connectivity(void)
{
    int cave_access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static byte component[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
    static int component_cells[MAX_DUNGEON_HGT * MAX_DUNGEON_WID];
    int y, x;

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    /* Log which room centers are unreachable before rescue attempts */
    flood_access(p_ptr->py, p_ptr->px, cave_access, true);
    int unreachable_rooms = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        int ry = dun->cent[i].y;
        int rx = dun->cent[i].x;
        if (in_bounds_fully(ry, rx) && !cave_access[ry][rx])
        {
            unreachable_rooms++;
            genlog_connect("UNREACHABLE ROOM #%d at (%d,%d) bounds=(%d,%d)-(%d,%d)",
                           i, ry, rx, 
                           dun->corner[i].y1, dun->corner[i].x1,
                           dun->corner[i].y2, dun->corner[i].x2);
        }
    }
    if (unreachable_rooms > 0)
    {
        genlog_fail("PRE-RESCUE: %d/%d rooms unreachable from player at (%d,%d)",
                    unreachable_rooms, dun->cent_n, p_ptr->py, p_ptr->px);
    }
    
    /* Reset for rescue loop */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    /* Attempt connectivity with iterative rescue tunnels for each disconnected component */
    int rescue_attempts = 0;
    while (true)
    {
        // Make sure entire dungeon is connected (ignoring rubble and chasms)
        flood_access(p_ptr->py, p_ptr->px, cave_access, true);
        int unreachable = 0;
        int sample_y = -1, sample_x = -1;
        for (y = 1; y < p_ptr->cur_map_hgt - 1; y++)
            for (x = 1; x < p_ptr->cur_map_wid - 1; x++)
                if (player_passable(y, x, true) && (cave_access[y][x] == false))
                {
                    unreachable++;
                    if (sample_y < 0)
                    {
                        sample_y = y;
                        sample_x = x;
                    }
                }

        if (unreachable == 0)
            break;

        /* Prefer sampling an unreachable room center to connect large components early. */
        if (dun)
        {
            for (int i = 0; i < dun->cent_n; ++i)
            {
                int ry = dun->cent[i].y;
                int rx = dun->cent[i].x;
                if (!in_bounds_fully(ry, rx)) continue;
                if (cave_access[ry][rx]) continue;
                if (!player_passable(ry, rx, true)) continue;
                sample_y = ry;
                sample_x = rx;
                break;
            }
        }

        /* Stop if we've tried too many rescues - scale with level size */
        /* Larger levels need more rescue attempts: base 20 + (blocks-8)*4 (and at least ~half room count). */
        int blocks = p_ptr->cur_map_hgt / PANEL_HGT;
        int max_rescues = 20 + MAX(0, (blocks - 8) * 4);  /* 20 for 8 blocks, 72 for 21 blocks */
        if (dun) max_rescues = MAX(max_rescues, 20 + (dun->cent_n / 2));
        if (rescue_attempts++ >= max_rescues)
        {
            log_trace("check_connectivity: %d unreachable passable grids after %d rescues (first at %d,%d) -- FAILING",
                      unreachable, rescue_attempts, sample_y, sample_x);
            genlog_fail("CONNECTIVITY FAILED: %d unreachable passable grids after %d rescues (max=%d), first at (%d,%d)",
                        unreachable, rescue_attempts, max_rescues, sample_y, sample_x);
            return false;
        }

        {
            int component_count;
            int source_y = -1, source_x = -1;
            int found_y = -1, found_x = -1;
            int carve_count = 0;
            int boundary_sources = 0;

            component_count = connectivity_unreachable_component(
                sample_y, sample_x, cave_access, component, component_cells);

            if (component_count <= 0)
            {
                log_trace("check_connectivity: failed to flood unreachable component from (%d,%d)", sample_y, sample_x);
                genlog_fail("CONNECTIVITY FAILED: could not flood unreachable component from (%d,%d)",
                    sample_y, sample_x);
                return false;
            }

            if (!connectivity_rescue_component(
                    component, component_cells, component_count, cave_access,
                    &source_y, &source_x, &found_y, &found_x,
                    &carve_count, &boundary_sources))
            {
                log_trace("check_connectivity: BFS rescue could not find a reachable target from component at (%d,%d) size=%d boundary=%d",
                    sample_y, sample_x, component_count, boundary_sources);
                genlog_fail("CONNECTIVITY FAILED: BFS rescue could not find reachable target from (%d,%d)",
                    sample_y, sample_x);
                return false;
            }

            log_trace("check_connectivity: component rescue from (%d,%d) boundary=(%d,%d) to reachable (%d,%d), component=%d boundary=%d carved=%d (unreachable=%d, attempt=%d)",
                sample_y, sample_x, source_y, source_x, found_y, found_x,
                component_count, boundary_sources, carve_count, unreachable,
                rescue_attempts);
            genlog_connect("RESCUE TUNNEL: component=%d boundary=%d from (%d,%d) to (%d,%d), carved=%d",
                component_count, boundary_sources, source_y, source_x, found_y,
                found_x, carve_count);
        }

        /* Clear and loop to re-check connectivity */
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
            for (x = 0; x < p_ptr->cur_map_wid; x++)
                cave_access[y][x] = false;
    }

    // Reset the array used for checking connectivity
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
            cave_access[y][x] = false;

    if (p_ptr->depth >= MORGOTH_DEPTH)
    {
        return (true);
    }

    if (p_ptr->create_stair == FEAT_MORE
        || p_ptr->create_stair == FEAT_MORE_SHAFT)
    {
        return (true);
    }

    // Make sure player can reach down stairs without going through rubble and
    // chasms
    flood_access(p_ptr->py, p_ptr->px, cave_access, false);
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (((cave_feat[y][x] == FEAT_MORE) && (cave_access[y][x] == true))
                || ((cave_feat[y][x] == FEAT_MORE_SHAFT)
                    && (cave_access[y][x] == true)))
            {
                return (true);
            }
        }

    genlog_fail("CONNECTIVITY FAILED: player cannot reach down stairs without rubble/chasms");
    return (false);
}

/*
 *  Check if there are two adjacent doors on the level.
 */
bool doubled_doors(void)
{
    int y, x;

    // Check each grid within boundary
    for (y = 0; y < p_ptr->cur_map_hgt - 1; y++)
        for (x = 0; x < p_ptr->cur_map_wid - 1; x++)
            if (cave_known_closed_door_bold(y, x))
            {
                if (cave_known_closed_door_bold(y + 1, x))
                    return (true);
                if (cave_known_closed_door_bold(y, x + 1))
                    return (true);
            }

    return (false);
}

bool connect_rooms_stairs(void)
{
    int i;
    int corridor_attempts;
    int r1, r2, r_closest, d_closest, d;
    int pieces = 0;
    int stairs = 0;
    int initial_up = FEAT_LESS;
    int initial_down = FEAT_MORE;

    bool joined;
    bool single_stair_mode = adult_single_stair;
    bool no_down_stairs = (p_ptr->depth >= MORGOTH_DEPTH);
    bool niena_level = (quest_lottery_winner == QUEST_ID_NIENA);

    /* Add backbone links across partition neighbors */
    connect_partition_hubs();

    // Phase 1:
    // connect each room to the closest room (if not already connected)
    // Try normal mode first, then desperate mode if that fails

    for (r1 = 0; r1 < dun->cent_n; r1++)
    {
        /* find closest room */
        r_closest = 0; /* default values that will get beaten trivially */
        d_closest = 1000;
        for (r2 = 0; r2 < dun->cent_n; r2++)
        {
            if (r2 != r1)
            {
                d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y,
                    dun->cent[r2].x);
                if (d < d_closest)
                {
                    d_closest = d;
                    r_closest = r2;
                }
            }
        }

        /* connect the rooms, if not already connected */
        if (!(dun->connection[r1][r_closest]))
        {
            /* Try normal mode first, then desperate mode */
            if (!connect_two_rooms(r1, r_closest, true, false))
            {
                (void)connect_two_rooms(r1, r_closest, true, true);
            }
        }
    }
    
    // Phase 1.5: Connect to second-closest room as well for redundancy
    for (r1 = 0; r1 < dun->cent_n; r1++)
    {
        int closest1 = -1, closest2 = -1;
        int dist1 = 99999, dist2 = 99999;
        
        for (r2 = 0; r2 < dun->cent_n; r2++)
        {
            if (r2 == r1) continue;
            d = distance(dun->cent[r1].y, dun->cent[r1].x, dun->cent[r2].y, dun->cent[r2].x);
            if (d < dist1)
            {
                dist2 = dist1; closest2 = closest1;
                dist1 = d; closest1 = r2;
            }
            else if (d < dist2)
            {
                dist2 = d; closest2 = r2;
            }
        }
        
        /* Try to connect to second-closest if not already connected */
        if (closest2 >= 0 && !(dun->connection[r1][closest2]))
        {
            (void)connect_two_rooms(r1, closest2, true, false);
        }
    }

    // Phase 2:
    // make some random connections between rooms so long as they don't
    // intersect things

    switch (p_ptr->cur_map_hgt / PANEL_HGT)
    {
    case 3:
        corridor_attempts = dun->cent_n * dun->cent_n;
        break;
    case 4:
        corridor_attempts = dun->cent_n * dun->cent_n * 2;
        break;
    case 5:
    default:
        corridor_attempts = dun->cent_n * dun->cent_n * 10;
        break;
    }

    for (i = 0; i < corridor_attempts; i++)
    {
        r1 = rand_int(dun->cent_n);
        r2 = rand_int(dun->cent_n);
        if ((r1 != r2) && !(dun->connection[r1][r2]))
        {
            (void)connect_two_rooms(r1, r2, true, false);
        }
    }

    // add some T-intersections in the corridors
    for (i = 0; i < corridor_attempts; i++)
    {
        r1 = rand_int(dun->cent_n);
        (void)connect_room_to_corridor(r1);
    }

    // Phase 3:
    // cut the dungeon up into connected pieces and try hard to make corridors
    // that connect them

    pieces = dungeon_pieces();
    while (pieces > 1)
    {
        joined = false;

        for (r1 = 0; r1 < dun->cent_n; r1++)
        {
            for (r2 = 0; r2 < dun->cent_n; r2++)
            {
                if (!joined && (dun->piece[r1] != dun->piece[r2]))
                {
                    for (i = 0; i < 10; i++)
                    {
                        if (!(dun->connection[r1][r2]))
                        {
                            joined = connect_two_rooms(r1, r2, true, true);
                        }
                    }
                }
            }
        }

        if (!joined)
            break;

        // cut the dungeon up into connected pieces and stop if there is only
        // one
        pieces = dungeon_pieces();
    }

    /* Phase 3.5: L-shaped corridor fallback before force-connect.
     * Try carving clean L-shaped corridors between disconnected pieces.
     * This produces better-looking results than diagonal Bresenham carving. */
    if (pieces > 1)
    {
        int l_connects = 0;
        for (int attempt = 0; attempt < 100 && pieces > 1; ++attempt)
        {
            /* Find the nearest pair of rooms from different pieces */
            int best_a = -1, best_b = -1;
            int best_dist = 999999;
            
            for (int ra = 0; ra < dun->cent_n; ++ra)
            {
                for (int rb = ra + 1; rb < dun->cent_n; ++rb)
                {
                    if (dun->piece[ra] == dun->piece[rb])
                        continue;
                    if (dun->connection[ra][rb])
                        continue;
                    
                    int dist = distance(dun->cent[ra].y, dun->cent[ra].x,
                                        dun->cent[rb].y, dun->cent[rb].x);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_a = ra;
                        best_b = rb;
                    }
                }
            }
            
            if (best_a < 0 || best_b < 0)
                break;
            
            int y0 = dun->cent[best_a].y, x0 = dun->cent[best_a].x;
            int y1 = dun->cent[best_b].y, x1 = dun->cent[best_b].x;
            
            /* Try L-shaped corridor (horizontal then vertical, or vice versa) */
            bool carved = false;
            for (int dir = 0; dir < 2 && !carved; ++dir)
            {
                bool valid = true;
                
                /* Check if the L-path is carveable (no permanent walls) */
                int min_x = MIN(x0, x1), max_x = MAX(x0, x1);
                int min_y = MIN(y0, y1), max_y = MAX(y0, y1);
                
                /* Check horizontal leg */
                int leg_y = (dir == 0) ? y0 : y1;
                for (int tx = min_x; tx <= max_x && valid; ++tx)
                {
                    if (!in_bounds_fully(leg_y, tx) || cave_feat[leg_y][tx] == FEAT_WALL_PERM)
                        valid = false;
                    if (coord_in_morgoth_region(leg_y, tx, 1))
                        valid = false;
                }
                
                /* Check vertical leg */
                int leg_x = (dir == 0) ? x1 : x0;
                for (int ty = min_y; ty <= max_y && valid; ++ty)
                {
                    if (!in_bounds_fully(ty, leg_x) || cave_feat[ty][leg_x] == FEAT_WALL_PERM)
                        valid = false;
                    if (coord_in_morgoth_region(ty, leg_x, 1))
                        valid = false;
                }
                
                if (valid)
                {
                    /* Carve horizontal leg */
                    for (int tx = min_x; tx <= max_x; ++tx)
                    {
                        if (coord_in_morgoth_region(leg_y, tx, 1))
                        {
                            valid = false;
                            break;
                        }
                        if (!cave_floor_bold(leg_y, tx))
                            cave_set_feat(leg_y, tx, FEAT_FLOOR);
                    }
                    if (!valid) continue;
                    /* Carve vertical leg */
                    for (int ty = min_y; ty <= max_y; ++ty)
                    {
                        if (coord_in_morgoth_region(ty, leg_x, 1))
                        {
                            valid = false;
                            break;
                        }
                        if (!cave_floor_bold(ty, leg_x))
                            cave_set_feat(ty, leg_x, FEAT_FLOOR);
                    }
                    if (!valid) continue;
                    
                    dun->connection[best_a][best_b] = true;
                    dun->connection[best_b][best_a] = true;
                    carved = true;
                    l_connects++;
                }
            }
            
            pieces = dungeon_pieces();
        }
        
        if (l_connects > 0)
            log_trace("connect_rooms_stairs: L-shaped fallback carved %d connections, pieces now %d", l_connects, pieces);
    }

    /* Last resort: forcibly connect distinct pieces by digging a straight corridor
     * ignoring tunnel safety checks (but respecting permanent walls). This handles
     * adjacent-but-unconnected rooms/vaults seen on dense maps.
     * IMPROVED: Instead of picking random pairs, find the NEAREST pair of rooms
     * from different pieces to minimize ugly cross-map tunnels. */
    if (pieces > 1)
    {
        for (int attempt = 0; attempt < 50 && pieces > 1; ++attempt)
        {
            /* Find the nearest pair of rooms from different pieces */
            int best_a = -1, best_b = -1;
            int best_dist = 999999;
            
            for (int ra = 0; ra < dun->cent_n; ++ra)
            {
                for (int rb = ra + 1; rb < dun->cent_n; ++rb)
                {
                    if (dun->piece[ra] == dun->piece[rb])
                        continue;
                    
                    int dist = distance(dun->cent[ra].y, dun->cent[ra].x,
                                        dun->cent[rb].y, dun->cent[rb].x);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_a = ra;
                        best_b = rb;
                    }
                }
            }
            
            if (best_a < 0 || best_b < 0)
                break;  /* No valid pair found */
            
            int a = best_a;
            int b = best_b;

            int y0 = dun->cent[a].y, x0 = dun->cent[a].x;
            int y1 = dun->cent[b].y, x1 = dun->cent[b].x;

            log_trace("force-connect: linking room %d (piece %d) to room %d (piece %d), dist=%d",
                      a, dun->piece[a], b, dun->piece[b], best_dist);

            /* Bresenham carve that ignores h/v tunnel constraints */
            int dy = ABS(y1 - y0), sx = (x0 < x1) ? 1 : -1;
            int dx = ABS(x1 - x0), sy = (y0 < y1) ? 1 : -1;
            int err = (dx > dy ? dx : -dy) / 2;
            int y = y0, x = x0;
            bool aborted = false;
            while (true)
            {
                if (coord_in_morgoth_region(y, x, 1))
                {
                    aborted = true;
                    break;
                }
                if (in_bounds_fully(y, x) && cave_feat[y][x] != FEAT_WALL_PERM)
                {
                    if (!cave_floor_bold(y, x))
                        cave_set_feat(y, x, FEAT_FLOOR);
                }
                if (y == y1 && x == x1) break;
                int e2 = err;
                if (e2 > -dx) { err -= dy; x += sx; }
                if (e2 < dy)  { err += dx; y += sy; }
            }

            if (!aborted)
            {
                dun->connection[a][b] = dun->connection[b][a] = true;
                pieces = dungeon_pieces();
            }
        }

        log_trace("connect_rooms_stairs: forced-connect phase reduced pieces to %d", pieces);
    }

    // label_rooms();

    if (single_stair_mode)
    {
        int down_feat = p_ptr->on_the_run ? FEAT_MORE_SHAFT : FEAT_MORE;
        int up_feat = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT : FEAT_LESS;
        bool down_ok = no_down_stairs;
        bool up_ok = false;

        for (int attempt = 0; attempt < 500 && !down_ok; ++attempt)
        {
            int yy = rand_range(1, p_ptr->cur_map_hgt - 2);
            int xx = rand_range(1, p_ptr->cur_map_wid - 2);

            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
            {
                cave_set_feat(yy, xx, down_feat);
                down_ok = true;
            }
        }

        for (int attempt = 0; attempt < 500 && !up_ok; ++attempt)
        {
            int yy = rand_range(1, p_ptr->cur_map_hgt - 2);
            int xx = rand_range(1, p_ptr->cur_map_wid - 2);

            if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
            {
                cave_set_feat(yy, xx, up_feat);
                up_ok = true;
            }
        }

        if (!down_ok || !up_ok)
        {
            log_trace("connect_rooms_stairs failed: single stair placement failed (down_ok=%d, up_ok=%d)", down_ok, up_ok);
            return (false);
        }

        if (no_down_stairs)
        {
            log_trace("connect_rooms_stairs: single stair mode placed one up stair on the final level");
        }
        else
        {
            log_trace("connect_rooms_stairs: single stair mode placed one up and one down stair");
        }
    }
    else
    {
        /* Calculate number of stairs based on map size: 2 for 66x66, 8 for 165x165 */
        /* Linear interpolation: stairs = 2 + (size - 66) * (8 - 2) / (165 - 66) */
        int map_size = (p_ptr->cur_map_hgt + p_ptr->cur_map_wid) / 2;  /* Average dimension */
        int stairs_max_base = 8;
        int stairs_max_total = 12;
        if (more_stairs)
        {
            stairs_max_base *= 2;
            stairs_max_total *= 2;
        }
        stairs = 2 + ((map_size - 66) * 6) / 99;  /* 6 = (8-2), 99 = (165-66) */
        if (stairs < 2) stairs = 2;   /* Minimum 2 */
        if (stairs > stairs_max_base) stairs = stairs_max_base;  /* Maximum 8 (or doubled) */
        
        /* Labyrinth bonus: +1 stair per labyrinth partition (more escape routes in mazes) */
        if (current_labyrinth_partitions > 0)
        {
            int stair_bonus = current_labyrinth_partitions;
            stairs += stair_bonus;
            log_trace("Labyrinth stair bonus: +%d stairs from %d labyrinth partitions (total=%d)",
                      stair_bonus, current_labyrinth_partitions, stairs);
        }

        if (more_stairs)
        {
            stairs += (stairs + 1) / 2; /* +50% (rounded up) */
        }
        if (stairs > stairs_max_total) stairs = stairs_max_total;
        
        log_trace("Map size %d leads to %d stairs each direction", map_size, stairs);
        if (niena_level && !no_down_stairs)
        {
            log_trace("Niena level: limiting down stairs to a single target stair for the mercy quest");
        }

        /* Determine partition count for guaranteed stair placement */
        int partition_count = (map_size <= 80) ? 2 : 3;  /* Reduced from 4/9 to match lower stair count */
        int grid_rows = 1;
        int grid_cols = partition_count;
        if (partition_count == 4)
        {
            grid_rows = 2;
            grid_cols = 2;
        }
        else if (partition_count == 9)
        {
            grid_rows = 3;
            grid_cols = 3;
        }

        /* Place guaranteed stairs: at least one up and one down per partition */
        int down_placed = 0;
        int up_placed = 0;
        
        /* First pass: place one of each type per partition */
        for (int pi = 0; pi < partition_count; ++pi)
        {
            int row = pi / grid_cols;
            int col = pi % grid_cols;
            
            int y1 = 1 + (row * p_ptr->cur_map_hgt / grid_rows);
            int y2 = ((row + 1) * p_ptr->cur_map_hgt / grid_rows) - 1;
            int x1 = 1 + (col * p_ptr->cur_map_wid / grid_cols);
            int x2 = ((col + 1) * p_ptr->cur_map_wid / grid_cols) - 1;
            
            /* Clamp boundaries */
            if (y2 >= p_ptr->cur_map_hgt - 1) y2 = p_ptr->cur_map_hgt - 2;
            if (x2 >= p_ptr->cur_map_wid - 1) x2 = p_ptr->cur_map_wid - 2;
            
            /* Place one down stair in this partition (unless final level) */
            if (!no_down_stairs && !niena_level)
            {
                for (int attempt = 0; attempt < 100; ++attempt)
                {
                    int yy = rand_range(y1, y2);
                    int xx = rand_range(x1, x2);
                    
                    if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                        cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                        cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                        cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                        cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
                    {
                        int feat = (p_ptr->on_the_run) ? FEAT_MORE_SHAFT : 
                                  (down_placed == 0 || p_ptr->depth >= MORGOTH_DEPTH) ? FEAT_MORE : 
                                  choose_down_stairs();
                        cave_set_feat(yy, xx, feat);
                        down_placed++;
                        break;
                    }
                }
            }
            
            /* Place one up stair in this partition */
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                int yy = rand_range(y1, y2);
                int xx = rand_range(x1, x2);
                
                if (cave_naked_bold(yy, xx) && cave_floor_bold(yy, xx) &&
                    cave_feat[yy - 1][xx] != FEAT_DOOR_HEAD &&
                    cave_feat[yy][xx - 1] != FEAT_DOOR_HEAD &&
                    cave_feat[yy + 1][xx] != FEAT_DOOR_HEAD &&
                    cave_feat[yy][xx + 1] != FEAT_DOOR_HEAD)
                {
                    int feat = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT :
                              (up_placed == 0 || !p_ptr->depth) ? FEAT_LESS :
                              choose_up_stairs();
                    cave_set_feat(yy, xx, feat);
                    up_placed++;
                    break;
                }
            }
        }
        
        log_trace("Guaranteed partition stairs: %d down, %d up placed", down_placed, up_placed);

        /* Second pass: place remaining stairs randomly across the map */
        int down_remaining = 0;
        if (!no_down_stairs)
        {
            down_remaining = niena_level ? 1 : (stairs - down_placed);
        }
        int up_remaining = stairs - up_placed;
        
        /* Place remaining down stairs */
        int down_stairs = down_remaining;
        if (p_ptr->on_the_run)
            down_stairs *= 2;
        if ((p_ptr->create_stair == FEAT_MORE) || (p_ptr->create_stair == FEAT_MORE_SHAFT))
            down_stairs--;
        
        initial_down = p_ptr->on_the_run ? FEAT_MORE_SHAFT : FEAT_MORE;
        
        if (no_down_stairs)
            down_stairs = 0;

        if (down_stairs > 0 && !(alloc_stairs(initial_down, down_stairs)))
        {
            if (cheat_room)
                msg_format("Failed to place remaining down stairs.");
            log_trace("connect_rooms_stairs failed: Could not place %d remaining down stairs", down_stairs);
            return (false);
        }

        /* Place remaining up stairs */
        int up_stairs = up_remaining;
        if (p_ptr->on_the_run && p_ptr->depth >= 2)
            up_stairs *= 2;
        if ((p_ptr->create_stair == FEAT_LESS) || (p_ptr->create_stair == FEAT_LESS_SHAFT))
            up_stairs--;
        
        initial_up = (p_ptr->on_the_run && p_ptr->depth >= 2) ? FEAT_LESS_SHAFT : FEAT_LESS;
        
        if (up_stairs > 0 && !(alloc_stairs(initial_up, up_stairs)))
        {
            if (cheat_room)
                msg_format("Failed to place remaining up stairs.");
            log_trace("connect_rooms_stairs failed: Could not place %d remaining up stairs", up_stairs);
            return (false);
        }
        
        log_trace("Total stairs placed: %d down, %d up", down_placed + down_stairs, up_placed + up_stairs);
    }

    /* Hack -- Add some quartz streamers */
    for (i = 0; i < DUN_STR_QUA; i++)
    {
        /*if we can't build streamers, something is wrong with level*/
        if (!build_streamer(FEAT_QUARTZ))
        {
            log_trace("connect_rooms_stairs failed: Could not build quartz streamer %d", i);
            return (false);
        }
    }

    /* Do not mix the legacy random-chasm pass with partition chasm rooms. The
     * two systems use different styling/connectivity rules and produce broken
     * visuals/access when overlaid. */
    if (!level_has_chasm_partition())
    {
        build_chasms();
    }
    else
    {
        log_trace("connect_rooms_stairs: skipping legacy build_chasms() because the partition generator already placed chasm terrain");
    }

    return (true);
}
