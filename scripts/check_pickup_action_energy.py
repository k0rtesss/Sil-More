#!/usr/bin/env python3
"""Check real pickup sinks: cancellation, one paid turn and reused movement.

The native interface reproductions live in check_player_scenarios_gameplay.py.
These engine checks additionally cover a pre-paid half action and mixed piles.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from qa9_sdl_fixture import HARNESS

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/player-systems-expanded/pickup-energy-guard'
CHECKS=r'''
#undef assert
#define assert(e) do { if(!(e)){fprintf(stderr,"Pickup contract failed: %s line %d\n",#e,__LINE__);exit(1);} } while(0)
static int replacement_questions;
bool __wrap_open_inventory_item_select_menu(int mode,cptr prompt,cptr empty,int* out)
{(void)mode;(void)prompt;(void)empty;(void)out;replacement_questions++;return false;}
bool __wrap_open_inventory_replacement_menu(inventory_menu_group group,
    const object_type* incoming,bool equip,bool supplies,cptr reason,int* out)
{(void)group;(void)incoming;(void)equip;(void)supplies;(void)reason;(void)out;replacement_questions++;return false;}
bool __wrap_get_check(cptr prompt){(void)prompt;return false;}
void __wrap_handle_stuff(void){}
void __wrap_msg_print(cptr text){(void)text;}
void __wrap_msg_format(cptr format,...){(void)format;}
void __wrap_tutorial_game_item(const object_type* o){(void)o;}
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* o){(void)action;(void)o;return true;}
void __wrap_tutorial_game_explain(const char* id,const char* title,const char* detail){(void)id;(void)title;(void)detail;}
static void reset_map(int depth)
{
    memset(o_list,0,z_info->o_max*sizeof(*o_list));
    memset(mon_list,0,MAX_MONSTERS*sizeof(*mon_list));
    o_max=mon_max=1;o_cnt=mon_cnt=0;
    for(int y=0;y<24;y++)for(int x=0;x<40;x++){
        cave_set_feat(y,x,FEAT_FLOOR);cave_info[y][x]=CAVE_MARK|CAVE_GLOW;
        cave_m_idx[y][x]=cave_o_idx[y][x]=0;
    }
    p_ptr->cur_map_hgt=24;p_ptr->cur_map_wid=40;p_ptr->depth=depth;
}
static void prepare(void)
{
    memset(p_ptr,0,sizeof(*p_ptr));memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();player_pack_action_reset();
    reset_map(1);rp_ptr=&p_info[0];current_character_profile=&c_info[0];
    p_ptr->py=p_ptr->px=5;cave_m_idx[5][5]=-1;
    p_ptr->playing=true;p_ptr->noscore=10;p_ptr->food=3000;
    p_ptr->mhp=p_ptr->chp=p_ptr->msp=p_ptr->csp=100;
    for(int i=0;i<A_MAX;i++)p_ptr->stat_base[i]=p_ptr->stat_use[i]=7;
    p_ptr->previous_action[0]=6;p_ptr->energy_use=0;
    character_generated=character_dungeon=true;character_icky=0;
    replacement_questions=0;turn=100;playerturn=10;
}
static void floor_item(int index,int tval,int sval,int quantity,int next)
{
    object_prep(&o_list[index],lookup_kind(tval,sval));o_list[index].number=quantity;
    object_known(&o_list[index]);o_list[index].iy=o_list[index].ix=5;
    o_list[index].marked=true;o_list[index].next_o_idx=next;
    o_max=MAX(o_max,index+1);o_cnt++;
}
static void bread(int quantity)
{
    object_type o;object_prep(&o,lookup_kind(TV_FOOD,SV_FOOD_BREAD));o.number=quantity;
    assert(supplies_absorb_object(&o));
    p_ptr->total_weight=supplies_total_weight();
}
static int units(int tval,int sval)
{
    int total=0;
    for(int i=0;i<supplies_entry_count();i++){
        object_type* o=supplies_entry_at(i);if(o&&o->tval==tval&&o->sval==sval)total+=o->number;
    }
    return total;
}
static void checks(void)
{
    for(int energy=0;energy<=100;energy+=50){
        prepare();floor_item(1,TV_FOOD,SV_FOOD_BREAD,2,0);cave_o_idx[5][5]=1;
        p_ptr->energy_use=energy;py_pickup_aux(1);
        assert(units(TV_FOOD,SV_FOOD_BREAD)==2&&!o_list[1].k_idx);
        assert(p_ptr->energy_use==(energy?energy:100));
        assert(p_ptr->previous_action[0]==(energy?6:ACTION_MISC));
        assert(turn==100&&playerturn==10);
    }
    prepare();bread(50);floor_item(1,TV_POTION,SV_POTION_HEALING,1,0);cave_o_idx[5][5]=1;
    py_pickup();assert(replacement_questions==1&&p_ptr->energy_use==0);
    assert(units(TV_FOOD,SV_FOOD_BREAD)==50&&o_list[1].number==1);
    prepare();bread(49);floor_item(2,TV_POTION,SV_POTION_HEALING,1,0);
    floor_item(1,TV_FOOD,SV_FOOD_BREAD,1,2);cave_o_idx[5][5]=1;
    py_pickup();assert(replacement_questions==1&&p_ptr->energy_use==100);
    assert(units(TV_FOOD,SV_FOOD_BREAD)==50&&!o_list[1].k_idx&&o_list[2].number==1);
    prepare();py_pickup();assert(!p_ptr->energy_use);
    prepare();floor_item(1,TV_SWORD,SV_DAGGER,1,0);cave_o_idx[5][5]=1;
    py_pickup_aux_to_pack(1);assert(player_pack_action_pending()&&p_ptr->energy_use==100);
    assert(o_list[1].number==1);player_pack_action_cancel();assert(o_list[1].number==1);
    puts("Pickup: menu commit, preserved 50/100-energy movement and direction, free rejected replacement/empty pile, mixed-pile payment and deferred Pack work PASS.");
}
'''


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    harness=HARNESS.replace('int main(int argc, char** argv)',CHECKS+'\nint main(int argc, char** argv)')
    harness=harness[:harness.index('    int last = 0;')]+'checks();sdl_quit_hook(NULL);return 0;\n}\n'
    source=OUT/'check.c';source.write_text(harness)
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response=OUT/'objects.rsp';response.write_text('\n'.join('"'+o+'"' for o in objects if not o.endswith('/src/main.c.obj')))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/d) for d in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),
        'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    env.update(SDL_VIDEO_DRIVER='dummy',SDL_AUDIO_DRIVER='dummy',SDL_RENDER_DRIVER='software')
    env['CONTROL_TEST_ASSETS']=str(ROOT/'lib')
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'@'+str(response),'@CMakeFiles/sil-more.dir/linkLibs.rsp',
        *('-Wl,--wrap='+n for n in ('open_inventory_item_select_menu','open_inventory_replacement_menu','get_check',
            'handle_stuff','msg_print','msg_format','tutorial_game_item','tutorial_game_action_allowed','tutorial_game_explain')),
        '-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='profile-',dir=OUT) as profile:
        subprocess.run([str(exe),'--windowed'],cwd=ROOT,
            env=dict(env,CONTROL_TEST_PROFILE=profile),check=True,timeout=30)


if __name__=='__main__':main()
