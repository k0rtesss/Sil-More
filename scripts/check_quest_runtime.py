#!/usr/bin/env python3
"""Behavioral tests using the production quest runtime, accounting, and save blocks."""
from pathlib import Path
import os
import subprocess
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'scripts/output/quest-runtime'
HARNESS = r'''
#include "angband.h"
#include "externs.h"
#include "metarun.h"
#include "quest/quest-runtime.h"
#include "quest/quest-internal.h"
#include "quest/quest-rewards-beta.h"
#include "quest/quest-challenges.h"
#include <assert.h>
#include <stdarg.h>
static player_type player;
player_type *p_ptr=&player;
static maxima maxima_test={.r_max=700,.quest_max=24};
maxima *z_info=&maxima_test;
static quest_type templates[24];
quest_type *quest_info=templates;
char *quest_name_text="\0quest";
static monster_race races[700]; monster_race *r_info=races;
static monster_lore lore[700]; monster_lore *l_list=lore;
char *r_name="dragon\0fire hatchling";
static monster_type monsters[4]; monster_type *mon_list=monsters;
static s16b map[4][MAX_DUNGEON_WID]; s16b (*cave_m_idx)[MAX_DUNGEON_WID]=map;
bool *valar_reserved_artifacts;
void cave_set_feat(int y,int x,int f){(void)y;(void)x;(void)f;}
void tulkas_quest_interaction(void){}
void aule_quest_interaction(void){}
void mandos_quest_interaction(void){}
void niena_quest_interaction(void){}
void orome_quest_interaction(void){}
void varda_quest_interaction(void){}
size_t strnfmt(char *s,size_t n,cptr fmt,...){va_list a;va_start(a,fmt);int v=vsnprintf(s,n,fmt,a);va_end(a);return v<0?0:(size_t)v;}
metarun metar;
static metarun persisted;
static bool enabled[17], rules, rewards, blitz;
static int saves, rng_calls, reward_calls, beta_calls, removed_givers, silmarils=1;
bool quest_enabled(int id) {return id>0 && id<=16 && enabled[id];}
bool quest_rules_enabled(void) {return rules;}
bool quest_rewards_enabled(void) {return rewards;}
bool quest_challenges_enabled(void) {return false;}
bool quest_lineage_enabled(void) {return false;}
bool run_mode_is_blitz(void) {return blitz;}
bool oath_invalid(int id) {(void)id;return false;}
int quest_challenge_completion_count(int id) {(void)id;return 1;}
void quest_challenge_validate(void) {}
void quest_challenge_unlock_for_quest(int id) {(void)id;}
void quest_challenge_record_escape(void) {}
void quest_beta_apply_reward(int id) {(void)id;++beta_calls;}
bool quest_varda_radiant_gift(void) {return true;}
void apply_quest_rewards(int id) {(void)id;++reward_calls;}
void refresh_current_metar_score(void) {}
errr save_metaruns(void) {++saves;return 0;}
metarun *metarun_current_mutable(void) {return &persisted;}
const metarun *metarun_current(void) {return &persisted;}
s16b metarun_current_index(void) {return 0;}
s16b metarun_entry_count(void) {return 1;}
int silmarils_possessed(void) {return silmarils;}
cptr *extract_quest_init_texts(int id,int *count) {(void)id;*count=0;return NULL;}
cptr *extract_quest_completion_texts(int id,int *count) {(void)id;*count=0;return NULL;}
void free_quest_texts(cptr *text,int count) {(void)text;(void)count;}
void quest_typewriter_menu(cptr title,cptr texts[],int n,byte a,byte b) {(void)title;(void)texts;(void)n;(void)a;(void)b;}
void do_cmd_note(char *text,int depth) {(void)text;(void)depth;}
char *format(cptr fmt,...) {static char s[1024];va_list args;va_start(args,fmt);vsnprintf(s,sizeof(s),fmt,args);va_end(args);return s;}
void msg_print(cptr s) {(void)s;}
void msg_format(cptr s,...) {(void)s;}
void remove_quest_giver_silent(int race) {(void)race;++removed_givers;}
bool is_quest_giver_present(int race) {(void)race;return true;}
bool spawn_quest_giver_near_player(int race) {(void)race;return true;}
bool ensure_reward_quest_giver_near_player(int race,int radius,cptr name,cptr arrival,int *y,int *x) {(void)race;(void)radius;(void)name;(void)arrival;(void)y;(void)x;return true;}
float calculate_parametric_probability(quest_type *q,int depth) {(void)q;(void)depth;return 1.0f;}
u32b Rand_div(u32b m) {++rng_calls;return m?0:0;}
void log_log(int level,const char *file,int line,const char *fmt,...) {(void)level;(void)file;(void)line;(void)fmt;}
'''
TESTS = r'''
static void reset(void) {
 memset(&player,0,sizeof(player));memset(&metar,0,sizeof(metar));memset(&persisted,0,sizeof(persisted));
 memset(templates,0,sizeof(templates));memset(races,0,sizeof(races));memset(lore,0,sizeof(lore));
 memset(enabled,0,sizeof(enabled));saves=rng_calls=reward_calls=beta_calls=removed_givers=0;rules=rewards=blitz=false;
 p_ptr->depth=10;p_ptr->cur_map_hgt=4;p_ptr->cur_map_wid=4;p_ptr->py=p_ptr->px=2;
 for(int id=1;id<=16;++id){templates[id].name=1;templates[id].completion_cap=7;templates[id].vala_id=VALA_MANDOS;}
 for(int i=0;i<700;++i) races[i].max_num=1;
 quest_followup_generation_begin();
}
static void activate(int id){enabled[id]=true;quest_set_state(id,QUEST_STATE_ACTIVE);}
static void test_disabled_pause(void) {
 reset();
 for(int id=7;id<=16;++id)quest_set_state(id,QUEST_STATE_ACTIVE);
 player_type before=player;metarun meta_before=metar;
 monster_type morgoth={.r_idx=R_IDX_MORGOTH,.hp=100,.maxhp=100};
 quest_followup_kill(R_IDX_ULFANG);quest_followup_damage(&morgoth,100,-1);
 quest_followup_update();quest_followup_escape();quest_followup_generate_giver();
 before.quest_lifetime_flags=3;assert(memcmp(&before,&player,sizeof(player))==0);assert(memcmp(&meta_before,&metar,sizeof(metar))==0);
 assert(!rng_calls && !saves && !reward_calls);
 enabled[7]=true;quest_followup_kill(R_IDX_ULFANG);assert(player.quest_followup_progress[0]==1);
 enabled[7]=false;quest_followup_kill(R_IDX_ULDOR);assert(player.quest_followup_progress[0]==1);
 enabled[7]=true;quest_followup_kill(R_IDX_ULDOR);assert(quest_get_state(7)==QUEST_STATE_SUCCESS);
}
static void test_objectives(void) {
 reset();activate(7);quest_followup_kill(R_IDX_ULFANG);quest_followup_kill(R_IDX_ULFANG);assert(player.quest_followup_progress[0]==1);quest_followup_kill(R_IDX_ULDOR);assert(quest_get_state(7)==3);
 reset();activate(8);quest_followup_kill(R_IDX_MAEGLIN);assert(quest_get_state(8)==3);
 reset();activate(9);races[300].flags3=RF3_DRAGON;races[301].flags3=RF3_DRAGON;races[301].name=7;
 for(int i=0;i<15;++i)quest_followup_kill(301);assert(player.quest_followup_progress[2]==0);
 for(int i=0;i<9;++i)quest_followup_kill(300);assert(quest_get_state(9)==2);quest_followup_kill(300);assert(quest_get_state(9)==3);
 reset();activate(10);for(size_t i=0;i<N_ELEMENTS(hunt_targets);++i)quest_followup_kill(hunt_targets[i]);assert(quest_get_state(10)==3);assert(metar.quest_reserved[1]==63);
 reset();activate(13);for(size_t i=0;i<N_ELEMENTS(orc_targets);++i)quest_followup_kill(orc_targets[i]);assert(quest_get_state(13)==3);
 reset();activate(15);quest_followup_kill(R_IDX_BELEGWATH);assert(quest_get_state(15)==3);
 reset();activate(16);quest_followup_kill(R_IDX_UNGOLIANT);assert(quest_get_state(16)==3);
 reset();activate(14);monster_type m={.r_idx=R_IDX_MORGOTH,.hp=100,.maxhp=100};
 quest_followup_damage(&m,50,2);assert(player.quest_followup_progress[7]==0);
 quest_followup_damage(&m,49,-1);m.hp-=49;assert(quest_get_state(14)==2);quest_followup_damage(&m,1,-1);assert(quest_get_state(14)==3);
 reset();activate(11);quest_followup_damage(&m,20,2);quest_followup_escape();assert(quest_get_state(11)==4);
 reset();activate(11);quest_followup_damage(&m,0,-1);quest_followup_escape();assert(quest_get_state(11)==2);
 reset();activate(12);quest_followup_escape();assert(quest_get_state(12)==4);
 reset();activate(12);quest_followup_kill(300);quest_followup_escape();assert(quest_get_state(12)==2);
 reset();activate(12);player.quest_lifetime_flags=1;quest_followup_escape();assert(quest_get_state(12)==2);
}
static void test_transitions(void) {
 for(int id=7;id<=16;++id){
  reset();enabled[id]=true;quest_set_state(id,QUEST_STATE_GIVER_PRESENT);
  assert(quest_followup_interaction(R_IDX_MANDOS));assert(quest_get_state(id)==QUEST_STATE_ACTIVE);
  quest_set_state(id,QUEST_STATE_SUCCESS);assert(quest_followup_interaction(R_IDX_MANDOS));
  assert(quest_get_state(id)==QUEST_STATE_REWARDED && reward_calls==1);
  assert(metarun_quest_completion_count(quest_metarun_flag(id))==1);
  assert(!quest_followup_interaction(R_IDX_MANDOS));assert(reward_calls==1);
 }
 reset();enabled[7]=true;quest_set_state(7,QUEST_STATE_GIVER_PRESENT);
 quest_followup_kill(R_IDX_ULFANG);quest_followup_kill(R_IDX_ULDOR);
 assert(quest_get_state(7)==QUEST_STATE_GIVER_PRESENT);
 assert(quest_followup_interaction(R_IDX_MANDOS));
 assert(quest_get_state(7)==QUEST_STATE_SUCCESS || quest_get_state(7)==QUEST_STATE_REWARDED);
 reset();activate(11);enabled[11]=false;
 monster_type morgoth={.r_idx=R_IDX_MORGOTH,.maxhp=100,.hp=100};
 quest_followup_damage(&morgoth,1,-1);enabled[11]=true;quest_followup_escape();
 assert(quest_get_state(11)!=QUEST_STATE_REWARDED);
 quest_followup_leave(19);assert(quest_get_state(11)!=QUEST_STATE_REWARDED);
}

static void test_accounting(void) {
 reset();
 for(int id=1;id<=16;++id){enabled[id]=true;metarun_mark_quest_completed(quest_metarun_flag(id));metarun_mark_quest_completed(quest_metarun_flag(id));assert(metarun_quest_completion_count(quest_metarun_flag(id))==1);}
 assert(saves==16 && metar.completed_quests==65535 && persisted.completed_quests==65535);
 assert(metarun_total_quest_completions(&metar)==16);
 for(int i=0;i<6;++i)assert(metar.quest_completion_counts[i]==1);
 for(int i=0;i<8;++i)assert(metar.reserved_runtime[i]==1);
 assert(player.quest_followup_recorded==1023);
 metarun_clamp_and_sync_quests(&metar);assert(metarun_total_quest_completions(&metar)==16);
 reset();for(int id=1;id<=6;++id){enabled[id]=true;templates[id].completion_cap=1;assert(quest_completion_cap(id)==7);}rules=true;assert(quest_completion_cap(3)==1);
 reset();activate(8);quest_set_state(8,3);assert(complete(8,false));assert(!complete(8,false));assert(reward_calls==1 && beta_calls==1 && saves==1);
 reset();activate(8);p_ptr->quest_test_sandbox=1;quest_set_state(8,3);assert(complete(8,false));assert(saves==0 && metar.completed_quests==0);
}
static void test_pending_failure(void) {
 reset();enabled[13]=true;
 quest_followup_vault_placed(13,10);quest_followup_generation_begin();quest_followup_generation_commit();assert(quest_get_state(13)==0);
 quest_followup_vault_placed(13,10);quest_followup_generation_commit();assert(quest_get_state(13)==1 && player.quest_reserved[0]==1);
 quest_followup_generation_commit();assert(player.quest_reserved[0]==1);
 quest_set_state(13,2);quest_followup_leave(11);u16b progress=player.quest_followup_progress[6];
 for(size_t i=0;i<N_ELEMENTS(orc_targets);++i)quest_followup_kill(orc_targets[i]);
 assert(player.quest_followup_progress[6]==progress && quest_get_state(13)!=QUEST_STATE_SUCCESS);
}
static void test_failed_giver_dispatch(void) {
 reset();enabled[13]=enabled[1]=true;templates[13].vala_id=VALA_TULKAS;
 quest_followup_vault_placed(13,10);quest_followup_generation_commit();
 quest_followup_leave(11);p_ptr->depth=11;
 assert(player.quest_followup_flags[6]&Q_FAILED);
 p_ptr->tulkas_quest=TULKAS_QUEST_GIVER_PRESENT;
 assert(!quest_followup_interaction(R_IDX_TULKAS));
 assert(!removed_givers && !reward_calls);
 assert(quest_get_state(13)==QUEST_STATE_GIVER_PRESENT);
 assert(p_ptr->tulkas_quest==TULKAS_QUEST_GIVER_PRESENT);
 /* A failed entry must not mask a valid followup using the same giver. */
 enabled[14]=true;templates[14].vala_id=VALA_TULKAS;
 quest_set_state(14,QUEST_STATE_SUCCESS);
 assert(quest_followup_interaction(R_IDX_TULKAS));
 assert(quest_get_state(14)==QUEST_STATE_REWARDED && removed_givers==1 && reward_calls==1);
}
static void test_disabled_departure(void) {
 reset();activate(15);
 quest_followup_vault_placed(15,10);quest_followup_generation_commit();
 assert(quest_accepted_count_this_run()==1 && quest_followup_reserve_race(R_IDX_BELEGWATH));
 enabled[15]=false;quest_followup_leave(11);p_ptr->depth=11;
 enabled[15]=true;quest_followup_update();
 assert(player.quest_followup_flags[8]&Q_FAILED);
 assert(!quest_accepted_count_this_run() && !quest_followup_reserve_race(R_IDX_BELEGWATH));
 assert(!reward_calls && !saves);
}
static void test_discarded_map(void) {
 const int ids[]={7,8,13,15};
 for(size_t i=0;i<N_ELEMENTS(ids);++i) for(int paused=0;paused<2;++paused)
 for(int state=QUEST_STATE_GIVER_PRESENT;state<=QUEST_STATE_REWARDED;++state) {
  int id=ids[i];reset();enabled[id]=true;quest_set_state(id,state);
  quest_followup_vault_placed(id,10);quest_followup_generation_commit();
  p_ptr->quest_followup_progress[id-7]=1;
  enabled[id]=!paused;
  /* Forced movement has already changed depth by the time generation runs. */
  p_ptr->depth=12;quest_followup_discard_level();quest_followup_discard_level();
  assert(!!(player.quest_followup_flags[id-7]&Q_FAILED)==(state<QUEST_STATE_SUCCESS));
  assert(quest_get_state(id)==state && player.quest_followup_progress[id-7]==1);
  enabled[id]=true;assert(!quest_accepted_count_this_run());
  assert(!reward_calls && !saves);
 }
 /* Pending placement from a rejected generation attempt is not a quest yet. */
 reset();enabled[13]=true;quest_followup_vault_placed(13,10);
 quest_followup_discard_level();quest_followup_generation_begin();
 quest_followup_generation_commit();assert(!player.quest_followup_flags[6]);
 quest_followup_vault_placed(13,10);quest_followup_generation_commit();
 assert(quest_get_state(13)==QUEST_STATE_GIVER_PRESENT && !(player.quest_followup_flags[6]&Q_FAILED));
 /* Pausing on the same map keeps progress; replacing that map abandons it. */
 reset();activate(15);activate(10);
 quest_followup_vault_placed(15,10);quest_followup_generation_commit();
 enabled[15]=false;quest_followup_update();enabled[15]=true;
 assert(!(player.quest_followup_flags[8]&Q_FAILED));
 quest_followup_discard_level();assert(player.quest_followup_flags[8]&Q_FAILED);
 assert(quest_get_state(10)==QUEST_STATE_ACTIVE && !player.quest_followup_flags[3]);
 assert(quest_accepted_count_this_run()==1);
}
int main(void){test_disabled_pause();test_objectives();test_transitions();test_accounting();test_pending_failure();test_failed_giver_dispatch();test_disabled_departure();test_discarded_map();test_save();puts("Quest runtime: objectives, attribution, pause/resume, disabled RNG/state, 16 counters, idempotence, retry commit, failed givers, level abandonment and save blocks: PASS");}
'''
def save_wrappers():
    save=(ROOT/'src/fs/save-player.c').read_text(encoding='utf-8')
    a=save.index('    wr_byte(0x5b);'); b=save.index('    /* Skeleton note state',a)
    load=(ROOT/'src/fs/load-player.c').read_text(encoding='utf-8')
    c=load.index('    memset(p_ptr->quest_followup_state');d=load.index('    /* Skeleton note state',c)
    return r'''
static byte storage[256];static size_t position,length;static bool new_version=true;
static void test_wr_byte(byte b){assert(position<sizeof(storage));storage[position++]=b;}
static void test_wr_u16b(u16b n){test_wr_byte(n&255);test_wr_byte(n>>8);}
static void test_wr_s16b(s16b n){test_wr_u16b((u16b)n);}
static void test_rd_byte(byte *b){assert(position<length);*b=storage[position++];}
static void test_rd_u16b(u16b *n){byte a,b;test_rd_byte(&a);test_rd_byte(&b);*n=a|((u16b)b<<8);}
static void test_rd_s16b(s16b *n){u16b v;test_rd_u16b(&v);*n=(s16b)v;}
static bool test_version(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;return new_version;}
static void test_note(cptr msg){(void)msg;}
static void save_block(void){int i;
'''+save[a:b].replace('wr_','test_wr_')+r'''
}
static int load_block(void){int i;byte marker;
'''+load[c:d].replace('rd_','test_rd_').replace('savefile_version_at_least','test_version').replace('note(','test_note(')+r'''
return 0;}
static void test_save(void){
 memset(&player,0,sizeof(player));
 for(int i=0;i<10;++i){player.quest_followup_state[i]=i%5;player.quest_followup_flags[i]=i%16;player.quest_followup_depth[i]=i+1;player.quest_followup_progress[i]=100+i;}
 player.quest_followup_recorded=1023;player.quest_test_sandbox=1;player.quest_challenge=4;player.orome_bow_hit_streak=2;player.orome_spear_ready=1;
 player_type expected=player;position=0;save_block();length=position;assert(length==70);
 memset(&player,0,sizeof(player));position=0;assert(load_block()==0 && position==length);assert(memcmp(&expected,&player,sizeof(player))==0);
 byte good[256];memcpy(good,storage,length);
 storage[63]=4;position=0;assert(load_block()==-1);memcpy(storage,good,length);
 storage[0]=0;position=0;assert(load_block()==-1);memcpy(storage,good,length);
 storage[1]=5;position=0;assert(load_block()==-1);memcpy(storage,good,length);
 storage[2]=16;position=0;assert(load_block()==-1);memcpy(storage,good,length);
 storage[3]=255;storage[4]=255;position=0;assert(load_block()==-1);memcpy(storage,good,length);
 new_version=false;position=0;memset(&player,0x7f,sizeof(player));assert(load_block()==0 && position==0);assert(player.quest_followup_state[0]==0 && !player.quest_followup_recorded && !player.quest_test_sandbox);
 memset(&player,0,sizeof(player));lore[300].pkills=1;p_ptr->morgoth_hits=1;position=0;assert(load_block()==0 && position==0 && player.quest_lifetime_flags==3);new_version=true;
}
'''
def legacy_layout():
    text = subprocess.check_output(["git", "-c", "core.fsmonitor=false", "show", "ed5e9b32:src/metarun.h"], cwd=ROOT).decode("utf-8")
    start = text.index("typedef struct metarun")
    end = text.index("} metarun;", start) + len("} metarun;")
    declaration = text[start:end].replace("struct metarun", "struct legacy_metarun").replace("} metarun;", "} legacy_metarun;")
    return declaration + "\n" + r'''
_Static_assert(METARUN_QUEST_SLOT_MAX == 8, "Original raw quest array must remain eight bytes");
_Static_assert(sizeof(metarun) == sizeof(legacy_metarun), "Raw metarun size changed");
_Static_assert(offsetof(metarun, quest_completion_counts) == offsetof(legacy_metarun, quest_completion_counts), "Quest offset changed");
_Static_assert(offsetof(metarun, reserved_runtime) == offsetof(legacy_metarun, reserved_runtime), "Reserved offset changed");
'''


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    source=OUT/'check.c'
    production = "\n".join((ROOT / name).read_text(encoding="utf-8").replace('#include "externs.h"', "") for name in ["src/quest/quest-runtime.c", "src/quest.c"])
    source.write_text(HARNESS + legacy_layout() + production + save_wrappers() + TESTS, encoding="utf-8")
    env=os.environ.copy();env['PATH']='C:/msys64/mingw64/bin;C:/msys64/usr/bin;'+env.get('PATH','')
    exe=OUT/'check.exe'
    subprocess.run(['C:/msys64/mingw64/bin/cc.exe','-std=c17','-O0','-ffunction-sections','-fdata-sections','@CMakeFiles/sil-more.dir/includes_C.rsp',str(source),'-Wl,--gc-sections','-o',str(exe)],cwd=ROOT/'build-standard',env=env,check=True)
    subprocess.run([str(exe)],env=env,check=True,timeout=15)
if __name__=='__main__':main()
