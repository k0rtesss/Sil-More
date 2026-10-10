#!/usr/bin/env python3
"""Check production mobile menu sizes with independent UI font profiles."""
from pathlib import Path
import os
import re
import shlex
import subprocess
from check_bigger_font_profiles import HARNESS as PROFILE_HARNESS

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/menu-font-sizes"

STARTUP_FONT_APPLY = re.search(
    r'        if \(g_startup_device_class == SDL_STARTUP_DEVICE_MOBILE_TOUCH\) \{\n'
    r'            set_sdl_all_bigger_fonts\(startup_bigger_font\);.*?\n        \}',
    (ROOT / "src/sdl/core/sdl-init.c").read_text(), re.S).group(0)
CHECK = "static void apply_startup_font_choice(bool startup_bigger_font) {\n" + STARTUP_FONT_APPLY + "\n}\n" + r"""

static int startup_answer, startup_prompts;
static bool startup_prompt_failure, startup_tablet;
bool __wrap_SDL_IsTablet(void) { return startup_tablet; }
bool __wrap_SDL_ShowMessageBox(const SDL_MessageBoxData *box,int *choice) {
    startup_prompts++;
    assert(box->numbuttons==2);
    if(!strcmp(box->title,"Choose Font Size")) {
        assert(strstr(box->buttons[0].text,"recommended"));
        assert(box->buttons[0].flags&SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT);
        assert(box->buttons[0].flags&SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT);
        assert(strstr(box->message,"less beautiful") && strstr(box->message,"difficult"));
        assert(strstr(box->message,"General Settings > Big Font"));
        assert(strstr(box->message,"one menu at a time"));
        assert(strstr(box->message,"B button") && strstr(box->message,"real time"));
        assert(strstr(box->message,"Normal font starts with B hidden"));
        assert(strstr(box->message,"turn B on or off"));
    } else assert(!strcmp(box->title,"Choose Orientation"));
    if(startup_prompt_failure) return false;
    *choice=startup_answer; return true;
}
static void check_first_start_choices(void) {
    g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    assert(!config.show_menu_font_button);
    startup_answer=0; assert(!sdl_prompt_mobile_startup_portrait_mode());
    assert(!sdl_prompt_mobile_startup_bigger_font());
    set_sdl_show_menu_font_button(true);
    apply_startup_font_choice(false);
    assert(!config.bigger_font && !config.show_menu_font_button);
    for(int i=0;i<SDL_MENU_FONT_COUNT;i++) assert(!config.menu_bigger_font[i]);
    startup_answer=1; assert(sdl_prompt_mobile_startup_portrait_mode());
    bool big=sdl_prompt_mobile_startup_bigger_font(); assert(big);
    apply_startup_font_choice(big);
    assert(config.bigger_font && config.show_menu_font_button);
    for(int i=0;i<SDL_MENU_FONT_COUNT;i++) assert(config.menu_bigger_font[i]);
    set_sdl_all_bigger_fonts(false);
    assert(config.show_menu_font_button); /* Later All fonts changes preserve visibility. */
    startup_tablet=true;
    assert(!sdl_prompt_mobile_startup_bigger_font());
    apply_startup_font_choice(false);
    assert(!config.bigger_font && !config.show_menu_font_button);
    startup_tablet=false;
    for(int device=SDL_STARTUP_DEVICE_DESKTOP;device<SDL_STARTUP_DEVICE_MOBILE_TOUCH;device++) {
        g_startup_device_class=device;
        assert(!sdl_prompt_mobile_startup_bigger_font());
    }
    g_startup_device_class=SDL_STARTUP_DEVICE_MOBILE_TOUCH;
    startup_prompt_failure=true; assert(!sdl_prompt_mobile_startup_bigger_font());
    apply_startup_font_choice(false);
    assert(!config.show_menu_font_button);
    startup_prompt_failure=false;
    assert(startup_prompts==5);
    set_sdl_all_bigger_fonts(false);
    puts("First-start fonts: touch phone only, tablet/controller/desktop skipped, normal/B hidden, big/B shown, explanatory copy, failure fallback: PASS");
}

static void check_menu_sizes(char **argv) {
    defaults();
    character_icky=1; /* Match an open menu; no live dungeon is updated. */
    assert(messages_init()==0);
    config.use_unsafe_area=true;
    config.input_ui_mode=SDL_INPUT_UI_MODE_PLATFORM;
    check_first_start_choices();
    SDL_strlcpy(config.monospace_font,argv[4],sizeof(config.monospace_font));
    SDL_strlcpy(config.story_font,argv[5],sizeof(config.story_font));
    SDL_strlcpy(config.story_font2,argv[6],sizeof(config.story_font2));
    g_state.system_scale=density;
    g_state.window=SDL_CreateWindow("Independent menu size fixture",width,height,SDL_WINDOW_HIDDEN);
    assert(g_state.window);
    g_state.renderer=SDL_CreateRenderer(g_state.window,"software"); assert(g_state.renderer);
    g_state.safe_area=(SDL_Rect){0,0,width,height};
    g_suppress_layout_refresh_present=true;
    set_sdl_mobile_portrait_mode(height>width);
    sdl_refresh_platform_max_main_view_scales_for_current_layout("fixture startup");
    for(int i=0;i<SDL_PANE_PROFILE_COUNT;i++)
        g_pane_profiles[i].aux_view_font_size=i<SDL_PANE_ORIENTATION_PROFILE_COUNT?24:8;
    /* Stay inside a reading surface, as in Options, while testing font choices;
     * closing the entire UI may legitimately recover a different gameplay grid. */
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_HELP);
    for(int mode=0;mode<SDL_MIN_TERMINAL_MODE_COUNT;mode++) {
        set_sdl_min_terminal_mode(mode);
        config.terminal_menu_scale_offset=0;
        int normal_h=0,normal_w=0,popup_px=0;
        for(int ui=0;ui<2;ui++) {
            set_sdl_bigger_font(ui);
            sdl_apply_stored_pane_profile(mode);
            sdl_apply_config_no_redraw();
            for(int menu=0;menu<SDL_MENU_FONT_COUNT;menu++) set_sdl_menu_bigger_font(menu,false);
            screen_push_supporting_panes_hidden();
            sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_INVENTORY);
            sdl_view *view=&g_views[PANE_MAIN];
            if(!ui) {normal_h=view->cell_h;normal_w=view->cell_w;}
            assert(view->cell_h==normal_h && view->cell_w==normal_w);
            sdl_pop_terminal_menu_scale();
            set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY,true);
            sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_INVENTORY);
            set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY,false);
            sdl_apply_config_no_redraw(); /* The B button's terminal redraw. */
            if(view->cell_h!=normal_h)
                fprintf(stderr,"%dx%d UI%d mode%d normal cell%d -> after B cell%d actualmode%d override%d target%d depth%d font%d\n",
                    width,height,ui,mode,normal_h,view->cell_h,config.min_terminal_mode,g_terminal_menu_scale_override,get_sdl_terminal_menu_scale(),g_terminal_menu_scale_depth,sdl_terminal_menu_font());
            assert(view->cell_h==normal_h && view->cell_w==normal_w);
            /* Updating an inactive parent while a child is open also restores the new size. */
            set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY,true);
            sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_HELP);
            set_sdl_menu_bigger_font(SDL_MENU_FONT_INVENTORY,false);
            sdl_pop_terminal_menu_scale();
            assert(view->cell_h==normal_h && view->cell_w==normal_w);
            sdl_pop_terminal_menu_scale();
            screen_pop_supporting_panes_hidden();
            int previous=sdl_menu_font_set_render_context(SDL_MENU_FONT_MAIN);
            int px=sdl_main_menu_pane_font_px();
            if(!ui) popup_px=px;
            if(px!=popup_px)
                fprintf(stderr,"Popup %dx%d mode%d actual_mode%d ui%d normal%d actual%d aux%d base%d portrait%d\n",
                    width,height,mode,config.min_terminal_mode,ui,popup_px,px,config.aux_view_font_size,
                    sdl_menu_base_aux_font_size(),config.mobile_portrait_mode);
            assert(px==popup_px); /* Big UI's 8px pane profile cannot shrink this 24px menu. */
            sdl_menu_font_set_render_context(previous);
        }
        /* Auto popup text follows the same normal profile, independent of the active UI map scale. */
        for(int i=0;i<SDL_PANE_PROFILE_COUNT;i++) g_pane_profiles[i].aux_view_font_size=0;
        int previous=sdl_menu_font_set_render_context(SDL_MENU_FONT_MAIN);
        set_sdl_bigger_font(false); sdl_apply_stored_pane_profile(mode);
        int normal_main=config.main_view_scale;
        int normal_px=sdl_main_menu_pane_font_px();
        set_sdl_bigger_font(true); config.main_view_scale=SDL_MAIN_VIEW_MIN_SCALE;
        assert(sdl_menu_auto_font_size_from_main(4,5)==MAX(8,MIN(48,
            ((int)(normal_main*TILE_SIZE/g_state.system_scale+.5f)*4+4)/5))
            || normal_main*TILE_SIZE/g_state.system_scale<=8);
        assert(sdl_main_menu_pane_font_px()==normal_px);
        sdl_menu_font_set_render_context(previous);
        for(int i=0;i<SDL_PANE_PROFILE_COUNT;i++)
            g_pane_profiles[i].aux_view_font_size=i<SDL_PANE_ORIENTATION_PROFILE_COUNT?24:8;
    }
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    printf("Normal menus %dx%d @%.2f: UI off/on, large-to-normal B, nested restore, explicit/auto popup pixels: PASS\n",width,height,density);
}
"""
HARNESS = PROFILE_HARNESS.replace("int main(int argc,char **argv) {",CHECK+"\nint main(int argc,char **argv) {")
HARNESS = HARNESS.replace("check_persistence(); check_layout(argv);", "check_menu_sizes(argv);")

def main():
    startup=(ROOT/"src/sdl/core/sdl-init.c").read_text()
    begin=startup.index("if (!config_exists) {")
    opening=startup.index("{",begin); end=opening+1; depth=1
    while depth:
        depth+=(startup[end]=="{")-(startup[end]=="}"); end+=1
    assert "sdl_prompt_mobile_startup_bigger_font()" in startup[begin:end]
    assert startup.count("sdl_prompt_mobile_startup_bigger_font()") == 1
    OUT.mkdir(parents=True, exist_ok=True)
    source=OUT/"check.c"
    source.write_text(HARNESS,encoding="utf-8")
    mobile_sources=["src/sdl/core/sdl-state.c", "src/sdl/core/sdl-layout.c",
        "src/sdl/config/sdl-settings.c", "src/sdl/render/sdl-fonts.c",
        "src/sdl-config.c", "src/sdl/input/sdl-touch-controls.c",
        "src/sdl/ui/sdl-panes.c", "src/sdl/ui/sdl-main-menu.c", "src/sdl/ui/sdl-menu-font.c"]
    objects=shlex.split((BUILD/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded=tuple("/"+p+".obj" for p in mobile_sources)+("/src/main.c.obj",)
    response=OUT/"objects.rsp"
    response.write_text("\n".join('"'+o+'"' for o in objects if not o.endswith(excluded)),encoding="utf-8")
    env=os.environ.copy()
    env["PATH"]=os.pathsep.join([str(BUILD/"_deps"/d) for d in ["SDL","SDL_ttf","SDL_image","SDL_mixer"]]
        +["C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    wraps=["SDL_ShowMessageBox","SDL_IsTablet","SDL_GetDisplayContentScale","sdl_touch_only_device_active","sdl_layout_matches_supporting_pane_visibility",
        "prt_frame_basic","sdl_depth_menu_pane_label","player_current_movement_energy","player_current_movement_speed"]
    exe=OUT/"check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-DSIL_IOS","-std=c17","-O0","-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",str(source)]+[str(ROOT/p) for p in mobile_sources]
        +["@"+str(response),"@CMakeFiles/sil-more.dir/linkLibs.rsp","-Wl,"+",".join("--wrap="+w for w in wraps),"-o",str(exe)],
        cwd=BUILD,env=env,check=True)
    for w,h,scale in [(360,800,1),(800,360,1),(580,1280,1.5),(1280,580,1.5),(1080,2400,2.75),(2400,1080,2.75)]:
        subprocess.run([str(exe),str(w),str(h),str(scale),str(ROOT/"lib/xtra/font/VictorMono-Medium.ttf"),
            str(ROOT/"lib/xtra/font/Cinzel-Medium.ttf"),str(ROOT/"lib/xtra/font/EBGaramond-Regular.ttf")],
            cwd=OUT,env=env,check=True,timeout=60)

if __name__=="__main__":
    main()
