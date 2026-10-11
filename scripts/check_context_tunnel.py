#!/usr/bin/env python3
"""Exercise real Alter/Tunnel cost and terrain changes with a picker boundary.

The item picker is injected; this does not test its filter cleanup or SDL UI.
The commands, tool lookup, eligibility and terrain mutation are production C.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/context-tunnel'

CHECKS = r'''
static int picker_choice, picker_calls;
bool __wrap_get_rep_dir(int* direction) { *direction=6;return true; }
bool __wrap_open_inventory_item_select_menu(int mode,cptr reason,cptr none,int* item)
{
    (void)none;assert(mode==USE_INVEN);assert(strstr(reason,"digger"));
    picker_calls++;
    if(picker_choice<0)return false;
    object_type* tool=player_pack_entry_at(picker_choice);
    assert(tool);*item=player_inventory_handle_for_object(tool);
    assert(player_inventory_handle_valid(*item));return true;
}
void __wrap_handle_stuff(void) { p_ptr->update=p_ptr->redraw=p_ptr->window=0; }
void __wrap_msg_print(cptr message) { (void)message; }
bool __wrap_tutorial_game_command_allowed(char command,int direction)
{ (void)command;(void)direction;return true; }
bool __wrap_tutorial_game_action_allowed(cptr action,const object_type* item)
{ (void)action;(void)item;return true; }
static void setup(bool tools,bool confused)
{
    memset(p_ptr,0,sizeof(*p_ptr));reset_map(3);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store();player_quiver_reset_store();
    player_pack_action_reset();supplies_reset_store();
    p_ptr->playing=true;p_ptr->chp=p_ptr->mhp=100;
    p_ptr->stat_use[A_STR]=3;p_ptr->confused=confused?10:0;
    p_ptr->command_dir=6;p_ptr->previous_action[0]=ACTION_NOTHING;
    cave_m_idx[2][2]=-1;
    cave_feat[2][3]=FEAT_WALL_EXTRA;cave_info[2][3]=CAVE_MARK|CAVE_SEEN|CAVE_WALL;
    if(confused)for(int d=1;d<=9;d++)if(d!=5) {
        cave_feat[2+ddy[d]][2+ddx[d]]=FEAT_WALL_EXTRA;
        cave_info[2+ddy[d]][2+ddx[d]]=CAVE_MARK|CAVE_SEEN|CAVE_WALL;
    }
    if(tools)for(int i=0;i<2;i++) {
        object_type tool;object_prep(&tool,lookup_kind(TV_DIGGING,i?SV_MATTOCK:SV_SHOVEL));
        tool.number=1;tool.pval=i?3:1;tool.storage=OBJECT_STORAGE_PACK;
        object_known(&tool);object_aware(&tool);assert(player_carried_extra_load(&tool));
    }
    object_prep(&inventory[INVEN_LITE],lookup_kind(TV_LIGHT,SV_LIGHT_TORCH));
    inventory[INVEN_LITE].number=1;inventory[INVEN_LITE].timeout=281;
    turn=1001;playerturn=100;picker_calls=0;
}
static void checks(void)
{
    for(int direct=0;direct<2;direct++)for(int mode=0;mode<4;mode++) {
        bool confused=mode==3;setup(!confused,confused);
        picker_choice=mode==0?-1:mode==1?0:1;
        object_type before[2];
        if(!confused)for(int i=0;i<2;i++)object_copy(&before[i],player_pack_entry_at(i));
        int wall_before=cave_feat[2][3];
        if(direct)do_cmd_tunnel();else do_cmd_alter();
        int expected=(mode==2||confused)?100:0;
        fprintf(stderr,"%s %s: picker=%d energy=%d wall=%d->%d\n",
            direct?"Tunnel":"Alter",mode==0?"cancel":mode==1?"weak":mode==2?"capable":"confused missing",
            picker_calls,p_ptr->energy_use,wall_before,cave_feat[2][3]);
        assert(picker_calls==(confused?0:1));assert(p_ptr->energy_use==expected);
        if(mode==2)assert(cave_feat[2][3]!=wall_before);
        else assert(cave_feat[2][3]==wall_before);
        if(mode<2)assert(p_ptr->previous_action[0]==ACTION_NOTHING);
        if(!confused) {
            assert(player_pack_entry_count()==2);
            for(int i=0;i<2;i++)assert(!memcmp(&before[i],player_pack_entry_at(i),sizeof(before[i])));
        } else assert(player_pack_entry_count()==0);
        assert(turn==1001&&playerturn==100&&inventory[INVEN_LITE].timeout==281);
        assert(!player_pack_action_pending()&&p_ptr->chp==100);
    }
    puts("Alter/Tunnel: picker cancel and weak tool free, capable tool paid with terrain progress, confused missing tool paid PASS");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT/'check.c'
    source.write_text(prefix+CHECKS+init+'checks(); SDL_Quit(); return 0;\n}\n')
    command = ROOT/'src/cmd/world/cmd-interact.c'
    if args.baseline:
        current = command.read_text()
        start = current.index('            if (!open_inventory_item_select_menu(USE_INVEN,',current.index('static bool do_cmd_tunnel_aux'))
        end = current.index('            else',start)
        branch = current[start:end]
        assert branch.count('p_ptr->energy_use = 0;')==1
        branch = branch.replace('p_ptr->energy_use = 0;','').replace('p_ptr->previous_action[0] = ACTION_NOTHING;','')
        command = OUT/'interact-baseline.c'
        command.write_text(current[:start]+branch+current[end:])
    objects = shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT/'objects.rsp'
    response.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(
        ('/src/main.c.obj','/src/cmd/world/cmd-interact.c.obj'))))
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([*(str(BUILD/'_deps'/p) for p in
        ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    exe = OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0',
        '@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),str(command),'@'+str(response),
        '@CMakeFiles/sil-more.dir/linkLibs.rsp',*('-Wl,--wrap='+name for name in
        ('get_rep_dir','open_inventory_item_select_menu','handle_stuff','msg_print',
         'tutorial_game_command_allowed','tutorial_game_action_allowed')),'-o',str(exe)],
        cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-',dir=OUT) as state:
        subprocess.run([str(exe),str(ROOT/'lib/edit'),state],cwd=state,env=env,check=True,timeout=20)


if __name__ == '__main__':
    main()
