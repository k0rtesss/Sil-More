#!/usr/bin/env python3
"""Exercise real thrall requests, inventory transactions and gifts in isolation.

Only native presentation/selection boundaries are adapted. Uses production
thrall interactions, request assignment, item matching/consumption and rewards.
"""
from pathlib import Path
import argparse
import os, shlex, subprocess, tempfile
from check_monster_scent_save import ENGINE_FIXTURE, fixture_function, WRITER, READER
ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build-standard'
OUT=ROOT/'scripts/output/thrall-quest-gameplay'
CHECKS=r'''
#include "thrall_quest.h"
#include "supplies.h"
#include "ui/question.h"
#include "object/object-ui-select.h"
static int give_choice,selected_item,books,questions,revealed;
static bool cancel_picker,insight_offered;
static cptr gift="Come later";
void __wrap_quest_typewriter_menu_pages(cptr title,cptr texts[],int count,byte a,byte b,int pages)
{(void)title;(void)a;(void)b;assert(count>0 && texts[0] && pages==1);books++;}
void __wrap_desc_art_fake(int artifact){assert(artifact>0);revealed++;}
int __wrap_ui_question_ask(cptr title,cptr desc,const ui_question_option* options,int count,int y,int x,int initial)
{
    (void)desc;(void)y;(void)x;(void)initial;questions++;
    if(strncmp(title,"Give ",5)==0) return give_choice;
    insight_offered=false;
    for(int i=0;i<count;i++) if(strstr(options[i].label,"Insight point")) insight_offered=true;
    for(int i=0;i<count;i++) if(strstr(options[i].label,gift)) {assert(!options[i].disabled);return i;}
    fprintf(stderr,"Missing reward %s in %s\n",gift,title);exit(2);
}
bool __wrap_open_inventory_item_select_menu(int mode,cptr prompt,cptr failure,int* item)
{(void)mode;(void)prompt;(void)failure;if(cancel_picker){cancel_picker=false;gift="Come later";return false;}*item=selected_item;assert(item_tester_hook==NULL || item_tester_hook(player_inventory_object(*item)));return true;}
bool __wrap_display_unified_identify_menu(bool floor,int* item,object_type** object)
{(void)floor;if(cancel_picker){cancel_picker=false;gift="Come later";return false;}*item=selected_item;*object=player_inventory_object(*item);return true;}
bool __wrap_object_choice_overlay(cptr title,cptr desc,const object_choice_entry entries[],int count,int initial,int* out)
{(void)title;(void)desc;(void)entries;(void)initial;if(cancel_picker){cancel_picker=false;return false;}assert(count>0);*out=count-1;return true;}
static const int tvals[THRALL_QUEST_MAX]={0,TV_DIGGING,TV_LIGHT,TV_FOOD,TV_LIGHT,TV_POTION,TV_SWORD,TV_CLOAK,TV_BOOTS,TV_FOOD,TV_FOOD,TV_POTION,TV_FLASK,TV_LIGHT,TV_FOOD,TV_FOOD,TV_FOOD};
static const int svals[THRALL_QUEST_MAX]={0,SV_SHOVEL,SV_LIGHT_LANTERN,SV_FOOD_HEALING,SV_LIGHT_MALLORN,SV_POTION_HEALING,SV_DAGGER,SV_CLOAK,SV_PAIR_OF_LEATHER_BOOTS,SV_FOOD_SUSTENANCE,SV_FOOD_RESTORATION,SV_POTION_CLARITY,0,SV_LIGHT_TORCH,SV_FOOD_BREAD,SV_FOOD_MEAT,SV_FOOD_LEMBAS};
static monster_type* setup(int race,int request)
{
    memset(p_ptr,0,sizeof(*p_ptr));memset(inventory,0,INVEN_TOTAL*sizeof(*inventory));reset_map(10);
    player_carried_extra_reset_store();player_quiver_reset_store();supplies_reset_store();
    p_ptr->song1=p_ptr->song2=SNG_NOTHING;p_ptr->chp=p_ptr->mhp=100;p_ptr->playing=true;
    p_ptr->tutorial_deferred=true;p_ptr->stat_use[A_STR]=20;
    op_ptr->opt[OPT_insight_beta]=true;
    Rand_state_init(CAMPAIGN_SEED+race*31+request*103);
    turn=1000;playerturn=100;character_generated=character_dungeon=true;
    monster_type* m=&mon_list[1];m->r_idx=race;m->fy=m->fx=3;m->ml=true;
    mon_max=2;cave_m_idx[3][3]=1;m->thrall_quest_item=request;
    give_choice=0;selected_item=1;books=questions=revealed=0;cancel_picker=false;gift="Come later";
    return m;
}
static void requested_object(object_type* item,int request,int number)
{
    int kind=lookup_kind(tvals[request],svals[request]);assert(kind>0);object_prep(item,kind);item->number=number;
    if(request==THRALL_QUEST_FLASK_OIL)item->pval=FUEL_FLASK;
    if(request==THRALL_QUEST_WOODEN_TORCH)item->timeout=1000;
}
static void assignment(void)
{
    const int depths[]={1,3,6};
    for(int elf=0;elf<2;elf++)for(int dep=0;dep<3;dep++) {
        int depth=depths[dep];
        monster_type* m=setup(elf?R_IDX_ALERT_ELF_THRALL:R_IDX_ALERT_HUMAN_THRALL,0);p_ptr->depth=depth;
        unsigned seen=0;Rand_state_init(CAMPAIGN_SEED+depth*17+elf);
        for(int i=0;i<8000;i++) {
            init_thrall_quest(m);int q=m->thrall_quest_item;assert(q>0 && q<THRALL_QUEST_MAX);
            assert(depth>=6 || q!=THRALL_QUEST_LANTERN);assert(depth>=3 || q!=THRALL_QUEST_MALLORN);
            assert(elf?(q!=THRALL_QUEST_DARK_BREAD && q!=THRALL_QUEST_DRIED_MEAT):q!=THRALL_QUEST_LEMBAS);
            seen|=1U<<q;assert(m->thrall_quest_completed==THRALL_QUEST_STATE_ACTIVE);
        }
        unsigned expected=0x1fffe;
        expected &= elf?~((1U<<14)|(1U<<15)):~(1U<<16);
        if(depth<6)expected&=~(1U<<2);if(depth<3)expected&=~(1U<<4);
        assert(seen==expected);
    }
    puts("Human/elven natural request pools, all16 types and depth gates PASS.");
}
size_t fixture_write_monster(const monster_type*,byte*,size_t);
int fixture_read_current_monster(const byte*,size_t,monster_type*,u32b*,size_t*);
static void roundtrip_thrall(monster_type* m)
{
    byte encoded[8192];monster_type before=*m,restored={0};u32b sentinel=0;size_t consumed=0;
    size_t length=fixture_write_monster(m,encoded,sizeof(encoded));
    assert(fixture_read_current_monster(encoded,length,&restored,&sentinel,&consumed)==0);
    assert(sentinel==0xB4C3D2E1U && consumed==length);
    assert(restored.r_idx==before.r_idx && restored.fy==before.fy && restored.fx==before.fx);
    assert(restored.thrall_quest_item==before.thrall_quest_item);
    assert(restored.thrall_quest_requested==before.thrall_quest_requested);
    assert(restored.thrall_quest_completed==before.thrall_quest_completed);
    *m=restored;
}
static void item_transactions(void)
{
    for(int elf=0;elf<2;elf++)for(int step=0;step<THRALL_QUEST_MAX-1;step++) {
        int q=(step+CAMPAIGN_VARIANT*5)%16+1;
        monster_type* m=setup(elf?R_IDX_ALERT_ELF_THRALL:R_IDX_ALERT_HUMAN_THRALL,q);
        assert(player_has_thrall_quest_item(q)==-1);assert(handle_thrall_interaction(m));assert(books==1 && m->thrall_quest_requested);
        assert(handle_thrall_interaction(m));assert(books==1 && questions==0);
        int stack=2+CAMPAIGN_VARIANT,mode=(q+CAMPAIGN_VARIANT)%3;
        object_type incoming;requested_object(&incoming,q,stack);
        int slot;
        if(mode==2 && supplies_can_absorb_object(&incoming)) {
            assert(supplies_absorb_object(&incoming));slot=SUPPLIES_INDEX;
        } else if(mode!=0) {
            assert(player_carried_extra_load(&incoming));slot=CARRIED_EXTRA_INDEX;
        } else {object_copy(&inventory[0],&incoming);p_ptr->inven_cnt=1;slot=0;}
        assert(player_has_thrall_quest_item(q)==slot);
        object_type* item=slot>=SUPPLIES_INDEX?supplies_entry_at(slot-SUPPLIES_INDEX):player_inventory_object(slot);
        int experience=p_ptr->new_exp;
        give_choice=CAMPAIGN_VARIANT==1?-1:1;assert(handle_thrall_interaction(m));assert(item->number==stack && m->thrall_quest_completed==THRALL_QUEST_STATE_ACTIVE);
        experience=p_ptr->new_exp; /* Identification before the offer may earn ordinary identification XP. */
        give_choice=CAMPAIGN_VARIANT==1?1:-1;assert(handle_thrall_interaction(m));assert(item->number==stack && p_ptr->new_exp==experience);
        give_choice=0;assert(handle_thrall_interaction(m));assert(item->number==stack-1 && p_ptr->new_exp==experience+75);
        assert(m->thrall_quest_completed==THRALL_QUEST_STATE_FIRST_REWARD_PENDING);roundtrip_thrall(m);
        for(int wait=0;wait<=CAMPAIGN_VARIANT;wait++){assert(handle_thrall_interaction(m));assert(item->number==stack-1 && p_ptr->new_exp==experience+75);}
        gift="Insight point";assert(handle_thrall_interaction(m));assert(p_ptr->insight_points==1 && m->thrall_quest_completed==THRALL_QUEST_STATE_REWARDED);
        roundtrip_thrall(m);assert(handle_thrall_interaction(m));assert(p_ptr->insight_points==1 && item->number==stack-1);
        printf("Request %d race%d storage%d stack%d deferred%d PASS\n",q,elf,slot,stack,CAMPAIGN_VARIANT+1);
    }
    puts("All16 request transactions for both thralls: no-item/request-once, refuse/Escape, consume exactly1, XP75 once, saved pending/rewarded monster records, deferred first gift, Insight and replay PASS.");
}
static void supplies_and_equipment(void)
{
    const int qs[]={3,5,9,10,11,12,14,15,16};
    for(int i=0;i<(int)N_ELEMENTS(qs);i++) {
        int q=qs[i];monster_type* m=setup(R_IDX_ALERT_HUMAN_THRALL,q);object_type item;requested_object(&item,q,2);
        assert(supplies_absorb_object(&item));int slot=player_has_thrall_quest_item(q);assert(slot>=SUPPLIES_INDEX);
        int before_oil=player_lamp_oil();int before=supplies_entry_at(slot-SUPPLIES_INDEX)->number;
        complete_thrall_quest(m,slot);
        if(q==12)assert(player_lamp_oil()==MAX(0,before_oil-FUEL_FLASK));
        else assert(supplies_entry_at(slot-SUPPLIES_INDEX)->number==before-1);
        assert(p_ptr->new_exp==75 && m->thrall_quest_completed==THRALL_QUEST_STATE_FIRST_REWARD_PENDING);
    }
    monster_type* m=setup(R_IDX_ALERT_HUMAN_THRALL,THRALL_QUEST_DAGGER);
    requested_object(&inventory[INVEN_WIELD],6,1);inventory[INVEN_WIELD].ident|=IDENT_CURSED;
    assert(player_has_thrall_quest_item(6)==-1);complete_thrall_quest(m,INVEN_WIELD);assert(inventory[INVEN_WIELD].k_idx && !p_ptr->new_exp);
    inventory[INVEN_WIELD].ident&=~IDENT_CURSED;assert(player_has_thrall_quest_item(6)==INVEN_WIELD);
    complete_thrall_quest(m,INVEN_WIELD);assert(!inventory[INVEN_WIELD].k_idx && p_ptr->new_exp==75);
    m=setup(R_IDX_ALERT_HUMAN_THRALL,13);requested_object(&inventory[0],13,1);inventory[0].timeout=999;assert(player_has_thrall_quest_item(13)==-1);
    inventory[0].timeout=1000;assert(player_has_thrall_quest_item(13)==0);
    puts("Supplies9 kinds including oil, equipped cursed refusal/consumption, torch fuel boundary PASS.");
}
static void selection_and_deferral(void)
{
    monster_type* m=setup(R_IDX_ALERT_HUMAN_THRALL,6);
    requested_object(&inventory[0],6,2);requested_object(&inventory[1],6,2);p_ptr->inven_cnt=2;
    cancel_picker=true;assert(handle_thrall_interaction(m));
    assert(inventory[0].number==2 && inventory[1].number==2 && p_ptr->new_exp==0);
    assert(m->thrall_quest_completed==THRALL_QUEST_STATE_ACTIVE);
    assert(handle_thrall_interaction(m));assert(inventory[0].number==2 && inventory[1].number==1);
    m=setup(R_IDX_ALERT_HUMAN_THRALL,6);object_type dagger;requested_object(&dagger,6,2);
    assert(player_carried_extra_load(&dagger));assert(player_has_thrall_quest_item(6)==CARRIED_EXTRA_INDEX);
    assert(handle_thrall_interaction(m));assert(player_carried_extra_entry_at(0)->number==1 && p_ptr->new_exp==75);
    for(int service=0;service<3;service++) {
        bool elf=service==1;m=setup(elf?R_IDX_ALERT_ELF_THRALL:R_IDX_ALERT_HUMAN_THRALL,6);
        requested_object(&inventory[0],6,2);p_ptr->inven_cnt=2;
        object_prep(&inventory[1],lookup_kind(TV_HELM,SV_HELM));
        if(service==0){object_set_ego_prefix(&inventory[1],180);assert(object_apply_ego_affix(&inventory[1],180,true));gift="Repair a damaged item";}
        else if(service==1){inventory[1].ident|=IDENT_CURSED;gift="Sanctify";}
        else{object_prep(&inventory[1],lookup_kind(TV_POTION,SV_POTION_CLARITY));k_info[inventory[1].k_idx].aware=false;gift="Identify an item";}
        cptr claim=gift;cancel_picker=true;assert(handle_thrall_interaction(m));
        assert(m->thrall_quest_completed==THRALL_QUEST_STATE_FIRST_REWARD_PENDING && inventory[0].number==1 && p_ptr->new_exp==75);
        assert(item_tester_hook==NULL);
        gift=claim;assert(handle_thrall_interaction(m));assert(m->thrall_quest_completed==THRALL_QUEST_STATE_REWARDED && inventory[0].number==1);
    }
    puts("Multiple-match picker cancellation/exact selection, expandable pack, cancelled repair/sanctify/identify then later claim PASS.");
}
static void one_insight_per_life(void)
{
    monster_type* first=setup(R_IDX_ALERT_HUMAN_THRALL,6);
    requested_object(&inventory[0],6,2);p_ptr->inven_cnt=1;gift="Insight point";
    assert(handle_thrall_interaction(first));assert(p_ptr->insight_points==1 && insight_offered);
    monster_type* second=&mon_list[2];memset(second,0,sizeof(*second));
    second->r_idx=R_IDX_ALERT_ELF_THRALL;second->fy=3;second->fx=4;second->thrall_quest_item=6;mon_max=3;
    gift="Come later";assert(handle_thrall_interaction(second));
    assert(second->thrall_quest_completed==THRALL_QUEST_STATE_REWARD_PENDING && !insight_offered);
    assert(p_ptr->new_exp==150 && p_ptr->insight_points==1 && !inventory[0].k_idx);
    gift="Artefact knowledge";assert(handle_thrall_interaction(second));
    assert(second->thrall_quest_completed==THRALL_QUEST_STATE_REWARDED && p_ptr->new_exp==150 && p_ptr->insight_points==1);
    puts("First-thrall Insight offered once per life; later human/elven service reward remains claimable PASS.");
}
static void rewards(void)
{
    const cptr names[]={"Artefact knowledge","Repair a damaged item","Sanctify","Identify an item","Identify potions"};
    for(int step=0;step<5;step++) {
        int reward=(step+CAMPAIGN_VARIANT)%5;
        bool elf=reward==2 || reward==4;monster_type* m=setup(elf?R_IDX_ALERT_ELF_THRALL:R_IDX_ALERT_HUMAN_THRALL,6);
        requested_object(&inventory[0],6,2);p_ptr->inven_cnt=2;
        object_prep(&inventory[1],lookup_kind(TV_HELM,SV_HELM));
        if(reward==1){object_set_ego_prefix(&inventory[1],180);assert(object_apply_ego_affix(&inventory[1],180,true));assert(object_can_repair_damage(&inventory[1]));}
        if(reward==2)inventory[1].ident|=IDENT_CURSED;
        if(reward==3 || reward==4){object_prep(&inventory[1],lookup_kind(TV_POTION,SV_POTION_CLARITY));k_info[inventory[1].k_idx].aware=false;}
        gift=names[reward];assert(handle_thrall_interaction(m));
        assert(m->thrall_quest_completed==THRALL_QUEST_STATE_REWARDED && p_ptr->new_exp>=75);
        if(reward==0)assert(revealed==3);
        if(reward==1)assert(!object_is_damaged_item(&inventory[1]));
        if(reward==2)assert(!cursed_p(&inventory[1]));
        if(reward>=3)assert(object_known_p(&inventory[1]) && object_aware_p(&inventory[1]));
        assert(item_tester_hook==NULL);
    }
    puts("Actual artefact lore3, repair, sanctify, human identify and elven nature identification reward paths PASS.");
}
'''
def main():
    global OUT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed',type=int,default=101)
    parser.add_argument('--variant',type=int,choices=(0,1,2),default=0)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if args.output:OUT=args.output.resolve()
    print(f"Thrall campaign seed={args.seed} variant={args.variant}",flush=True)
    OUT.mkdir(parents=True,exist_ok=True)
    prefix=ENGINE_FIXTURE[:ENGINE_FIXTURE.index('static const char* guids[]')]+fixture_function('terminal_extra')+'\n'+fixture_function('reset_map')+'\n'
    init=ENGINE_FIXTURE[ENGINE_FIXTURE.index('int main(int argc,char** argv)'):];init=init[:init.index('    check_templates();')]
    prefix+=f'\n#define CAMPAIGN_SEED {args.seed}\n#define CAMPAIGN_VARIANT {args.variant}\n'
    source=OUT/'check.c';source.write_text(prefix+CHECKS+init+'''assert(init_flavor_info()==0);assignment();item_transactions();supplies_and_equipment();selection_and_deferral();one_insight_per_life();rewards();SDL_Quit();return 0;}''',encoding='utf-8')
    sources=[str(source)]
    for filename,body in [('writer.c',WRITER),('reader.c',READER)]:
        path=OUT/filename;path.write_text(body,encoding='utf-8');sources.append(str(path))
    objects=shlex.split((BUILD/'CMakeFiles/sil-more.dir/objects1.rsp').read_text());rsp=OUT/'objects.rsp';rsp.write_text('\n'.join('"'+p+'"' for p in objects if not p.endswith(('/src/main.c.obj','/src/fs/save.c.obj','/src/fs/load.c.obj'))))
    env=os.environ.copy();env['PATH']=os.pathsep.join([*(str(BUILD/'_deps'/p) for p in ('SDL','SDL_ttf','SDL_image','SDL_mixer')),'C:/msys64/mingw64/bin','C:/msys64/usr/bin',env['PATH']])
    wraps=['quest_typewriter_menu_pages','desc_art_fake','ui_question_ask','open_inventory_item_select_menu','display_unified_identify_menu','object_choice_overlay']
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-DUSE_SDL','-std=c17','-O0','-g','@CMakeFiles/sil-more.dir/includes_C.rsp',*sources,'@'+str(rsp),'@CMakeFiles/sil-more.dir/linkLibs.rsp',*['-Wl,--wrap='+w for w in wraps],'-o',str(exe)],cwd=BUILD,env=env,check=True)
    with tempfile.TemporaryDirectory(prefix='data-',dir=OUT) as data:subprocess.run([str(exe),str(ROOT/'lib/edit'),data],cwd=data,env=env,check=True,timeout=60)
if __name__=='__main__':main()
