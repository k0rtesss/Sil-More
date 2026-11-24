#!/usr/bin/env python3
"""Varied SDL player workflows, compound effects and interrupted-work saves.

All actions use GameControl. Scene setup and observations are fixture-only;
no monster AI decision or combat balance is an assertion in this campaign.
"""
from __future__ import annotations

import argparse
import copy
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import time
import threading
import traceback
import uuid

import check_player_systems_gameplay as base

ROOT = base.ROOT
OUT = ROOT / "scripts/output/player-systems-expanded"

EXTRA_GLOBALS = r'''
#include "metarun.h"
static int work_pause_after, work_steps;
static bool work_paused, work_pause_used;
static void publish_json(const char* leaf, cJSON* root, bool append)
{
    char path[1400], temporary[1400];
    strnfmt(path,sizeof(path),"%s/%s",SDL_getenv("CONTROL_TEST_PROFILE"),leaf);
    strnfmt(temporary,sizeof(temporary),"%s.tmp",path);
    char* data=cJSON_PrintUnformatted(root); assert(data);
    FILE* f=fopen(append?path:temporary,append?"ab":"wb"); assert(f);
    assert(fwrite(data,1,strlen(data),f)==strlen(data));
    if(append)assert(fputc('\n',f)!=EOF);
    assert(fclose(f)==0);
    if(!append) {
        bool done=false;
        for(int i=0;i<20 && !done;i++){done=SDL_RenamePath(temporary,path);if(!done)SDL_Delay(1);}
        assert(done);
    }
    cJSON_free(data);
}
static void observe_player(cJSON* root)
{
#define N(key,value) cJSON_AddNumberToObject(root,key,value)
    N("disease",p_ptr->diseased);N("disease_name",p_ptr->disease_name);
    N("disease_cure",p_ptr->disease_cure);N("disease_knowledge",p_ptr->disease_knowledge);
    N("disease_immunity",p_ptr->disease_immunity);
    N("fear",p_ptr->afraid);N("image",p_ptr->image);N("entranced",p_ptr->entranced);
    N("rage",p_ptr->rage);N("fast",p_ptr->fast);N("slow",p_ptr->slow);
    N("tmp_str",p_ptr->tmp_str);N("tmp_dex",p_ptr->tmp_dex);
    N("tmp_con",p_ptr->tmp_con);N("tmp_gra",p_ptr->tmp_gra);N("tmp_per",p_ptr->tmp_per);
    N("res_fire",p_ptr->oppose_fire);N("res_cold",p_ptr->oppose_cold);N("res_poison",p_ptr->oppose_pois);
    N("see_invisible",p_ptr->tim_invis);N("speed",p_ptr->pspeed);
    N("smithing_leftover",p_ptr->smithing_leftover);
    N("pack_used",inventory_limit_usage_for_group(INV_LIMIT_PACK));
    N("pack_cap",inventory_limit_limit_for_group(INV_LIMIT_PACK));
    N("harness_used",inventory_limit_usage_for_group(INV_LIMIT_HARNESS));
    N("harness_cap",inventory_limit_limit_for_group(INV_LIMIT_HARNESS));
    N("supply_weight",supplies_total_weight());N("supply_cap",supplies_current_weight_cap());
    N("pack_work",player_pack_action_turns_left());N("resting",p_ptr->resting);
    N("lamp_oil",p_ptr->lamp_oil);N("climbing",p_ptr->climbing);N("leaping",p_ptr->leaping);
    N("hero",p_ptr->pcharacter);N("race",p_ptr->prace);
    N("oath",p_ptr->oath_type);N("broken_oaths",p_ptr->oaths_broken);
    N("curse_stacks",metarun_net_curse_stacks());
    N("item_tester_tval",item_tester_tval);N("item_tester_hook",item_tester_hook!=NULL);
    N("item_tester_full",item_tester_full);N("get_item_mode",p_ptr->get_item_mode);
    N("fixture_work_pause",work_paused);
    N("ui_main",g_main_menu_overlay_active);N("ui_book",sdl_character_sheet_screen_active());
    char rng[32];strnfmt(rng,sizeof(rng),"%016llx",(unsigned long long)Rand_state_export());
    cJSON_AddStringToObject(root,"rng",rng);
    const s16b* arrays[]={p_ptr->stat_base,p_ptr->stat_drain,p_ptr->stat_disease,p_ptr->stat_use,
        p_ptr->skill_base,p_ptr->skill_use};
    const char* names[]={"stats","drain","disease_stats","stats_use","skills","skills_use"};
    for(int i=0;i<6;i++){
        cJSON* row=cJSON_AddArrayToObject(root,names[i]);
        for(int j=0;j<(i<4?A_MAX:S_MAX);j++)cJSON_AddItemToArray(row,cJSON_CreateNumber(arrays[i][j]));
    }
#undef N
}
static void observe_question(cJSON* root)
{
    if(g_touch_pane_yes_no_prompt_active)
        cJSON_AddStringToObject(root,"confirmation",g_touch_pane_yes_no_prompt_text);
    if(!g_question_menu.active || g_question_menu.context_hint)return;
    cJSON* q=cJSON_AddObjectToObject(root,"question");
    cJSON_AddStringToObject(q,"title",g_question_menu.title);
    cJSON_AddStringToObject(q,"description",g_question_menu.desc);
    cJSON_AddBoolToObject(q,"text_input",g_question_menu.text_input);
    cJSON* choices=cJSON_AddArrayToObject(q,"choices");
    for(int i=0;i<g_question_menu.count;i++){
        const sdl_question_menu_entry_state* row=&g_question_menu.entries[i];
        cJSON* o=cJSON_CreateObject();cJSON_AddItemToArray(choices,o);
        cJSON_AddNumberToObject(o,"choice",row->choice);cJSON_AddNumberToObject(o,"color",row->text_attr);
        cJSON_AddStringToObject(o,"key",row->letter);cJSON_AddStringToObject(o,"label",row->text);
    }
}
'''

EXTRA_SETUP = r'''
    work_pause_after=number(spec,"pause_work_after",0);
    if(number(spec,"ascii",0))set_sdl_tiles(false);
    p_ptr->prace=number(spec,"race",0);p_ptr->pcharacter=number(spec,"hero",0);
    assert(p_ptr->prace<z_info->p_max && p_ptr->pcharacter<z_info->c_max);
    rp_ptr=&p_info[p_ptr->prace];current_character_profile=&c_info[p_ptr->pcharacter];
'''

EXTRA_STATS = r'''
    p_ptr->diseased=number(spec,"disease",0);
    const char* stat_keys[]={"strength","dexterity","constitution","grace"};
    cJSON* drain=cJSON_GetObjectItemCaseSensitive(spec,"drain");
    cJSON* disease_stats=cJSON_GetObjectItemCaseSensitive(spec,"disease_stats");
    for(int i=0;i<A_MAX;i++){
        p_ptr->stat_base[i]=number(spec,stat_keys[i],p_ptr->stat_base[i]);
        const cJSON* d=cJSON_GetArrayItem(drain,i);if(cJSON_IsNumber(d))p_ptr->stat_drain[i]=d->valueint;
        d=cJSON_GetArrayItem(disease_stats,i);if(cJSON_IsNumber(d))p_ptr->stat_disease[i]=d->valueint;
    }
    const cJSON* skills=cJSON_GetObjectItemCaseSensitive(spec,"skill_values");
    for(int i=0;i<S_MAX;i++){
        const cJSON* d=cJSON_GetArrayItem(skills,i);if(cJSON_IsNumber(d))p_ptr->skill_base[i]=d->valueint;
    }
'''

EXTRA_STATUS = r'''
    p_ptr->stun=number(spec,"stun",0);p_ptr->afraid=number(spec,"fear",0);
    p_ptr->image=number(spec,"image",0);p_ptr->rage=number(spec,"rage",0);
    p_ptr->slow=number(spec,"slow",0);p_ptr->fast=number(spec,"fast",0);
    p_ptr->diseased=number(spec,"disease",0);p_ptr->disease_name=number(spec,"disease_name",0);
    p_ptr->disease_cure=number(spec,"disease_cure",0);p_ptr->disease_knowledge=number(spec,"disease_knowledge",0);
    p_ptr->disease_immunity=number(spec,"disease_immunity",0);
    p_ptr->oath_type=number(spec,"oath",0);
'''

EXTRA_ITEM = r'''
        if(number(entry,"ego",0))o.name2=number(entry,"ego",0);
        if(number(entry,"cursed",0))o.ident|=IDENT_CURSED;
        if(number(entry,"broken",0))o.ident|=IDENT_BROKEN;
        o.att=number(entry,"attack",o.att);o.ds=number(entry,"damage_sides",o.ds);
        const cJSON* note=cJSON_GetObjectItemCaseSensitive(entry,"note");
        if(cJSON_IsString(note))o.obj_note=quark_add(note->valuestring);
'''

WORK_OBSERVER = r'''
void __real_process_player(void);
void __wrap_process_player(void)
{
    int before=playerturn;
    __real_process_player();
    if(!character_dungeon || !p_ptr || playerturn==before)return;
    cJSON* event=cJSON_CreateObject();
    cJSON_AddNumberToObject(event,"player_turn",playerturn);
    cJSON_AddNumberToObject(event,"turn",turn);
    cJSON_AddNumberToObject(event,"hp",p_ptr->chp);cJSON_AddNumberToObject(event,"voice",p_ptr->csp);
    cJSON_AddNumberToObject(event,"poison",p_ptr->poisoned);cJSON_AddNumberToObject(event,"cut",p_ptr->cut);
    cJSON_AddNumberToObject(event,"blind",p_ptr->blind);cJSON_AddNumberToObject(event,"confused",p_ptr->confused);
    cJSON_AddNumberToObject(event,"stun",p_ptr->stun);cJSON_AddNumberToObject(event,"smithing",p_ptr->smithing);
    cJSON_AddNumberToObject(event,"fletching",p_ptr->fletching);cJSON_AddNumberToObject(event,"food",p_ptr->food);
    observe_player(event);publish_json("turns.jsonl",event,true);cJSON_Delete(event);
    if(work_pause_after>0 && !work_pause_used &&
        (p_ptr->smithing || p_ptr->fletching || player_pack_action_pending()) && ++work_steps>=work_pause_after){
        work_pause_used=true;work_paused=true;
        /* Freeze only between completed player turns. The next real SDL input
         * is pumped normally and the production abort check handles it. */
        while(Term->key_head==Term->key_tail){
            snapshot(true);(void)__real_sdl_control_poll(true);
            SDL_Event e;while(SDL_PollEvent(&e))sdl_handle_event(&g_state,&e);
            if(Term->key_head==Term->key_tail)SDL_Delay(1);
        }
        work_paused=false;
    }
}
'''


def fixture_source() -> str:
    source = base.FIXTURE
    source = source.replace('static int number(', EXTRA_GLOBALS+'\nstatic int number(', 1)
    source = source.replace('    VALUE("feat", cave_feat[p_ptr->py][p_ptr->px]);',
                            '    VALUE("feat", cave_feat[p_ptr->py][p_ptr->px]);\n    observe_player(root);observe_question(root);')
    source = source.replace('    cJSON* rows = cJSON_AddArrayToObject(root, "items");',
                            '    cJSON* rows = cJSON_AddArrayToObject(root, "items");\n'
                            '    item_json(rows,smith_o_ptr,-1,"work");')
    source = source.replace('    cJSON_AddNumberToObject(row, "storage", o->storage);',
                            '    cJSON_AddNumberToObject(row, "storage", o->storage);\n'
                            '    cJSON_AddNumberToObject(row,"aware",k_info[o->k_idx].aware);\n'
                            '    cJSON_AddNumberToObject(row,"cursed",cursed_p(o));\n'
                            '    cJSON_AddNumberToObject(row,"broken",broken_p(o));')
    source = source.replace('    inventory[INVEN_LITE].timeout=3000;',
                            '    inventory[INVEN_LITE].timeout=3000;\n'
                            '    if(number(spec,"weapon_cursed",0))inventory[INVEN_WIELD].ident|=IDENT_CURSED;')
    source = source.replace('    rp_ptr=&p_info[0]; current_character_profile=&c_info[0];',EXTRA_SETUP)
    source = source.replace('    character_generated=character_dungeon=true; character_icky=character_xtra=0;\n    turn=playerturn=100;',
                            EXTRA_STATS+'\n    character_generated=character_dungeon=true; character_icky=character_xtra=0;\n    turn=playerturn=100;')
    source = source.replace('        o.storage=number(entry,"storage",o.storage);',
                            '        o.storage=number(entry,"storage",o.storage);\n'+EXTRA_ITEM)
    source = source.replace('m->alertness=ALERTNESS_UNWARY;',
                            'm->alertness=number(entry,"alertness",ALERTNESS_UNWARY);')
    source = source.replace('    p_ptr->energy=100; p_ptr->redraw=',EXTRA_STATUS+'\n    p_ptr->energy=100; p_ptr->redraw=')
    return source+'\n'+WORK_OBSERVER


def write_report(path: Path, value) -> None:
    temp=path.with_name(path.name+'.tmp')
    temp.write_text(json.dumps(value,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    # Windows readers can briefly hold the destination without delete sharing.
    # Retry publication, never the game action that produced this report.
    for attempt in range(101):
        try:
            temp.replace(path)
            return
        except PermissionError:
            if attempt==100:raise
            time.sleep(.05)


class Scene:
    def __init__(self, profile: Path):
        self.profile=profile
        self.children=[]
        self.record=base.act(profile)
        self.initial=copy.deepcopy(self.p)
    @property
    def p(self):return self.record['telemetry']
    @property
    def state(self):return self.record['response'].get('state',{})
    @property
    def terminal(self):return '\n'.join(self.state.get('terminal',[]))
    @property
    def question(self):
        return self.p.get('question',{}) or ({'title':self.p['confirmation'],
            'choices':[{'key':'y','label':'Yes'},{'key':'n','label':'No'}]}
            if self.p.get('confirmation') else {})
    def press(self,*keys):
        for key in keys:self.record=base.act(self.profile,'key',key)
        return self.p
    def text(self,value):
        self.record=base.act(self.profile,'text',value);return self.p
    def observe(self):self.record=base.act(self.profile);return self.p
    def choose(self, fragment):
        rows=[r for r in self.question.get('choices',[]) if fragment.lower() in r['label'].lower()]
        assert len(rows)==1,(fragment,self.question)
        key=rows[0]['key'].strip().rstrip(')')
        assert key,(fragment,self.question)
        self.press(key)
    def label(self,fragment):
        rows=[]
        for line in self.terminal.splitlines():
            match=re.match(r'^\s*([a-zA-Z0-9-])\)\s',line)
            if match and fragment.lower() in line.lower():rows.append(match.group(1))
        assert len(rows)==1,(fragment,rows,self.terminal)
        return rows[0]
    def total(self,kind,store=None):
        return sum(o['number'] for o in self.p['items'] if o['kind']==kind and (store is None or o['store']==store))
    def brief(self):return {k:v for k,v in self.p.items() if k not in ('items','tiles','targets')}
    def turns(self):
        path=self.profile/'turns.jsonl'
        return [json.loads(x) for x in path.read_text().splitlines()] if path.exists() else []
    def checkpoint(self):
        self.press('Ctrl+s');path=self.profile/'save/Player_Audit';assert path.is_file();return path
    def close_views(self):
        for _ in range(8):
            if not self.question and not self.state.get('modal') and not self.p.get('ui_main') and not self.p.get('ui_book'):return
            self.press('Escape')
        raise AssertionError('View did not close')
    def setup(self,fragment):
        self.press('i');self.press(self.label(fragment))
        assert self.question.get('title')=='Choose item setup' or self.question.get('title','').startswith('Move to Harness '),self.question
    def limits(self):
        assert 0<=self.p['pack_used']<=self.p['pack_cap'],self.brief()
        assert 0<=self.p['harness_used']<=self.p['harness_cap'],self.brief()
        assert 0<=self.p['supply_weight']<=self.p['supply_cap'],self.brief()
        assert self.p['arrows']<=48,self.brief()
    def close(self):
        def closed():
            session=base.read_json(self.profile/'control/session.json')
            process=base._fixture_processes.get(self.profile.resolve())
            if session.get('running') is False:
                assert process is not None,'Missing owned process handle'
                assert process.wait(timeout=2)==0,('Fixture did not exit cleanly',process.returncode)
                return True
            if process is not None and process.poll() is not None:
                raise AssertionError(('Fixture exited without closing control session',process.returncode))
            return False
        for _ in range(10):
            if closed():return
            try:
                if self.question or self.state.get('modal') or self.state.get('text_input'):
                    self.press('Escape')
                else:
                    self.press('Ctrl+x')
                    if self.record['response'].get('closed'):
                        base._fixture_processes[self.profile.resolve()].wait(timeout=2)
                        assert closed();return
                    self.press('Escape')
            except base.ControlError:
                # Quit can publish running=false between the client's response
                # poll and its liveness poll. Require a clean, owned exit.
                if closed():return
                raise
            if self.record['response'].get('closed'):
                base._fixture_processes[self.profile.resolve()].wait(timeout=2)
                assert closed();return
        raise AssertionError('Could not close owned scene')
    def clean_selector(self):
        assert self.p['item_tester_tval']==0 and not self.p['item_tester_hook'] and not self.p['get_item_mode'],self.p


class Campaign:
    def __init__(self,args):
        global OUT
        OUT=args.output.resolve();OUT.mkdir(parents=True,exist_ok=True);base.OUT=OUT
        if (OUT/'summary.json').exists():
            (OUT/('discovery-'+uuid.uuid4().hex[:8]+'.json')).write_bytes((OUT/'summary.json').read_bytes())
        self.args=args;self.results=[];self.jobs=[];self.lock=threading.Lock()
        self.core_sha=hashlib.sha256((base.BUILD/'sil-more.exe').read_bytes()).hexdigest()
        self.exe,self.env=base.build(fixture=fixture_source(),wrappers=('process_player',))
        (self.exe.parent/'driver.py').write_bytes(Path(__file__).read_bytes())
        (self.exe.parent/'base-driver.py').write_bytes(Path(base.__file__).read_bytes())
        self.receipt={'started_at':datetime.now(timezone.utc).isoformat(),
            'monster_ai':'excluded by user request','seeds':args.seeds,'cases':self.results,
            'driver_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'fixture':str(self.exe),'fixture_sha256':hashlib.sha256(self.exe.read_bytes()).hexdigest(),
            'core_sha256':self.core_sha,'workers':getattr(args,'workers',1)}
        self.existing={}
        for report in getattr(args,'resume',None) or []:
            old=json.loads(report.read_text(encoding='utf-8'))
            provenance=json.loads((Path(old['fixture']).parent/'provenance.json').read_text())
            assert provenance['build_sha256']==self.core_sha,('Resume requires the same engine',report)
            assert hashlib.sha256(Path(old['fixture']).read_bytes()).hexdigest()==old['fixture_sha256']
            for case in old['cases']:
                if case['passed']:self.existing[case['name']]=(case,report)
        self.save()
    def save(self):
        self.receipt.update(passed=sum(r['passed'] for r in self.results),failed=sum(not r['passed'] for r in self.results))
        write_report(OUT/'summary.json',self.receipt)
    def run(self,name,spec,check,category):
        if self.args.only and not any(pattern in name for pattern in self.args.only):return
        if self.args.groups and category not in self.args.groups:return
        if name in self.existing and category in ('consumables','foods','disease','resistance'):
            old,report=self.existing[name]
            assert old['spec']==spec,(name,'Changed replay spec')
            replay=ReplayScene(Path(old['profile']))
            check(replay)
            assert all(r['op']=='key' and r['value'] in ('Ctrl+x','Escape')
                for r in replay.records[replay.cursor:]),(name,'Unexpected unverified input tail')
            assert not json.loads((replay.profile/'control/session.json').read_text())['running']
            row=dict(old,revalidated_from=str(report),before_cleanup=copy.deepcopy(replay.brief()))
            self.results.append(row);self.save()
            print(f"REVALIDATED {category}/{name}",flush=True)
            return
        self.jobs.append((name,spec,check,category))
    def execute(self,job):
        name,spec,check,category=job
        if sum(not r['passed'] for r in self.results)>=self.args.max_failures:
            raise RuntimeError('Failure limit reached; inspect retained failures before continuing')
        profile=base.start(name,spec,self.exe,self.env)
        row=dict(name=name,category=category,spec=spec,profile=str(profile),passed=False)
        scene=None;started=time.monotonic()
        try:
            scene=Scene(profile);check(scene);row['passed']=True
        except Exception:row['error']=traceback.format_exc()
        finally:
            if scene:
                row['before_cleanup']=copy.deepcopy(scene.brief())
                row['children']=[str(child.profile) for child in scene.children]
                for child in scene.children:
                    try:child.close()
                    except Exception:row['child_cleanup_error']=traceback.format_exc();row['passed']=False
                try:scene.close()
                except Exception:row['cleanup_error']=traceback.format_exc();row['passed']=False
        row['seconds']=round(time.monotonic()-started,3)
        with self.lock:
            self.results.append(row);self.save()
            print(f"{len(self.results)} {'PASS' if row['passed'] else 'FAIL'} {category}/{name}",flush=True)
    def variant(self,name,spec,check,category):
        for seed in self.args.seeds:self.run(name+'-'+str(seed),dict(spec,seed=seed),check,category)
    def finish(self):
        with ThreadPoolExecutor(max_workers=getattr(self.args,'workers',1)) as pool:
            list(pool.map(self.execute,self.jobs))


class ReplayScene(Scene):
    """Re-run the current assertions against actual immutable SDL frames."""
    def __init__(self,profile):
        self.profile=profile
        self.records=[json.loads(x) for x in (profile/'inputs.jsonl').read_text().splitlines()]
        self.record=self.records[0];self.initial=copy.deepcopy(self.p);self.cursor=1
    def press(self,*keys):
        for key in keys:
            record=self.records[self.cursor];self.cursor+=1
            assert record['op']=='key' and record['value']==key,(key,record.get('value'))
            assert 'observer_error' not in record and record['response']['ok']
            self.record=record
        return self.p


def consume(command,kind,oracle, floor, identifiable=True):
    def check(s: Scene):
        key='-' if floor else 'a'
        s.press(command,key)
        assert s.question,('No consumption confirmation',s.terminal,s.state)
        s.press('n')
        assert s.p['player_turn']==s.initial['player_turn'] and s.total(kind)==3
        assert s.p['rng']==s.initial['rng'],'Cancel changed game RNG'
        s.press(key,'y')
        if s.p.get('confirmation','').startswith('Autoinscribe this herb type?'):
            s.press('n')
        assert s.total(kind)==2,(kind,s.p['items'])
        assert s.p['player_turn']>=s.initial['player_turn']+1
        s.clean_selector();oracle(s)
        assert s.p['hp']>0 and not s.p['dead']
    return check


def consumables(c: Campaign):
    effects={
        313:({'hp':15,'voice':2,'poison':8,'cut':6,'fear':10,'stun':12,'confused':10,'image':15},lambda s: s.p['hp']>15 and s.p['voice']==s.p['max_voice'] and not any(s.p[k] for k in ('poison','fear','stun','confused','image'))),
        315:({'hp':15,'fear':10},lambda s:s.p['hp']>15 and not s.p['fear']),
        316:({'voice':2},lambda s:s.p['voice']>2 and s.p['tmp_per']>0),
        317:({'stun':12,'confused':15,'image':20,'rage':10},lambda s:not any(s.p[k] for k in ('stun','confused','image','rage'))),
        318:({'hp':15,'cut':12},lambda s:s.p['hp']>15 and s.p['cut']<12),
        319:({'voice':2},lambda s:s.p['voice']==s.p['max_voice']),
        320:({'image':20},lambda s:not s.p['image'] and s.p['see_invisible']>0),
        321:({'poison':12},lambda s:not s.p['poison']),
        322:({},lambda s:s.p['fast']>0),
        323:({},lambda s:s.p['res_fire']>0 and s.p['res_cold']>0),
        327:({},lambda s:s.p['tmp_str']>0),328:({},lambda s:s.p['tmp_dex']>0),
        329:({},lambda s:s.p['tmp_con']>0),330:({},lambda s:s.p['tmp_gra']>0),
        343:({},lambda s:s.p['slow']>0),344:({},lambda s:any(e['poison']>0 for e in s.turns())),
        345:({},lambda s:s.p['blind']>0),346:({},lambda s:s.p['confused']>0),
        348:({},lambda s:s.p['drain'][1]<s.initial['drain'][1]),
        350:({},lambda s:s.p['drain'][3]<s.initial['drain'][3]),
    }
    for kind,(status,test) in effects.items():
        for floor in (False,True):
            for known in (False,True):
                spec=dict(stats=7,food=2500,**status,items=[dict(kind=kind,number=3,known=int(known),floor=int(floor))])
                if kind in (343,345,346,348,350):weaken_will(spec)
                def oracle(s,test=test):assert test(s),s.brief()
                c.variant(f'potion-{kind}-'+('floor' if floor else 'carried')+('-known' if known else '-unknown'),spec,
                    consume('q',kind,oracle,floor),'consumables')


def foods(c: Campaign):
    effects={
        380:({'fear':12},lambda s:s.p['rage']>0 and not s.p['fear']),
        381:({},lambda s:s.p['food']>s.initial['food']+1900),
        382:({},lambda s:s.p['fear']>0 and s.p['fast']>0),
        383:({'hp':15,'cut':10},lambda s:s.p['hp']>15 and s.p['cut']<10),
        384:({'drain':[-2,-3,-1,-2]},lambda s:s.p['drain']==[0,0,0,0]),
        385:({},lambda s:s.p['food']<s.initial['food']-900),
        386:({},lambda s:s.p['image']>0),
        387:({},lambda s:any(e['entranced']>0 for e in s.turns()) and not s.p['entranced']),
        388:({},lambda s:s.p['drain'][0]<0),
        389:({},lambda s:s.p['disease']>0 and s.p['disease_stats'][2]<0),
        399:({},lambda s:s.p['food']>s.initial['food']+1400),
        400:({},lambda s:s.p['food']>s.initial['food']+1900),
        401:({'drain':[0,0,0,-1]},lambda s:s.p['food']>s.initial['food']+2900 and s.p['drain'][3]==0),
    }
    for kind,(status,test) in effects.items():
        for floor in (False,True):
            for known in ((False,True) if kind<399 else (True,)):
                spec=dict(stats=7,food=2500,**status,items=[dict(kind=kind,number=3,known=int(known),floor=int(floor))])
                if kind in (382,386,387,388):weaken_will(spec)
                def oracle(s,test=test):assert test(s),s.brief()
                c.variant(f'food-{kind}-'+('floor' if floor else 'carried')+('-known' if known else '-unknown'),spec,
                    consume('E',kind,oracle,floor),'foods')


def diseases(c: Campaign):
    for disease in range(1,9):
        for kind,command in ((383,'E'),(318,'q'),(313,'q')):
            for floor in (False,True):
                spec=dict(stats=7,food=2500,hp=15,disease=200,disease_name=disease,disease_cure=9,
                    disease_stats=[0,-1,-2,0],drain=[-2,0,0,-1],
                    items=[dict(kind=kind,number=3,floor=int(floor),known=1)])
                def cure(s,kind=kind,disease=disease):
                    assert not s.p['disease'] and s.p['disease_stats']==[0,0,0,0],s.brief()
                    assert s.p['drain']==[-2,0,0,-1] and s.p['disease_immunity']==1<<(disease-1),s.brief()
                    if kind==383:
                        assert s.p['food']<=s.initial['food'],s.brief()
                        # Removing Constitution penalties rescales Health. It
                        # must not add the herb's ordinary healing on top.
                        assert s.p['hp']*s.initial['max_hp']<=(s.initial['hp']+1)*s.p['max_hp'],s.brief()
                    else:assert s.p['hp']>s.initial['hp']
                c.variant(f'disease-{disease}-remedy-{kind}-'+('floor' if floor else 'carried'),spec,
                    consume(command,kind,cure,floor),'disease')
    for disease in (1,7,8):
        for cure,kind in ((0,380),(8,388),(9,389)):
            spec=dict(stats=7,food=2500,disease=200,disease_name=disease,disease_cure=cure,
                disease_stats=[0,0,-1,0],items=[dict(kind=kind,number=3)])
            def assigned(s,disease=disease,kind=kind):
                if disease==8:
                    assert s.p['disease']>0 and s.p['disease_name']==8 and not s.p['disease_immunity'],s.brief()
                else:
                    assert not s.p['disease'] and s.p['disease_stats']==[0,0,0,0],s.brief()
                    assert s.p['food']<=s.initial['food'] and s.p['drain']==s.initial['drain'],s.brief()
                    if kind==380:assert not s.p['rage']
            c.variant(f'assigned-cure-{disease}-{kind}',spec,consume('E',kind,assigned,False),'disease')
    def immune(s):assert s.p['disease']==0 and s.p['disease_immunity']==255,s.brief()
    c.variant('all-disease-immunity',dict(stats=7,food=2500,disease_immunity=255,
        items=[dict(kind=389,number=3)]),consume('E',389,immune,False),'disease')


def weaken_will(spec):
    # A legal, severely drained mid-run character with no trained Will.
    # Effective Will < 0 makes the default difficulty-10 save fail even on
    # its best opposed d10 roll. High-Will controls exercise the other branch.
    spec['drain']=[0,0,0,-11]
    spec['skill_values']=[15,15,15,15,15,0,40,15,0]


def resistance(c: Campaign):
    for kind,command,field,index in (
        (315,'q','stun',None),(343,'q','slow',None),(345,'q','blind',None),
        (346,'q','confused',None),(348,'q','drain',1),(350,'q','drain',3),
        (382,'E','fear',None),(386,'E','image',None),(387,'E','entranced',None),
        (388,'E','drain',0),
    ):
        for weak in (False,True):
            spec=dict(stats=7,food=2500,items=[dict(kind=kind,number=3)])
            if weak:weaken_will(spec)
            else:spec['skill_values']=[15,15,15,15,15,40,40,15,0]
            def oracle(s,weak=weak,field=field,index=index):
                if field=='drain':affected=s.p['drain'][index]<s.initial['drain'][index]
                else:affected=any(e[field]>0 for e in s.turns())
                assert affected==weak,(field,weak,s.brief())
            c.variant(f'saving-throw-{kind}-'+('drained' if weak else 'trained'),spec,
                consume(command,kind,oracle,False),'resistance')


def inventory(c: Campaign):
    def transfer(s):
        s.setup('Daggers');s.choose('Store in Pack')
        assert s.p['player_turn']==s.initial['player_turn']+3,s.brief()
        assert s.total(56)==3 and all(o['storage']==1 for o in s.p['items'] if o['kind']==56)
        s.limits();s.setup('Daggers')
        if s.p.get('confirmation'):s.press('y')
        else:s.choose('Harness')
        assert s.p['player_turn']==s.initial['player_turn']+6,s.brief()
        assert s.total(56)==3 and all(o['storage']==2 for o in s.p['items'] if o['kind']==56)
        s.limits()
    c.variant('dagger-storage-roundtrip',dict(items=[dict(kind=56,number=3,storage=2)]),transfer,'inventory')
    for storage in (1,2):
        def drop(s,storage=storage):
            s.setup('Daggers');s.press('Escape','z')
            assert 'how many' in s.question.get('title',''),s.question
            s.press('Escape','z','-','Enter','n')
            assert s.total(56)==3 and s.p['player_turn']==s.initial['player_turn']
            assert s.p['rng']==s.initial['rng']
            s.press('z','-','Enter','y')
            assert s.total(56,'floor')==2 and s.total(56)==3
            assert s.p['player_turn']==s.initial['player_turn']+(3 if storage==1 else 1),s.brief()
            s.limits()
        c.variant('drop-partial-stack-'+str(storage),dict(items=[dict(kind=56,number=3,storage=storage)]),drop,'inventory')
    for arrows,incoming in ((47,1),(48,1),(47,2),(48,50)):
        def pickup_arrows(s,arrows=arrows,incoming=incoming):
            s.press('g')
            assert not s.question,s.question
            # Picking up stores loose arrows; Equipping explicitly fills Quiver.
            assert s.total(116)==arrows+incoming and s.total(116,'floor')==max(0,incoming-48),s.p['items']
            assert s.p['arrows']==arrows,s.brief()
            s.limits()
        c.variant(f'arrow-pickup-{arrows}-{incoming}',dict(items=[dict(kind=116,number=arrows,quiver=1),
            dict(kind=116,number=incoming,floor=1)]),pickup_arrows,'inventory')
    for arrows,incoming in ((47,1),(47,2),(48,1)):
        def equip_arrows(s,arrows=arrows,incoming=incoming):
            s.press('w','-','y')
            if s.question.get('title','').startswith('Your Quiver can only hold'):
                s.press('Enter')
            s.close_views()
            assert s.total(116)==arrows+incoming
            assert s.p['arrows']==min(48,arrows+incoming),s.brief()
            assert s.total(116,'floor')==max(0,arrows+incoming-48),s.p['items']
            if arrows==48:assert s.p['player_turn']==s.initial['player_turn']
            s.limits()
        c.variant(f'arrow-equip-{arrows}-{incoming}',dict(items=[dict(kind=116,number=arrows,quiver=1),
            dict(kind=116,number=incoming,floor=1)]),equip_arrows,'inventory')
    def supply_full(s):
        s.press('g')
        s.close_views()
        assert s.total(399)==50 and s.total(318,'floor')==1,s.p['items']
        assert s.p['player_turn']==s.initial['player_turn'],s.brief()
        s.limits()
    c.variant('supplies-full-cancel',dict(items=[dict(kind=399,number=50),dict(kind=318,floor=1)]),supply_full,'inventory')
    def supply_exact(s):
        s.press('g')
        assert s.total(318,'supplies')==1 and s.total(318,'floor')==0
        assert s.p['supply_weight']==s.p['supply_cap'];s.limits()
    c.variant('supplies-exact-cap',dict(items=[dict(kind=399,number=49),dict(kind=318,number=1,floor=1)]),supply_exact,'inventory')
    for route in ('keyboard','browser','menu'):
        def pickup_route(s,route=route):
            if route=='keyboard':s.press('g')
            elif route=='browser':s.press('i','Tab','Tab','-','y')
            else:s.press('m','i','Tab','Tab','-','y')
            assert s.total(399,'supplies')==2 and s.total(399,'floor')==0
            assert s.p['player_turn']==s.initial['player_turn']+1,s.brief()
        c.variant('pickup-route-'+route,dict(items=[dict(kind=399,number=2,floor=1)]),pickup_route,'inventory')
    for hero in (0,4):
        def two_handed(s,hero=hero):
            s.press('w','-','n')
            assert s.p['player_turn']==s.initial['player_turn']
            s.press('-','y')
            wield=next(o for o in s.p['items'] if o['store']=='equipped' and o['handle']==24)
            assert wield['kind']==(64 if hero==4 else 68),s.p['items']
            assert s.total(68,'floor')==(1 if hero==4 else 0)
            assert s.p['player_turn']==s.initial['player_turn']+(0 if hero==4 else 1)
            s.close_views();s.clean_selector();s.limits()
        c.variant('two-handed-hero-'+str(hero),dict(hero=hero,stats=7,items=[dict(kind=68,floor=1)]),two_handed,'equipment')
    def cursed(s):
        s.press('w','-','y');s.close_views()
        wield=next(o for o in s.p['items'] if o['store']=='equipped' and o['handle']==24)
        assert wield['kind']==64 and wield['cursed'] and s.total(68,'floor')==1
        assert s.p['player_turn']==s.initial['player_turn'];s.clean_selector()
    c.variant('cursed-weapon-replacement',dict(weapon_cursed=1,stats=7,items=[dict(kind=68,floor=1)]),cursed,'equipment')


def work(c: Campaign):
    for pause in (1,3,8):
        def smith(s,pause=pause):
            s.press('0','a','F')
            assert s.p['fixture_work_pause'] and s.p['smithing_leftover']==10-pause,s.brief()
            s.press('Escape')
            assert not s.p['smithing'] and s.p['smithing_leftover']==10-pause
            checkpoint=s.checkpoint();saved=copy.deepcopy(s.p)
            branch=Scene(base.start('resumed-smith',dict(load_from=str(checkpoint)),c.exe,c.env));s.children.append(branch)
            for key in ('hp','voice','food','exp','new_exp','smithing_leftover','items','drain'):
                assert branch.p[key]==saved[key],(key,branch.brief(),s.brief())
            before=branch.p['player_turn'];left=branch.p['smithing_leftover']
            branch.press('0','Enter')
            assert branch.p['player_turn']==before+left and not branch.p['smithing_leftover']
            assert branch.p['feat']==66 and branch.total(56)==5 and branch.total(56,'work')==0
            branch.limits()
        c.variant('interrupted-smith-'+str(pause),dict(pause_work_after=pause,
            abilities=[dict(skill=6,ability=0)],tiles=[dict(y=12,x=12,feat=67)]),smith,'work')
    for quiver in (False,True):
        def fletch(s,quiver=quiver):
            s.press('-','a');assert s.p['fixture_work_pause']
            left=s.p['fletching'];assert 0<left<10
            s.press('Escape');assert not s.p['fletching'] and s.total(116)==10
            improved=sum(o['number'] for o in s.p['items'] if o['kind']==116 and o['attack']==3)
            assert improved==10-left,(left,improved,s.p['items'])
            checkpoint=s.checkpoint()
            branch=Scene(base.start('loaded-fletch',dict(load_from=str(checkpoint)),c.exe,c.env));s.children.append(branch)
            assert branch.total(116)==10
            assert sum(o['number'] for o in branch.p['items'] if o['kind']==116 and o['attack']==3)==improved
            branch.limits()
        c.variant('partial-fletch-'+('quiver' if quiver else 'harness'),dict(pause_work_after=4,
            abilities=[dict(skill=1,ability=1)],items=[dict(kind=116,number=10,quiver=int(quiver))]),fletch,'work')


def ui_rng(c: Campaign):
    views=[('inventory',('i',)),('equipment',('e',)),('character',('h',)),
        ('known-lore',('~',)),('help',('?',)),('abilities',('y',)),('map',('M',)),
        ('look',('l',)),('messages',('Ctrl+p',)),('combat-history',('Ctrl+q',)),
        ('settings',('O',)),('quests',('m','t')),('achievements',('m','v')),
        ('tale',('m','y')),('statistics',('m','g')),('tutorials',('m','u')),
        ('halls',('m','d')),('about',('m','b')),
        ('smith-root',('0',)),('smith-draft',('0','a')),
        ('redraw',('Ctrl+r',)),('hallucination-redraw',('Ctrl+r',)),
        ('hallucination-ascii',('Ctrl+r',))]
    for name,keys in views:
        spec=dict(items=[dict(kind=56,number=2)])
        if name.startswith('smith'):
            spec.update(abilities=[dict(skill=6,ability=0)],tiles=[dict(y=12,x=12,feat=67)])
        if name.startswith('hallucination-'):
            spec.update(image=30,items=[dict(kind=318,number=2,floor=1,y=12,x=13)])
        if name=='hallucination-ascii':spec['ascii']=1
        def browse(s,keys=keys):
            s.press(*keys);s.close_views()
            assert s.p['player_turn']==s.initial['player_turn'] and s.p['turn']==s.initial['turn']
            assert s.p['rng']==s.initial['rng'],('Free view changed game RNG',s.brief())
            assert [o for o in s.p['items'] if o['store']!='work']==s.initial['items']
        c.variant('rng-'+name,spec,browse,'ui_rng')
    def equivalence(s):
        spec=copy.deepcopy(json.loads((s.profile/'scenario.json').read_text()))
        branch=Scene(base.start('with-extra-cancel',spec,c.exe,c.env));s.children.append(branch)
        s.press('q','a','y')
        branch.press('q','a','n','a','y')
        assert s.p==branch.p,('Cancelled browsing changed paid outcome',s.brief(),branch.brief())
    c.variant('cancel-vs-direct-poison',dict(stats=7,items=[dict(kind=344,number=3)]),equivalence,'ui_rng')


DIRECTIONS=(('north','Up',-1,0),('north-east','PageUp',-1,1),('east','Right',0,1),
    ('south-east','PageDown',1,1),('south','Down',1,0),('south-west','End',1,-1),
    ('west','Left',0,-1),('north-west','Home',-1,-1))


def terrain(c: Campaign):
    for direction,key,dy,dx in DIRECTIONS:
        for feature in (2,85,87):
            def hazard(s,key=key,dy=dy,dx=dx,feature=feature):
                s.press(key);assert s.p.get('confirmation','').startswith('Step into'),s.question
                s.press('n')
                assert (s.p['y'],s.p['x'])==(12,12)
                assert s.p['player_turn']==s.initial['player_turn'] and s.p['rng']==s.initial['rng']
                s.press(key,'y')
                assert s.p['player_turn']==s.initial['player_turn']+1,s.brief()
                if feature==85:assert s.p['dead'] and s.p['hp']==0
                elif feature==2:assert s.p['depth']>s.initial['depth'] and s.p['leaving'],s.brief()
                else:
                    assert 0<s.p['poison']<=8 and s.p['hp']>0,s.brief()
                    severity=s.p['poison'];s.press(key)
                    assert (s.p['y'],s.p['x'])==(12+2*dy,12+2*dx)
                    assert s.p['poison']<=severity,s.brief()
            c.variant(f'hazard-{feature}-{direction}',dict(stats=7,
                tiles=[dict(y=12+dy,x=12+dx,feat=feature)]),hazard,'terrain')
        for feature,delay in ((84,15),(100,40),(86,10)):
            def movement(s,key=key,dy=dy,dx=dx,delay=delay):
                s.press(key)
                assert (s.p['y'],s.p['x'])==(12+dy,12+dx)
                assert s.p['player_turn']==s.initial['player_turn']+1
                assert s.p['turn']==s.initial['turn']+delay,s.brief()
                assert not s.p['dead']
            c.variant(f'movement-{feature}-{direction}',dict(stats=7,
                tiles=[dict(y=12+dy,x=12+dx,feat=feature)]),movement,'terrain')
        def protected(s,key=key,dy=dy,dx=dx):
            s.press(key)
            match=re.search(r'take (\d+) damage',s.p.get('confirmation',''))
            assert match,s.question
            expected=int(match.group(1));s.press('y')
            assert not s.p['dead'] and s.initial['hp']-s.p['hp']==expected,s.brief()
            health=s.p['hp'];s.press(key)
            assert (s.p['y'],s.p['x'])==(12+2*dy,12+2*dx)
            assert s.p['hp']>=health,'Leaving lava charged the old square again'
        c.variant('protected-lava-'+direction,dict(stats=7,items=[dict(kind=156,slot=27)],
            tiles=[dict(y=12+dy,x=12+dx,feat=85)]),protected,'terrain')
    for feature in (17,18,19,20,21,22,23,24,27,28,29,101):
        def disarm(s,feature=feature):
            s.press('D','Escape')
            assert s.p['player_turn']==s.initial['player_turn'] and s.p['tiles'][12][13]==feature
            s.press('D','Right')
            expected=feature if feature in (17,18) else 1
            assert s.p['tiles'][12][13]==expected and s.p['tiles'][12][11]==17,s.brief()
            if feature in (17,18):assert any('cannot disarm' in line for line in s.state['messages'])
            assert s.p['player_turn']==s.initial['player_turn']+1 and (s.p['y'],s.p['x'])==(12,12)
        c.variant('disarm-'+str(feature),dict(stats=7,skills=50,
            tiles=[dict(y=12,x=13,feat=feature),dict(y=12,x=11,feat=17)]),disarm,'traps')
    for kind in (372,373,374,375,376,377):
        def chest(s,kind=kind):
            s.press('o','Escape')
            assert s.p['player_turn']==s.initial['player_turn'] and s.total(kind,'floor')==1
            s.press('o');s.choose('Open:')
            boxes=[o for o in s.p['items'] if o['kind']==kind and o['store']=='floor']
            assert not boxes or all(o['pval']==0 for o in boxes),s.p['items']
            assert s.p['player_turn']==s.initial['player_turn']+1,s.brief()
            assert s.p['hp']==s.initial['hp'] and not s.p['dead']
        c.variant('open-disarmed-chest-'+str(kind),dict(stats=7,skills=50,
            items=[dict(kind=kind,floor=1,pval=-3)]),chest,'chests')


def exchange_reactions(c: Campaign):
    def slip_past_troll(s):
        s.press('X', 'Right')
        troll = s.p['targets'][0]
        messages = s.state['messages']
        knocked = 'You are knocked back.' in messages
        exchanged = any('You exchange places' in msg for msg in messages)
        if s.p['dead']:
            assert not exchanged and troll['x'] == 13
        elif knocked:
            assert (s.p['x'], troll['x']) == (11, 13) and not exchanged
            assert s.p['player_turn'] == s.initial['player_turn'] + 2
        else:
            assert (s.p['x'], troll['x']) == (13, 12) and exchanged
            assert s.p['player_turn'] == s.initial['player_turn'] + 1

    c.variant('exchange-with-mountain-troll', dict(stats=3, constitution=0,
              skills=0, food=4000, abilities=[dict(skill=3, ability=3)],
              targets=[dict(race=74, y=12, x=13, hp=90, alertness=10)]),
              slip_past_troll, 'exchange')


def resting_and_slow(c: Campaign):
    for count, light_off in ((1, True), (3, True), (7, False)):
        def timed_rest(s, count=count, light_off=light_off):
            def fuel(player):
                return next(item['timeout'] for item in player['items'] if item['handle'] == 30)

            s.press('R', str(count), 'Z')
            assert 'while resting' in s.p.get('confirmation', '')
            s.press('y' if light_off else 'n')
            assert s.p['player_turn'] == s.initial['player_turn'] + count, s.brief()
            assert not s.p['resting'] and s.p['light'] == s.initial['light']
            assert (fuel(s.p) == fuel(s.initial)) == light_off

        c.variant(f'timed-rest-{count}', dict(food=5000), timed_rest, 'upkeep')

    def find_door(s):
        assert s.initial['tiles'][12][13] == 48  # Secret door
        s.press('R', '7', 'Z', 'y')
        assert 32 <= s.p['tiles'][12][13] <= 47
        assert any('found a secret door' in msg for msg in s.state['messages'])
        assert s.p['player_turn'] == s.initial['player_turn'] + 1
        assert not s.p['resting'] and s.p['light'] == s.initial['light']

    c.variant('rest-finds-door-on-first-turn',
              dict(food=5000, skills=60, tiles=[dict(y=12, x=13, feat=48)]), find_door, 'upkeep')

    for skill in (0, 3):
        def recover(s, skill=skill):
            effective = max(0, s.initial['skills_use'][7])
            assert (effective == 0) == (skill == 0)
            s.press('s', 'd')  # Song of Freedom
            assert s.p['song'] == 3
            assert s.p['slow'] == max(0, s.initial['slow'] - max(1, effective))
            s.press('KP5')
            assert s.p['slow'] == max(0, s.initial['slow'] - 2 * max(1, effective))

        c.variant(f'freedom-slow-recovery-{skill}',
                  dict(food=5000, grace=-2, skills=skill, slow=8,
                       abilities=[dict(skill=7, ability=3)]), recover, 'upkeep')


def silence_oath(c: Campaign):
    def expect_intact(s):
        for key in ('broken_oaths', 'curse_stacks', 'player_turn', 'song', 'voice'):
            assert s.p[key] == s.initial[key], (key, s.p[key], s.initial[key])

    def observed(s):
        s.press('s', 'n')  # Song of Disguise
        assert any('while observed' in msg for msg in s.state['messages'])
        assert 'Breaking a Sacred Oath' not in s.terminal
        expect_intact(s)

    c.variant('silence-oath-observed-disguise',
              dict(oath=2, abilities=[dict(skill=7, ability=13)],
                   targets=[dict(race=15, x=14, alertness=0)]), observed, 'songs')

    def exhausted(s):
        s.press('s', 't')
        assert any('no Voice left' in msg for msg in s.state['messages'])
        assert 'Breaking a Sacred Oath' not in s.terminal
        expect_intact(s)

    c.variant('silence-oath-no-voice',
              dict(oath=2, voice=0, abilities=[dict(skill=7, ability=18)],
                   targets=[dict(race=15, x=14)]), exhausted, 'songs')

    def cancel_duel(s):
        s.press('s', 't')  # Song of Contest
        assert 'Breaking a Sacred Oath' in s.terminal
        s.press('y', 'Escape')
        assert any('before it finds a target' in msg for msg in s.state['messages'])
        expect_intact(s)

    c.variant('silence-oath-cancel-duel',
              dict(oath=2, abilities=[dict(skill=7, ability=18)],
                   targets=[dict(race=15, x=14)]), cancel_duel, 'songs')

    def sing_after_declining(s):
        s.press('s', 'a', 'n')  # Song of Elbereth
        expect_intact(s)
        s.press('s', 'a', 'y', 'Space', 'Space')
        assert s.p['broken_oaths'] == 2 and s.p['curse_stacks'] == s.initial['curse_stacks'] + 1
        assert s.p['song'] == 0 and s.p['player_turn'] == s.initial['player_turn'] + 1
        s.press('s', 's')
        assert s.p['song'] == 100 and s.p['curse_stacks'] == s.initial['curse_stacks'] + 1

    c.variant('silence-oath-decline-then-sing',
              dict(oath=2, voice=1, abilities=[dict(skill=7, ability=0)]), sing_after_declining, 'songs')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=OUT)
    p.add_argument('--seeds',nargs='+',type=int,default=[1701,7307,9919])
    p.add_argument('--only',nargs='+')
    p.add_argument('--groups',nargs='+')
    p.add_argument('--max-failures',type=int,default=12)
    p.add_argument('--workers',type=int,choices=(1,2,3,4),default=3)
    p.add_argument('--resume',nargs='+',type=Path)
    args=p.parse_args();campaign=Campaign(args)
    consumables(campaign)
    foods(campaign)
    diseases(campaign)
    resistance(campaign)
    inventory(campaign)
    work(campaign)
    ui_rng(campaign)
    terrain(campaign)
    exchange_reactions(campaign)
    resting_and_slow(campaign)
    silence_oath(campaign)
    campaign.finish()
    assert campaign.results,'No selected scenarios'
    campaign.receipt['finished_at']=datetime.now(timezone.utc).isoformat();campaign.save()
    return int(bool(campaign.receipt['failed']))


if __name__=='__main__':raise SystemExit(main())
