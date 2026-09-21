#!/usr/bin/env python3
"""Real-engine territory, patrol topology, movement and persistence regressions."""
import check_monster_scent_save as persistence

TESTS = r'''
#include "monster/monster-routine.h"
#include "melee/melee-movement.h"
#include "melee/melee-process.h"
#include "melee/melee-util.h"
#include "level-generation/level-generation-internal.h"

static int routine_race;
static void routine_map(int kind)
{
    fresh_map();
    partition_meta_save meta = {0};
    meta.grid_rows = 1; meta.grid_cols = 2; meta.partition_count = 2;
    meta.modes[0] = kind; meta.modes[1] = QUAD_MODE_ROOMY;
    level_partition_meta_set(&meta);
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
}
static monster_type* routine_place(int y, int x)
{
    assert(place_monster_one(y, x, routine_race, false, true, NULL));
    return &mon_list[cave_m_idx[y][x]];
}
static void routine_island(void)
{
    /* A vault-like island in a connected passage network. */
    for (int y = 9; y <= 13; y++) for (int x = 6; x <= 10; x++)
    { cave_feat[y][x] = FEAT_WALL_EXTRA; cave_info[y][x] = CAVE_WALL; }
}
static void test_routine_territory(void)
{
    for (int kind = QUAD_MODE_CHASM; kind <= QUAD_MODE_BIG_CAVE; kind++)
    {
        routine_map(kind);
        monster_type* m = routine_place(12, 5);
        assert(m->routine.territory == 1 && !m->routine.style);
        int edge = 1;
        while (level_partition_index_for_point(12, edge) == 0) edge++;
        monster_swap(m->fy, m->fx, 12, edge - 1);
        bool bash = false;
        assert(!monster_routine_allows(m, 12, edge));
        assert(!cave_passable_mon(m, 12, edge, &bash));
        process_move(m, 12, edge, false);
        assert(m->fx == edge - 1);
        /* The other member of the same race gets its own birth behavior. */
        monster_type* other = routine_place(15, edge + 2);
        assert(!other->routine.territory);
        assert(!(r_info[m->r_idx].flags2 & RF2_TERRITORIAL));
        for (int turn = 0; turn < 300; turn++)
        {
            int y, x;
            if (get_move_wander(m, &y, &x)) process_move(m, y, x, false);
            assert(level_partition_index_for_point(m->fy, m->fx) == 0);
        }
        /* Displacement is possible, but the birth territory remains stable. */
        monster_swap(m->fy, m->fx, 12, edge + 1);
        for (int turn = 0; turn < 30 && level_partition_index_for_point(m->fy, m->fx); turn++)
        {
            int y, x;
            if (monster_routine_move(m, &y, &x)) process_move(m, y, x, false);
        }
        assert(level_partition_index_for_point(m->fy, m->fx) == 0);
        int my = m->fy, mx = m->fx;
        monster_routine_state saved = m->routine;
        size_t size, bytes = fixture_write_dungeon(encoded, sizeof(encoded), &size);
        size_t consumed; u32b sentinel;
        routine_map(QUAD_MODE_ROOMY);
        assert(!fixture_read_dungeon(encoded, bytes, VERSION_EXTRA, &sentinel, &consumed));
        m = &mon_list[cave_m_idx[my][mx]];
        assert(!memcmp(&saved, &m->routine, sizeof(saved)));
        assert(level_partition_index_for_point(m->routine.home_y, m->routine.home_x) == 0);
    }
    puts("Birth partition, individual assignment, hard movement boundary and displaced return PASS.");
}
static void test_routine_patrol(void)
{
    routine_map(QUAD_MODE_ROOMY);
    monster_type* m = routine_place(12, 4);
    assert(!monster_routine_plan(m)); /* Empty rooms are not circuits. */
    routine_island();
    assert(monster_routine_plan(m));
    assert(monster_routine_valid(m));
    monster_routine_state original = m->routine;
    int length = m->routine.count;
    /* Start on a waypoint so every action follows the exact closed circuit. */
    monster_swap(m->fy, m->fx, m->routine.y[0], m->routine.x[0]);
    m->routine.next = 0;
    for (int lap = 0; lap < 3; lap++)
    {
        for (int i = 1; i <= length; i++)
        {
            int y, x;
            assert(get_move_wander(m, &y, &x));
            process_move(m, y, x, false);
            assert(m->fy == original.y[i % length] && m->fx == original.x[i % length]);
        }
    }
    /* A temporary occupant waits without altering the route. */
    int blocked = (m->routine.next + 1) % length;
    monster_type* blocker = routine_place(original.y[blocked], original.x[blocked]);
    int y, x;
    assert(!get_move_wander(m, &y, &x));
    delete_monster_idx(cave_m_idx[blocker->fy][blocker->fx]);
    assert(get_move_wander(m, &y, &x));
    process_move(m, y, x, false);
    /* Pursuit/displacement keeps the circuit, and idle behavior rejoins it. */
    monster_swap(m->fy, m->fx, 16, 18);
    for (int i = 0; i < 60; i++)
        if (get_move_wander(m, &y, &x)) process_move(m, y, x, false);
    assert(m->routine.style == MON_ROUTINE_PATROL);
    assert(!memcmp(original.y, m->routine.y, length));
    assert(!memcmp(original.x, m->routine.x, length));
    bool on_route = false;
    for (int i = 0; i < length; i++)
        if (m->fy == original.y[i] && m->fx == original.x[i]) on_route = true;
    assert(on_route);
    int next = m->routine.next;
    if (m->fy == m->routine.y[next] && m->fx == m->routine.x[next])
        next = (next + 1) % length;
    int closed_y = m->routine.y[next], closed_x = m->routine.x[next];
    cave_feat[closed_y][closed_x] = FEAT_WALL_EXTRA;
    cave_info[closed_y][closed_x] = CAVE_WALL;
    assert(!get_move_wander(m, &y, &x));
    assert(!m->routine.style && !m->routine.count);
    cave_feat[closed_y][closed_x] = FEAT_FLOOR;
    cave_info[closed_y][closed_x] = 0;
    assert(monster_routine_plan(m));
    puts("Obstacle circuit, three exact laps, occupant waiting and route rejoining PASS.");

    monster_routine_state saved = m->routine;
    int my = m->fy, mx = m->fx;
    size_t size, length_bytes = fixture_write_dungeon(encoded, sizeof(encoded), &size);
    u32b sentinel; size_t consumed;
    routine_map(QUAD_MODE_ROOMY);
    assert(!fixture_read_dungeon(encoded, length_bytes, VERSION_EXTRA, &sentinel, &consumed));
    assert(consumed == length_bytes && sentinel == 0xA1B2C3D4U);
    m = &mon_list[cave_m_idx[my][mx]];
    assert(!memcmp(&saved, &m->routine, sizeof(saved)));
    /* Every truncation, impossible route count and broken closure is rejected. */
    size_t block = size - 4 - 6 - 2 * saved.count;
    decode(encoded, plain, length_bytes);
    assert(plain[block] == (MON_ROUTINE_SAVE_MAGIC & 255));
    for (size_t cut = block; cut < size; cut++)
    {
        routine_map(QUAD_MODE_ROOMY);
        assert(fixture_read_dungeon(encoded, cut, VERSION_EXTRA, &sentinel, &consumed));
    }
    size_t bad[] = {block, block + 2, block + 8, block + 10};
    for (int i = 0; i < 4; i++)
    {
        byte old = plain[bad[i]]; plain[bad[i]] = 255;
        encode(plain, modified, length_bytes); routine_map(QUAD_MODE_ROOMY);
        assert(fixture_read_dungeon(modified, length_bytes, VERSION_EXTRA, &sentinel, &consumed));
        plain[bad[i]] = old;
    }
    /* Version 20 retains all previous state, with no inferred birth location. */
    memmove(plain + block, plain + size, 8);
    encode(plain, modified, block + 8); routine_map(QUAD_MODE_ROOMY);
    assert(!fixture_read_dungeon(modified, block + 8, 20, &sentinel, &consumed));
    m = &mon_list[cave_m_idx[my][mx]];
    assert(!m->routine.territory && !m->routine.style && !m->routine.count);
    puts("Patrol route/progress roundtrip, truncated/corrupt saves and v20 compatibility PASS.");
}
static void test_routine_assignment(void)
{
    int selected = 0, ordinary = 0, live = 0;
    Rand_state_init(8192);
    for (int i = 0; i < 40; i++)
    {
        routine_map(QUAD_MODE_ROOMY);
        routine_island();
        monster_type* m = routine_place(12, 4);
        if (m->routine.style == MON_ROUTINE_PENDING)
        {
            selected++;
            monster_routine_finish_level();
            assert(m->routine.style == MON_ROUTINE_PATROL && monster_routine_valid(m));
        }
        else ordinary++;
        /* Live spawns plan immediately, without waiting for another level. */
        character_dungeon = true;
        m = routine_place(14, 4);
        assert(m->routine.style != MON_ROUTINE_PENDING);
        if (m->routine.style == MON_ROUTINE_PATROL) live++;
    }
    assert(selected && ordinary && live);
    routine_map(QUAD_MODE_ROOMY);
    /* A dead-end corridor has no closed patrol circuit. */
    for (int y = 1; y < 19; y++) for (int x = 1; x < 23; x++)
        if (y != 12) { cave_feat[y][x] = FEAT_WALL_EXTRA; cave_info[y][x] = CAVE_WALL; }
    monster_type* m = routine_place(12, 4);
    assert(!monster_routine_plan(m));
    puts("Individual spawn selection, final-map/live route planning and no-loop fallback PASS.");
}
static void test_monster_routines(void)
{
    for (int i = 1; i < z_info->r_max; i++)
        if ((r_info[i].flags3 & RF3_ORC) && r_info[i].blow[0].dd
            && !(r_info[i].flags1 & (RF1_UNIQUE | RF1_NEVER_MOVE | RF1_PEACEFUL))
            && !(r_info[i].flags2 & RF2_TERRITORIAL)) { routine_race = i; break; }
    assert(routine_race);
    test_routine_territory();
    test_routine_patrol();
    test_routine_assignment();
}
'''

if __name__ == "__main__":
    persistence.OUT = persistence.ROOT / "scripts/output/monster-routines"
    persistence.main(TESTS, "    test_monster_routines();")
