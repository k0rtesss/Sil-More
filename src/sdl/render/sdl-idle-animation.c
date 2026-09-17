#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-fixtures.h"
#include "cave/cave-bridge.h"
#include "cave/cave-water-flow.h"
#include "sdl/render/sdl-bridge.h"

/* Fixtures share one 160x16 atlas; each animated liquid shares one 64x16 atlas.
 * Connected surfaces use 256x256 atlas pages. Calm freshwater and acid have
 * four Verdant 03 frames; directional currents retain their three native
 * frames.
 * The original isolated textures remain the fallback if an atlas is absent.
 * No I/O, texture creation, timers, threads, or heap allocation occurs in
 * the animation update. */
/* A shared 25 Hz ceiling coalesces independent fixture deadlines. Each flame
 * advances only every 4-6 steps (160-240 ms), without a timer per fixture. */
#define IDLE_STEP_NS 40000000ULL
#define FIXTURE_TORCH_FRAME_COUNT 4
#define FIXTURE_ATLAS_FRAME_COUNT 3
#define FIXTURE_ATLAS_TORCH_FRAME_START FIXTURE_TORCH_FRAME_COUNT
#define FIXTURE_BRAZIER_FRAME_START \
    (FIXTURE_ATLAS_TORCH_FRAME_START + FIXTURE_ATLAS_FRAME_COUNT)
#define FIXTURE_TOTAL_FRAME_COUNT \
    (FIXTURE_BRAZIER_FRAME_START + FIXTURE_ATLAS_FRAME_COUNT)
#define WATER_STILL_FRAME_COUNT 4
#define WATER_CURRENT_FRAME_COUNT 3
#define WATER_DIRECTION_COUNT 5
#define WATER_PAGE_COUNT \
    (WATER_STILL_FRAME_COUNT \
        + (WATER_DIRECTION_COUNT - 1) * WATER_CURRENT_FRAME_COUNT)
#define CAVE_STYLE_DIRT 44
#define CAVE_STYLE_SNOW 62
#define CAVE_STYLE_BASALT 63

typedef struct idle_cell {
    int col, row, y, x;
    int width;
    byte a, ta;
    char c, tc;
    byte frame_steps, phase_steps, drawn_frame, frame_count;
    byte liquid_feat;
} idle_cell;

static SDL_Texture* fixture_texture;
static bool fixture_load_attempted;
static idle_cell* cells;
static int cell_count, cell_capacity;
static Uint64 frame_tick;
static SDL_Texture* water_texture;
static bool water_load_attempted;
static SDL_Texture* lava_texture;
static bool lava_load_attempted;
static SDL_Texture* poison_texture;
static bool poison_load_attempted;
static SDL_Texture* ice_texture;
static bool ice_load_attempted;
static SDL_Texture* ice_transition_texture;
static SDL_Texture* lava_transition_texture;
static bool ice_transition_load_attempted;
static bool lava_transition_load_attempted;
static SDL_Texture* water_transition_texture;
static SDL_Texture* deep_water_transition_texture;
static SDL_Texture* poison_transition_texture;
static bool water_transition_load_attempted;
static bool deep_water_transition_load_attempted;
static bool poison_transition_load_attempted;
static SDL_Texture* water_depth_transition_texture;
static SDL_Texture* water_bank_overlay_texture;
static bool water_depth_transition_load_attempted;
static bool water_bank_overlay_load_attempted;
static SDL_Texture* snow_dirt_transition_texture;
static SDL_Texture* basalt_dirt_transition_texture;
static bool snow_dirt_transition_load_attempted;
static bool basalt_dirt_transition_load_attempted;

static bool liquid_is_water(byte feat)
{
    return feat == FEAT_WATER || feat == FEAT_DEEP_WATER;
}

static bool liquid_has_current(byte feat)
{
    return liquid_is_water(feat) || feat == FEAT_POISON;
}

static byte liquid_frame_count(byte feat)
{
    if (feat == FEAT_ICE || feat == FEAT_CHASM) return 1;
    if ((feat == FEAT_POISON && poison_transition_texture)
        || (feat == FEAT_WATER && water_transition_texture)
        || (feat == FEAT_DEEP_WATER && deep_water_transition_texture))
        return feat == FEAT_POISON ? 3 : WATER_CURRENT_FRAME_COUNT;
    /* The original fallback strips and Verdant 03 calm water contain four
     * frames. */
    return 4;
}

static byte liquid_frame_count_at(int y, int x, byte feat)
{
    if (liquid_has_current(feat) && !cave_water_flow_direction(y, x))
        return WATER_STILL_FRAME_COUNT;
    return liquid_frame_count(feat);
}

static int water_page_index(int direction, int frame)
{
    if (direction <= 0)
        return frame;
    return WATER_STILL_FRAME_COUNT
        + (direction - 1) * WATER_CURRENT_FRAME_COUNT + frame;
}

static byte visible_liquid(int y, int x)
{
    u16b info;
    if (!p_ptr || !in_bounds(y, x))
        return 0;
    byte feat = cave_bridge_underlay(cave_feat[y][x]);
    if (feat != FEAT_WATER && feat != FEAT_DEEP_WATER && feat != FEAT_LAVA && feat != FEAT_ICE
        && feat != FEAT_POISON && feat != FEAT_CHASM)
        return 0;
    info = cave_info[y][x];
    if (!(info & (CAVE_MARK | CAVE_SEEN))
        || ((p_ptr->rage || g_labyrinth_view_active) && !(info & CAVE_SEEN)))
        return 0;
    return feat;
}

/* Raw clockwise eight-neighbor masks index the atlas. Unknown terrain always
 * contributes zero, regardless of its feature, so shore shapes reveal nothing
 * beyond explored terrain. Bridges connect through their liquid underlay. */
static byte liquid_transition_mask(int y, int x, byte feat)
{
    static const int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    static const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    byte mask = 0;
    for (int i = 0; i < 8; i++)
    {
        byte neighbor = visible_liquid(y + dy[i], x + dx[i]);
        if (neighbor == feat || (liquid_is_water(feat) && liquid_is_water(neighbor)))
            mask |= (byte)(1u << i);
    }
    return mask;
}

/* The three Verdant chasm fills are static tiling variants, not transition
 * pieces. Choose them from world coordinates so the pattern remains stable
 * while the map pans or repaints, without consuming gameplay RNG. */
static int chasm_fill_variant(int y, int x)
{
    u32b hash = (u32b)y * 0x9E3779B9U ^ (u32b)x * 0x85EBCA6BU;
    hash ^= hash >> 16;
    return (int)(hash % 3U);
}

static void draw_chasm_fill(int y, int x, const SDL_FRect* dst, bool live)
{
    char tile = (char)(TILE_FLAG | chasm_fill_variant(y, x) * 2
        | (live ? 0 : 1));
    sdl_draw_tileset_sprite(f_info[FEAT_CHASM].x_attr, tile, dst, false);
}

/* Treat every non-deep neighbor, including unknown terrain, as connected
 * shoal. Only known deep water may introduce a depth boundary. */
static byte water_depth_transition_mask(int y, int x)
{
    static const int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    static const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    byte mask = 255;
    for (int i = 0; i < 8; i++)
        if (visible_liquid(y + dy[i], x + dx[i]) == FEAT_DEEP_WATER)
            mask &= (byte)~(1u << i);
    return mask;
}

static SDL_Texture* load_transition_atlas(SDL_Texture** texture,
    bool* attempted, const char* path, int frame_count)
{
    if (*texture || *attempted) return *texture;
    *attempted = true;
    SDL_Surface* atlas = IMG_Load(path);
    if (!atlas || atlas->w != 16 * TILE_SIZE
        || atlas->h != frame_count * 16 * TILE_SIZE)
    {
        log_warn("Terrain transitions unavailable: %s (%s)", path, SDL_GetError());
        SDL_DestroySurface(atlas);
        return NULL;
    }
    *texture = SDL_CreateTextureFromSurface(g_state.renderer, atlas);
    SDL_DestroySurface(atlas);
    if (*texture)
    {
        SDL_SetTextureScaleMode(*texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);
    }
    return *texture;
}

/* The elemental big-cave floor styles are encoded in cave_color rather than
 * in the feature itself. Dirt is style 44, so it remains the outside material
 * and the renderer only needs to draw the upper-material edge for styles 62/63. */
static bool visible_material_floor_style(int y, int x, int* style_out)
{
    u16b info;
    byte feat;
    int style;
    if (!p_ptr || !in_bounds(y, x))
        return false;
    feat = cave_bridge_underlay(cave_feat[y][x]);
    if (feat != FEAT_FLOOR && feat != FEAT_RAGE_FLOOR
        && feat != FEAT_SUNLIGHT)
        return false;
    info = cave_info[y][x];
    if (!(info & (CAVE_MARK | CAVE_SEEN))
        || ((p_ptr->rage || g_labyrinth_view_active) && !(info & CAVE_SEEN)))
        return false;
    style = styles_decode_color_style(cave_color[y][x]);
    if (style_out)
        *style_out = style;
    return true;
}

static bool visible_known_terrain(int y, int x)
{
    u16b info;
    if (!p_ptr || !in_bounds(y, x))
        return false;
    info = cave_info[y][x];
    return (info & (CAVE_MARK | CAVE_SEEN))
        && !((p_ptr->rage || g_labyrinth_view_active) && !(info & CAVE_SEEN));
}

static int visible_elemental_floor_style(int y, int x)
{
    int style;
    if (!visible_material_floor_style(y, x, &style))
        return -1;
    return (style == CAVE_STYLE_SNOW || style == CAVE_STYLE_BASALT)
        ? style : -1;
}

static bool elemental_transition_has_dirt_neighbor(int y, int x)
{
    static const int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    static const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    for (int i = 0; i < 8; i++)
    {
        int style;
        if (visible_material_floor_style(y + dy[i], x + dx[i], &style)
            && style == CAVE_STYLE_DIRT)
            return true;
    }
    return false;
}

static byte elemental_transition_mask(int y, int x)
{
    static const int dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    static const int dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    byte mask = 0;
    for (int i = 0; i < 8; i++)
    {
        int neighbor_style;
        if (visible_material_floor_style(y + dy[i], x + dx[i], &neighbor_style))
        {
            /* Only DirtCave is outside material. Other known floor styles
             * are kept connected so a cave edge cannot paint dirt over them. */
            if (neighbor_style != CAVE_STYLE_DIRT)
                mask |= (byte)(1u << i);
        }
        else if (visible_known_terrain(y + dy[i], x + dx[i]))
        {
            /* Walls, hazards and other known non-floor features are not dirt. */
            mask |= (byte)(1u << i);
        }
    }
    return mask;
}

static SDL_Texture* load_elemental_transition_texture(int style)
{
    if (style == CAVE_STYLE_SNOW)
        return load_transition_atlas(&snow_dirt_transition_texture,
            &snow_dirt_transition_load_attempted,
            "lib/xtra/graf/transition_snow_on_dirt.png", 1);
    if (style == CAVE_STYLE_BASALT)
        return load_transition_atlas(&basalt_dirt_transition_texture,
            &basalt_dirt_transition_load_attempted,
            "lib/xtra/graf/transition_basalt_on_dirt.png", 1);
    return NULL;
}

static bool draw_elemental_transition(int y, int x, const SDL_FRect* dst)
{
    int style = visible_elemental_floor_style(y, x);
    if (style < 0)
        return false;
    if (!elemental_transition_has_dirt_neighbor(y, x))
        return false;

    /* A fully connected cell already has the correct variant from F:TILE;
     * skipping it preserves the four snow floor variants and avoids replacing
     * a solid basalt tile with a single atlas sample. */
    byte mask = elemental_transition_mask(y, x);
    if (mask == 255)
        return false;

    SDL_Texture* transition = load_elemental_transition_texture(style);
    if (!transition)
        return false;
    SDL_FRect src = { (mask % 16) * TILE_SIZE,
        (mask / 16) * TILE_SIZE, TILE_SIZE, TILE_SIZE };
    bool live = !p_ptr->blind && (cave_info[y][x] & CAVE_SEEN);
    int light = live ? 255 : 96;
    SDL_SetTextureColorMod(transition, light, light, light);
    return SDL_RenderTexture(g_state.renderer, transition, &src, dst);
}

static SDL_Texture* load_liquid_transition_texture(byte feat)
{
    SDL_Texture** texture;
    bool* attempted;
    const char* path;
    int frame_count;
    switch (feat)
    {
    case FEAT_ICE:
        texture = &ice_transition_texture;
        attempted = &ice_transition_load_attempted;
        path = "lib/xtra/graf/transition_ice_on_snow.png";
        frame_count = 1;
        break;
    case FEAT_LAVA:
        texture = &lava_transition_texture;
        attempted = &lava_transition_load_attempted;
        path = "lib/xtra/graf/transition_lava_on_basalt.png";
        frame_count = 4;
        break;
    case FEAT_WATER:
        texture = &water_transition_texture;
        attempted = &water_transition_load_attempted;
        path = "lib/xtra/graf/transition_freshwater.png";
        frame_count = WATER_PAGE_COUNT;
        break;
    case FEAT_DEEP_WATER:
        texture = &deep_water_transition_texture;
        attempted = &deep_water_transition_load_attempted;
        path = "lib/xtra/graf/transition_freshwater_deep.png";
        frame_count = WATER_PAGE_COUNT;
        break;
    case FEAT_POISON:
        texture = &poison_transition_texture;
        attempted = &poison_transition_load_attempted;
        path = "lib/xtra/graf/transition_acid_on_stone.png";
        frame_count = WATER_PAGE_COUNT;
        break;
    default:
        return NULL;
    }
    if (liquid_is_water(feat))
    {
        /* Load once on the initial water draw, so new neighbor connectivity
         * never triggers asset I/O during an idle animation update. */
        load_transition_atlas(&water_transition_texture,
            &water_transition_load_attempted,
            "lib/xtra/graf/transition_freshwater.png", WATER_PAGE_COUNT);
        load_transition_atlas(&deep_water_transition_texture,
            &deep_water_transition_load_attempted,
            "lib/xtra/graf/transition_freshwater_deep.png", WATER_PAGE_COUNT);
        load_transition_atlas(&water_depth_transition_texture,
            &water_depth_transition_load_attempted,
            "lib/xtra/graf/transition_freshwater_depth.png", WATER_PAGE_COUNT);
        load_transition_atlas(&water_bank_overlay_texture,
            &water_bank_overlay_load_attempted,
            "lib/xtra/graf/transition_freshwater_bank.png", WATER_PAGE_COUNT);
    }
    return load_transition_atlas(texture, attempted, path, frame_count);
}

static bool load_liquid_texture(byte feat)
{
    SDL_Surface* atlas;
    if (feat == FEAT_ICE)
    {
        if (ice_texture || ice_load_attempted)
            return ice_texture != NULL;
        ice_load_attempted = true;
        atlas = IMG_Load("lib/xtra/graf/ice_sheet.png");
        if (!atlas || atlas->w != TILE_SIZE || atlas->h != TILE_SIZE)
        {
            log_warn("Ice surface unavailable: %s", SDL_GetError());
            SDL_DestroySurface(atlas);
            return false;
        }
        ice_texture = SDL_CreateTextureFromSurface(g_state.renderer, atlas);
        SDL_DestroySurface(atlas);
        if (!ice_texture) return false;
        SDL_SetTextureScaleMode(ice_texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(ice_texture, SDL_BLENDMODE_BLEND);
        return true;
    }
    SDL_Texture** texture = feat == FEAT_POISON ? &poison_texture
        : feat == FEAT_LAVA ? &lava_texture : &water_texture;
    bool* attempted = feat == FEAT_POISON ? &poison_load_attempted
        : feat == FEAT_LAVA ? &lava_load_attempted : &water_load_attempted;
    const char* name = (feat == FEAT_LAVA || feat == FEAT_POISON)
        ? "lava_flow" : "water_surface";
    if (*texture || *attempted)
        return *texture != NULL;
    *attempted = true;
    atlas = SDL_CreateSurface(4 * TILE_SIZE, TILE_SIZE, SDL_PIXELFORMAT_RGBA32);
    if (!atlas) return false;
    for (int frame = 0; frame < 4; frame++)
    {
        char path[256];
        SDL_Rect dst = { frame * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE };
        strnfmt(path, sizeof(path), "lib/xtra/graf/anim_%s_f%d.png", name, frame);
        SDL_Surface* source = IMG_Load(path);
        if (!source || source->w != TILE_SIZE || source->h != TILE_SIZE)
        {
            log_warn("Liquid animation unavailable: %s (%s)", path, SDL_GetError());
            SDL_DestroySurface(source);
            SDL_DestroySurface(atlas);
            return false;
        }
        SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_NONE);
        SDL_BlitSurface(source, NULL, atlas, &dst);
        SDL_DestroySurface(source);
    }
    if (feat == FEAT_POISON)
    {
        /* Exactly the lava pixels and frames, with warm channels turned green
         * and the green channel slightly toned down. RGBA32 guarantees byte
         * order; retain alpha and the source geometry. Recolor once when
         * loading, never in the idle animation update. */
        if (!SDL_LockSurface(atlas))
        {
            SDL_DestroySurface(atlas);
            return false;
        }
        for (int y = 0; y < atlas->h; y++)
            for (int x = 0; x < atlas->w; x++)
            {
                Uint8* pixel = (Uint8*)atlas->pixels + y * atlas->pitch + x * 4;
                Uint8 red = pixel[0];
                pixel[0] = pixel[1] / 2;
                pixel[1] = (Uint8)(red * 7 / 8);
            }
        SDL_UnlockSurface(atlas);
    }
    *texture = SDL_CreateTextureFromSurface(g_state.renderer, atlas);
    SDL_DestroySurface(atlas);
    if (!*texture) return false;
    SDL_SetTextureScaleMode(*texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(*texture, SDL_BLENDMODE_BLEND);
    frame_tick = SDL_GetTicksNS() / IDLE_STEP_NS;
    return true;
}

static bool draw_liquid(int y, int x, const SDL_FRect* dst)
{
    byte feat = cave_bridge_underlay(cave_feat[y][x]);
    bool live = !p_ptr->blind && (cave_info[y][x] & CAVE_SEEN);
    if (feat == FEAT_CHASM)
    {
        draw_chasm_fill(y, x, dst, live);
        return true;
    }
    if (!load_liquid_texture(feat))
    {
        /* Keep lethal terrain recognizable even if an asset is missing. */
        if (feat == FEAT_LAVA)
            SDL_SetRenderDrawColor(g_state.renderer, live ? 240 : 90,
                live ? 74 : 28, live ? 16 : 6, 255);
        else if (feat == FEAT_POISON)
            SDL_SetRenderDrawColor(g_state.renderer, live ? 37 : 14,
                live ? 210 : 79, live ? 16 : 6, 255);
        else if (feat == FEAT_ICE)
            SDL_SetRenderDrawColor(g_state.renderer, live ? 156 : 58,
                live ? 216 : 81, live ? 232 : 87, 255);
        else if (feat == FEAT_DEEP_WATER)
            SDL_SetRenderDrawColor(g_state.renderer, live ? 9 : 3,
                live ? 29 : 11, live ? 94 : 35, 255);
        else
            SDL_SetRenderDrawColor(g_state.renderer, 24, 78, 108, 255);
        SDL_RenderFillRect(g_state.renderer, dst);
        return true;
    }
    SDL_Texture* texture = feat == FEAT_ICE ? ice_texture
        : feat == FEAT_POISON ? poison_texture
        : feat == FEAT_LAVA ? lava_texture : water_texture;
    SDL_Texture* transition = load_liquid_transition_texture(feat);
    int frame = live ? (int)((frame_tick / 8)
        % liquid_frame_count_at(y, x, feat)) : 0;
    SDL_FRect src = { frame * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE };
    SDL_FRect bank_src = { 0 };
    bool draw_bank = false;
    if (transition)
    {
        byte mask = liquid_transition_mask(y, x, feat);
        texture = transition;
        src.x = (mask % 16) * TILE_SIZE;
        /* Select surface direction independently of the fixed shoreline mask.
         * Rotating an entire tile would rotate its bank into the channel. */
        int page = liquid_has_current(feat)
            ? water_page_index(cave_water_flow_direction(y, x), frame) : frame;
        src.y = (mask / 16 + page * 16) * TILE_SIZE;
        if (feat == FEAT_WATER && water_depth_transition_texture
            && water_bank_overlay_texture)
        {
            byte depth_mask = water_depth_transition_mask(y, x);
            if (depth_mask != 255)
            {
                bank_src = src;
                draw_bank = true;
                texture = water_depth_transition_texture;
                src.x = (depth_mask % 16) * TILE_SIZE;
                src.y = (depth_mask / 16 + page * 16) * TILE_SIZE;
            }
        }
    }
    int light = live ? 255 : 96;
    bool tint_deep = feat == FEAT_DEEP_WATER && !transition;
    SDL_SetTextureColorMod(texture,
        tint_deep ? 80 * light / 255 : light,
        tint_deep ? 105 * light / 255 : light,
        tint_deep ? 205 * light / 255 : light);
    SDL_RenderTexture(g_state.renderer, texture, &src, dst);
    if (draw_bank)
    {
        SDL_SetTextureColorMod(water_bank_overlay_texture, light, light, light);
        SDL_RenderTexture(g_state.renderer, water_bank_overlay_texture, &bank_src, dst);
    }
    return true;
}

static byte fixture_frame_count(byte kind)
{
    return kind == CAVE_FIXTURE_WALL_TORCH
        ? FIXTURE_TORCH_FRAME_COUNT : FIXTURE_ATLAS_FRAME_COUNT;
}

static void fixture_timing(int y, int x, byte kind, byte* steps, byte* phase)
{
    /* Stable across viewport changes, with no gameplay RNG or saved state. */
    Uint32 hash = (Uint32)x * 0x9e3779b9u ^ (Uint32)y * 0x85ebca6bu;
    byte frame_count = fixture_frame_count(kind);
    hash ^= (Uint32)(p_ptr ? p_ptr->depth : 0) * 0xc2b2ae35u;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    *steps = (byte)(4 + hash % 3);
    *phase = (byte)((hash >> 8) % (*steps * frame_count));
}

static byte fixture_frame_start(byte kind)
{
    switch (kind)
    {
    case CAVE_FIXTURE_WALL_TORCH_2:
        return FIXTURE_ATLAS_TORCH_FRAME_START;
    case CAVE_FIXTURE_BRAZIER:
        return FIXTURE_BRAZIER_FRAME_START;
    default:
        return 0;
    }
}

static byte fixture_frame(Uint64 tick, byte steps, byte phase, byte count)
{
    return (byte)(((tick + phase) / steps) % count);
}

static Uint64 fixture_next_tick(Uint64 tick, byte steps, byte phase)
{
    return tick + steps - (tick + phase) % steps;
}

static bool load_fixture_texture(void)
{
    SDL_Surface* atlas;
    SDL_Surface* source;

    if (fixture_load_attempted)
        return fixture_texture != NULL;
    fixture_load_attempted = true;
    atlas = SDL_CreateSurface(TILE_SIZE * FIXTURE_TOTAL_FRAME_COUNT, TILE_SIZE,
        SDL_PIXELFORMAT_RGBA32);
    if (!atlas)
        return false;
    SDL_ClearSurface(atlas, 0, 0, 0, 0);
    for (int frame = 0; frame < FIXTURE_TORCH_FRAME_COUNT; frame++)
    {
        char path[128];
        SDL_Rect dst = { frame * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE };
        strnfmt(path, sizeof(path), "lib/xtra/graf/anim_torch_f%d.png", frame);
        source = IMG_Load(path);
        if (!source || source->w != TILE_SIZE || source->h != TILE_SIZE)
        {
            log_warn("Idle fixture animation unavailable: %s (%s)", path,
                SDL_GetError());
            SDL_DestroySurface(source);
            SDL_DestroySurface(atlas);
            return false;
        }
        SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_NONE);
        SDL_BlitSurface(source, NULL, atlas, &dst);
        SDL_DestroySurface(source);
    }
    source = IMG_Load("lib/xtra/graf/animated.png");
    if (!source || source->w != TILE_SIZE * FIXTURE_ATLAS_FRAME_COUNT * 2
        || source->h != TILE_SIZE)
    {
        log_warn("Idle fixture animation unavailable: animated.png (%s)",
            SDL_GetError());
        SDL_DestroySurface(source);
        SDL_DestroySurface(atlas);
        return false;
    }
    SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_NONE);
    SDL_Rect dst = { FIXTURE_ATLAS_TORCH_FRAME_START * TILE_SIZE, 0,
        source->w, source->h };
    SDL_BlitSurface(source, NULL, atlas, &dst);
    SDL_DestroySurface(source);
    fixture_texture = SDL_CreateTextureFromSurface(g_state.renderer, atlas);
    SDL_DestroySurface(atlas);
    if (!fixture_texture)
    {
        log_warn("Idle fixture texture unavailable: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(fixture_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(fixture_texture, SDL_BLENDMODE_BLEND);
    frame_tick = SDL_GetTicksNS() / IDLE_STEP_NS;
    return true;
}

void sdl_idle_animation_clear_cells(void)
{
    cell_count = 0;
}

bool sdl_idle_animation_tracks_grid(int y, int x)
{
    for (int i = 0; i < cell_count; i++)
        if (cells[i].y == y && cells[i].x == x)
            return true;
    return false;
}

void sdl_idle_animation_redraw_cached_cells(
    void (*redraw_cell)(int col, int row, int width))
{
    if (Term != term_screen || !redraw_cell)
        return;
    /* A pan can put an identical wall glyph under a former torch. Mark the
     * OLD screen positions before prt_map queues their new terrain, so the
     * terminal cannot retain the decoration's pixels there. This visits only
     * cached fixtures and queues repainting; it does not render or present. */
    for (int i = 0; i < cell_count; i++)
        redraw_cell(cells[i].col, cells[i].row, cells[i].width);
}

void sdl_idle_animation_shutdown(void)
{
    sdl_chasm_edge_shutdown();
    SDL_DestroyTexture(water_depth_transition_texture);
    water_depth_transition_texture = NULL;
    water_depth_transition_load_attempted = false;
    SDL_DestroyTexture(water_bank_overlay_texture);
    water_bank_overlay_texture = NULL;
    water_bank_overlay_load_attempted = false;
    SDL_DestroyTexture(water_transition_texture);
    water_transition_texture = NULL;
    water_transition_load_attempted = false;
    SDL_DestroyTexture(deep_water_transition_texture);
    deep_water_transition_texture = NULL;
    deep_water_transition_load_attempted = false;
    SDL_DestroyTexture(poison_transition_texture);
    poison_transition_texture = NULL;
    poison_transition_load_attempted = false;
    SDL_DestroyTexture(ice_transition_texture);
    ice_transition_texture = NULL;
    ice_transition_load_attempted = false;
    SDL_DestroyTexture(lava_transition_texture);
    lava_transition_texture = NULL;
    lava_transition_load_attempted = false;
    SDL_DestroyTexture(snow_dirt_transition_texture);
    snow_dirt_transition_texture = NULL;
    snow_dirt_transition_load_attempted = false;
    SDL_DestroyTexture(basalt_dirt_transition_texture);
    basalt_dirt_transition_texture = NULL;
    basalt_dirt_transition_load_attempted = false;
    SDL_DestroyTexture(poison_texture);
    poison_texture = NULL;
    poison_load_attempted = false;
    SDL_DestroyTexture(ice_texture);
    ice_texture = NULL;
    ice_load_attempted = false;
    SDL_DestroyTexture(water_texture);
    water_texture = NULL;
    water_load_attempted = false;
    SDL_DestroyTexture(lava_texture);
    lava_texture = NULL;
    lava_load_attempted = false;
    SDL_DestroyTexture(fixture_texture);
    fixture_texture = NULL;
    fixture_load_attempted = false;
    SDL_free(cells);
    cells = NULL;
    cell_count = cell_capacity = 0;
}

/* Discovered fixtures keep burning outside the current field of view.
 * Unexplored/darkened terrain and the rage/labyrinth masks reveal nothing. */
static byte visible_fixture(int y, int x)
{
    byte kind = cave_fixture_at(y, x);
    u16b info;
    if (!kind)
        return CAVE_FIXTURE_NONE;
    info = cave_info[y][x];
    if (!(info & (CAVE_MARK | CAVE_SEEN)) || !(info & CAVE_GLOW)
        || ((p_ptr->rage || g_labyrinth_view_active) && !(info & CAVE_SEEN)))
        return CAVE_FIXTURE_NONE;
    return kind;
}

bool sdl_idle_animation_draw(int y, int x, const SDL_FRect* dst)
{
    if (g_state.use_tiles && visible_liquid(y, x))
    {
        bool drawn = draw_liquid(y, x, dst);
        if (FEAT_IS_BRIDGE(cave_feat[y][x])) sdl_draw_bridge_deck(y, x, dst);
        return drawn;
    }
    if (g_state.use_tiles && draw_elemental_transition(y, x, dst))
        return true;
    byte kind = visible_fixture(y, x);
    bool live;
    bool animate;
    byte steps, phase;
    int frame;
    SDL_FRect src;
    if (!kind || !g_state.use_tiles || !load_fixture_texture())
        return false;
    live = !p_ptr->blind;
    animate = live && (!op_ptr || op_ptr->opt[OPT_torch_animation_always]
        || (cave_info[y][x] & CAVE_SEEN));
    fixture_timing(y, x, kind, &steps, &phase);
    /* All frozen fixtures use the second frame, without a phase offset. */
    frame = animate
        ? fixture_frame(frame_tick, steps, phase, fixture_frame_count(kind)) : 1;
    src = (SDL_FRect){ (fixture_frame_start(kind) + frame) * TILE_SIZE, 0,
        TILE_SIZE, TILE_SIZE };
    SDL_SetTextureColorMod(fixture_texture, live ? 255 : 96,
        live ? 255 : 96, live ? 255 : 96);
    SDL_RenderTexture(g_state.renderer, fixture_texture, &src, dst);
    return true;
}

void sdl_idle_animation_invalidate_span(int col, int row, int width)
{
    if (Term != term_screen)
        return;
    for (int i = 0; i < cell_count;)
    {
        idle_cell* cell = &cells[i];
        if (cell->row == row && cell->col < col + width
            && cell->col + cell->width > col)
            cells[i] = cells[--cell_count];
        else
            i++;
    }
}

void sdl_idle_animation_track(int col, int row, int y, int x,
    byte a, char c, byte ta, char tc)
{
    idle_cell* cell;
    sdl_idle_animation_invalidate_span(col, row, use_bigtile ? 2 : 1);
    byte liquid = visible_liquid(y, x);
    if ((!liquid && !visible_fixture(y, x))
        || !(ta & TILE_FLAG) || !((byte)tc & TILE_FLAG))
        return;
    if (cell_count == cell_capacity)
    {
        int capacity = cell_capacity ? cell_capacity * 2 : 32;
        idle_cell* resized = SDL_realloc(cells, capacity * sizeof(*cells));
        if (!resized)
            return;
        cells = resized;
        cell_capacity = capacity;
    }
    cell = &cells[cell_count++];
    *cell = (idle_cell){ .col = col, .row = row, .y = y, .x = x,
        .width = use_bigtile ? 2 : 1, .a = a, .ta = ta, .c = c, .tc = tc };
    cell->liquid_feat = liquid;
    if (liquid)
    {
        cell->frame_steps = 8;
        cell->phase_steps = 0;
        cell->drawn_frame = (!p_ptr->blind && (cave_info[y][x] & CAVE_SEEN))
            ? (byte)((frame_tick / 8) % liquid_frame_count_at(y, x, liquid)) : 0;
        return;
    }
    byte fixture_kind = visible_fixture(y, x);
    fixture_timing(y, x, fixture_kind, &cell->frame_steps, &cell->phase_steps);
    cell->frame_count = fixture_frame_count(fixture_kind);
    cell->drawn_frame = (!p_ptr->blind
        && (op_ptr->opt[OPT_torch_animation_always] || (cave_info[y][x] & CAVE_SEEN)))
        ? fixture_frame(frame_tick, cell->frame_steps, cell->phase_steps,
            cell->frame_count) : 1;
}

static bool animation_context_active(void)
{
    SDL_WindowFlags flags;
    if ((!fixture_texture && !water_texture && !lava_texture && !poison_texture)
        || !cell_count || !g_state.window
        || !g_state.use_tiles || !sdl_mouse_gameplay_context_active()
        || g_minimap.active || g_main_menu_overlay_active
        || g_description_overlay.active || g_sdl_present_suppressed
        || g_suppress_layout_refresh_present
        || sdl_welcome_screen_active() || sdl_pause_text_screen_active()
        || sdl_tale_screen_active() || sdl_poetry_screen_active()
        || sdl_halls_screen_active() || sdl_hint_quest_menu_active()
        || sdl_character_sheet_screen_active()
        || !g_views[PANE_MAIN].canvas || p_ptr->blind)
        return false;
    flags = SDL_GetWindowFlags(g_state.window);
    return (flags & SDL_WINDOW_INPUT_FOCUS)
        && !(flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
}

static bool cell_can_animate(const idle_cell* cell)
{
    /* Chasm and ice are static; calm freshwater still has a real four-frame
     * surface animation and must remain in the idle scheduler. */
    if (cell->liquid_feat == FEAT_ICE || cell->liquid_feat == FEAT_CHASM)
        return false;
    term* t = term_screen;
    term_win* scr = t ? t->scr : NULL;
    SDL_FRect window_rect;
    int col = cell->col, row = cell->row;
    if (!scr || col < 0 || row < 0 || col >= t->wid || row >= t->hgt
        || cell->width != (use_bigtile ? 2 : 1)
        || cell->y != p_ptr->wy + row - ROW_MAP
        || cell->x != p_ptr->wx + (col - COL_MAP) / (use_bigtile ? 2 : 1)
        || !(cell->liquid_feat ? visible_liquid(cell->y, cell->x) == cell->liquid_feat
                         : visible_fixture(cell->y, cell->x)))
        return false;
    if (cell->liquid_feat
        && (!(cell->liquid_feat == FEAT_POISON ? poison_texture
                : cell->liquid_feat == FEAT_LAVA ? lava_texture : water_texture)
            || !(cave_info[cell->y][cell->x] & CAVE_SEEN)))
        return false;
    if (op_ptr && !op_ptr->opt[OPT_torch_animation_always]
        && !(cave_info[cell->y][cell->x] & CAVE_SEEN))
        return false;
    /* Cached cells can outlive a pan, saved screen, or pending term redraw. */
    if (scr->a[row][col] != cell->a || scr->c[row][col] != cell->c
        || scr->ta[row][col] != cell->ta || scr->tc[row][col] != cell->tc)
        return false;
    /* The terminal paints its soft cursor into this same canvas. Leave the
     * selected cell intact until the next ordinary terminal redraw. */
    if (t->soft_cursor && t->old && t->old->cv && !t->old->cu
        && t->old->cy == row && t->old->cx >= col
        && t->old->cx < col + (use_bigtile ? 2 : 1))
        return false;
    /* Liquids remain visible around transparent actors/items. A fixture is
     * hidden beneath an occupant and needs no idle work there. */
    if (!cell->liquid_feat
        && ((cell->a & TILE_INDEX_MASK) != (cell->ta & TILE_INDEX_MASK)
            || ((byte)cell->c & TILE_INDEX_MASK) != ((byte)cell->tc & TILE_INDEX_MASK)))
        return false;
    /* The map may be clipped by the responsive pane layout. */
    return sdl_map_grid_cell_rect(cell->y, cell->x, &window_rect);
}

int sdl_idle_animation_timeout_ms(Uint64 now_ns)
{
    Uint64 next_ns = 0;
    if (!animation_context_active())
        return -1;
    for (int i = 0; i < cell_count; i++)
        if (cell_can_animate(&cells[i]))
        {
            Uint64 next = fixture_next_tick(frame_tick,
                cells[i].frame_steps, cells[i].phase_steps) * IDLE_STEP_NS;
            if (now_ns >= next)
                return 0;
            if (!next_ns || next < next_ns)
                next_ns = next;
        }
    return next_ns ? (int)((next_ns - now_ns + 999999ULL) / 1000000ULL) : -1;
}

void sdl_idle_animation_update(Uint64 now_ns)
{
    sdl_view* view = &g_views[PANE_MAIN];
    SDL_Texture* target;
    SDL_Rect clip;
    bool clipped;
    Uint64 tick = now_ns / IDLE_STEP_NS;
    if (tick == frame_tick || sdl_idle_animation_timeout_ms(now_ns) != 0)
        return;
    /* Skip missed frames after suspension; never run a catch-up loop. */
    frame_tick = tick;
    target = SDL_GetRenderTarget(g_state.renderer);
    clipped = SDL_RenderClipEnabled(g_state.renderer);
    SDL_GetRenderClipRect(g_state.renderer, &clip);
    SDL_SetRenderTarget(g_state.renderer, view->canvas);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    for (int i = 0; i < cell_count; i++)
    {
        idle_cell* cell = &cells[i];
        byte frame;
        SDL_FRect dst;
        if (!cell_can_animate(cell))
            continue;
        frame = cell->liquid_feat ? (byte)((tick / 8)
            % liquid_frame_count_at(cell->y, cell->x, cell->liquid_feat))
            : fixture_frame(tick, cell->frame_steps, cell->phase_steps,
                cell->frame_count);
        if (frame == cell->drawn_frame)
            continue;
        dst = (SDL_FRect){ cell->col * view->cell_w, cell->row * view->cell_h,
            view->cell_w * (use_bigtile ? 2 : 1), view->cell_h };
        sdl_draw_map_tile_layers_at(cell->y, cell->x, cell->a, cell->c,
            cell->ta, cell->tc, &dst);
        cell->drawn_frame = frame;
        g_state.need_present = true;
    }
    SDL_SetRenderTarget(g_state.renderer, target);
    SDL_SetRenderClipRect(g_state.renderer, clipped ? &clip : NULL);
}
