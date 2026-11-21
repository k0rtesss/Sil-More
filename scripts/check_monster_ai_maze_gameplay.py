#!/usr/bin/env python3
"""Play new seeded room/maze encounters through GameControl without a window."""
from __future__ import annotations

import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import random
import traceback

import check_monster_ai_stress_gameplay as stress

ROOT=stress.ROOT
play,engine,audit=stress.play,stress.engine,stress.audit
OUT=ROOT/'scripts/output/monster-ai-round9/mazes'


def paths(grid,origin):
    previous={origin:None}
    queue=deque([origin])
    while queue:
        y,x=queue.popleft()
        for dy,dx in ((-1,0),(0,1),(1,0),(0,-1)):
            cell=(y+dy,x+dx)
            if cell not in previous and grid.get(cell) in (1,4,5,84,86):
                previous[cell]=(y,x)
                queue.append(cell)
    return previous


def route(previous,target):
    result=[]
    while previous[target] is not None:
        result.append(target)
        target=previous[target]
    return list(reversed(result))


def generate(seed):
    rng=random.Random(seed)
    rows,cols=13,17
    grid={}
    def open_cell(r,c):grid[8+2*r,20+2*c]=1
    seen={(6,10)}
    stack=[(6,10)]
    open_cell(6,10)
    while stack:
        r,c=stack[-1]
        neighbors=[(r+dr,c+dc) for dr,dc in ((-1,0),(0,1),(1,0),(0,-1))
                   if 0<=r+dr<rows and 0<=c+dc<cols and (r+dr,c+dc) not in seen]
        if not neighbors:
            stack.pop();continue
        rr,cc=rng.choice(neighbors)
        open_cell(rr,cc)
        grid[8+r+rr,20+c+cc]=1
        seen.add((rr,cc));stack.append((rr,cc))
    # Rooms and loops add different lanes to the narrow spanning corridors.
    for _ in range(7):
        cy,cx=8+2*rng.randrange(1,12),20+2*rng.randrange(1,16)
        for y in range(cy-1,cy+2):
            for x in range(cx-2,cx+3):grid[y,x]=1
    for y in range(19,22):
        for x in range(39,42):grid[y,x]=1
    all_cells=list(grid)
    rng.shuffle(all_cells)
    for cell in all_cells[:12]:
        if max(abs(cell[0]-20),abs(cell[1]-40))>4:grid[cell]=rng.choice((32,33))
    for cell in all_cells[12:50]:
        if grid[cell]==1 and max(abs(cell[0]-20),abs(cell[1]-40))>2:
            grid[cell]=rng.choice((84,86))
    previous=paths(grid,(20,40))
    candidates=[cell for cell in previous if grid[cell]==1 and 6<=len(route(previous,cell))<=22]
    assert len(candidates)>=8
    rng.shuffle(candidates)
    candidates.sort(key=lambda p:len(route(previous,p)))
    targets=sorted(previous,key=lambda p:len(route(previous,p)),reverse=True)
    target=rng.choice(targets[:min(12,len(targets))])
    walk=route(previous,target)[:64]
    directions={(-1,0):'KP8',(0,1):'KP6',(1,0):'KP2',(0,-1):'KP4'}
    keys=['KP5']*24
    origin=(20,40)
    for y,x in walk:
        keys.append(directions[y-origin[0],x-origin[1]])
        origin=(y,x)
    keys+=['KP5']*24
    tiles=[dict(y1=1,y2=38,x1=1,x2=62,feat=63)]
    tiles += [dict(y1=y,x1=x,feat=feat) for (y,x),feat in sorted(grid.items())]
    return grid,tiles,candidates,keys


def cases(seed):
    families={
        'orcs':[91,81,61,41,51,51],
        'men':[409,82,121,103,103],
        'cats':[165,177,154,175],
        'wolves':[205,172,11,11,11],
        'giants':[135,146,114,114],
        'trolls':[214,192,151,74],
        'shadows':[409,408,82,103],
        'dragons-and-orc':[61,405,181,164],
    }
    for family,races in families.items():
        for variant in range(8):
            geometry_seed=(seed+int(hashlib.sha256(f'{family}-{variant}'.encode()).hexdigest()[:8],16)) & 0x7fffffff
            grid,tiles,positions,keys=generate(geometry_seed)
            party=[dict(race=race,y=y,x=x,heard=1,morale=150)
                   for race,(y,x) in zip(races,positions)]
            spec=dict(seed=geometry_seed,recognition=1,evasion=40,will=40,
                      allow_death=True,confirm_traps=True,decline_chasms=True,
                      tiles=tiles,monsters=party)
            previous=paths(grid,(20,40))
            spec['navigation_goal']=list(max(previous,key=lambda p:len(route(previous,p))))
            yield f'{family}-maze-{variant}',spec,keys,dict(
                geometry_sha256=hashlib.sha256(json.dumps(tiles,sort_keys=True).encode()).hexdigest())


def navigation_policy(spec):
    # The fixture marks its starting map as known. Subsequent map changes
    # come only from currently visible tiles, never hidden monster positions.
    known={(t['y1'],t['x1']):t['feat'] for t in spec['tiles']
           if 'y2' not in t and 'x2' not in t}
    goal=tuple(spec['navigation_goal'])
    blocked={}
    directions={(-1,0):'KP8',(0,1):'KP6',(1,0):'KP2',(0,-1):'KP4'}
    reverse={v:k for k,v in directions.items()}
    prior=None
    def choose(profile,frames):
        nonlocal prior,goal
        frame=frames[-1]
        now=frame['player_turn']
        start=frames[0]['player_turn']
        if now-start>=120 or len(frames)>=360:return None
        menu=frame.get('question_menu')
        if menu:
            for label in ('Open it','Step onto it'):
                choice=next((o for o in menu['choices'] if o['label']==label),None)
                if choice:return choice['key']
            return 'Escape'
        if frame.get('question','').startswith('Step into'):
            return 'n'
        player=frame['player']
        current=(player['y'],player['x'])
        for y,x,feature in frame.get('visible_terrain',[]):known[y,x]=feature
        if prior:
            old,turn,key=prior
            if current==old and now==turn and key in reverse:
                dy,dx=reverse[key]
                blocked[old[0]+dy,old[1]+dx]=now+8
        key='KP5'
        if 16<=now-start<96:
            available={p:f for p,f in known.items() if blocked.get(p,-1)<now}
            previous=paths(available,current)
            if goal==current or goal not in previous:
                goal=max(previous,key=lambda p:len(route(previous,p)))
            walk=route(previous,goal)
            if walk:
                y,x=walk[0]
                key=directions[y-current[0],x-current[1]]
        prior=(current,now,key)
        return key
    return choose


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seed',type=int,default=20261123)
    parser.add_argument('--filter',default='')
    parser.add_argument('--exclude-filter',default='')
    parser.add_argument('--worker',type=int,default=0)
    parser.add_argument('--workers',type=int,default=1)
    parser.add_argument('--output-dir',type=Path,default=OUT)
    args=parser.parse_args()
    destination=args.output_dir.resolve()
    stress.instrument(destination)
    play.FIXTURE=play.FIXTURE.replace(
        '        cJSON* grid = cJSON_AddArrayToObject(root, "grid");',r'''
        cJSON* visible = cJSON_AddArrayToObject(root, "visible_terrain");
        for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
            for (int x = 0; x < p_ptr->cur_map_wid; ++x) {
                if (!player_can_see_bold(y, x)) continue;
                cJSON* tile = cJSON_CreateArray();
                cJSON_AddItemToArray(tile, cJSON_CreateNumber(y));
                cJSON_AddItemToArray(tile, cJSON_CreateNumber(x));
                cJSON_AddItemToArray(tile, cJSON_CreateNumber(cave_feat[y][x]));
                cJSON_AddItemToArray(visible, tile);
            }
        cJSON* grid = cJSON_AddArrayToObject(root, "grid");''')
    exe,env=audit.build()
    env['SIL_LOG_LEVEL']='DEBUG'
    env['AI_PLAYTEST_SIGNATURE']=hashlib.sha256((env['AI_PLAYTEST_SIGNATURE']+Path(__file__).read_text()).encode()).hexdigest()
    (destination/'driver-source.py').write_bytes(Path(__file__).read_bytes())
    results,failures=[],[]
    for name,spec,keys,check in cases(args.seed):
        if args.filter and args.filter not in name:continue
        if args.exclude_filter and args.exclude_filter in name:continue
        if int(hashlib.sha256(name.encode()).hexdigest(),16)%args.workers!=args.worker:continue
        try:
            result=stress.verify(play.run(exe,audit.case_env(env,spec),name,spec,navigation_policy(spec)),{})
            frames,events=engine.read_run(result)
            assert result['died'] or result['player_turns']>=120, 'Navigation stalls before its gameplay budget'
            assert any(e['kind'] in ('melee','ranged','reaction') for e in events),'No engagement in a connected maze'
            result.update(maze_check=check,case_name=name)
            results.append(result)
        except Exception as exc:
            failure=dict(name=name,error=str(exc),traceback=traceback.format_exc())
            failures.append(failure)
            print(json.dumps(failure),flush=True)
        report=destination/'results.json'
        temporary=report.with_suffix('.tmp')
        temporary.write_text(json.dumps(dict(results=results,failures=failures),indent=2))
        temporary.replace(report)
    print(f'Maze encounters: {len(results)} passed, {len(failures)} failures',flush=True)
    raise SystemExit(bool(failures))


if __name__=='__main__':main()
