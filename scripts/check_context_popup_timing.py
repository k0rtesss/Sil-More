#!/usr/bin/env python3
"""Check map pointer actions and popup timing with temporary player data.

Run after build-incremental.ps1. Reuses the isolated engine/template fixture;
never opens a player save or configuration.
"""
import check_new_monsters as engine

CHECKS = r'''
#include "support/movement-input.h"
#include "tutorial/tutorial.h"
void process_player(void);

static int input_requests;
static int hint_refreshes;
static errr popup_terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_FRESH
        && sdl_question_menu_context_hint_active()) {
        assert(p_ptr->py == 10 && p_ptr->px == 12);
        hint_refreshes++;
    }
    if (action == TERM_XTRA_EVENT) {
        assert(++input_requests < 20);
        Term_keypress(';');
        Term_keypress('5');
    }
    return 0;
}

static void popup_move_fixture(bool item)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    reset_map(5);
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
            if (!y || !x || y == p_ptr->cur_map_hgt - 1
                || x == p_ptr->cur_map_wid - 1) {
                cave_feat[y][x] = FEAT_WALL_PERM;
                cave_info[y][x] |= CAVE_WALL;
            }
    p_ptr->py = 10; p_ptr->px = 11;
    cave_m_idx[10][11] = -1;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->playing = p_ptr->restoring = true;
    p_ptr->energy = 100;
    character_dungeon = true;
    tutorial_set_enabled(false);
    character_icky = 0;
    playerturn = 100;
    hjkl_movement = angband_keyset = false;
    set_sdl_show_context_square_popups(true);
    sdl_question_menu_clear();
    Term_flush();
    inkey_next_set(NULL);
    inkey_xtra = false;
    input_requests = 0;
    hint_refreshes = 0;
    test_term.xtra_hook = popup_terminal_extra;
    if (item) {
        int kind = 0;
        for (int i = 1; i < z_info->k_max; ++i)
            if (k_info[i].tval == TV_SWORD) { kind = i; break; }
        assert(kind);
        object_prep(&o_list[1], kind);
        o_list[1].iy = 10; o_list[1].ix = 12;
        o_list[1].marked = true;
        o_list[1].pickup = false;
        cave_o_idx[10][12] = 1;
        o_max = 2; o_cnt = 1;
    }
}

static void move_once(void)
{
    Term_keypress(';');
    Term_keypress('6');
    process_player();
    assert(p_ptr->py == 10 && p_ptr->px == 12);
    assert(playerturn == 101 && p_ptr->energy == 0);
    assert(!inkey_flag);
}

static void check_pickup_context_label(void)
{
    int kind = 0;
    int key = 0;
    char label[32];

    popup_move_fixture(false);
    p_ptr->px = 12;
    for (int i = 1; i < z_info->k_max; i++)
    {
        if (k_info[i].tval == TV_SWORD && k_info[i].sval == SV_DAGGER)
        {
            kind = i;
            break;
        }
    }
    assert(kind);
    object_prep(&o_list[1], kind);
    o_list[1].iy = 10;
    o_list[1].ix = 12;
    o_list[1].marked = true;
    cave_o_idx[10][12] = 1;
    o_max = 2;
    o_cnt = 1;
    assert(object_can_choose_pack_or_harness(&o_list[1]));

    assert(touch_shortcut_context_action(' ', false, &key, label,
        sizeof(label)));
    assert(key == ' ' && strcmp(label, "Pick Up") == 0);
    assert(touch_shortcut_context_action(' ', true, &key, label,
        sizeof(label)));
    assert(key == ' ' && strcmp(label, "Pick Up") == 0);
    puts("PASS: Pack/Harness chooser is labeled Pick Up in both context views.");
}

static void check_popup_timing(void)
{
    check_pickup_context_label();
    puts("Checking real movement popup ordering...");
    popup_move_fixture(true);
    move_once();
    /* No monster/world pass or second player input turn has run yet. */
    assert(sdl_question_menu_context_hint_active());
    assert(hint_refreshes > 0);
    assert(cave_o_idx[10][12] == 1);
    puts("PASS: real movement refreshes the prepared popup before monster/world processing.");

    /* Reproduce the logged case: the player frame has already consumed all
     * terminal damage, so only the native popup is dirty. */
    Term_fresh();
    assert(Term->y1 > Term->y2);
    hint_refreshes = 0;
    assert(do_cmd_context_square_action_popup());
    assert(hint_refreshes == 1);
    puts("PASS: a popup-only change refreshes even with a clean terminal buffer.");

    popup_move_fixture(false);
    move_once();
    assert(!sdl_question_menu_is_active());
    assert(hint_refreshes == 0);

    popup_move_fixture(true);
    set_sdl_show_context_square_popups(false);
    move_once();
    assert(!sdl_question_menu_is_active());

    popup_move_fixture(true);
    do_cmd_suppress_context_square_popups();
    move_once();
    assert(!sdl_question_menu_is_active());
    /* Expire suppression for the remaining independent cases. */
    playerturn = 110;
    assert(do_cmd_context_square_action_popup());
    sdl_question_menu_clear();

    popup_move_fixture(true);
    cave_feat[10][12] = FEAT_WALL_EXTRA;
    cave_info[10][12] |= CAVE_WALL | CAVE_MARK;
    Term_keypress(';');
    Term_keypress('6');
    Term_keypress(';');
    Term_keypress('5'); /* A known wall can refund the attempted step. */
    process_player();
    assert(p_ptr->py == 10 && p_ptr->px == 11);
    assert(!sdl_question_menu_is_active());

    popup_move_fixture(false);
    cave_feat[10][12] = FEAT_MORE;
    move_once();
    assert(sdl_question_menu_context_hint_active());
    assert(hint_refreshes > 0);
    puts("PASS: empty/blocked squares, disabled/suppressed hints, and stairs.");
}

static SDL_Event map_pointer_event(int type, int y, int x, int button)
{
    SDL_Event event = {0};
    SDL_FRect tile;
    float window_x, window_y;

    assert(sdl_player_map_rect(y, x, &tile));
    assert(SDL_RenderCoordinatesToWindow(g_state.renderer,
        tile.x + tile.w / 2, tile.y + tile.h / 2, &window_x, &window_y));
    event.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        event.motion.windowID = SDL_GetWindowID(g_state.window);
        event.motion.which = 1;
        event.motion.x = window_x;
        event.motion.y = window_y;
        event.motion.xrel = tile.w;
    } else {
        event.button.windowID = SDL_GetWindowID(g_state.window);
        event.button.which = 1;
        event.button.button = button;
        event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.x = window_x;
        event.button.y = window_y;
    }
    return event;
}

static bool release_in_question;

static errr dismiss_grid_question(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) {
        SDL_Event esc = {0};
        assert(sdl_question_menu_captures_pointer());
        SDL_Event right_up = map_pointer_event(SDL_EVENT_MOUSE_BUTTON_UP,
            10, 12, SDL_BUTTON_RIGHT);
        sdl_handle_event(&g_state, &right_up);
        if (release_in_question) {
            SDL_FRect question;
            SDL_Event release = map_pointer_event(SDL_EVENT_MOUSE_BUTTON_UP,
                10, 12, SDL_BUTTON_LEFT);
            assert(sdl_question_menu_current_rect(&question));
            assert(SDL_RenderCoordinatesToWindow(g_state.renderer,
                question.x + question.w / 2, question.y + question.h / 2,
                &release.button.x, &release.button.y));
            sdl_handle_event(&g_state, &release);
        }
        esc.type = SDL_EVENT_KEY_DOWN;
        esc.key.windowID = SDL_GetWindowID(g_state.window);
        esc.key.key = SDLK_ESCAPE;
        esc.key.scancode = SDL_SCANCODE_ESCAPE;
        sdl_handle_event(&g_state, &esc);
    }
    return 0;
}

static void check_hover_and_click(void)
{
    assert(g_mouse_path.hover_visible && g_mouse_path.path_valid
        && g_mouse_path.path_len > 0);
    assert(g_mouse_path.target_y == 10 && g_mouse_path.target_x == 15);
    assert(!g_main_map_drag.active && !g_mouse_path.follow_active);
    assert(p_ptr->wy == 0 && p_ptr->wx == 0);

    /* Confirm the renderer actually highlights the tile about to be clicked. */
    SDL_FRect tile;
    assert(sdl_player_map_rect(10, 15, &tile));
    assert(SDL_SetRenderTarget(g_state.renderer, NULL));
    assert(SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255));
    assert(SDL_RenderClear(g_state.renderer));
    sdl_mouse_path_render();
    SDL_Rect center = {(int)(tile.x + tile.w / 2), (int)(tile.y + tile.h / 2), 1, 1};
    SDL_Surface* pixel = SDL_RenderReadPixels(g_state.renderer, &center);
    Uint8 r, g, b;
    assert(pixel && SDL_ReadSurfacePixel(pixel, 0, 0, &r, &g, &b, NULL));
    assert(r || g || b);
    SDL_DestroySurface(pixel);

    SDL_Event press = map_pointer_event(SDL_EVENT_MOUSE_BUTTON_DOWN,
        10, 15, SDL_BUTTON_LEFT);
    SDL_Event release = map_pointer_event(SDL_EVENT_MOUSE_BUTTON_UP,
        10, 15, SDL_BUTTON_LEFT);
    sdl_handle_event(&g_state, &press);
    sdl_handle_event(&g_state, &release);
    assert(g_mouse_path.follow_active);
    test_term.xtra_hook = callback_sdl_xtra;
}

static errr scan_command_motion(int action, int value)
{
    if (action == TERM_XTRA_EVENT) {
        assert(!value && inkey_flag && !g_sdl_blocking_key_wait);
        SDL_Event motion = map_pointer_event(SDL_EVENT_MOUSE_MOTION, 10, 15, 0);
        assert(SDL_PushEvent(&motion));
        assert(!callback_sdl_xtra(action, value));
        check_hover_and_click();
    }
    return 0;
}

static void check_hover_after_question(bool held_left)
{
    int command, dir;
    popup_move_fixture(false);
    for (int x = 11; x <= 15; ++x)
        cave_info[10][x] |= CAVE_MARK | CAVE_SEEN;
    character_generated = true;
    term_screen = &test_term;
    inkey_flag = true;
    g_sdl_blocking_key_wait = true;

    SDL_Event initial_motion = map_pointer_event(SDL_EVENT_MOUSE_MOTION,
        10, 14, 0);
    sdl_handle_event(&g_state, &initial_motion);
    assert(g_mouse_path.hover_visible && g_mouse_path.path_valid);
    assert(g_mouse_path.target_y == 10 && g_mouse_path.target_x == 14);

    if (held_left) {
        SDL_Event left_down = map_pointer_event(SDL_EVENT_MOUSE_BUTTON_DOWN,
            10, 12, SDL_BUTTON_LEFT);
        sdl_handle_event(&g_state, &left_down);
        assert(g_main_map_drag.active && g_main_map_drag.mouse);
    }
    SDL_Event right_down = map_pointer_event(SDL_EVENT_MOUSE_BUTTON_DOWN,
        10, 12, SDL_BUTTON_RIGHT);
    sdl_handle_event(&g_state, &right_down);
    assert(g_mouse_path.grid_question_pending);

    release_in_question = held_left;
    test_term.xtra_hook = dismiss_grid_question;
    assert(sdl_grid_question_take_command(&command, &dir));
    assert(command == ' ' && dir == 0 && !sdl_question_menu_is_active());
    assert(!g_mouse_path.hover_visible && !g_mouse_path.follow_active);

    if (held_left) {
        SDL_Event motion = map_pointer_event(SDL_EVENT_MOUSE_MOTION, 10, 15, 0);
        inkey_flag = true;
        sdl_handle_event(&g_state, &motion);
        sdl_main_map_flush_pending_pan();
        check_hover_and_click();
    } else {
        g_sdl_blocking_key_wait = false;
        test_term.xtra_hook = scan_command_motion;
    }

    request_command();
    assert(p_ptr->command_cmd == ';' && p_ptr->command_dir == 6);
    puts(held_left ? "PASS: releasing during a grid question restores preview and movement without panning."
        : "PASS: the first command scan after a grid question restores preview and movement.");
    sdl_mouse_path_cancel();
    g_sdl_blocking_key_wait = false;
}

static void send_map_mouse_event(Uint32 type, int x)
{
    SDL_Event event = map_pointer_event(type, 10, x, SDL_BUTTON_LEFT);

    if (type == SDL_EVENT_MOUSE_MOTION)
        event.motion.state = SDL_BUTTON_LMASK;
    sdl_handle_event(&g_state, &event);
}

static void begin_mouse_run_hold(void)
{
    inkey_flag = true;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_DUNGEON);
    send_map_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, 12);
    assert(g_main_map_drag.active && g_main_map_drag.run_dir == 6);
    assert(!g_main_map_drag.run_ready && Term->key_head == Term->key_tail);
}

static void check_mouse_run_hold(void)
{
    popup_move_fixture(false);
    character_generated = true;
    term_screen = &test_term;
    test_term.xtra_hook = callback_sdl_xtra;
    for (int x = 11; x <= 15; ++x)
        cave_info[10][x] |= CAVE_MARK | CAVE_SEEN;

    begin_mouse_run_hold();
    send_map_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, 12);
    assert(!g_main_map_drag.active && g_mouse_path.follow_active);
    request_command();
    assert(p_ptr->command_cmd == ';' && p_ptr->command_dir == 6);
    sdl_mouse_path_cancel();

    begin_mouse_run_hold();
    Uint64 deadline = g_main_map_drag.run_deadline_ns;
    sdl_main_map_flush_pending_run(deadline - 1);
    assert(!g_main_map_drag.run_ready);
    assert(sdl_main_map_run_pending_timeout_ms(deadline - 1) == 1);
    while (!g_main_map_drag.run_ready
        && SDL_GetTicksNS() < deadline + SDL_MS_TO_NS(1500))
    {
        assert(!callback_sdl_xtra(TERM_XTRA_EVENT, 1));
    }
    assert(g_main_map_drag.run_ready && Term->key_head == Term->key_tail);
    assert(p_ptr->py == 10 && p_ptr->px == 11);

    SDL_FRect tile;
    assert(sdl_player_map_rect(10, 12, &tile));
    assert(SDL_SetRenderTarget(g_state.renderer, NULL));
    assert(SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255));
    assert(SDL_RenderClear(g_state.renderer));
    sdl_mouse_path_render();
    SDL_Rect center = {(int)(tile.x + tile.w / 2), (int)(tile.y + tile.h / 2), 1, 1};
    SDL_Surface* pixel = SDL_RenderReadPixels(g_state.renderer, &center);
    Uint8 r, g, b;
    assert(pixel && SDL_ReadSurfacePixel(pixel, 0, 0, &r, &g, &b, NULL));
    assert(g > r && g > b);
    SDL_DestroySurface(pixel);

    send_map_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, 12);
    assert(!g_main_map_drag.active && !g_mouse_path.follow_active);
    request_command();
    assert(p_ptr->command_cmd == '.' && p_ptr->command_dir == 6);
    assert(Term->key_head == Term->key_tail);

    SDL_Event cancellation[] = {
        { .type = SDL_EVENT_MOUSE_MOTION },
        { .key = { .type = SDL_EVENT_KEY_DOWN, .key = SDLK_ESCAPE } },
        { .type = SDL_EVENT_WINDOW_FOCUS_LOST },
    };
    for (int i = 0; i < (int)N_ELEMENTS(cancellation); ++i) {
        begin_mouse_run_hold();
        sdl_main_map_flush_pending_run(g_main_map_drag.run_deadline_ns);
        assert(g_main_map_drag.run_ready);
        if (cancellation[i].type == SDL_EVENT_MOUSE_MOTION) {
            send_map_mouse_event(SDL_EVENT_MOUSE_MOTION, 13);
            send_map_mouse_event(SDL_EVENT_MOUSE_MOTION, 12);
        } else {
            sdl_handle_event(&g_state, &cancellation[i]);
        }
        assert(!g_main_map_drag.run_ready);
        send_map_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, 12);
        assert(!g_main_map_drag.active && !g_mouse_path.follow_active);
        assert(Term->key_head == Term->key_tail);
        sdl_main_map_flush_pending_pan();
        assert(p_ptr->wy == 0 && p_ptr->wx == 0);
    }

    config.mouse_movement_mode = SDL_MOUSE_MOVEMENT_RIGHT_ONLY;
    send_map_mouse_event(SDL_EVENT_MOUSE_BUTTON_DOWN, 12);
    assert(!g_main_map_drag.run_dir);
    send_map_mouse_event(SDL_EVENT_MOUSE_BUTTON_UP, 12);
    assert(!g_mouse_path.follow_active && Term->key_head == Term->key_tail);
    config.mouse_movement_mode = SDL_MOUSE_MOVEMENT_ON;
    movement_input_set_active_context(MOVEMENT_INPUT_CONTEXT_NONE);
    inkey_flag = false;
    puts("PASS: mouse hold preview, directional Run, quick click, drag-off, Escape, focus loss, and right-click-only mode.");
}

static void check_popup_map_hover(const char* edit_dir)
{
    assert(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"));
    assert(SDL_InitSubSystem(SDL_INIT_VIDEO));
    assert(TTF_Init());
    sdl_config_set_defaults(&config);
    sdl_config_mark_character_wheel_tutorial_seen();
    path_build(config.story_font, sizeof(config.story_font), edit_dir,
        "../xtra/font/EBGaramond-Regular.ttf");
    SDL_strlcpy(config.story_font2, config.story_font,
        sizeof(config.story_font2));
    path_build(config.monospace_font, sizeof(config.monospace_font), edit_dir,
        "../xtra/font/VictorMono-Medium.ttf");
    config.mouse_enabled = true;
    config.mouse_movement_mode = SDL_MOUSE_MOVEMENT_ON;
    g_state.system_scale = 1;
    g_state.window = SDL_CreateWindow("Map hover fixture", 960, 576,
        SDL_WINDOW_FULLSCREEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, "software");
    assert(g_state.renderer);
    sdl_sync_palette();
    sdl_view* view = &g_views[PANE_MAIN];
    view->term_ready = true;
    view->cols = 80;
    view->rows = 24;
    view->cell_w = 12;
    view->cell_h = 24;
    view->rect = (SDL_Rect){0, 0, 960, 576};
    view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, 960, 576);
    assert(view->canvas);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    sdl_set_present_suppressed(true);
    check_hover_after_question(true);
    check_hover_after_question(false);
    check_mouse_run_hold();
    sdl_ui_text_cache_clear();
    sdl_story_font_cache_clear();
    SDL_DestroyRenderer(g_state.renderer);
    SDL_DestroyWindow(g_state.window);
    TTF_Quit();
}
'''


def main():
    engine.OUT = engine.ROOT / "scripts/output/context-popup-timing"
    prefix, main_body = engine.HARNESS.split("int main(int argc,char** argv)", 1)
    prefix = prefix.split("static void check_templates(void)", 1)[0]
    prefix = prefix.replace('#include "level-generation/level-generation-internal.h"',
                            '#include "sdl/main-sdl-private.h"')
    prefix = prefix.replace("    memset(dun, 0, sizeof(*dun));\n", "")
    prefix = prefix.replace("    current_build_vault_exact_token=false;\n"
                            "    current_build_vault_type=0;\n", "")
    initialization = main_body.split("    check_templates();", 1)[0]
    engine.HARNESS = (prefix + CHECKS + "\nint main(int argc,char** argv)"
                      + initialization + "    check_popup_timing();\n"
                      + "    check_popup_map_hover(argv[1]);\n"
                      + "    SDL_Quit();\n    return 0;\n}\n")
    engine.check_layouts = lambda: None
    engine.main()


if __name__ == "__main__":
    main()
