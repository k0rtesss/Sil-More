#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-fixtures.h"

/* Fixtures share one 64x32 atlas; each animated liquid shares one 64x16 atlas.
 * Solid ice uses one static 16x16 tile, cached here for map invalidation.
 * No I/O, texture creation, timers, threads, or heap allocation occurs in
 * the animation update. */
/* A shared 25 Hz ceiling coalesces independent fixture deadlines. Each flame
 * advances only every 4-6 steps (160-240 ms), without a timer per fixture. */
#define IDLE_STEP_NS 40000000ULL
#define IDLE_FRAME_COUNT 4

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

static byte visible_liquid(int y, int x)
{
    u16b info;
    if (!p_ptr || !in_bounds(y, x)
        || (cave_feat[y][x] != FEAT_WATER && cave_feat[y][x] != FEAT_LAVA
            && cave_feat[y][x] != FEAT_ICE && cave_feat[y][x] != FEAT_POISON))
        return 0;
    info = cave_info[y][x];
    if (!(info & (CAVE_MARK | CAVE_SEEN))
        || ((p_ptr->rage || g_labyrinth_view_active) && !(info & CAVE_SEEN)))
        return 0;
    return cave_feat[y][x];
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
        /* Exactly the lava pixels and frames, with warm channels turned green.
         * RGBA32 guarantees byte order; retain alpha and the source geometry.
         * Recolor once when loading, never in the idle animation update. */
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
                pixel[1] = red;
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
    byte feat = cave_feat[y][x];
    bool live = !p_ptr->blind && (cave_info[y][x] & CAVE_SEEN);
    if (!load_liquid_texture(feat))
    {
        /* Keep lethal terrain recognizable even if an asset is missing. */
        if (feat == FEAT_LAVA)
            SDL_SetRenderDrawColor(g_state.renderer, live ? 240 : 90,
                live ? 74 : 28, live ? 16 : 6, 255);
        else if (feat == FEAT_POISON)
            SDL_SetRenderDrawColor(g_state.renderer, live ? 37 : 14,
                live ? 240 : 90, live ? 16 : 6, 255);
        else if (feat == FEAT_ICE)
            SDL_SetRenderDrawColor(g_state.renderer, live ? 156 : 58,
                live ? 216 : 81, live ? 232 : 87, 255);
        else
            SDL_SetRenderDrawColor(g_state.renderer, 24, 78, 108, 255);
        SDL_RenderFillRect(g_state.renderer, dst);
        return true;
    }
    SDL_Texture* texture = feat == FEAT_ICE ? ice_texture
        : feat == FEAT_POISON ? poison_texture
        : feat == FEAT_LAVA ? lava_texture : water_texture;
    int frame = live && feat != FEAT_ICE ? (int)((frame_tick / 8) % 4) : 0;
    SDL_FRect src = { frame * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE };
    SDL_SetTextureColorMod(texture, live ? 255 : 96,
        live ? 255 : 96, live ? 255 : 96);
    SDL_RenderTexture(g_state.renderer, texture, &src, dst);
    return true;
}

static void fixture_timing(int y, int x, byte* steps, byte* phase)
{
    /* Stable across viewport changes, with no gameplay RNG or saved state. */
    Uint32 hash = (Uint32)x * 0x9e3779b9u ^ (Uint32)y * 0x85ebca6bu;
    hash ^= (Uint32)(p_ptr ? p_ptr->depth : 0) * 0xc2b2ae35u;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    *steps = (byte)(4 + hash % 3);
    *phase = (byte)((hash >> 8) % (*steps * IDLE_FRAME_COUNT));
}

static byte fixture_frame_count(byte kind)
{
    return kind == CAVE_FIXTURE_WALL_TORCH ? 4 : 3;
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
    const char* names[] = { "torch", "brazier" };

    if (fixture_load_attempted)
        return fixture_texture != NULL;
    fixture_load_attempted = true;
    atlas = SDL_CreateSurface(TILE_SIZE * IDLE_FRAME_COUNT, TILE_SIZE * 2,
        SDL_PIXELFORMAT_RGBA32);
    if (!atlas)
        return false;
    SDL_ClearSurface(atlas, 0, 0, 0, 0);
    for (int kind = 0; kind < 2; kind++)
        for (int frame = 0; frame < fixture_frame_count(kind + 1); frame++)
        {
            char path[128];
            SDL_Surface* source;
            SDL_Rect dst = { frame * TILE_SIZE, kind * TILE_SIZE,
                TILE_SIZE, TILE_SIZE };
            strnfmt(path, sizeof(path), "lib/xtra/graf/anim_%s_f%d.png",
                names[kind], frame);
            source = IMG_Load(path);
            if (!source || source->w != TILE_SIZE || source->h != TILE_SIZE)
            {
                log_warn("Idle fixture animation unavailable: %s (%s)",
                    path, SDL_GetError());
                SDL_DestroySurface(source);
                SDL_DestroySurface(atlas);
                return false;
            }
            SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(source, NULL, atlas, &dst);
            SDL_DestroySurface(source);
        }
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
        return draw_liquid(y, x, dst);
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
    fixture_timing(y, x, &steps, &phase);
    /* All frozen fixtures use the second frame, without a phase offset. */
    frame = animate
        ? fixture_frame(frame_tick, steps, phase, fixture_frame_count(kind)) : 1;
    src = (SDL_FRect){ frame * TILE_SIZE, (kind - 1) * TILE_SIZE,
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
            ? (byte)((frame_tick / 8) % 4) : 0;
        return;
    }
    fixture_timing(y, x, &cell->frame_steps, &cell->phase_steps);
    cell->frame_count = fixture_frame_count(visible_fixture(y, x));
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
    /* Track ice for pan/erase invalidation, but its solid surface never moves. */
    if (cell->liquid_feat == FEAT_ICE)
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
        frame = cell->liquid_feat ? (byte)((tick / 8) % 4)
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
