#!/usr/bin/env python3
"""Measure and visualize actual full-pipeline terrain, including its failures.

Run check_dungeon_terrain_pipeline.py first. This report uses exported game cells,
not synthetic artwork. --baseline reports the older small terrain without
claiming that it passes the landmark acceptance criteria.
"""
import argparse
from collections import Counter, deque
import html
import json
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "scripts/output/dungeon-terrain-pipeline"
MATERIAL = {2: "chasm", 84: "water", 85: "lava", 86: "ice", 87: "poison"}
COLORS = {1: "#b8ad92", 2: "#080b13", 84: "#298bd2", 85: "#fc612c",
          86: "#a0e6f1", 87: "#6faf42", 80: "#ffffff", 81: "#ffffff",
          82: "#ffffff", 83: "#ffffff", 49: "#947e69"}
FAMILIES = ("Pool chain", "Tributary network", "Flooded chamber", "Perched pools",
            "Major underground river", "Fracture network")
TERMINALS = {1: "edge mouth", 2: "wall spring", 3: "wall sink",
             4: "receiving basin", 5: "lava vent", 6: "fissure tip"}
SCENARIOS = ("Lake to lake", "Cross-map flow", "Spring and sink", "Volcanic flow",
             "Perched pools", "Flooded works", "Fractures")
ROCK = {51, 56, 57, 58, 59, 63}
COLORS.update({feature: "#b79562" if feature<92 else "#a5aa9e" for feature in range(88,98)})
EPOCHS = {0: "Built around older geology", 1: "Later terrain disaster", 2: "Older river overflow"}


def bridge_underlay(feature):
    return (84,2,85,87,86)[(feature-88)//2] if 88<=feature<=97 else feature


def system_views(data):
    if not data.get("systems"):
        return [data]
    views=[]
    for system in data["systems"]:
        view={**data,**system}
        view.pop("systems",None)
        views.append(view)
    return views


def components(mask):
    height, width = len(mask), len(mask[0])
    seen = set()
    for y in range(height):
        for x in range(width):
            if not mask[y][x] or (y, x) in seen:
                continue
            cells = [(y, x)]
            seen.add((y, x))
            for cy, cx in cells:
                for ny, nx in ((cy-1,cx), (cy+1,cx), (cy,cx-1), (cy,cx+1)):
                    if (0 <= ny < height and 0 <= nx < width and mask[ny][nx]
                            and (ny,nx) not in seen):
                        seen.add((ny,nx))
                        cells.append((ny,nx))
            yield cells


def shape_metrics(cells, data):
    if not cells:
        return {"area": 0, "span_percent": 0, "partitions": 0}
    ys, xs = zip(*cells)
    height, width = max(ys)-min(ys)+1, max(xs)-min(xs)+1
    rows, cols = Counter(ys), Counter(xs)
    return {"area": len(cells), "bbox": [min(ys),min(xs),max(ys),max(xs)],
            "height": height, "width": width,
            "span_percent": round(100*max(height/data["height"],width/data["width"]),2),
            "minor_span_percent": round(100*min(height/data["height"],width/data["width"]),2),
            "bbox_fill": round(len(cells)/(height*width),3),
            "mean_cross_section": round(len(cells)/max(height,width),2),
            "max_cross_section": max(rows.values()) if width < height else max(cols.values()),
            "partitions": len({data["partition_ids"][y][x] for y,x in cells
                               if data["partition_ids"][y][x] >= 0})}


def morphology(data):
    """Measure final raster geometry; basin labels never define basin cores.

    Chebyshev distance to dry shore erodes all one/two-cell connectors away.
    Surviving broad components therefore distinguish pools joined by necks
    from one uniform ribbon. Local widths use the actual raster at each
    designed channel spine cell, not the generator's requested width.
    """
    h,w=data["height"],data["width"]
    mask=data.get("landmark_cells",data["new_terrain"])
    distance=[[0]*w for _ in range(h)]
    queue=deque()
    for y in range(h):
        for x in range(w):
            if not mask[y][x]: queue.append((y,x))
            else: distance[y][x]=h+w
    while queue:
        y,x=queue.popleft()
        for dy in (-1,0,1):
            for dx in (-1,0,1):
                yy,xx=y+dy,x+dx
                if 0<=yy<h and 0<=xx<w and distance[yy][xx]>distance[y][x]+1:
                    distance[yy][xx]=distance[y][x]+1
                    queue.append((yy,xx))
    cores=[g for g in components([[d>=2 for d in row] for row in distance]) if len(g)>=3]
    cores.sort(key=len,reverse=True)
    spine=data.get("landmark_channels",[[0]*w for _ in range(h)])
    basins=data.get("landmark_basins",[[0]*w for _ in range(h)])
    bridges=data.get("landmark_bridges",[[0]*w for _ in range(h)])
    widths=Counter(); thin=[[0]*w for _ in range(h)]; invalid_spine=0
    for y in range(h):
        for x in range(w):
            if not spine[y][x]:continue
            if not mask[y][x] or basins[y][x]:invalid_spine+=1
            runs=[]
            for dy,dx in ((0,1),(1,0)):
                run=1
                for sign in (-1,1):
                    yy,xx=y+sign*dy,x+sign*dx
                    while 0<=yy<h and 0<=xx<w and mask[yy][xx]:
                        run+=1;yy+=sign*dy;xx+=sign*dx
                runs.append(run)
            width=min(runs);widths[width]+=1
            thin[y][x]=width==1
    necks=sorted((len(c) for c in components(thin)),reverse=True)
    # Count degree-three junction clusters directly on the designed spine.
    junctions=[[0]*w for _ in range(h)]
    for y in range(1,h-1):
        for x in range(1,w-1):
            junctions[y][x]=bool(spine[y][x] and sum(bool(spine[yy][xx]) for yy,xx in
                ((y-1,x),(y+1,x),(y,x-1),(y,x+1)))>=3)
    channel_count=sum(widths.values())
    dry_islands=[c for c in components([[not v for v in row] for row in mask])
                 if not any(y in (0,h-1) or x in (0,w-1) for y,x in c)]
    bank_neighbors=Counter()
    banks=data.get("landmark_banks",[[0]*w for _ in range(h)])
    for y in range(1,h-1):
        for x in range(1,w-1):
            if spine[y][x]:
                bank_neighbors[sum(bool(banks[yy][xx]) for yy,xx in
                    ((y-1,x),(y+1,x),(y,x-1),(y,x+1)))]+=1
    return {"broad_core_count":len(cores),"broad_core_areas":[len(c) for c in cores],
            "broad_core_bounds":[[min(y for y,x in c),min(x for y,x in c),
                                  max(y for y,x in c),max(x for y,x in c)] for c in cores],
            "max_distance_to_shore":max(map(max,distance)),
            "channel_width_histogram":dict(sorted(widths.items())),"channel_samples":channel_count,
            "one_cell_percent":round(100*widths[1]/channel_count,1) if channel_count else 0,
            "longest_thin_link":max(necks,default=0),"junction_clusters":len(list(components(junctions))),
            "invalid_spine_cells":invalid_spine,
            "dry_island_areas":sorted((len(c) for c in dry_islands),reverse=True),
            "tiny_dry_islands":sum(len(c)<=4 for c in dry_islands),
            "channel_bank_neighbor_histogram":dict(sorted(bank_neighbors.items())),
            "wet_area":sum(bool(mask[y][x]) and not bridges[y][x]
                           for y in range(h) for x in range(w))}


def measure(data):
    mask = data.get("landmark_cells", data["new_terrain"])
    groups = [shape_metrics(cells,data) for cells in components(mask)]
    groups.sort(key=lambda x: x["area"],reverse=True)
    whole = shape_metrics([(y,x) for y,row in enumerate(mask)
                           for x,value in enumerate(row) if value],data)
    return {"depth": data["depth"], "seed": data["seed"], "attempts": data["attempts"],
            "system":data.get("system"),"epoch":data.get("epoch"),
            "elapsed_ms": data["elapsed_ms"], "landmark": data.get("landmark"),
            "network":data.get("network"),"flow":data.get("flow"),
            "structures":data.get("structures"),"morphology":morphology(data),
            "whole": whole, "components": groups}


def terminal_geometry(data):
    """Check endpoint pixels against their physical setting, not their label."""
    h,w=data["height"],data["width"]
    mask=data["landmark_cells"]
    features=data.get("terrain_after",data["features"])
    labels=data.get("landmark_terminals")
    if labels is None:
        return ["Missing endpoint cells; source/outlet counts alone are not geometry proof"]
    basins=data.get("landmark_basins",[[0]*w for _ in range(h)])
    cores=set()
    for y in range(1,h-1):
        for x in range(1,w-1):
            if basins[y][x] and all(mask[y+dy][x+dx] for dy in (-1,0,1) for dx in (-1,0,1)):
                cores.add(basins[y][x])
    errors=[];total=0
    for y,row in enumerate(labels):
        for x,kind in enumerate(row):
            if not kind:continue
            total+=1; where=f"{TERMINALS.get(kind,'unknown terminal')} at ({y},{x})"
            if not 0<y<h-1 or not 0<x<w-1 or not mask[y][x]:
                errors.append(f"{where} is outside the traversable hydraulic map")
                continue
            if data.get("landmark") and features[y][x]!=(84,2,85,87,86)[data["landmark"]["material"]]:
                errors.append(f"{where} was replaced with dry floor or a different material")
            if kind==1:
                if y not in (1,h-2) and x not in (1,w-2):
                    errors.append(f"{where} stops inside the map instead of crossing its edge")
            elif kind==4:
                if basins[y][x] not in cores:
                    errors.append(f"{where} lacks a real broad basin core")
            elif kind==5:
                footprint=(features[y][x]==85 and all(features[y+dy][x+dx]==85
                    for dy in (-1,0,1) for dx in (-1,0,1)))
                rim=any(features[yy][xx] in ROCK for yy in range(max(0,y-2),min(h,y+3))
                        for xx in range(max(0,x-2),min(w,x+3)))
                if not footprint or not rim:
                    errors.append(f"{where} lacks its full 3x3 lava opening and nearby rock rim")
            elif kind in (2,3,6):
                wall=any(features[yy][xx] in ROCK for yy,xx in ((y-1,x),(y+1,x),(y,x-1),(y,x+1)))
                neighbors=sum(bool(mask[yy][xx]) for yy,xx in ((y-1,x),(y+1,x),(y,x-1),(y,x+1)))
                widths=[]
                for dy,dx in ((0,1),(1,0)):
                    width=1
                    for sign in (-1,1):
                        yy,xx=y+sign*dy,x+sign*dx
                        while 0<=yy<h and 0<=xx<w and mask[yy][xx]:
                            width+=1;yy+=sign*dy;xx+=sign*dx
                    widths.append(width)
                if not wall or min(widths)!=1 or neighbors!=1:
                    errors.append(f"{where} lacks a narrow throat against intact rock")
                if kind==6 and features[y][x]!=2:
                    errors.append(f"{where} does not contain chasm terrain")
            else:errors.append(f"Unknown endpoint kind {kind}")
    if total<2:errors.append("Accepted network has fewer than two physically marked terminals")
    return errors


def terminal_self_test():
    h=w=15
    features=[[56]*w for _ in range(h)];mask=[[0]*w for _ in range(h)]
    labels=[[0]*w for _ in range(h)];basins=[[0]*w for _ in range(h)]
    for x in range(3,12):features[7][x]=84;mask[7][x]=1
    labels[7][3]=2;labels[7][11]=3
    data={"height":h,"width":w,"features":features,"landmark_cells":mask,
          "landmark_terminals":labels,"landmark_basins":basins}
    assert not terminal_geometry(data)
    data["landmark"]={"material":0};features[7][3]=1
    assert any("replaced with dry floor" in e for e in terminal_geometry(data))
    features[7][3]=84;data.pop("landmark")
    labels[7][3]=0;labels[7][7]=2
    assert any("narrow throat" in e for e in terminal_geometry(data))
    labels[7][7]=0;labels[7][3]=2
    labels[7][3]=1
    assert any("instead of crossing" in e for e in terminal_geometry(data))
    labels[7][3]=2
    for y in (6,8):
        for x in range(2,5):features[y][x]=84;mask[y][x]=1
    assert any("narrow throat" in e for e in terminal_geometry(data))
    labels[7][3]=5
    assert any("3x3 lava" in e for e in terminal_geometry(data))
    for y in range(6,9):
        for x in range(2,5):features[y][x]=85;mask[y][x]=1
    assert not terminal_geometry(data)
    labels[7][3]=4
    assert any("broad basin core" in e for e in terminal_geometry(data))
    basins[7][3]=1
    assert not terminal_geometry(data)


def morphology_self_test():
    """Guard the independent metric against the rejected uniform-band shape."""
    h,w=30,60
    mask=[[0]*w for _ in range(h)];spine=[[0]*w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            mask[y][x]=((y-15)**2+(x-12)**2<=5**2 or
                        (y-15)**2+(x-46)**2<=7**2 or y==15 and 12<=x<=46)
    for x in range(18,39):spine[15][x]=1
    data={"height":h,"width":w,"new_terrain":mask,"landmark_cells":mask,
          "landmark_channels":spine}
    measured=morphology(data)
    assert measured["broad_core_count"]==2
    assert len(set(measured["broad_core_areas"]))==2
    assert measured["one_cell_percent"]==100 and measured["longest_thin_link"]==21
    ribbon=[[10<=y<=20 and 5<=x<=55 for x in range(w)] for y in range(h)]
    data["landmark_cells"]=ribbon
    measured=morphology(data)
    assert measured["broad_core_count"]==1 and measured["one_cell_percent"]==0
    assert measured["longest_thin_link"]==0
    # Visible separated pools must still have two cores without pretending
    # that a hidden geological connection is traversable liquid.
    for x in range(18,39):mask[15][x]=0
    data["landmark_cells"]=mask;data["landmark_channels"]=[[0]*w for _ in range(h)]
    measured=morphology(data)
    assert measured["broad_core_count"]==2 and measured["channel_samples"]==0


def validate(data, metrics):
    failures = []
    landmark = data.get("landmark")
    if landmark is None:
        return ["No landmark instrumentation: local terrain is not landmark proof"]
    if data["attempts"] > 40:
        failures.append("Whole-level generation exceeded 40 attempts")
    if not landmark["accepted"]:
        return failures
    failures.extend(terminal_geometry(data))
    if landmark["attempted"] > 24:
        failures.append("Landmark exceeded 24 bounded candidate attempts")
    shape = metrics["whole"]
    network=data.get("network")
    if network is None:
        return failures+["Missing network metadata; legacy ribbon acceptance is not network proof"]
    family=network["family"]
    minimum=network["min_span_percent"]
    largest = metrics["components"][0] if metrics["components"] else {"area": 0}
    if family not in (2,3) and shape["span_percent"]+.01<minimum:
        failures.append(f"Network extent {shape['span_percent']}% is below configured {minimum}%")
    if family not in (2,3) and len(data["partitions"])>1 and shape["partitions"]<2:
        failures.append("Extended network does not cross a partition boundary")
    if family!=3 and largest["area"] < shape["area"] * .75:
        failures.append("Less than 75% of landmark belongs to its main connected shape")
    geometry=metrics["morphology"]
    if metrics.get("preview"):
        cy,cx=metrics["preview"]["center"]
        if not data["landmark_cells"][cy][cx]:
            failures.append("Room crop is centered outside the actual network")
    if geometry["invalid_spine_cells"]:
        failures.append("Channel spine contains dry cells or basin interiors")
    feature=(84,2,85,87,86)[landmark["material"]]
    changed_liquid=[(y,x) for y,row in enumerate(data["landmark_cells"]) for x,v in enumerate(row)
                    if v and not data["landmark_bridges"][y][x]
                    and data["features"][y][x]!=feature]
    if changed_liquid:
        failures.append(f"Final map changed {len(changed_liquid)} claimed network terrain cells")
    for name,expected in (("landmark_rubble",49),("landmark_repairs",1)):
        for y,row in enumerate(data.get(name,[])):
            for x,value in enumerate(row):
                if not value:continue
                if data["landmark_cells"][y][x]:
                    failures.append(f"Dry {name} tile at ({y},{x}) incorrectly inflates water geometry")
                if data["terrain_after"][y][x]!=expected:
                    failures.append(f"{name} tile at ({y},{x}) has the wrong committed feature")
                if name=="landmark_rubble" and data["terrain_before"][y][x] not in ROCK:
                    failures.append(f"Rubble at ({y},{x}) was placed on an original usable floor")
    if family==0:
        if geometry["broad_core_count"]<2:
            failures.append("Pool chain lacks two independently measured broad basin cores")
        elif len(set(geometry["broad_core_areas"]))<2:
            failures.append("Pool chain basin cores have no size variation")
        if geometry["longest_thin_link"]<3:
            failures.append("Pool chain lacks a sustained one-cell connecting reach")
    if family in (2,3) and not geometry["broad_core_count"]:
        failures.append("Pool/chamber family has no independently measured broad basin core")
    if family==3 and geometry["broad_core_count"]<2:
        failures.append("Perched pools lack multiple independently measured basin cores")
    if family in (0,1):
        if geometry["channel_samples"]<3 or geometry["one_cell_percent"]<40:
            failures.append(f"Only {geometry['one_cell_percent']}% of actual channel samples are one cell wide (minimum40%)")
    if family==1 and not geometry["junction_clusters"]:
        failures.append("Tributary network has no independently measured branch junction")
    bridge_mask=data.get("landmark_bridges")
    if bridge_mask:
        bridge_groups=list(components(bridge_mask))
        # The terrain pass must lay an intact floor span. Later ordinary trap
        # placement may put a trap on that floor (for example a dart trap);
        # rubble, walls and renewed river/chasm terrain still break the span.
        changed=[(y,x) for group in bridge_groups for y,x in group
                 if not 88<=data["terrain_after"][y][x]<=97
                 or data["features"][y][x]!=data["terrain_after"][y][x]
                 or bridge_underlay(data["features"][y][x])!=feature]
        if changed:
            failures.append(f"Generation broke {len(changed)} typed bridge decks or their material identity")
    # Every critical pre-existing cell must survive the terrain operation.
    before, after = data.get("terrain_before"), data.get("terrain_after")
    protected = data.get("critical_before")
    if before is not None and after is not None and protected is not None:
        changed = [(y,x) for y,row in enumerate(protected) for x,v in enumerate(row)
                   if v and before[y][x] != after[y][x]]
        if changed:
            failures.append(f"Terrain altered {len(changed)} critical authored/occupied cells")
    if "epoch" in data:
        initial=data["initial_terrain"]
        if data["epoch"]==2:
            actual_added=sum(bool(v) and bridge_underlay(data["terrain_before"][y][x])!=feature
                and bridge_underlay(data["features"][y][x])==feature
                for y,row in enumerate(data["landmark_cells"]) for x,v in enumerate(row))
            if data["structures"].get("overflow_tiles",0)>actual_added:
                failures.append("Reported overflow expansion exceeds independently measured added terrain")
        if data["epoch"] in (0,2):
            missing_before=[(y,x) for y,row in enumerate(initial) for x,value in enumerate(row)
                            if value and bridge_underlay(data["terrain_before"][y][x])!=value]
            missing_after=[(y,x) for y,row in enumerate(initial) for x,value in enumerate(row)
                           if value and bridge_underlay(data["features"][y][x])!=value]
            if missing_before:failures.append(f"Construction erased {len(missing_before)} cells of older geology")
            if missing_after:failures.append(f"Later passes erased {len(missing_after)} cells of older geology")
            if data["epoch"]==0:
                additions=sum(bool(v) and not initial[y][x]
                    for y,row in enumerate(data["landmark_cells"]) for x,v in enumerate(row))
                if additions:failures.append(f"Ancient-only system acquired {additions} unplanned liquid cells")
        broken_caps=[(y,x) for y,row in enumerate(data.get("landmark_caps",[])) for x,cap in enumerate(row)
                     if cap and data["features"][y][x]!=cap]
        if broken_caps:failures.append(f"Construction or later terrain altered {len(broken_caps)} protected source or shore cells")
        failures.extend("Final map: "+error for error in terminal_geometry({**data,"terrain_after":data["features"]}))
        roles=data.get("landmark_roles",[])
        if roles:
            sources=sum(bool(v&1) for row in roles for v in row)
            outlets=sum(bool(v&2) for row in roles for v in row)
            if (sources,outlets)!=(data["flow"]["sources"],data["flow"]["outlets"]):
                failures.append("Endpoint role cells disagree with source/outlet counts")
    return failures


def write_png(path, pixels, scale=4):
    height,width=len(pixels),len(pixels[0])
    scanlines=b"".join(b"\0"+b"".join(bytes.fromhex(c[1:])*scale for c in row)
                       for row in pixels for _ in range(scale))
    def chunk(kind,content):
        return struct.pack(">I",len(content))+kind+content+struct.pack(">I",zlib.crc32(kind+content))
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",width*scale,height*scale,8,2,0,0,0))
                     +chunk(b"IDAT",zlib.compress(scanlines))+chunk(b"IEND",b""))


def render(data, metrics, out, stem):
    height,width=data["height"],data["width"]
    mask=data.get("landmark_cells",data["new_terrain"])
    bridges=data.get("landmark_bridges",[[0]*width for _ in range(height)])
    repairs=data.get("landmark_repairs",[[0]*width for _ in range(height)])
    rubble=data.get("landmark_rubble",[[0]*width for _ in range(height)])
    pixels=[]
    rectangles=[]
    for y in range(height):
        row=[]
        for x in range(width):
            feat=data["features"][y][x]
            color=COLORS.get(feat,"#604e43" if 32<=feat<=47 else "#303740")
            if bridges[y][x]: color="#f2c550"
            if repairs[y][x]: color="#dfcba2"
            row.append(color)
            rectangles.append(f'<rect x="{x}" y="{y}" width="1" height="1" fill="{color}"/>')
        pixels.append(row)
    py,px=data["player"]
    pixels[py][px]="#ff71c5"
    write_png(out/f"{stem}.png",pixels)
    if "terrain_before" in data:
        before=[[COLORS.get(feat,"#604e43" if 32<=feat<=47 else "#303740")
                 for feat in row] for row in data["terrain_before"]]
        write_png(out/f"{stem}-before.png",before)
    outlines=[]
    for y in range(height):
        for x in range(width):
            pi=data["partition_ids"][y][x]
            if x+1<width and pi!=data["partition_ids"][y][x+1]:
                outlines.append(f'M{x+1},{y}v1')
            if y+1<height and pi!=data["partition_ids"][y+1][x]:
                outlines.append(f'M{x},{y+1}h1')
    outline=f'<path d="{" ".join(outlines)}" fill="none" stroke="#d7b9fc" stroke-opacity=".6" stroke-width=".18"/>'
    bbox=metrics["whole"].get("bbox")
    bounds=""
    if bbox:
        y1,x1,y2,x2=bbox
        bounds=f'<rect x="{x1}" y="{y1}" width="{x2-x1+1}" height="{y2-y1+1}" fill="none" stroke="#fff" stroke-width=".35" stroke-dasharray="1.5 1"/>'
    (out/f"{stem}.svg").write_text(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" shape-rendering="crispEdges">'
        +"".join(rectangles)+outline+bounds+f'<circle cx="{px+.5}" cy="{py+.5}" r="1" fill="#ff71c5"/></svg>',encoding="utf-8")
    # Isolation keeps tiny scattered terrain visually distinguishable from a landmark.
    isolated=[[pixels[y][x] if mask[y][x] or bridges[y][x] or repairs[y][x] or rubble[y][x] else "#18212b"
               for x in range(width)] for y in range(height)]
    write_png(out/f"{stem}-landmark.png",isolated)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input",type=Path,default=DEFAULT)
    parser.add_argument("--baseline",action="store_true")
    parser.add_argument("--depths",nargs="+",type=int)
    parser.add_argument("--seeds",nargs="+",type=int)
    args=parser.parse_args()
    morphology_self_test()
    terminal_self_test()
    out=args.input/"landmark-gallery"
    out.mkdir(parents=True,exist_ok=True)
    reports=[]; cards=[]; errors=[]
    for path in sorted(args.input.glob("depth-*-seed-*.json")):
        data=json.loads(path.read_text())
        if args.depths and data["depth"] not in args.depths: continue
        if args.seeds and data["seed"] not in args.seeds: continue
        map_stem=path.stem
        for data in system_views(data):
            stem=map_stem+(f"-system-{data['system']}" if "system" in data else "")
            metrics=measure(data)
            preview_path=args.input/f"preview-{stem}.json"
            if preview_path.exists():metrics["preview"]=json.loads(preview_path.read_text())
            failures=[] if args.baseline else validate(data,metrics)
            metrics["failures"]=failures;reports.append(metrics)
            errors.extend(f"{stem}: {failure}" for failure in failures)
            render(data,metrics,out,stem)
            shape=metrics["whole"]
            largest=metrics["components"][0] if metrics["components"] else {"area":0,"span_percent":0}
            origin="Forced family fixture" if data.get("fixture") else f"{data['depth']*50} ft"
            if "system" in data:
                history_label=EPOCHS[data["epoch"]]
                if data["epoch"]==2 and not data.get("structures",{}).get("overflow_tiles",0):
                    history_label="Older geology; overflow remained contained"
                origin+=f" · system {data['system']+1} · {history_label}"
            label=(f"{origin} · seed {data['seed']} · {data['height']}×{data['width']} · "
                   f"{data['attempts']} attempts · {data['elapsed_ms']} ms")
            detail=(f"Total area {shape['area']}; whole extent {shape['span_percent']}%; "
                    f"largest connected shape {largest['area']} cells / {largest['span_percent']}%; "
                    f"partitions {shape['partitions']}")
            if data.get("landmark",{}).get("accepted"):
                lm=data["landmark"]
                names=("water","chasm","lava","poison","ice")
                family=FAMILIES[data["network"]["family"]] if data.get("network") else "Legacy landmark"
                detail=(f"{names[lm['material']].capitalize()} · {family} · "
                        f"{lm['bridges']} bridges · {lm['vaults']} vaults · "+detail)
                if data.get("network"):
                    morph=metrics["morphology"]
                    detail+=(f" · measured basin cores {morph['broad_core_areas']}"
                        f" · one-cell channel {morph['one_cell_percent']}% of {morph['channel_samples']} samples"
                        f" · longest thin link {morph['longest_thin_link']}"
                        f" · spine junctions {morph['junction_clusters']}"
                        f" · bridge travel saved {data['network']['bridge_savings']}")
                if data.get("flow"):
                    flow=data["flow"]
                    detail+=(f" · {SCENARIOS[flow['scenario']]}"
                        f" · sources/outlets {flow['sources']}/{flow['outlets']}"
                        f" · edge mouths {flow['edge_mouths']}, springs {flow['springs']}, sinks {flow['sinks']}"
                        f", vents {flow['vents']}, terminal basins {flow['terminal_basins']}")
                if data.get("structures"):
                    structure=data["structures"]
                    detail+=(f" · structures flooded/breached/repaired {structure['flooded']}/{structure['breached']}/{structure['repaired']}"
                        f" · rubble {structure['rubble_tiles']}, rebuilt floor {structure['repair_tiles']}"
                        f" · overflow expansion {structure.get('overflow_tiles',0)}")
            print(f"{stem}: {detail}"+(" FAIL: "+"; ".join(failures) if failures else ""))
            family_id=data.get("network",{}).get("family",-1)
            material_id=data.get("landmark",{}).get("material",-1)
            crop_label="Room scale — production game tiles"
            if metrics.get("preview"):
                preview=metrics["preview"]
                crop_label+=f" · {preview['reason']} at {preview['center']}"
            cards.append(f'<article data-family="{family_id}" data-material="{material_id}"><h2>{html.escape(label)}</h2><p>{html.escape(detail)}</p>'
                +(f'<a href="../{map_stem}-game-tiles.png"><img src="../{map_stem}-game-tiles.png" alt="Production SDL full map tiles"></a>'
                  f'<details open><summary>{html.escape(crop_label)}</summary><img src="../{stem}-room-tiles.png"></details>'
                  if (args.input/f"{map_stem}-game-tiles.png").exists() else "")
                +''.join(f'<details open><summary>{caption} — production game tiles</summary>'
                    f'<img src="../{stem}-{suffix}.png"></details>'
                    for suffix,caption in (("terminal-tiles","Source or outlet"),("structure-tiles","Flood, breach or repair"))
                    if (args.input/f"{stem}-{suffix}.png").exists())
                +'<details><summary>Geometry schematic and partition boundaries</summary>'
                f'<a href="{stem}.svg"><img src="{stem}.svg" alt="Full dungeon schematic"></a>'
                '</details>'
                f'<details><summary>Isolate network and structural effects</summary><img src="{stem}-landmark.png"></details>'
                +(f'<details><summary>Before terrain placement</summary><img src="{stem}-before.png"></details>'
                  if "terrain_before" in data else "")
                +("<p class=fail>"+html.escape("; ".join(failures))+"</p>" if failures else "")+"</article>")
    assert reports,"No exported maps found"
    if not args.baseline and len(reports)>=5:
        accepted=sum(bool(r["landmark"] and r["landmark"]["accepted"]) for r in reports)
        if accepted*2<len(reports):
            errors.append(f"Only {accepted}/{len(reports)} sampled levels contain landmarks; needs at least half")
    (out/"metrics.json").write_text(json.dumps(reports,indent=2)+"\n",encoding="utf-8")
    title="Baseline: previous terrain geometry" if args.baseline else "Underground networks — production maps"
    family_options=''.join(f'<option value="{i}">{name}</option>' for i,name in enumerate(FAMILIES))
    material_options=''.join(f'<option value="{i}">{name}</option>' for i,name in enumerate(("Water","Chasm","Lava","Poison","Ice")))
    (out/"index.html").write_text(f'<!doctype html><meta charset="utf-8"><title>{title}</title>'
        '<style>body{margin:24px;background:#101720;color:#e2e8ef;font:15px system-ui}main{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:24px}article{background:#202a35;padding:16px}h2{font-size:16px}img{width:100%;image-rendering:pixelated}a{color:#a8d5ff}.fail{color:#ff998c}select{margin:0 24px 24px 8px;padding:8px;background:#202a35;color:inherit}summary{cursor:pointer;padding:8px 0}@media(max-width:900px){main{grid-template-columns:1fr}}</style>'
        f'<h1>{title}</h1><p>Full maps and room crops use the production SDL layered renderer and shipped game artwork where available, '
        'with terrain revealed and diagnostic lighting; this is not a manual gameplay test. Forced family fixtures are synthetic room layouts '
        'passed through the production terrain generator; ordinary seeds are full generate_cave output after population. '
        'Each historical system is audited separately; its before image is after construction and before its final incident. '
        'Expandable schematics show geometry only. '
        'Purple lines: partition boundaries. White dashed box: new terrain extent. Gold: architectural bridges. Pink: player. '
        'Water blue; lava orange; poison green; ice cyan; chasm black. Click maps for scalable SVG.</p>'
        f'<label>Family<select id="family"><option value="all">All families</option>{family_options}</select></label>'
        f'<label>Material<select id="material"><option value="all">All materials</option>{material_options}</select></label>'
        '<main>'+"".join(cards)+'</main><script>const selects=[...document.querySelectorAll("select")];'
        'function filter(){document.querySelectorAll("article").forEach(card=>{card.hidden=selects.some(s=>s.value!=="all"&&card.dataset[s.id]!==s.value)})}'
        'selects.forEach(s=>s.addEventListener("change",filter));</script>',encoding="utf-8")
    print(f"Gallery: {out/'index.html'}")
    if errors:
        raise SystemExit("Landmark acceptance failed:\n"+"\n".join(errors))


if __name__=="__main__":
    main()
