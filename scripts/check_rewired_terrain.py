#!/usr/bin/env python3
"""Exercise trap rewiring, Song of Freedom terrain changes, and saved map state."""
import check_monster_scent_save as engine


CHECKS = r'''
#include "player/player-song-internal.h"
#include "spell/spell-projection-internal.h"

static void check_replaced_trap(void)
{
    fresh_map();
    int y = p_ptr->py, x = p_ptr->px + 1;
    character_generated = character_dungeon = true;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    cave_set_feat(y, x, FEAT_TRAP_DART);
    cave_info[y][x] |= CAVE_MARK | CAVE_SEEN;
    p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS] = true;
    p_ptr->skill_use[S_PER] = 100;
    do_cmd_disarm_aux(y, x);
    int quality = cave_rewired[y][x];
    assert(quality > 0);

    cave_set_feat(y, x, FEAT_TRAP_DART);
    assert(cave_rewired[y][x] == quality);
    hit_trap(y, x);
    assert(p_ptr->chp == 100);

    earthquake(y, x, y, x, 1, -1);
    assert(cave_feat[y][x] == FEAT_TRAP_PIT);
    monster_swap(p_ptr->py, p_ptr->px, y, x);
    int hp = p_ptr->chp;
    hit_trap(y, x);
    assert(p_ptr->chp < hp && !cave_rewired[y][x]);
    puts("A rewired dart trap spares the player, but its replacement earthquake pit deals damage.");
}

static void check_freedom_disarm(void)
{
    fresh_map();
    character_generated = character_dungeon = true;
    int y = p_ptr->py, x = p_ptr->px + 1;
    cave_environment_seed();
    cave_set_feat(y, x, FEAT_TRAP_CALTROPS);
    cave_rewired[y][x] = 37;
    cave_set_feat(y + 1, x, FEAT_TRAP_DART);
    cave_info[y + 1][x] |= CAVE_HIDDEN;
    for (int row = y; row <= y + 1; row++) {
        cave_info[row][x] |= CAVE_MARK | CAVE_SEEN;
        note_spot(row, x);
        cave_info[row][x] &= ~CAVE_SEEN;
    }
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    sing_song_of_freedom(200);
    assert(cave_feat[y][x] == FEAT_FLOOR && !cave_rewired[y][x]);
    assert(cave_feat[y + 1][x] == FEAT_FLOOR && !(cave_info[y + 1][x] & CAVE_HIDDEN));
    assert(cave_environment_known_feature(y + 1, x) == FEAT_FLOOR);
    assert(cave_environment_known_feature(y, x) == FEAT_TRAP_CALTROPS);
    puts("Freedom removes trap wiring and concealment along with the mechanism.");
}

static void check_web_on_chest(void)
{
    fresh_map();
    character_generated = character_dungeon = true;
    int y = p_ptr->py, x = p_ptr->px;
    cave_m_idx[y][x] = -1;
    object_type chest;
    object_prep(&chest, lookup_kind(TV_CHEST, SV_CHEST_SMALL_WOODEN));
    chest.pval = 3;
    int idx = floor_carry(y, x, &chest);
    assert(idx > 0 && place_monster_one(y, x + 2, 71, false, true, NULL));
    int spider = cave_m_idx[y][x + 2];
    p_ptr->skill_use[S_EVN] = -100;
    assert(project_p(spider, y, x, 0, 0, -1, GF_WEB));
    assert(cave_feat[y][x] == FEAT_TRAP_WEB && cave_o_idx[y][x] == idx);
    /* Older floor piles can contain another chest below the first. */
    int buried = o_pop();
    assert(buried > 0);
    object_copy(&o_list[buried], &o_list[idx]);
    o_list[buried].pval = 7;
    o_list[idx].next_o_idx = buried;
    update_flow(y, x, FLOW_PLAYER_NOISE);
    sing_song_of_freedom(200);
    assert(o_list[idx].pval == -3 && object_known_p(&o_list[idx]));
    assert(o_list[buried].pval == -7 && object_known_p(&o_list[buried]));
    assert(cave_feat[y][x] == FEAT_FLOOR);
    puts("Freedom unlocks the chest and clears an Attercop web on the same square.");
}

static void check_saved_traps(void)
{
    fresh_map();
    cave_set_feat(7, 7, FEAT_TRAP_CALTROPS);
    cave_set_feat(7, 8, FEAT_TRAP_PIT);
    cave_set_feat(7, 9, FEAT_TRAP_DART);
    cave_set_feat(7, 11, FEAT_CHASM);
    cave_info[7][9] |= CAVE_HIDDEN;
    /* Earlier games could leave wiring on a pit or on empty floor. */
    cave_rewired[7][7] = 33;
    cave_rewired[7][8] = cave_rewired[7][10] = cave_rewired[7][11] = 22;
    size_t dungeon_size, consumed;
    size_t length = fixture_write_dungeon(encoded, sizeof(encoded), &dungeon_size);
    u32b sentinel;
    fresh_map();
    assert(fixture_read_current_dungeon(encoded, length, &sentinel, &consumed) == 0);
    assert(sentinel == 0xA1B2C3D4U && consumed == length);
    assert(cave_rewired[7][7] == 33 && (cave_info[7][9] & CAVE_HIDDEN));
    assert(!cave_rewired[7][8] && !cave_rewired[7][10] && !cave_rewired[7][11]);
    puts("Reload restores valid trap wiring and concealment and drops obsolete wiring.");
}

static void check_freedom_doors(void)
{
    fresh_map();
    character_generated = character_dungeon = true;
    cave_environment_seed();
    int y = p_ptr->py, x = p_ptr->px + 1;
    for (int i = 0; i < 2; ++i) {
        cave_info[y + i][x] |= CAVE_MARK | CAVE_SEEN;
        cave_set_feat(y + i, x, FEAT_DOOR_HEAD + (i ? 10 : 3));
    }
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    sing_song_of_freedom(200);
    for (int i = 0; i < 2; ++i) {
        assert(cave_feat[y + i][x] == FEAT_DOOR_HEAD);
        cave_info[y + i][x] &= ~CAVE_SEEN;
        assert(cave_environment_known_feature(y + i, x) == FEAT_DOOR_HEAD);
    }
    puts("Doors visibly unlocked or unjammed by Freedom stay that way in map memory.");
}
'''

if __name__ == "__main__":
    engine.OUT = engine.ROOT / "scripts/output/rewired-terrain"
    engine.main(CHECKS, "    check_web_on_chest(); check_freedom_doors(); check_replaced_trap(); check_freedom_disarm(); check_saved_traps();")
