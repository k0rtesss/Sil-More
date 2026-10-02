#!/usr/bin/env python3
"""Check expandable inventory in thrall services, identification and depth costs.

Uses production engine code and templates in temporary directories. The item
picker is wrapped to select a stable synthetic handle; services remain real.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_new_monsters import HARNESS
from check_monster_scent_save import fixture_function

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "scripts/output/carried-services"

TESTS = r'''
#include "cave/cave-fixtures.h"
#include "thrall-under-test.c"
static int selected_item;
bool __wrap_open_inventory_item_select_menu(int mode, cptr prompt,
    cptr failure, int* item)
{
    (void)mode; (void)prompt; (void)failure;
    *item = selected_item;
    return true;
}
void __wrap_msg_print(cptr message) { (void)message; }
void __wrap_handle_stuff(void) { p_ptr->update = 0; }
int __wrap_ui_question_ask(cptr title, cptr desc,
    const ui_question_option* options, int count, int y, int x, int initial)
{
    (void)title; (void)desc; (void)options; (void)count;
    (void)y; (void)x; (void)initial;
    return -1; /* Leave an earned reward pending instead of entering its UI. */
}
int fixture_reforge_target(void);
bool fixture_reforge(void);
int fixture_melt_menu(int*);
int fixture_melt_item(int);
int fixture_forge_uses(void);
int fixture_prefix_choice(int, int*);

static void clear_items(void)
{
    reset_map(10);
    player_carried_extra_reset_store(); player_quiver_reset_store(); supplies_reset_store();
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    memset(p_ptr->have_ability,0,sizeof(p_ptr->have_ability));
    memset(p_ptr->active_ability,0,sizeof(p_ptr->active_ability));
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    playerturn=100; min_depth_counter=0;
    op_ptr->opt[OPT_insight_beta]=false;
}

static int add_extra(int tval,int sval)
{
    object_type item;
    int kind=lookup_kind(tval,sval); assert(kind>0);
    object_prep(&item,kind);
    assert(player_carried_extra_load(&item));
    return CARRIED_EXTRA_INDEX+player_carried_extra_entry_count()-1;
}

static int add_broken(void)
{
    int item=add_extra(TV_HELM,SV_HELM);
    object_type* object=player_inventory_object(item);
    object_set_ego_prefix(object,180);
    assert(object_apply_ego_affix(object,180,true));
    return item;
}

static void check_thrall_services(void)
{
    clear_items();
    int item=add_broken();
    assert(find_broken_item_to_upgrade()==item);
    assert(fixture_reforge_target()==item);
    assert(repair_damaged_item(item));
    assert(!object_is_damaged_item(player_inventory_object(item)));
    assert(!repair_damaged_item(-1));
    assert(!repair_damaged_item(CARRIED_EXTRA_INDEX+9));
    clear_items();
    item=add_extra(TV_HELM,SV_HELM);
    player_inventory_object(item)->ident|=IDENT_CURSED;
    assert(find_sanctifiable_item()==item);
    assert(sanctify_item(item));
    assert(!cursed_p(player_inventory_object(item)));
    assert(!sanctify_item(-1));
    clear_items();
    item=add_broken();
    selected_item=item;
    cave_set_feat(p_ptr->py,p_ptr->px,FEAT_FORGE_NORMAL_HEAD+3);
    p_ptr->active_ability[S_SMT][SMT_REPAIR]=true;
    int before=fixture_forge_uses();
    assert(fixture_reforge());
    assert(!object_is_damaged_item(player_inventory_object(item)));
    assert(fixture_forge_uses()==before-1);
    clear_items();
    item=add_extra(TV_POTION,SV_POTION_HEALING);
    object_type* object=player_inventory_object(item);
    k_info[object->k_idx].aware=false;
    assert(count_elven_identify_targets()==1);
    assert(count_carried_identify_targets()==1);
    assert(identify_elven_carried_items());
    assert(object_aware_p(object));
    assert(count_elven_identify_targets()==0);
    clear_items();
    item=add_extra(TV_SWORD,SV_DAGGER);
    object=player_inventory_object(item); object->number=2;
    monster_type* thrall=&mon_list[1];
    thrall->r_idx=R_IDX_ALERT_HUMAN_THRALL;
    thrall->fy=thrall->fx=8;
    thrall->thrall_quest_item=THRALL_QUEST_DAGGER;
    int experience=p_ptr->new_exp;
    complete_thrall_quest(thrall,item);
    assert(player_inventory_object(item)->number==1);
    assert(p_ptr->new_exp==experience+THRALL_QUEST_COMPLETION_EXP);
    assert(thrall->thrall_quest_completed==THRALL_QUEST_STATE_REWARD_PENDING);
    complete_thrall_quest(NULL,item);
    complete_thrall_quest(thrall,CARRIED_EXTRA_INDEX+9);
    assert(player_inventory_object(item)->number==1);
    clear_items();
    item=add_extra(TV_HELM,SV_MITHRIL_HELM);
    assert(fixture_melt_item(1)==item);
    assert(fixture_melt_item(2)==-1);
    int highlight=1;
    Term_keypress('\r');
    assert(fixture_melt_menu(&highlight)==1);
    clear_items();
    item=add_extra(TV_HELM,SV_HELM); selected_item=item;
    for (int i=0;i<ABILITIES_MAX;i++)
        p_ptr->active_ability[S_SMT][i]=true;
    p_ptr->skill_base[S_SMT]=100; p_ptr->new_exp=100000;
    cave_set_feat(p_ptr->py,p_ptr->px,FEAT_FORGE_NORMAL_HEAD+3);
    int prefix=0;
    int choice=fixture_prefix_choice(item,&prefix);
    assert(choice>=0 && prefix>0);
    for (int i=0;i<choice;i++) Term_keypress('2');
    Term_keypress('\r');
    assert(fixture_reforge());
    object=player_inventory_object(item);
    assert(object_ego_prefix(object)==prefix && object->unused1==2);
    assert(object_known_p(object));
    assert(fixture_forge_uses()==2);
    puts("Thrall repair/sanctification/identification, item hand-in, forge repair/prefix reforging and melting on synthetic handles PASS.");
}

static void check_identification(void)
{
    clear_items();
    int item=add_extra(TV_HELM,SV_HELM);
    object_type* object=player_inventory_object(item);
    pseudo_id_everything(); assert(object->ident&IDENT_SENSE);
    id_everything(); assert(object_known_p(object));
    clear_items();
    item=add_extra(TV_HELM,SV_HELM);
    object=player_inventory_object(item);
    k_info[object->k_idx].aware=false;
    assert(count_carried_identify_targets()==1);
    id_everything(); assert(object_known_p(object)&&object_aware_p(object));
    puts("Mass identification and pseudo-identification include expandable Pack/Harness entries PASS.");
}

static void check_timer(void)
{
    clear_items();
    int item=add_extra(TV_HELM,SV_HELM);
    object_type* object=player_inventory_object(item);
    u32b flags=k_info[object->k_idx].flags4;
    k_info[object->k_idx].flags4|=TR4_DEEP_CALL;
    int extra,legacy;
    min_depth_timer_status(NULL,&extra,NULL,NULL,NULL);
    object_copy(&inventory[0],object);
    player_carried_extra_reset_store();
    min_depth_timer_status(NULL,&legacy,NULL,NULL,NULL);
    assert(extra==legacy);
    assert(legacy>5*(p_ptr->depth-min_depth()));
    k_info[inventory[0].k_idx].flags4=flags;
    puts("Depth timer: carried Deep Call costs are equal in legacy and expandable slots PASS.");
}

static void check_description(void)
{
    clear_items();
    object_type item;
    object_prep(&item,lookup_kind(TV_HELM,SV_HELM));
    item.ident|=IDENT_SPOIL;
    char* original_names=k_name;
    u32b original_name=k_info[item.k_idx].name;
    char long_name[1800];
    memset(long_name,'a',sizeof(long_name)); long_name[sizeof(long_name)-1]=0;
    k_name=long_name; k_info[item.k_idx].name=0;
    struct { char text[128]; char guard[16]; } output;
    memset(&output,'Z',sizeof(output));
    object_desc(output.text,sizeof(output.text),&item,false,3);
    assert(output.text[sizeof(output.text)-1]==0);
    assert(!memcmp(output.guard,"ZZZZZZZZZZZZZZZZ",16));
    char untouched='Z'; object_desc(&untouched,0,&item,false,3); assert(untouched=='Z');
    k_name="~"; item.number=2;
    object_desc(output.text,sizeof(output.text),&item,false,0);
    assert(!strcmp(output.text,"s"));
    k_name=original_names; k_info[item.k_idx].name=original_name;
    item.number=1; item.discount=50;
    object_desc(output.text,sizeof(output.text),&item,false,3);
    assert(strstr(output.text,"50% off"));
    puts("Object descriptions: long names, pluralizer at the start, zero capacity and guarded output PASS.");
}

static void check_bridge_traps(void)
{
    clear_items();
    for (int feature=FEAT_BRIDGE_HEAD; feature<=FEAT_BRIDGE_TAIL; feature++)
    {
        cave_set_feat(8,8,feature);
        place_trap(8,8);
        assert(cave_feat[8][8]==feature);
    }
    cave_set_feat(8,8,FEAT_FLOOR); place_trap(8,8);
    assert(cave_trap_bold(8,8));
    puts("Generation traps preserve every typed bridge deck; ordinary floors still receive traps PASS.");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=("all","thrall","identify","timer","description","bridges"), default="all")
    parser.add_argument("--portable", action="store_true")
    args = parser.parse_args()
    build = ROOT / ("build-portable" if args.portable else "build-standard")
    out = OUT / ("portable" if args.portable else "standard")
    out.mkdir(parents=True, exist_ok=True)
    prefix = HARNESS[:HARNESS.index("static const char* guids[]")]
    reset = fixture_function("terminal_extra") + "\n" + fixture_function("reset_map")
    init = HARNESS[HARNESS.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = out / "check.c"
    (out / "thrall-under-test.c").write_text(
        (ROOT / "src/thrall_quest.c").read_text(encoding="utf-8").replace('#include "externs.h"', ''),
        encoding="utf-8")
    calls = "".join(f"    check_{name}();\n" for name in
                    (["thrall_services","identification","timer","description","bridge_traps"] if args.case=="all" else
                     [{"thrall":"thrall_services","identify":"identification","timer":"timer","description":"description","bridges":"bridge_traps"}[args.case]]))
    source.write_text(prefix+reset+TESTS+init+calls+"    SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    smith = out / "smith.c"
    smith.write_text('#include "cmd/ui/cmd-ui-smithing.c"\n'
                     'int fixture_reforge_target(void){return find_reforge_target_item();}\n'
                     'bool fixture_reforge(void){return smith_reforge_item();}\n'
                     'int fixture_melt_menu(int* n){return melt_menu_aux(n);}\n'
                     'int fixture_melt_item(int n){return smith_melt_item_handle_for_choice(n);}\n'
                     'int fixture_forge_uses(void){return forge_uses(p_ptr->py,p_ptr->px);}\n'
                     r'''
int fixture_prefix_choice(int item, int* prefix)
{
    object_type* source=player_inventory_object(item);
    int count=0;
    for (int i=1;i<z_info->e_max && count<26;i++) {
        reforge_preview_type preview;
        if (!ego_prefix_can_apply_to_object(source,i)
            || !reforge_preview_build(source,i,&preview)) continue;
        if (preview.affordable) { *prefix=i; return count; }
        count++;
    }
    return -1;
}
''', encoding="utf-8")
    units = ["dungeon/dungeon-identify", "cmd/movement/cmd-depth-bonus", "object/object-desc", "object/object-place"]
    excluded = ("/src/main.c.obj", "/src/thrall_quest.c.obj", "/src/cmd/ui/cmd-ui-smithing.c.obj",
                *(f"/src/{unit}.c.obj" for unit in units))
    objects = shlex.split((build/"CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    response = out / "objects.rsp"
    response.write_text("\n".join('"'+obj+'"' for obj in objects if not obj.endswith(excluded)), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(build/"_deps"/name) for name in ("SDL","SDL_ttf","SDL_image","SDL_mixer")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    executable = out / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe","-DUSE_SDL","-std=c17","-O0","-g","-fstack-protector-all",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp",str(source),str(smith),
                    *(str(ROOT/"src"/(unit+".c")) for unit in units),
                    "@"+str(response),"@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-Wl,--wrap=open_inventory_item_select_menu","-Wl,--wrap=msg_print",
                    "-Wl,--wrap=handle_stuff","-Wl,--wrap=ui_question_ask",
                    "-o",str(executable)], cwd=build,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix="data-",dir=out) as data:
        subprocess.run([str(executable),str(ROOT/"lib/edit"),data],cwd=data,env=env,check=True,timeout=20)


if __name__ == "__main__":
    main()
