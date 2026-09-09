#!/usr/bin/env python3
"""Exercise corridor generation, SDL animation and fixture save compatibility.

Build with build-incremental.ps1 first. Runs the production C functions with a
software SDL renderer and isolated byte streams, without opening player saves.
"""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/idle-animation-check"

HARNESS = r'''
#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "cave/cave-fixtures.h"
#include "rng.h"
#include <assert.h>
#include <time.h>

static SDL_WindowFlags test_flags = SDL_WINDOW_INPUT_FOCUS;
static int image_loads, texture_creations, allocations;
static int fixture_draws, first_torch_draws, second_torch_draws;
static bool missing_asset;
static int presents;
void __wrap_sdl_present_if_needed(sdl_view* view) {
    (void)view;
    if (g_state.need_present) {
        presents++;
        SDL_FlushRenderer(g_state.renderer);
        g_state.need_present = false;
    }
}
static SDL_WindowFlags test_window_flags(SDL_Window* window) {
    (void)window; return test_flags;
}
static SDL_Surface* test_image_load(const char* path) {
    image_loads++;
    return missing_asset ? NULL : IMG_Load(path);
}
static SDL_Texture* test_texture(SDL_Renderer* r, SDL_Surface* s) {
    texture_creations++; return SDL_CreateTextureFromSurface(r, s);
}
static void* test_realloc(void* p, size_t n) {
    allocations++; return SDL_realloc(p, n);
}
static bool test_fixture_render(SDL_Renderer* renderer, SDL_Texture* texture,
    const SDL_FRect* src, const SDL_FRect* dst) {
    fixture_draws++;
    if (dst->x == (COL_MAP+11)*16 && dst->y == (ROW_MAP+9)*16) first_torch_draws++;
    if (dst->x == (COL_MAP+14)*16 && dst->y == (ROW_MAP+9)*16) second_torch_draws++;
    return SDL_RenderTexture(renderer,texture,src,dst);
}
#define SDL_GetWindowFlags test_window_flags
#define IMG_Load test_image_load
#define SDL_CreateTextureFromSurface test_texture
#define SDL_realloc test_realloc
#define SDL_RenderTexture test_fixture_render
#include "sdl/render/sdl-idle-animation.c"
#undef SDL_GetWindowFlags
#undef IMG_Load
#undef SDL_CreateTextureFromSurface
#undef SDL_realloc
#undef SDL_RenderTexture

void test_write_fixtures(void);
errr test_read_fixtures(void);
bool test_pick_torch_option(bool* handled);
void test_reset_torch_option(bool* app_dirty);
void apply_tunnel_niche_torch_glow(int, int, int, int);
static byte stream[200000];
static unsigned stream_size, stream_pos;
static bool new_save = true;
static int load_errors;
extern u32b load_byte_offset;
void __wrap_save_wr_byte(byte v) { assert(stream_size < sizeof(stream)); stream[stream_size++] = v; }
void __wrap_save_wr_u16b(u16b v) { __wrap_save_wr_byte(v); __wrap_save_wr_byte(v >> 8); }
void __wrap_load_rd_byte(byte* v) {
    *v = 0;
    if (stream_pos < stream_size) { *v = stream[stream_pos++]; load_byte_offset++; }
}
void __wrap_load_rd_u16b(u16b* v) {
    byte lo, hi; __wrap_load_rd_byte(&lo); __wrap_load_rd_byte(&hi); *v = lo | (hi << 8);
}
bool __wrap_load_savefile_version_at_least(byte a, byte b, byte c, byte d) {
    assert(a == 0 && b == 9 && c == 8 && d == 1); return new_save;
}
void __wrap_load_note(cptr msg) { (void)msg; load_errors++; }

#include "ui/question.h"
static int picker_choice;
int __wrap_ui_question_ask_overlay_buttons(cptr title, cptr desc,
    const ui_question_option* options, int count,
    const ui_question_button* buttons, int button_count,
    int y, int x, int default_index) {
    (void)title; (void)desc; (void)buttons; (void)button_count; (void)y; (void)x;
    assert(count == 2);
    assert(strcmp(options[0].label, "Always animate") == 0);
    assert(strcmp(options[1].label, "Freeze outside sight") == 0);
    assert(default_index == (op_ptr->opt[OPT_torch_animation_always] ? 0 : 1));
    return picker_choice;
}

static void option_tests(void) {
    const char* path = "scripts/output/idle-animation-check/config.json";
    assert(option_norm[OPT_torch_animation_always]);
    assert(strcmp(option_text[OPT_torch_animation_always], "torch_animation_always") == 0);
    assert(option_is_app_persistent(OPT_torch_animation_always));
    bool found = false;
    for (int i=0; i<OPT_PAGE_PER; i++)
        if (option_page[VISUAL_PAGE][i] == OPT_torch_animation_always) found = true;
    assert(found);
    sdl_config_reset_app_options_to_defaults();
    assert(op_ptr->opt[OPT_torch_animation_always]);
    FILE* file = fopen(path, "wb"); assert(file);
    fputs("{\"appOptions\":{\"visual\":{}}}", file); fclose(file);
    op_ptr->opt[OPT_torch_animation_always] = false;
    sdl_config_load_app_options(path);
    assert(op_ptr->opt[OPT_torch_animation_always]); /* Old/missing key default. */
    bool handled = false, app_dirty = false;
    picker_choice = 1; p_ptr->redraw = 0;
    assert(test_pick_torch_option(&handled) && handled);
    assert(!op_ptr->opt[OPT_torch_animation_always] && (p_ptr->redraw & PR_MAP));
    assert(sdl_config_save(path, &config, NULL, 0));
    sdl_config_reset_app_options_to_defaults();
    sdl_config_load_app_options(path);
    assert(!op_ptr->opt[OPT_torch_animation_always]);
    picker_choice = -1;
    assert(!test_pick_torch_option(&handled));
    assert(!op_ptr->opt[OPT_torch_animation_always]);
    test_reset_torch_option(&app_dirty);
    assert(app_dirty && op_ptr->opt[OPT_torch_animation_always]);
    assert(sdl_config_save(path, &config, NULL, 0));
    op_ptr->opt[OPT_torch_animation_always] = false;
    sdl_config_load_app_options(path);
    assert(op_ptr->opt[OPT_torch_animation_always]);
    picker_choice = 1; assert(test_pick_torch_option(&handled));
    picker_choice = 0; assert(test_pick_torch_option(&handled));
    assert(op_ptr->opt[OPT_torch_animation_always]);
    puts("Visual setting, picker choices/cancel/reset, missing-key default and JSON persistence: PASS");
}

static void floor_at(int y, int x) {
    cave_feat[y][x] = FEAT_FLOOR; cave_info[y][x] = 0;
}
static void niche(int y, int x) {
    floor_at(y, x);
    for (int i = -1; i <= 1; i++) floor_at(y + 1, x + i);
    apply_tunnel_niche_torch_glow(y, x, 1, 0);
}
static void fixture_save_tests(void) {
    stream_size = stream_pos = load_byte_offset = 0;
    test_write_fixtures();
    assert(stream_size == 10); /* Header plus two 3-byte fixtures. */
    cave_fixtures_clear();
    assert(test_read_fixtures() == 0 && stream_pos == stream_size);
    assert(cave_fixture_at(9, 11) == CAVE_FIXTURE_WALL_TORCH);
    assert(cave_fixture_at(10, 18) == CAVE_FIXTURE_BRAZIER);
    new_save = false; stream_pos = load_byte_offset = 0;
    assert(test_read_fixtures() == 0 && stream_pos == 0);
    assert(!cave_fixture_at(9, 11));
    new_save = true;
    assert(test_read_fixtures() == 0);
    for (unsigned n = 0; n < stream_size; n++) {
        unsigned full = stream_size;
        stream_size = n; stream_pos = load_byte_offset = 0;
        assert(test_read_fixtures() != 0);
        stream_size = full;
    }
    byte saved = stream[4]; stream[4] = 255;
    stream_pos = load_byte_offset = 0;
    assert(test_read_fixtures() != 0);
    stream[4] = saved;
    saved = stream[6]; stream[6] = 255;
    stream_pos = load_byte_offset = 0;
    assert(test_read_fixtures() != 0);
    stream[6] = saved;
    stream_pos = load_byte_offset = 0;
    assert(test_read_fixtures() == 0);
    puts("Fixture save roundtrip, old-save default, truncation and invalid records: PASS");
}

static void draw_cell(int y, int x, bool covered) {
    int col = COL_MAP + x * (use_bigtile ? 2 : 1), row = ROW_MAP + y;
    byte ta = TILE_FLAG, a = TILE_FLAG;
    char tc = (char)(TILE_FLAG | (cave_feat[y][x] == FEAT_FLOOR ? 1 : 4));
    char c = covered ? (char)(TILE_FLAG | 2) : tc;
    Term->scr->a[row][col] = a; Term->scr->ta[row][col] = ta;
    Term->scr->c[row][col] = c; Term->scr->tc[row][col] = tc;
    callback_sdl_pict(col, row, 1, &a, &c, &ta, &tc);
}
static SDL_Surface* capture(int index) {
    char path[160];
    SDL_SetRenderTarget(g_state.renderer, g_views[PANE_MAIN].canvas);
    SDL_Surface* s = SDL_RenderReadPixels(g_state.renderer, NULL); assert(s);
    strnfmt(path, sizeof(path), "scripts/output/idle-animation-check/frame%d.bmp", index);
    assert(SDL_SaveBMP(s, path)); return s;
}
static unsigned differences(SDL_Surface* a, SDL_Surface* b) {
    unsigned changes = 0;
    assert(a->w == b->w && a->h == b->h && a->format == b->format);
    for (int y = 0; y < a->h; y++) for (int x = 0; x < a->w; x++) {
        Uint32 pa = ((Uint32*)((byte*)a->pixels + y*a->pitch))[x];
        Uint32 pb = ((Uint32*)((byte*)b->pixels + y*b->pitch))[x];
        if (pa != pb) {
            int col = x / g_views[PANE_MAIN].cell_w;
            int row = y / g_views[PANE_MAIN].cell_h;
            assert((col == COL_MAP + 11 && row == ROW_MAP + 9)
                || (col == COL_MAP + 18 && row == ROW_MAP + 10));
            changes++;
        }
    }
    return changes;
}
static void expect_paused(Uint64 now) {
    assert(sdl_idle_animation_timeout_ms(now) == -1);
    g_state.need_present = false;
    sdl_idle_animation_update(now);
    assert(!g_state.need_present);
}

static Uint64 next_animation_time(Uint64 now) {
    int ms = sdl_idle_animation_timeout_ms(now);
    assert(ms >= 0);
    return now + (Uint64)ms * 1000000ULL;
}

static void expect_middle_frame(SDL_Surface* canvas, int y, int x, const char* asset) {
    SDL_Surface* source = IMG_Load(asset); assert(source);
    SDL_Surface* expected = SDL_ConvertSurface(source, canvas->format); assert(expected);
    for (int row=0; row<16; row++)
        assert(memcmp((byte*)canvas->pixels + ((ROW_MAP+y)*16+row)*canvas->pitch
                + (COL_MAP+x)*16*4,
            (byte*)expected->pixels + row*expected->pitch, 16*4) == 0);
    SDL_DestroySurface(expected); SDL_DestroySurface(source);
}

static void freeze_tests(Uint64* now) {
    op_ptr->opt[OPT_torch_animation_always] = false;
    cave_info[9][11] &= ~CAVE_SEEN; cave_info[10][18] &= ~CAVE_SEEN;
    draw_cell(9,11,false); draw_cell(10,18,false);
    SDL_Surface* frozen = capture(6);
    expect_middle_frame(frozen,9,11,"lib/xtra/graf/anim_wall_torch_f1.png");
    expect_middle_frame(frozen,10,18,"lib/xtra/graf/anim_brazier_f1.png");
    int loads = image_loads, textures = texture_creations, mallocs = allocations;
    for (int i=0; i<100; i++) { *now += IDLE_STEP_NS; expect_paused(*now); }
    assert(image_loads == loads && texture_creations == textures && allocations == mallocs);
    SDL_Surface* still = capture(7);
    assert(differences(frozen,still) == 0);
    SDL_DestroySurface(still); SDL_DestroySurface(frozen);
    /* Seen torches still animate in freeze mode. */
    cave_info[9][11] |= CAVE_SEEN; cave_info[10][18] |= CAVE_SEEN;
    draw_cell(9,11,false); draw_cell(10,18,false);
    SDL_Surface* visible = capture(8);
    *now = next_animation_time(frame_tick * IDLE_STEP_NS);
    assert(sdl_idle_animation_timeout_ms(*now) == 0);
    sdl_idle_animation_update(*now);
    SDL_Surface* moving = capture(9);
    assert(differences(visible,moving) > 0);
    SDL_DestroySurface(visible); SDL_DestroySurface(moving);

    /* Real lite_spot/prt_map paths must repaint the unchanged wall glyph on
     * loss of sight and when a setting changes, then select the middle frame. */
    int col = COL_MAP + 11, row = ROW_MAP + 9;
    Term->old->a[row][col] = Term->scr->a[row][col];
    Term->old->c[row][col] = Term->scr->c[row][col];
    Term->old->ta[row][col] = Term->scr->ta[row][col];
    Term->old->tc[row][col] = Term->scr->tc[row][col];
    cave_info[9][11] &= ~CAVE_SEEN;
    lite_spot(9,11);
    assert(Term->old->a[row][col] == 255 && Term->old->c[row][col] == 0);
    callback_sdl_pict(col,row,1,&Term->scr->a[row][col],&Term->scr->c[row][col],
        &Term->scr->ta[row][col],&Term->scr->tc[row][col]);
    SDL_Surface* lost_sight = capture(10);
    expect_middle_frame(lost_sight,9,11,"lib/xtra/graf/anim_wall_torch_f1.png");
    SDL_DestroySurface(lost_sight);
    Term->old->a[row][col] = Term->scr->a[row][col];
    Term->old->c[row][col] = Term->scr->c[row][col];
    prt_map();
    assert(Term->old->a[row][col] == 255 && Term->old->c[row][col] == 0);
    op_ptr->opt[OPT_torch_animation_always] = true;
    cave_info[9][11] |= CAVE_SEEN; cave_info[10][18] |= CAVE_SEEN;
    draw_cell(9,11,false); draw_cell(10,18,false);
    *now += IDLE_STEP_NS;
    puts("Freeze mode: exact second frame for both fixtures, no idle wakeups, seen animation and unchanged-glyph redraw: PASS");
}
static void animation_tests(void) {
    Uint64 initial_tick = frame_tick;
    Uint64 now = initial_tick * IDLE_STEP_NS;
    assert(sdl_idle_animation_timeout_ms(now) > 0 && sdl_idle_animation_timeout_ms(now) <= 240);
    u64b rng = Rand_state_export();
    s32b turns = turn, player_turns = playerturn;
    int loads = image_loads, textures = texture_creations, mallocs = allocations;
    SDL_Surface* frames[3];
    frames[0] = capture(0);
    for (int i = 1; i < 3; i++) {
        now = next_animation_time(now);
        assert(sdl_idle_animation_timeout_ms(now) == 0);
        sdl_idle_animation_update(now);
        assert(g_state.need_present);
        frames[i] = capture(i);
        assert(differences(frames[i-1], frames[i]) > 0);
        g_state.need_present = false;
        sdl_idle_animation_update(now + IDLE_STEP_NS - 1);
        assert(!g_state.need_present);
    }
    assert(differences(frames[0], frames[2]) > 0);
    for (int i=0; i<3; i++) SDL_DestroySurface(frames[i]);
    freeze_tests(&now);
    now += 10 * IDLE_STEP_NS;
    character_icky = 1; expect_paused(now); character_icky = 0;
    g_minimap.active = true; expect_paused(now); g_minimap.active = false;
    g_main_menu_overlay_active = true; expect_paused(now); g_main_menu_overlay_active = false;
    g_sdl_present_suppressed = true; expect_paused(now); g_sdl_present_suppressed = false;
    g_state.use_tiles = false; expect_paused(now); g_state.use_tiles = true;
    p_ptr->blind = 1; expect_paused(now); p_ptr->blind = 0;
    test_flags = 0; expect_paused(now);
    test_flags = SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MINIMIZED; expect_paused(now);
    test_flags = SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_HIDDEN; expect_paused(now);
    test_flags = SDL_WINDOW_INPUT_FOCUS;
    p_ptr->wy = 1; expect_paused(now); p_ptr->wy = 0;
    /* Leaving sight must neither dim nor freeze explored torches/braziers. */
    SDL_Surface* seen = capture(3);
    cave_info[9][11] &= ~CAVE_SEEN; cave_info[10][18] &= ~CAVE_SEEN;
    draw_cell(9, 11, false); draw_cell(10, 18, false);
    SDL_Surface* remembered = capture(4);
    assert(differences(seen, remembered) == 0);
    SDL_DestroySurface(seen);
    now = next_animation_time(frame_tick * IDLE_STEP_NS);
    assert(sdl_idle_animation_timeout_ms(now) == 0);
    sdl_idle_animation_update(now);
    SDL_Surface* animated_memory = capture(5);
    assert(differences(remembered, animated_memory) > 0);
    SDL_DestroySurface(remembered); SDL_DestroySurface(animated_memory);
    assert(frame_tick == now / IDLE_STEP_NS);
    now += IDLE_STEP_NS;
    /* Undiscovered tiles, and terrain suppressed by special visibility modes,
     * must still cause no animation work or direct fixture rendering. */
    cave_info[9][11] &= ~CAVE_MARK; cave_info[10][18] &= ~CAVE_MARK;
    expect_paused(now);
    SDL_FRect hidden_dst = {0,0,16,16};
    assert(!sdl_idle_animation_draw(9, 11, &hidden_dst));
    assert(!sdl_idle_animation_draw(10, 18, &hidden_dst));
    cave_info[9][11] |= CAVE_MARK; cave_info[10][18] |= CAVE_MARK;
    p_ptr->rage = 1; expect_paused(now); p_ptr->rage = 0;
    g_labyrinth_view_active = true; expect_paused(now); g_labyrinth_view_active = false;
    assert(Rand_state_export() == rng && turn == turns && playerturn == player_turns);
    assert(image_loads == loads && texture_creations == textures && allocations == mallocs);
    /* Exercise 10,000 scheduler steps without sleeping or presenting. */
    clock_t begin = clock();
    for (int i = 0; i < 10000; i++) {
        now += IDLE_STEP_NS; sdl_idle_animation_update(now);
        SDL_FlushRenderer(g_state.renderer);
    }
    printf("10,000 two-fixture scheduler steps: %.3f CPU seconds; %d image loads, %d GPU texture, no update allocations\n",
        (double)(clock()-begin)/CLOCKS_PER_SEC, image_loads, texture_creations);
    assert(image_loads == loads && texture_creations == textures && allocations == mallocs);
    assert(Rand_state_export() == rng && turn == turns && playerturn == player_turns);
    /* Run the real blocking input callback with no key/mouse input. Only the
     * final window presentation is intercepted; the SDL event wait is real. */
    frame_tick = SDL_GetTicksNS() / IDLE_STEP_NS;
    draw_cell(9,11,false); draw_cell(10,18,false);
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    presents = 0; g_state.need_present = false;
    Uint64 wait_begin = SDL_GetTicksNS();
    for (int i=0; i<6; i++) callback_sdl_xtra(TERM_XTRA_EVENT, 1);
    Uint64 elapsed = SDL_GetTicksNS() - wait_begin;
    assert(elapsed >= 5 * IDLE_STEP_NS && elapsed < 2 * 1000000000ULL);
    assert(presents == 6 && Term->key_head == Term->key_tail);
    assert(Rand_state_export() == rng && turn == turns && playerturn == player_turns);
    printf("Real blocking SDL input loop: 6 idle frames in %.3f seconds, no input or turns\n",
        elapsed / 1000000000.0);
    now = (frame_tick + 1) * IDLE_STEP_NS;
    Term->soft_cursor = true; Term->old->cv = true; Term->old->cu = false;
    Term->old->cx = COL_MAP + 11; Term->old->cy = ROW_MAP + 9;
    cave_info[10][18] &= ~(CAVE_MARK | CAVE_SEEN);
    expect_paused(now);
    cave_info[10][18] |= CAVE_MARK | CAVE_SEEN;
    Term->old->cv = false; Term->soft_cursor = false;
    draw_cell(10, 18, true);
    cave_info[9][11] &= ~(CAVE_MARK | CAVE_SEEN);
    expect_paused(now + IDLE_STEP_NS);
    cave_info[9][11] |= CAVE_MARK | CAVE_SEEN;
    draw_cell(10, 18, false);
    callback_sdl_wipe(COL_MAP + 11, ROW_MAP + 9, 1);
    callback_sdl_wipe(COL_MAP + 18, ROW_MAP + 10, 1);
    expect_paused(now + IDLE_STEP_NS);
    assert(cell_count == 0);
    draw_cell(9, 11, false); draw_cell(10, 18, false);
    callback_sdl_xtra(TERM_XTRA_CLEAR, 0); assert(cell_count == 0);
    /* Rebuilding resources resets both tracked cells and the failed-load latch. */
    sdl_idle_animation_shutdown(); assert(!fixture_texture && !cells);
    missing_asset = true;
    SDL_FRect dst = {0,0,32,32};
    loads = image_loads;
    for (int i=0; i<100; i++) assert(!sdl_idle_animation_draw(9,11,&dst));
    assert(image_loads == loads + 1);
    sdl_idle_animation_shutdown(); missing_asset = false;
    use_bigtile = true;
    draw_cell(9, 11, false); assert(cell_count == 1);
    assert(sdl_idle_animation_timeout_ms(frame_tick * IDLE_STEP_NS) > 0
        && sdl_idle_animation_timeout_ms(frame_tick * IDLE_STEP_NS) <= 240);
    sdl_idle_animation_invalidate_span(COL_MAP + 11*2 + 1, ROW_MAP + 9, 1);
    assert(cell_count == 0); use_bigtile = false;
    puts("Real SDL frames, dirty-cell bounds, rate cap, visibility, overlays, focus, big tiles, failure caching and unchanged game state: PASS");
}

static bool same_surface(SDL_Surface* a, SDL_Surface* b) {
    assert(a->format == b->format && a->w == b->w && a->h == b->h);
    for (int y=0; y<a->h; y++)
        if (memcmp((byte*)a->pixels+y*a->pitch, (byte*)b->pixels+y*b->pitch, a->w*4))
            return false;
    return true;
}
static void asynchronous_tests(void) {
    p_ptr->wx = p_ptr->wy = 0;
    op_ptr->opt[OPT_torch_animation_always] = true;
    cave_feat[9][14] = FEAT_WALL_EXTRA;
    cave_info[9][14] |= CAVE_MARK | CAVE_SEEN | CAVE_GLOW;
    cave_fixture_set(9,14,CAVE_FIXTURE_WALL_TORCH);
    frame_tick = 0;
    Term->total_erase = true; prt_map(); Term_fresh();
    assert(cell_count == 3);
    int loads = image_loads, textures = texture_creations, mallocs = allocations;
    bool first_alone = false, second_alone = false;
    int first_total = 0, second_total = 0, refreshes = 0;
    u64b rng = Rand_state_export(); s32b turns = turn;
    for (int step=1; step<=750; step++) {
        fixture_draws = first_torch_draws = second_torch_draws = 0;
        g_state.need_present = false;
        Uint64 now = step * IDLE_STEP_NS;
        sdl_idle_animation_update(now);
        assert(g_state.need_present == (fixture_draws != 0));
        assert(fixture_draws <= 3);
        if (first_torch_draws && !second_torch_draws) first_alone = true;
        if (second_torch_draws && !first_torch_draws) second_alone = true;
        first_total += first_torch_draws; second_total += second_torch_draws;
        if (fixture_draws) refreshes++;
        int drawn = fixture_draws;
        sdl_idle_animation_update(now + IDLE_STEP_NS - 1);
        assert(fixture_draws == drawn); /* At most one refresh per 40 ms. */
        int timeout = sdl_idle_animation_timeout_ms(now);
        assert(timeout >= 40 && timeout <= 240);
        SDL_FlushRenderer(g_state.renderer);
    }
    assert(first_alone && second_alone);
    assert(first_total >= 125 && first_total <= 188);
    assert(second_total >= 125 && second_total <= 188);
    assert(image_loads == loads && texture_creations == textures && allocations == mallocs);
    assert(Rand_state_export() == rng && turn == turns);
    printf("Independent torches: %d and %d frame changes, %.2f shared refreshes/sec over 30 simulated seconds; 25/sec cap: PASS\n",
        first_total,second_total,refreshes/30.0);

    /* A dense room uses the same scheduler and texture, with no per-fixture
     * timers. Measure render work separately from the window compositor. */
    for (int y=2; y<10; y++) for (int x=2; x<10; x++) {
        cave_feat[y][x] = FEAT_WALL_EXTRA;
        cave_info[y][x] |= CAVE_MARK | CAVE_SEEN | CAVE_GLOW;
        cave_fixture_set(y,x,CAVE_FIXTURE_WALL_TORCH);
    }
    prt_map(); Term_fresh(); assert(cell_count == 67);
    loads = image_loads; textures = texture_creations; mallocs = allocations;
    int draws_before = fixture_draws;
    clock_t begin = clock();
    for (int i=0; i<10000; i++) {
        sdl_idle_animation_update((751ULL+i)*IDLE_STEP_NS);
        SDL_FlushRenderer(g_state.renderer);
    }
    printf("67 fixtures / 10,000 scheduler steps: %.3f CPU seconds, %d changed-cell draws; no new loads, textures or allocations\n",
        (double)(clock()-begin)/CLOCKS_PER_SEC,fixture_draws-draws_before);
    assert(image_loads == loads && texture_creations == textures && allocations == mallocs);
    assert(Rand_state_export() == rng && turn == turns);
}
static void pan_tests(void) {
    Term->mapped_flag = true;
    Term->higher_pict = true; Term->never_frosh = true; Term->never_bored = true;
    Term->pict_hook = callback_sdl_pict; Term->wipe_hook = callback_sdl_wipe;
    Term->text_hook = callback_sdl_text; Term->xtra_hook = callback_sdl_xtra;
    Term->curs_hook = callback_sdl_curs; Term->bigcurs_hook = callback_sdl_bigcurs;
    for (int big=0; big<2; big++) for (int mode=0; mode<2; mode++) {
        use_bigtile = big; op_ptr->opt[OPT_torch_animation_always] = mode;
        p_ptr->wx = p_ptr->wy = 0;
        Term->total_erase = true; prt_map(); Term_fresh();
        assert(!Term->total_erase && cell_count == 2);
        for (int pan=0; pan<4; pan++) {
            p_ptr->wx = (pan == 0) ? 1 : 0;
            p_ptr->wy = (pan == 2) ? 1 : 0;
            prt_map(); Term_fresh();
            SDL_Surface* incremental = capture(20 + pan*2);
            /* A forced repaint is the reference: same map, same frame. */
            force_map_redraw(); Term_fresh();
            SDL_Surface* full = capture(21 + pan*2);
            if (!same_surface(incremental,full)) {
                fprintf(stderr,"Stale map pixels after pan %d (big=%d mode=%d)\n", pan,big,mode);
                assert(false);
            }
            SDL_DestroySurface(incremental); SDL_DestroySurface(full);
        }
    }
    use_bigtile = false;
    puts("Map pan out/back: incremental pixels match a full repaint, both animation modes and tile widths: PASS");
}
int main(void) {
    setbuf(stdout, NULL); log_set_level(LOG_WARN);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));
    assert(TTF_Init());
    sdl_config_set_defaults(&config);
    option_tests();
    config.min_terminal_mode = false;
    assert(SDL_CreateWindowAndRenderer("Idle animation test", 1024, 768,
        SDL_WINDOW_HIDDEN, &g_state.window, &g_state.renderer));
    g_state.use_tiles = true; assert(sdl_load_tileset_texture());
    use_graphics = GRAPHICS_MICROCHASM;
    sdl_view* view = &g_views[PANE_MAIN];
    view->cols = 64; view->rows = 24; view->cell_w = view->cell_h = 16;
    view->rect = (SDL_Rect){0,0,1024,384};
    view->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, 1024,384); assert(view->canvas);
    term_init(&view->t,64,24,256); view->term_ready = true;
    view->t.data = (void*)(uintptr_t)PANE_MAIN;
    term_screen = Term = &view->t; angband_term[PANE_MAIN] = Term;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 32;
    p_ptr->playing = true; p_ptr->wy = p_ptr->wx = 0;
    p_ptr->py = 15; p_ptr->px = 15;
    cave_feat = calloc(MAX_DUNGEON_HGT, sizeof(*cave_feat));
    cave_info = calloc(MAX_DUNGEON_HGT, sizeof(*cave_info));
    cave_m_idx = calloc(MAX_DUNGEON_HGT, sizeof(*cave_m_idx));
    cave_o_idx = calloc(MAX_DUNGEON_HGT, sizeof(*cave_o_idx));
    cave_light = calloc(MAX_DUNGEON_HGT, sizeof(*cave_light));
    cave_color = calloc(MAX_DUNGEON_HGT, sizeof(*cave_color));
    cave_rewired = calloc(MAX_DUNGEON_HGT, sizeof(*cave_rewired));
    f_info = calloc(256, sizeof(*f_info));
    z_info = calloc(1, sizeof(*z_info)); z_info->style_max = 1;
    style_info = calloc(1, sizeof(*style_info));
    style_info[0].name = 1; style_info[0].wall_col = 4; style_info[0].floor_col = 1;
    for (int i=0; i<256; i++) { f_info[i].mimic = i; f_info[i].x_attr = TILE_FLAG; }
    f_info[FEAT_NONE].x_char = (char)TILE_FLAG;
    f_info[FEAT_FLOOR].x_char = (char)(TILE_FLAG | 1);
    f_info[FEAT_WALL_EXTRA].x_char = (char)(TILE_FLAG | 4);
    for (int y=0;y<32;y++) for(int x=0;x<32;x++) {
        cave_feat[y][x] = FEAT_WALL_EXTRA; cave_info[y][x] = CAVE_WALL;
        cave_color[y][x] = COLOR_STYLE_BASE; cave_light[y][x] = 2;
    }
    Rand_state_init(1234); u64b rng = Rand_state_export();
    niche(10,11); niche(10,18);
    assert(Rand_state_export() == rng);
    assert(cave_fixture_at(9,11) == CAVE_FIXTURE_WALL_TORCH);
    assert(cave_fixture_at(10,18) == CAVE_FIXTURE_BRAZIER);
    assert(cave_feat[10][11] == FEAT_FLOOR && (cave_info[11][11] & CAVE_GLOW));
    fixture_save_tests();
    cave_set_feat(9,11,FEAT_FLOOR); assert(!cave_fixture_at(9,11));
    cave_feat[9][11] = FEAT_WALL_EXTRA; cave_fixture_set(9,11,CAVE_FIXTURE_WALL_TORCH);
    character_generated = character_dungeon = true;
    for (int y=0;y<20;y++) for(int x=0;x<30;x++) {
        cave_info[y][x] |= CAVE_MARK | CAVE_SEEN; draw_cell(y,x,false);
    }
    assert(image_loads == 6 && texture_creations == 1 && cell_count == 2);
    animation_tests();
    pan_tests();
    asynchronous_tests();
    sdl_idle_animation_shutdown();
    SDL_Quit(); puts("Idle animation checks: PASS"); return 0;
}
'''

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    wrappers = []
    for operation, internal in (("write", "wr"), ("read", "rd")):
        stem = "save" if operation == "write" else "load"
        p = OUT / (stem + "-check.c")
        result = "void" if operation == "write" else "errr"
        ret = "" if operation == "write" else "return "
        p.write_text(f'#include "fs/{stem}-dungeon.c"\n'
                     f'{result} test_{operation}_fixtures(void) {{ {ret}{internal}_fixtures(); }}\n',
                     encoding="utf-8")
        wrappers.append(str(p))
    menu_source = OUT / "settings-check.c"
    menu_source.write_text('#include "cmd/ui/cmd-ui-settings.c"\n'
        'bool test_pick_torch_option(bool* handled) { return option_pick_value(OPT_torch_animation_always, handled); }\n'
        'void test_reset_torch_option(bool* app_dirty) { const int opt[] = { OPT_torch_animation_always }; bool meta = false, sound = false; options_aux_reset_to_default(VISUAL_PAGE, opt, 0, false, NULL, app_dirty, &sound, &meta); }\n',
        encoding="utf-8")
    wrappers.append(str(menu_source))
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    exclude = ("/src/main.c.obj", "/src/sdl/render/sdl-idle-animation.c.obj",
               "/src/fs/save-dungeon.c.obj", "/src/fs/load-dungeon.c.obj",
               "/src/cmd/ui/cmd-ui-settings.c.obj")
    objects = [p for p in objects if not p.endswith(exclude)]
    rsp = OUT / "objects.rsp"
    rsp.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *[str(BUILD / "_deps" / x) for x in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")], env["PATH"]])
    env["SDL_VIDEO_DRIVER"] = "dummy"
    env["SDL_RENDER_DRIVER"] = "software"
    symbols = ("save_wr_byte", "save_wr_u16b", "load_rd_byte", "load_rd_u16b",
               "load_savefile_version_at_least", "load_note", "sdl_present_if_needed",
               "ui_question_ask_overlay_buttons")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *wrappers,
        "@" + str(rsp), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        *["-Wl,--wrap=" + s for s in symbols], "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=45)

if __name__ == "__main__":
    main()
