#!/usr/bin/env python3
"""Exercise new monster templates, authored vaults and combat using real engine objects.

Run build-incremental.ps1 first. Temporary raw data never touches player files.
Random treasure is disabled only in the in-memory vault layouts; exact monster
placement, template parsing and combat use the production engine unchanged.
"""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/new-monsters"

def check_layouts():
    text = (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8")
    for number in (521, 522):
        entry = text.split(f"N:{number}:", 1)[1].split("\nN:", 1)[0]
        rows = [line[2:] for line in entry.splitlines() if line.startswith("D:")]
        assert len({len(row) for row in rows}) == 1
        dry = {(y, x) for y, row in enumerate(rows)
               for x, token in enumerate(row) if token not in "#_7 "}
        seen = {point for point in dry if rows[point[0]][point[1]] == "$"}
        assert len(seen) == 2
        todo = list(seen)
        while todo:
            y, x = todo.pop()
            for point in ((y-1, x), (y+1, x), (y, x-1), (y, x+1)):
                if point in dry and point not in seen:
                    seen.add(point)
                    todo.append(point)
        assert seen == dry, f"Disconnected dry retreat or treasure at vault {number}"
        assert not set("<>0") & set("".join(rows)), "Unexpected travel or forge"
    print("Source layouts: rectangular, two exits, connected dry retreat/treasure routes PASS.", flush=True)


HARNESS = r'''

#include "angband.h"
#include "init/init2-internal.h"
#include "level-generation/level-generation-internal.h"
#include "monster/monster-abilities.h"
#include <assert.h>

static term test_term;
static const char* guids[] = {
    "4bb71145c50a9794", "7222d6d7c82f6571", "cbfc75c5e401080b",
    "9b6ad11d2169260e", "322c93f003d1b00a", "a1850b923ec90b6d",
    "e03ee4217593ca39", "23eb4b3c8f754c0a", "f1f38a4b25dfcc33"
};
static const int tiles[] = {13,14,18,19,20,21,22,23,24};
static const int hp[] = {65,120,150,80,40,60,60,160,150};

static errr terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_EVENT) Term_keypress(' ');
    return 0;
}

static void reset_map(int depth)
{
    memset(cave_info, 0, MAX_DUNGEON_HGT * sizeof(*cave_info));
    for (int y=0; y<MAX_DUNGEON_HGT; y++)
        for (int x=0; x<MAX_DUNGEON_WID; x++) {
            cave_feat[y][x] = FEAT_FLOOR;
            cave_light[y][x] = 2;
        }
    memset(cave_o_idx, 0, MAX_DUNGEON_HGT * sizeof(*cave_o_idx));
    memset(cave_m_idx, 0, MAX_DUNGEON_HGT * sizeof(*cave_m_idx));
    memset(o_list, 0, z_info->o_max * sizeof(*o_list));
    memset(mon_list, 0, MAX_MONSTERS * sizeof(*mon_list));
    memset(dun, 0, sizeof(*dun));
    for (int i=1; i<z_info->r_max; i++) {
        r_info[i].cur_num=0;
        r_info[i].max_num=(r_info[i].flags1 & RF1_UNIQUE) ? 1 : 100;
    }
    o_max = mon_max = 1;
    o_cnt = mon_cnt = 0;
    p_ptr->depth=depth;
    p_ptr->cur_map_hgt=p_ptr->cur_map_wid=88;
    p_ptr->py=p_ptr->px=2;
    current_build_vault_exact_token=false;
    current_build_vault_type=0;
}

static void check_templates(void)
{
    for (int i=0; i<9; i++) {
        int r=403+i;
        assert(monster_lookup_guid_text(guids[i])==r);
        assert((r_info[r].x_attr & 0x3f)==21);
        assert((r_info[r].x_char & 0x3f)==tiles[i]);
        assert(r_info[r].flags1 & RF1_UNIQUE);
        reset_map(20);
        current_build_vault_exact_token=true;
        assert(place_monster_one(10,10,r,false,true,NULL));
        current_build_vault_exact_token=false;
        monster_type* m=&mon_list[cave_m_idx[10][10]];
        assert(m->maxhp==hp[i] && m->hp==hp[i]);
    }
    assert(r_info[403].flags5 & RF5_CONCENTRATION);
    assert(r_info[403].flags2 & RF2_RIPOSTE);
    assert(r_info[403].per==6 && r_info[403].blow[0].att==17);
    assert(r_info[404].flags4 & RF4_SLOW);
    assert(r_info[406].flags2 & RF2_ZONE_OF_CONTROL);
    assert((r_info[406].flags3 & (RF3_ORC|RF3_RAUKO))==(RF3_ORC|RF3_RAUKO));
    assert(r_info[406].blow[1].effect==RBE_DISARM && r_info[406].blow[1].dd==0);
    assert(r_info[407].flags5 & RF5_SPRINTING);
    assert(r_info[407].speed==3);
    assert(r_info[408].flags5 & RF5_DODGING);
    assert(r_info[408].evn==18 && r_info[408].light==-3);
    assert((r_info[409].flags3 & (RF3_MAN|RF3_RAUKO))==(RF3_MAN|RF3_RAUKO));
    assert((r_info[409].flags4 & (RF4_RALLY|RF4_SCARE))==(RF4_RALLY|RF4_SCARE));
    assert(r_info[410].speed==1);
    assert(r_info[411].flags5 & RF5_SMITE);
    assert(r_info[82].flags5 & RF5_BLOCKING);
    assert(!(r_info[82].flags2 & RF2_FLANKING));
    assert(r_info[82].shield_dd==1 && r_info[82].pd==3 && r_info[82].ps==4);
    assert(r_info[253].flags5 & RF5_VENGEANCE);
    assert(r_info[253].speed==4 && r_info[253].level==25);
    assert(r_info[253].blow[0].dd==2 && r_info[253].blow[0].ds==12);
    assert(monster_lookup_guid_text("d41e3b6bfafb596f")==76);
    assert(strcmp(r_name+r_info[76].name,"Baugon, the Merciless")==0);
    assert(r_info[76].hdice==15 && r_info[76].blow[0].att==11);
    assert(monster_lookup_guid_text("90921d863b6a4eaa")==402);
    assert(r_info[402].hdice==40 && r_info[402].level==18);
    for (int i=0; i<alloc_race_size; i++) {
        int r=alloc_race_table[i].index;
        assert(r!=402 && r!=404 && r!=405 && r!=409 && r!=410 && r!=411);
    }
    int reserved[]={404,410,411};
    for (int i=0;i<3;i++) {
        int r=reserved[i];
        assert(r_info[r].rarity==0);
        assert(r_info[r].flags1 & RF1_SPECIAL_GEN);
        assert(r_info[r].flags3 & RF3_SPECIAL_VAULT_ONLY);
        reset_map(20);
        assert(!place_monster_one(10,10,r,false,false,NULL));
    }
    puts("Real templates: nine stable GUIDs/tiles/fixed unique HP; ability and shield fields; old identities; reserved allocation and normal placement exclusions PASS.");
}

static void check_vault(int v_idx, int r_idx, int depth, bool all_orientations)
{
    vault_type* v=&v_info[v_idx];
    char* layout=v_text+v->text;
    for(char* p=layout; *p; p++) if(strchr("!*&~",*p)) *p='.';
    unsigned orientations=0;
    /* Replay the two reflection choices, then verify the generated tokens.
     * The layouts are horizontally symmetric, so tokens alone cannot
     * distinguish both horizontal choices. */
    for(int diagonal=0;diagonal<=1;diagonal++)
    for(int seed=1;seed<=32;seed++) {
        reset_map(depth);
        Rand_state_init(seed);
        int expected_fy=0, expected_fx=0;
        if(depth>0 && depth<MORGOTH_DEPTH) {
            expected_fy=one_in_(2); expected_fx=one_in_(2);
        }
        Rand_state_init(seed);
        op_ptr->vault_drop_frequency=VDF_MEAGER;
        assert(build_vault(44,44,v,diagonal));
        int unique=0, warriors=0, archers=0;
        for(int i=1;i<mon_max;i++) {
            int r=mon_list[i].r_idx;
            if(r==r_idx) unique++;
            else if(r==82) warriors++;
            else if(r==103) archers++;
            else assert(r==0);
        }
        assert(unique==1);
        assert(warriors==(v_idx==522?2:0));
        assert(archers==(v_idx==522?2:0));
        assert(mon_cnt==(v_idx==522?5:1));
        assert(!place_vault_monster_token(v_idx==521?'i':'l',2,3));
        /* Check every exact token at each hypothesised orientation. */
        bool found_expected=false;
        for(int fy=0;fy<=1;fy++) for(int fx=0;fx<=1;fx++) {
            bool matches=true;
            for(int dy=0;dy<v->hgt;dy++) for(int dx=0;dx<v->wid;dx++) {
                char token=layout[dy*v->wid+dx];
                int race=token=='i'?405:token=='l'?409:token=='m'?82:token=='p'?103:0;
                int ay=fy?v->hgt-1-dy:dy, ax=fx?v->wid-1-dx:dx;
                int y=44-(diagonal?v->wid:v->hgt)/2+(diagonal?ax:ay);
                int x=44-(diagonal?v->hgt:v->wid)/2+(diagonal?ay:ax);
                if(race && mon_list[cave_m_idx[y][x]].r_idx!=race) matches=false;
                if(token=='_' && cave_feat[y][x]!=FEAT_WATER) matches=false;
            }
            if(matches && fy==expected_fy && fx==expected_fx) {
                found_expected=true;
                orientations|=1u<<(diagonal*4+fy*2+fx);
            }
        }
        assert(found_expected);
    }
    if(all_orientations) assert(orientations==255);
    printf("Real vault %d at depth %d: 64 placements, exact unique/escort counts and cap, orientation mask 0x%02x PASS.\n",v_idx,depth,orientations);
}


/* Fixtures alter only player defences and the placed monster's live state;
 * real race attacks, dice, dispatchers and scheduler remain unchanged. */
static monster_type* combat_fixture(int race, int evasion)
{
    memset(p_ptr,0,sizeof(*p_ptr));
    memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));
    reset_map(20);
    p_ptr->py=10; p_ptr->px=11;
    p_ptr->chp=p_ptr->mhp=20000;
    p_ptr->skill_use[S_EVN]=evasion;
    p_ptr->playing=true;
    p_ptr->food=PY_FOOD_FULL;
    current_build_vault_exact_token=true;
    assert(place_monster_one(10,10,race,false,true,NULL));
    current_build_vault_exact_token=false;
    monster_type* m=&mon_list[cave_m_idx[10][10]];
    m->ml=true; m->alertness=ALERTNESS_ALERT;
    m->skip_next_turn=m->skip_this_turn=false;
    m->cdis=1;
    combat_number=0;
    memset(combat_rolls,0,sizeof(combat_rolls));
    Rand_state_init(4321);
    return m;
}

static void check_combat(void)
{
    monster_type* m=combat_fixture(411,-100);
    monster_abilities_begin_action(m);
    int before=p_ptr->chp;
    assert(make_attack_normal(m));
    assert(combat_number==1);
    combat_roll roll=combat_rolls[0][0];
    assert(roll.att_type==COMBAT_ROLL_ROLL);
    assert(roll.att+roll.att_roll>roll.evn+roll.evn_roll);
    assert(roll.dd>=4 && roll.ds==9 && roll.dam==roll.dd*roll.ds);
    assert(p_ptr->chp==before-roll.dam);
    assert(m->mana==5 && m->smite_recovery==1 && m->skip_next_turn);
    assert(!make_attack_reaction(m));
    monster_abilities_end_action(m,10,10,false);
    before=p_ptr->chp;
    int old_mana=m->mana;
    m->energy=100;
    process_monsters(100);
    assert(m->energy==0 && m->fy==10 && m->fx==10);
    assert(p_ptr->chp==before && combat_number==1);
    assert(m->mana>=old_mana && !m->skip_next_turn && m->smite_recovery==2);
    assert(!make_attack_reaction(m));

    m=combat_fixture(411,1000);
    monster_abilities_begin_action(m);
    before=p_ptr->chp;
    assert(make_attack_normal(m));
    roll=combat_rolls[0][0];
    assert(roll.att+roll.att_roll<=roll.evn+roll.evn_roll);
    assert(p_ptr->chp==before);
    assert(m->mana==5 && m->smite_recovery==1 && m->skip_next_turn);
    monster_abilities_end_action(m,10,10,false);
    m->energy=100; process_monsters(100);
    assert(m->energy==0 && m->smite_recovery==2 && p_ptr->chp==before);
    puts("Real Smite attacks: normal hit/miss contest, all dice maximized on hit, cost 10 on either outcome; real scheduler spends one full recovery action and denies reactions PASS.");

    m=combat_fixture(253,-100);
    monster_abilities_begin_action(m);
    assert(make_attack_normal(m));
    int uncharged_dice=combat_rolls[0][0].dd;
    m=combat_fixture(253,-100);
    monster_receive_melee_damage(m,1);
    monster_receive_melee_damage(m,1);
    assert(m->vengeance==1);
    monster_abilities_begin_action(m);
    assert(make_attack_normal(m));
    assert(combat_rolls[0][0].dd==uncharged_dice+1 && m->vengeance==0);
    m=combat_fixture(253,1000);
    monster_receive_melee_damage(m,1);
    monster_abilities_begin_action(m);
    before=p_ptr->chp;
    assert(make_attack_normal(m));
    assert(m->vengeance==1 && p_ptr->chp==before);
    m=combat_fixture(253,-100);
    /* Fixed heavy protection makes the landed hit fully blocked. */
    inventory[INVEN_BODY].tval=TV_MAIL;
    inventory[INVEN_BODY].pd=255;
    inventory[INVEN_BODY].ps=1;
    monster_receive_melee_damage(m,1);
    monster_abilities_begin_action(m);
    before=p_ptr->chp;
    assert(make_attack_normal(m));
    assert(combat_rolls[0][0].dam<=combat_rolls[0][0].prot);
    assert(p_ptr->chp==before && m->vengeance==0);
    puts("Real Vengeance attacks: one non-stacking extra die, miss retains charge, landed fully blocked hit spends it PASS.");

    m=combat_fixture(403,1000);
    for(int i=0;i<4;i++) {
        monster_abilities_begin_action(m);
        assert(make_attack_normal(m));
        assert(combat_rolls[0][i].att==17+MIN(i,3));
        monster_abilities_end_action(m,10,10,false);
    }
    /* A real reaction uses base accuracy and leaves the sustained chain alone. */
    p_ptr->skill_use[S_EVN]=-100;
    assert(make_attack_reaction(m));
    assert(combat_rolls[0][4].att==17 && m->consecutive_attacks==3);
    monster_abilities_begin_action(m);
    monster_abilities_end_action(m,10,10,false); /* pass */
    assert(m->consecutive_attacks==0);
    puts("Real ordinary/reaction attacks: Concentration grows after misses to +3, reaction stays at base and preserves chain, waiting resets PASS.");

    m=combat_fixture(408,0);
    m->previous_action[0]=ACTION_MISC;
    int stationary=total_monster_evasion(m,false);
    m->previous_action[0]=6;
    assert(total_monster_evasion(m,false)==stationary+3);
    assert(stationary==18);
    puts("Real total_monster_evasion route: Dúron +18 stationary / +21 after movement PASS.");
    for(int moving=0; moving<=1; moving++) {
        m=combat_fixture(82,0);
        m->hp=m->maxhp=20000;
        m->previous_action[0]=moving?6:ACTION_MISC;
        p_ptr->skill_use[S_MEL]=100;
        p_ptr->mdd=p_ptr->mds=1;
        py_attack_aux(10,10,ATT_MAIN);
        assert(combat_number==1);
        assert(combat_rolls[0][0].pd==(moving?3:4));
        assert(combat_rolls[0][0].ps==4);
    }
    m=combat_fixture(82,0);
    m->hp=m->maxhp=20000;
    m->song_armor_dice_penalty=1;
    p_ptr->skill_use[S_MEL]=100;
    p_ptr->mdd=p_ptr->mds=1;
    py_attack_aux(10,10,ATT_MAIN);
    assert(combat_rolls[0][0].pd==3);
    puts("Real player-melee protection route: Blocking 4d4 stationary / 3d4 after movement, song penalty still subtracts one die PASS.");

    m=combat_fixture(253,0);
    m->hp=m->maxhp=20000;
    p_ptr->skill_use[S_MEL]=100;
    p_ptr->mdd=1; p_ptr->mds=50;
    py_attack_aux(10,10,ATT_MAIN);
    assert(m->hp<m->maxhp && m->vengeance==1);
    puts("Real incoming player-melee route: net damage arms Carcharoth's Vengeance PASS.");

    m=combat_fixture(407,0);
    p_ptr->px=30;
    calc_monster_speed(m->fy,m->fx);
    assert(m->mspeed==3);
    for(int step=0;step<4;step++) {
        int old_x=m->fx;
        int m_idx=cave_m_idx[m->fy][old_x];
        monster_abilities_begin_action(m);
        cave_m_idx[m->fy][old_x]=0;
        m->fx++;
        cave_m_idx[m->fy][m->fx]=m_idx;
        monster_abilities_end_action(m,m->fy,old_x,false);
        calc_monster_speed(m->fy,m->fx);
        assert(m->mspeed==(step==3?4:3));
    }
    m->hasted=1;
    calc_monster_speed(m->fy,m->fx);
    assert(m->mspeed==4);
    m->hasted=0;
    /* A future faster race must not be slowed by the Sprinting cap. */
    int old_speed=r_info[407].speed;
    r_info[407].speed=5;
    calc_monster_speed(m->fy,m->fx);
    assert(m->mspeed==5);
    r_info[407].speed=old_speed;
    monster_abilities_begin_action(m);
    monster_abilities_end_action(m,m->fy,m->fx,false);
    calc_monster_speed(m->fy,m->fx);
    assert(m->mspeed==3);
    puts("Real calc_monster_speed: Langon 3 to 4 after four moves, wait resets to 3, haste stays at 4, faster base 5 is preserved PASS.");


}

int main(int argc,char** argv)
{
    assert(argc==3);
    setbuf(stdout,NULL);
    log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT=argv[1];
    ANGBAND_DIR_DATA=ANGBAND_DIR_USER=ANGBAND_DIR_PREF=argv[2];
    assert(term_init(&test_term,80,24,256)==0);
    test_term.xtra_hook=terminal_extra;
    angband_term[0]=&test_term; Term_activate(&test_term);
    assert(init_z_info()==0); assert(init_f_info()==0);
    assert(init_k_info()==0); assert(init_b_info()==0);
    assert(init_a_info()==0); assert(init_e_info()==0);
    assert(init_r_info()==0); assert(init_v_info()==0);
    assert(init_p_info()==0); assert(init_c_info()==0);
    assert(init_oath_info()==0); assert(init_cu_info()==0); assert(init_mb_info()==0);
    assert(init_style_info()==0); assert(init_other()==0); assert(init_alloc()==0);
    rp_ptr=&p_info[0]; current_character_profile=&c_info[0];
    check_templates();
    assert(v_info[521].depth==19 && v_info[521].max_depth==20);
    assert(v_info[522].depth==14 && v_info[522].max_depth==15);
    for(int d=18;d<=21;d++) {
        reset_map(d);
        assert(vault_type8_is_eligible(521,false)==(d==19 || d==20));
    }
    check_vault(521,405,19,true);
    check_vault(521,405,20,false);
    check_vault(522,409,14,true);
    check_vault(522,409,15,true);
    check_combat();
    puts("New monster engine integration: PASS.");
    SDL_Quit();
    return 0;
}
'''

def main():
    check_layouts()
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    cmake_dir = BUILD / "CMakeFiles/sil-more.dir"
    objects = shlex.split((cmake_dir / "objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith("/src/main.c.obj")]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
        env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O0", "-g",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    with tempfile.TemporaryDirectory(prefix="data-", dir=OUT) as data:
        assert Path(data).resolve().is_relative_to(OUT.resolve())
        subprocess.run([str(exe), str(ROOT / "lib/edit"), data],
                       cwd=data, env=env, check=True, timeout=45)


if __name__ == "__main__":
    main()
