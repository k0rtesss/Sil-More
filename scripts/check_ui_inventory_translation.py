#!/usr/bin/env python3
"""Exercise localized Inventory and Equipped through the production browser."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function
from check_qa9_browser_shortcuts import CHECKS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/ui-inventory-translation"

INSPECT = r'''
#include "ui/ui-language.h"

static int browser_height = 40, row_reserve;
static bool phone, portrait, equipped, saw_item, saw_title, saw_row_click, saw_tab_targets;
static char frame[256][256];

bool __wrap_sdl_handheld_ui_active(void) { return phone; }
bool __wrap_sdl_touch_only_device_active(void) { return phone; }
bool __wrap_sdl_mobile_portrait_layout_active(void) { return portrait; }
bool __wrap_get_sdl_compact_inventory_menus(void) { return true; }
int __wrap_sdl_touch_menu_button_reserved_rows(void) { return row_reserve; }
bool __real_ui_menu_click_take_action(int *choice, int *action);

static void check_page_hit(int col, int row, int expected)
{
    int choice, action;
    assert(ui_menu_click_has_cell(col, row));
    assert(ui_menu_click_handle_cell_action(col, row, UI_MENU_CLICK_PRIMARY));
    assert(__real_ui_menu_click_take_action(&choice, &action));
    assert(choice == expected && action == UI_MENU_CLICK_PRIMARY);
}

static void inspect_desktop_tabs(cptr row)
{
    const cptr names[] = {"Equipped", "Inventory", "Jewelry", "Supplies"};
    size_t total = 3;
    for (size_t i = 0; i < N_ELEMENTS(names); ++i)
        total += strlen(ui_text(names[i])) + 2;

    if (total > Term->wid) {
        const char *next = strchr(row, '>');
        assert(row[0] == '<' && next);
        check_page_hit(0, 0, equipped ? SUPPLY_CLICK_PAGE_SUPPLIES : SUPPLY_CLICK_PAGE_EQUIPPED);
        check_page_hit((int)(next - row), 0,
            equipped ? SUPPLY_CLICK_PAGE_INVENTORY : SUPPLY_CLICK_PAGE_JEWELRY);
    } else {
        const char *supplies = strstr(row, ui_text("Supplies"));
        assert(supplies);
        check_page_hit((int)(supplies - row), 0, SUPPLY_CLICK_PAGE_SUPPLIES);
    }
    saw_tab_targets = true;
}

static void inspect_browser(void)
{
    cptr english_title = equipped ? "Equipped" : "Inventory";
    cptr local_title = ui_text(english_title);
    cptr item = equipped ? "Longsword" : "an Arrow";
    assert(strcmp(local_title, english_title));
    bool item_in_frame = false;

    for (int y = 0; y < Term->hgt; ++y) {
        char row[256];
        assert(Term->wid < sizeof(row));
        memcpy(row, Term->scr->c[y], Term->wid);
        row[Term->wid] = '\0';
        const char *cursor = row;
        size_t remaining = Term->wid;
        while (remaining && *cursor) {
            if (SDL_StepUTF8(&cursor, &remaining) == SDL_INVALID_UNICODE_CODEPOINT) {
                fprintf(stderr, "%s %s %ux%u invalid UTF-8 at row %d: %s\n",
                    ui_language(), english_title, Term->wid, Term->hgt, y, row);
                assert(false);
            }
        }

        if (!phone && y == 0)
            inspect_desktop_tabs(row);
        saw_title |= strstr(row, local_title) != NULL;
        const char *entry = strstr(row, item);
        if (entry && y < Term->hgt - row_reserve - 1) {
            item_in_frame = true;
            int col = (int)(entry - row);
            if (ui_menu_click_has_cell(col, y)) {
                int choice, action;
                assert(ui_menu_click_handle_cell_action(col, y, UI_MENU_CLICK_PRIMARY));
                assert(__real_ui_menu_click_take_action(&choice, &action));
                saw_row_click |= choice >= SUPPLY_CLICK_ENTRY_BASE && action == UI_MENU_CLICK_PRIMARY;
            }
        }
    }
    if (!saw_item) {
        for (int y = 0; y < Term->hgt; ++y) {
            memcpy(frame[y], Term->scr->c[y], Term->wid);
            frame[y][Term->wid] = '\0';
        }
    }
    saw_item |= item_in_frame;
}
'''

LOCALES = r'''
static void check_locales(cptr output)
{
    const char *languages[] = {"ru", "ja", "ko", "zh"};
    const int viewports[][3] = {{120,40,0}, {80,24,0}, {64,24,0}, {51,16,4}, {51,10,4}};
    const int preview[] = {'!'};

    for (size_t l = 0; l < N_ELEMENTS(languages); ++l) {
        ui_set_language(languages[l]);
        for (size_t v = 0; v < N_ELEMENTS(viewports); ++v) {
            browser_width = viewports[v][0];
            browser_height = viewports[v][1];
            row_reserve = viewports[v][2];
            phone = portrait = row_reserve > 0;
            for (int page = 0; page < 2; ++page) {
                equipped = mixed_harness = page != 0;
                saw_item = saw_title = saw_row_click = saw_tab_targets = false;
                printf("%s %s at %dx%d: ", languages[l], equipped ? "Equipped" : "Inventory",
                    browser_width, browser_height);
                fflush(stdout);
                run(3, true, preview, 1, 0, equipped ? 101 : 1);
                if (!saw_title || !saw_item || !saw_row_click) {
                    fprintf(stderr, "visible title=%d item=%d item click=%d\n",
                        saw_title, saw_item, saw_row_click);
                    for (int y = 0; y < browser_height; ++y)
                        fprintf(stderr, "%d: %s\n", y, frame[y]);
                }
                assert(saw_title && saw_item && saw_row_click);
                assert(phone || saw_tab_targets);
                char path[2048];
                strnfmt(path, sizeof(path), "%s/%s-%s-%dx%d.txt", output,
                    languages[l], equipped ? "equipped" : "inventory", browser_width, browser_height);
                FILE *stream = fopen(path, "wb");
                assert(stream);
                for (int y = 0; y < browser_height; ++y)
                    fprintf(stream, "%s\n", frame[y]);
                fclose(stream);
                puts("valid UTF-8, visible translated title and clickable item, preview and free exit PASS");
            }
        }
    }
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n"
    checks = CHECKS[:CHECKS.index("static void check(void)")]
    start = checks.index("    assert(pos<count);")
    end = checks.index("    int key=keys[pos++];", start)
    checks = checks[:start] + "    assert(pos<count);\n    inspect_browser();\n" + checks[end:]
    checks = checks.replace("char __wrap_inkey(void)", INSPECT + "\nchar __wrap_inkey(void)")
    checks = checks.replace("Term->hgt!=40", "Term->hgt!=browser_height").replace(
        "Term_resize(browser_width,40)", "Term_resize(browser_width,browser_height)")
    checks = checks.replace("inventory[INVEN_WIELD].number=1;",
        'inventory[INVEN_WIELD].number=1;inventory[INVEN_WIELD].obj_note=quark_add("101");')
    checks = checks.replace("    int chosen=-999;", """    if (equipped) {
        request.page = SUPPLY_MENU_PAGE_EQUIPPED;
        request.focus_inventory_group = false;
    }
    int chosen=-999;""")
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")].replace("assert(argc==3);", "assert(argc==5);")
    init = init.replace("term_init(&test_term,80,24,256)", "term_init(&test_term,120,40,256)")
    source = OUT / "check.c"
    source.write_text(prefix + checks + LOCALES + init + """    ANGBAND_DIR_HELP = argv[3];
    assert(!init_flavor_info()); flavor_init();
    for (int k = 0; k < z_info->k_max; ++k) {
        k_info[k].x_attr = k_info[k].d_attr;
        k_info[k].x_char = k_info[k].d_char;
    }
    for (int k = 0; k < z_info->flavor_max; ++k) {
        flavor_info[k].x_attr = flavor_info[k].d_attr;
        flavor_info[k].x_char = flavor_info[k].d_char;
    }
    check_locales(argv[4]);
    ui_language_cleanup(); SDL_Quit(); return 0;
}
""", encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + path + '"' for path in objects
        if not path.endswith("/src/main.c.obj")), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / lib) for lib in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ("inkey", "message_flush", "handle_stuff", "Term_putstr", "get_check",
        "object_info_overlay_show_multi", "ui_menu_click_take_action",
        "sdl_gamepad_control_available", "steamdeck_info_key", "sdl_handheld_ui_active",
        "sdl_touch_only_device_active", "sdl_mobile_portrait_layout_active",
        "get_sdl_compact_inventory_menus", "sdl_touch_menu_button_reserved_rows")
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in wraps),
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state, str(ROOT / "lib/help"), str(OUT)],
            cwd=state, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
