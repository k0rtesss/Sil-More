#!/usr/bin/env python3
"""Render real SDL tutorial cards offscreen using the game's fonts/text code.

No player file or configuration is loaded. Catalogue-derived views are injected
at the renderer boundary; this checks pixels/layout, not physical input.
"""
from pathlib import Path
import json
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-render-check"

HARNESS = r'''
#include "angband.h"
#include <assert.h>
#include "cJSON.h"
#include "sdl/ui/sdl-gameplay-tutorial.c"
static tutorial_view fixture;
static tutorial_mode fixture_mode=TUTORIAL_MODE_EXTENDED;
tutorial_mode __wrap_get_sdl_gameplay_tutorial_mode(void) { return fixture_mode; }
bool __wrap_tutorial_get_view(tutorial_view *out) { *out=fixture; return fixture.active; }
bool __wrap_tutorial_is_active(void) { return fixture.active; }
unsigned int __wrap_tutorial_revision(void) { return 1; }

static void field(cJSON *obj,const char *key,char *dst,size_t size)
{
    cJSON *value=cJSON_GetObjectItemCaseSensitive(obj,key);
    SDL_strlcpy(dst,cJSON_IsString(value)?value->valuestring:"",size);
}

static void paint(cJSON *entry,int width,int height,int index,int scroll)
{
    char file[80];
    SDL_Surface *pixels;
    sdl_view *view=&g_views[PANE_MAIN];
    g_state.window=SDL_CreateWindow("Tutorial render fixture",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");
    assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    term_init(&view->t,width/8,height/16,256);
    Term_activate(&view->t); term_screen=&view->t;
    view->term_ready=true; view->rect=(SDL_Rect){0,0,width,height};
    view->cell_w=8; view->cell_h=16; view->cols=width/8; view->rows=height/16;
    view->canvas=SDL_CreateTexture(g_state.renderer,SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,width,height);
    fixture=(tutorial_view){.active=true,.step=1,.step_count=3,.revision=1};
    field(entry,"title",fixture.title,sizeof(fixture.title));
    field(entry,"body",fixture.body,sizeof(fixture.body));
    field(entry,"action",fixture.action,sizeof(fixture.action));
    field(entry,"subject",fixture.action_subject,sizeof(fixture.action_subject));
    field(entry,"anchor",fixture.anchor,sizeof(fixture.anchor));
    cJSON *mode=cJSON_GetObjectItemCaseSensitive(entry,"mode");
    fixture_mode=cJSON_IsNumber(mode)?mode->valueint:TUTORIAL_MODE_EXTENDED;
    fixture.can_continue=!fixture.action[0];
    cJSON *input=cJSON_GetObjectItemCaseSensitive(entry,"input");
    tutorial_last_input=cJSON_IsNumber(input)?input->valueint:TUTORIAL_INPUT_KEYBOARD;
    config.input_ui_mode=tutorial_last_input==TUTORIAL_INPUT_CONTROLLER
        ?SDL_INPUT_UI_MODE_CONTROLLER:SDL_INPUT_UI_MODE_AUTO;
    fixture.kind=fixture.can_continue?TUTORIAL_STEP_INFO:TUTORIAL_STEP_ACTION;
    character_icky=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"menu"));
    sdl_gameplay_tutorial_set_menu_preview(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"preview")),false);
    sdl_gameplay_tutorial_set_menu_preview_control("View");
    ui_menu_click_clear();
    if (character_icky) {
        ui_menu_click_begin();
        for (int row=2;row<MIN(view->rows-2,10);++row)
            ui_menu_click_add_full_row(row,row);
    }
    tutorial_seen_revision=1; tutorial_was_active=true;
    tutorial_focus=0; tutorial_scroll=scroll;
    tutorial_reading=scroll>0 && !fixture.can_continue;
    SDL_SetRenderDrawColor(g_state.renderer,34,38,39,255);
    SDL_RenderClear(g_state.renderer);
    SDL_SetRenderDrawColor(g_state.renderer,56,61,57,255);
    for (int x=0;x<width;x+=32) SDL_RenderLine(g_state.renderer,x,0,x,height);
    for (int y=0;y<height;y+=32) SDL_RenderLine(g_state.renderer,0,y,width,y);
    sdl_touch_tutorial_draw_text_line("LIVE GAME / MENU FIXTURE",12,12,width-24,18,
        (SDL_Color){180,195,177,255},false);
    sdl_gameplay_tutorial_render();
    assert(tutorial_card.x>=0 && tutorial_card.y>=0);
    assert(tutorial_card.x+tutorial_card.w<=width);
    assert(tutorial_card.y+tutorial_card.h<=height);
    for (int i=0;i<3;++i) {
        if (!tutorial_buttons[i].w) { assert(i==0); continue; }
        assert(tutorial_buttons[i].h>=44);
        assert(tutorial_buttons[i].x>=tutorial_card.x);
        assert(tutorial_buttons[i].x+tutorial_buttons[i].w<=tutorial_card.x+tutorial_card.w);
    }
    pixels=SDL_RenderReadPixels(g_state.renderer,NULL);
    assert(pixels);
    SDL_snprintf(file,sizeof(file),"card-%d-%dx%d-%s.bmp",index,width,height,scroll?"end":"start");
    assert(SDL_SaveBMP(pixels,file));
    SDL_DestroySurface(pixels);
    printf("%s: card %.0fx%.0f; scroll %d/%d\n",file,tutorial_card.w,tutorial_card.h,tutorial_scroll,tutorial_max_scroll);
    sdl_story_font_cache_clear();
    SDL_DestroyTexture(view->canvas); view->canvas=NULL;
    term_nuke(&view->t); view->term_ready=false; term_screen=NULL; Term=NULL;
    SDL_DestroyRenderer(g_state.renderer); g_state.renderer=NULL;
    SDL_DestroyWindow(g_state.window); g_state.window=NULL;
}

int main(int argc,char **argv)
{
    size_t length;
    char *data;
    maxima limits={0};
    player_type player={0};
    assert(argc==2);
    setbuf(stdout,NULL);
    log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));
    assert(TTF_Init());
    sdl_config_set_defaults(&config);
    SDL_strlcpy(config.story_font,argv[1],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    config.use_unsafe_area=true;
    z_info=&limits; p_ptr=&player;
    p_ptr->playing=true; p_ptr->py=7;p_ptr->px=8;
    p_ptr->cur_map_hgt=32;p_ptr->cur_map_wid=64;
    character_generated=character_dungeon=true;
    data=SDL_LoadFile("fixtures.json",&length); assert(data);
    cJSON *entries=cJSON_Parse(data); assert(entries); SDL_free(data);
    for (int i=0;i<cJSON_GetArraySize(entries);++i) {
        cJSON *entry=cJSON_GetArrayItem(entries,i);
        paint(entry,1280,720,i,0);
        paint(entry,320,480,i,0);
        if (cJSON_GetObjectItemCaseSensitive(entry,"action")->valuestring[0])
            paint(entry,320,240,i,0);
        paint(entry,320,240,i,1000);
    }
    cJSON_Delete(entries); TTF_Quit(); SDL_Quit();
    puts("Gameplay tutorial software-render fixtures: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    catalogue = json.loads((ROOT / "lib/help/tutorials.json").read_text())
    lessons = catalogue["lessons"]
    opening = next(lesson for lesson in lessons if lesson["id"] == "opening.move")
    longest = max(((lesson, step) for lesson in lessons for step in lesson["steps"]),
                  key=lambda pair: len(pair[1]["text"]))
    selections = [(opening, opening["steps"][0], False, 1, False),
                  (opening, next(step for step in opening["steps"] if step.get("action")), False, 4, False),
                  (longest[0], longest[1], True, 1, False)]
    description = next(lesson for lesson in lessons if lesson["id"] == "item.first_description")
    preview_step = next(step for step in description["steps"] if step.get("action") == "examine")
    selections.extend((description, preview_step, True, device, True) for device in range(1, 5))
    fixtures = []
    for lesson, step, menu, device, preview in selections:
        body = step["text"].replace("{subject}", "The selected item")
        body = body.replace("{context}", "Read the current requirements and costs before choosing.")
        body = body.replace("{detail}", "Read the current requirements and costs before choosing.")
        fixtures.append({"title": lesson["title"], "body": body,
                         "action": step.get("action", ""),
                         "subject": step.get("subject_type", ""),
                         "anchor": "inventory" if preview else "abilities" if menu else step.get("anchor", "none"),
                         "menu": menu, "input": device, "preview": preview})
    for mode in (0, 1):
        fixtures.append(dict(fixtures[0], mode=mode))
    (OUT / "fixtures.json").write_text(json.dumps(fixtures), encoding="utf-8")
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
                "/src/sdl/ui/sdl-main-menu.c.obj", "/src/sdl-config.c.obj",
                "/src/tutorial/tutorial.c.obj")
    objects = [obj for obj in objects if not obj.endswith(excluded)]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin"] +
        [str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]] + [env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    str(ROOT / "src/sdl/ui/sdl-main-menu.c"), str(ROOT / "src/sdl-config.c"),
                    str(ROOT / "src/tutorial/tutorial.c"), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=tutorial_get_view,--wrap=tutorial_is_active,--wrap=tutorial_revision,--wrap=get_sdl_gameplay_tutorial_mode",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf")],
                   cwd=OUT, env=env, check=True, timeout=60)


if __name__ == "__main__":
    main()
