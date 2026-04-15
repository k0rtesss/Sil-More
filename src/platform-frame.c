#include "angband.h"

#include "platform-frame.h"
#include "sdl-main-internal.h"

static const char* const g_platform_view_names[ANGBAND_TERM_MAX] = {
    VERSION_NAME, "Inventory", "Equipment", "Combat Rolls",
    "Recall", "Character", "Messages", "Monster List"
};

static bool platform_frame_session_input_pending(void)
{
    app_session* session = app_session_current();

    if (!session)
        return false;
    if (!app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT))
        return false;

    return app_session_pending_input_count(session) > 0;
}

static void platform_frame_flush_pending_inputs(Uint64 now_ns)
{
    sdl_gamepad_flush_pending_dpad(now_ns, false);
    sdl_gamepad_flush_pending_left_stick(now_ns, false);
    sdl_gamepad_flush_pending_shoulder(now_ns, false);
    sdl_touch_pane_flush_pending_press(now_ns);
    sdl_drain_legacy_input_queue();
}

bool platform_frame_view_ready(int view_index)
{
    return view_index >= 0 && view_index < MAX_TERM_DATA
        && g_views[view_index].term_ready;
}

const char* platform_frame_view_name(int view_index)
{
    if (view_index < 0 || view_index >= ANGBAND_TERM_MAX)
        return "View";

    return g_platform_view_names[view_index];
}

int platform_frame_active_view_index(void)
{
    return sdl_active_view_index();
}

void platform_frame_set_active_view(int view_index)
{
    if (!platform_frame_view_ready(view_index))
        return;

    sdl_set_active_view_index(view_index);
}

bool platform_frame_main_view_ready(void)
{
    return platform_frame_view_ready(0);
}

int platform_frame_main_grid_cols(void)
{
    if (platform_frame_main_view_ready() && g_views[0].cols > 0)
        return g_views[0].cols;

    return 80;
}

int platform_frame_main_grid_rows(void)
{
    if (platform_frame_main_view_ready() && g_views[0].rows > 0)
        return g_views[0].rows;

    return 24;
}

int platform_frame_active_grid_cols(void)
{
    int view_index = platform_frame_active_view_index();

    if (platform_frame_view_ready(view_index) && g_views[view_index].cols > 0)
        return g_views[view_index].cols;

    return platform_frame_main_grid_cols();
}

int platform_frame_active_grid_rows(void)
{
    int view_index = platform_frame_active_view_index();

    if (platform_frame_view_ready(view_index) && g_views[view_index].rows > 0)
        return g_views[view_index].rows;

    return platform_frame_main_grid_rows();
}

bool platform_frame_active_view_is_main(void)
{
    return platform_frame_active_view_index() == 0;
}

void platform_frame_shutdown_views(void)
{
    for (int i = MAX_TERM_DATA - 1; i >= 0; --i)
    {
        if (!g_views[i].term_ready)
            continue;

        term_nuke(&g_views[i].t);
        g_views[i].term_ready = false;
        sdl_view_destroy(&g_views[i]);
    }
}

void platform_frame_present(void)
{
    sdl_present_if_needed(NULL);
}

static bool platform_frame_render_ui_scene_to_canvas(sdl_view* view,
    const app_ui_scene* scene)
{
    const sdl_view* main_view;
    int canvas_w;
    int canvas_h;

    if (!scene || !view)
        return false;
    if (!view || !view->canvas || view->cols <= 0 || view->rows <= 0
        || view->cell_w <= 0 || view->cell_h <= 0)
    {
        return false;
    }

    main_view = (g_views[0].term_ready && g_views[0].canvas)
        ? &g_views[0]
        : view;
    canvas_w = view->cols * view->cell_w;
    canvas_h = view->rows * view->cell_h;
    if (!sdl_scene_ui_render(view->canvas, main_view, canvas_w, canvas_h,
            scene))
        return false;

    g_state.need_present = true;
    return true;
}

bool platform_frame_render_ui_scene_to_view(int view_index,
    const app_ui_scene* scene)
{
    if (view_index < 0 || view_index >= MAX_TERM_DATA)
        return false;

    return platform_frame_render_ui_scene_to_canvas(&g_views[view_index], scene);
}

bool platform_frame_render_ui_scene_to_active_view(const app_ui_scene* scene)
{
    int view_index;

    if (!scene)
        return false;

    view_index = platform_frame_active_view_index();
    if (!platform_frame_view_ready(view_index))
        return false;

    return platform_frame_render_ui_scene_to_canvas(&g_views[view_index], scene);
}

void platform_frame_process_events(bool wait)
{
    SDL_Event ev;

    if (wait)
    {
        sdl_music_update();
        if (platform_frame_session_input_pending())
        {
            while (SDL_PollEvent(&ev))
                sdl_handle_event(&g_state, &ev);
        }
        else
        {
            Uint64 now_ns = SDL_GetTicksNS();
            int timeout_ms = sdl_gamepad_pending_timeout_ms(now_ns);
            int touch_timeout_ms = sdl_touch_pane_pending_timeout_ms(now_ns);
            int scene_timeout_ms = sdl_scene_stack_pending_timeout_ms(now_ns);

            if (timeout_ms < 0
                || (touch_timeout_ms >= 0 && touch_timeout_ms < timeout_ms))
            {
                timeout_ms = touch_timeout_ms;
            }
            if (timeout_ms < 0
                || (scene_timeout_ms >= 0 && scene_timeout_ms < timeout_ms))
            {
                timeout_ms = scene_timeout_ms;
            }

            if (timeout_ms >= 0)
            {
                if (SDL_WaitEventTimeout(&ev, timeout_ms))
                    sdl_handle_event(&g_state, &ev);
            }
            else if (SDL_WaitEvent(&ev))
            {
                sdl_handle_event(&g_state, &ev);
            }
        }

        {
            Uint64 flush_ns = SDL_GetTicksNS();
            int scene_timeout_ms = sdl_scene_stack_pending_timeout_ms(flush_ns);

            platform_frame_flush_pending_inputs(flush_ns);
            sdl_music_update();
            if (scene_timeout_ms == 0)
                g_state.need_present = true;
        }
    }
    else
    {
        bool handled = false;
        Uint64 flush_ns;
        int scene_timeout_ms;

        sdl_music_update();
        while (SDL_PollEvent(&ev))
        {
            handled = true;
            sdl_handle_event(&g_state, &ev);
        }

        flush_ns = SDL_GetTicksNS();
        scene_timeout_ms = sdl_scene_stack_pending_timeout_ms(flush_ns);
        platform_frame_flush_pending_inputs(flush_ns);

        if (!handled && !platform_frame_session_input_pending()
            && scene_timeout_ms != 0)
        {
            SDL_Delay(1);
        }

        if (scene_timeout_ms == 0)
            g_state.need_present = true;
    }

    platform_frame_present();
}

void platform_frame_flush_events(void)
{
    SDL_Event ev;

    while (SDL_PollEvent(&ev))
        sdl_handle_event(&g_state, &ev);

    sdl_touch_pane_flush_pending_press(SDL_GetTicksNS());
    sdl_clear_legacy_input_queue();
    platform_frame_present();
}

void platform_frame_delay_ms(u32b msec)
{
    Uint32 remaining = (Uint32)msec;
    const Uint32 chunk = 20;

    while (remaining > 0)
    {
        Uint32 this_delay = (remaining < chunk) ? remaining : chunk;

        SDL_Delay(this_delay);
        remaining -= this_delay;
        platform_frame_process_events(false);
    }
}

void platform_frame_react(void)
{
    log_debug("platform_frame_react (tiles_mode=%d use_graphics=%d arg_graphics=%d)",
        g_state.use_tiles, use_graphics, runtime_cli_graphics_mode());
    sdl_sync_palette();
    reset_visuals(true);
}

void platform_frame_set_active(bool active)
{
    (void)active;
}

void platform_frame_notify_noise(void)
{
}
