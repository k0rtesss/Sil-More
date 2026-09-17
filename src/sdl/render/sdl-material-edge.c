#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-bridge.h"

/*
 * Material transitions are deliberately a small renderer overlay.  The
 * normal terrain sprite remains responsible for the centre of a cell; this
 * pass resolves one owner for each boundary pixel. Imported tiles and newly
 * added styles participate automatically without a second transition atlas.
 */
enum {
    MATERIAL_EDGE_WIDTH = 3,
};

typedef enum material_edge_class {
    MATERIAL_EDGE_NONE = 0,
    MATERIAL_EDGE_FLOOR,
    MATERIAL_EDGE_WALL,
} material_edge_class;

typedef struct material_edge_source {
    material_edge_class class_id;
    u64b material_id;
    unsigned priority;
    byte row;
    byte col;
} material_edge_source;

/* Clockwise from north.  Keeping this order fixed is important at corners:
 * no gameplay RNG is consumed and repeated redraws produce the same pixels. */
static const int material_edge_dy[8] = {
    -1, -1, 0, 1, 1, 1, 0, -1
};
static const int material_edge_dx[8] = {
    0, 1, 1, 1, 0, -1, -1, -1
};

static bool material_edge_cell_visible(int y, int x)
{
    u16b info;

    if (!p_ptr || !in_bounds(y, x)
        || (unsigned)y >= (unsigned)p_ptr->cur_map_hgt
        || (unsigned)x >= (unsigned)p_ptr->cur_map_wid)
        return false;

    info = cave_info[y][x];

    /* This is intentionally the same gate as map_info().  A remembered floor
     * is only a usable source pixel while it is illuminated; a remembered
     * wall or other interesting feature is usable once marked. */
    if (cave_floorlike_bold(y, x))
    {
        if (!(info & CAVE_SEEN)
            && (!(info & CAVE_MARK) || cave_light[y][x] <= 0))
            return false;
    }
    else if (!(info & CAVE_MARK))
        return false;

    /* Rage and labyrinth views hide remembered terrain outside current LOS. */
    if (!p_ptr->is_dead && (p_ptr->rage || g_labyrinth_view_active)
        && !(info & CAVE_SEEN))
        return false;

    return true;
}

static bool material_edge_excluded_feature(byte feat)
{
    /* Water, hazards, chasm and bridges have their own animation or edge
     * treatment.  A bridge is excluded before looking through its underlay,
     * so its deck remains a complete foreground layer. */
    return FEAT_IS_BRIDGE(feat)
        || feat == FEAT_OPEN || feat == FEAT_BROKEN
        || (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)
        || feat == FEAT_WARDED || feat == FEAT_WARDED2 || feat == FEAT_WARDED3
        || feat == FEAT_CHASM
        || feat == FEAT_WATER
        || feat == FEAT_DEEP_WATER
        || feat == FEAT_LAVA
        || feat == FEAT_ICE
        || feat == FEAT_POISON;
}

static int material_edge_style(int y, int x)
{
    int style;

    if (!cave_color || !z_info || !style_info)
        return -1;

    style = styles_decode_color_style(cave_color[y][x]);
    if (style < 0 || style >= z_info->style_max || !style_info[style].name)
        return -1;

    return style;
}

/* Dark microchasm tiles use the immediately following atlas column.  They
 * are lighting variants of one artwork, not another material.  Only fold the
 * pair when it matches the feature's ordinary source coordinate; arbitrary
 * imported source coordinates remain distinct when no style is encoded. */
static void material_edge_normalize_source(int feat, byte* row, byte* col)
{
    byte base_row;
    byte base_col;

    if (!row || !col || !z_info || !f_info || feat < 0
        || feat >= z_info->f_max)
        return;

    base_row = (byte)(f_info[feat].x_attr & TILE_INDEX_MASK);
    base_col = (byte)((byte)f_info[feat].x_char & TILE_INDEX_MASK);
    if (*row == base_row
        && *col == (byte)((base_col + 1) & TILE_INDEX_MASK))
        *col = base_col;
}

static material_edge_class material_edge_feature_class(int y, int x,
    byte feat)
{
    byte mimic = FEAT_NONE;

    /* The terrain layer replaces these interesting features with a floor
     * underlay.  Keep their edge identity aligned with map_info(), even when
     * a test or an importer writes cave_feat directly and leaves CAVE_WALL
     * stale. */
    if (feat == FEAT_FLOOR || feat == FEAT_RAGE_FLOOR
        || feat == FEAT_SUNLIGHT || feat == FEAT_RUBBLE
        || FEAT_IS_TRAP(feat)
        || (feat >= FEAT_STAIR_HEAD && feat <= FEAT_STAIR_TAIL)
        || (feat >= FEAT_FORGE_HEAD && feat <= FEAT_FORGE_TAIL))
        return MATERIAL_EDGE_FLOOR;

    /* Open/broken doors have low feature IDs but still use door artwork. */
    if (feat == FEAT_OPEN || feat == FEAT_BROKEN
        || (feat >= FEAT_DOOR_HEAD && feat <= FEAT_WALL_TAIL)
        || feat == FEAT_RAGE_WALL
        || feat == FEAT_WARDED || feat == FEAT_WARDED2
        || feat == FEAT_WARDED3)
        return MATERIAL_EDGE_WALL;

    if (f_info && z_info && feat < z_info->f_max)
        mimic = f_info[feat].mimic;
    if (mimic == FEAT_FLOOR || mimic == FEAT_RAGE_FLOOR
        || mimic == FEAT_SUNLIGHT || mimic == FEAT_RUBBLE)
        return MATERIAL_EDGE_FLOOR;
    if (mimic == FEAT_OPEN || mimic == FEAT_BROKEN
        || (mimic >= FEAT_DOOR_HEAD && mimic <= FEAT_WALL_TAIL)
        || mimic == FEAT_RAGE_WALL)
        return MATERIAL_EDGE_WALL;

    /* CAVE_WALL remains a useful fallback for a custom feature whose visual
     * mimic is not one of the stock ranges. */
    if (cave_info[y][x] & CAVE_WALL)
        return MATERIAL_EDGE_WALL;

    /* Hidden traps are floorlike in map_info(); this fallback is needed for
     * custom hidden-floor features that carry the same CAVE_HIDDEN flag. */
    if (cave_floorlike_bold(y, x))
        return MATERIAL_EDGE_FLOOR;

    return MATERIAL_EDGE_NONE;
}

/* An artwork family, not a style number, defines a material. Two styles may
 * share the same art, and a bank override may replace a style's normal floor.
 * Sort and deduplicate the small variant set: variant ordering/frequency is
 * not a material boundary. Keep the actual lit/dark donor coordinates intact. */
static bool material_edge_floor_family(const style_type* style,
    material_edge_source* out)
{
    unsigned tiles[8];
    int count = 0;
    bool matches = false;
    int variants = style->floor_count ? MIN(style->floor_count, 8) : 1;
    for (int i = 0; i < variants; i++)
    {
        byte row = style->floor_count ? style->floor_rowv[i] : style->floor_row;
        byte col = style->floor_count ? style->floor_colv[i] : style->floor_col;
        unsigned tile = ((unsigned)row << 8) | col;
        if (out->row == row && (out->col == col || out->col == col + 1))
            matches = true;
        int at = 0;
        while (at < count && tiles[at] < tile) at++;
        if (at < count && tiles[at] == tile) continue;
        for (int j = count; j > at; j--) tiles[j] = tiles[j - 1];
        tiles[at] = tile;
        count++;
    }
    if (!matches) return false;
    out->priority = tiles[0];
    out->material_id = 14695981039346656037ULL;
    for (int i = 0; i < count; i++)
        out->material_id = (out->material_id ^ tiles[i]) * 1099511628211ULL;
    return true;
}

static bool material_edge_describe(int y, int x, byte ta, char tc,
    material_edge_source* out)
{
    byte feat;
    int style;

    if (!out || !material_edge_cell_visible(y, x)
        || !(ta & TILE_FLAG) || !(((byte)tc) & TILE_FLAG)
        || !cave_color)
        return false;

    feat = cave_feat[y][x];
    if (material_edge_excluded_feature(feat))
        return false;

    out->class_id = material_edge_feature_class(y, x, feat);
    if (out->class_id == MATERIAL_EDGE_NONE)
        return false;

    out->row = (byte)(ta & TILE_INDEX_MASK);
    out->col = (byte)((byte)tc & TILE_INDEX_MASK);
    style = material_edge_style(y, x);
    if (out->class_id == MATERIAL_EDGE_FLOOR && style_info && z_info)
    {
        if (style >= 0 && material_edge_floor_family(&style_info[style], out))
            return true;
        /* Actual shore art can come from a different style. */
        for (int i = 0; i < z_info->style_max; i++)
            if (style_info[i].name && material_edge_floor_family(&style_info[i], out))
                return true;
    }
    byte key_row = out->row;
    byte key_col = out->col;
    bool normalized = false;
    if (out->class_id == MATERIAL_EDGE_WALL && style_info && z_info)
    {
        for (int i = 0; i < z_info->style_max; i++)
        {
            const style_type* s = &style_info[i];
            if (s->name && key_row == s->wall_row
                && (key_col == s->wall_col || key_col == s->wall_col + 1))
            {
                key_col = s->wall_col;
                normalized = true;
                break;
            }
        }
    }
    if (!normalized)
        material_edge_normalize_source(feat, &key_row, &key_col);
    out->priority = ((unsigned)key_row << 8) | key_col;
    /* Match the singleton floor family key as well as unstyled coordinates. */
    out->material_id = (14695981039346656037ULL ^ out->priority) * 1099511628211ULL;

    return true;
}

static bool material_edge_sources_differ(const material_edge_source* a,
    const material_edge_source* b)
{
    return a && b && a->class_id != MATERIAL_EDGE_NONE &&
        b->class_id != MATERIAL_EDGE_NONE
        && (a->class_id != b->class_id
            || a->material_id != b->material_id);
}

/* Fill neighbours with the terrain layer returned by map_info(), rather than
 * reconstructing an atlas coordinate from cave_feat.  That preserves custom
 * floor/wall art and the current style selection. */
static int material_edge_collect(int y, int x, byte ta, char tc,
    material_edge_source* current, material_edge_source neighbours[8],
    bool collect_sources)
{
    int count = 0;

    if (!current || !neighbours || !p_ptr || p_ptr->image
        || !material_edge_describe(y, x, ta, tc, current))
        return 0;

    for (int i = 0; i < 8; i++)
    {
        int ny = y + material_edge_dy[i];
        int nx = x + material_edge_dx[i];
        byte nta = 0;
        char ntc = 0;

        neighbours[i].class_id = MATERIAL_EDGE_NONE;
        neighbours[i].material_id = 0;
        neighbours[i].row = neighbours[i].col = 0;

        if (!material_edge_cell_visible(ny, nx))
            continue;

        /* p_ptr->image is rejected above because map_info() may randomize
         * hallucinated styles while resolving this terrain source. */
        map_info_terrain(ny, nx, &nta, &ntc);
        if (!material_edge_describe(ny, nx, nta, ntc, &neighbours[i]))
            continue;

        if (material_edge_sources_differ(current, &neighbours[i]))
        {
            count++;
            if (!collect_sources) return count;
        }
        else if (!collect_sources)
            neighbours[i].class_id = MATERIAL_EDGE_NONE;
    }

    return count;
}

/* A stable total order makes exactly one side own a material boundary. The
 * upper artwork recedes into its own cell; the lower artwork stays intact.
 * Ranking uses canonical artwork coordinates, never style IDs or draw order. */
static bool material_edge_lower(const material_edge_source* current,
    const material_edge_source* donor)
{
    return donor->class_id == current->class_id
        && material_edge_sources_differ(current, donor)
        && (donor->priority > current->priority
            || (donor->priority == current->priority
                && donor->material_id > current->material_id));
}

/* World-anchored, sparse one-pixel contour variation. Endpoints use the same
 * width as the concave corner so edges of adjacent tiles always meet. */
static int material_edge_width(int y, int x, int along, bool horizontal)
{
    if (along < MATERIAL_EDGE_WIDTH || along >= TILE_SIZE - MATERIAL_EDGE_WIDTH)
        return MATERIAL_EDGE_WIDTH;
    u32b h = (u32b)(horizontal ? x : y) * 0x9e3779b9u
        ^ (u32b)(horizontal ? y : x) * 0x85ebca6bu
        ^ (u32b)(along / 3) * 0xc2b2ae35u;
    h ^= h >> 16;
    return MATERIAL_EDGE_WIDTH - (h % 5 == 0 ? 1 : 0);
}

/* Quarter-cell contour resolver. Cardinal edges define the two side cuts;
 * a missing diagonal makes a concave notch only when BOTH intervening cells
 * are known and on the same physical plane. A corner never reaches through
 * walls, liquids, doors or unknown cells. There is one donor, not a stack of
 * translucent strips, even at a junction of three or four materials. */
static int material_edge_owner(int y, int x, int px, int py,
    const material_edge_source* current, const material_edge_source n[8],
    bool preserve_floor_contour)
{
    if (preserve_floor_contour && current->class_id == MATERIAL_EDGE_FLOOR)
        return -1;
    int h = px < TILE_SIZE / 2 ? 6 : 2;
    int v = py < TILE_SIZE / 2 ? 0 : 4;
    int d = h == 6 ? (v == 0 ? 7 : 5) : (v == 0 ? 1 : 3);
    int u = px < TILE_SIZE / 2 ? px : TILE_SIZE - 1 - px;
    int w = py < TILE_SIZE / 2 ? py : TILE_SIZE - 1 - py;
    bool cut_h = material_edge_lower(current, &n[h]);
    bool cut_v = material_edge_lower(current, &n[v]);
    int owner = -1;
    if (cut_h && u < material_edge_width(y, x, py, false)) owner = h;
    if (cut_v && w < material_edge_width(y, x, px, true)
        && (owner < 0 || w < u || (w == u && material_edge_lower(&n[h], &n[v]))))
        owner = v;
    if (!cut_h && !cut_v && material_edge_lower(current, &n[d])
        && n[h].class_id == current->class_id
        && n[v].class_id == current->class_id
        && u + w < MATERIAL_EDGE_WIDTH)
        owner = d;
    return owner;
}

/* Physical height contacts are separate from material ownership. Preserve the
 * wall's own artwork and solidity; a small south/east face shade and a floor
 * contact shadow supply depth without borrowing floor pixels into a wall. */
static byte material_edge_shade(int px, int py,
    const material_edge_source* current, const material_edge_source n[8])
{
    int h = px < TILE_SIZE / 2 ? 6 : 2;
    int v = py < TILE_SIZE / 2 ? 0 : 4;
    int d = h == 6 ? (v == 0 ? 7 : 5) : (v == 0 ? 1 : 3);
    int u = px < TILE_SIZE / 2 ? px : TILE_SIZE - 1 - px;
    int w = py < TILE_SIZE / 2 ? py : TILE_SIZE - 1 - py;
    byte shade = 255;
    material_edge_class other = current->class_id == MATERIAL_EDGE_FLOOR
        ? MATERIAL_EDGE_WALL : MATERIAL_EDGE_FLOOR;
    if (current->class_id == MATERIAL_EDGE_FLOOR)
    {
        if (n[v].class_id == other && w < 2)
            shade = v == 0 ? (w == 0 ? 176 : 222) : (w == 0 ? 226 : 246);
        if (n[h].class_id == other && u < 2)
            shade = MIN(shade, h == 6 ? (u == 0 ? 198 : 234) : (u == 0 ? 218 : 242));
    }
    else
    {
        if (n[4].class_id == other && py >= TILE_SIZE - 2)
            shade = py == TILE_SIZE - 1 ? 180 : 218;
        if (n[2].class_id == other && px == TILE_SIZE - 1)
            shade = MIN(shade, 218);
    }
    if (n[d].class_id == other && u + w < 2
        && n[h].class_id == current->class_id && n[v].class_id == current->class_id)
        shade = MIN(shade, 226);
    return shade;
}

/* Resolve the small coverage field once, then merge equal pixels into maximal
 * rectangles. SDL's sprite path batches these and uses fast blits on software
 * renderers; triangle rasterization of individual pixel runs is much slower.
 * Only the coverage labels live on the stack; no textures or heap allocations
 * are created here. Donors continue at the same local texel coordinates. */
static bool material_edge_draw_sources(int y, int x,
    const material_edge_source* current,
    const material_edge_source neighbours[8], const SDL_FRect* dst,
    bool preserve_floor_contour)
{
    bool drawn = false;
    SDL_BlendMode texture_blend = SDL_BLENDMODE_NONE, draw_blend = SDL_BLENDMODE_NONE;
    Uint8 r, g, b, a, tr, tg, tb, ta;
    SDL_GetTextureBlendMode(g_state.tileset, &texture_blend);
    SDL_GetTextureColorMod(g_state.tileset, &tr, &tg, &tb);
    SDL_GetTextureAlphaMod(g_state.tileset, &ta);
    SDL_GetRenderDrawBlendMode(g_state.renderer, &draw_blend);
    SDL_GetRenderDrawColor(g_state.renderer, &r, &g, &b, &a);
    SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(g_state.tileset, 255, 255, 255);
    SDL_SetTextureAlphaMod(g_state.tileset, 255);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

    /* Shade the composited result after material ownership, preserving native
     * atlas pixels. Choosing a single shade avoids doubled corner shadows. */
    for (int pass = 0; pass < 2; pass++)
    {
        byte coverage[TILE_SIZE][TILE_SIZE];
        for (int py = 0; py < TILE_SIZE; py++)
            for (int px = 0; px < TILE_SIZE; px++)
                coverage[py][px] = pass
                    ? 255 - material_edge_shade(px, py, current, neighbours)
                    : 1 + material_edge_owner(y, x, px, py, current, neighbours,
                        preserve_floor_contour);
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
                if (pass)
                {
                    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, label);
                    drawn |= SDL_RenderFillRect(g_state.renderer, &rect);
                }
                else
                {
                    const material_edge_source* source = &neighbours[label - 1];
                    SDL_FRect src = {source->col * TILE_SIZE + px,
                        source->row * TILE_SIZE + py, right - px, bottom - py};
                    /* Transparent texels mean the same black backing as a
                     * normal terrain tile, not fragments of the old owner. */
                    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
                    SDL_RenderFillRect(g_state.renderer, &rect);
                    drawn |= SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &rect);
                }
            }
    }
    SDL_SetTextureBlendMode(g_state.tileset, texture_blend);
    SDL_SetTextureColorMod(g_state.tileset, tr, tg, tb);
    SDL_SetTextureAlphaMod(g_state.tileset, ta);
    SDL_SetRenderDrawBlendMode(g_state.renderer, draw_blend);
    SDL_SetRenderDrawColor(g_state.renderer, r, g, b, a);
    return drawn;
}

bool sdl_material_edge_draw(int y, int x, byte ta, char tc,
    const SDL_FRect* dst, bool preserve_floor_contour)
{
    material_edge_source current;
    material_edge_source neighbours[8];

    if (!dst || !g_state.use_tiles || !g_state.renderer || !g_state.tileset
        || !p_ptr || p_ptr->image)
        return false;

    if (!material_edge_collect(y, x, ta, tc, &current, neighbours, true))
        return false;

    return material_edge_draw_sources(y, x, &current, neighbours, dst, preserve_floor_contour);
}

bool sdl_material_edge_at(int y, int x)
{
    byte ta = 0;
    char tc = 0;
    material_edge_source current;
    material_edge_source neighbours[8];

    if (!g_state.use_tiles || !g_state.renderer || !g_state.tileset
        || !p_ptr || p_ptr->image || !material_edge_cell_visible(y, x))
        return false;

    map_info_terrain(y, x, &ta, &tc);
    return material_edge_collect(y, x, ta, tc, &current, neighbours, false) > 0;
}
