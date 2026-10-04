#!/usr/bin/env python3
"""Exercise uncovered naming, self-knowledge, score-context and buffer-view paths.

Compiles production units against the configured Windows engine. Only modal UI
presentation is intercepted; templates, calculations and file reads stay real.
Every scenario uses fresh temporary player stores and a process timeout.
"""
from pathlib import Path
import argparse
import json
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS
from check_monster_scent_save import fixture_function

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/remaining-review-paths"
CASES = ("self-knowledge", "stun", "identify", "flavors", "names",
         "viewer", "rounding", "scores", "parsers", "quarks", "history")
PARSERS = (
    ("parse_b_info", "ability_type", "ability"),
    ("parse_a_info", "artefact_type", "artefact"),
    ("parse_cu_info", "curse_type", "curses"),
    ("parse_e_info", "ego_item_type", "special"),
    ("parse_flavor_info", "flavor_type", "flavor"),
    ("parse_h_info", "hist_type", "history"),
    ("parse_mb_info", "major_blessing_type", "blessing"),
    ("parse_r_info", "monster_race", "monster"),
    ("parse_oath_info", "oath_type", "oath"),
    ("parse_k_info", "object_kind", "object"),
    ("parse_p_info", "player_race", "race"),
    ("parse_c_info", "character_profile", "character"),
    ("parse_quest_info", "quest_type", "quest"),
    ("parse_st_info", "story_type", "story"),
    ("parse_f_info", "feature_type", "terrain"),
    ("parse_v_info", "vault_type", "vault"),
    ("parse_rt_info", "runtype_type", "runtypes"),
    ("parse_style_info", "style_type", "style"),
)

TESTS = r'''
#include "supplies.h"
#include "ui/file-viewer.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_runs.h"
#include <limits.h>
static int report_count;
static char report[512][200];
static void check_parsers(void);
void fixture_display_attributes(char s[][200], char t[][200], bool good[], int count)
{
    (void)t; (void)good; assert(count<=512); report_count=count;
    for (int i=0;i<count;i++) SDL_strlcpy(report[i],s[i],sizeof(report[i]));
}
void __wrap_msg_print(cptr text) { (void)text; }
static int score_views;
static int score_flush_errors;
static bool simulate_score_flush_failure;
bool __real_SDL_FlushIO(SDL_IOStream* stream);
bool __wrap_SDL_FlushIO(SDL_IOStream* stream)
{
    if (simulate_score_flush_failure) {
        SDL_SetError("Simulated score flush failure");
        return false;
    }
    return __real_SDL_FlushIO(stream);
}
static void count_score_flush_error(log_Event* event)
{
    if (strstr(event->fmt,"Failed to flush high score file"))
        score_flush_errors++;
}
int fixture_run_rating(int curses);
int fixture_history_probe(void);
int fixture_capture_tail(char s[][200], char t[][200], bool good[], int count);
void fixture_score_view(const high_score* entry)
{
    assert(score_file_active_ctx()!=score_file_global_ctx());
    assert(score_file_active_ctx()->entry_count==1);
    assert(scores_version_has_curses(score_file_active_ctx()));
    assert(score_calculate_breakdown(entry).curses==20);
    high_score stored; assert(highscore_seek(0)==0 && highscore_read(&stored)==0);
    assert(!strcmp(stored.who,"Archived")); score_views++;
}
static void setup(void)
{
    reset_map(10); memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store(); supplies_reset_store();
    memset(p_ptr->have_ability,0,sizeof(p_ptr->have_ability));
    memset(p_ptr->innate_ability,0,sizeof(p_ptr->innate_ability));
    p_ptr->update=0; p_ptr->stun=0;
}
static void check_self_knowledge(void)
{
    setup(); self_knowledge(); int base=report_count, added=0;
    for (int i=1;i<z_info->b_max;i++) {
        ability_type* ability=&b_info[i];
        if (!ability->name || ability->skilltype>=S_MAX || ability->abilitynum>=ABILITIES_MAX)
            continue;
        if (!p_ptr->have_ability[ability->skilltype][ability->abilitynum]) added++;
        p_ptr->have_ability[ability->skilltype][ability->abilitynum]=true;
    }
    assert(base+added>100); self_knowledge(); assert(report_count==base+added);
    char summary[320][200], detail[320][200]; bool good[320];
    for(int i=0;i<320;i++) {
        SDL_strlcpy(summary[i],"A long attribute description that wraps across several display lines without dropping later entries.",sizeof(summary[i]));
        SDL_strlcpy(detail[i],"This detail keeps its colour when wrapped.",sizeof(detail[i]));
        good[i]=true;
    }
    SDL_strlcpy(summary[319],"TAIL",sizeof(summary[319]));
    SDL_strlcpy(detail[319],"ENDDETAIL",sizeof(detail[319]));
    term* saved=Term; int wrap=text_out_wrap,indent=text_out_indent;
    void (*hook)(byte,cptr)=text_out_hook;
    assert(fixture_capture_tail(summary,detail,good,320)>255);
    assert(Term==saved && text_out_hook==hook && text_out_wrap==wrap && text_out_indent==indent);
}
static void check_stun(void)
{
    setup();
    for (int amount=50;amount<=51;amount++) {
        p_ptr->stun=amount; self_knowledge(); bool found=false;
        for (int i=0;i<report_count;i++) if (strstr(report[i],"stunned")) {
            assert((strstr(report[i],"heavily")!=NULL)==(amount>50)); found=true;
        }
        assert(found);
    }
}
static void check_identify(void)
{
    setup(); object_type object;
    object_prep(&inventory[0],lookup_kind(TV_SWORD,SV_DAGGER));
    object_prep(&object,lookup_kind(TV_HELM,SV_HELM));
    assert(player_carried_extra_load(&object));
    object_prep(&object,lookup_kind(TV_ARROW,SV_NORMAL_ARROW));
    assert(player_quiver_absorb_arrow(&object)==1);
    object_prep(&object,lookup_kind(TV_POTION,SV_POTION_HEALING));
    assert(supplies_absorb_object(&object));
    identify_pack();
    assert(object_known_p(&inventory[0]));
    assert(object_known_p(player_carried_extra_entry_at(0)));
    assert(object_known_p(player_quiver_store_entry_at(0)));
    assert(object_known_p(supplies_entry_at(0)));
}
static void check_flavors(void)
{
    assert(init_flavor_info()==0);
    byte* authored=malloc(z_info->flavor_max); assert(authored);
    for (int i=0;i<z_info->flavor_max;i++) authored[i]=flavor_info[i].sval;
    u16b* first=calloc(z_info->k_max,sizeof(*first)); assert(first);
    Rand_state_init(456); u64b state=Rand_state_export();
    seed_flavor=123; flavor_init();
    assert(Rand_state_export()==state);
    for (int i=0;i<z_info->k_max;i++) first[i]=k_info[i].flavor;
    for (int i=0;i<z_info->flavor_max;i++) assert(flavor_info[i].sval==authored[i]);
    seed_flavor=789; flavor_init(); bool changed=false;
    for (int i=0;i<z_info->k_max;i++) if(first[i]!=k_info[i].flavor) changed=true;
    assert(changed && Rand_state_export()==state);
    seed_flavor=123; flavor_init();
    for (int i=0;i<z_info->k_max;i++) assert(first[i]==k_info[i].flavor);
    seed_flavor=0; flavor_init();
    for (int i=0;i<z_info->k_max;i++) first[i]=k_info[i].flavor;
    Rand_state_init(999); state=Rand_state_export(); flavor_init();
    assert(Rand_state_export()==state);
    for (int i=0;i<z_info->k_max;i++) assert(first[i]==k_info[i].flavor);
    free(first); free(authored);
}
static void check_names(void)
{
    names_type names={0}; n_info=&names; char output[80];
    make_random_name(output,sizeof(output)); assert(output[0]);
    header head={0}; head.info_ptr=&names;
    char line[]="N:Brrr"; assert(parse_n_info(line,&head)==0);
    make_random_name(output,sizeof(output)); assert(output[0]);
    memset(&names,0,sizeof(names));
    char valid[]="N:Elbereth"; assert(parse_n_info(valid,&head)==0);
    make_random_name(output,sizeof(output)); assert(!strcmp(output,"Elbereth"));
}
static char at(int x,int y)
{
    byte attr; char c; assert(Term_what(x,y,&attr,&c)==0); return c;
}
static void check_viewer(void)
{
    char text[2000]; memset(text,'A',sizeof(text));
    text[1800]='\n'; SDL_strlcpy(text+1801,"Next",sizeof(text)-1801);
    Term_keypress(ESCAPE); assert(show_buffer(text,0));
    assert(at(0,2)=='A' && at(0,3)=='N');
    Term_keypress(ESCAPE); assert(show_buffer("\nStart\nNext",0));
    assert(at(0,2)==' ' && at(0,3)=='S');
}
static void check_rounding(void)
{
    for(int n=-35;n<=35;n++) for(int d=-20;d<=20;d++) {
        int expected=n;
        if(d) {
            int a=abs(n),b=abs(d); expected=(a+b/2)/b;
            if((n<0)!=(d<0))expected=-expected;
        }
        assert(div_round(n,d)==expected);
    }
    assert(div_round(INT_MIN,-1)==INT_MAX);
    assert(div_round(INT_MIN,INT_MIN)==1);
    assert(div_round(INT_MAX,2)==1073741824);
}
static void check_quarks(void)
{
    assert(quark_add("first")>0);
    assert(quarks_free()==0 && quark_str(0)==NULL);
    assert(quarks_free()==0);
    assert(quarks_init()==0);
    assert(quark_add("fresh")==1 && !strcmp(quark_str(1),"fresh"));
    assert(quark_add("fresh")==1 && quark_add(NULL)==0);
    assert(quarks_init()==0 && quark_str(1)==NULL);
}
static void check_scores(void)
{
    high_score score={0}; SDL_strlcpy(score.who,"Archived",sizeof(score.who));
    SDL_strlcpy(score.pts,"20",sizeof(score.pts));
    score_file_ctx active={0};
    active.version_major=SCORE_FILE_VERSION_MAJOR; active.version_minor=SCORE_FILE_VERSION_MINOR;
    active.version_patch=SCORE_FILE_VERSION_PATCH; active.version_extra=SCORE_FILE_VERSION_EXTRA;
    score_file_ctx* global=score_file_global_ctx(); memset(global,0,sizeof(*global));
    score_file_set_active_ctx(&active);
    assert(score_calculate_breakdown(&score).curses==20);
    score_file_set_active_ctx(global); *global=active; global->entry_count=77;
    active.version_major=active.version_minor=active.version_patch=active.version_extra=0;
    score_file_set_active_ctx(&active);
    assert(score_calculate_breakdown(&score).curses==0);
    assert(fixture_run_rating(20)>fixture_run_rating(0));
    assert(score_file_active_ctx()==&active);
    score_file_set_active_ctx(global); score_file_ctx before=*global;
    score_file_header header={0}; header.version_major=SCORE_FILE_VERSION_MAJOR;
    header.version_minor=SCORE_FILE_VERSION_MINOR; header.version_patch=SCORE_FILE_VERSION_PATCH;
    header.version_extra=SCORE_FILE_VERSION_EXTRA; header.entry_count=1;
    FILE* file=fopen("archive.raw","wb"); assert(file);
    assert(fwrite(&header,sizeof(header),1,file)==1 && fwrite(&score,sizeof(score),1,file)==1);
    fclose(file); show_scores_interactive_highlight_from_file("archive.raw",&score);
    assert(score_views==1 && !memcmp(global,&before,sizeof(before)));
    assert(score_file_active_ctx()==global);

    /* SDL3 flushes return true on success.  Saving a score must not report
     * an error for a successful flush, and must report a real failure. */
    score_file_ctx writer={0};
    score_file_set_active_ctx(&writer);
    writer.fd=score_file_open("flush.raw",O_RDWR|O_CREAT); assert(writer.fd);
    assert(log_add_callback(count_score_flush_error,NULL,LOG_ERROR)==0);
    assert(highscore_add(&score)==0 && score_flush_errors==0);
    assert(writer.entry_count==1);
    simulate_score_flush_failure=true;
    log_set_quiet(true);
    assert(highscore_add(&score)==0 && score_flush_errors==1);
    log_set_quiet(false);
    simulate_score_flush_failure=false;
    assert(SDL_CloseIO(writer.fd)); writer.fd=NULL;
    writer.fd=score_file_open("flush.raw",O_RDONLY); assert(writer.fd);
    high_score stored;
    assert(writer.entry_count==1 && highscore_seek(0)==0);
    assert(highscore_read(&stored)==0 && !memcmp(&stored,&score,sizeof(score)));
    assert(SDL_CloseIO(writer.fd)); writer.fd=NULL;
    score_file_set_active_ctx(global);
    assert(!memcmp(global,&before,sizeof(before)));
}
static void check_history(void)
{
    ANGBAND_DIR_APEX="."; ANGBAND_DIR_METARUN=NULL;
    score_db_header header={0}; memcpy(header.magic,SCORE_DB_MAGIC,4);
    header.record_count=1;
    score_record_v1 record={0}; record.record_id=1; record.status=SCORE_RECORD_DEAD;
    memset(record.player_name,'A',sizeof(record.player_name));
    memset(record.cause_of_death,'B',sizeof(record.cause_of_death));
    memset(record.killer_name,'C',sizeof(record.killer_name));
    memset(record.savefile_hint,'D',sizeof(record.savefile_hint));
    score_run_detail_header_v1 detail={0}; detail.version=1;
    for(int scenario=0;scenario<4;scenario++) {
        header.version=scenario==0?0x00010000:(scenario==2?0x00030000:0x00020000);
        FILE* file=fopen("runs.db","wb"); assert(file);
        fwrite(&header,sizeof(header),1,file); fwrite(&record,sizeof(record),1,file);
        if(scenario!=3)fwrite(&detail,sizeof(detail),1,file);
        fclose(file);
        assert(fixture_history_probe()==(scenario<2?1:0));
    }
}
static void test_case(const char* name)
{
    if(!strcmp(name,"self-knowledge"))check_self_knowledge();
    else if(!strcmp(name,"stun"))check_stun();
    else if(!strcmp(name,"identify"))check_identify();
    else if(!strcmp(name,"flavors"))check_flavors();
    else if(!strcmp(name,"names"))check_names();
    else if(!strcmp(name,"viewer"))check_viewer();
    else if(!strcmp(name,"rounding"))check_rounding();
    else if(!strcmp(name,"scores"))check_scores();
    else if(!strcmp(name,"parsers"))check_parsers();
    else if(!strcmp(name,"quarks"))check_quarks();
    else if(!strcmp(name,"history"))check_history();
    else assert(false);
    printf("Remaining review scenario %s PASS\n",name);
}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case",choices=("all",*CASES),default="all")
    parser.add_argument("--portable",action="store_true")
    args=parser.parse_args()
    build=ROOT/("build-portable" if args.portable else "build-standard")
    out=OUT/("portable" if args.portable else "standard")
    out.mkdir(parents=True,exist_ok=True)
    prefix=HARNESS[:HARNESS.index("static const char* guids[]")]
    init=HARNESS[HARNESS.index("int main(int argc,char** argv)"):]
    init=init[:init.index("    check_templates();")].replace("assert(argc==3);","assert(argc==4);")
    source=out/"check.c"
    parser_rows=[]
    for function,typ,template in PARSERS:
        starts={"oath":("N:","O:"),"quest":("N:","Q:")}.get(template,("N:",))
        lines=(ROOT/"lib/edit"/(template+".txt")).read_text(encoding="utf-8").splitlines()
        index=next(i for i,line in enumerate(lines) if line.startswith(starts))
        initial=lines[index]
        orphan=next(line for line in lines[index+1:] if line and not line.startswith("#"))
        parser_rows.append('{'+function+',sizeof('+typ+'),'+json.dumps(initial,ensure_ascii=False)+','+
                           json.dumps(orphan,ensure_ascii=False)+'}')
    parser_test=r'''
static void check_parsers(void)
{
    struct { parse_info_txt_func parse; size_t size; const char* initial; const char* orphan; } cases[]={
@ROWS@
    };
    for(size_t i=0;i<N_ELEMENTS(cases);i++) {
        header old={0},next={0}; old.info_num=next.info_num=64;
        old.info_len=next.info_len=cases[i].size;
        old.info_ptr=calloc(old.info_num,cases[i].size);
        next.info_ptr=calloc(next.info_num,cases[i].size);
        old.name_ptr=calloc(z_info->fake_name_size,1); next.name_ptr=calloc(z_info->fake_name_size,1);
        old.text_ptr=calloc(z_info->fake_text_size,1); next.text_ptr=calloc(z_info->fake_text_size,1);
        assert(old.info_ptr&&next.info_ptr&&old.name_ptr&&next.name_ptr&&old.text_ptr&&next.text_ptr);
        char line[1024]; SDL_strlcpy(line,cases[i].initial,sizeof(line)); error_idx=-1;
        assert(cases[i].parse(line,&old)==0);
        void* snapshot=malloc(old.info_num*cases[i].size); assert(snapshot);
        memcpy(snapshot,old.info_ptr,old.info_num*cases[i].size);
        SDL_strlcpy(line,cases[i].orphan,sizeof(line)); error_idx=-1;
        int result=cases[i].parse(line,&next);
        if(result!=PARSE_ERROR_MISSING_RECORD_HEADER) {
            printf("Parser %zu (%s) returned %d for orphan %s\n",i,cases[i].initial,result,cases[i].orphan);
            exit(2);
        }
        if(cases[i].parse==parse_c_info) {
            SDL_strlcpy(line,"A:Alternate",sizeof(line));
            assert(cases[i].parse(line,&next)==PARSE_ERROR_MISSING_RECORD_HEADER);
            SDL_strlcpy(line,"B:Start",sizeof(line));
            assert(cases[i].parse(line,&next)==PARSE_ERROR_MISSING_RECORD_HEADER);
        }
        assert(!memcmp(snapshot,old.info_ptr,old.info_num*cases[i].size));
        free(snapshot); free(old.info_ptr); free(next.info_ptr);
        free(old.name_ptr); free(next.name_ptr); free(old.text_ptr); free(next.text_ptr);
    }
}
'''.replace("@ROWS@",",\n".join(parser_rows))
    source.write_text(prefix+fixture_function("terminal_extra")+"\n"+fixture_function("reset_map")+
                      TESTS+parser_test+init+"    test_case(argv[3]); SDL_Quit(); return 0;\n}\n",encoding="utf-8")
    utility=out/"utility.c"
    utility.write_text((ROOT/"src/spell/spell-utility.c").read_text(encoding="utf-8").replace(
        "    display_attributes(s, t, good, i);","    fixture_display_attributes(s, t, good, i);").replace(
        "// Function declarations","void fixture_display_attributes(char s[][200], char t[][200], bool good[], int count);\n// Function declarations")+r'''
int fixture_capture_tail(char s[][200], char t[][200], bool good[], int count)
{
    self_knowledge_capture capture={0};
    if(!self_knowledge_capture_build(s,t,good,count,&capture))return 0;
    bool tail=false,detail=false;
    for(int y=0;y<capture.height;y++)for(int x=0;x<capture.width;x++) {
        const char* cell=capture.chars+y*capture.width+x;
        if(x+4<=capture.width && !memcmp(cell,"TAIL",4))tail=true;
        if(x+9<=capture.width && !memcmp(cell,"ENDDETAIL",9))detail=true;
    }
    int height=capture.height; self_knowledge_capture_free(&capture);
    return tail&&detail?height:0;
}
''',encoding="utf-8")
    ui=(ROOT/"src/score/score_ui.c").read_text(encoding="utf-8")
    start=ui.index("void show_scores_interactive_highlight_from_file(")
    end=ui.index("static const char* score_run_status_label",start)
    ui=ui[:start]+ui[start:end].replace("show_scores_interactive_highlight(entry)","fixture_score_view(entry)")+ui[end:]
    score_ui=out/"score-ui.c"
    score_ui.write_text('#include "angband.h"\n#include <assert.h>\nvoid fixture_score_view(const high_score*);\n'+ui+
        '\nint fixture_run_rating(int curses){score_record_v1 r={0};r.max_depth=10;'
        'r.net_curses=curses;return run_history_compute_rating(&r);}\n'+r'''
int fixture_history_probe(void)
{
    run_history_entry entries[2],found;
    int count=collect_run_history(entries,2);
    assert(run_history_find_by_record_id(1,&found)==(count==1));
    if(count) {
        assert(!entries[0].record.player_name[31] && !found.record.player_name[31]);
        assert(!entries[0].record.cause_of_death[63] && !found.record.cause_of_death[63]);
        assert(!entries[0].record.killer_name[47] && !found.record.killer_name[47]);
        assert(!entries[0].record.savefile_hint[31] && !found.record.savefile_hint[31]);
    }
    return count;
}
''',encoding="utf-8")
    units=("rng","randart","object/object-flavor","ui/file-viewer","score/score_logic","score/score_runs","support/quark",
           "init/init-style",*(path.relative_to(ROOT/"src").with_suffix("").as_posix()
                               for path in sorted((ROOT/"src/init").glob("init-parse-*.c"))))
    objects=shlex.split((build/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded=("/src/main.c.obj","/src/spell/spell-utility.c.obj","/src/score/score_ui.c.obj",
              *(f"/src/{unit}.c.obj" for unit in units))
    response=out/"objects.rsp"
    response.write_text("\n".join('"'+obj+'"' for obj in objects if not obj.endswith(excluded)),encoding="utf-8")
    env=os.environ.copy()
    env["PATH"]=os.pathsep.join([*(str(build/"_deps"/name) for name in
        ("SDL","SDL_ttf","SDL_image","SDL_mixer")),"C:/msys64/mingw64/bin","C:/msys64/usr/bin",env["PATH"]])
    executable=out/"check.exe"
    defines=["-DUSE_SDL"]+(["-DSIL_USE_LOCAL_DATA"] if args.portable else [])
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe",*defines,"-std=c17","-O0","-g","-fstack-protector-all",
        "@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),str(utility),str(score_ui),
        *(str(ROOT/"src"/(unit+".c")) for unit in units),"@"+str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp","-Wl,--wrap=msg_print",
        "-Wl,--wrap=SDL_FlushIO","-o",str(executable)],
        cwd=build,env=env,check=True)
    failed=[]
    for case in CASES if args.case=="all" else (args.case,):
        with tempfile.TemporaryDirectory(prefix="data-",dir=out) as data:
            try:
                result=subprocess.run([str(executable),str(ROOT/"lib/edit"),data,case],
                    cwd=data,env=env,timeout=5)
                if result.returncode: failed.append((case,result.returncode))
            except subprocess.TimeoutExpired: failed.append((case,"timeout"))
    assert not failed,failed


if __name__=="__main__":
    main()
