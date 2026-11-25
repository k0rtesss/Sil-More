#!/usr/bin/env python3
"""Exercise discovery and description banners with an injected SDL clock."""

import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import BUILD, HARNESS, ROOT


OUT = ROOT / "scripts/output/narrative-banner-sequence"
CHECKS = r'''
void reset_level_entry_tracking(void);
void advance_narrative_banner_turn(void);
void handle_partition_entry(bool force_message, int narrative_mode);
void display_partition_narrative_banner(int old_style, int new_style,
    level_partition_kind kind, bool line_delay);

enum { SAVED_PARTITION_MODE_CHASM = 4 };
static Uint64 clock_ns;
static unsigned touch_cancels;
static bool trace_text;
static char rendered_text[1024];

Uint64 __real_SDL_GetTicksNS(void);
Uint64 __wrap_SDL_GetTicksNS(void)
{
    return clock_ns ? clock_ns : __real_SDL_GetTicksNS();
}

void __real_SDL_Delay(Uint32 ms);
void __wrap_SDL_Delay(Uint32 ms)
{
    assert(!clock_ns || !ms);
    __real_SDL_Delay(ms);
}

void __real_sdl_touch_cancel_all_inputs(void);
void __wrap_sdl_touch_cancel_all_inputs(void)
{
    touch_cancels++;
    __real_sdl_touch_cancel_all_inputs();
}

SDL_Texture* __real_sdl_ui_text_texture(TTF_Font* font, cptr text,
    SDL_Color color, int* width, int* height);
SDL_Texture* __wrap_sdl_ui_text_texture(TTF_Font* font, cptr text,
    SDL_Color color, int* width, int* height)
{
    SDL_Texture* texture = __real_sdl_ui_text_texture(font, text, color, width, height);
    if (trace_text && texture) {
        if (rendered_text[0]) SDL_strlcat(rendered_text, " ", sizeof(rendered_text));
        SDL_strlcat(rendered_text, text, sizeof(rendered_text));
    }
    return texture;
}

static void tick(unsigned ms)
{
    clock_ns += SDL_MS_TO_NS(ms);
    sdl_narrative_banner_update(clock_ns);
}

static Uint64 banner_pixels(void)
{
    SDL_Renderer* renderer = g_state.renderer;
    SDL_Texture* target = SDL_GetRenderTarget(renderer);
    assert(SDL_SetRenderTarget(renderer, NULL));
    assert(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
    assert(SDL_RenderClear(renderer));
    rendered_text[0] = '\0';
    trace_text = true;
    sdl_narrative_banner_render();
    trace_text = false;
    SDL_Surface* frame = SDL_RenderReadPixels(renderer, NULL);
    assert(frame);
    Uint64 brightness = 0;
    for (int y = 0; y < frame->h; ++y) {
        for (int x = 0; x < frame->w; ++x) {
            Uint8 r, g, b, a;
            assert(SDL_ReadSurfacePixel(frame, x, y, &r, &g, &b, &a));
            brightness += r + g + b;
        }
    }
    SDL_DestroySurface(frame);
    assert(SDL_SetRenderTarget(renderer, target));
    return brightness;
}

static void arm_discovery(byte turns, bool fade, bool initial_entry)
{
    reset_level_entry_tracking();
    p_ptr->discovery_lore_flags &= ~DISC_LORE_CHASM;
    op_ptr->narrative_banner_turns = turns;
    op_ptr->level_entry_narrative_mode = fade ? LEVEL_ENTRY_NARRATIVE_BANNER_DELAY
                                            : LEVEL_ENTRY_NARRATIVE_BANNER;
    p_ptr->command_cmd = initial_entry ? 0 : '6';
    s32b before = p_ptr->exp;
    if (initial_entry) {
        /* The level setup and initial prose are separate engine callbacks. */
        handle_partition_entry(true, PARTITION_NARRATIVE_OFF);
        display_partition_narrative_banner(-1, 0, LEVEL_PART_CHASM, fade);
    } else {
        handle_partition_entry(false, fade ? PARTITION_NARRATIVE_BANNER_DELAY
                                           : PARTITION_NARRATIVE_BANNER);
    }
    assert(p_ptr->exp == before + 300 && (p_ptr->discovery_lore_flags & DISC_LORE_CHASM));
    assert(strstr(active_narrative_banner_text(), "You gain 300 experience."));
    assert(active_narrative_banner_has_followup());
    assert(active_narrative_banner_consumes_input() == (turns == 0));
}

static void check_sequence(void)
{
    assert(SDL_SetWindowSize(g_state.window, 720, 1000));
    assert(SDL_SyncWindow(g_state.window));
    sdl_apply_config_no_redraw();
    p_ptr->playing = true;
    p_ptr->py = p_ptr->px = 12;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 40;
    p_ptr->depth = p_ptr->max_depth = 11;
    p_ptr->chp = p_ptr->mhp = 100;
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    character_generated = character_dungeon = true;
    character_icky = 0;
    turn = 331;
    playerturn = 33;
    cave_color[p_ptr->py][p_ptr->px] = 128;
    const partition_meta_save partition = {
        .grid_rows = 1, .grid_cols = 1, .partition_count = 1,
        .modes = { SAVED_PARTITION_MODE_CHASM },
    };
    level_partition_meta_set(&partition);
    assert(level_partition_kind_for_point(p_ptr->py, p_ptr->px) == LEVEL_PART_CHASM);

    clock_ns = SDL_MS_TO_NS(10000);
    arm_discovery(0, true, true);
    u64b rng = Rand_state_export();
    assert(sdl_narrative_banner_timeout_ms(clock_ns) == 0);
    tick(0);
    assert(banner_pixels() == 0);
    tick(SDL_NARRATIVE_BANNER_FADE_MS / 2);
    Uint64 partial = banner_pixels();
    assert(partial > 0);
    tick(SDL_NARRATIVE_BANNER_FADE_MS / 2);
    Uint64 discovery = banner_pixels();
    assert(discovery > partial && strstr(rendered_text, "You gain 300 experience."));
    assert(sdl_narrative_banner_timeout_ms(clock_ns) > 1000);

    /* A blocking foreground question hides the banner and suspends its
     * reading window. It must never advance behind this question. */
    sdl_touch_pane_begin_yes_no_prompt("Keep the question in front?");
    tick(10000);
    assert(sdl_narrative_banner_timeout_ms(clock_ns) == -1);
    assert(banner_pixels() == 0);
    assert(strstr(active_narrative_banner_text(), "You gain 300 experience."));
    sdl_touch_pane_end_yes_no_prompt();
    tick(0);
    assert(banner_pixels() == discovery);
    unsigned cancels = touch_cancels;
    int reading_ms = sdl_narrative_banner_timeout_ms(clock_ns);
    assert(reading_ms > 1000);
    tick(reading_ms - 1);
    assert(strstr(active_narrative_banner_text(), "You gain 300 experience."));
    tick(1);
    assert(strstr(active_narrative_banner_text(), "A vast darkness yawns below"));
    assert(!active_narrative_banner_has_followup());
    assert(active_narrative_banner_consumes_input() && cancels == touch_cancels);
    assert(banner_pixels() == 0);
    tick(SDL_NARRATIVE_BANNER_FADE_MS);
    assert(banner_pixels() > 0 && strstr(rendered_text, "A vast darkness yawns below"));
    assert(sdl_narrative_banner_timeout_ms(clock_ns) == -1);
    SDL_Event back = { .type = SDL_EVENT_KEY_DOWN };
    back.key.windowID = SDL_GetWindowID(g_state.window);
    back.key.key = SDLK_ESCAPE;
    back.key.scancode = SDL_SCANCODE_ESCAPE;
    sdl_handle_event(&g_state, &back);
    assert(!active_narrative_banner_consumes_input() && banner_pixels() == 0);
    assert(turn == 331 && playerturn == 33 && Rand_state_export() == rng);

    /* With a turn lifetime, the arrival action does not consume the notice.
     * One subsequent action advances it and a second removes it. */
    arm_discovery(1, false, false);
    assert(banner_pixels() > 0);
    advance_narrative_banner_turn();
    assert(active_narrative_banner_has_followup());
    advance_narrative_banner_turn();
    assert(strstr(active_narrative_banner_text(), "A vast darkness yawns below"));
    assert(!active_narrative_banner_consumes_input() && banner_pixels() > 0);
    advance_narrative_banner_turn();
    assert(!active_narrative_banner_visible());

    arm_discovery(0, false, true);
    tick(0);
    assert(banner_pixels() > 0 && strstr(rendered_text, "You gain 300 experience."));
    reading_ms = sdl_narrative_banner_timeout_ms(clock_ns);
    assert(reading_ms > 1000);
    tick(reading_ms);
    assert(banner_pixels() > 0 && strstr(rendered_text, "A vast darkness yawns below"));
    assert(active_narrative_banner_consumes_input());

    /* Dismissal and level reset discard both notices while the fade is active. */
    arm_discovery(0, true, true);
    assert(dismiss_active_narrative_banner());
    tick(10000);
    assert(!active_narrative_banner_has_followup() && banner_pixels() == 0);
    assert(sdl_narrative_banner_timeout_ms(clock_ns) == -1);
    arm_discovery(3, true, true);
    reset_level_entry_tracking();
    tick(10000);
    assert(!active_narrative_banner_has_followup() && banner_pixels() == 0);

    /* A restored character already credited for the discovery stays silent. */
    s32b experience = p_ptr->exp;
    p_ptr->restoring = true;
    handle_partition_entry(true, PARTITION_NARRATIVE_OFF);
    assert(p_ptr->exp == experience && !active_narrative_banner_visible());
    assert(!active_narrative_banner_has_followup());

    clock_ns = 0;
    puts("Discovery and description: async fade, readable pixels, modal priority, turn lifetime and dismissal PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    start = HARNESS.index("static cptr make_path")
    init = HARNESS[start:HARNESS.index("    int last = ")]
    source = OUT / "check.c"
    source.write_text(HARNESS[:start] + CHECKS + init
                      + "    check_sequence(); sdl_quit_hook(NULL); return 0;\n}\n",
                      encoding="utf-8")
    sources = ("src/dungeon/dungeon-narrative.c", "src/sdl/ui/sdl-menus.c")
    cmake = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake / "objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", *("/" + src + ".obj" for src in sources))
    response = OUT / "objects.rsp"
    response.write_text("\n".join(f'"{obj}"' for obj in objects if not obj.endswith(excluded)),
                        encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / dep) for dep in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"],
    ])
    env.update(SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIO_DRIVER="dummy",
               CONTROL_TEST_ASSETS=str(ROOT / "lib"))
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O2",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), *(str(ROOT / src) for src in sources),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-Wl,--wrap=SDL_GetTicksNS,--wrap=SDL_Delay,--wrap=sdl_touch_cancel_all_inputs,"
        "--wrap=sdl_ui_text_texture",
        "-o", str(exe),
    ], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(dir=OUT, prefix="profile-") as profile:
        subprocess.run([str(exe), "--windowed", "--tiles"], cwd=ROOT,
                       env=dict(env, CONTROL_TEST_PROFILE=profile), check=True, timeout=30)


if __name__ == "__main__":
    main()
