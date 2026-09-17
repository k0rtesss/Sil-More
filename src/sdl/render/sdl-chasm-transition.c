#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-bridge.h"

/* A chasm recedes into its own cell. Its neighbors keep every original pixel:
 * no raised lip, outline, shadow or painted border is added to their art. */
typedef struct chasm_donor {
    byte state; /* 0 unknown, 1 connected void (including bridge), 2 terrain */
    byte a;
    char c;
    int y, x;
} chasm_donor;

static const int chasm_dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
static const int chasm_dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };

static bool chasm_known(int y, int x)
{
    if (!p_ptr || !in_bounds(y, x)
        || (unsigned)y >= (unsigned)p_ptr->cur_map_hgt
        || (unsigned)x >= (unsigned)p_ptr->cur_map_wid) return false;
    u16b info = cave_info[y][x];
    if (cave_floorlike_bold(y, x))
    {
        if (!(info & CAVE_SEEN)
            && (!(info & CAVE_MARK) || cave_light[y][x] <= 0)) return false;
    }
    else if (!(info & CAVE_MARK)) return false;
    return p_ptr->is_dead || (!(p_ptr->rage || g_labyrinth_view_active)
        || (info & CAVE_SEEN));
}

/* Vary broad runs of the boundary, anchored to the world rather than the
 * viewport. The clear middle eight pixels keep even a one-cell crack open. */
static int chasm_width(int y, int x, int along, bool horizontal)
{
    unsigned position = (unsigned)(horizontal ? x : y) * TILE_SIZE + along;
    u32b hash = (position / 3) * 0x9e3779b9u
        ^ (u32b)(horizontal ? y : x) * 0x85ebca6bu;
    hash ^= hash >> 16;
    return 2 + hash % 3;
}

/* Each quarter resolves convex coast corners, concave diagonal notches and
 * straight contacts. Unknown cells never contribute artwork or corner cuts. */
static int chasm_owner(int y, int x, int px, int py, const chasm_donor n[8])
{
    int h = px < TILE_SIZE / 2 ? 6 : 2;
    int v = py < TILE_SIZE / 2 ? 0 : 4;
    int d = h == 6 ? (v == 0 ? 7 : 5) : (v == 0 ? 1 : 3);
    int u = px < TILE_SIZE / 2 ? px : TILE_SIZE - 1 - px;
    int w = py < TILE_SIZE / 2 ? py : TILE_SIZE - 1 - py;
    bool horizontal = n[h].state == 2, vertical = n[v].state == 2;
    int owner = -1;
    if (horizontal && u < chasm_width(y, x, py, false)) owner = h;
    if (vertical && w < chasm_width(y, x, px, true)
        && (owner < 0 || w <= u)) owner = v;
    if (horizontal && vertical && u + w < 7)
        owner = u < w ? h : v;
    if (!horizontal && !vertical && n[h].state == 1 && n[v].state == 1
        && n[d].state == 2 && u + w < 4) owner = d;
    return owner;
}

void sdl_chasm_transition_draw(int y, int x, const SDL_FRect* dst)
{
    if (!dst || !g_state.use_tiles || !g_state.renderer || !g_state.tileset
        || !p_ptr || p_ptr->image || !chasm_known(y, x)
        || cave_bridge_underlay(cave_feat[y][x]) != FEAT_CHASM) return;
    chasm_donor n[8] = {0};
    bool any = false;
    for (int i = 0; i < 8; i++)
    {
        int ny = y + chasm_dy[i], nx = x + chasm_dx[i];
        if (!chasm_known(ny, nx)) continue;
        n[i].y = ny; n[i].x = nx;
        byte feat = cave_bridge_underlay(cave_feat[ny][nx]);
        if (feat == FEAT_CHASM) { n[i].state = 1; continue; }
        /* Do not repeat doors into the void. Other foreground features and
         * actors already use their floor/wall underlay in map_info_terrain. */
        if (feat == FEAT_OPEN || feat == FEAT_BROKEN
            || (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
            || feat == FEAT_WARDED || feat == FEAT_WARDED2 || feat == FEAT_WARDED3)
            map_info_floor_terrain(ny, nx, &n[i].a, &n[i].c);
        else map_info_terrain(ny, nx, &n[i].a, &n[i].c);
        if (!(n[i].a & TILE_FLAG) || !((byte)n[i].c & TILE_FLAG)) continue;
        n[i].state = 2;
        any = true;
    }
    if (!any) return;

    byte coverage[TILE_SIZE][TILE_SIZE];
    for (int py = 0; py < TILE_SIZE; py++)
        for (int px = 0; px < TILE_SIZE; px++)
            coverage[py][px] = 1 + chasm_owner(y, x, px, py, n);

    SDL_BlendMode blend, draw_blend;
    Uint8 r, g, b, a, tr, tg, tb, ta;
    SDL_GetTextureBlendMode(g_state.tileset, &blend);
    SDL_GetTextureColorMod(g_state.tileset, &tr, &tg, &tb);
    SDL_GetTextureAlphaMod(g_state.tileset, &ta);
    SDL_GetRenderDrawColor(g_state.renderer, &r, &g, &b, &a);
    SDL_GetRenderDrawBlendMode(g_state.renderer, &draw_blend);
    SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(g_state.tileset, 255, 255, 255);
    SDL_SetTextureAlphaMod(g_state.tileset, 255);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
    /* Merge the ownership field into sprite rectangles. No per-frame memory,
     * image processing, new textures or individual triangle primitives. */
    for (int py = 0; py < TILE_SIZE; py++)
        for (int px = 0; px < TILE_SIZE; px++)
        {
            byte label = coverage[py][px];
            if (!label) continue;
            int right = px + 1, bottom = py + 1;
            while (right < TILE_SIZE && coverage[py][right] == label) right++;
            while (bottom < TILE_SIZE)
            {
                int end = px;
                while (end < right && coverage[bottom][end] == label) end++;
                if (end != right) break;
                bottom++;
            }
            for (int yy = py; yy < bottom; yy++)
                for (int xx = px; xx < right; xx++) coverage[yy][xx] = 0;
            SDL_FRect rect = {dst->x + dst->w * px / TILE_SIZE,
                dst->y + dst->h * py / TILE_SIZE,
                dst->w * (right - px) / TILE_SIZE,
                dst->h * (bottom - py) / TILE_SIZE};
            SDL_FRect pixels = {px, py, right - px, bottom - py};
            const chasm_donor* donor = &n[label - 1];
            if (sdl_idle_animation_draw_liquid_piece(donor->y, donor->x, &pixels, &rect)) continue;
            SDL_FRect src = {((byte)donor->c & TILE_INDEX_MASK) * TILE_SIZE + px,
                (donor->a & TILE_INDEX_MASK) * TILE_SIZE + py, right - px, bottom - py};
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(g_state.renderer, &rect);
            SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &rect);
        }
    SDL_SetTextureBlendMode(g_state.tileset, blend);
    SDL_SetTextureColorMod(g_state.tileset, tr, tg, tb);
    SDL_SetTextureAlphaMod(g_state.tileset, ta);
    SDL_SetRenderDrawBlendMode(g_state.renderer, draw_blend);
    SDL_SetRenderDrawColor(g_state.renderer, r, g, b, a);
}
