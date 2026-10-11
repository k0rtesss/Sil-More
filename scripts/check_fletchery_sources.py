#!/usr/bin/env python3
"""Exercise Fletchery source selection and transactions with real engine stores.

Only the modal choice/quantity UI and repaint boundaries are fixtures. The
production flat selector builds its actual entries, and the production Fletch
command consumes cached torches or starts/finishes work on real Quiver handles.
No player saves or preferences are opened.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/fletchery-sources"


def function(source: str, marker: str) -> str:
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


CHECKS = r'''
#include "object/object-ui-select.h"
#include "object/object-internal.h"
#include "supplies.h"
bool inventory_menu_uses_expanded_supplies(void);
#ifndef USE_QUIVER
#define USE_QUIVER 0x08
#endif
static int wanted, offered[64], offered_count, quantity_max;
static bool cancel_choice;
static int active_change_reports;
static char active_change_message[160];
bool fixture_choice(cptr title,cptr desc,const object_choice_entry entries[],
    int count,int default_index,int *selected)
{
    (void)title;(void)desc;(void)default_index;
    offered_count=count;assert(count<=64);*selected=-1;
    for(int i=0;i<count;i++) {
        offered[i]=entries[i].item;
        assert(entries[i].text[0]);
        if(entries[i].item==wanted)*selected=i;
    }
    return !cancel_choice && *selected>=0;
}
s16b __wrap_get_quantity_action(cptr prompt,cptr action,int max)
{ assert(strstr(prompt,"Convert"));assert(streq(action,"Convert"));quantity_max=max;return 1; }
void __wrap_handle_stuff(void) { p_ptr->update=p_ptr->redraw=p_ptr->window=0; }
static void capture_message(cptr text)
{
    if(text && strstr(text,"Your active arrows are now ")) {
        active_change_reports++;
        SDL_strlcpy(active_change_message,text,sizeof(active_change_message));
    }
}
void __wrap_msg_print(cptr text) { capture_message(text); }
void __wrap_msg_format(cptr format,...)
{
    char text[256];va_list args;va_start(args,format);
    vsnprintf(text,sizeof(text),format,args);va_end(args);capture_message(text);
}
bool __wrap_tutorial_game_action_allowed(cptr action,const object_type *obj)
{ (void)action;(void)obj;return true; }
static bool offered_item(int item)
{ for(int i=0;i<offered_count;i++)if(offered[i]==item)return true;return false; }
static object_type fixture_object(int tval,int sval,int number)
{
    object_type obj;object_prep(&obj,lookup_kind(tval,sval));
    obj.number=number;obj.ident=IDENT_KNOWN;return obj;
}
static int quiver_count(int att)
{
    int result=0;
    for(int i=0;i<player_quiver_store_entry_count();i++) {
        object_type *obj=inventory_item_to_object_ptr(QUIVER_INDEX+i);
        if(obj && obj->att==att && !obj->name1)result+=obj->number;
    }
    return result;
}
static int supplies_count(int tval,int sval)
{
    int result=0;
    for(int i=0;i<supplies_entry_count();i++) {
        object_type *obj=supplies_entry_at(i);
        if(obj->tval==tval && obj->sval==sval)result+=obj->number;
    }
    return result;
}
static void prepare(void)
{
    reset_map(1);memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
    memset(p_ptr->active_ability,0,sizeof(p_ptr->active_ability));
    p_ptr->active_ability[S_ARC][ARC_FLETCHERY]=true;
    p_ptr->inven_cnt=p_ptr->equip_cnt=0;p_ptr->total_weight=0;
    p_ptr->energy_use=p_ptr->fletching=0;p_ptr->fletch_item=-1;
    p_ptr->get_item_mode=p_ptr->command_wrk=0;p_ptr->command_see=false;
    inventory_menu_set_expand_supplies(false);inventory_menu_set_include_equip(false);
    item_tester_tval=0;item_tester_hook=NULL;item_tester_full=false;
    cancel_choice=false;offered_count=quantity_max=0;
    object_type obj=fixture_object(TV_ARROW,SV_NORMAL_ARROW,20);
    assert(player_quiver_absorb_arrow(&obj)==20);
    obj=fixture_object(TV_ARROW,SV_NORMAL_ARROW,13);obj.att=3;
    assert(player_quiver_absorb_arrow(&obj)==13);
    obj=fixture_object(TV_ARROW,SV_NORMAL_ARROW,7);obj.att=1;
    assert(player_quiver_absorb_arrow(&obj)==7);
    inventory_item_to_object_ptr(QUIVER_INDEX+2)->name1=1;
    obj=fixture_object(TV_LIGHT,SV_LIGHT_TORCH,3);obj.timeout=1000;
    assert(supplies_absorb_object(&obj));
    obj=fixture_object(TV_FOOD,SV_FOOD_LEMBAS,2);assert(supplies_absorb_object(&obj));
    inventory[INVEN_LITE]=fixture_object(TV_LIGHT,SV_LIGHT_TORCH,1);
    inventory[INVEN_LITE].timeout=281;
}
static void active_selection_checks(void)
{
    int failures=0;
    for(int source_active=0;source_active<2;source_active++) {
        for(int partial=0;partial<2;partial++) {
            prepare();player_quiver_reset_store();
            u16b worked_note=quark_add("worked-source");
            u16b unrelated_note=quark_add("unrelated-ammo");
            object_type obj=fixture_object(TV_ARROW,SV_NORMAL_ARROW,20);
            obj.obj_note=worked_note;assert(player_quiver_absorb_arrow(&obj)==20);
            obj=fixture_object(TV_ARROW,SV_NORMAL_ARROW,7);
            obj.att=1;obj.obj_note=unrelated_note;
            assert(player_quiver_absorb_arrow(&obj)==7);
            assert(player_quiver_select_arrow(QUIVER_INDEX+(source_active?0:1)));
            wanted=QUIVER_INDEX;do_cmd_fletchery();
            assert(p_ptr->fletch_item==QUIVER_INDEX && p_ptr->fletching==20);
            active_change_reports=0;active_change_message[0]=0;
            finish_fletching(partial?15:0);
            assert(active_change_reports==0);
            object_type *selected=inventory_item_to_object_ptr(
                player_quiver_selected_arrow_slot());
            assert(selected && selected->k_idx);
            int expected_att=source_active?3:1;
            int expected_count=source_active?(partial?5:20):7;
            u16b expected_note=source_active?worked_note:unrelated_note;
            bool correct=selected->obj_note==expected_note
                && selected->att==expected_att && selected->number==expected_count;
            fprintf(stderr,"Fletch active=%s finish=%s selected='%s' att=%d count=%d; expected='%s' att=%d count=%d\n",
                source_active?"source":"unrelated",partial?"partial":"full",
                quark_str(selected->obj_note),selected->att,selected->number,
                quark_str(expected_note),expected_att,expected_count);
            if(!correct)failures++;
            int source_count=0,unrelated_count=0;
            for(int i=0;i<player_quiver_store_entry_count();i++) {
                object_type *arrow=inventory_item_to_object_ptr(QUIVER_INDEX+i);
                if(arrow->obj_note==worked_note)source_count+=arrow->number;
                if(arrow->obj_note==unrelated_note)unrelated_count+=arrow->number;
            }
            assert(source_count==20 && unrelated_count==7 && !player_pack_entry_count());
        }
    }
    assert(failures==0);
    prepare();player_quiver_reset_store();
    object_type ordinary=fixture_object(TV_ARROW,SV_NORMAL_ARROW,20);
    ordinary.obj_note=quark_add("exhausted-source");
    assert(player_quiver_absorb_arrow(&ordinary)==20);
    object_type spare=fixture_object(TV_ARROW,SV_NORMAL_ARROW,7);
    spare.att=1;spare.obj_note=quark_add("replacement-ammo");
    assert(player_quiver_absorb_arrow(&spare)==7);
    assert(player_quiver_select_arrow(QUIVER_INDEX));
    active_change_reports=0;active_change_message[0]=0;
    player_quiver_remove_arrows(QUIVER_INDEX,20);
    assert(active_change_reports==1 && active_change_message[0]);
    assert(inventory_item_to_object_ptr(player_quiver_selected_arrow_slot())->obj_note
        ==quark_add("replacement-ammo"));
    puts("Fletchery active ammo identity: full/partial source improves active result; nonactive work preserves unrelated active stack PASS");
    puts("Fletchery rebuild omits transient active-ammo announcements; ordinary stack exhaustion still reports replacement PASS");
}
static void checks(void)
{
    prepare();wanted=QUIVER_INDEX;int selected=-1;
    /* Legacy selectors continue to offer one supply summary, not Quiver. */
    assert(!open_inventory_item_select_menu(USE_INVEN|USE_EQUIP|USE_FLOOR,
        "Legacy selector",NULL,&selected));
    assert(!offered_item(QUIVER_INDEX));assert(offered_item(SUPPLIES_INDEX));
    assert(!inventory_menu_uses_expanded_supplies());

    /* The opt-in exposes real handles but still applies the caller's filter. */
    do_cmd_fletchery();
    assert(offered_item(QUIVER_INDEX));
    assert(!offered_item(QUIVER_INDEX+1)); /* Already +3. */
    assert(!offered_item(QUIVER_INDEX+2)); /* Artefact. */
    assert(offered_item(SUPPLIES_INDEX));assert(offered_item(INVEN_LITE));
    assert(p_ptr->fletch_item==QUIVER_INDEX && p_ptr->fletching==20);
    assert(p_ptr->energy_use==100);
    assert(item_tester_hook==NULL && !item_tester_tval && !p_ptr->get_item_mode);
    finish_fletching(15); /* An interruption after improving five arrows. */
    assert(quiver_count(0)==15 && quiver_count(3)==18);
    assert(player_pack_entry_count()==0);
    int artefact_count=0;
    for(int i=0;i<player_quiver_store_entry_count();i++) {
        object_type *obj=inventory_item_to_object_ptr(QUIVER_INDEX+i);
        if(obj->name1)artefact_count+=obj->number;
    }
    assert(artefact_count==7);
    puts("Fletchery real Quiver handle: eligibility, +3/artefact exclusions and interrupted five-arrow improvement PASS");

    prepare();wanted=QUIVER_INDEX;cancel_choice=true;
    s32b world=turn,actions=playerturn;
    item_tester_full=true;
    do_cmd_fletchery();
    assert(offered_item(QUIVER_INDEX));
    assert(p_ptr->energy_use==0 && !p_ptr->fletching && turn==world && playerturn==actions);
    assert(quiver_count(0)==20 && supplies_count(TV_LIGHT,SV_LIGHT_TORCH)==3);
    assert(item_tester_full && item_tester_hook==NULL && !item_tester_tval);
    assert(!p_ptr->get_item_mode && !p_ptr->command_wrk && !p_ptr->command_see);
    assert(!inventory_menu_uses_expanded_supplies());
    assert(!inventory_menu_set_include_equip(false));

    prepare();wanted=SUPPLIES_INDEX;
    do_cmd_fletchery();
    assert(quantity_max==3); /* Actual torch total, not the food summary. */
    assert(supplies_count(TV_LIGHT,SV_LIGHT_TORCH)==2);
    assert(supplies_count(TV_FOOD,SV_FOOD_LEMBAS)==2);
    assert(inventory[INVEN_LITE].number==1 && inventory[INVEN_LITE].timeout==281);
    int created=0;
    for(int i=0;i<player_pack_entry_count();i++) {
        object_type *obj=player_pack_entry_at(i);
        if(obj->tval==TV_ARROW && obj->att==3)created+=obj->number;
    }
    assert(created==3 && quiver_count(0)==20 && quiver_count(3)==13);
    assert(p_ptr->energy_use==100 && !p_ptr->fletching);
    assert(!inventory_menu_uses_expanded_supplies() && item_tester_hook==NULL);
    puts("Fletchery supply summary: one real torch consumed, three +3 Pack arrows, food/equipped light unchanged; cancel free and selection globals reset PASS");
    active_selection_checks();
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true",
                        help="Reproduce the legacy selector's missing Quiver source")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "checks(); SDL_Quit(); return 0;\n}\n")
    # Keep real entry construction/filtering; substitute only the modal choice
    # boundary so tests can inspect the offered handles and select/cancel one.
    selector = (ROOT / "src/object/object-ui-select.c").read_text()
    adapter = OUT / "selector.c"
    body = function(selector, "bool object_item_select_overlay(")
    if args.baseline:
        old_gate = "|| (mode & USE_QUIVER)"
        assert body.count(old_gate) == 1
        body = body.replace(old_gate, "")
    body = body.replace("bool object_item_select_overlay(", "bool __wrap_object_item_select_overlay(")
    body = body.replace("object_choice_overlay(reason", "fixture_choice(reason")
    adapter.write_text('#include "angband.h"\n#include "externs.h"\n#include "object/object-ui-select.h"\n'
        '#include "object/object-internal.h"\n#include "supplies.h"\n'
        'bool inventory_menu_uses_expanded_supplies(void);\n'
        'bool fixture_choice(cptr,cptr,const object_choice_entry[],int,int,int*);\n'
        + function(selector, "static bool object_item_select_add_entry(") + "\n" + body)
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/cmd/item/cmd-fletchery.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in
        ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin",
        "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0",
        "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(adapter),
        str(ROOT / "src/cmd/item/cmd-fletchery.c"), "@" + str(response),
        "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap=" + name for name in
        ("object_item_select_overlay", "get_quantity_action", "handle_stuff", "msg_print", "msg_format",
         "tutorial_game_action_allowed")), "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state,
                       env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()

