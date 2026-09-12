#!/usr/bin/env python3
"""Exercise production pickup previews and question focus using built SDL objects."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/pickup-destination-preview"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include "sdl/ui/sdl-question-menu.c"
#include "cmd/item/cmd-item-core.c"
#include "ui/question.c"

static object_type held[INVEN_TOTAL];
static object_kind kinds[4];
static maxima limits;
static term test_term;
static floor_context_action pack_action = {
    .kind = FLOOR_CONTEXT_ACTION_PACK, .key = 'g', .label = "Pack", .attr = TERM_L_BLUE
};
static floor_context_action harness_action = {
    .kind = FLOOR_CONTEXT_ACTION_HARNESS, .key = 'h', .label = "Harness", .attr = TERM_L_BLUE
};
static floor_context_action quiver_action = {
    .kind = FLOOR_CONTEXT_ACTION_QUIVER, .key = 'v', .label = "Quiver", .attr = TERM_L_BLUE
};

static object_type item(int kind, int number, int storage, int volume)
{
    object_type obj = {0};
    obj.k_idx = kind;
    obj.tval = kinds[kind].tval;
    obj.sval = kinds[kind].sval;
    obj.number = number;
    obj.storage = storage;
    obj.volume = volume;
    obj.pickup_slot = -1;
    obj.ident = IDENT_KNOWN;
    return obj;
}

static void clear_items(void)
{
    memset(held, 0, sizeof(held));
    player_carried_extra_reset_store();
    player_quiver_reset_store();
    memset(p_ptr->active_ability, 0, sizeof(p_ptr->active_ability));
}

static void render_preview(pickup_destination_preview* preview, int width,
    int height, const char* path)
{
    int scroll = 0;
    sdl_question_menu_layout_info layout;
    assert(SDL_SetWindowSize(g_state.window, width, height));
    g_pane_rects[PANE_DESCRIPTION] = (SDL_Rect){0, 0, width, height};
    sdl_sync_palette();
    g_state.system_scale = 1.0f;
    config.aux_view_font_size = 20;
    ui_question_show("Pick up where?", "Choose where to put 12 arrows.",
        preview->options, preview->icons, preview->count, -1, -1, 3,
        &scroll, true, NULL, 0, false);
    assert(sdl_question_menu_layout(&layout));
    assert(layout.panel.x >= 0 && layout.panel.y >= 0);
    assert(layout.panel.x + layout.panel.w <= width + 1);
    assert(layout.panel.y + layout.panel.h <= height + 1);
    int hit = 99;
    bool in_panel = false;
    assert(!sdl_question_menu_choice_at(layout.rows[1].x + layout.rows[1].w / 2,
        layout.rows[1].y + layout.rows[1].h / 2, &hit, &in_panel));
    assert(in_panel && hit == -1);
    SDL_SetRenderDrawColor(g_state.renderer, 15, 17, 20, 255);
    assert(SDL_RenderClear(g_state.renderer));
    sdl_question_menu_render();
    SDL_Surface* surface = SDL_RenderReadPixels(g_state.renderer, NULL);
    assert(surface);
    assert(SDL_SaveBMP(surface, path));
    SDL_DestroySurface(surface);
}

int main(void)
{
    pickup_destination_preview preview = {0};
    object_type arrows;
    object_type dagger;
    int heading;
    int scroll = 0;
    setbuf(stdout, NULL);
    log_set_level(LOG_WARN);
    inventory = held;
    k_info = kinds;
    z_info = &limits;
    limits.k_max = N_ELEMENTS(kinds);
    k_name = "\0& Arrow~\0& Dagger~\0& Cloak~\0";
    kinds[1].name = 1; kinds[1].tval = TV_ARROW;
    kinds[2].name = 10; kinds[2].tval = TV_SWORD; kinds[2].sval = SV_DAGGER;
    kinds[3].name = 20; kinds[3].tval = TV_CLOAK;
    for (int i = 1; i < 4; i++) kinds[i].aware = true;
    arrows = item(1, 12, OBJECT_STORAGE_PACK, 12);
    dagger = item(2, 1, OBJECT_STORAGE_HARNESS, 10);

    clear_items();
    held[0] = arrows;
    held[1] = item(3, 1, OBJECT_STORAGE_PACK, 50);
    held[INVEN_OUTER] = item(3, 1, OBJECT_STORAGE_PACK, 70);
    heading = floor_context_pickup_preview_destination(&preview, &pack_action, &arrows);
    assert(heading == 0 && preview.count == 2);
    assert(strstr(preview.labels[0], "6.2/26.0 qt") != NULL);
    assert(strstr(preview.labels[0], "19.8 free") != NULL);
    assert(strstr(preview.labels[0], "12 arrows in Pack") != NULL);
    assert(preview.icons[1] == &held[0]);
    p_ptr->active_ability[S_WIL][WIL_CON] = true;
    memset(&preview, 0, sizeof(preview));
    floor_context_pickup_preview_destination(&preview, &pack_action, &arrows);
    assert(strstr(preview.labels[0], "6.2/32.0 qt") != NULL);
    puts("PASS: Pack capacity includes other items, excludes worn apparel, and honors Constitution.");

    clear_items();
    object_type stored_arrows = item(1, 7, OBJECT_STORAGE_PACK, 12);
    assert(player_quiver_absorb_arrow(&stored_arrows) == 7);
    held[INVEN_QUIVER1] = item(1, 3, OBJECT_STORAGE_PACK, 12);
    held[INVEN_QUIVER1].pickup_slot = INVEN_QUIVER1;
    memset(&preview, 0, sizeof(preview));
    floor_context_pickup_preview_destination(&preview, &quiver_action, &arrows);
    assert(preview.count == 3);
    assert(strstr(preview.labels[0], "10/48 arrows (38 free)") != NULL);
    assert(preview.icons[1] == &held[INVEN_QUIVER1]);
    puts("PASS: Quiver lists mixed store and transient legacy slot.");

    clear_items();
    held[INVEN_WIELD] = dagger;
    object_type extra = item(2, 2, OBJECT_STORAGE_HARNESS, 10);
    assert(player_carried_extra_load(&extra));
    p_ptr->active_ability[S_PER][PER_GRA] = true;
    memset(&preview, 0, sizeof(preview));
    floor_context_pickup_preview_destination(&preview, &harness_action, &dagger);
    assert(preview.count == 3);
    assert(strstr(preview.labels[0], "3.0/27.0 qt") != NULL);
    assert(preview.icons[1] == &held[INVEN_WIELD]);
    assert(preview.icons[2] == player_carried_extra_entry_at(0));
    heading = floor_context_pickup_preview_destination(&preview, &pack_action, &dagger);
    assert(heading == 3 && preview.count == 5);
    assert(strcmp(preview.labels[4], "Empty") == 0);
    assert(ui_question_next_enabled(preview.options, preview.count, 0, 1) == 3);
    assert(ui_question_next_enabled(preview.options, preview.count, 3, 1) == 0);
    assert(ui_question_next_enabled(preview.options, preview.count, 0, -1) == 3);

    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));
    assert(TTF_Init());
    sdl_config_set_defaults(&config);
    g_state.window = SDL_CreateWindow("Pickup preview check", 800, 600, SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer = SDL_CreateRenderer(g_state.window, NULL);
    assert(g_state.renderer);
    g_pane_rects[PANE_DESCRIPTION] = (SDL_Rect){0, 0, 800, 600};
    assert(term_init(&test_term, 80, 24, 256) == 0);
    Term_activate(&test_term);
    angband_term[0] = &test_term;
    ui_question_show("Pick up where?", "Choose where to put a dagger.",
        preview.options, preview.icons, preview.count, -1, -1, heading,
        &scroll, true, NULL, 0, false);
    assert(g_question_menu.count == preview.count);
    for (int i = 0; i < preview.count; i++)
    {
        assert(g_question_menu.entries[i].choice == (preview.options[i].disabled ? -1 : i));
        assert(g_question_menu.entries[i].text_attr == preview.options[i].attr);
    }
    puts("PASS: Equipped/extra Harness contents, Grace, empty Pack, and heading-only focus.");

    const char* keys[] = {"g", "h", "2\r", "8\r", "\033"};
    const int expected[] = {3, 0, 3, 3, -1};
    for (int i = 0; i < 5; i++)
    {
        Term_flush();
        inkey_next_set(keys[i]);
        assert(ui_question_ask_aux("Pick up where?", "Choose destination.",
            preview.options, preview.icons, preview.count, -1, -1, 0,
            false, NULL, 0, false) == expected[i]);
        inkey_next_set(NULL);
    }
    puts("PASS: Direct keys, up/down plus Enter, and Escape return correct heading/cancel values.");

    clear_items();
    held[0] = arrows;
    held[1] = item(3, 1, OBJECT_STORAGE_PACK, 50);
    object_type first = item(1, 7, OBJECT_STORAGE_PACK, 12);
    assert(player_quiver_absorb_arrow(&first) == 7);
    held[INVEN_QUIVER1] = item(1, 3, OBJECT_STORAGE_PACK, 12);
    held[INVEN_QUIVER1].pickup_slot = INVEN_QUIVER1;
    memset(&preview, 0, sizeof(preview));
    floor_context_pickup_preview_destination(&preview, &quiver_action, &arrows);
    floor_context_pickup_preview_destination(&preview, &pack_action, &arrows);
    render_preview(&preview, 1100, 650, "scripts/output/pickup-destination-preview/landscape.bmp");
    render_preview(&preview, 480, 800, "scripts/output/pickup-destination-preview/portrait.bmp");
    puts("PASS: Portrait and landscape production overlays rendered within window bounds.");

    clear_items();
    memset(&preview, 0, sizeof(preview));
    floor_context_pickup_preview_destination(&preview, &quiver_action, &arrows);
    assert(strcmp(preview.labels[1], "No arrows") == 0);
    floor_context_pickup_preview_destination(&preview, &pack_action, &arrows);
    assert(strcmp(preview.labels[3], "No arrows") == 0);
    assert(strstr(preview.labels[2], "0 arrows in Pack") != NULL);
    for (int i = 0; i < PICKUP_PREVIEW_STACKS + 3; i++)
        assert(player_carried_extra_load(&dagger));
    memset(&preview, 0, sizeof(preview));
    floor_context_pickup_preview_destination(&preview, &harness_action, &dagger);
    assert(preview.count == PICKUP_PREVIEW_STACKS + 2);
    assert(strcmp(preview.labels[preview.count - 1], "3 more stacks") == 0);
    floor_context_pickup_preview_destination(&preview, &pack_action, &dagger);
    assert(preview.count <= PICKUP_PREVIEW_ROWS);
    puts("PASS: Empty arrow destinations and explicit bounded overflow rows.");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    # externs.h has no include guard; the SDL private header already loads it.
    harness = HARNESS
    for path in ("cmd/item/cmd-item-core.c", "ui/question.c"):
        code = (ROOT / "src" / path).read_text(encoding="utf-8")
        code = code.replace('#include "externs.h"', '')
        harness = harness.replace(f'#include "{path}"', code)
    source.write_text(harness, encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    exclude = ("/src/main.c.obj", "/src/cmd/item/cmd-item-core.c.obj",
               "/src/ui/question.c.obj", "/src/sdl/ui/sdl-question-menu.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects
                                  if not p.endswith(exclude)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *[str(BUILD / "_deps" / name) for name in
          ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")], env["PATH"]])
    env["SDL_VIDEO_DRIVER"] = "dummy"
    env["SDL_RENDER_DRIVER"] = "software"
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
        "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
