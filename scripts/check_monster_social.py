#!/usr/bin/env python3
"""Exercise monster diplomacy/combat and actual dungeon save/load in isolation.

Run after build-cmake.bat standard. Reuses the real-engine persistence fixture;
all templates, logs, and saves stay in scripts/output and temporary directories.
"""
import check_monster_scent_save as persistence

TESTS = r'''
#include "monster/monster-social.h"
#include "cave/cave-events.h"
#include "melee/melee-process.h"
#include "melee/melee-attack.h"

static int social_orc;

static void social_fight(monster_type* a, monster_type* b)
{
    assert(monster_social_start_feud(cave_m_idx[a->fy][a->fx], cave_m_idx[b->fy][b->fx]));
    a->social_state = b->social_state = MON_SOCIAL_FIGHT;
    a->social_timer = b->social_timer = MON_SOCIAL_DISPUTE_ACTIONS;
}

static monster_type* social_place(int y, int x, int race)
{
    assert(place_monster_one(y, x, race, false, true, NULL));
    monster_type* m = &mon_list[cave_m_idx[y][x]];
    m->alertness = ALERTNESS_UNWARY;
    m->stance = STANCE_CONFIDENT;
    m->skip_next_turn = m->skip_this_turn = false;
    m->hp = m->maxhp = 1000;
    memset(&m->ai.sense, 0, sizeof(m->ai.sense));
    return m;
}

static void social_map(void)
{
    fresh_map();
    monster_social_reset();
    /* The player is behind a wall, out of the social encounter. */
    for (int x = 1; x < 23; x++) {
        cave_feat[7][x] = FEAT_WALL_EXTRA;
        cave_info[7][x] = CAVE_WALL;
    }
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    Rand_state_init(12345);
}

static void test_social_relationships(void)
{
    social_map();
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    monster_type* c = social_place(13, 10, social_orc);
    int ai = cave_m_idx[12][10], bi = cave_m_idx[12][11];
    assert(monster_social_group(a) == MON_GROUP_ORC);
    assert(monster_social_relation(a, b) == MON_REL_NEUTRAL);
    assert(monster_social_start_feud(ai, bi));
    assert(a->social_state == MON_SOCIAL_CHALLENGE && b->social_state == MON_SOCIAL_CHALLENGE);
    assert(monster_social_relation(a, b) == MON_REL_HOSTILE);
    assert(monster_social_relation(b, a) == MON_REL_HOSTILE);
    assert(monster_social_relation(a, c) == MON_REL_NEUTRAL);
    assert(!monster_social_start_feud(ai, ai));
    assert(!monster_social_start_feud(ai, mon_max));
    assert(!monster_social_start_feud(ai, cave_m_idx[13][10]));
    for (int i = 0; i < 50; i++) monster_social_tick(a);
    assert(a->social_memory == MON_SOCIAL_MEMORY);
    monster_swap(b->fy, b->fx, 1, 20);
    for (int i = 0; i < MON_SOCIAL_MEMORY; i++) monster_social_tick(a);
    assert(!a->social_rival && !b->social_rival);
    assert(a->social_cooldown == MON_SOCIAL_COOLDOWN);
    assert(monster_social_relation(a, b) == MON_REL_NEUTRAL);

    assert(monster_social_set_group(a, 8));
    assert(monster_social_set_group(b, 9));
    assert(monster_social_set_group(c, 8));
    assert(monster_group_set_relation(8, 9, MON_REL_HOSTILE));
    assert(monster_social_relation(c, b) == MON_REL_HOSTILE);
    assert(monster_social_relation(a, c) == MON_REL_NEUTRAL);
    /* Newly joined members use existing group diplomacy immediately. */
    monster_type* d = social_place(14, 10, social_orc);
    assert(monster_social_set_group(d, 9));
    assert(monster_social_relation(d, c) == MON_REL_HOSTILE);
    assert(monster_social_start_feud(ai, bi));
    assert(monster_group_set_relation(8, 9, MON_REL_NEUTRAL));
    assert(!a->social_rival && !b->social_rival);
    assert(monster_social_relation(a, b) == MON_REL_NEUTRAL);
    assert(!monster_group_set_relation(8, 8, MON_REL_HOSTILE));
    assert(!monster_group_set_relation(0, 9, MON_REL_HOSTILE));
    assert(!monster_group_set_relation(8, 16, MON_REL_HOSTILE));
    assert(!monster_group_set_relation(8, 9, 2));
    assert(!monster_social_set_group(a, 16));
    puts("Neutral defaults, mutual local feuds, expiry, group transitions and new members PASS.");
}

static void test_social_combat(bool territorial)
{
    social_map();
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    int ai = cave_m_idx[12][10], bi = cave_m_idx[12][11];
    social_fight(a, b);
    int hp = b->hp;
    for (int n = 0; n < 100 && b->hp == hp; n++)
        assert(monster_social_turn(a));
    assert(b->hp < hp && b->alertness >= ALERTNESS_UNWARY);
    assert(!b->skip_next_turn && b->ai.sense.kind == MON_SENSE_NONE);
    cave_world_event event;
    assert(cave_event_latest(&event) && event.kind == CAVE_EVENT_FIGHT);
    assert(event.y == a->fy && event.x == a->fx && event.volume == 24);
    assert(cave_fighting_mask_at(a->fy, a->fx) == 8);
    assert(cave_fighting_mask_at(b->fy, b->fx) > 0);
    assert(!cave_fighting_mask_at(p_ptr->py, p_ptr->px));
    assert(a->noise >= 10); /* Listen can pick up the combatant's racket. */
    hp = a->hp;
    for (int n = 0; n < 100 && a->hp == hp; n++)
        assert(monster_social_turn(b));
    assert(a->hp < hp);
    /* A stale/distant player track cannot erase an immediate social fight. */
    monster_senses_hear(a, p_ptr->py, p_ptr->px);
    assert(monster_social_turn(a));
    monster_social_player_attack(a);
    assert(!monster_social_turn(a));
    a->social_player_threat = 0;
    memset(&a->ai.sense, 0, sizeof(a->ai.sense));
    a->confused = 1; assert(!monster_social_turn(a)); a->confused = 0;
    a->stunned = 1; assert(!monster_social_turn(a)); a->stunned = 0;
    a->skip_this_turn = true; assert(!monster_social_turn(a)); a->skip_this_turn = false;
    a->smite_recovery = 1; assert(!monster_social_turn(a)); a->smite_recovery = 0;
    a->alertness = ALERTNESS_UNWARY - 1; assert(!monster_social_turn(a));
    a->alertness = ALERTNESS_UNWARY;
    p_ptr->truce = true; assert(!monster_social_turn(a)); p_ptr->truce = false;

    /* An ambient death must retain carried loot without granting player credit. */
    object_type loot;
    object_prep(&loot, lookup_kind(TV_POTION, SV_POTION_HEALING));
    int held = monster_carry(bi, &loot);
    assert(held > 0);
    int exp = p_ptr->exp, kill_exp = p_ptr->kill_exp;
    int kills = l_list[social_orc].pkills, total = l_list[social_orc].tkills;
    int encounter_exp = p_ptr->encounter_exp;
    p_ptr->niena_quest = NIENA_QUEST_ACTIVE;
    u32b old_flags = r_info[social_orc].flags2;
    if (territorial) r_info[social_orc].flags2 |= RF2_TERRITORIAL;
    b->hp = 1;
    /* Group hostility remains lethal; the victim still gets normal chances to
     * withdraw when its own scheduled action arrives. */
    monster_social_set_group(a, 8); monster_social_set_group(b, 9);
    monster_group_set_relation(8, 9, MON_REL_HOSTILE);
    for (int n = 0; n < 200 && b->r_idx; n++) assert(monster_social_turn(a));
    r_info[social_orc].flags2 = old_flags;
    assert(!b->r_idx && !a->social_rival);
    assert(p_ptr->exp == exp && p_ptr->kill_exp == kill_exp);
    assert(p_ptr->encounter_exp == encounter_exp);
    assert(l_list[social_orc].pkills == kills && l_list[social_orc].tkills == total);
    assert(p_ptr->niena_quest == NIENA_QUEST_ACTIVE && !p_ptr->niena_monsters_killed);
    bool found = false;
    for (int i = 1; i < o_max; i++)
        if (o_list[i].k_idx == loot.k_idx && !o_list[i].held_m_idx) found = true;
    assert(found);
    if (territorial) assert(o_cnt == 1); /* Do not regenerate the hoard. */
    puts("Physical combat, retaliation, priorities, loot and no player kill/quest credit PASS.");
}

static void test_social_quarrels_and_movement(void)
{
    int human = 0;
    for (int i = 1; i < z_info->r_max; i++)
        if ((r_info[i].flags3 & RF3_MAN) && r_info[i].blow[0].dd
            && i != R_IDX_HUMAN_THRALL && i != R_IDX_ALERT_HUMAN_THRALL
            && !(r_info[i].flags1 & (RF1_UNIQUE | RF1_PEACEFUL | RF1_NEVER_BLOW)))
        { human = i; break; }
    assert(human);
    social_map();
    monster_type* orc = social_place(12, 10, social_orc);
    monster_type* man = social_place(12, 11, human);
    for (int n = 0; n < 20000 && !orc->social_rival; n++) monster_social_turn(orc);
    assert(orc->social_rival && man->social_rival);
    assert(monster_social_relation(orc, man) == MON_REL_HOSTILE);
    assert(monster_group_relation(MON_GROUP_ORC, MON_GROUP_MAN) == MON_REL_NEUTRAL);

    social_map();
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    for (int n = 0; n < 20000 && !a->social_rival; n++) monster_social_turn(a);
    assert(a->social_rival && b->social_rival);
    assert(a->hp == a->maxhp && b->hp == b->maxhp); /* challenge costs a turn */
    social_fight(a, b);
    int old_y = b->fy, old_x = b->fx;
    monster_swap(old_y, old_x, 12, 15);
    assert(monster_social_turn(a));
    assert(distance(a->fy, a->fx, b->fy, b->fx) < 5);

    /* An enclosed rival cannot be pursued through neutral creatures/player. */
    for (int k = 0; k < 8; k++) {
        int y = a->fy + ddy_ddd[k], x = a->fx + ddx_ddd[k];
        cave_feat[y][x] = FEAT_WALL_PERM; cave_info[y][x] = CAVE_WALL;
    }
    old_y = a->fy; old_x = a->fx;
    assert(!monster_social_turn(a));
    assert(a->fy == old_y && a->fx == old_x);

    social_map();
    a = social_place(12, 10, social_orc);
    b = social_place(12, 11, R_IDX_HUMAN_THRALL);
    assert(monster_social_group(b) == MON_GROUP_THRALL);
    assert(!monster_social_start_feud(cave_m_idx[12][10], cave_m_idx[12][11]));
    for (int n = 0; n < 20000; n++) assert(!monster_social_turn(a));
    assert(!a->social_rival && b->hp == b->maxhp);
    b->r_idx = R_IDX_ALERT_HUMAN_THRALL;
    assert(!monster_social_start_feud(cave_m_idx[12][10], cave_m_idx[12][11]));
    puts("Seeded spontaneous quarrels, visible pursuit, walls and thrall protection PASS.");
}

static void test_social_lifecycle_and_save(void)
{
    social_map();
    social_place(10, 10, social_orc); /* Deleted hole before the feud pair. */
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    monster_social_set_group(a, 8); monster_social_set_group(b, 9);
    monster_group_set_relation(8, 9, MON_REL_HOSTILE);
    assert(monster_social_start_feud(cave_m_idx[12][10], cave_m_idx[12][11]));
    delete_monster_idx(cave_m_idx[10][10]);
    compact_monsters(0);
    a = &mon_list[cave_m_idx[12][10]]; b = &mon_list[cave_m_idx[12][11]];
    assert(&mon_list[a->social_rival] == b && &mon_list[b->social_rival] == a);
    a->social_memory = 7; b->social_memory = 9;
    a->social_cooldown = 12;
    size_t dungeon_size, length = fixture_write_dungeon(encoded, sizeof(encoded), &dungeon_size);
    decode(encoded, plain, length);
    size_t block_size = 4 + (MON_GROUP_MAX-1)*(MON_GROUP_MAX-2)/2 + MON_SOCIAL_RECORD_BYTES*(mon_max-1);
    size_t block = dungeon_size - block_size;
    assert(plain[block] == (MON_SOCIAL_SAVE_MAGIC & 255));
    u32b sentinel; size_t consumed;
    social_map();
    assert(!fixture_read_dungeon(encoded, length, VERSION_EXTRA, &sentinel, &consumed));
    assert(sentinel == 0xA1B2C3D4U && consumed == length);
    a = &mon_list[cave_m_idx[12][10]]; b = &mon_list[cave_m_idx[12][11]];
    assert(a->social_group == 8 && b->social_group == 9);
    assert(a->social_memory == 7 && b->social_memory == 9 && a->social_cooldown == 12);
    assert(a->social_state == MON_SOCIAL_CHALLENGE && a->social_timer == 2);
    assert(&mon_list[a->social_rival] == b && &mon_list[b->social_rival] == a);
    assert(monster_group_relation(8, 9) == MON_REL_HOSTILE);
    int victim = cave_m_idx[b->fy][b->fx];
    delete_monster_idx(victim);
    assert(!a->social_rival && a->social_cooldown == MON_SOCIAL_COOLDOWN);
    int old_max = mon_max;
    mon_max = MAX_MONSTERS; /* Force the real allocator to reuse a hole. */
    b = social_place(12, 11, social_orc);
    assert(cave_m_idx[12][11] == victim);
    mon_max = old_max;
    assert(monster_social_relation(a, b) == MON_REL_NEUTRAL);

    /* Reject every truncation of the new block; no leaked partial diplomacy. */
    for (size_t cut = block; cut < dungeon_size; cut++) {
        social_map();
        assert(fixture_read_dungeon(encoded, cut, VERSION_EXTRA, &sentinel, &consumed));
        assert(monster_group_relation(8, 9) == MON_REL_NEUTRAL);
    }
    size_t record = block + 4 + (MON_GROUP_MAX-1)*(MON_GROUP_MAX-2)/2;
    size_t corrupt[] = {block, block+2, record, record+1, record+3, record+4,
        record+5, record+6, record+7, record+9, record+11};
    for (size_t i = 0; i < N_ELEMENTS(corrupt); i++) {
        byte old = plain[corrupt[i]]; plain[corrupt[i]] = 255;
        encode(plain, modified, length); social_map();
        assert(fixture_read_dungeon(modified, length, VERSION_EXTRA, &sentinel, &consumed));
        assert(monster_group_relation(8, 9) == MON_REL_NEUTRAL);
        plain[corrupt[i]] = old;
    }
    /* Strip appended per-monster fields to create an actual v19 social block. */
    size_t legacy_end = record;
    for (int i = 0; i < 2; i++) {
        memcpy(plain + legacy_end, plain + record + MON_SOCIAL_RECORD_BYTES*i, 5);
        legacy_end += 5;
    }
    memmove(plain + legacy_end, plain + dungeon_size, 8);
    encode(plain, modified, legacy_end + 8); social_map();
    assert(!fixture_read_dungeon(modified, legacy_end + 8, 19, &sentinel, &consumed));
    assert(sentinel == 0xA1B2C3D4U && consumed == legacy_end + 8);
    a = &mon_list[cave_m_idx[12][10]]; b = &mon_list[cave_m_idx[12][11]];
    assert(a->social_state == MON_SOCIAL_FIGHT && b->social_state == MON_SOCIAL_FIGHT);
    assert(a->social_timer == MON_SOCIAL_DISPUTE_ACTIONS && !a->social_focus && !a->social_player_threat);
    decode(encoded, plain, length);
    /* Version 18 has identical earlier lanes but no social block. */
    memmove(plain + block, plain + dungeon_size, 8);
    encode(plain, modified, block + 8); social_map();
    monster_group_set_relation(8, 9, MON_REL_HOSTILE);
    assert(!fixture_read_dungeon(modified, block + 8, 18, &sentinel, &consumed));
    assert(sentinel == 0xA1B2C3D4U && consumed == block + 8);
    assert(monster_group_relation(8, 9) == MON_REL_NEUTRAL);
    for (int i = 1; i < mon_max; i++)
        assert(!mon_list[i].social_group && !mon_list[i].social_rival);
    monster_group_set_relation(8, 9, MON_REL_HOSTILE);
    wipe_mon_list();
    assert(monster_group_relation(8, 9) == MON_REL_NEUTRAL);
    puts("Real compaction/deletion, save roundtrip, corrupt/truncated data, v18 defaults and level reset PASS.");
}

static void test_social_scheduler(void)
{
    social_map();
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    int ai = cave_m_idx[12][10], bi = cave_m_idx[12][11];
    assert(monster_social_start_feud(ai, bi));
    int ahp = a->hp, bhp = b->hp;
    for (int n = 0; n < 50 && (a->hp == ahp || b->hp == bhp); n++) {
        a->energy = b->energy = 100;
        process_monsters(100);
        assert(a->energy < 100 && b->energy < 100);
    }
    assert(a->hp < ahp && b->hp < bhp);
    assert(a->social_rival == bi && b->social_rival == ai);
    assert(!a->ai.sense.kind && !b->ai.sense.kind);
    puts("Real scheduler spends actions on reciprocal combat without a player target PASS.");
}

static void test_social_cooperation(void)
{
    social_map();
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    b->alertness = ALERTNESS_ALERT;
    assert(monster_social_allies(a, b));
    assert(morale_from_friends(a) > 0);
    social_fight(a, b);
    assert(!monster_social_allies(a, b));
    assert(morale_from_friends(a) == 0);
    a->alertness = ALERTNESS_ALERT;
    a->ai.observations[MON_AI_FIRE].value = 3;
    a->ai.observations[MON_AI_FIRE].ttl = 40;
    a->ai.observations[MON_AI_FIRE].turn = playerturn;
    monster_ai_share_warning(a);
    assert(!monster_ai_confidence(b, MON_AI_FIRE));
    monster_social_set_group(a, 8); monster_social_set_group(b, 9);
    a->wandering_idx = b->wandering_idx = FLOW_WANDERING_HEAD;
    assert(!monster_social_allies(a, b)); /* Neutral different bands aren't friends. */
    assert(morale_from_friends(a) == 0);
    monster_social_set_group(b, 8);
    assert(monster_social_allies(a, b));
    monster_ai_share_warning(a);
    assert(monster_ai_confidence(b, MON_AI_FIRE) > 0);
    monster_group_set_relation(8, 9, MON_REL_HOSTILE);
    monster_social_set_group(b, 9);
    assert(!monster_social_allies(a, b));

    social_map();
    a = social_place(12, 10, 11); /* Wolf */
    b = social_place(12, 11, 32); /* Spider hatchling */
    a->wandering_idx = b->wandering_idx = 0;
    assert(monster_social_group(a) == MON_GROUP_OTHER && monster_social_group(b) == MON_GROUP_OTHER);
    assert(!monster_social_allies(a, b));
    monster_social_set_group(a, 8); monster_social_set_group(b, 8);
    assert(monster_social_allies(a, b)); /* Authored mixed-species bands work. */
    puts("Hostile and neutral bands excluded from morale/warnings; actual allies and mixed bands cooperate PASS.");
}

static void test_social_escalation_and_retreat(void)
{
    social_map();
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    assert(monster_social_start_feud(cave_m_idx[12][10], cave_m_idx[12][11]));
    assert(monster_social_turn(a));
    assert(a->social_state == MON_SOCIAL_CHALLENGE && a->hp == 1000 && b->hp == 1000);
    assert(monster_social_turn(a));
    assert(a->social_state == MON_SOCIAL_FIGHT && b->social_state == MON_SOCIAL_FIGHT);
    assert(a->hp == 1000 && b->hp == 1000); /* Escalation is also a paid action. */
    b->hp = 100;
    calc_morale(b); calc_stance(b);
    assert(b->stance == STANCE_CONFIDENT); /* Existing player-facing stance. */
    int ahp = a->hp, bhp = b->hp;
    assert(monster_social_turn(b));
    assert(!a->social_rival && !b->social_rival);
    assert(b->social_state == MON_SOCIAL_AVOID);
    assert(distance(a->fy, a->fx, b->fy, b->fx) > 1);
    assert(a->hp == ahp && b->hp == bhp);
    assert(monster_social_relation(a, b) == MON_REL_NEUTRAL);
    assert(a->social_cooldown && b->social_cooldown);
    for (int n = 0; n < 10; n++) {
        monster_social_tick(b); monster_social_turn(b);
        assert(monster_social_valid(b));
    }
    assert(b->social_state == MON_SOCIAL_NONE);

    /* An already wounded creature can yield during the challenge. */
    social_map();
    a = social_place(12, 10, social_orc); b = social_place(12, 11, social_orc);
    b->hp = 400;
    assert(monster_social_start_feud(cave_m_idx[12][10], cave_m_idx[12][11]));
    assert(monster_social_turn(b));
    assert(!a->social_rival && !b->social_rival && a->hp == 1000 && b->hp == 400);

    /* Equal opponents eventually disengage even when armour prevents damage. */
    social_map();
    a = social_place(12, 10, social_orc); b = social_place(12, 11, social_orc);
    social_fight(a, b);
    for (int n = 0; n < MON_SOCIAL_DISPUTE_ACTIONS && a->social_rival; n++) {
        monster_social_tick(a); assert(monster_social_turn(a));
        assert(monster_social_valid(a) && monster_social_valid(b));
    }
    assert(!a->social_rival && !b->social_rival && a->r_idx && b->r_idx);

    /* Yielding doesn't require an escape square and never enters hazards. */
    social_map();
    a = social_place(12, 10, social_orc); b = social_place(12, 11, social_orc);
    social_fight(a, b); b->hp = 100;
    for (int k = 0; k < 8; k++) {
        int y = b->fy + ddy_ddd[k], x = b->fx + ddx_ddd[k];
        if (!cave_m_idx[y][x]) cave_feat[y][x] = FEAT_CHASM;
    }
    assert(monster_social_turn(b));
    assert(b->fy == 12 && b->fx == 11 && !a->social_rival && !b->social_rival);
    puts("Paid challenge/escalation, unwary injury withdrawal, yielding, timeout, cooldown and safe cornered retreat PASS.");
}

static void test_social_attention(void)
{
    social_map();
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    social_fight(a, b);
    monster_senses_hear(a, p_ptr->py, p_ptr->px);
    playerturn += 10;
    assert(monster_social_turn(a)); /* Even a retained target isn't a blanket veto. */
    monster_ai_player_attack(a, 0);
    assert(a->social_player_threat == 3 && !monster_social_turn(a));
    for (int i = 0; i < 3; i++) monster_social_tick(a);
    assert(monster_social_turn(a));
    assert(!mon_take_hit(cave_m_idx[a->fy][a->fx], 0, "", -1));
    assert(a->social_player_threat == 3); /* A direct ranged hit also counts. */
    a->social_player_threat = 0; a->skip_next_turn = false;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->py = 12; p_ptr->px = 9; cave_m_idx[12][9] = -1;
    bool old_recognition = visual_recognition;
    visual_recognition = false;
    assert(monster_has_sight(a) && !monster_social_turn(a));
    cave_m_idx[12][9] = 0; p_ptr->px = 18; cave_m_idx[12][18] = -1;
    assert(monster_has_sight(a) && monster_social_turn(a));
    visual_recognition = old_recognition;
    puts("Stale/distant player tracking yields to nearby conflict; visible nearby player and real aggression take priority PASS.");
}

static void test_social_witnesses(void)
{
    social_map();
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    monster_type* c = social_place(11, 10, social_orc);
    monster_social_set_group(a, 8); monster_social_set_group(b, 9);
    monster_social_set_group(c, 8);
    social_fight(a, b);
    assert(monster_social_turn(c));
    assert(c->social_state == MON_SOCIAL_HELP && c->social_ally == cave_m_idx[a->fy][a->fx]);
    assert(c->social_focus == cave_m_idx[b->fy][b->fx]);
    assert(a->hp == 1000 && b->hp == 1000); /* Joining spends an action. */
    assert(monster_social_allies(c, a) && !monster_social_allies(c, b));
    assert(monster_social_relation(c, b) == MON_REL_HOSTILE);
    for (int i = 0; i < 7 && b->hp == 1000; i++) assert(monster_social_turn(c));
    assert(b->hp < 1000);
    b->hp = 100;
    assert(monster_social_turn(b)); /* Yield ends the helpers' hostility too. */
    assert(c->social_state == MON_SOCIAL_NONE && !c->social_focus && !c->social_ally);
    assert(monster_group_relation(8, 9) == MON_REL_NEUTRAL);

    social_map();
    a = social_place(12, 10, social_orc); b = social_place(12, 11, social_orc);
    c = social_place(11, 10, social_orc);
    monster_social_set_group(a, 8); monster_social_set_group(b, 9);
    monster_social_set_group(c, 8); social_fight(a, b);
    c->hp = 250;
    assert(monster_social_turn(c));
    assert(c->social_state == MON_SOCIAL_AVOID && !c->social_ally);
    assert(distance(c->fy, c->fx, b->fy, b->fx) > 1 && b->hp == 1000);

    social_map();
    a = social_place(12, 10, social_orc); b = social_place(12, 11, social_orc);
    c = social_place(12, 8, social_orc);
    monster_social_set_group(a, 8); monster_social_set_group(b, 9);
    monster_social_set_group(c, 8); social_fight(a, b);
    cave_feat[12][9] = FEAT_WALL_EXTRA; cave_info[12][9] = CAVE_WALL;
    cave_event_emit(CAVE_EVENT_FIGHT, a->fy, a->fx, 24);
    assert(!monster_social_turn(c) && c->social_state == MON_SOCIAL_NONE);

    /* A captain trusted by both parties can stop a dispute before any blows. */
    social_map();
    int captain = 0;
    for (int i = 1; i < z_info->r_max; i++)
        if ((r_info[i].flags3 & RF3_ORC) && (r_info[i].flags1 & (RF1_ESCORT | RF1_ESCORTS))
            && !(r_info[i].flags1 & (RF1_UNIQUE | RF1_PEACEFUL))
            && r_info[i].level >= r_info[social_orc].level) { captain = i; break; }
    assert(captain);
    a = social_place(12, 10, social_orc); b = social_place(12, 11, social_orc);
    c = social_place(11, 10, captain);
    assert(monster_social_start_feud(cave_m_idx[12][10], cave_m_idx[12][11]));
    assert(monster_social_turn(c));
    assert(!a->social_rival && !b->social_rival && a->hp == 1000 && b->hp == 1000);

    /* Only two helpers per side join a personal quarrel, on their own actions. */
    social_map();
    a = social_place(12, 10, social_orc); b = social_place(12, 11, social_orc);
    c = social_place(11, 9, social_orc);
    monster_type* d = social_place(13, 9, social_orc);
    monster_type* e = social_place(14, 9, social_orc);
    monster_social_set_group(a, 8); monster_social_set_group(b, 9);
    monster_social_set_group(c, 8); monster_social_set_group(d, 8); monster_social_set_group(e, 8);
    social_fight(a, b);
    a->energy = b->energy = d->energy = e->energy = 0; c->energy = 100;
    process_monsters(100);
    assert(c->energy < 100 && c->social_state == MON_SOCIAL_HELP);
    assert(a->hp == 1000 && b->hp == 1000);
    assert(monster_social_turn(d) && d->social_state == MON_SOCIAL_HELP);
    assert(!monster_social_turn(e) && e->social_state == MON_SOCIAL_NONE);
    monster_group_set_relation(8, 9, MON_REL_NEUTRAL);
    assert(c->social_state == MON_SOCIAL_NONE && d->social_state == MON_SOCIAL_NONE);

    /* Hostile group clashes also expose fighters to frightened bystanders. */
    social_map();
    a = social_place(12, 10, social_orc); b = social_place(12, 11, social_orc);
    c = social_place(11, 10, social_orc);
    monster_social_set_group(a, 8); monster_social_set_group(b, 9); monster_social_set_group(c, 10);
    monster_group_set_relation(8, 9, MON_REL_HOSTILE);
    assert(monster_social_turn(a) && a->social_state == MON_SOCIAL_FIGHT);
    c->hp = 250;
    assert(monster_social_turn(c) && c->social_state == MON_SOCIAL_AVOID);
    b->hp = 250; assert(monster_social_turn(b));
    assert(monster_group_relation(8, 9) == MON_REL_HOSTILE);
    puts("Witnesses defend trusted allies, retreat when wounded, require direct sight, and obey allied captain intervention PASS.");
}

static void test_social_response_persistence(void)
{
    social_map();
    social_place(10, 10, social_orc);
    monster_type* a = social_place(12, 10, social_orc);
    monster_type* b = social_place(12, 11, social_orc);
    monster_type* c = social_place(11, 10, social_orc);
    monster_type* d = social_place(13, 11, social_orc);
    monster_social_set_group(a, 8); monster_social_set_group(b, 9);
    monster_social_set_group(c, 8); monster_social_set_group(d, 10);
    social_fight(a, b);
    assert(monster_social_turn(c));
    d->hp = 250; assert(monster_social_turn(d));
    assert(c->social_state == MON_SOCIAL_HELP && d->social_state == MON_SOCIAL_AVOID);
    int dy = d->fy, dx = d->fx;
    monster_social_player_attack(c);
    delete_monster_idx(cave_m_idx[10][10]); compact_monsters(0);
    a = &mon_list[cave_m_idx[12][10]]; b = &mon_list[cave_m_idx[12][11]];
    c = &mon_list[cave_m_idx[11][10]]; d = &mon_list[cave_m_idx[dy][dx]];
    assert(&mon_list[c->social_ally] == a && &mon_list[c->social_focus] == b);
    assert(monster_social_valid(d));
    size_t size, length = fixture_write_dungeon(encoded, sizeof(encoded), &size);
    social_map(); u32b sentinel; size_t consumed;
    assert(!fixture_read_dungeon(encoded, length, VERSION_EXTRA, &sentinel, &consumed));
    assert(consumed == length && sentinel == 0xA1B2C3D4U);
    a = &mon_list[cave_m_idx[12][10]]; b = &mon_list[cave_m_idx[12][11]];
    c = &mon_list[cave_m_idx[11][10]]; d = &mon_list[cave_m_idx[dy][dx]];
    assert(c->social_state == MON_SOCIAL_HELP && c->social_player_threat == 3);
    assert(&mon_list[c->social_ally] == a && &mon_list[c->social_focus] == b);
    assert(d->social_state == MON_SOCIAL_AVOID && monster_social_valid(d));
    delete_monster_idx(cave_m_idx[b->fy][b->fx]);
    assert(c->social_state == MON_SOCIAL_NONE && !c->social_focus && !c->social_ally);
    assert(monster_social_valid(a) && monster_social_valid(c) && monster_social_valid(d));
    puts("Assistance, withdrawal and attention survive real compaction/save/load; deleted combatants clear witness references PASS.");
}

static void test_social_simulation(void)
{
    social_map();
    const int ys[] = {10,10,11,11,14,14,15,15};
    const int xs[] = {5,6,5,6,15,16,15,16};
    const int groups[] = {8,9,8,10,9,8,9,10};
    for (int i = 0; i < 8; i++) {
        monster_type* m = social_place(ys[i], xs[i], social_orc);
        monster_social_set_group(m, groups[i]);
        if (groups[i] == 10) m->hp = 250;
    }
    monster_group_set_relation(8, 9, MON_REL_HOSTILE);
    for (int action = 0; action < 100; action++) {
        ++playerturn; turn += 10;
        if (action == 35) monster_group_set_relation(8, 9, MON_REL_NEUTRAL);
        if (action == 65) monster_group_set_relation(8, 9, MON_REL_HOSTILE);
        for (int i = 1; i < mon_max; i++) if (mon_list[i].r_idx) mon_list[i].energy = 100;
        process_monsters(100);
        for (int i = 1; i < mon_max; i++) {
            monster_type* m = &mon_list[i];
            if (!m->r_idx) continue;
            assert(m->energy < 100 && monster_social_valid(m));
            if (m->social_rival) {
                assert(mon_list[m->social_rival].social_rival == i);
                assert(mon_list[m->social_rival].social_state == m->social_state);
            }
            if (m->social_state == MON_SOCIAL_HELP)
                assert(mon_list[m->social_ally].social_rival == m->social_focus);
        }
        if (action % 25 == 24) {
            compact_monsters(0);
            size_t size, length = fixture_write_dungeon(encoded, sizeof(encoded), &size);
            u32b sentinel; size_t consumed;
            s32b saved_playerturn = playerturn, saved_turn = turn;
            social_map(); /* Real loading begins with an empty monster/object list. */
            playerturn = saved_playerturn; turn = saved_turn;
            assert(!fixture_read_dungeon(encoded, length, VERSION_EXTRA, &sentinel, &consumed));
            assert(consumed == length && sentinel == 0xA1B2C3D4U);
        }
    }
    puts("100 scheduled multi-band actions with diplomacy changes, repeated compaction and live saves preserve state invariants PASS.");
}

static void test_monster_social(void)
{
    drop_system_init();
    assert(init_flavor_info() == 0);
    flavor_init();
    for (int i = 1; i < z_info->r_max; i++)
        if ((r_info[i].flags3 & RF3_ORC) && r_info[i].blow[0].dd
            && !(r_info[i].flags1 & (RF1_UNIQUE | RF1_NEVER_BLOW | RF1_NEVER_MOVE | RF1_PEACEFUL))
            && !(r_info[i].flags2 & RF2_TERRITORIAL)) { social_orc = i; break; }
    assert(social_orc);
    test_social_relationships();
    test_social_combat(false);
    test_social_combat(true);
    test_social_quarrels_and_movement();
    test_social_scheduler();
    test_social_lifecycle_and_save();
    test_social_cooperation();
    test_social_escalation_and_retreat();
    test_social_attention();
    test_social_witnesses();
    test_social_response_persistence();
    test_social_simulation();
    puts("Monster social AI engine integration: PASS.");
}
'''

if __name__ == "__main__":
    persistence.OUT = persistence.ROOT / "scripts/output/monster-social"
    persistence.main(TESTS, "    test_monster_social();")
