#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-bridge.h"

/*
 * Material transitions are deliberately a small renderer overlay.  The
 * normal terrain sprite remains responsible for the centre of a cell; this
 * pass only splats a few pixels from a visible neighbouring terrain sprite at
 * a material boundary.  That makes imported tiles and newly added styles
 * participate automatically without a second transition atlas.
 */
enum {
    MATERIAL_EDGE_WIDTH = 3,
    MATERIAL_EDGE_CONTACT_WIDTH = 2,
    MATERIAL_EDGE_MAX_PATCHES = 8,
    MATERIAL_EDGE_MAX_VERTICES = MATERIAL_EDGE_MAX_PATCHES * 4,
    MATERIAL_EDGE_MAX_INDICES = MATERIAL_EDGE_MAX_PATCHES * 6,
};

typedef enum material_edge_class {
    MATERIAL_EDGE_NONE = 0,
    MATERIAL_EDGE_FLOOR,
    MATERIAL_EDGE_WALL,
} material_edge_class;

typedef struct material_edge_source {
    material_edge_class class_id;
    u32b material_id;
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
    if (style >= 0)
    {
        /* Styles intentionally ignore F:TILE variant selection. */
        out->material_id = 0x80000000u | (u32b)style;
    }
    else
    {
        out->row = (byte)(ta & TILE_INDEX_MASK);
        out->col = (byte)((byte)tc & TILE_INDEX_MASK);
        /* Normalize only the comparison key.  A remembered/dark source must
         * still donate its actual ta/tc pixels, or the edge would brighten it
         * by sampling the light variant. */
        byte key_row = out->row;
        byte key_col = out->col;
        material_edge_normalize_source(feat, &key_row, &key_col);
        out->material_id = ((u32b)key_row << 8) | key_col;
    }

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
        byte na, nta;
        char nc, ntc;

        neighbours[i].class_id = MATERIAL_EDGE_NONE;
        neighbours[i].material_id = 0;
        neighbours[i].row = neighbours[i].col = 0;

        if (!material_edge_cell_visible(ny, nx))
            continue;

        /* p_ptr->image is rejected above because map_info() may randomize
         * hallucinated styles while resolving this terrain source. */
        na = nta = 0;
        nc = ntc = 0;
        map_info(ny, nx, &na, &nc, &nta, &ntc);
        if (!material_edge_describe(ny, nx, nta, ntc, &neighbours[i]))
            continue;

        if (material_edge_sources_differ(current, &neighbours[i]))
            count++;
        else if (!collect_sources)
            neighbours[i].class_id = MATERIAL_EDGE_NONE;
    }

    return count;
}

static float material_edge_pair_alpha(material_edge_class current,
    material_edge_class neighbour, bool diagonal)
{
    float alpha;

    if (current == MATERIAL_EDGE_FLOOR && neighbour == MATERIAL_EDGE_FLOOR)
        alpha = 0.58f;
    else if (current == MATERIAL_EDGE_WALL && neighbour == MATERIAL_EDGE_WALL)
        alpha = 0.50f;
    else if (current == MATERIAL_EDGE_FLOOR)
        alpha = 0.25f; /* restrained wall face at a floor edge */
    else
        alpha = 0.14f; /* never paint a floor over a wall interior */

    return diagonal ? alpha * 0.72f : alpha;
}

static float material_edge_pair_width(material_edge_class current,
    material_edge_class neighbour, bool diagonal)
{
    if (diagonal)
        return (float)MATERIAL_EDGE_CONTACT_WIDTH;
    if (current == MATERIAL_EDGE_WALL && neighbour == MATERIAL_EDGE_FLOOR)
        return (float)MATERIAL_EDGE_CONTACT_WIDTH;
    return (float)MATERIAL_EDGE_WIDTH;
}

static void material_edge_vertex(SDL_Vertex* vertex, float x, float y,
    float u, float v, float alpha)
{
    vertex->position = (SDL_FPoint){ x, y };
    vertex->color = (SDL_FColor){ 1.0f, 1.0f, 1.0f, alpha };
    vertex->tex_coord = (SDL_FPoint){ u, v };
}

/* Add one donor-pixel quad.  The outer edge is stronger than the inner edge,
 * so the centre of the already rendered tile stays unchanged. */
static bool material_edge_add_patch(SDL_Vertex* vertices, int* vertex_count,
    int* indices, int* index_count, const SDL_FRect* dst,
    const material_edge_source* donor, int direction, float width_px,
    float alpha, float texture_w, float texture_h)
{
    float x0, x1, y0, y1;
    float sx0, sx1, sy0, sy1;
    float du, dv;
    float source_width = width_px;
    int base;
    float outer_alpha = alpha;
    float inner_alpha = alpha * 0.12f;

    if (!vertices || !vertex_count || !indices || !index_count || !dst
        || !donor || *vertex_count + 4 > MATERIAL_EDGE_MAX_VERTICES
        || *index_count + 6 > MATERIAL_EDGE_MAX_INDICES
        || width_px <= 0.0f || texture_w <= 0.0f || texture_h <= 0.0f)
        return false;

    x0 = dst->x;
    x1 = dst->x + dst->w;
    y0 = dst->y;
    y1 = dst->y + dst->h;

    du = source_width * dst->w / (float)TILE_SIZE;
    dv = source_width * dst->h / (float)TILE_SIZE;
    if (du < 0.5f) du = 0.5f;
    if (dv < 0.5f) dv = 0.5f;
    if (du > dst->w) du = dst->w;
    if (dv > dst->h) dv = dst->h;

    sx0 = (float)donor->col * TILE_SIZE;
    sx1 = sx0 + TILE_SIZE;
    sy0 = (float)donor->row * TILE_SIZE;
    sy1 = sy0 + TILE_SIZE;

    /* The source and destination rectangles are kept inside one atlas cell;
     * the atlas's nearest filtering then cannot bleed an adjacent artwork. */
    switch (direction)
    {
    case 0: /* north */
        x0 = dst->x; x1 = dst->x + dst->w;
        y0 = dst->y; y1 = dst->y + dv;
        sx0 += 0.25f; sx1 -= 0.25f;
        sy0 = sy1 - source_width + 0.25f; sy1 -= 0.25f;
        material_edge_vertex(&vertices[*vertex_count + 0], x0, y0,
            sx0 / texture_w, sy0 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 1], x1, y0,
            sx1 / texture_w, sy0 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 2], x1, y1,
            sx1 / texture_w, sy1 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 3], x0, y1,
            sx0 / texture_w, sy1 / texture_h, inner_alpha);
        break;
    case 2: /* east */
        x0 = dst->x + dst->w - du; x1 = dst->x + dst->w;
        y0 = dst->y; y1 = dst->y + dst->h;
        sx0 = sx0 + 0.25f; sx1 = sx0 + source_width - 0.5f;
        sy0 += 0.25f; sy1 -= 0.25f;
        material_edge_vertex(&vertices[*vertex_count + 0], x0, y0,
            sx0 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 1], x1, y0,
            sx1 / texture_w, sy0 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 2], x1, y1,
            sx1 / texture_w, sy1 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 3], x0, y1,
            sx0 / texture_w, sy1 / texture_h, inner_alpha);
        break;
    case 4: /* south */
        x0 = dst->x; x1 = dst->x + dst->w;
        y0 = dst->y + dst->h - dv; y1 = dst->y + dst->h;
        sx0 += 0.25f; sx1 -= 0.25f;
        sy0 += 0.25f; sy1 = sy0 + source_width - 0.5f;
        material_edge_vertex(&vertices[*vertex_count + 0], x0, y0,
            sx0 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 1], x1, y0,
            sx1 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 2], x1, y1,
            sx1 / texture_w, sy1 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 3], x0, y1,
            sx0 / texture_w, sy1 / texture_h, outer_alpha);
        break;
    case 6: /* west */
        x0 = dst->x; x1 = dst->x + du;
        y0 = dst->y; y1 = dst->y + dst->h;
        sx0 = sx1 - source_width + 0.25f; sx1 -= 0.25f;
        sy0 += 0.25f; sy1 -= 0.25f;
        material_edge_vertex(&vertices[*vertex_count + 0], x0, y0,
            sx0 / texture_w, sy0 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 1], x1, y0,
            sx1 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 2], x1, y1,
            sx1 / texture_w, sy1 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 3], x0, y1,
            sx0 / texture_w, sy1 / texture_h, outer_alpha);
        break;
    case 1: /* north-east */
        x0 = dst->x + dst->w - du; x1 = dst->x + dst->w;
        y0 = dst->y; y1 = dst->y + dv;
        sx0 += 0.25f; sx1 = sx0 + source_width - 0.5f;
        sy0 = sy1 - source_width + 0.25f; sy1 -= 0.25f;
        material_edge_vertex(&vertices[*vertex_count + 0], x0, y0,
            sx0 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 1], x1, y0,
            sx1 / texture_w, sy0 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 2], x1, y1,
            sx1 / texture_w, sy1 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 3], x0, y1,
            sx0 / texture_w, sy1 / texture_h, inner_alpha);
        break;
    case 3: /* south-east */
        x0 = dst->x + dst->w - du; x1 = dst->x + dst->w;
        y0 = dst->y + dst->h - dv; y1 = dst->y + dst->h;
        sx0 += 0.25f; sx1 = sx0 + source_width - 0.5f;
        sy0 += 0.25f; sy1 = sy0 + source_width - 0.5f;
        material_edge_vertex(&vertices[*vertex_count + 0], x0, y0,
            sx0 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 1], x1, y0,
            sx1 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 2], x1, y1,
            sx1 / texture_w, sy1 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 3], x0, y1,
            sx0 / texture_w, sy1 / texture_h, inner_alpha);
        break;
    case 5: /* south-west */
        x0 = dst->x; x1 = dst->x + du;
        y0 = dst->y + dst->h - dv; y1 = dst->y + dst->h;
        sx0 = sx1 - source_width + 0.25f; sx1 -= 0.25f;
        sy0 += 0.25f; sy1 = sy0 + source_width - 0.5f;
        material_edge_vertex(&vertices[*vertex_count + 0], x0, y0,
            sx0 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 1], x1, y0,
            sx1 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 2], x1, y1,
            sx1 / texture_w, sy1 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 3], x0, y1,
            sx0 / texture_w, sy1 / texture_h, outer_alpha);
        break;
    case 7: /* north-west */
        x0 = dst->x; x1 = dst->x + du;
        y0 = dst->y; y1 = dst->y + dv;
        sx0 = sx1 - source_width + 0.25f; sx1 -= 0.25f;
        sy0 = sy1 - source_width + 0.25f; sy1 -= 0.25f;
        material_edge_vertex(&vertices[*vertex_count + 0], x0, y0,
            sx0 / texture_w, sy0 / texture_h, outer_alpha);
        material_edge_vertex(&vertices[*vertex_count + 1], x1, y0,
            sx1 / texture_w, sy0 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 2], x1, y1,
            sx1 / texture_w, sy1 / texture_h, inner_alpha);
        material_edge_vertex(&vertices[*vertex_count + 3], x0, y1,
            sx0 / texture_w, sy1 / texture_h, inner_alpha);
        break;
    default:
        return false;
    }

    base = *vertex_count;
    indices[*index_count + 0] = base + 0;
    indices[*index_count + 1] = base + 1;
    indices[*index_count + 2] = base + 2;
    indices[*index_count + 3] = base + 0;
    indices[*index_count + 4] = base + 2;
    indices[*index_count + 5] = base + 3;
    *vertex_count += 4;
    *index_count += 6;
    return true;
}

static bool material_edge_draw_sources(int y, int x,
    const material_edge_source* current,
    const material_edge_source neighbours[8], const SDL_FRect* dst)
{
    SDL_Vertex vertices[MATERIAL_EDGE_MAX_VERTICES];
    int indices[MATERIAL_EDGE_MAX_INDICES];
    int vertex_count = 0;
    int index_count = 0;
    float texture_w, texture_h;
    float live_scale;
    SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
    bool have_previous_blend_mode;
    bool drawn;

    if (!current || !neighbours || !dst || !g_state.tileset
        || !g_state.renderer)
        return false;
    if (!SDL_GetTextureSize(g_state.tileset, &texture_w, &texture_h))
        return false;

    /* A remembered source remains recognizable but must not brighten a dark
     * floor as though it were currently in LOS. */
    live_scale = (p_ptr && !p_ptr->blind
        && (cave_info[y][x] & CAVE_SEEN)) ? 1.0f : 0.72f;

    /* Diagonals are appended first so the four cardinal faces win ties at a
     * corner.  Within each group the clockwise order above is stable. */
    for (int pass = 0; pass < 2; pass++)
    {
        for (int i = 0; i < 8; i++)
        {
            bool diagonal = (i & 1) != 0;
            float width_px;
            float alpha;
            int direction;

            if ((pass == 0) != diagonal
                || !material_edge_sources_differ(current, &neighbours[i]))
                continue;

            direction = i;
            width_px = material_edge_pair_width(current->class_id,
                neighbours[i].class_id, diagonal);
            alpha = material_edge_pair_alpha(current->class_id,
                neighbours[i].class_id, diagonal) * live_scale;
            material_edge_add_patch(vertices, &vertex_count, indices,
                &index_count, dst, &neighbours[i], direction,
                width_px, alpha,
                texture_w, texture_h);
        }
    }

    if (vertex_count == 0)
        return false;

    have_previous_blend_mode = SDL_GetTextureBlendMode(g_state.tileset,
        &previous_blend_mode);
    SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
    drawn = SDL_RenderGeometry(g_state.renderer, g_state.tileset, vertices,
        vertex_count, indices, index_count);
    if (have_previous_blend_mode)
        SDL_SetTextureBlendMode(g_state.tileset, previous_blend_mode);
    return drawn;
}

bool sdl_material_edge_draw(int y, int x, byte ta, char tc,
    const SDL_FRect* dst)
{
    material_edge_source current;
    material_edge_source neighbours[8];

    if (!dst || !g_state.use_tiles || !g_state.renderer || !g_state.tileset
        || !p_ptr || p_ptr->image)
        return false;

    if (!material_edge_collect(y, x, ta, tc, &current, neighbours, true))
        return false;

    return material_edge_draw_sources(y, x, &current, neighbours, dst);
}

bool sdl_material_edge_at(int y, int x)
{
    byte a, ta;
    char c, tc;
    material_edge_source current;
    material_edge_source neighbours[8];

    if (!p_ptr || p_ptr->image || !material_edge_cell_visible(y, x))
        return false;

    a = ta = 0;
    c = tc = 0;
    map_info(y, x, &a, &c, &ta, &tc);
    return material_edge_collect(y, x, ta, tc, &current, neighbours, false) > 0;
}
