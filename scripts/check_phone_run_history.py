#!/usr/bin/env python3
"""Drive production Run History against an isolated binary runs.db fixture.

Checks native mobile fields/actions/details at real display densities and
30x12,30x30,68x12 terminal projections. Never opens player data.
"""
from pathlib import Path
import os
import shlex
import subprocess
from check_phone_aux_layout import HARNESS as AUX

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/"build-standard"
OUT=ROOT/"scripts/output/phone-run-history-check"
PREFIX=AUX[:AUX.index("static void check_touch_panels(void)")]
INIT=AUX[AUX.index("int main(int argc,char **argv)"):AUX.index("    for(int big=")]
INIT=INIT.replace("argc==7","argc==9").replace("term_init(&view->t,80,24,256)",
    "term_init(&view->t,atoi(argv[7]),atoi(argv[8]),256)")
HARNESS=PREFIX+r'''
#include "score/score_ui.c"
static int modal_stage,inspect_count;
static void contains(cptr text)
{
    bool found=false;
    for(int i=0;i<g_question_menu.count;i++)
        if(strstr(g_question_menu.entries[i].text,text)) found=true;
    if(!found) fprintf(stderr,"stage%d missing %s\n",modal_stage,text);
    fixture_assert(found);
}
static void screenshot(cptr name)
{
    sdl_question_menu_layout_info layout;
    fixture_assert(sdl_question_menu_layout(&layout)); inside(layout.panel);
    fixture_assert(layout.font_px==sdl_ui_role_font_px(SDL_UI_FONT_BODY));
    for(int i=0;i<layout.button_count;i++) {
        fixture_assert(layout.buttons[i].w>=sdl_ui_min_tap_px());
        fixture_assert(layout.buttons[i].h>=sdl_ui_min_tap_px());
    }
    SDL_SetRenderDrawColor(g_state.renderer,0,0,0,255); SDL_RenderClear(g_state.renderer);
    sdl_question_menu_render();
    if(!layout.actions_in_list) for(int i=0;i<layout.button_count;i++)
        ink(layout.buttons[i],sdl_ui_role_font_px(SDL_UI_FONT_CONTROL));
    capture(name);
}
static int click(int choice)
{
    fixture_assert(ui_menu_click_handle_choice_action(choice,UI_MENU_CLICK_PRIMARY,NULL));
    return UI_MENU_CLICK_WAKE_KEY;
}
char __wrap_inkey(void)
{
    fixture_assert(g_question_menu.active);
    switch(modal_stage++) {
    case 0:
        fixture_assert(streq(g_question_menu.title,"Run History"));
        contains("Fixture Hero 2"); contains("Depth:"); contains("Rating:");
        contains("Silmarils:"); contains("FATE_END_MARKER");
        screenshot("history-date");
        {
            sdl_question_menu_layout_info layout;
            fixture_assert(sdl_question_menu_layout(&layout));
            g_question_menu.scroll_follow_highlight=false;
            sdl_question_menu_scroll_offset_by(&layout,100000);
            screenshot("history-last-fields");
            fixture_assert(sdl_question_menu_layout(&layout));
            SDL_FRect last=layout.rows[g_question_menu.count-1];
            fixture_assert(last.y+last.h<=layout.entries_rect.y+layout.entries_rect.h+1);
        }
        return click(-1);
    case 1:
        fixture_assert(strstr(g_question_menu.desc,"Rating"));
        screenshot("history-rating"); return click(-6);
    case 2:
        fixture_assert(g_question_menu.count==2); return click(-3);
    case 3:
        contains("Player:"); contains("Race:"); contains("Rating:");
        contains("Cause of death: FATE_END_MARKER"); contains("Turns spent:");
        screenshot("detail-general");
        *g_question_menu.scroll_offset_ptr=100000;
        screenshot("detail-general-end"); return '6';
    case 4:
        contains("Base: 7 | Drain: -2 | Current: 5");
        contains("Stat bonus: 3 | Item/other bonus: 4");
        screenshot("detail-stats"); return '6';
    case 5:
        contains("Sequence: 9 | Turn: 123456"); contains("Depth: 950 ft");
        screenshot("detail-abilities"); return '6';
    case 6:
        fixture_assert(g_question_menu.count==240); contains("MILESTONE_239");
        screenshot("detail-milestones"); return click(-9);
    case 7:
        fixture_assert(g_question_menu.count==1); contains("MILESTONE_240_END_MARKER");
        screenshot("detail-milestones-last"); return '6';
    case 8:
        contains("No records available"); screenshot("detail-artefacts"); return '6';
    case 9:
        contains("Azog the Orc Captain"); contains("Seen: 17 | Slain: 2 | Deaths: 1");
        screenshot("detail-unique-monsters"); return click(-7);
    case 10:
        fixture_assert(streq(g_question_menu.title,"Run record sections"));
        contains("General"); contains("Stats"); contains("Abilities"); contains("Milestones");
        contains("Artefacts"); contains("Monsters"); return click(1010);
    case 11:
        fixture_assert(strstr(g_question_menu.desc,"Unique"));
        screenshot("detail-monster-encounters"); return click(-5);
    case 12:
        screenshot("detail-monster-sort"); return click(-4);
    case 13:
        fixture_assert(inspect_count==1); return click(-1);
    case 14:
        fixture_assert(streq(g_question_menu.title,"Run History")); return click(-2);
    default: fixture_assert(false); return ESCAPE;
    }
}
bool __wrap_screen_roff(int race,monster_type* monster)
{ fixture_assert(race==1); inspect_count++; return true; }
static void write_db(void)
{
    SDL_IOStream *file=SDL_IOFromFile("runs.db","wb"); fixture_assert(file);
    score_db_header header={0}; memcpy(header.magic,SCORE_DB_MAGIC,4);
    header.version=0x00020000; header.record_count=2;
    fixture_assert(SDL_WriteIO(file,&header,sizeof(header))==sizeof(header));
    for(int n=1;n<=2;n++) {
        score_record_v1 rec={0}; rec.record_id=n; rec.metarun_id=777;
        rec.created_utc=1760000000; rec.completed_utc=1760001000+n;
        rec.status=SCORE_RECORD_DEAD; rec.exit_depth=19; rec.max_depth=20;
        rec.silmarils=2; rec.turns_spent=654321; rec.xp_earned=10000;
        SDL_snprintf(rec.player_name,sizeof(rec.player_name),"Fixture Hero %d",n);
        SDL_strlcpy(rec.cause_of_death,"FATE_END_MARKER slain by an orc warrior",sizeof(rec.cause_of_death));
        fixture_assert(SDL_WriteIO(file,&rec,sizeof(rec))==sizeof(rec));
        score_run_detail_header_v1 detail={0}; detail.version=2;
        detail.monster_count=detail.monster_capacity=1;
        fixture_assert(SDL_WriteIO(file,&detail,sizeof(detail))==sizeof(detail));
        u16b count=1;
        score_run_stat_v1 stat={0}; stat.base=7; stat.drain=-2; stat.current=5;
        SDL_WriteIO(file,&count,sizeof(count)); SDL_WriteIO(file,&stat,sizeof(stat));
        score_run_skill_v1 skill={0}; skill.base=8; skill.current=15;
        skill.stat_bonus=3; skill.item_bonus=4;
        SDL_WriteIO(file,&count,sizeof(count)); SDL_WriteIO(file,&skill,sizeof(skill));
        score_run_ability_v1 ability={0}; ability.order=9; ability.player_turn=123456; ability.depth=19;
        SDL_WriteIO(file,&count,sizeof(count)); SDL_WriteIO(file,&ability,sizeof(ability));
        count=241; SDL_WriteIO(file,&count,sizeof(count));
        for(int i=0;i<count;i++) {
            score_run_milestone_v1 milestone={0}; milestone.player_turn=1000+i; milestone.depth=19;
            SDL_snprintf(milestone.note,sizeof(milestone.note),"MILESTONE_%d%s",i,
                i==240?"_END_MARKER full note after the native page boundary":"");
            SDL_WriteIO(file,&milestone,sizeof(milestone));
        }
        score_run_monster_v1 monster={0}; monster.r_idx=1; monster.seen=17; monster.killed=2; monster.deaths=1;
        SDL_WriteIO(file,&monster,sizeof(monster));
    }
    SDL_CloseIO(file);
    file=SDL_IOFromFile("runs.db","rb");
    fixture_assert(file && score_runs_validate_history_db(file,&header)); SDL_CloseIO(file);
}
''' + INIT + r'''
    static monster_race races[2];
    static char names[]="\0Azog the Orc Captain\0";
    races[1].name=1; races[1].level=19; races[1].flags1=RF1_UNIQUE;
    r_info=races; r_name=names; limits.r_max=2;
    char meta_path[64]; SDL_snprintf(meta_path,sizeof(meta_path),".%smetaruns",PATH_SEP);
    ANGBAND_DIR_APEX="."; ANGBAND_DIR_METARUN=meta_path;
    character_generated=false; player.is_dead=true;
    write_db();
    for(int big=1;big<2;big++) {
        config.bigger_font=big; fixture.active=false;
        modal_stage=inspect_count=0;
        do_cmd_run_history();
        fixture_assert(modal_stage==15 && !g_question_menu.active);
    }
    printf("Run History DB/modal %dx%d @%.3f terminal%sx%s Big font: PASS\n",
        fixture_width,fixture_height,fixture_density,argv[7],argv[8]);
    sdl_ui_text_cache_clear(); sdl_story_font_cache_clear(); term_nuke(&view->t);
    SDL_DestroyRenderer(g_state.renderer); SDL_DestroyWindow(g_state.window); TTF_Quit(); SDL_Quit();
}
'''


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    # externs.h contains enum definitions without an include guard. The live
    # UI source normally includes it once; this composed fixture already has it.
    live=(ROOT/"src/score/score_ui.c").read_text().replace('#include "externs.h"','')
    (OUT/"score-ui-live.c").write_text(live,encoding="utf-8")
    source=OUT/"check.c"
    source.write_text(HARNESS.replace('#include "score/score_ui.c"',
        '#include "score-ui-live.c"'),encoding="utf-8")
    objects=shlex.split((BUILD/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded=("/src/main.c.obj","/src/sdl/ui/sdl-gameplay-tutorial.c.obj",
        "/src/sdl/input/sdl-touch-tutorial.c.obj","/src/sdl/render/sdl-fonts.c.obj",
        "/src/sdl/ui/sdl-song-menu.c.obj","/src/sdl/ui/sdl-question-menu.c.obj","/src/score/score_ui.c.obj")
    response=OUT/"objects.rsp"
    response.write_text("\n".join('"'+obj+'"' for obj in objects if not obj.endswith(excluded)))
    env=os.environ.copy(); env["PATH"]=os.pathsep.join(
        [str(BUILD/"_deps"/dep) for dep in ["SDL","SDL_ttf","SDL_image","SDL_mixer"]]
        +["C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    wraps=["tutorial_get_view","tutorial_is_active","tutorial_revision","get_sdl_gameplay_tutorial_mode",
        "SDL_WaitEvent","sdl_touch_round_layer_controls_active","sdl_touch_round_compute_layout",
        "sdl_touch_thumb_current_bounds","sdl_map_grid_cell_rect","sdl_touch_only_device_active",
        "sdl_touch_only_mobile_device_active","sdl_get_layout_screen_rect","sdl_overlay_pane_anchor_rect",
        "SDL_GetDisplayContentScale","sdl_terminal_menu_font_px","sdl_mobile_lifecycle_handle_event",
        "sdl_touch_tutorial_device_available","SDL_RenderTexture","inkey","screen_roff"]
    exe=OUT/"check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),"@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp","-Wl,"+",".join("--wrap="+w for w in wraps),
        "-o",str(exe)],cwd=BUILD,env=env,check=True)
    for w,h,density,cols,rows in [(720,1600,2,30,12),(720,1600,2,30,30),
        (1600,720,2,68,12),(1080,2340,2.75,30,30),(2340,1080,2.75,68,12),
        (1080,2400,2.625,30,30),(2400,1080,2.625,68,12)]:
        subprocess.run([str(exe),str(w),str(h),str(density),
            str(ROOT/"lib/xtra/font/EBGaramond-Regular.ttf"),str(ROOT/"lib/xtra/font/Cinzel-Medium.ttf"),
            str(ROOT/"lib/xtra/font/VictorMono-Medium.ttf"),str(cols),str(rows)],cwd=OUT,env=env,check=True,timeout=60)


if __name__=="__main__": main()
