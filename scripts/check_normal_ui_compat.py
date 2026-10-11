#!/usr/bin/env python3
"""Compare actual normal UI pixels and hit rectangles to pre-big-font sources.

Compiles the pinned historical UI into an isolated fixture. No user saves or
settings are opened. Covers phone portrait/landscape and desktop render paths,
including returning from big font with populated shared font/texture caches.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

import check_normal_touch_compat as base

ROOT, BUILD, BEFORE = base.ROOT, base.BUILD, base.BEFORE
OUT = ROOT / "scripts/output/normal-ui-compat"
DESKTOP = "--desktop" in sys.argv
if DESKTOP:
    OUT = ROOT / "scripts/output/normal-ui-compat-desktop"
UI = ["src/sdl/ui/" + name + ".c" for name in (
    "sdl-screens", "sdl-halls-screen", "sdl-main-menu", "sdl-question-menu",
    "sdl-song-menu", "sdl-pane-menus", "sdl-hint-quest-menu")]
UI += ["src/sdl/input/sdl-tooltips.c", "src/sdl/input/sdl-touch-tutorial.c"]
MODULES = base.MODULES
INCLUDED = base.INCLUDED + UI
includes = "\n#ifdef ORIGINAL\n" + "\n".join(
    '#include "old-' + Path(p).name + '"' for p in UI)
includes += "\n#else\n" + "\n".join(
    '#include "' + p.removeprefix("src/") + '"' for p in UI) + "\n#endif\n"
HARNESS = base.HARNESS[:base.HARNESS.index("static void grid(void)")]
HARNESS += includes + r'''
#ifdef ORIGINAL
void sdl_character_sheet_screen_set_select_confirm_label(cptr label) { }
bool sdl_character_sheet_screen_debug_turn_page(int dir) { return false; }
bool sdl_standalone_screen_handle_key(int key) { return false; }
bool sdl_standalone_screen_handle_pointer(float x,float y,int action) { return false; }
#endif
bool __wrap_sdl_mobile_lifecycle_handle_event(const SDL_Event *ev) { return false; }
SDL_Rect __wrap_sdl_get_layout_screen_rect(void) { return (SDL_Rect){0,0,width,height}; }
bool __wrap_sdl_overlay_pane_anchor_rect(int pane,SDL_Rect *r) {
    *r=(SDL_Rect){0,0,width,height}; return true;
}
static void record(const char *id,const SDL_FRect *hits,int count) {
    unsigned long long pixels=capture(id);
    printf("{\"id\":\"%s\",\"pixels\":\"%016llx\",\"hits\":[",id,pixels);
    for(int i=0;i<count;i++){if(i)printf(",");rect(hits[i]);} printf("]}\n");
}
static void record_character(const char *id,SDL_Rect *canvas) {
    SDL_FRect hits[256];clear();sdl_character_sheet_screen_render_canvas(canvas);
    int n=g_sdl_character_sheet_screen.hit_count;
    for(int i=0;i<n;i++)hits[i]=g_sdl_character_sheet_screen.hits[i].rect;
    record(id,hits,n);
}
static void screens(void) {
    SDL_Rect canvas={0,0,width,height}; SDL_FRect hits[256]; char id[80];
    pane_config_count=0; g_direct_touch_present=true;
    config.touch_top_panel_arrows_visible=false;
    for(int style=INTRO_STYLE_FLAME;style<INTRO_STYLE_MAX;style++) {
        assert(sdl_welcome_screen_show_intro(style,false)); clear();
        sdl_welcome_render_intro_canvas(&canvas);
        strnfmt(id,sizeof(id),"intro-%d",style);record(id,NULL,0);
    }
    for(int wizard=0;wizard<2;wizard++)for(int fresh=0;fresh<2;fresh++) {
        assert(sdl_welcome_screen_show_menu(wizard,fresh));clear();
        sdl_welcome_render_intro_canvas(&canvas);sdl_welcome_render_menu_footer_canvas(&canvas);
        hits[0]=g_sdl_welcome_screen.continue_rect;hits[1]=g_sdl_welcome_screen.quit_rect;
        strnfmt(id,sizeof(id),"welcome-%d-%d",wizard,fresh);record(id,hits,2);
    }
    sdl_welcome_screen_hide();
    for(int full=0;full<2;full++)for(int actions=4;actions<=7;actions++) {
        sdl_halls_screen_begin("Here are remembered the fates of those who entered Angband.",
            "Score order | page 2 of 3",full,-1);
        int capacity=sdl_halls_screen_page_capacity(full);
        for(int i=0;i<capacity;i++)sdl_halls_screen_add_entry(i,"1","Aglar the Bold",
            "Score: 123,456","Slain by an orc warrior in the depths of Angband.",
            "12,345 turns | deepest descent 950 ft | 2026-10-06","Honourable",TERM_YELLOW,
            "Score increases: treasure and exploration","Score decreases: none",TERM_WHITE,i==0);
        const char *labels[]={"Back","Run History","Order: Score","View: Full","Open Hero","Previous","Next"};
        for(int i=0;i<actions;i++)sdl_halls_screen_add_action(-1-i,labels[i],TERM_WHITE,true);
        clear();sdl_halls_screen_render();int n=0;
        for(int i=0;i<capacity;i++)hits[n++]=g_sdl_halls.entries[i].hit_rect;
        for(int i=0;i<actions;i++)hits[n++]=g_sdl_halls.actions[i].hit_rect;
        strnfmt(id,sizeof(id),"halls-%d-%d",full,actions);record(id,hits,n);sdl_halls_screen_hide();
    }
    for(int help=0;help<3;help++)for(int context=0;context<2;context++) {
        sdl_question_menu_begin("Choose your next action");
        if(context)sdl_question_menu_set_context_hint();
        if(help)sdl_question_menu_set_help("Read the complete description before choosing your action. Keep supplies ready for the journey.");
        else sdl_question_menu_set_desc("Read the complete description before choosing your action.");
        for(int i=0;i<12;i++)sdl_question_menu_add_entry(i,"a","A long choice name\tReady",TERM_WHITE);
        sdl_question_menu_add_button(-1,"Back",TERM_WHITE);
        sdl_question_menu_add_button(-2,"Confirm choice",TERM_WHITE);
        sdl_question_menu_set_highlight(2);sdl_question_menu_finish();
        if(help==2)sdl_question_menu_toggle_help();
        sdl_question_menu_layout_info layout;assert(sdl_question_menu_layout(&layout));
        int n=0;hits[n++]=layout.panel;hits[n++]=layout.title_row;
        for(int i=0;i<12;i++)hits[n++]=layout.rows[i];
        for(int i=0;i<layout.button_count;i++)hits[n++]=layout.buttons[i];
        clear();sdl_question_menu_render();strnfmt(id,sizeof(id),"question-%d-%d",help,context);
        record(id,hits,n);sdl_question_menu_clear();
    }
    for(int count=4;count<=16;count+=12) {
        sdl_song_menu_begin("Choose a song");
        for(int i=0;i<count;i++)sdl_song_menu_add_entry(i,"a","Song of the long journey",TERM_WHITE);
        sdl_song_menu_set_highlight(1);sdl_song_menu_finish();
        sdl_song_menu_layout_info l;assert(sdl_song_menu_layout(&l));
        hits[0]=l.panel;for(int i=0;i<count;i++)hits[i+1]=l.rows[i];
        clear();sdl_song_menu_render();strnfmt(id,sizeof(id),"songs-%d",count);
        record(id,hits,count+1);sdl_song_menu_clear();
    }
    g_main_menu_overlay_active=true;g_main_menu_overlay_highlight=1;
    main_menu_pane_layout menu;assert(sdl_main_menu_overlay_layout(&menu));
    clear();sdl_main_menu_pane_render();hits[0]=menu.panel;record("main-menu",hits,1);
    g_main_menu_overlay_active=false;
    for(int dynamic=0;dynamic<2;dynamic++) {
        sdl_character_sheet_screen_begin_select(1,"Interface settings");
        sdl_character_sheet_screen_set_select_menu_style(true);
        for(int i=0;i<12;i++)sdl_character_sheet_screen_add_select_row(i,
            "Setting name\tEnabled",TERM_WHITE,"A useful explanation of this setting.");
        sdl_character_sheet_screen_set_select_description("A useful explanation of this setting.");
        g_sdl_select_dynamic_description=dynamic;
        assert(sdl_character_sheet_screen_commit_select(1));clear();
        sdl_character_sheet_screen_render_canvas(&canvas);
        int n=g_sdl_character_sheet_screen.hit_count;
        for(int i=0;i<n;i++)hits[i]=g_sdl_character_sheet_screen.hits[i].rect;
        strnfmt(id,sizeof(id),"settings-%d",dynamic);record(id,hits,n);
        sdl_character_sheet_screen_hide();
    }
    g_touch_pane_yes_no_prompt_active=true;
    SDL_strlcpy(g_touch_pane_yes_no_prompt_text,"Descend the stairs? You cannot return to this floor.",sizeof(g_touch_pane_yes_no_prompt_text));
    assert(sdl_touch_pane_yes_no_prompt_layout(&hits[0],&hits[1],&hits[2],&hits[3]));
    clear();sdl_touch_pane_render_yes_no_prompt();record("yes-no",hits,4);
    g_touch_pane_yes_no_prompt_active=false;
    SDL_FRect choices[SDL_TOUCH_TUTORIAL_CHOICE_COUNT];
    sdl_touch_tutorial_choice_layout(&canvas,choices);
    clear();sdl_touch_tutorial_draw_profile_choice_screen(0,choices);record("touch-presets",choices,SDL_TOUCH_TUTORIAL_CHOICE_COUNT);
    clear();sdl_touch_tutorial_draw_footer(&canvas,false,false);record("touch-footer",NULL,0);
    ui_menu_click_begin();
    sdl_character_sheet_screen_begin_select(0,"Choose your hero");
    const char *names[]={"Feanor","Maedhros","Celebrimbor"};
    for(int i=0;i<3;i++)sdl_character_sheet_screen_add_select_row(i,names[i],TERM_WHITE,"");
    sdl_character_sheet_screen_set_select_title_detail("Celebrimbor, Last of the House of Feanor"," *** mighty",TERM_GREEN);
    sdl_character_sheet_screen_set_select_description("A mighty spirit, maker of Silmarils, you carry this tale into darkness. Through fire and grief the long history unfolds.");
    sdl_character_sheet_screen_set_select_detail_size_hint(4,2,9);
    sdl_character_sheet_screen_set_select_ability_rows(2);
    const char *stats[]={"STR\t+2","DEX\t+3","CON\t+3","GRA\t+4"};
    for(int i=0;i<4;i++)sdl_character_sheet_screen_add_select_detail(stats[i],TERM_L_BLUE,"Attribute bonus");
    sdl_character_sheet_screen_add_select_detail("Artifice",TERM_RED,"Starting ability");
    sdl_character_sheet_screen_add_select_detail("Jeweler",TERM_RED,"Starting ability");
    for(int i=0;i<9;i++)sdl_character_sheet_screen_add_select_detail("Trait Affinity",TERM_GREEN,"Trait bonus");
    assert(sdl_character_sheet_screen_commit_select(0));record_character("hero-carousel",&canvas);
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
    sdl_character_sheet_screen_begin_book("The War of the Jewels");
    sdl_character_sheet_screen_add_book_contents("The Tale",5001,0);
    sdl_character_sheet_screen_add_book_contents("The War",5002,1);
    sdl_character_sheet_screen_set_book_target_page_count(2);
    for(int i=0;i<6;i++)sdl_character_sheet_screen_add_book_paragraph("Through fire and grief the long history unfolds. Remember the long journey and prepare for dangerous encounters in the dark halls.");
    sdl_character_sheet_screen_set_book_close_button(true);
    sdl_character_sheet_screen_set_book_close_label("Begin your journey");
    sdl_character_sheet_screen_commit_book();
    for(int page=0;page<g_sdl_character_sheet_screen.select_page_count;page++) {
        g_sdl_character_sheet_screen.select_page=page;
        strnfmt(id,sizeof(id),"book-%d",page);record_character(id,&canvas);
        clear();sdl_character_sheet_screen_render();
        int n=g_sdl_character_sheet_screen.hit_count;
        for(int i=0;i<n;i++)hits[i]=g_sdl_character_sheet_screen.hits[i].rect;
        strnfmt(id,sizeof(id),"book-public-%d",page);record(id,hits,n);
    }
    sdl_character_sheet_screen_hide();ui_menu_click_clear();
    /* Allocation is displayed before the engine marks a hero generated. */
    character_generated=false;character_dungeon=false;
    int values[S_MAX]={0};
    sdl_character_sheet_screen_show_birth_stats(values,values,A_GRA,5000);
    record_character("attributes",&canvas);sdl_character_sheet_screen_hide();
    sdl_character_sheet_screen_show_birth_skills(values,values,values,S_SNG,5000);
    record_character("skills",&canvas);sdl_character_sheet_screen_hide();
    character_generated=true;character_dungeon=true;
    sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_HINTS,"Hints & Quests","Saved knowledge",true,false,0);
    for(int i=0;i<8;i++)sdl_hint_quest_menu_add_block("Remember the long journey and keep supplies ready.",TERM_WHITE,0,100+i);
    sdl_hint_quest_menu_add_button(-1,"Previous page",TERM_WHITE);
    sdl_hint_quest_menu_add_button(-2,"Next page",TERM_WHITE);
    sdl_hint_quest_menu_add_button(-3,"Close book",TERM_WHITE);
    clear();sdl_hint_quest_menu_render();
    for(int i=0;i<g_hint_quest.hit_count;i++)hits[i]=g_hint_quest.hits[i].rect;
    record("hintbook",hits,g_hint_quest.hit_count);sdl_hint_quest_menu_hide();
    pane_config_count=2;
    pane_config[0]=(struct pane_config){.pane=PANE_LOG,.where=PLACE_BOTTOM,.enabled=true,.rect.rows=4};
    pane_config[1]=(struct pane_config){.pane=PANE_ROLLS,.where=PLACE_TOP_RIGHT,.enabled=false,.rect.rows=4};
    g_log_pane_menu.active=true;g_log_pane_menu.target_pane=PANE_LOG;
    g_log_pane_menu.anchor_x=width*.7f;g_log_pane_menu.anchor_y=height*.5f;
    log_pane_menu_entry entries[SDL_LOG_PANE_MENU_MAX_ENTRIES];int count=0;
    assert(sdl_log_pane_menu_layout(entries,&count,&hits[0]));
    for(int i=0;i<count;i++)hits[i+1]=entries[i].rect;
    clear();sdl_log_pane_menu_render();record("log-context-menu",hits,count+1);
    g_log_pane_menu.active=false;
}
'''
main = base.HARNESS[base.HARNESS.index("int main(int argc,char **argv)"):]
main = main.replace("grid(); hud();", "screens();")
main = main.replace("z_info=&limits;", "static object_kind kinds[2]; k_info=kinds; z_info=&limits;")
HARNESS += main
if DESKTOP:
    HARNESS = HARNESS.replace("bool __wrap_sdl_touch_only_device_active(void) { return true; }",
                              "bool __wrap_sdl_touch_only_device_active(void) { return false; }")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source=OUT/"check.c"; source.write_text(HARNESS,encoding="utf-8")
    for p in MODULES+INCLUDED:
        (OUT/("old-"+Path(p).name)).write_bytes(subprocess.check_output(["git","show",f"{BEFORE}:{p}"],cwd=ROOT))
    objects=shlex.split((BUILD/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded=tuple('/'+p+'.obj' for p in MODULES+INCLUDED)+('/src/main.c.obj',)
    response=OUT/"objects.rsp"
    response.write_text('\n'.join('"'+o+'"' for o in objects if not o.endswith(excluded)),encoding="utf-8")
    env=os.environ.copy()
    env['PATH']=os.pathsep.join([str(BUILD/'_deps'/d) for d in ['SDL','SDL_ttf','SDL_image','SDL_mixer']]+['C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wraps=['SDL_GetDisplayContentScale','sdl_touch_only_device_active','prt_frame_basic','sdl_layout_matches_supporting_pane_visibility','sdl_depth_menu_pane_label','player_current_movement_energy','player_current_movement_speed','floor_context_collect_square_actions','touch_shortcut_context_action','player_active_weapon_is_ranged','player_quick_throw_available','sdl_get_layout_screen_rect','sdl_overlay_pane_anchor_rect','sdl_mobile_lifecycle_handle_event']
    for original in (True,False):
        name='before' if original else 'current'
        cmd=['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g']
        if not DESKTOP:cmd+=['-DSIL_IOS']
        if original:cmd+=['-DORIGINAL']
        cmd+=['@CMakeFiles/sil-more.dir/includes_C.rsp',str(source)]
        cmd+=[str(OUT/('old-'+Path(p).name) if original else ROOT/p) for p in MODULES]
        cmd+=['@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp','-Wl,'+','.join('--wrap='+w for w in wraps),'-o',str(OUT/(name+'.exe'))]
        subprocess.run(cmd,cwd=BUILD,env=env,check=True)
    sizes = [(1280,720,1),(1920,1080,1.5)] if DESKTOP else [(580,1280,1.5),(1280,580,1.5),(720,1600,2),(1600,720,2),(1080,2400,2.75),(2400,1080,2.75)]
    for w,h,density in sizes:
        results=[]
        for name in ('before','current'):
            cmd=[str(OUT/(name+'.exe')),str(w),str(h),str(density),name]+[str(ROOT/'lib/xtra/font'/f) for f in ('VictorMono-Medium.ttf','Cinzel-Medium.ttf','EBGaramond-Regular.ttf')]
            run=subprocess.run(cmd,cwd=OUT,env=env,capture_output=True,text=True,timeout=60)
            (OUT/f'{name}-{w}x{h}.jsonl').write_text(run.stdout,encoding='utf-8')
            (OUT/f'{name}-{w}x{h}.stderr').write_text(run.stderr,encoding='utf-8')
            run.check_returncode()
            results.append([json.loads(l) for l in run.stdout.splitlines() if l.startswith('{')])
        assert len(results[0])==len(results[1]) and len(results[0])>=25
        for a,b in zip(*results):base.compare(a,b,f'{w}x{h}:{a["id"]}')
        print(f'Normal {w}x{h} @{density}: {len(results[0])} UI renders and hit rectangles match {BEFORE} PASS',flush=True)


if __name__=='__main__':
    main()
