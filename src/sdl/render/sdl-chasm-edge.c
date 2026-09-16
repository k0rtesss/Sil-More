#include "angband.h"
#include "sdl/main-sdl-private.h"

/* Chasm edges are a renderer overlay rather than another terrain material.
 * This keeps a floor, wall, bridge, or hand-drawn style visible underneath the
 * cut while making every known chasm boundary read consistently.  The atlas
 * below is generated once per renderer lifetime: one native 16x16 RGBA cell
 * for each of the 256 clockwise eight-neighbour masks. */

enum {
    CHASM_EDGE_ATLAS_SIDE = 16,
    CHASM_EDGE_SHADOW_WIDTH = 3,
    CHASM_EDGE_TEXTURE_ALPHA = 230,
};

static const int chasm_edge_dy[8] = {
    -1, -1, 0, 1, 1, 1, 0, -1
};
static const int chasm_edge_dx[8] = {
    0, 1, 1, 1, 0, -1, -1, -1
};

static SDL_Texture* chasm_edge_texture;
static bool chasm_edge_load_attempted;

static bool chasm_edge_visible_cell(int y, int x)
{
    u16b info;

    if (!p_ptr || !in_bounds(y, x))
        return false;

    info = cave_info[y][x];
    /* Keep the edge test in step with map_info().  Boring floors (including
     * hidden traps) are rendered from CAVE_SEEN, or from a marked cell that
     * remains illuminated.  Interesting terrain such as walls, liquids and
     * chasms is rendered from its memorized CAVE_MARK state. */
    if (cave_floorlike_bold(y, x))
    {
        if (!(info & CAVE_SEEN)
            && (!(info & CAVE_MARK) || cave_light[y][x] <= 0))
            return false;
    }
    else if (!(info & CAVE_MARK))
        return false;

    /* Remembered cells are deliberately hidden by rage and labyrinth views,
     * exactly as in map_info(). */
    if (!p_ptr->is_dead && (p_ptr->rage || g_labyrinth_view_active)
        && !(info & CAVE_SEEN))
        return false;

    return true;
}

static bool chasm_edge_is_visible_chasm(int y, int x)
{
    return chasm_edge_visible_cell(y, x) && cave_feat[y][x] == FEAT_CHASM;
}

/* Return the raw clockwise adjacency mask.  A set bit means that the
 * corresponding direct neighbour is a known, visible FEAT_CHASM.  In
 * particular, a bridge whose underlay is chasm remains a solid neighbour;
 * only the actual chasm feature opens a boundary. */
byte sdl_chasm_edge_mask(int y, int x)
{
    byte mask = 0;

    if (!chasm_edge_visible_cell(y, x) || cave_feat[y][x] == FEAT_CHASM)
        return 0;

    for (int i = 0; i < 8; i++)
    {
        if (chasm_edge_is_visible_chasm(y + chasm_edge_dy[i],
                x + chasm_edge_dx[i]))
            mask |= (byte)(1u << i);
    }

    return mask;
}

static Uint32* chasm_edge_surface_pixels(SDL_Surface* surface)
{
    return (Uint32*)surface->pixels;
}

static void chasm_edge_put_pixel(SDL_Surface* surface, int x, int y,
    Uint32 pixel)
{
    Uint32* pixels = chasm_edge_surface_pixels(surface);
    pixels[y * (surface->pitch / (int)sizeof(Uint32)) + x] = pixel;
}

static int chasm_edge_jagged_width(int direction, int tangent)
{
    /* A small stable offset breaks up the perfectly straight 16px line while
     * remaining identical when an unrelated neighbour is discovered.  The
     * end pixels are fixed so a diagonal corner meets both cardinal edges. */
    Uint32 hash;

    if (tangent == 0 || tangent == TILE_SIZE - 1)
        return CHASM_EDGE_SHADOW_WIDTH;

    hash = (Uint32)tangent * 0x85EBCA6BU
        ^ (Uint32)direction * 0x27D4EB2FU;
    hash ^= hash >> 16;
    return CHASM_EDGE_SHADOW_WIDTH + ((hash & 7U) == 0U ? 1 : 0);
}

static int chasm_edge_cardinal_distance(int direction, int x, int y)
{
    switch (direction)
    {
    case 0: return y;       /* N */
    case 2: return 15 - x;  /* E */
    case 4: return 15 - y;  /* S */
    case 6: return x;       /* W */
    default: return 99;
    }
}

static int chasm_edge_diagonal_distance(int direction, int x, int y)
{
    int horizontal = (direction == 1 || direction == 2 || direction == 3)
        ? 15 - x : x;
    int vertical = (direction == 1 || direction == 7) ? y : 15 - y;

    /* Diagonals only need to bridge the corner.  Returning the sum keeps an
     * isolated diagonal from painting a full side of the tile. */
    return horizontal + vertical;
}

static Uint32 chasm_edge_pixel_for_depth(SDL_Surface* surface, int depth,
    bool highlight)
{
    if (depth <= 1)
        return SDL_MapSurfaceRGBA(surface, 10, 14, 13, 185);
    if (highlight)
        return SDL_MapSurfaceRGBA(surface, 196, 201, 177, 125);
    return SDL_MapSurfaceRGBA(surface, 61, 68, 59, 125);
}

static bool chasm_edge_build_atlas(void)
{
    SDL_Surface* atlas;
    Uint32 clear;

    atlas = SDL_CreateSurface(CHASM_EDGE_ATLAS_SIDE * TILE_SIZE,
        CHASM_EDGE_ATLAS_SIDE * TILE_SIZE, SDL_PIXELFORMAT_RGBA32);
    if (!atlas)
    {
        log_warn("Chasm edge overlay unavailable: %s", SDL_GetError());
        return false;
    }

    clear = SDL_MapSurfaceRGBA(atlas, 0, 0, 0, 0);
    SDL_FillSurfaceRect(atlas, NULL, clear);

    for (int mask = 0; mask < 256; mask++)
    {
        int cell_x = (mask & 15) * TILE_SIZE;
        int cell_y = (mask >> 4) * TILE_SIZE;

        for (int y = 0; y < TILE_SIZE; y++)
        {
            for (int x = 0; x < TILE_SIZE; x++)
            {
                int best_distance = 99;
                bool best_highlight = false;

                for (int direction = 0; direction < 8; direction++)
                {
                    if (!(mask & (1 << direction)))
                        continue;

                    if ((direction & 1) == 0)
                    {
                        int distance = chasm_edge_cardinal_distance(direction,
                            x, y);
                        int tangent = (direction == 0 || direction == 4)
                            ? x : y;
                        int width = chasm_edge_jagged_width(direction,
                            tangent);
                        if (distance < width && distance < best_distance)
                        {
                            best_distance = distance;
                            best_highlight = distance == width - 1;
                        }
                    }
                    else
                    {
                        int distance = chasm_edge_diagonal_distance(direction,
                            x, y);
                        if (distance < CHASM_EDGE_SHADOW_WIDTH
                            && distance < best_distance)
                        {
                            best_distance = distance;
                            best_highlight = distance == CHASM_EDGE_SHADOW_WIDTH - 1;
                        }
                    }
                }

                if (best_distance < 99)
                {
                    Uint32 pixel = chasm_edge_pixel_for_depth(atlas,
                        best_distance, best_highlight);
                    chasm_edge_put_pixel(atlas, cell_x + x, cell_y + y,
                        pixel);
                }
            }
        }
    }

    chasm_edge_texture = SDL_CreateTextureFromSurface(g_state.renderer,
        atlas);
    SDL_DestroySurface(atlas);
    if (!chasm_edge_texture)
    {
        log_warn("Chasm edge overlay texture unavailable: %s", SDL_GetError());
        return false;
    }

    SDL_SetTextureScaleMode(chasm_edge_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(chasm_edge_texture, SDL_BLENDMODE_BLEND);
    return true;
}

static bool chasm_edge_ensure_texture(void)
{
    if (chasm_edge_texture)
        return true;
    if (chasm_edge_load_attempted)
        return false;

    chasm_edge_load_attempted = true;
    return chasm_edge_build_atlas();
}

bool sdl_chasm_edge_draw(int y, int x, const SDL_FRect* dst)
{
    byte mask;
    SDL_FRect src;
    int light;

    if (!dst || !g_state.renderer || !g_state.use_tiles
        || !g_state.tileset || !chasm_edge_visible_cell(y, x)
        || cave_feat[y][x] == FEAT_CHASM)
        return false;

    mask = sdl_chasm_edge_mask(y, x);
    if (!mask || !chasm_edge_ensure_texture())
        return false;

    src = (SDL_FRect){
        (float)(mask & 15) * TILE_SIZE,
        (float)(mask >> 4) * TILE_SIZE,
        TILE_SIZE,
        TILE_SIZE,
    };

    /* Dim remembered boundaries consistently with animated terrain while
     * leaving their alpha geometry independent of source material. */
    light = (!p_ptr->blind && (cave_info[y][x] & CAVE_SEEN)) ? 255 : 150;
    SDL_SetTextureColorMod(chasm_edge_texture, (Uint8)light,
        (Uint8)light, (Uint8)light);
    SDL_SetTextureAlphaMod(chasm_edge_texture, CHASM_EDGE_TEXTURE_ALPHA);
    SDL_RenderTexture(g_state.renderer, chasm_edge_texture, &src, dst);
    return true;
}

void sdl_chasm_edge_shutdown(void)
{
    SDL_DestroyTexture(chasm_edge_texture);
    chasm_edge_texture = NULL;
    chasm_edge_load_attempted = false;
}
