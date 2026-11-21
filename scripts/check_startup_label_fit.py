#!/usr/bin/env python3
"""Check production book and hero labels at phone and desktop-like sizes.

The fixture renders the real mobile screen code and checks the source texture,
not merely its destination rectangle, so a cropped string cannot pass as fit.
It also checks and activates the actual book footer hit targets. No saves or
settings are opened. Use --desktop for the Windows layout or --baseline to
demonstrate the regression against HEAD.
"""
from pathlib import Path
import json
import os
import re
import shlex
import subprocess
import sys

import check_phone_books_layout as base

ROOT = base.ROOT
BUILD = base.BUILD
OUT = ROOT / "scripts/output/startup-label-fit"

HARNESS = base.HARNESS[:base.HARNESS.index("typedef struct { char text[256];")]
HARNESS = HARNESS.replace(
    "static void record_text(TTF_Font*,cptr,SDL_FRect,float);",
    "static bool grid_recording;\nstatic void record_text(cptr,SDL_Texture*,SDL_FRect,SDL_FRect);")
HARNESS += r'''
typedef struct {
    char text[256]; SDL_FRect src, dst; float width, height; bool footer, grid;
} text_record;
static text_record records[2048];
static int record_count, issues;
static void record_text(cptr text,SDL_Texture *texture,SDL_FRect src,SDL_FRect dst)
{
    if(!recording || !text || !text[0]) return;
    fixture_assert(record_count < (int)N_ELEMENTS(records));
    text_record *r=&records[record_count++];
    *r=(text_record){.src=src,.dst=dst,.footer=footer_recording,.grid=grid_recording};
    SDL_strlcpy(r->text,text,sizeof(r->text));
    fixture_assert(SDL_GetTextureSize(texture,&r->width,&r->height));
}
static void expect(bool ok,cptr what)
{
    if(!ok) {
        ++issues;
        fprintf(stderr,"%s %dx%d density%.2f bigger%d: %s\n",fixture_id,
            fixture_width,fixture_height,fixture_density,config.bigger_font,what);
    }
}
static bool intersects(SDL_FRect a,SDL_FRect b)
{ return a.x < b.x+b.w && b.x < a.x+a.w && a.y < b.y+b.h && b.y < a.y+a.h; }
static void complete(text_record *r)
{
    if(r->src.x != 0 || r->src.y != 0 || r->src.w+0.01f < r->width
        || r->src.h+0.01f < r->height) {
        fprintf(stderr,"cropped [%s]: src %.1fx%.1f, texture %.1fx%.1f\n",
            r->text,r->src.w,r->src.h,r->width,r->height);
        expect(false,"label texture cropped");
    }
    expect(r->dst.x>=-1 && r->dst.y>=-1
        && r->dst.x+r->dst.w<=fixture_width+1
        && r->dst.y+r->dst.h<=fixture_height+1,"label outside screen");
}
static text_record *find_text(cptr text)
{
    for(int i=0;i<record_count;i++) if(!strcmp(records[i].text,text)) return &records[i];
    return NULL;
}
static void frame(void)
{
    record_count=0;recording=true;
    fixture_assert(sdl_render_current_window_frame());
    recording=false;
}
static void check_footer(void)
{
    int count=0;
    for(int i=0;i<record_count;i++) if(records[i].footer) { complete(&records[i]);++count; }
    expect(count>0,"footer has no text");
    for(int i=0;i<g_sdl_character_sheet_screen.hit_count;i++) {
        sdl_character_sheet_hit *a=&g_sdl_character_sheet_screen.hits[i];
        if(a->choice>=0 || a->rect.y<fixture_height*.6f) continue;
        expect(a->rect.x>=0 && a->rect.y>=0 && a->rect.x+a->rect.w<=fixture_width+1
            && a->rect.y+a->rect.h<=fixture_height+1,"footer target outside screen");
        for(int j=i+1;j<g_sdl_character_sheet_screen.hit_count;j++) {
            sdl_character_sheet_hit *b=&g_sdl_character_sheet_screen.hits[j];
            if(b->choice<0 && b->rect.y>=fixture_height*.6f)
                expect(!intersects(a->rect,b->rect),"footer targets overlap");
        }
        int choice=0,action=0; char key=0;
        ui_menu_click_clear_pending_hover();
        expect(sdl_character_sheet_screen_handle_pointer_button(a->rect.x+a->rect.w/2,
            a->rect.y+a->rect.h/2,UI_MENU_CLICK_PRIMARY),"footer tap unhandled");
        expect(ui_menu_click_take_action(&choice,&action) && choice==a->choice
            && action==UI_MENU_CLICK_PRIMARY,"footer tap routed to wrong choice");
        expect(Term_inkey(&key,false,true)==0,"footer tap did not wake engine");
    }
}
static void books(void)
{
    SDL_strlcpy(fixture_id,"race-book",sizeof(fixture_id));
    ui_menu_click_begin();
    sdl_character_sheet_screen_begin_select(0,"Choose your people");
    sdl_character_sheet_screen_set_select_intro(birth_intro_lore);
    sdl_character_sheet_screen_set_select_frame(birth_frame_top,birth_frame_bottom);
    for(int i=0;i<4;i++) sdl_character_sheet_screen_add_select_row(i,
        ((cptr[]){"Noldor","Sindar","Naugrim","Edain"})[i],TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_description("The peoples of Beleriand.");
    fixture_assert(sdl_character_sheet_screen_commit_select(0));
    int pages=g_sdl_character_sheet_screen.select_page_count;
    expect(pages>=2,"race book has fewer than two pages");
    for(int page=0;page<pages;page++) {
        g_sdl_character_sheet_screen.select_page=page;frame();check_footer();
        if(!config.bigger_font && page>0)
            expect(find_text("\xe2\x80\xb9 Previous page")!=NULL,"previous label missing");
        if(!config.bigger_font && page<pages-1)
            expect(find_text("Turn the page \xe2\x80\xba")!=NULL,"next label missing");
    }
    sdl_character_sheet_screen_hide();ui_menu_click_clear();

    SDL_strlcpy(fixture_id,"narrative-book",sizeof(fixture_id));
    ui_menu_click_begin();
    sdl_character_sheet_screen_begin_book("The War of the Jewels");
    sdl_character_sheet_screen_set_book_target_page_count(3);
    for(int i=0;i<8;i++) sdl_character_sheet_screen_add_book_paragraph(birth_intro_lore);
    sdl_character_sheet_screen_set_book_close_button(true);
    sdl_character_sheet_screen_set_book_close_label("Begin your journey");
    sdl_character_sheet_screen_commit_book();
    pages=g_sdl_character_sheet_screen.select_page_count;
    for(int page=0;page<pages;page++) {
        g_sdl_character_sheet_screen.select_page=page;frame();check_footer();
    }
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
}
static void tales(void)
{
    static cptr prompts[]={"[Tap to finish passage]  *  [Skip intro]",
        "[Tap to turn the page]  *  [Skip intro]",
        "[Tap to close the chronicle]"};
    SDL_strlcpy(fixture_id,"intro-chronicle",sizeof(fixture_id));
    for(int manuscript=0;manuscript<2;manuscript++) {
        fixture_assert(sdl_tale_screen_begin("The Nameless Chronicle"));
        sdl_tale_screen_set_manuscript(manuscript);
        sdl_tale_screen_add_entry("","You awaken in darkness.");
        sdl_tale_screen_set_active_entry(0,255);
        for(int i=0;i<(int)N_ELEMENTS(prompts);i++) {
            sdl_tale_screen_set_prompt(prompts[i],true,i==2);
            SDL_Rect canvas={0,0,fixture_width,fixture_height};
            record_count=0;recording=true;
            sdl_tale_screen_render_canvas(&canvas);recording=false;
            if(!config.bigger_font) {
                text_record *label=find_text(prompts[i]);
                expect(label!=NULL,"chronicle prompt missing");
                if(label) complete(label);
            }
            if(i<2) {
                SDL_FRect next=g_sdl_tale_screen.prompt_next_rect;
                SDL_FRect skip=g_sdl_tale_screen.prompt_skip_rect;
                expect(next.w>0 && skip.w>0 && !intersects(next,skip),
                    "chronicle next and skip targets invalid");
                char key=0;
                expect(sdl_tale_screen_handle_pointer(skip.x+skip.w*.5f,skip.y+skip.h*.5f)
                    && Term_inkey(&key,false,true)==0 && key==ESCAPE,
                    "chronicle skip did not work");
                expect(sdl_tale_screen_handle_pointer(next.x+next.w*.5f,next.y+next.h*.5f)
                    && Term_inkey(&key,false,true)==0 && key=='\r',
                    "chronicle continue did not work");
            }
        }
        sdl_tale_screen_hide();
    }
}
static void allocation(void)
{
    int stats[A_MAX]={0}, skills[S_MAX]={0};
    character_generated=false;character_dungeon=false;turn=0;
    SDL_strlcpy(fixture_id,"attribute-allocation",sizeof(fixture_id));
    ui_menu_click_begin();
    sdl_character_sheet_screen_show_birth_stats(stats,stats,A_GRA,5000);
    frame();check_footer();
    if(fixture_width>fixture_height) {
        if(!config.bigger_font) {
            text_record *perception=find_text("Perception");
            expect(perception!=NULL && perception->grid,"Perception skill label missing");
        }
        for(int i=0;i<record_count;i++) if(records[i].grid) complete(&records[i]);
    }
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
    SDL_strlcpy(fixture_id,"skill-allocation",sizeof(fixture_id));
    ui_menu_click_begin();
    sdl_character_sheet_screen_show_birth_skills(skills,skills,skills,S_SNG,5000);
    frame();check_footer();
    if(fixture_width>fixture_height)
        for(int i=0;i<record_count;i++) if(records[i].grid) complete(&records[i]);
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
}
/* HERO TITLES */
static void heroes(void)
{
    SDL_strlcpy(fixture_id,"hero-carousel",sizeof(fixture_id));
    for(int selected=0;selected<(int)N_ELEMENTS(hero_titles);selected++) {
        ui_menu_click_begin();
        sdl_character_sheet_screen_begin_select(selected,"Choose your hero");
        for(int i=0;i<(int)N_ELEMENTS(hero_titles);i++) {
            sdl_character_sheet_screen_add_select_row(i,hero_titles[i],TERM_WHITE,"");
            sdl_character_sheet_screen_add_select_title_candidate(hero_titles[i]," ** fair");
        }
        sdl_character_sheet_screen_set_select_title_detail(hero_titles[selected]," ** fair",TERM_WHITE);
        sdl_character_sheet_screen_set_select_description("A hero of Beleriand.\n\nRemember this tale.");
        sdl_character_sheet_screen_set_select_detail_size_hint(4,1,1);
        sdl_character_sheet_screen_add_select_detail("STR\t+3",TERM_WHITE,"");
        fixture_assert(sdl_character_sheet_screen_commit_select(selected));
        frame();
        text_record *name=find_text(hero_titles[selected]);
        text_record *power=find_text(" ** fair");
        expect(name && power,"hero name or power missing");
        if(name) complete(name);
        if(power) complete(power);
        if(name && power) expect(!intersects(name->dst,power->dst),"hero name overlaps power");
        char counter[32];strnfmt(counter,sizeof(counter),"%d/%d",selected+1,(int)N_ELEMENTS(hero_titles));
        text_record *number=find_text(counter);
        expect(number!=NULL,"hero page counter missing");
        if(number) complete(number);
        if(power && number) expect(!intersects(power->dst,number->dst),"power overlaps page counter");
        if(!config.bigger_font && name && power && number) {
            float name_cy=name->dst.y+name->dst.h*.5f;
            float power_cy=power->dst.y+power->dst.h*.5f;
            float number_cy=number->dst.y+number->dst.h*.5f;
            expect(SDL_fabsf(name_cy-power_cy)<2 && SDL_fabsf(number_cy-power_cy)<2,
                "hero name, power and counter are vertically misaligned");
            expect(number->dst.h<=power->dst.h*1.8f,
                "hero page counter is oversized beside power");
        }
        sdl_character_sheet_screen_hide();ui_menu_click_clear();
    }
}
int main(int argc,char **argv)
{
    maxima limits={0};player_type player={0};player_other options={0};object_type items[INVEN_TOTAL]={0};
    fixture_assert(argc==8);setbuf(stdout,NULL);log_set_quiet(true);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");fixture_assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS));fixture_assert(TTF_Init());
    sdl_config_set_defaults(&config);config.bigger_font=atoi(argv[7]);
    fixture_density=atof(argv[6]);g_state.system_scale=fixture_density;
    g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    for(int i=0;i<SDL_MENU_FONT_COUNT;i++)config.menu_bigger_font[i]=config.bigger_font;
    fixture_width=atoi(argv[2]);fixture_height=atoi(argv[3]);
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[1],sizeof(config.story_font2));
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;config.use_unsafe_area=true;
    player.song1=player.song2=SNG_NOTHING;
    z_info=&limits;p_ptr=&player;op_ptr=&options;inventory=items;
    g_state.window=SDL_CreateWindow("Startup labels",fixture_width,fixture_height,SDL_WINDOW_HIDDEN);fixture_assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software");fixture_assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){0,0,fixture_width,fixture_height};
    sdl_view *view=&g_views[PANE_MAIN];term_init(&view->t,80,24,256);Term_activate(&view->t);term_screen=&view->t;character_icky=1;
    for(int i=0;i<16;i++)g_state.palette[i]=(SDL_Color){angband_color_table[i][1],angband_color_table[i][2],angband_color_table[i][3],255};
    books();tales();heroes();allocation();
    printf("%dx%d density%.2f bigger%d: %s (%d issues)\n",fixture_width,fixture_height,fixture_density,config.bigger_font,issues?"FAIL":"PASS",issues);
    return issues?1:0;
}
'''


def main():
    baseline = "--baseline" in sys.argv
    desktop = "--desktop" in sys.argv
    screens = (subprocess.check_output(["git", "show", "HEAD:src/sdl/ui/sdl-screens.c"], cwd=ROOT).decode()
               if baseline else (ROOT / "src/sdl/ui/sdl-screens.c").read_text())
    for signature in ["static SDL_FRect sdl_char_sheet_draw_text_aligned(",
                      "static SDL_FRect sdl_char_sheet_draw_text_alpha("]:
        start = screens.index(signature)
        end = base.brace_end(screens, start)
        drawing = screens[start:end]
        call = "sdl_ui_render_text_texture(texture, &src, &dst);"
        assert drawing.count(call) == 1
        drawing = drawing.replace(call, "record_text(text,texture,src,dst);\n    " + call)
        screens = screens[:start] + drawing + screens[end:]
    signature = "static void sdl_char_sheet_draw_book_page_controls("
    start = screens.index(signature)
    end = base.brace_end(screens, start)
    actual = screens[start:end].replace("sdl_char_sheet_draw_book_page_controls(", "actual_book_controls(", 1)
    wrapper = '''\nstatic void sdl_char_sheet_draw_book_page_controls(TTF_Font *f,float x,float w,float y,float h,int p,int n)
{ footer_recording=true;actual_book_controls(f,x,w,y,h,p,n);footer_recording=false; }\n'''
    screens = screens[:start] + actual + wrapper + screens[end:]
    signature = "void sdl_char_sheet_draw_prompt("
    start = screens.index(signature)
    end = base.brace_end(screens, start)
    actual = screens[start:end].replace("sdl_char_sheet_draw_prompt(", "actual_draw_prompt(", 1)
    wrapper = '''\nvoid sdl_char_sheet_draw_prompt(TTF_Font *f,cptr p,float x,float y,float w,float h)
{ bool old=footer_recording;footer_recording=true;actual_draw_prompt(f,p,x,y,w,h);footer_recording=old; }\n'''
    screens = screens[:start] + actual + wrapper + screens[end:]
    signature = "void sdl_char_sheet_alloc_text("
    start = screens.index(signature)
    end = base.brace_end(screens, start)
    actual = screens[start:end].replace("sdl_char_sheet_alloc_text(", "actual_alloc_text(", 1)
    wrapper = '''\nvoid sdl_char_sheet_alloc_text(TTF_Font *f,float x,float y,float w,float h,int r,int c,int s,byte a,cptr t,bool focus)
{ bool old=grid_recording;grid_recording=true;actual_alloc_text(f,x,y,w,h,r,c,s,a,t,focus);grid_recording=old; }\n'''
    screens = screens[:start] + actual + wrapper + screens[end:]
    birth = (ROOT / "src/birth/birth-selection.c").read_text()
    constants = "\n".join(re.search(r"static const char " + name + r'\[\] =.*?";', birth, re.S).group()
                          for name in ["birth_frame_top", "birth_intro_lore", "birth_frame_bottom"])
    titles = []
    for block in re.split(r"(?m)^N:\d+:", (ROOT / "lib/edit/character.txt").read_text(encoding="utf-8"))[1:]:
        name = block.splitlines()[0]
        alt = re.search(r"(?m)^A:(.*)$", block)
        titles.append(name + (alt.group(1) if alt else ""))
    selected = [next(t for t in titles if t.startswith("Telchar,"))]
    selected += [t for t in sorted(titles, key=len, reverse=True) if t not in selected][:9]
    selected += ["Fallen " + selected[0]]
    heroes = "static cptr hero_titles[]={" + ",".join(json.dumps(t, ensure_ascii=False) for t in selected) + "};"
    OUT.mkdir(parents=True, exist_ok=True)
    harness = HARNESS
    if desktop:
        harness = harness.replace("#define SIL_SDL_MOBILE_BUILD 1", "#define SIL_SDL_MOBILE_BUILD 0")
        harness = harness.replace("bool __wrap_sdl_touch_only_device_active(void) { return true; }",
                                  "bool __wrap_sdl_touch_only_device_active(void) { return false; }")
        harness = harness.replace("g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;",
                                  "g_startup_device_class=SDL_STARTUP_DEVICE_DESKTOP;")
        harness = harness.replace("books();tales();heroes();allocation();", "books();tales();allocation();")
    variant = ("baseline-" if baseline else "") + ("desktop" if desktop else "phone")
    source = OUT / (variant + ".c")
    source.write_text(harness.replace("/* SCREENS SOURCE */", screens).replace("/* RACE CONSTANTS */", constants)
                      .replace("/* HERO TITLES */", heroes), encoding="utf-8")
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/sdl/ui/sdl-gameplay-tutorial.c.obj", "/src/sdl/ui/sdl-screens.c.obj",
                "/src/sdl/render/sdl-fonts.c.obj", "/src/sdl/input/sdl-touch-tutorial.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + obj + '"' for obj in objects if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([str(BUILD / "_deps" / dep) for dep in ["SDL", "SDL_ttf", "SDL_image", "SDL_mixer"]]
                                  + ["C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    wraps = ["tutorial_get_view", "tutorial_is_active", "tutorial_revision", "get_sdl_gameplay_tutorial_mode", "SDL_WaitEvent",
             "sdl_touch_round_layer_controls_active", "sdl_touch_round_compute_layout", "sdl_touch_thumb_current_bounds",
             "sdl_map_grid_cell_rect", "sdl_touch_only_device_active", "sdl_get_layout_screen_rect", "SDL_GetDisplayContentScale",
             "sdl_touch_tutorial_device_available", "sdl_mobile_portrait_layout_active", "ui_question_ask_overlay"]
    exe = OUT / (variant + ".exe")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), "@" + str(response),
                    "@CMakeFiles/sil-more.dir/linkLibs.rsp", "-Wl," + ",".join("--wrap=" + w for w in wraps), "-o", str(exe)], cwd=BUILD, env=env, check=True)
    failures = []
    sizes = ([(1280, 720, 1), (1920, 1080, 1.5), (2880, 1800, 1.5)] if desktop else
             [(360, 800, 1), (580, 1280, 1.5), (720, 1600, 2),
              (1080, 2340, 2.75), (800, 360, 1), (1280, 580, 1.5), (1560, 720, 2)])
    for width, height, density in sizes:
        for bigger in (0, 1):
            result = subprocess.run([str(exe), str(ROOT / "lib/xtra/font/EBGaramond-Regular.ttf"), str(width), str(height),
                str(ROOT / "lib/xtra/font/VictorMono-Medium.ttf"), str(ROOT / "lib/xtra/font/Cinzel-Medium.ttf"),
                str(density), str(bigger)], cwd=OUT, env=env, timeout=90)
            if result.returncode: failures.append(f"{width}x{height}@{density} bigger={bigger}")
    if failures: raise SystemExit("Failed: " + ", ".join(failures))


if __name__ == "__main__":
    main()
