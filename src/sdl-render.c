#include "angband.h"
#include "sdl-main-internal.h"
#include "ui/ui-status.h"

sdl_state g_state;
sdl_view g_views[MAX_TERM_DATA];
static int g_active_view_index = 0;

static void sdl_apply_mono_font_settings(TTF_Font* font)
{
    int style = TTF_STYLE_NORMAL;

    if (config.mono_bold)
        style |= TTF_STYLE_BOLD;
    if (config.mono_italic)
        style |= TTF_STYLE_ITALIC;
    if (config.mono_underline)
        style |= TTF_STYLE_UNDERLINE;
    if (config.mono_strikethrough)
        style |= TTF_STYLE_STRIKETHROUGH;
    if (style != TTF_STYLE_NORMAL)
        TTF_SetFontStyle(font, style);

    TTF_SetFontHinting(font, config.mono_hinting);
    TTF_SetFontKerning(font, config.mono_kerning);
    if (config.mono_outline > 0)
        TTF_SetFontOutline(font, config.mono_outline);
}

static SDL_Texture* sdl_load_ttf_font(const char* font_path, int font_size, int* actual_font_size);
static void sdl_view_clear_canvas(sdl_view* view);
static void sdl_clear_supporting_view_canvases(void);
static u32b sdl_visible_window_mask(void);

int sdl_active_view_index(void)
{
    return g_active_view_index;
}

void sdl_set_active_view_index(int view_index)
{
    if (view_index < 0 || view_index >= MAX_TERM_DATA)
        return;
    if (!g_views[view_index].ready)
        return;

    g_active_view_index = view_index;
}

static void sdl_view_clear_canvas(sdl_view* view)
{
    if (!view || !view->canvas)
        return;

    SDL_SetRenderTarget(g_state.renderer, view->canvas);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);
    g_state.need_present = true;
}

void sdl_sync_palette(void)
{
    for (int i = 0; i < 16; i++) {
        g_state.palette[i].r = angband_color_table[i][1];
        g_state.palette[i].g = angband_color_table[i][2];
        g_state.palette[i].b = angband_color_table[i][3];
        g_state.palette[i].a = 255;
    }
}

void sdl_view_destroy(sdl_view* d)
{
    if (!d)
        return;

    if (d->canvas) {
        SDL_DestroyTexture(d->canvas);
        d->canvas = NULL;
    }
    if (d->font_atlas) {
        SDL_DestroyTexture(d->font_atlas);
        d->font_atlas = NULL;
    }

    d->ready = false;
}

void sdl_present_if_needed(sdl_view* d)
{
    int active_views = 0;
    Uint64 now_ns = SDL_GetTicksNS();
    bool scene_frame_due;
    bool handled_main = false;

    (void)sdl_scene_stack_prepare_frame(now_ns);
    scene_frame_due = (sdl_scene_stack_pending_timeout_ms(now_ns) == 0);

    if (!g_state.need_present && !scene_frame_due)
        return;

    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    handled_main = sdl_scene_stack_handles_main_view();
    if (handled_main)
        active_views++;

    for (int i = 0; i < MAX_TERM_DATA; i++) {
        if (handled_main && i == 0)
            continue;
        if (g_views[i].canvas)
            active_views++;
    }

    if (handled_main)
        handled_main = sdl_scene_stack_render_main_layer();

    for (int i = 0; i < MAX_TERM_DATA; i++) {
        sdl_view* view = &g_views[i];
        float dst_w;
        float dst_h;

        if (handled_main && i == 0)
            continue;
        if (!view->canvas)
            continue;
        if (view->cols <= 0 || view->rows <= 0 || view->cell_w <= 0 || view->cell_h <= 0)
            continue;

        dst_w = (float)(view->cols * view->cell_w);
        dst_h = (float)(view->rows * view->cell_h);
        if (dst_w <= 0.0f || dst_h <= 0.0f)
            continue;

        SDL_RenderTexture(g_state.renderer, view->canvas, NULL, &(SDL_FRect){
            .x = view->rect.x + view->margin_x,
            .y = view->rect.y + view->margin_y,
            .w = dst_w,
            .h = dst_h,
        });

        if (active_views > 1 && i != 0) {
            SDL_FRect frame = {
                .x = view->rect.x + view->margin_x,
                .y = view->rect.y + view->margin_y,
                .w = dst_w,
                .h = dst_h,
            };
            SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 128);
            SDL_RenderRect(g_state.renderer, &frame);
        }
    }

    sdl_scene_stack_render_overlay_layer();

    if (g_pane_rects[PANE_TOUCH].w > 0 && sdl_touch_pane_is_config_enabled())
        sdl_touch_pane_render();

    sdl_touch_pane_render_reset_prompt();
    SDL_RenderPresent(g_state.renderer);

    if (d && d->canvas)
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
    else
        SDL_SetRenderTarget(g_state.renderer, NULL);

    g_state.need_present = false;
}

void sdl_handle_renderer_reset(void)
{
    for (int i = 0; i < MAX_TERM_DATA; i++) {
        sdl_view* view = &g_views[i];
        if (!view->ready)
            continue;

        if (view->canvas) {
            SDL_DestroyTexture(view->canvas);
            view->canvas = NULL;
        }

        view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET, view->cols * view->cell_w, view->rows * view->cell_h);
        if (view->canvas) {
            SDL_SetTextureBlendMode(view->canvas, SDL_BLENDMODE_NONE);
            SDL_SetTextureScaleMode(view->canvas, SDL_SCALEMODE_NEAREST);
            sdl_view_clear_canvas(view);
        } else {
            log_error("Failed to recreate canvas for view %d: %s", i, SDL_GetError());
        }
    }

    if (g_state.use_tiles && g_state.tileset) {
        SDL_DestroyTexture(g_state.tileset);
        g_state.tileset = NULL;

        SDL_Surface* ts = IMG_Load("lib/xtra/graf/16x16.png");
        if (ts) {
            g_state.tileset = SDL_CreateTextureFromSurface(g_state.renderer, ts);
            if (g_state.tileset) {
                SDL_SetTextureScaleMode(g_state.tileset, SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
            }
            SDL_DestroySurface(ts);
        }
    }

    sdl_scene_stack_on_renderer_reset();
    g_state.need_present = true;
    sdl_redraw_all_views();
}

void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col)
{
    if (!d || !d->font_atlas || n <= 0)
        return;

    SDL_SetTextureColorMod(d->font_atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    for (int i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        SDL_FRect src = {
            (ch & 15) * d->cell_w,
            (ch >> 4) * d->cell_h,
            d->cell_w,
            d->cell_h,
        };
        SDL_FRect dst = {
            (x + i) * d->cell_w,
            y * d->cell_h,
            d->cell_w,
            d->cell_h
        };
        if (use_graphics == GRAPHICS_PSEUDO && solid_walls && (ch == '#' || ch == '%')) {
            SDL_SetRenderDrawColor(g_state.renderer, col.r, col.g, col.b, SDL_ALPHA_OPAQUE);
            SDL_RenderFillRect(g_state.renderer, &dst);
        }
        SDL_RenderTexture(g_state.renderer, d->font_atlas, &src, &dst);
    }
}

static void sdl_clear_supporting_view_canvases(void)
{
    for (int i = 1; i < MAX_TERM_DATA; i++)
    {
        if (!g_views[i].ready)
            continue;

        sdl_view_clear_canvas(&g_views[i]);
    }
}

static u32b sdl_visible_window_mask(void)
{
    u32b mask = 0L;

    if (!op_ptr)
        return 0L;

    for (int i = 0; i < MAX_TERM_DATA; i++)
    {
        if (!g_views[i].ready)
            continue;

        mask |= op_ptr->window_flag[i];
    }

    return mask;
}

void sdl_redraw_all_views(void)
{
    app_session* session = app_session_current();
    const app_snapshot* snapshot = session ? app_session_snapshot(session) : NULL;

    if (!snapshot || snapshot->scene == APP_SCENE_KIND_NONE)
    {
        for (int i = 0; i < MAX_TERM_DATA; i++)
        {
            if (!g_views[i].ready)
                continue;

            sdl_view_clear_canvas(&g_views[i]);
        }
        sdl_present_if_needed(NULL);
        return;
    }

    if (snapshot->scene == APP_SCENE_KIND_DUNGEON && p_ptr)
    {
        u32b update_mask = p_ptr->update;
        u32b redraw_mask = p_ptr->redraw;
        u32b window_mask = sdl_visible_window_mask() | p_ptr->window;

        if (p_ptr->update)
            update_stuff();
        if (p_ptr->redraw)
            redraw_stuff();
        if (window_mask)
            p_ptr->window &= ~window_mask;

        (void)app_session_build_dungeon_snapshot(session, update_mask,
            redraw_mask, window_mask);
        sdl_scene_stack_clear();

        if (window_mask)
            ui_status_refresh_window_mask(window_mask);
        else
            sdl_clear_supporting_view_canvases();

        sdl_present_if_needed(NULL);
        return;
    }

    sdl_scene_stack_clear();
    sdl_clear_supporting_view_canvases();
    sdl_present_if_needed(NULL);
}

static SDL_Texture* sdl_load_ttf_font(const char* font_path, int font_size, int* actual_font_size)
{
    int cell_height = font_size;
    int cell_width = font_size / 2;
    int min_size = font_size / 2;
    TTF_Font* font = NULL;

    for (; font_size >= min_size; font_size--) {
        int measured_w = 0;
        if (font == NULL) {
            font = TTF_OpenFont(font_path, font_size);
            if (!font) {
                log_error("TTF_OpenFont failed: %s", SDL_GetError());
                quit("could not load TTF font");
            }
            sdl_apply_mono_font_settings(font);
        }
        TTF_MeasureString(font, "M", 1, 0, &measured_w, NULL);
        if (measured_w <= cell_width)
            break;
        TTF_CloseFont(font);
        font = NULL;
    }

    if (!font) {
        log_error("could not find suitable font size");
        quit("could not find suitable font size");
    }

    SDL_Texture* font_atlas = SDL_CreateTexture(g_state.renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 16 * cell_width, 16 * cell_height);
    if (!font_atlas) {
        log_error("SDL_CreateTexture failed: %s", SDL_GetError());
        quit("could not create TTF glyph cache");
    }

    SDL_SetRenderTarget(g_state.renderer, font_atlas);
    SDL_Color white = (SDL_Color){ 255, 255, 255, 255 };
    SDL_FRect dst = { .w = cell_width, .h = cell_height };
    for (Uint32 ch = 0; ch < 256; ch++) {
        SDL_Surface* gsurf = TTF_RenderGlyph_Blended(font, ch, white);
        if (!gsurf) {
            const char* error = SDL_GetError();
            if (!SDL_strcmp(error, "Text has zero width"))
                continue;
            log_error("could not render `%c` character: %s", ch, error);
            quit("could not render TTF character");
        }
        SDL_Texture* gtex = SDL_CreateTextureFromSurface(g_state.renderer, gsurf);
        SDL_DestroySurface(gsurf);
        if (!gtex) {
            log_error("prepare_glyph: could not create texture from surface: %s", SDL_GetError());
            quit("could not create SDL texture");
        }
        SDL_SetTextureBlendMode(gtex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(gtex, SDL_SCALEMODE_LINEAR);
        dst.x = cell_width * (ch % 16);
        dst.y = cell_height * (ch >> 4);
        SDL_RenderTexture(g_state.renderer, gtex, NULL, &dst);
        SDL_DestroyTexture(gtex);
    }
    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetTextureScaleMode(font_atlas, SDL_SCALEMODE_LINEAR);
    TTF_CloseFont(font);
    if (actual_font_size)
        *actual_font_size = font_size;
    return font_atlas;
}

void sdl_window_set_position(int x, int y)
{
    if (g_state.window && x >= 0 && y >= 0) {
        SDL_SetWindowPosition(g_state.window, x, y);
        log_debug("Window position set to (%d, %d)", x, y);
    }
}

void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles)
{
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    if (fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    if (!SDL_CreateWindowAndRenderer("Sil-more SDL3", window_width, window_height,
            flags, &g_state.window, &g_state.renderer)) {
        log_error("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        quit("could not create SDL window");
    }

    if (!SDL_SetRenderVSync(g_state.renderer, 1))
        log_warn("Failed to enable V-SYNC: %s", SDL_GetError());

    g_state.system_scale = SDL_GetWindowDisplayScale(g_state.window);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    g_state.use_tiles = use_tiles;
    if (g_state.use_tiles) {
        SDL_Surface* ts = IMG_Load("lib/xtra/graf/16x16.png");
        if (!ts) {
            log_error("Failed to load tileset PNG: %s", SDL_GetError());
            quit("could not load tileset");
        }
        int tileset_width = ts->w;
        g_state.tileset = SDL_CreateTextureFromSurface(g_state.renderer, ts);
        SDL_DestroySurface(ts);
        if (!g_state.tileset) {
            log_error("Failed to create tileset texture: %s", SDL_GetError());
            quit("could not create tileset texture");
        }
        SDL_SetTextureScaleMode(g_state.tileset, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(g_state.tileset, SDL_BLENDMODE_BLEND);
        g_state.tileset_cols = tileset_width / TILE_SIZE;
    }
}

void sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin)
{
    if (!d)
        return;

    if (scale) {
#ifdef __ANDROID__
        int requested_scale = scale;
        int min_cols = sdl_current_min_terminal_cols();
        int min_rows = sdl_current_min_terminal_rows();
        int max_scale_for_min_cols = (rect.w / min_cols) * 2 / TILE_SIZE;
        int max_scale_for_min_rows = rect.h / min_rows / TILE_SIZE;
        int effective_scale = requested_scale;

        if (max_scale_for_min_cols < 1)
            max_scale_for_min_cols = 1;
        if (max_scale_for_min_rows < 1)
            max_scale_for_min_rows = 1;

        int max_scale_for_min_size = max_scale_for_min_cols;
        if (max_scale_for_min_rows < max_scale_for_min_size)
            max_scale_for_min_size = max_scale_for_min_rows;
        if (effective_scale > max_scale_for_min_size)
            effective_scale = max_scale_for_min_size;

        d->cell_w = effective_scale * TILE_SIZE / 2;
        d->cell_h = effective_scale * TILE_SIZE;
#else
        d->cell_w = scale * TILE_SIZE / 2;
        d->cell_h = scale * TILE_SIZE;
#endif
    } else if (font_size) {
        d->cell_h = g_state.system_scale * font_size;
        d->cell_w = d->cell_h / 2;
    } else {
        quit("sdl_view_create: font_size and scale cannot both be zero");
    }

    d->font_atlas = sdl_load_ttf_font(font_path, d->cell_h, NULL);
    SDL_SetTextureBlendMode(d->font_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(d->font_atlas, 255, 255, 255);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    d->rect = rect;
    d->cols = rect.w / d->cell_w;
    d->rows = rect.h / d->cell_h;
    (void)margin;
    d->margin_x = (rect.w - d->cols * d->cell_w) / 2;
    if (d->margin_x < 0)
        d->margin_x = 0;
    d->margin_y = (rect.h - d->rows * d->cell_h) / 2;
    if (d->margin_y < 0)
        d->margin_y = 0;

    d->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, d->cols * d->cell_w, d->rows * d->cell_h);
    if (d->canvas) {
        SDL_SetTextureBlendMode(d->canvas, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(d->canvas, SDL_SCALEMODE_NEAREST);
        sdl_view_clear_canvas(d);
    } else {
        log_error("Create canvas failed: %s", SDL_GetError());
        quit("could not create canvas");
    }

    d->ready = true;
}
