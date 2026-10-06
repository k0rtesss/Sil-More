#!/usr/bin/env python3
"""Validate real paired equip, bonus cleanup and active-weapon previews.

Uses authored artefacts/set data and production commands/calculations. Only
input, tutorial and redraw boundaries are wrapped; no player save is opened.
--baseline compiles the committed bonus/preview code as a negative control.
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
OUT = ROOT / "scripts/output/paired-offhand"
PREVIEW = r'''
#include "player/player-active-weapon.c"
#include <assert.h>
bool fixture_preview(int* attack, int* dd, int* ds)
{
    active_weapon_preview preview;
    player_type before=*p_ptr;
    object_type items[INVEN_TOTAL]; memcpy(items,inventory,sizeof(items));
    assert(active_weapon_choice_preview(PLAYER_ACTIVE_WEAPON_MELEE,
        INVEN_WIELD,INVEN_WIELD,-1,-1,&preview));
    assert(!memcmp(&before,p_ptr,sizeof(before)));
    assert(!memcmp(items,inventory,sizeof(items)));
    *attack=preview.offhand_attack; *dd=preview.offhand_dd; *ds=preview.offhand_ds;
    return preview.has_offhand;
}
'''
CHECKS = r'''
#include "item_set.h"
#include "fs/path.h"
#include "fs/io_sdl.h"
#include "player/player-upkeep-internal.h"
#undef assert
#define assert(test) do { if (!(test)) { fprintf(stderr,"FAIL: %s line %d\n",#test,__LINE__); exit(1); } } while (0)
extern bool fixture_preview(int*,int*,int*);
extern bool sdl_combat_overlay_melee_uses_offhand_row(void);
extern bool sdl_combat_overlay_source_row_at_index(int,int*);
static bool confirm=true;
static int prompts;
bool __wrap_get_check(cptr text) { (void)text; prompts++; return confirm; }
void __wrap_handle_stuff(void) { }
void __wrap_msg_print(cptr text) { (void)text; }
bool __wrap_tutorial_game_action_allowed(const char* action,const object_type* item)
{ (void)action; (void)item; return true; }
void __wrap_tutorial_game_action_done(const char* action,const object_type* item)
{ (void)action; (void)item; }
void __wrap_tutorial_game_identified(const object_type* item,const char* event)
{ (void)item; (void)event; }
char __wrap_inkey(void) { assert(!"Unexpected UI input"); return 0; }
cptr __wrap_get_sdl_config_path(void) { return "fixture-sdl.json"; }
static void prepare(void)
{
    memset(p_ptr,0,sizeof(*p_ptr)); reset_map(1);
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    player_carried_extra_reset_store(); player_quiver_reset_store(); supplies_reset_store();
    p_ptr->py=p_ptr->px=5; cave_m_idx[5][5]=-1;
    p_ptr->playing=true; p_ptr->chp=p_ptr->mhp=100;
    p_ptr->food=PY_FOOD_FULL-1000; p_ptr->song1=p_ptr->song2=SNG_NOTHING;
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    p_ptr->skill_base[S_MEL]=20;
    for(int i=0;i<A_MAX;i++)p_ptr->stat_base[i]=5;
    character_dungeon=character_generated=true; turn=100; playerturn=10;
    op_ptr->opt[OPT_insight_beta]=false;
    confirm=true; prompts=0;
}
static void artefact(object_type* object,int id)
{
    assert(make_fake_artefact(object,(byte)id)); object->number=1;
    object_aware(object); object_known(object);
}
static void load_sets(void)
{
    char path[1024],line[1024]; header head; init_header(&head,1,1);
    item_sets_reset(); path_build(path,sizeof(path),ANGBAND_DIR_EDIT,"set.txt");
    SDL_IOStream* file=sdl_fopen(path,"r"); assert(file);
    assert(!init_info_txt(file,line,&head,parse_set_info)); sdl_fclose(file);
    assert(!item_sets_finalize()); assert(get_paired_artefact(65)==64);
}
static void verify_preview(void)
{
    int attack,dd,ds;
    assert(fixture_preview(&attack,&dd,&ds));
    assert(attack==p_ptr->skill_use[S_MEL]+p_ptr->offhand_mel_mod);
    assert(dd==p_ptr->mdd2 && ds==p_ptr->mds2 && dd>0 && ds>0);
}
static void verify_overlay(bool offhand)
{
    int row;
    assert(sdl_combat_overlay_melee_uses_offhand_row()==offhand);
    assert(sdl_combat_overlay_source_row_at_index(0,&row));
    assert(row==(offhand?ROW_MEL-1:ROW_MEL));
    assert(sdl_combat_overlay_source_row_at_index(1,&row));
    assert(row==(offhand?ROW_MEL:ROW_ARC));
}
static void checks(void)
{
    load_sets(); prepare();
    artefact(&inventory[INVEN_WIELD],65); p_ptr->equip_cnt=1;
    object_type mate; artefact(&mate,64);
    assert(player_carried_extra_load(&mate));
    assert(!p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    do_cmd_wield(player_inventory_object(CARRIED_EXTRA_INDEX),CARRIED_EXTRA_INDEX);
    assert(prompts==1 && inventory[INVEN_ARM].name1==64);
    assert(p_ptr->energy_use==100); calc_bonuses();
    assert(inventory[INVEN_ARM].name1==64 && inventory[INVEN_WIELD].name1==65);
    assert(!p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    assert(p_ptr->mdd2==total_mdd(&inventory[INVEN_ARM]));
    assert(p_ptr->mds2==total_mds(&inventory[INVEN_ARM],0));
    assert(p_ptr->offhand_mel_mod==inventory[INVEN_ARM].att-inventory[INVEN_WIELD].att);
    verify_preview(); verify_overlay(true);
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_RANGED_1;
    assert(!sdl_combat_overlay_melee_uses_offhand_row());
    int row; assert(sdl_combat_overlay_source_row_at_index(0,&row) && row==ROW_ARC);
    assert(sdl_combat_overlay_source_row_at_index(1,&row) && row==ROW_QUIVER);
    p_ptr->active_weapon_mode=PLAYER_ACTIVE_WEAPON_MELEE;
    puts("Matched authored pair: legacy equip, retained without TWF, zero penalties, exact preview and melee/ranged overlay rows PASS.");

    object_type replacement; object_prep(&replacement,lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    replacement.number=1; assert(player_carried_extra_load(&replacement));
    assert(do_cmd_wield_to_slot(player_inventory_object(CARRIED_EXTRA_INDEX),CARRIED_EXTRA_INDEX,INVEN_WIELD));
    calc_bonuses(); assert(!inventory[INVEN_ARM].k_idx);
    assert(!inventory[INVEN_WIELD].name1 && !p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    int attack,dd,ds; assert(!fixture_preview(&attack,&dd,&ds));
    verify_overlay(false);
    puts("Breaking the pair via real main-hand replacement: stale offhand cleaned and preview absent PASS.");

    prepare(); object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    object_prep(&inventory[INVEN_ARM],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    inventory[INVEN_WIELD].number=inventory[INVEN_ARM].number=1; p_ptr->equip_cnt=2;
    assert(!player_offhand_weapon_allowed(&inventory[INVEN_WIELD],&inventory[INVEN_ARM]));
    calc_bonuses(); assert(!inventory[INVEN_ARM].k_idx && p_ptr->mdd2==0);
    assert(!fixture_preview(&attack,&dd,&ds));
    verify_overlay(false);
    puts("Unmatched without TWF: rejected policy, real cleanup and absent preview PASS.");

    prepare(); object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    object_prep(&inventory[INVEN_ARM],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    inventory[INVEN_WIELD].number=inventory[INVEN_ARM].number=1; p_ptr->equip_cnt=2;
    p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]=true;
    p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=true;
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=true;
    calc_bonuses(); assert(inventory[INVEN_ARM].k_idx);
    assert(p_ptr->mds2==total_mds(&inventory[INVEN_ARM],-3)); verify_preview(); verify_overlay(true);
    puts("Learned active TWF: retained with normal strength penalty and exact preview PASS.");

    prepare(); artefact(&inventory[INVEN_WIELD],65); p_ptr->equip_cnt=1;
    artefact(&mate,64); assert(player_carried_extra_load(&mate));
    inventory[INVEN_WIELD].ident|=IDENT_CURSED;
    object_type before=*player_inventory_object(CARRIED_EXTRA_INDEX);
    inventory[INVEN_ARM].ident|=IDENT_CURSED; inventory[INVEN_ARM].k_idx=lookup_kind(TV_SHIELD,SV_ROUND_SHIELD);
    do_cmd_wield(player_inventory_object(CARRIED_EXTRA_INDEX),CARRIED_EXTRA_INDEX);
    assert(!p_ptr->energy_use && !memcmp(&before,player_inventory_object(CARRIED_EXTRA_INDEX),sizeof(before)));
    assert(inventory[INVEN_WIELD].name1==65);
    puts("Cursed destination: legacy paired offer rejects without transfer/payment PASS.");

    prepare(); object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    inventory[INVEN_WIELD].number=1; p_ptr->equip_cnt=1;
    object_prep(&mate,lookup_kind(TV_SWORD,SV_SHORT_SWORD)); mate.number=1;
    mate.abilities=1; mate.skilltype[0]=S_MEL; mate.abilitynum[0]=MEL_TWO_WEAPON;
    object_aware(&mate); object_known(&mate); assert(player_carried_extra_load(&mate));
    assert(do_cmd_can_wield_offhand(player_inventory_object(CARRIED_EXTRA_INDEX)));
    assert(do_cmd_wield_to_slot(player_inventory_object(CARRIED_EXTRA_INDEX),CARRIED_EXTRA_INDEX,INVEN_ARM));
    calc_bonuses(); assert(inventory[INVEN_ARM].k_idx);
    assert(p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON] && p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    verify_preview(); puts("New known item-granted TWF: actual equip activates and retains weapon PASS.");

    prepare(); object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    inventory[INVEN_WIELD].number=1; p_ptr->equip_cnt=1;
    assert(player_carried_extra_load(&mate));
    p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]=true;
    p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=true;
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=false;
    before=*player_inventory_object(CARRIED_EXTRA_INDEX);
    object_type main=inventory[INVEN_WIELD];
    assert(!do_cmd_can_wield_offhand(player_inventory_object(CARRIED_EXTRA_INDEX)));
    assert(!do_cmd_wield_to_slot(player_inventory_object(CARRIED_EXTRA_INDEX),CARRIED_EXTRA_INDEX,INVEN_ARM));
    assert(!p_ptr->energy_use && !inventory[INVEN_ARM].k_idx);
    assert(!memcmp(&before,player_inventory_object(CARRIED_EXTRA_INDEX),sizeof(before)));
    assert(!memcmp(&main,&inventory[INVEN_WIELD],sizeof(main)));
    assert(!p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    puts("Learned TWF intentionally off: actual forced equip rejects freely and preserves toggle/source PASS.");
    do_cmd_wield(player_inventory_object(CARRIED_EXTRA_INDEX),CARRIED_EXTRA_INDEX);
    assert(prompts==0 && p_ptr->energy_use==100);
    calc_bonuses(); assert(inventory[INVEN_WIELD].abilities==1 && !inventory[INVEN_ARM].k_idx);
    assert(!p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]);
    puts("Legacy owned-off grant: ordinary main equip remains legal, no offhand offer or silent reactivation PASS.");

    prepare(); object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    object_prep(&inventory[INVEN_ARM],lookup_kind(TV_SHIELD,SV_ROUND_SHIELD));
    inventory[INVEN_WIELD].number=inventory[INVEN_ARM].number=1; p_ptr->equip_cnt=2;
    calc_bonuses(); assert(inventory[INVEN_ARM].tval==TV_SHIELD);
    assert(!p_ptr->mdd2 && !fixture_preview(&attack,&dd,&ds)); verify_overlay(false);
    puts("Shield without TWF: retained as shield, no second attack/preview PASS.");

    prepare(); object_prep(&inventory[INVEN_WIELD],lookup_kind(TV_SWORD,SV_SHORT_SWORD));
    inventory[INVEN_WIELD].number=1; p_ptr->equip_cnt=1;
    p_ptr->innate_ability[S_MEL][MEL_TWO_WEAPON]=true;
    p_ptr->have_ability[S_MEL][MEL_TWO_WEAPON]=true;
    p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]=true;
    object_prep(&mate,lookup_kind(TV_SWORD,SV_DAGGER)); mate.number=1;
    object_aware(&mate); object_known(&mate); assert(player_carried_extra_load(&mate));
    main=inventory[INVEN_WIELD];
    assert(do_cmd_wield_to_slot(player_inventory_object(CARRIED_EXTRA_INDEX),CARRIED_EXTRA_INDEX,INVEN_ARM));
    assert(p_ptr->energy_use==100 && prompts==0);
    assert(!memcmp(&main,&inventory[INVEN_WIELD],sizeof(main)));
    calc_bonuses(); assert(inventory[INVEN_ARM].sval==SV_DAGGER && inventory[INVEN_ARM].tval==TV_SWORD);
    verify_preview(); verify_overlay(true);
    puts("Throwing-capable Dagger: actual explicit Off-hand equip bypasses slot picker, preserves main and costs one turn PASS.");
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    prefix = ENGINE_FIXTURE[:ENGINE_FIXTURE.index("static const char* guids[]")]
    prefix += fixture_function("terminal_extra") + "\n" + fixture_function("reset_map") + "\n"
    init = ENGINE_FIXTURE[ENGINE_FIXTURE.index("int main(int argc,char** argv)"):]
    init = init[:init.index("    check_templates();")]
    source = OUT / "check.c"
    source.write_text(prefix + CHECKS + init + "    assert(!init_flavor_info()); flavor_init(); checks(); SDL_Quit(); return 0;\n}\n", encoding="utf-8")
    preview = OUT / "preview.c"
    preview.write_text(PREVIEW, encoding="utf-8")
    bonuses = ROOT / "src/player/player-bonuses.c"
    if args.baseline:
        bonuses = OUT / "bonuses-baseline.c"
        bonuses.write_bytes(subprocess.check_output(["git", "-c", f"safe.directory={ROOT.as_posix()}", "show", "HEAD:src/player/player-bonuses.c"], cwd=ROOT))
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    excluded = ("/src/main.c.obj", "/src/player/player-active-weapon.c.obj", "/src/player/player-bonuses.c.obj", "/src/cmd/item/cmd-item-core.c.obj", "/src/sdl/core/sdl-layout.c.obj")
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"'+p+'"' for p in objects if not p.endswith(excluded)))
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([*(str(BUILD / "_deps" / p) for p in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")), "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    exe = OUT / "check.exe"
    wrapped = ("get_check", "handle_stuff", "msg_print", "inkey", "tutorial_game_action_allowed", "tutorial_game_action_done", "tutorial_game_identified", "get_sdl_config_path")
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g", "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source), str(preview), str(bonuses), str(ROOT / "src/cmd/item/cmd-item-core.c"), str(ROOT / "src/sdl/core/sdl-layout.c"), "@"+str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp", *("-Wl,--wrap="+s for s in wrapped), "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="fixture-", dir=OUT) as state:
        subprocess.run([str(exe), str(ROOT / "lib/edit"), state], cwd=state, env=env, check=True, timeout=30)

if __name__ == "__main__":
    main()
