#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "cave/cave.h"
#include "cave/cave-fixtures.h"

/* Session-only inspection overlay; never stored in saves or gameplay options. */
static bool illusion_debug_enabled;

bool cave_illusion_debug_enabled(void)
{
    return illusion_debug_enabled;
}

bool cave_illusion_debug_marked(int y, int x)
{
    return illusion_debug_enabled && p_ptr && cave_feat && in_bounds(y, x)
        && cave_feat[y][x] == FEAT_ILLUSORY_WALL;
}

void cave_illusion_debug_set(bool enabled)
{
    if (illusion_debug_enabled == enabled) return;
    illusion_debug_enabled = enabled;
#ifdef USE_SDL
    sdl_minimap_map_texture_cache_clear();
#endif
    if (!character_dungeon) return;
    p_ptr->redraw |= PR_MAP;
    p_ptr->window |= PW_OVERHEAD;
    /* The wall glyph is unchanged in tile mode: invalidate its cached pixels
     * and the side map on both enable and disable, even outside sight. */
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
            if (cave_feat[y][x] == FEAT_ILLUSORY_WALL)
                lite_spot(y, x);
}

/* Light only reveals the disguise while the player can actually see it. */
int cave_illusion_opacity(int y, int x)
{
    if (!in_bounds(y, x) || cave_feat[y][x] != FEAT_ILLUSORY_WALL
        || p_ptr->blind || !(cave_info[y][x] & CAVE_SEEN))
        return 255;
    return 255 - 30 * MIN(5, MAX(0, cave_light[y][x]));
}

void cave_dissolve_illusion(int y, int x)
{
    if (cave_feat[y][x] != FEAT_ILLUSORY_WALL) return;
    bool player_entered = p_ptr->py == y && p_ptr->px == x;
    if (player_entered)
    {
        msg_print(p_ptr->blind ? "The wall gives way without resistance."
                              : "The illusory wall dissolves as you pass through it.");
        disturb(0, 0);
    }
    else if (cave_info[y][x] & CAVE_SEEN)
        msg_print("An illusory wall dissolves.");
    cave_set_feat(y, x, FEAT_FLOOR);
#ifdef USE_SDL
    if (illusion_debug_enabled) sdl_minimap_map_texture_cache_clear();
#endif
    p_ptr->redraw |= PR_MAP;
    p_ptr->window |= PW_OVERHEAD;
}

static bool illusion_stone(int feat)
{
    return feat == FEAT_WALL_EXTRA || feat == FEAT_WALL_INNER
        || feat == FEAT_WALL_OUTER;
}

/* Only pierce a single thickness of ordinary stone, within one partition.
 * Protected rooms, their perimeter, fixtures, and permanent walls are intact. */
static bool illusion_candidate(int y, int x)
{
    if (!illusion_stone(cave_feat[y][x])) return false;
    int partition = level_partition_index_for_point(y, x);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int yy = y + dy, xx = x + dx;
            if ((cave_info[yy][xx] & (CAVE_ICKY | CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL))
                || cave_fixture_at(yy, xx) || cave_m_idx[yy][xx]
                || cave_o_idx[yy][xx]
                || cave_feat[yy][xx] == FEAT_ILLUSORY_WALL
                || level_partition_index_for_point(yy, xx) != partition)
                return false;
        }
    return (cave_feat[y - 1][x] == FEAT_FLOOR
               && cave_feat[y + 1][x] == FEAT_FLOOR
               && illusion_stone(cave_feat[y][x - 1])
               && illusion_stone(cave_feat[y][x + 1]))
        || (cave_feat[y][x - 1] == FEAT_FLOOR
               && cave_feat[y][x + 1] == FEAT_FLOOR
               && illusion_stone(cave_feat[y - 1][x])
               && illusion_stone(cave_feat[y + 1][x]));
}

void place_illusory_passages(void)
{
    /* Keep the surface, throne-room, and Utumno layouts authored. */
    if (!illusory_walls || p_ptr->depth <= 0 || p_ptr->depth >= MORGOTH_DEPTH)
        return;
    int placed = 0;
    int target = 1 + p_ptr->depth / 7;
    for (int n = 0; n < target; n++)
    {
        int candidates = 0, chosen_y = 0, chosen_x = 0;
        for (int y = 2; y < p_ptr->cur_map_hgt - 2; y++)
            for (int x = 2; x < p_ptr->cur_map_wid - 2; x++)
                if (illusion_candidate(y, x) && rand_int(++candidates) == 0)
                {
                    chosen_y = y;
                    chosen_x = x;
                }
        if (!candidates) break;
        cave_set_feat(chosen_y, chosen_x, FEAT_ILLUSORY_WALL);
        placed++;
    }
    log_debug("Placed %d illusory passages at depth %d", placed, p_ptr->depth);
}
