#!/usr/bin/env python3
"""Check real interaction animation/final captions for player and monster actors."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import argparse
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/interaction-roll-actor'
CHECKS=r'''
#undef assert
#define assert(expr) do { if(!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__);exit(1); } } while(0)
static cptr expected;
static int actor_rows;
static monster_type* refresh_actor;
static bool refresh_visibility;
static int visibility_refreshes;
int __wrap_get_sdl_dice_roll_lock_ms(void) { return 250; }
int __wrap_get_sdl_dice_roll_overlay_ms(void) { return 100; }
void __wrap_handle_stuff(void)
{
    /* Instrument the documented view-flush boundary. Full map visibility
     * recomputation is outside this isolated caption/animation fixture. */
    if (refresh_actor) {
        assert(p_ptr->update & PU_MONSTERS);
        refresh_actor->ml=refresh_visibility;
        refresh_actor=NULL;
        visibility_refreshes++;
    }
    p_ptr->update=p_ptr->redraw=p_ptr->window=0;
}
void __wrap_sdl_question_menu_add_text(cptr text,byte attr)
{
    (void)attr;
    if(!strstr(text," d10 ") || !strncmp(text,"Difficulty",10))return;
    if(strncmp(text,expected,strlen(expected)))
        fprintf(stderr,"Expected actor '%s' after %d visibility refreshes; got: %s\n",
            expected,visibility_refreshes,text);
    assert(!strncmp(text,expected,strlen(expected)));
    if(!strcmp(expected,"it"))assert(!strstr(text,"Wolf"));
    actor_rows++;
}
static void check_actor(monster_type *actor,cptr label)
{
    expected=label;actor_rows=0;character_icky=0;
    bool old_cursor=hide_cursor;
    p_ptr->energy_use=75;long actions=playerturn;s32b old_turn=turn;
    skill_roll_details roll;
    int result=show_interaction_skill_roll_animation_actor(actor,"Noticing tampering",
        "Studying the trap",5,5,2,27,&roll);
    assert(result==roll.result && actor_rows==2);
    assert(hide_cursor==old_cursor && p_ptr->energy_use==75
        && playerturn==actions && turn==old_turn);
    sdl_question_menu_clear();
}
static void checks(void)
{
    monster_type wolf={0};
    for(int i=1;i<z_info->r_max;i++)if(!strcmp(r_name+r_info[i].name,"Wolf"))wolf.r_idx=i;
    assert(wolf.r_idx);hide_cursor=false;wolf.ml=true;check_actor(&wolf,"the Wolf");
    wolf.ml=false;check_actor(&wolf,"it");
    check_actor(PLAYER,"You");
    hide_cursor=true;wolf.ml=true;refresh_actor=&wolf;refresh_visibility=false;
    p_ptr->update|=PU_UPDATE_VIEW|PU_MONSTERS;
    check_actor(&wolf,"it");assert(!wolf.ml && visibility_refreshes==1);
    hide_cursor=false;wolf.ml=false;refresh_actor=&wolf;refresh_visibility=true;
    p_ptr->update|=PU_UPDATE_VIEW|PU_MONSTERS;
    check_actor(&wolf,"the Wolf");assert(wolf.ml && visibility_refreshes==2);
    puts("Interaction actors: player/visible/hidden and pending visible-to-hidden/hidden-to-visible refresh labels match in preview/final; rolls and action state preserved PASS.");
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',action='store_true')
    parser.add_argument('--old-order',action='store_true',
                        help='Restore the old pre-refresh description order only')
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix+=fixture_function('terminal_extra')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init=init[:init.index('    check_templates();')]
    source=OUT/'check.c';source.write_text(prefix+CHECKS+init+'    checks();SDL_Quit();return 0;\n}\n',encoding='utf-8')
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp';response.write_text('\n'.join('"'+p+'"' for p in objects
        if not p.endswith(('/src/main.c.obj','/src/cmd/world/cmd-interact.c.obj'))),encoding='utf-8')
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p)for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe=OUT/'check.exe'
    interact=ROOT/'src/cmd/world/cmd-interact.c'
    if args.baseline:
        text=interact.read_text(encoding='utf-8')
        old='line, sizeof(line), actor_label, skill_die, roll->skill_sides, roll->skill);'
        assert text.count(old)==1
        interact=OUT/'interact-baseline.c'
        interact.write_text(text.replace(old,old.replace('actor_label','"You"'),1),encoding='utf-8')
    elif args.old_order:
        text=interact.read_text(encoding='utf-8')
        block='''    /* Pending view updates may change whether the actor is visible. */
    if (actor == PLAYER)
        SDL_strlcpy(actor_label, "You", sizeof(actor_label));
    else
        monster_desc(actor_label, sizeof(actor_label), actor, 0);

'''
        assert text.count(block)==1
        text=text.replace(block,'',1)
        anchor='    memset(&preview_roll, 0, sizeof(preview_roll));'
        assert text.count(anchor)==1
        interact=OUT/'interact-old-order.c'
        interact.write_text(text.replace(anchor,block+anchor,1),encoding='utf-8')
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','@CMakeFiles/sil-more.dir/includes_C.rsp',
        str(source),str(interact),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',
        *(f'-Wl,--wrap={name}'for name in ('get_sdl_dice_roll_lock_ms','get_sdl_dice_roll_overlay_ms',
            'handle_stuff','sdl_question_menu_add_text')),'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT)as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=15)

if __name__=='__main__':main()
