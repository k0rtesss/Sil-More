#!/usr/bin/env python3
"""Check lazy pathfinding through the production scheduler in isolated data.

Build standard first. Uses the existing initialized-template fixture; never
opens player saves. Count searches rather than asserting machine-specific time.
"""
import os
import check_new_monsters as engine

CHECKS = r'''
#include "monster/monster-senses.h"
static int full_flows, pursuit_flows;
void __real_update_flow(int y, int x, int idx);
void __real_update_pursuit_flow(int y, int x, int idx, bool allow_player);
void __wrap_update_flow(int y, int x, int idx) {
    if (idx < MAX_MONSTERS) full_flows++;
    __real_update_flow(y, x, idx);
}
void __wrap_update_pursuit_flow(int y, int x, int idx, bool allow_player) {
    pursuit_flows++;
    __real_update_pursuit_flow(y, x, idx, allow_player);
}
static monster_type* flow_work_fixture(void) {
    monster_type* m = combat_fixture(403, 0);
    r_info[403].flags1 = r_info[403].flags2 = r_info[403].flags3 = 0;
    r_info[403].flags4 = r_info[403].flags5 = 0;
    r_info[403].freq_ranged = r_info[403].light = 0;
    m->energy = 100; m->ml = false;
    m->min_range = m->best_range = 1;
    m->stance = STANCE_AGGRESSIVE;
    m->wandering_idx = m->target_y = m->target_x = 0;
    memset(&m->ai, 0, sizeof(m->ai));
    p_ptr->py = p_ptr->px = 30;
    cave_m_idx[30][30] = -1;
    m->cdis = distance(m->fy, m->fx, 30, 30);
    visual_recognition = false;
    use_sound = false;
    full_flows = pursuit_flows = 0;
    return m;
}
static void check_flow_work(void) {
    monster_race saved = r_info[403];
    monster_type* m = flow_work_fixture();
    monster_senses_hear(m, 10, 18);
    process_monsters(100);
    assert(full_flows == 0 && pursuit_flows == 1);
    assert(m->fx > 10 && m->energy < 100);
    puts("Sound-evidence pursuit: one bounded search, no eager duplicate PASS.");

    m = flow_work_fixture();
    r_info[403].flags1 |= RF1_NEVER_MOVE;
    cave_m_idx[30][30] = 0; p_ptr->py = 10; p_ptr->px = 18;
    cave_m_idx[10][18] = -1;
    m->cdis = 8;
    process_monsters(100);
    assert(full_flows == 0 && pursuit_flows == 0);
    assert(m->fy == 10 && m->fx == 10 && m->energy < 100);
    puts("Stationary monster: no unused full-map pursuit search PASS.");

    m = flow_work_fixture();
    monster_type model = *m;
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 198;
    cave_m_idx[30][30] = cave_m_idx[10][10] = 0;
    p_ptr->py = p_ptr->px = 190; cave_m_idx[190][190] = -1;
    for (int y = 0; y < 198; y++) for (int x = 0; x < 198; x++)
        if (!y || !x || y == 197 || x == 197) {
            cave_feat[y][x] = FEAT_WALL_PERM; cave_info[y][x] = CAVE_WALL;
        }
    mon_max = 601; mon_cnt = 600;
    for (int i = 1; i <= 600; i++) {
        mon_list[i] = model;
        mon_list[i].fy = 3 + (i - 1) / 30 * 3;
        mon_list[i].fx = 3 + (i - 1) % 30 * 3;
        mon_list[i].cdis = distance(mon_list[i].fy, mon_list[i].fx, 190, 190);
        cave_m_idx[mon_list[i].fy][mon_list[i].fx] = i;
    }
    Uint64 start = SDL_GetTicksNS();
    process_monsters(100);
    double ms = (SDL_GetTicksNS() - start) / 1000000.0;
    assert(full_flows == 0 && pursuit_flows == 0);
    for (int i = 1; i <= 600; i++) assert(mon_list[i].energy < 100);
    printf("600 alert idle monsters on 198x198: %.2f ms, zero unused pursuit searches PASS.\n", ms);
    r_info[403] = saved;
}
'''


def main():
    engine.OUT = engine.ROOT / "scripts/output/monster-flow-work"
    engine.HARNESS = engine.HARNESS.replace(
        "int main(int argc,char** argv)", CHECKS + "\nint main(int argc,char** argv)")
    engine.HARNESS = engine.HARNESS.replace(
        "    check_combat();", "    check_combat();\n    check_flow_work();")
    original_run = engine.subprocess.run

    def wrapped_run(args, **kw):
        if args[0].endswith("cc.exe"):
            args = [*args, "-Wl,--wrap=update_flow", "-Wl,--wrap=update_pursuit_flow"]
        if "env" in kw:
            kw["env"] = dict(kw["env"])
            # Prefer the DLLs matching the engine objects to installed SDL DLLs.
            kw["env"]["PATH"] = os.pathsep.join([
                *(str(engine.BUILD / "_deps" / name)
                  for name in ("SDL", "SDL_ttf", "SDL_image", "SDL_mixer")),
                kw["env"]["PATH"]])
        return original_run(args, **kw)

    engine.subprocess.run = wrapped_run
    try:
        engine.main()
    finally:
        engine.subprocess.run = original_run


if __name__ == "__main__":
    main()
