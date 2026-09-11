#!/usr/bin/env python3
"""Exercise current-world tutorial observations with production core/world code."""
import json
import os
from pathlib import Path
import subprocess

import check_gameplay_tutorial as core

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/tutorial-world-check"

HARNESS = core.HARNESS.split("int main(", 1)[0] + r'''
__WORLD_IMPLEMENTATION__
static player_type test_player;
player_type *p_ptr=&test_player;
static object_type test_inventory[INVEN_TOTAL];
object_type *inventory=test_inventory;
static monster_type test_monsters[3];
monster_type *mon_list=test_monsters;
static monster_race test_races[3];
monster_race *r_info=test_races;
static monster_lore test_lore[3];
monster_lore *l_list=test_lore;
s16b mon_max=2;
bool character_generated=true;
static u16b test_info[12][256];
u16b (*cave_info)[256]=test_info;
static int fuel=100, carried_silmarils, pack_used, total_limit=100;
static level_partition_kind region;
static big_cave_type_t cave_type;
static int8_t test_curses[METAR_CURSE_SLOTS];
static u64b seen_curses;
int8_t *active_curse_stacks(void) { return test_curses; }
u64b *active_curses_seen_ptr(void) { return &seen_curses; }
bool death_spectator_active(void) { return false; }
int player_carried_extra_entry_count(void) { return 0; }
object_type *player_carried_extra_entry_at(int index) { (void)index;return NULL; }
int inventory_limit_usage_for_group(enum inventory_limit_group group)
{ return group==INV_LIMIT_PACK?pack_used:0; }
int inventory_limit_limit_for_group(enum inventory_limit_group group)
{ (void)group;return total_limit; }
int player_quiver_arrow_count(void) { return 0; }
int player_quiver_arrow_space(void) { return 99; }
int weight_limit(void) { return 100; }
int player_light_fuel(const object_type *item) { (void)item;return fuel; }
int player_light_sputter_threshold(const object_type *item) { (void)item;return 20; }
level_partition_kind level_partition_kind_for_point(int y,int x)
{ (void)y;(void)x;return region; }
big_cave_type_t level_partition_big_cave_type_for_point(int y,int x)
{ (void)y;(void)x;return cave_type; }
int silmarils_possessed(void) { return carried_silmarils; }
void monster_desc(char *buf,size_t size,const monster_type *m,int mode)
{ (void)mode;SDL_snprintf(buf,size,"Monster %d",m->r_idx); }

static bool pending(const char *id)
{
    int index=lesson_index(id);
    if (index>=0 && active==index) return true;
    for(int i=0;i<queue_count;++i) if(queue[i].lesson==index) return true;
    return false;
}

int main(void)
{
    assert(SDL_Init(0));
    test_tale.id=8100; tutorial_sync_tale();
    assert(tutorial_load_catalogue("catalogue.json"));
    p_ptr->playing=true; p_ptr->py=p_ptr->px=5;
    inventory[INVEN_LITE]=(object_type){.k_idx=1,.tval=TV_LIGHT,.sval=SV_LIGHT_TORCH};
    fuel=10;
    tutorial_world_start(); tutorial_world_checkpoint();
    assert(pending("world.light_low")); /* Current danger also matters after load. */
    fuel=0; tutorial_world_checkpoint();
    assert(!pending("world.light_low") && pending("world.light_out"));
    fuel=100; tutorial_world_checkpoint();
    assert(!pending("world.light_low") && !pending("world.light_out"));
    p_ptr->total_weight=200; tutorial_world_checkpoint(); assert(pending("storage.weight"));
    p_ptr->total_weight=0; tutorial_world_checkpoint(); assert(!pending("storage.weight"));

    /* Lore must be recorded AND true AND currently visible. A sensed monster
     * outside line of sight cannot retain a card claiming it is visible. */
    mon_list[1]=(monster_type){.r_idx=1,.ml=true,.fy=5,.fx=6};
    r_info[1].flags2=RF2_FLYING;
    cave_info[5][6]=CAVE_VIEW;
    tutorial_world_checkpoint(); assert(!pending("monster.rf2_flying"));
    l_list[1].flags2=RF2_FLYING;
    tutorial_world_checkpoint(); assert(pending("monster.rf2_flying"));
    cave_info[5][6]=0;
    tutorial_world_checkpoint(); assert(!pending("monster.rf2_flying"));
    cave_info[5][6]=CAVE_VIEW; r_info[1].flags2=0;
    tutorial_world_checkpoint(); assert(!pending("monster.rf2_flying"));
    r_info[1].flags2=RF2_FLYING; p_ptr->image=1;
    tutorial_world_checkpoint(); assert(!pending("monster.rf2_flying")); p_ptr->image=0;

    /* An earlier Extended-only trait must not starve a later Normal trait. */
    r_info[1].flags1=l_list[1].flags1=RF1_UNIQUE;
    tutorial_set_mode(TUTORIAL_MODE_NORMAL);
    tutorial_world_checkpoint();
    assert(!pending("monster.rf1_unique") && pending("monster.rf2_flying"));
    tutorial_set_mode(TUTORIAL_MODE_EXTENDED);
    tutorial_world_checkpoint(); assert(pending("monster.rf1_unique"));
    p_ptr->blind=1; tutorial_world_checkpoint();
    assert(!pending("monster.rf1_unique") && !pending("monster.rf2_flying"));
    p_ptr->blind=0;

    /* Regions are present state, and moving away expires the offer. */
    cave_type=BIG_CAVE_FIRE; tutorial_world_checkpoint(); assert(pending("world.partition.fire"));
    cave_type=BIG_CAVE_ICE; tutorial_world_checkpoint();
    assert(!pending("world.partition.fire") && pending("world.partition.cold"));
    cave_type=0; tutorial_world_checkpoint(); assert(!pending("world.partition.cold"));

    /* Giver placement is secret until encountered; generation alone does not
     * announce a quest. Acceptance is a real public transition. */
    p_ptr->mandos_quest=MANDOS_QUEST_GIVER_PRESENT;
    tutorial_world_checkpoint(); assert(!pending("quest.3"));
    p_ptr->mandos_quest=MANDOS_QUEST_ACTIVE;
    tutorial_world_checkpoint(); assert(pending("quest.3") && pending("quest.progress"));
    tutorial_invalidate_context();
    p_ptr->tutorial_deferred=true;
    tutorial_world_checkpoint(); assert(queue_count==0);
    tutorial_shutdown(); SDL_Quit();
    puts("Tutorial world: current fuel/weight, live known lore and expiry, Normal trait filtering, regions, public quest transitions, legacy deferral: PASS");
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for path in OUT.glob("tale-*-tutorials.json"):
        path.unlink()
    ids = ["world.light_low", "world.light_out", "storage.weight", "monster.rf1_unique",
           "monster.rf2_flying", "world.partition.fire", "world.partition.cold",
           "world.partition.poison", "quest.3", "quest.progress"]
    lessons = [{"id": name, "title": name,
                "level": "extended" if name == "monster.rf1_unique" else "normal",
                "steps": [{"text": "{subject}: {detail}"}]} for name in ids]
    (OUT / "catalogue.json").write_text(json.dumps({"version": 1, "lessons": lessons}), encoding="utf-8")
    world = (ROOT / "src/tutorial/tutorial-world.c").read_text(encoding="utf-8-sig")
    world = world.replace('#include "externs.h"', '')
    world = world.replace('#include "tutorial-world.h"', '#include "tutorial/tutorial-world.h"')
    world = world.replace('#include "tutorial.h"', '#include "tutorial/tutorial.h"')
    source = OUT / "check.c"
    source.write_text(HARNESS.replace("__WORLD_IMPLEMENTATION__", world), encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join(["C:/msys64/mingw64/bin", "C:/msys64/usr/bin",
                                   str(BUILD / "_deps/SDL"), env["PATH"]])
    exe = OUT / "check.exe"
    subprocess.run(["C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17",
                    "-Wall", "-Wextra", "-Wno-unused-function", "-O2", "-fwhole-program",
                    "-ffunction-sections", "-fdata-sections",
                    "@CMakeFiles/sil-more.dir/includes_C.rsp", str(source),
                    str(ROOT / "src/cJSON.c"), "_deps/SDL/libSDL3.dll.a", "-Wl,--gc-sections",
                    "-o", str(exe)], cwd=BUILD, env=env, check=True)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True, timeout=30)


if __name__ == "__main__":
    main()
