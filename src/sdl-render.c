#include "angband.h"
#include "sdl-main-internal.h"

sdl_state g_state;
sdl_view g_views[MAX_TERM_DATA];

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

static void draw_cursor(int x, int y, bool big);
static errr callback_sdl_curs(int x, int y);
static errr callback_sdl_bigcurs(int x, int y);
static errr callback_sdl_wipe(int x, int y, int n);
static errr callback_sdl_text(int x, int y, int n, byte a, cptr s);
static errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
    const byte* tap, const char* tcp);
static void callback_sdl_nuke(term* t);
static void callback_sdl_init(term* t);
static errr callback_sdl_xtra(int n, int v);
static SDL_Texture* sdl_load_ttf_font(const char* font_path, int font_size, int* actual_font_size);

static bool sdl_legacy_input_pending(void)
{
    app_session* session = app_session_current();

    if (!session)
        return false;

    if (!app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT))
        return false;

    return app_session_pending_input_count(session) > 0;
}

sdl_view* sdl_view_from_term(term* t)
{
    if (!t)
        return NULL;

    size_t idx = (size_t)(uintptr_t)t->data;
    if (idx >= MAX_TERM_DATA) {
        log_warn("sdl_view_from_term: invalid term index %zu (max %d)", idx, MAX_TERM_DATA - 1);
        return NULL;
    }

    return &g_views[idx];
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
    if (d->canvas) {
        SDL_DestroyTexture(d->canvas);
        d->canvas = NULL;
    }
    if (d->font_atlas) {
        SDL_DestroyTexture(d->font_atlas);
        d->font_atlas = NULL;
    }
}

void sdl_present_if_needed(sdl_view* d)
{
    int active_views = 0;

    if (!g_state.need_present)
        return;

    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    for (int i = 0; i < MAX_TERM_DATA; i++) {
        if (g_views[i].canvas)
            active_views++;
    }

    for (int i = 0; i < MAX_TERM_DATA; i++) {
        sdl_view* view = &g_views[i];
        float dst_w;
        float dst_h;

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
        if (!view->term_ready)
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
            SDL_SetRenderTarget(g_state.renderer, view->canvas);
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_state.renderer);
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

    g_state.need_present = true;
    Term_redraw();
}

static errr callback_sdl_xtra(int n, int v)
{
    sdl_view* d = sdl_view_from_term(Term);

    switch (n) {
    case TERM_XTRA_EVENT: {
        SDL_Event ev;
        if (v) {
            bool legacy_pending = sdl_legacy_input_pending();

            sdl_music_update();
            if (legacy_pending) {
                while (SDL_PollEvent(&ev))
                    sdl_handle_event(&g_state, &ev);
            } else {
                Uint64 now_ns = SDL_GetTicksNS();
                int timeout_ms = sdl_gamepad_pending_timeout_ms(now_ns);
                int touch_timeout_ms = sdl_touch_pane_pending_timeout_ms(now_ns);
                if (timeout_ms < 0 || (touch_timeout_ms >= 0 && touch_timeout_ms < timeout_ms))
                    timeout_ms = touch_timeout_ms;
                if (timeout_ms >= 0) {
                    if (SDL_WaitEventTimeout(&ev, timeout_ms))
                        sdl_handle_event(&g_state, &ev);
                } else {
                    if (SDL_WaitEvent(&ev))
                        sdl_handle_event(&g_state, &ev);
                }
            }
            Uint64 flush_ns = SDL_GetTicksNS();
            sdl_gamepad_flush_pending_dpad(flush_ns, false);
            sdl_gamepad_flush_pending_left_stick(flush_ns, false);
            sdl_gamepad_flush_pending_shoulder(flush_ns, false);
            sdl_touch_pane_flush_pending_press(flush_ns);
            sdl_drain_legacy_input_queue();
            sdl_music_update();
        } else {
            bool handled = false;
            sdl_music_update();
            while (SDL_PollEvent(&ev)) {
                handled = true;
                sdl_handle_event(&g_state, &ev);
            }
            Uint64 flush_ns = SDL_GetTicksNS();
            sdl_gamepad_flush_pending_dpad(flush_ns, false);
            sdl_gamepad_flush_pending_left_stick(flush_ns, false);
            sdl_gamepad_flush_pending_shoulder(flush_ns, false);
            sdl_touch_pane_flush_pending_press(flush_ns);
            sdl_drain_legacy_input_queue();

            if (!handled && !sdl_legacy_input_pending())
                SDL_Delay(1);
        }
        sdl_present_if_needed(d);
        return 0;
    }

    case TERM_XTRA_FLUSH: {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            sdl_handle_event(&g_state, &ev);
        sdl_touch_pane_flush_pending_press(SDL_GetTicksNS());
        sdl_clear_legacy_input_queue();
        sdl_present_if_needed(d);
        return 0;
    }

    case TERM_XTRA_CLEAR:
        if (!d || !d->canvas)
            return 0;
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        g_state.need_present = true;
        return 0;

    case TERM_XTRA_FRESH:
        sdl_present_if_needed(d);
        return 0;

    case TERM_XTRA_DELAY: {
        Uint32 total_delay = (Uint32)v;
        Uint32 chunk = 20;

        while (total_delay > 0) {
            Uint32 this_delay = (total_delay < chunk) ? total_delay : chunk;
            SDL_Delay(this_delay);
            total_delay -= this_delay;

            sdl_music_update();

            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                sdl_handle_event(&g_state, &ev);
            sdl_touch_pane_flush_pending_press(SDL_GetTicksNS());
            sdl_drain_legacy_input_queue();
        }
        return 0;
    }

    case TERM_XTRA_REACT:
        log_debug("TERM_XTRA_REACT received (tiles_mode=%d use_graphics=%d arg_graphics=%d)",
            g_state.use_tiles, use_graphics, runtime_cli_graphics_mode());
        sdl_sync_palette();
        reset_visuals(true);
        return 0;

    default:
        return 0;
    }
}

static void draw_cursor(int x, int y, bool big)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas || !Term)
        return;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return;

    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 255, 255, 255);
    SDL_RenderRect(g_state.renderer, &r);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
}

static errr callback_sdl_curs(int x, int y)
{
    draw_cursor(x, y, false);
    return 0;
}

static errr callback_sdl_bigcurs(int x, int y)
{
    draw_cursor(x, y, true);
    return 0;
}

static errr callback_sdl_wipe(int x, int y, int n)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas || !Term || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
        return 0;

    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(g_state.renderer, &r);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
    return 0;
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

static errr callback_sdl_text(int x, int y, int n, byte a, cptr s)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas || !Term || !s || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
        return 0;

    SDL_SetRenderTarget(g_state.renderer, d->canvas);

    TTF_Font* story_font = sdl_story_font_for_view(d);
    bool chunk_story_font = (Term && Term->story_chunk_active && story_font);

    if (!chunk_story_font && Term && Term->scr && story_font) {
        if (y >= 0 && y < Term->hgt && Term->scr->story && Term->scr->story[y]) {
            for (int i = 0; i < n && (x + i) < Term->wid; i++) {
                if (Term->scr->story[y][x + i]) {
                    chunk_story_font = true;
                    break;
                }
            }
        }
    }

    bool story_mode = (chunk_story_font && story_font);

    if (!story_mode) {
        SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
        SDL_SetRenderClipRect(g_state.renderer, &clip);
        SDL_FRect bg = {
            (float)(x * d->cell_w),
            (float)(y * d->cell_h),
            (float)(n * d->cell_w),
            (float)(d->cell_h)
        };
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &bg);
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    } else {
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    SDL_Color col = {
        angband_color_table[a][1],
        angband_color_table[a][2],
        angband_color_table[a][3],
        255
    };

    byte* story_row = NULL;
    char* row_chars = NULL;
    byte* row_attr = NULL;
    if (Term && Term->scr && y >= 0 && y < Term->hgt) {
        if (Term->scr->story)
            story_row = Term->scr->story[y];
        if (Term->scr->c)
            row_chars = Term->scr->c[y];
        if (Term->scr->a)
            row_attr = Term->scr->a[y];
    }

    if (story_mode) {
        if (story_row) {
            if (row_chars && row_attr) {
                sdl_render_story_row_packed(d, story_font, y, story_row, row_chars, row_attr);
                g_state.need_present = true;
                return 0;
            }

            int offset = 0;
            while (offset < n && (x + offset) < Term->wid) {
                int term_col = x + offset;
                byte flags = story_row[term_col];
                bool use_story = (flags & STORY_FLAG_USE) != 0;
                bool grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;

                int chunk_remaining = n - offset;
                int chunk_run = 1;
                while ((chunk_run < chunk_remaining) && (term_col + chunk_run) < Term->wid) {
                    byte next_flags = story_row[term_col + chunk_run];
                    bool next_story = (next_flags & STORY_FLAG_USE) != 0;
                    bool next_grid = (next_flags & STORY_FLAG_CELL_ALIGN) != 0;
                    if (next_story != use_story || next_grid != grid_align)
                        break;
                    if (row_attr && row_attr[term_col + chunk_run] != a)
                        break;
                    chunk_run++;
                }

                bool can_extend_story = use_story && row_chars;
                int render_col = term_col;
                int render_end = term_col + chunk_run;

                if (can_extend_story) {
                    while (render_col > 0) {
                        byte prev_flags = story_row[render_col - 1];
                        bool prev_story = (prev_flags & STORY_FLAG_USE) != 0;
                        bool prev_grid = (prev_flags & STORY_FLAG_CELL_ALIGN) != 0;
                        if (!prev_story || prev_grid != grid_align)
                            break;
                        if (row_attr && row_attr[render_col - 1] != a)
                            break;
                        render_col--;
                    }
                    while (render_end < Term->wid) {
                        byte next_flags = story_row[render_end];
                        bool next_story = (next_flags & STORY_FLAG_USE) != 0;
                        bool next_grid = (next_flags & STORY_FLAG_CELL_ALIGN) != 0;
                        if (!next_story || next_grid != grid_align)
                            break;
                        if (row_attr && row_attr[render_end] != a)
                            break;
                        render_end++;
                    }
                }

                int render_run = render_end - render_col;
                const char* render_text = (can_extend_story && row_chars) ? (row_chars + render_col) : (s + offset);

                SDL_FRect clear_rect = {
                    (float)(render_col * d->cell_w),
                    (float)(y * d->cell_h),
                    (float)(render_run * d->cell_w),
                    (float)d->cell_h
                };
                SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(g_state.renderer, &clear_rect);

                if (use_story) {
                    if (grid_align)
                        sdl_render_story_text_grid(d, story_font, render_col, y, render_run, render_text, col);
                    else
                        sdl_render_story_text_free(d, story_font, render_col, y, render_run, render_text, col);
                } else {
                    sdl_render_mono_text(d, render_col, y, render_run, render_text, col);
                }

                offset += chunk_run;
            }
        } else {
            SDL_FRect clear_rect = {
                (float)(x * d->cell_w),
                (float)(y * d->cell_h),
                (float)(n * d->cell_w),
                (float)d->cell_h
            };
            SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(g_state.renderer, &clear_rect);
            sdl_render_story_text_free(d, story_font, x, y, n, s, col);
        }
    } else {
        sdl_render_mono_text(d, x, y, n, s, col);
    }

    g_state.need_present = true;
    return 0;
}

static errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
    const byte* tap, const char* tcp)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas || !Term || !ap || !cp || !tap || !tcp || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
        return 0;

    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_SetRenderClipRect(g_state.renderer, &(SDL_Rect){
        x * d->cell_w,
        y * d->cell_h,
        n * d->cell_w * (use_bigtile + 1),
        d->cell_h,
    });

    SDL_FRect src = { .w = TILE_SIZE, .h = TILE_SIZE };
    SDL_FRect dst = {
        x * d->cell_w,
        y * d->cell_h,
        d->cell_w * (use_bigtile + 1),
        d->cell_h,
    };

    for (int i = 0; i < n; ++i, dst.x += dst.w) {
        byte a = ap[i];
        char c = cp[i];
        bool glow = a & GRAPHICS_GLOW_MASK;
        bool alert = c & GRAPHICS_ALERT_MASK;
        bool seen = tcp[i] & GRAPHICS_SEEN_MASK;
        bool sleep = tap[i] & GRAPHICS_SLEEP_MASK;

        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(g_state.renderer, &dst);

        src.x = (tcp[i] & 0x3F) * TILE_SIZE;
        src.y = (tap[i] & 0x3F) * TILE_SIZE;
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);

        if (Term == term_screen) {
            int term_x = x + (i * (use_bigtile + 1));
            if (y >= ROW_MAP && term_x >= COL_MAP) {
                int map_y = y - ROW_MAP;
                int map_x = term_x - COL_MAP;
                if (use_bigtile)
                    map_x /= 2;

                int dy = p_ptr->wy + map_y;
                int dx = p_ptr->wx + map_x;

                if ((dy >= 0) && (dx >= 0) && (dy < p_ptr->cur_map_hgt)
                    && (dx < p_ptr->cur_map_wid)) {
                    u16b info = cave_info[dy][dx];
                    bool hide_square = (!p_ptr->is_dead)
                        && (p_ptr->rage || g_labyrinth_view_active)
                        && !(info & (CAVE_SEEN));

                    if (!hide_square) {
                        s16b m_idx = cave_m_idx[dy][dx];
                        bool creature_visible = (m_idx < 0)
                            || ((m_idx > 0) && mon_list[m_idx].ml);

                        if (creature_visible && (info & (CAVE_MARK))) {
                            byte feat = cave_feat[dy][dx];
                            feat = f_info[feat].mimic;
                            if (((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
                                || ((feat >= FEAT_STAIR_HEAD) && (feat <= FEAT_STAIR_TAIL))
                                || ((feat >= FEAT_FORGE_HEAD) && (feat <= FEAT_FORGE_TAIL))
                                || (feat == FEAT_SUNLIGHT)) {
                                feature_type* f_ptr = &f_info[feat];
                                byte feat_a = f_ptr->x_attr;
                                char feat_c = f_ptr->x_char;

                                if ((use_graphics == GRAPHICS_MICROCHASM)
                                    && feat_supports_lighting(feat)) {
                                    bool is_dark = p_ptr->blind
                                        || ((cave_light[dy][dx] <= 0) && !(info & (CAVE_GLOW)));
                                    if (is_dark || !(info & (CAVE_SEEN)))
                                        feat_c += 1;
                                }

                                src.x = ((byte)feat_c & 0x3F) * TILE_SIZE;
                                src.y = (feat_a & 0x3F) * TILE_SIZE;
                                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
                            }
                        }
                    }
                }
            }
        }

        if (glow) {
            byte icon_a = misc_to_attr[ICON_GLOW];
            byte icon_c = (byte)misc_to_char[ICON_GLOW];
            if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG)) {
                src.x = (icon_c & 0x7F) * TILE_SIZE;
                src.y = (icon_a & 0x7F) * TILE_SIZE;
                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
            }
        }

        src.x = (c & 0x3F) * TILE_SIZE;
        src.y = (a & 0x3F) * TILE_SIZE;
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);

        if (sleep) {
            byte icon_a = misc_to_attr[ICON_SLEEPING];
            byte icon_c = (byte)misc_to_char[ICON_SLEEPING];
            if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG)) {
                src.x = (icon_c & 0x7F) * TILE_SIZE;
                src.y = (icon_a & 0x7F) * TILE_SIZE;
                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
            }
        }

        if (seen) {
            byte icon_a = misc_to_attr[ICON_MONSTER_SEES_PLAYER];
            byte icon_c = (byte)misc_to_char[ICON_MONSTER_SEES_PLAYER];
            if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG)) {
                src.x = (icon_c & 0x7F) * TILE_SIZE;
                src.y = (icon_a & 0x7F) * TILE_SIZE;
                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
            }
        }

        if (alert) {
            byte icon_a = misc_to_attr[ICON_ALERT];
            byte icon_c = (byte)misc_to_char[ICON_ALERT];
            if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG)) {
                src.x = (icon_c & 0x7F) * TILE_SIZE;
                src.y = (icon_a & 0x7F) * TILE_SIZE;
                SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, &dst);
            }
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
    return 0;
}

static void callback_sdl_nuke(term* t)
{
    sdl_view* d = sdl_view_from_term(t ? t : Term);
    if (!d)
        return;

    if (d->font_atlas)
        SDL_DestroyTexture(d->font_atlas);
    d->font_atlas = NULL;
    if (d->canvas)
        SDL_DestroyTexture(d->canvas);
    d->canvas = NULL;
}

static void callback_sdl_init(term* t)
{
    (void)t;
}

errr sdl_view_link_term(sdl_view* d, int term_index)
{
    term* t = &d->t;
    if (d->term_ready) {
        term* old = Term;
        Term_activate(t);
        Term_resize(d->cols, d->rows);
        Term_redraw();
        Term_activate(old);
        return 0;
    }
    term_init(t, d->cols, d->rows, 256);
    t->soft_cursor = true;
    t->higher_pict = g_state.use_tiles;
    t->never_frosh = true;
    t->init_hook = callback_sdl_init;
    t->nuke_hook = callback_sdl_nuke;
    t->xtra_hook = callback_sdl_xtra;
    t->curs_hook = callback_sdl_curs;
    t->bigcurs_hook = callback_sdl_bigcurs;
    t->wipe_hook = callback_sdl_wipe;
    t->text_hook = callback_sdl_text;
    if (g_state.use_tiles)
        t->pict_hook = callback_sdl_pict;
    t->data = (void*)(uintptr_t)term_index;
    angband_term[term_index] = t;
    d->term_ready = true;
    return 0;
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
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_state.renderer);
        g_state.need_present = true;
    } else {
        log_error("Create canvas failed: %s", SDL_GetError());
        quit("could not create canvas");
    }
}
