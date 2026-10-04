#!/usr/bin/env python3
"""Exercise inscription selection with real supply, carried and floor objects.

Run the normal Windows build first. Uses current engine objects and an isolated
temporary template/config fixture; never opens a normal save or preferences.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

from check_monster_scent_save import ENGINE_FIXTURE, fixture_function

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-standard'
OUT = ROOT / 'scripts/output/inventory-inscriptions'

CHECKS = r'''
#include "supplies.h"
#include "object/object-internal.h"
#include "player/player-upkeep-internal.h"
#include "ui/menu-click.h"
#undef assert
#define assert(expr) do { if (!(expr)) { fprintf(stderr,"Assertion failed: %s line %d\n",#expr,__LINE__); exit(1); } } while (0)
static int next_key, input_calls, text_calls;
static bool crown_click_test, crown_confirmation;
static bool ordinary_delete_test, delete_confirmation;
static char labels[64];
static int label_count;
char __wrap_inkey(void)
{
    input_calls++;
    if (crown_click_test) {
        assert(input_calls<=(ordinary_delete_test?4:3));
        if (input_calls<=2) {
            bool wake=false;
            assert(!crown_confirmation && !delete_confirmation);
            assert(ui_menu_click_handle_choice_action(-9,
                input_calls==1?UI_MENU_CLICK_HOVER:UI_MENU_CLICK_PRIMARY,&wake));
            return UI_MENU_CLICK_WAKE_KEY;
        }
        if (ordinary_delete_test && input_calls==3) {
            bool wake=false;
            assert(!delete_confirmation);
            assert(ui_menu_click_handle_choice_action(10000,UI_MENU_CLICK_PRIMARY,&wake));
            return UI_MENU_CLICK_WAKE_KEY;
        }
        return ESCAPE;
    }
    assert(input_calls==1); return next_key;
}
void __wrap_message_flush(void) {}
/* Item identification's nested full-window redraw is unrelated to this
 * capacity fixture; run real bonus calculations without an SDL game window. */
void __wrap_id_known_specials(void) {}
bool __wrap_get_check(cptr prompt)
{
    if (crown_click_test && strstr(prompt,"prise a Silmaril"))
        crown_confirmation=true;
    if (ordinary_delete_test && strstr(prompt,"DELETE"))
        delete_confirmation=true;
    return false;
}
bool __wrap_tutorial_game_action_allowed(const char *action, const object_type *item)
{ (void)action; (void)item; return true; }
bool __wrap_term_get_string(cptr prompt, char *buf, size_t len)
{
    assert(strstr(prompt,"Inscription"));
    text_calls++;
    SDL_strlcpy(buf,"keep",len);
    return true;
}
void __wrap_sdl_question_menu_add_object_entry(int choice, cptr letter,
    cptr label, byte attr, const object_type *object)
{
    (void)choice; (void)label; (void)attr; (void)object;
    assert(label_count < 64);
    if (letter && letter[0] && letter[0] != '-') {
        for (int i=0;i<label_count;i++) assert(labels[i] != letter[0]);
        labels[label_count++]=letter[0];
    }
}
static void select_key(char key)
{ next_key=key; input_calls=label_count=0; }
static void check_inscriptions(void)
{
    object_type incoming;
    int food=lookup_kind(TV_FOOD,SV_FOOD_BREAD);
    int sword=lookup_kind(TV_SWORD,SV_SHORT_SWORD);
    assert(food && sword);
    supplies_reset_store();
    object_prep(&incoming,food); incoming.number=3;
    assert(supplies_absorb_object(&incoming));
    object_prep(&inventory[0],sword); inventory[0].number=1;
    object_prep(&inventory[INVEN_WIELD],sword); inventory[INVEN_WIELD].number=1;
    p_ptr->py=p_ptr->px=2;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=20;
    object_prep(&o_list[1],food); o_list[1].number=1;
    cave_o_idx[2][2]=1; o_max=2;
    inventory_menu_set_expand_supplies(false);
    select_key('a'); do_cmd_inscribe();
    assert(text_calls==1 && supplies_entry_at(0)->obj_note);
    assert(!inventory_menu_uses_expanded_supplies());
    assert(inventory[0].obj_note==0 && inventory[INVEN_WIELD].obj_note==0);
    select_key('b'); do_cmd_inscribe();
    assert(text_calls==2 && inventory[0].obj_note);
    select_key('c'); do_cmd_inscribe();
    assert(text_calls==3 && inventory[INVEN_WIELD].obj_note);
    select_key('-'); do_cmd_inscribe();
    assert(text_calls==4 && o_list[1].obj_note);
    select_key('a'); do_cmd_uninscribe();
    assert(!supplies_entry_at(0)->obj_note && inventory[0].obj_note);
    select_key(ESCAPE); do_cmd_inscribe();
    assert(text_calls==4 && !inventory_menu_uses_expanded_supplies());
    object_prep(&incoming,sword); incoming.number=1;
    for (int i=0;i<40;i++) assert(player_carried_extra_load(&incoming));
    select_key('0'); do_cmd_inscribe();
    assert(text_calls==5 && player_carried_extra_entry_at(24)->obj_note);
    select_key(ESCAPE); do_cmd_inscribe();
    assert(text_calls==5 && !inventory_menu_uses_expanded_supplies());
    player_carried_extra_reset_store();
    object_prep(&incoming,lookup_kind(TV_ARROW,1)); incoming.number=24;
    assert(player_quiver_absorb_arrow(&incoming)==24);
    select_key('c'); do_cmd_inscribe();
    assert(text_calls==6 && player_quiver_store_entry_at(0)->obj_note);
    select_key('c'); do_cmd_uninscribe();
    assert(!player_quiver_store_entry_at(0)->obj_note);
    puts("Inventory inscriptions: supply/carried/equipment/floor roundtrip, unique shortcuts and cancellation PASS.");
}
static void check_quiver_to_pack(void)
{
    object_type arrow;
    int robes=lookup_kind(TV_SOFT_ARMOR,SV_ROBE);
    int arrows=lookup_kind(TV_ARROW,1);
    int packed=0;
    assert(robes && arrows);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    player_pack_action_reset();
    object_prep(&inventory[0],robes); inventory[0].number=7;
    object_prep(&arrow,arrows); arrow.number=48;
    /* Reproduce a saved, formerly invisible stack restored into an overfull
     * Pack. Moving it to Quiver must revoke the old high-water allowance. */
    object_copy(&inventory[1],&arrow);
    inventory[1].storage=OBJECT_STORAGE_PACK;
    inventory_limit_grandfather_current_overflow();
    assert(player_quiver_absorb_arrow(&arrow)==48);
    object_wipe(&inventory[1]);
    character_generated=true; character_icky=0; character_xtra=false;
    calc_bonuses();
    /* The capacity normalization requires a live player. Keep unrelated
     * narrative/redraw work out of this focused terminal fixture afterward. */
    character_generated=false;
    assert(inventory_limit_usage_for_group(INV_LIMIT_PACK)==245);
    select_key('\r');
    assert(do_cmd_move_item_to_storage(QUIVER_INDEX,OBJECT_STORAGE_PACK));
    assert(player_pack_action_pending() && player_pack_action_turns_left()==2);
    assert(player_quiver_arrow_count()==48 && input_calls==0);
    player_pack_action_process();
    assert(player_pack_action_turns_left()==1 && player_quiver_arrow_count()==48);
    player_pack_action_process();
    assert(!player_pack_action_pending());
    for (int i=0;i<player_pack_entry_count();i++) {
        object_type *object=player_pack_entry_at(i);
        if (object->tval==TV_ARROW) {
            assert(object->storage==OBJECT_STORAGE_PACK);
            assert(inventory_limit_group_for_object(object)==INV_LIMIT_PACK);
            packed+=object->number;
        }
    }
    assert(packed>0 && packed<48);
    assert(packed+player_quiver_arrow_count()==48);
    assert(inventory_limit_usage_for_group(INV_LIMIT_PACK)<=260);
    puts("Quiver to Pack: three-turn delay, partial capacity, visible Pack storage and arrow conservation PASS.");
}
static void check_crown_footer(void)
{
    supply_menu_request request={0};
    int crown=ART_MORGOTH_3;
    int kind=lookup_kind(a_info[crown].tval,a_info[crown].sval);
    assert(kind);
    object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    inventory[INVEN_WIELD].number=1;
    object_prep(&o_list[1],kind); o_list[1].name1=crown;
    o_list[1].number=1; o_list[1].next_o_idx=0;
    o_list[1].iy=p_ptr->py; o_list[1].ix=p_ptr->px;
    cave_o_idx[p_ptr->py][p_ptr->px]=1; o_max=2;
    p_ptr->playing=true;
    request.focus_page=true; request.page=SUPPLY_MENU_PAGE_INVENTORY;
    request.focus_floor_item=true; request.floor_o_idx=1;
    crown_click_test=true; crown_confirmation=false; delete_confirmation=false;
    ordinary_delete_test=false; input_calls=0;
    (void)do_cmd_knowledge_supplies(&request);
    assert(crown_confirmation);
    assert(o_list[1].name1==crown && o_list[1].number==1);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store();
    object_prep(&o_list[1],lookup_kind(TV_SOFT_ARMOR,SV_ROBE));
    o_list[1].number=1; o_list[1].iy=p_ptr->py; o_list[1].ix=p_ptr->px;
    ordinary_delete_test=true; crown_confirmation=delete_confirmation=false;
    input_calls=0;
    (void)do_cmd_knowledge_supplies(&request);
    assert(delete_confirmation && !crown_confirmation && o_list[1].k_idx);
    crown_click_test=false;
    puts("Crown footer: hover is free, click confirms Prise directly; normal Delete still waits for row click; cancellation preserves objects PASS.");
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]
    prefix += fixture_function('terminal_extra') + '\n'
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):]
    init = init[:init.index('    check_templates();')]
    source = OUT / 'check.c'
    source.write_text(prefix + CHECKS + init +
                      '    check_inscriptions(); check_quiver_to_pack(); check_crown_footer(); SDL_Quit(); return 0;\n}\n', encoding='utf-8')
    objects = shlex.split((BUILD / 'CMakeFiles/sil-more.dir/objects1.rsp').read_text())
    response = OUT / 'objects.rsp'
    response.write_text('\n'.join('"' + p + '"' for p in objects
                                 if not p.endswith('/src/main.c.obj')), encoding='utf-8')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([
        *(str(BUILD / '_deps' / p) for p in ('SDL', 'SDL_ttf', 'SDL_image', 'SDL_mixer')),
        'C:/msys64/mingw64/bin', 'C:/msys64/usr/bin', env['PATH']])
    exe = OUT / 'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe', '-DUSE_SDL', '-std=c17', '-O0',
                    '@CMakeFiles/sil-more.dir/includes_C.rsp', str(source), '@' + str(response),
                    '@CMakeFiles/sil-more.dir/linkLibs.rsp',
                    *(f'-Wl,--wrap={name}' for name in ('inkey', 'message_flush', 'id_known_specials', 'get_check',
                      'tutorial_game_action_allowed', 'term_get_string',
                      'sdl_question_menu_add_object_entry')), '-o', str(exe)],
                   cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix='fixture-', dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / 'lib/edit'), state], cwd=state,
                       env=env, check=True, timeout=15)


if __name__ == '__main__':
    main()

