#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-fixtures.h"

/* Six source images become one 48x32 GPU texture. No I/O, texture creation,
 * timers, threads, or heap allocation occurs in the animation update. */
/* A shared 25 Hz ceiling coalesces independent fixture deadlines. Each flame
 * advances only every 4-6 steps (160-240 ms), without a timer per fixture. */
#define IDLE_STEP_NS 40000000ULL
#define IDLE_FRAME_COUNT 3

typedef struct idle_cell {
    int col, row, y, x;
    int width;
    byte a, ta;
    char c, tc;
    byte frame_steps, phase_steps, drawn_frame;
} idle_cell;

static SDL_Texture* fixture_texture;
static bool fixture_load_attempted;
static idle_cell* cells;
static int cell_count, cell_capacity;
static Uint64 frame_tick;

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

static byte fixture_frame(Uint64 tick, byte steps, byte phase)
{
    return (byte)(((tick + phase) / steps) % IDLE_FRAME_COUNT);
}

static Uint64 fixture_next_tick(Uint64 tick, byte steps, byte phase)
{
    return tick + steps - (tick + phase) % steps;
}

static bool load_fixture_texture(void)
{
    SDL_Surface* atlas;
    const char* names[] = { "wall_torch", "brazier" };

    if (fixture_load_attempted)
        return fixture_texture != NULL;
    fixture_load_attempted = true;
    atlas = SDL_CreateSurface(TILE_SIZE * IDLE_FRAME_COUNT, TILE_SIZE * 2,
        SDL_PIXELFORMAT_RGBA32);
    if (!atlas)
        return false;
    SDL_ClearSurface(atlas, 0, 0, 0, 0);
    for (int kind = 0; kind < 2; kind++)
        for (int frame = 0; frame < IDLE_FRAME_COUNT; frame++)
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
    /* All frozen fixtures use frame 2 of 3, without a coordinate phase offset. */
    frame = animate ? fixture_frame(frame_tick, steps, phase) : 1;
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
    if (!visible_fixture(y, x) || !(ta & TILE_FLAG) || !((byte)tc & TILE_FLAG))
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
    fixture_timing(y, x, &cell->frame_steps, &cell->phase_steps);
    cell->drawn_frame = (!p_ptr->blind
        && (op_ptr->opt[OPT_torch_animation_always] || (cave_info[y][x] & CAVE_SEEN)))
        ? fixture_frame(frame_tick, cell->frame_steps, cell->phase_steps) : 1;
}

static bool animation_context_active(void)
{
    SDL_WindowFlags flags;
    if (!fixture_texture || !cell_count || !g_state.window
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
    term* t = term_screen;
    term_win* scr = t ? t->scr : NULL;
    SDL_FRect window_rect;
    int col = cell->col, row = cell->row;
    if (!scr || col < 0 || row < 0 || col >= t->wid || row >= t->hgt
        || cell->width != (use_bigtile ? 2 : 1)
        || cell->y != p_ptr->wy + row - ROW_MAP
        || cell->x != p_ptr->wx + (col - COL_MAP) / (use_bigtile ? 2 : 1)
        || !visible_fixture(cell->y, cell->x))
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
    /* A floor item/creature completely covers its brazier; no idle work. */
    if ((cell->a & TILE_INDEX_MASK) != (cell->ta & TILE_INDEX_MASK)
        || ((byte)cell->c & TILE_INDEX_MASK) != ((byte)cell->tc & TILE_INDEX_MASK))
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
        frame = fixture_frame(tick, cell->frame_steps, cell->phase_steps);
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
