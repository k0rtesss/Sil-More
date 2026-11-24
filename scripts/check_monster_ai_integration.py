#!/usr/bin/env python3
"""Exercise adaptive AI through the real engine, in isolated temporary data.

Build first. Reuses the initialized-template encounter fixture; no player save,
configuration, or interactive frontend is opened.
"""
import check_new_monsters as engine

CHECKS = r'''
#include "monster/monster-ai.h"
#include "monster/monster-senses.h"
#include "monster/monster-tactics.h"
#include "monster/monster-routine.h"
#include "melee/melee-attack.h"
#include "melee/melee-movement.h"
#include "melee/melee-process.h"
#include "melee/melee-util.h"
#include "spell/spell-projection-internal.h"
#include "player/player-song-internal.h"
void process_player(void); /* Internal production scheduler entry point. */

static void check_command_profiles(void)
{
    assert(r_info[41].command_grade == MON_COMMAND_ORDINARY);
    assert(r_info[81].command_grade == MON_COMMAND_LEADER);
    assert(r_info[91].command_grade == MON_COMMAND_COMMANDER);
    assert(r_info[82].command_grade == MON_COMMAND_LEADER);
    assert(r_info[192].command_grade == MON_COMMAND_LEADER);
    assert(r_info[11].command_style == MON_COMMAND_PACK);
    assert(r_info[222].command_authority == (1UL << MON_KIN_DRAGON));
    assert(r_info[241].command_authority & (1UL << MON_KIN_TROLL));
    assert(r_info[409].command_authority & (1UL << MON_KIN_RAUKO));
    int ordinary_leaders = 0;
    for (int i = 1; i < z_info->r_max; ++i)
    {
        const monster_race* r = &r_info[i];
        if (r->command_kin) {
            assert(r->command_kin < MON_KIN_MAX && r->command_grade <= MON_COMMAND_COMMANDER);
            assert(r->command_authority & (1UL << r->command_kin));
        }
        if (!(r->flags1 & RF1_UNIQUE) && r->command_grade) ++ordinary_leaders;
    }
    assert(ordinary_leaders >= 40);
    /* This fixture's data directory is temporary. Exercise both raw-cache load
     * and automatic replacement of a cache with the old race-record size. */
    assert(free_info(&r_head) == 0); assert(init_r_info() == 0);
    assert(r_info[41].command_grade == MON_COMMAND_ORDINARY);
    char path[1024]; strnfmt(path, sizeof(path), "%s/monster.raw", ANGBAND_DIR_DATA);
    FILE* raw = fopen(path, "r+b"); assert(raw);
    header old; assert(fread(&old, sizeof(old), 1, raw) == 1);
    old.info_len -= 8;
    rewind(raw); assert(fwrite(&old, sizeof(old), 1, raw) == 1); assert(fclose(raw) == 0);
    assert(free_info(&r_head) == 0); assert(init_r_info() == 0);
    assert(r_info[241].command_grade == MON_COMMAND_COMMANDER);
    const char* invalid[] = {"L:KING:TACTICAL:ORC", "L:ORDINARY:SOLDIER:ORC",
        "L:LEADER:TACTICAL:NONE", "L:ORDINARY:TACTICAL:ORC:TROLL",
        "L:COMMANDER:PACK:WOLF:ORC", "L:COMMANDER:TACTICAL:ORC:TROLL|",
        "L:COMMANDER:TACTICAL:ORC:ORC", "L:LEADER:TACTICAL"};
    for (int i = 0; i < (int)N_ELEMENTS(invalid); ++i)
    {
        monster_race race = {0}; char names[128] = {0};
        header head = {.info_num = 1, .info_ptr = &race, .name_ptr = names};
        char name[] = "N:0:command parser fixture", line[128]; error_idx = -1;
        assert(parse_r_info(name, &head) == 0);
        SDL_strlcpy(line, invalid[i], sizeof(line));
        assert(parse_r_info(line, &head) != 0 && !race.command_kin);
    }
    printf("Command templates: %d ordinary leader-capable races; grades, kin, malformed records and cache regeneration PASS.\n", ordinary_leaders);
}

static monster_type* adaptive_fixture(int race, int evasion)
{
    monster_type* m = combat_fixture(race, evasion);
    p_ptr->song1 = p_ptr->song2 = SNG_NOTHING;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    p_ptr->cur_light = 4;
    /* The fixture is open floor; establish its field of fire, which the
     * production projectable() fast path requires even for adjacent attacks. */
    for (int y=0; y<p_ptr->cur_map_hgt; ++y)
        for (int x=0; x<p_ptr->cur_map_wid; ++x)
            if (!y || !x || y==p_ptr->cur_map_hgt-1 || x==p_ptr->cur_map_wid-1) {
                cave_feat[y][x] = FEAT_WALL_PERM;
                cave_info[y][x] |= CAVE_WALL;
            }
            else if (distance(y,x,p_ptr->py,p_ptr->px) <= MAX_SIGHT)
                cave_info[y][x] |= CAVE_FIRE | CAVE_VIEW;
    return m;
}

static void corner_point(int orientation, int dy, int dx, int* y, int* x)
{
    if (orientation & 4) dx = -dx;
    for (int i = 0; i < (orientation & 3); ++i) {
        int old = dy; dy = dx; dx = -old;
    }
    *y = 30 + dy; *x = 30 + dx;
}

static monster_type* territorial_corner(int race, int orientation)
{
    monster_type* m = adaptive_fixture(race, 0);
    cave_m_idx[m->fy][m->fx] = 0;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->py = p_ptr->px = 30;
    for (int dy = -1; dy <= 5; ++dy) for (int dx = -3; dx <= 3; ++dx) {
        int y, x; corner_point(orientation, dy, dx, &y, &x);
        bool floor = (dx == 0 && dy >= -1 && dy <= 1)
            || (dy >= 2 && dy <= 4 && dx >= -1 && dx <= 1);
        cave_set_feat(y, x, floor ? FEAT_FLOOR : FEAT_WALL_PERM);
        cave_info[y][x] |= CAVE_ICKY;
    }
    int my, mx; corner_point(orientation, 2, -1, &my, &mx);
    m->fy = my; m->fx = mx;
    cave_m_idx[m->fy][m->fx] = (int)(m - mon_list);
    cave_m_idx[30][30] = -1;
    m->routine.home_y = m->fy; m->routine.home_x = m->fx;
    m->cdis = 2; m->min_range = 0; m->best_range = 0;
    m->stance = STANCE_CONFIDENT;
    m->tmp_morale = 100;
    memset(&m->ai.sense, 0, sizeof(m->ai.sense));
    assert(!monster_ai_can_see_player(m) && monster_lair_contains(m, m->fy, m->fx));
    return m;
}

static void check_territorial_defense(void)
{
    int y, x, by, bx;
    bool fear, bash;
    const int races[] = {124, 15, 32}; /* dragon, orc and spider */
    for (int i = 0; i < (int)N_ELEMENTS(races); ++i)
        for (int orientation = 0; orientation < 8; ++orientation)
            for (int evidence = 0; evidence < 3; ++evidence) {
                monster_type* m = territorial_corner(races[i], orientation);
                fear = bash = false;
                assert(!get_move(m, &y, &x, &fear, false));
                if (evidence == 0) monster_senses_hear(m, 30, 30);
                if (evidence == 1) { monster_senses_see(m, 30, 30); ++playerturn; }
                if (evidence == 2) monster_senses_share_trace(m, 30, 30);
                u64b rng = Rand_state_export();
                assert(get_move(m, &y, &x, &fear, false));
                corner_point(orientation, 1, 0, &by, &bx);
                assert(y == by && x == bx && !fear);
                assert(Rand_state_export() == rng);
                assert(cave_passable_mon(m, y, x, &bash) == 100);
                assert(make_move(m, &y, &x, fear, &bash));
                process_move(m, y, x, bash);
                assert(m->fy == by && m->fx == bx);
            }
    puts("Territorial doorway: 72 dragon/orc/spider orientation/evidence cases take the legal adjacent block without blind targeting PASS.");

    /* Real scheduler with the race's original random trait: every step stays
     * in its saved lair even while the visible player remains beyond it. */
    monster_type* m = adaptive_fixture(124, 0);
    cave_m_idx[10][10] = cave_m_idx[10][11] = 0;
    p_ptr->py = 30; p_ptr->px = 40; cave_m_idx[30][40] = -1;
    m->fy = 30; m->fx = 35; cave_m_idx[30][35] = (int)(m - mon_list);
    m->routine.home_y = m->routine.home_x = 30;
    m->stance = STANCE_CONFIDENT; m->tmp_morale = 100;
    for (int action = 0; action < 32; ++action) {
        m->cdis = distance(m->fy, m->fx, 30, 40);
        ++playerturn; m->energy = 100; process_monsters(100);
        assert(monster_lair_contains(m, m->fy, m->fx));
        assert(m->routine.home_y == 30 && m->routine.home_x == 30);
    }

    m = territorial_corner(124, 0); bash = false;
    cave_info[31][30] &= ~CAVE_ICKY;
    assert(!cave_passable_mon(m, 31, 30, &bash));
    int oy = m->fy, ox = m->fx;
    process_move(m, 31, 30, false);
    assert(m->fy == oy && m->fx == ox);

    /* Forced displacement is a return journey, even with a nearer visible
     * player outside the vault. It must not establish a new pursuit center. */
    m = adaptive_fixture(124, 0);
    cave_m_idx[10][10] = cave_m_idx[10][11] = 0;
    p_ptr->py = 30; p_ptr->px = 34; cave_m_idx[30][34] = -1;
    cave_info[30][30] |= CAVE_ICKY;
    m->fy = 30; m->fx = 33; cave_m_idx[30][33] = (int)(m - mon_list);
    m->routine.home_y = m->routine.home_x = 30;
    m->cdis = 1;
    assert(monster_routine_needs_return(m));
    for (int action = 0; action < 3; ++action) {
        m->energy = 100; ++playerturn; process_monsters(100);
        assert(m->fx < 33 - action);
        assert(m->routine.home_y == 30 && m->routine.home_x == 30);
    }
    assert(m->fy == 30 && m->fx == 30 && !monster_routine_needs_return(m));
    puts("Territorial scheduler: visible pursuit/random movement stays local; vault exits rejected; displaced defender returns to its saved home PASS.");
}

static void check_weapon_fear(void)
{
    int spider = 0, orc = 0;
    for (int i = 1; i < z_info->r_max; ++i) {
        const monster_race* race = &r_info[i];
        if ((race->flags1 & RF1_UNIQUE) || (race->flags2 & RF2_MINDLESS)
            || (race->flags3 & RF3_NO_FEAR)) continue;
        if (!spider && (race->flags3 & RF3_SPIDER)) spider = i;
        if (!orc && (race->flags3 & RF3_ORC)) orc = i;
    }
    assert(spider && orc);
    monster_type* m = adaptive_fixture(spider, 0);
    assert(place_monster_one(9,10,spider,false,true,NULL));
    monster_type* friend = &mon_list[cave_m_idx[9][10]];
    friend->alertness = ALERTNESS_ALERT;
    friend->tmp_morale = 0;
    assert(place_monster_one(9,11,orc,false,true,NULL));
    monster_type* other = &mon_list[cave_m_idx[9][11]];
    other->alertness = ALERTNESS_ALERT;
    other->tmp_morale = 0;
    m->tmp_morale = 0;
    m->hp = m->maxhp = 20000;
    object_type* weapon = &inventory[INVEN_WIELD];
    object_prep(weapon, lookup_kind(TV_SWORD, SV_LONG_SWORD));
    weapon->name2 = 22; /* Production 'of Doriath' template. */
    weapon->ident |= IDENT_KNOWN;
    assert(e_info[22].flags1 & TR1_SLAY_SPIDER);
    playerturn = 100;
    p_ptr->mdd = p_ptr->mds = 1;
    p_ptr->skill_use[S_MEL] = -1000;
    py_attack_aux(10,10,ATT_MAIN);
    assert(!monster_ai_confidence(m, MON_AI_SLAY_FEAR));
    assert(m->tmp_morale == 0 && friend->tmp_morale == 0);
    playerturn++;
    p_ptr->skill_use[S_MEL] = 1000;
    py_attack_aux(10,10,ATT_MAIN);
    assert(monster_ai_confidence(m, MON_AI_SLAY_FEAR) == 3);
    assert(monster_ai_confidence(friend, MON_AI_SLAY_FEAR) == 3);
    assert(m->tmp_morale == -20 && friend->tmp_morale == -20);
    assert(other->tmp_morale == 0);
    assert(!monster_ai_confidence(other, MON_AI_SLAY_FEAR));
    monster_ai_reset(friend);
    monster_ai_share_warning(m);
    assert(!monster_ai_confidence(friend, MON_AI_SLAY_FEAR));
    u32b noticed = 0;
    playerturn++;
    weapon->name2 = 0;
    assert(slay_bonus(weapon,m,&noticed) == 0);
    assert(m->tmp_morale == -20);
    assert(monster_ai_confidence(m, MON_AI_SLAY_FEAR) == 3);
    monster_ai_reset(m);
    monster_ai_reset(friend);
    scare_onlooking_friends(m,-20);
    assert(m->tmp_morale == -40 && friend->tmp_morale == -40);
    assert(!monster_ai_confidence(m, MON_AI_SLAY_FEAR));
    assert(!monster_ai_confidence(friend, MON_AI_SLAY_FEAR));
    weapon->name2 = 22;
    m->confused = 1;
    assert(slay_bonus(weapon,m,&noticed) == 1);
    assert(m->tmp_morale == -60);
    assert(!monster_ai_confidence(m, MON_AI_SLAY_FEAR));
    assert(monster_ai_confidence(friend, MON_AI_SLAY_FEAR) == 3);
    m->confused = 0;
    monster_ai_reset(friend);
    u32b flags3 = r_info[spider].flags3;
    r_info[spider].flags3 |= RF3_NO_FEAR;
    assert(slay_bonus(weapon,m,&noticed) == 1);
    assert(m->tmp_morale == -60);
    assert(!monster_ai_confidence(m, MON_AI_SLAY_FEAR));
    assert(!monster_ai_confidence(friend, MON_AI_SLAY_FEAR));
    r_info[spider].flags3 = flags3;
    p_ptr->py = p_ptr->px = 50; /* The blow's victim is still visible to its kin. */
    assert(!monster_ai_can_see_player(m));
    assert(!monster_ai_can_see_player(friend));
    assert(slay_bonus(weapon,m,&noticed) == 1);
    assert(m->tmp_morale == -80);
    assert(!monster_ai_confidence(m, MON_AI_SLAY_FEAR));
    assert(!monster_ai_confidence(friend, MON_AI_SLAY_FEAR));
    p_ptr->py = 10; p_ptr->px = 11;
    m->stance = friend->stance = STANCE_CONFIDENT;
    calc_morale(m);
    m->tmp_morale += 10 - m->morale;
    calc_morale(m);
    assert(m->morale == 10);
    assert(slay_bonus(weapon,m,&noticed) == 1);
    calc_morale(m);
    assert(m->morale == -10);
    calc_stance(m);
    assert(m->stance == STANCE_FLEEING);
    assert(monster_ai_confidence(m, MON_AI_SLAY_FEAR) == 3);
    puts("Production Doriath hit: exact -20 fear cohort learns and a shaken target flees; misses, ordinary fear, unrelated kinds, confusion, fear immunity, unseen attackers and warning spread stay distinct PASS.");
}

static void check_commanded_squad(void)
{
    monster_type* leader = adaptive_fixture(409, 0); /* Fankil's actual template. */
    int leader_idx = cave_m_idx[leader->fy][leader->fx];
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->py = 10; p_ptr->px = 16;
    cave_m_idx[10][16] = -1;
    assert(place_monster_one(8, 11, 82, false, true, NULL));
    assert(place_monster_one(12, 11, 82, false, true, NULL));
    assert(place_monster_one(10, 8, 103, false, true, NULL));
    int guards[] = {cave_m_idx[8][11], cave_m_idx[12][11]};
    int archer_idx = cave_m_idx[10][8];
    int moved = 0;
    for (int round = 0; round < 6; ++round)
    {
        int old_y[MAX_MONSTERS], old_x[MAX_MONSTERS];
        for (int i = 1; i < mon_max; ++i)
        {
            monster_type* m = &mon_list[i];
            if (!m->r_idx) continue;
            old_y[i] = m->fy; old_x[i] = m->fx;
            m->alertness = ALERTNESS_ALERT;
            m->skip_next_turn = m->skip_this_turn = false;
            m->stance = STANCE_AGGRESSIVE; m->tmp_morale = 200;
            m->min_range = i == archer_idx ? 2 : 1;
            m->best_range = i == archer_idx ? 4 : 1;
            m->cdis = distance(m->fy, m->fx, p_ptr->py, p_ptr->px);
            m->energy = 100;
        }
        process_monsters(100); /* Must prepare the plan and consume it itself. */
        assert(leader->squad.role == MON_SQUAD_COMMANDER);
        for (int n = 0; n < 2; ++n)
        {
            monster_type* m = &mon_list[guards[n]];
            assert(m->squad.commander == leader_idx && m->squad.role == MON_SQUAD_SOLDIER);
            moved += m->fy != old_y[guards[n]] || m->fx != old_x[guards[n]];
            assert(cave_m_idx[m->fy][m->fx] == guards[n]);
        }
        for (int i = 1; i < mon_max; ++i)
            for (int j = 1; j < i; ++j)
                if (mon_list[i].squad.role && mon_list[j].squad.role)
                    assert(mon_list[i].squad.y != mon_list[j].squad.y
                        || mon_list[i].squad.x != mon_list[j].squad.x);
        ++playerturn;
    }
    assert(moved >= 2);
    assert(distance(mon_list[guards[0]].fy, mon_list[guards[0]].fx, 10, 16) == 1
        || distance(mon_list[guards[1]].fy, mon_list[guards[1]].fx, 10, 16) == 1);
    assert(distance(mon_list[archer_idx].fy, mon_list[archer_idx].fx, 10, 16) >= 2);
    delete_monster_idx(leader_idx);
    assert(!mon_list[guards[0]].squad.role && !mon_list[guards[1]].squad.role);
    assert(place_monster_one(10, 10, 82, false, true, NULL));
    monster_squad_prepare();
    assert(mon_list[guards[0]].squad.role && mon_list[guards[1]].squad.role);
    assert(mon_list[guards[0]].squad.commander != leader_idx);
    puts("Real Fankil squad: six scheduler rounds, separate orders, melee approach, ranged spacing and commander death PASS.");
}

static void check_ordinary_squad(void)
{
    monster_type* first = adaptive_fixture(41, 0); /* Ordinary orc soldiers. */
    int first_idx = cave_m_idx[10][10];
    cave_m_idx[p_ptr->py][p_ptr->px] = 0; p_ptr->px = 13; cave_m_idx[10][13] = -1;
    assert(place_monster_one(9, 10, 41, false, true, NULL));
    int follower = cave_m_idx[9][10];
    for (int i = 1; i < mon_max; ++i) if (mon_list[i].r_idx) {
        monster_type* m = &mon_list[i];
        m->alertness = ALERTNESS_ALERT; m->skip_next_turn = m->skip_this_turn = false;
        m->stance = STANCE_AGGRESSIVE; m->tmp_morale = 200;
        m->min_range = m->best_range = 1; m->energy = 100;
        m->cdis = distance(m->fy, m->fx, 10, 13);
    }
    process_monsters(100);
    assert(first->squad.role == MON_SQUAD_COMMANDER);
    assert(mon_list[follower].squad.commander == first_idx);
    assert(first->fy != 10 || first->fx != 10 || mon_list[follower].fy != 9 || mon_list[follower].fx != 10);
    /* A champion takes over; its death returns command to an ordinary survivor. */
    assert(place_monster_one(11, 10, 81, false, true, NULL));
    int champion = cave_m_idx[11][10];
    monster_type* chief = &mon_list[champion];
    chief->alertness = ALERTNESS_ALERT; chief->stance = STANCE_AGGRESSIVE;
    chief->skip_this_turn = chief->skip_next_turn = false;
    chief->min_range = chief->best_range = 1;
    monster_squad_prepare();
    assert(first->squad.commander == champion);
    delete_monster_idx(champion); monster_squad_prepare();
    assert(first->squad.role && mon_list[follower].squad.role);
    int commander = first->squad.commander;
    assert(r_info[mon_list[commander].r_idx].command_grade == MON_COMMAND_ORDINARY);
    /* Execute a commander-ordered withdrawal through real get/make/process_move. */
    monster_type* m = adaptive_fixture(91, 0);
    assert(place_monster_one(9, 9, 41, false, true, NULL));
    for (int i = 1; i < mon_max; ++i) if (mon_list[i].r_idx) {
        monster_type* n = &mon_list[i]; n->alertness = ALERTNESS_ALERT;
        n->skip_this_turn = n->skip_next_turn = false; n->stance = STANCE_AGGRESSIVE;
        n->min_range = n->best_range = 1; n->hp = MAX(1, n->maxhp / 4);
    }
    ++playerturn; monster_squad_prepare();
    assert(m->squad.plan == MON_PLAN_REGROUP);
    int y, x; bool fear = false, bash = false;
    assert(get_move(m, &y, &x, &fear, false));
    assert(distance(y, x, p_ptr->py, p_ptr->px) > 1);
    assert(make_move(m, &y, &x, fear, &bash)); process_move(m, y, x, bash);
    assert(distance(m->fy, m->fx, p_ptr->py, p_ptr->px) > 1);
    puts("Ordinary orc soldier scheduler coordination, champion succession and real commander retreat movement: PASS.");
}

static void check_protected_terrain_movement(void)
{
    monster_type* m = adaptive_fixture(403, 0);
    monster_race saved = r_info[403];
    r_info[403].flags2 = RF2_SMART | RF2_KNOCK_BACK;
    int ordinary = monster_melee_utility(m, 0, true, false);
    cave_set_feat(10, 12, FEAT_LAVA);
    assert(monster_melee_utility(m, 0, true, false) > ordinary);
    cave_set_feat(10, 12, FEAT_FLOOR);
    r_info[403].flags2 = RF2_SMART;
    r_info[403].flags3 = RF3_RES_FIRE;
    m->stance = STANCE_AGGRESSIVE;
    m->min_range = m->best_range = 1;
    cave_set_feat(9, 10, FEAT_LAVA);
    int y, x;
    bool fear = false;
    assert(get_move(m, &y, &x, &fear, false));
    assert(y == 9 && x == 10 && !fear);
    bool bash = false;
    assert(make_move(m, &y, &x, fear, &bash));
    process_move(m, y, x, bash);
    assert(m->fy == 9 && m->fx == 10);
    assert(get_move(m, &y, &x, &fear, false));
    assert(y == p_ptr->py && x == p_ptr->px); /* attack from the useful footing */
    r_info[403] = saved;
    puts("Production melee values knockback into lava; movement enters protected lava footing, then resumes attacking PASS.");
}

static int disguise_notice_count(void)
{
    int count = 0;
    for (int i = 0; i < message_num(); ++i)
        if (strstr(message_str(i), "sees through your disguise")) ++count;
    return count;
}

static void check_revealing_removal_before_disguise(void)
{
    monster_type* first = adaptive_fixture(41, 0);
    first->ml = false;
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    song_revealing_clear();
    sing_song_of_revealing(1000);
    assert(first->ml);
    delete_monster_idx((int)(first - mon_list));
    compact_monsters(0);
    assert(place_monster_one(9, 10, 51, false, true, NULL));
    monster_type* newcomer = &mon_list[cave_m_idx[9][10]];
    newcomer->ml = false;
    byte attr = 0; char glyph = 0;
    assert(!song_revealing_overlay((int)(newcomer - mon_list), &attr, &glyph));
    assert(!newcomer->ml);
    song_revealing_clear();
    puts("Revealing removal clears an empty/reused slot before any Disguise buffers are needed PASS.");
}

static void check_see_invisible_identification(void)
{
    monster_type* m = adaptive_fixture(175, 0); /* Invisible cat assassin. */
    int idx = (int)(m - mon_list);
    object_type* helm = &inventory[INVEN_HEAD];
    object_prep(helm, lookup_kind(TV_HELM, SV_HELM));
    object_set_ego_suffix(helm, 73); /* of true Sight */
    assert(!object_known_p(helm));
    m->ml = false;
    m->mflag &= ~MFLAG_VIEW;
    p_ptr->see_inv = 1;
    p_ptr->blind = 1;
    assert(detect_monsters(20) && m->ml);
    assert(!(helm->ident & IDENT_EXPERIENCED));
    p_ptr->blind = 0;
    p_ptr->skill_use[S_PER] = 100;
    cave_info[m->fy][m->fx] |= CAVE_VIEW | CAVE_SEEN;
    update_mon(idx, false);
    assert((m->mflag & MFLAG_VIEW) && (helm->ident & IDENT_EXPERIENCED));
    puts("True Sight equipment is recognized on seeing an invisible creature, including one already detected.");
}

static void check_revealing_visibility(void)
{
    monster_type* m = adaptive_fixture(41, 0);
    int idx = (int)(m - mon_list);
    int old_wx = p_ptr->wx;
    p_ptr->wx = 32;  /* Keep the detected orc outside the main viewport. */
    cave_info[m->fy][m->fx] = 0;
    m->ml = false;
    song_revealing_clear();
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    sing_song_of_revealing(1000);
    assert(m->ml);
    update_mon(idx, false);
    bool before_draw = m->ml;
    byte a, ta;
    char c, tc;
    map_info(m->fy, m->fx, &a, &c, &ta, &tc);
    printf("Strong Revealing detection: offscreen %d, after drawing %d.\n", before_draw, m->ml);
    assert(before_draw && m->ml && !(m->mflag & MFLAG_VIEW));
    assert(!m->encountered);

    /* A maximum-strength result decays from 30 to 9, then to zero. */
    for (int i = 0; i < 7; i++) song_revealing_decay();
    assert(!m->ml && song_revealing_overlay(idx, &a, &c));
    for (int i = 0; i < 3; i++) song_revealing_decay();
    assert(!m->ml && !song_revealing_overlay(idx, &a, &c));

    sing_song_of_revealing(1000);
    song_revealing_clear();
    assert(!m->ml && !song_revealing_overlay(idx, &a, &c));
    sing_song_of_revealing(1000);
    m->mflag |= MFLAG_MARK; /* An independently active detection still applies. */
    song_revealing_clear();
    assert(m->ml);
    m->mflag &= ~MFLAG_MARK;
    update_mon(idx, false);
    assert(!m->ml);
    p_ptr->wx = old_wx;
    puts("Revealing visibility and hint expiry are independent of redraws; other detections survive.");
}

static void check_light_morale_perception(void)
{
    monster_type* m = adaptive_fixture(51, 0);
    bool saved_recognition = visual_recognition;
    visual_recognition = false;
    assert(r_info[m->r_idx].flags3 & RF3_HURT_LITE);
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->px = 15;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    m->cdis = distance(m->fy, m->fx, p_ptr->py, p_ptr->px);
    cave_set_feat(10,12,FEAT_WALL_PERM);
    assert(!monster_ai_can_see_player(m));
    cave_light[p_ptr->py][p_ptr->px] = 3;
    calc_morale(m);
    int hidden_dim = m->morale;
    cave_light[p_ptr->py][p_ptr->px] = 9;
    calc_morale(m);
    assert(m->morale == hidden_dim);
    cave_set_feat(10,12,FEAT_FLOOR);
    assert(monster_ai_can_see_player(m));
    cave_light[p_ptr->py][p_ptr->px] = 3;
    calc_morale(m);
    int visible_dim = m->morale;
    cave_light[p_ptr->py][p_ptr->px] = 9;
    calc_morale(m);
    assert(m->morale == visible_dim - 60);
    visual_recognition = true;
    p_ptr->cur_light = 0;
    cave_info[p_ptr->py][p_ptr->px] &= ~CAVE_GLOW;
    assert(monster_ai_can_see_player(m) && !monster_has_visual_perception_bonus(m));
    calc_morale(m);
    assert(m->morale == visible_dim - 60);
    visual_recognition = false;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->px = 40;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    cave_light[p_ptr->py][p_ptr->px] = 9;
    assert(!monster_ai_can_see_player(m));
    calc_morale(m);
    assert(m->morale == hidden_dim);
    visual_recognition = saved_recognition;
    puts("Light morale requires physical sight; walls and range block remote fear while stealth recognition does not blind an alerted monster PASS.");
}

static void check_disguise_observation_range(void)
{
    monster_type* m = adaptive_fixture(51, 0);
    bool saved_recognition = visual_recognition;
    visual_recognition = false;
    assert(song_disguise_any_monster_observes_player());
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->px = 40;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    assert(los(m->fy, m->fx, p_ptr->py, p_ptr->px));
    assert(!monster_has_sight(m));
    assert(!song_disguise_any_monster_observes_player());
    /* An alerted observer still watches in darkness; recognition only affects
     * the stealth roll, not whether it witnesses the start of Disguise. */
    visual_recognition = true;
    p_ptr->cur_light = 0;
    int range = monster_sight_range(m);
    assert(range == MAX_SIGHT);
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->px = m->fx + 5;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    assert(!monster_has_visual_perception_bonus(m));
    assert(song_disguise_any_monster_observes_player());
    visual_recognition = false;
    u32b saved_flags = r_info[m->r_idx].flags2;
    r_info[m->r_idx].flags2 |= RF2_SHORT_SIGHTED;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->px = m->fx + 3;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    assert(!song_disguise_any_monster_observes_player());
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    --p_ptr->px;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    assert(song_disguise_any_monster_observes_player());
    m->alertness = ALERTNESS_UNWARY;
    assert(!song_disguise_any_monster_observes_player());
    r_info[m->r_idx].flags2 = saved_flags;
    visual_recognition = saved_recognition;
    puts("Disguise observation obeys physical MAX_SIGHT, walls, short sight and alertness independently of stealth recognition PASS.");
}

static void check_song_tracking_compaction(void)
{
    for (int i = 1; i < MAX_MONSTERS; ++i)
        song_disguise_handle_monster_removed(i);
    monster_type* first = adaptive_fixture(41, 0);
    assert(place_monster_one(9, 10, 51, false, true, NULL));
    monster_type* survivor = &mon_list[cave_m_idx[9][10]];
    survivor->alertness = ALERTNESS_ALERT;
    p_ptr->song1 = SNG_DISGUISE;
    song_disguise_start();
    sing_song_of_disguise(1000);
    assert(song_disguise_monster_is_fooled(first));
    assert(song_disguise_monster_is_fooled(survivor));
    delete_monster_idx((int)(first - mon_list));
    compact_monsters(0);
    assert(mon_list[1].r_idx == 51);
    assert(song_disguise_monster_is_fooled(&mon_list[1]));
    assert(place_monster_one(8, 12, 51, false, true, NULL));
    monster_type* newcomer = &mon_list[cave_m_idx[8][12]];
    newcomer->alertness = ALERTNESS_ALERT;
    assert(!song_disguise_monster_is_fooled(newcomer));
    song_disguise_stop();
    for (int i = 1; i < mon_max; ++i)
        if (mon_list[i].r_idx) delete_monster_idx(i);

    first = adaptive_fixture(41, 0);
    assert(place_monster_one(9, 10, 51, false, true, NULL));
    mon_list[cave_m_idx[9][10]].alertness = ALERTNESS_ALERT;
    p_ptr->song1 = SNG_DISGUISE;
    song_disguise_start();
    sing_song_of_disguise(-1000);
    delete_monster_idx((int)(first - mon_list));
    compact_monsters(0);
    msg_print("Disguise compaction test boundary.");
    int notices = disguise_notice_count();
    sing_song_of_disguise(-1000);
    assert(disguise_notice_count() == notices);
    song_disguise_stop();
    for (int i = 1; i < mon_max; ++i)
        if (mon_list[i].r_idx) delete_monster_idx(i);

    first = adaptive_fixture(41, 0);
    assert(place_monster_one(9, 10, 175, false, true, NULL));
    survivor = &mon_list[cave_m_idx[9][10]];
    first->ml = survivor->ml = false;
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    song_revealing_clear();
    sing_song_of_revealing(1000);
    assert(survivor->ml);
    survivor->ml = false;
    byte attr = 0; char glyph = 0;
    assert(!song_revealing_overlay((int)(survivor - mon_list), &attr, &glyph));
    assert(!survivor->ml);
    update_mon((int)(survivor - mon_list), false);
    assert(survivor->ml);
    delete_monster_idx((int)(first - mon_list));
    compact_monsters(0);
    assert(mon_list[1].r_idx == 175);
    update_mon(1, false);
    assert(!song_revealing_overlay(1, &attr, &glyph) && mon_list[1].ml);
    song_revealing_clear();
    p_ptr->song1 = SNG_NOTHING;
    puts("Disguise pacification/recognition and Revealing hints follow real monster compaction; empty slots cannot inherit them PASS.");
}

static void check_archer_vacated_square(void)
{
    monster_type* m = adaptive_fixture(103, 0);
    bool saved_recognition = visual_recognition;
    /* Isolate the projection preview at full sight. Recognition-bound posts
     * are covered independently below and must not invalidate this retreat. */
    visual_recognition = false;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->px = 15;
    cave_m_idx[10][15] = -1;
    for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x) {
        cave_set_feat(9, x, FEAT_WALL_PERM);
        cave_set_feat(11, x, FEAT_WALL_PERM);
    }
    m->cdis = 5; m->min_range = 7; m->best_range = 8;
    m->stance = STANCE_AGGRESSIVE;
    assert(projectable(10, 9, 10, 15, PROJECT_CHCK) == PROJECT_NOT_CLEAR);
    /* A candidate retreat must ignore its own old square, while restoring
     * the enclosing caller's projection state and leaving the world pure. */
    project_path_ignore = true;
    project_path_ignore_y = 4; project_path_ignore_x = 4;
    monster_type before = *m;
    u64b rng = Rand_state_export();
    int utility = monster_tactical_move_utility(m);
    assert(utility > 0 && monster_tactical_move_utility(m) == utility);
    assert(!memcmp(m, &before, sizeof(before)) && Rand_state_export() == rng);
    assert(project_path_ignore && project_path_ignore_y == 4 && project_path_ignore_x == 4);
    assert(cave_m_idx[10][10] == (int)(m - mon_list) && cave_m_idx[10][9] == 0);
    project_path_ignore = false;
    project_path_ignore_y = project_path_ignore_x = 0;
    int y, x;
    bool bash = false, fear = false;
    assert(get_move(m, &y, &x, &fear, false) && y == 10 && x == 9);
    assert(make_move(m, &y, &x, false, &bash));
    process_move(m, y, x, bash);
    assert(projectable(m->fy, m->fx, 10, 15, PROJECT_CHCK) == PROJECT_CLEAR);
    visual_recognition = saved_recognition;
    puts("Real corridor archer retreats through its vacated firing line; previews preserve map, actor, RNG and nested projection state PASS.");
}

static void check_archer_crowded_detour(void)
{
    monster_type* m = adaptive_fixture(51, 0);
    bool saved_recognition = visual_recognition;
    visual_recognition = false;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->py = p_ptr->px = 9;
    cave_m_idx[9][9] = -1;
    int idx = (int)(m - mon_list);
    cave_m_idx[m->fy][m->fx] = 0;
    m->fy = 11; m->fx = 8; cave_m_idx[11][8] = idx;
    m->min_range = 2; m->best_range = 3; m->cdis = 2;
    m->stance = STANCE_AGGRESSIVE;
    const int allies[][2] = {{8,10},{9,8},{10,8},{10,9},{9,10},{10,10}};
    for (int i = 0; i < 6; ++i)
    {
        assert(place_monster_one(allies[i][0], allies[i][1], 61, false, true, NULL));
        mon_list[cave_m_idx[allies[i][0]][allies[i][1]]].alertness = ALERTNESS_ALERT;
    }
    assert(projectable(m->fy, m->fx, 9, 9, PROJECT_CHCK) == PROJECT_NOT_CLEAR);
    monster_type before = *m;
    u64b rng = Rand_state_export();
    int utility = monster_tactical_move_utility(m);
    assert(utility > 0 && monster_tactical_move_utility(m) == utility);
    assert(!memcmp(m, &before, sizeof(before)) && Rand_state_export() == rng);
    int moves = 0;
    while (projectable(m->fy, m->fx, 9, 9, PROJECT_CHCK) != PROJECT_CLEAR && moves < 12)
    {
        int y, x, old_y = m->fy, old_x = m->fx;
        bool bash = false, fear = false;
        assert(get_move(m, &y, &x, &fear, false));
        assert(distance(old_y, old_x, y, x) == 1 && !cave_m_idx[y][x]);
        assert(make_move(m, &y, &x, false, &bash));
        process_move(m, y, x, bash);
        monster_ai_end_turn(m, old_y, old_x, false);
        ++playerturn; ++moves;
    }
    assert(moves > 2 && moves < 12 && distance(m->fy, m->fx, 9, 9) >= 2);
    assert(projectable(m->fy, m->fx, 9, 9, PROJECT_CHCK) == PROJECT_CLEAR);
    visual_recognition = saved_recognition;
    puts("Real blocked archer walks around six contact fighters to a clear lane; preview preserves actor and RNG PASS.");
}

static void check_archer_recognition_range(void)
{
    monster_type* m = adaptive_fixture(51, 0);
    bool saved_recognition = visual_recognition;
    visual_recognition = true;
    p_ptr->cur_light = 1;
    cave_info[p_ptr->py][p_ptr->px] |= CAVE_GLOW;
    int vision = monster_skill(m, S_PER) / 2 + p_ptr->cur_light + 2;
    assert(vision >= 2 && vision < 8 && monster_ai_can_see_player(m));
    find_range(m);
    assert(m->best_range == 8 && m->min_range == 7);
    visual_recognition = false;
    find_range(m);
    assert(m->best_range == 8 && m->min_range == 7);
    visual_recognition = saved_recognition;
    puts("Stealth recognition setting does not shorten an alerted archer's usable combat range PASS.");
}

static void check_archer_dim_contact(void)
{
    bool saved_recognition = visual_recognition;
    visual_recognition = true;
    for (int wrath = 0; wrath < 2; ++wrath)
    {
        monster_type* m = adaptive_fixture(51, 0);
        p_ptr->cur_light = 1;
        cave_info[p_ptr->py][p_ptr->px] &= ~CAVE_GLOW;
        p_ptr->aggravate = wrath;
        m->tmp_morale = 100;
        calc_morale(m); calc_stance(m);
        playerturn = 100;
        assert(monster_sight_range(m) == MAX_SIGHT);
        m->mana = 0; /* Arrows cost no mana, including after Challenge. */
        find_range(m);
        assert(m->best_range == 8 && m->min_range == 7);
        /* The direct southwest step is the only retreat. Its projection
         * crosses the archer's vacated square and retains the known player. */
        const int blocks[][2] = {{9,9},{9,10},{9,11},{10,9},{11,11}};
        for (int i = 0; i < (int)N_ELEMENTS(blocks); ++i)
            cave_set_feat(blocks[i][0], blocks[i][1], FEAT_WALL_PERM);
        monster_type before = *m;
        u64b rng = Rand_state_export();
        int utility = monster_tactical_move_utility(m);
        assert(utility > 0 && monster_tactical_move_utility(m) == utility);
        assert(!memcmp(&before, m, sizeof(before)) && Rand_state_export() == rng);
        int y, x; bool fear = false, bash = false;
        assert(get_move(m, &y, &x, &fear, false) && y == 11 && x == 9);
        assert(make_move(m, &y, &x, false, &bash));
        process_move(m, y, x, bash);
        assert(monster_has_sight(m) && monster_ranged_attack_legal(m, 96));
        /* Light loss does not blind an alerted archer; a wall still blocks it. */
        p_ptr->cur_light = 0;
        assert(monster_has_sight(m) && !monster_has_visual_perception_bonus(m));
        p_ptr->cur_light = 1;
        cave_set_feat(10, 10, FEAT_WALL_PERM);
        cave_set_feat(11, 10, FEAT_WALL_PERM);
        assert(!monster_has_sight(m));
        cave_set_feat(10, 10, FEAT_FLOOR);
        cave_set_feat(11, 10, FEAT_FLOOR);
        playerturn += 2;
        assert(monster_has_sight(m));
        m->ai.sense.observed_turn = playerturn;
        m->ai.sense.kind = MON_SENSE_SOUND;
        assert(monster_has_sight(m));
        m->ai.sense.kind = MON_SENSE_SHARED_TRACE;
        assert(monster_has_sight(m));
        m->alertness = ALERTNESS_UNWARY;
        memset(&m->ai.sense, 0, sizeof(m->ai.sense));
        monster_senses_refresh(m);
        assert(!monster_ai_can_see_player(m) && m->ai.sense.kind == MON_SENSE_NONE);
    }
    visual_recognition = saved_recognition;
    puts("Dim-light archers take a direct diagonal retreat and shoot without Wrath/mana dependence; physical LOS and alertness still gate targeting and previews are pure PASS.");
}

static void check_all_monster_recognition(void)
{
    bool saved_recognition = visual_recognition;
    monster_type* m = adaptive_fixture(51, 0);
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->px = 15;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    m->cdis = 5;
    p_ptr->cur_light = 0;
    cave_info[p_ptr->py][p_ptr->px] &= ~CAVE_GLOW;
    int count = 0;
    for (int race = 1; race < z_info->r_max; ++race)
    {
        monster_race* r = &r_info[race];
        if (!r->name) continue;
        m->r_idx = race; m->mana = MON_MANA_MAX;
        m->alertness = ALERTNESS_ALERT; m->stance = STANCE_CONFIDENT;
        memset(&m->ai, 0, sizeof(m->ai));
        bool physical = !(r->flags2 & RF2_SHORT_SIGHTED);
        visual_recognition = true;
        assert(monster_ai_can_see_player(m) == physical);
        u32b legal = 0;
        for (int bit = 0; bit <= 24; ++bit)
            if (monster_ranged_attack_legal(m, 96 + bit)) legal |= 1UL << bit;
        find_range(m);
        int minimum = m->min_range, preferred = m->best_range;
        for (int disguise = 0; disguise <= 1; ++disguise)
        {
            p_ptr->active_ability[S_STL][STL_DISGUISE] = disguise;
            int vision = monster_skill(m, S_PER) / (disguise ? 4 : 2);
            bool expected = physical && (monster_race_is_vala(race)
                || !(r->flags2 & RF2_SMART) || vision >= 5);
            assert(monster_has_visual_perception_bonus(m) == expected);
            assert(monster_ai_can_see_player(m) == physical);
        }
        p_ptr->active_ability[S_STL][STL_DISGUISE] = false;
        visual_recognition = false;
        assert(monster_has_visual_perception_bonus(m) == physical);
        assert(monster_ai_can_see_player(m) == physical);
        for (int bit = 0; bit <= 24; ++bit)
            assert(monster_ranged_attack_legal(m, 96 + bit) == !!(legal & (1UL << bit)));
        find_range(m);
        assert(m->min_range == minimum && m->best_range == preferred);
        m->alertness = ALERTNESS_UNWARY;
        monster_senses_refresh(m);
        assert(!monster_ai_can_see_player(m) && m->ai.sense.kind != MON_SENSE_SIGHT);
        ++count;
    }
    /* Real perception rolls retain the visual bonus; recognizing a quiet
     * character is not itself a successful detection or target acquisition. */
    int alertness[2];
    for (int setting = 0; setting < 2; ++setting)
    {
        m = adaptive_fixture(51, 0);
        p_ptr->cur_light = 0;
        cave_info[p_ptr->py][p_ptr->px] &= ~CAVE_GLOW;
        m->alertness = ALERTNESS_UNWARY;
        visual_recognition = setting;
        playerturn = 100;
        update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
        Rand_state_init(1701);
        monster_perception(true, false, -10);
        alertness[setting] = m->alertness;
        m->alertness = ALERTNESS_UNWARY;
        memset(&m->ai, 0, sizeof(m->ai));
        monster_perception(true, false, 1000);
        monster_senses_refresh(m);
        assert(m->alertness == ALERTNESS_UNWARY && m->ai.sense.kind == MON_SENSE_NONE);
    }
    assert(alertness[0] - alertness[1] == 8); /* Open-floor visual bonus. */
    visual_recognition = saved_recognition;
    printf("All %d named monster templates: recognition/Disguise retain stealth bonuses without altering physical sight, combat ranges or legal spells; failed detection never acquires a target PASS.\n", count);
}

static void check_deep_water_archer_escape(void)
{
    bool saved_recognition = visual_recognition;
    visual_recognition = false;
    for (int race = 0; race < 2; ++race)
    {
        monster_type* m = adaptive_fixture(race ? 103 : 51, 0);
        cave_m_idx[p_ptr->py][p_ptr->px] = 0;
        p_ptr->px = 13;
        cave_m_idx[10][13] = -1;
        m->cdis = 3;
        m->stance = STANCE_AGGRESSIVE;
        m->tmp_morale = 1000;
        /* A flooded corridor has no immediately dry adjacent square. */
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
            for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                if (y != 10) cave_set_feat(y, x, FEAT_WALL_PERM);
        for (int x = 9; x <= 11; ++x) cave_set_feat(10, x, FEAT_DEEP_WATER);
        m->min_range = 7; m->best_range = 8;
        for (int step = 0; step < 3 && cave_feat[m->fy][m->fx] == FEAT_DEEP_WATER; ++step)
        {
            m->energy = 100;
            process_monsters(100);
            assert(m->energy <= -300); /* The real 400% water movement cost. */
            ++playerturn;
        }
        assert(cave_feat[m->fy][m->fx] != FEAT_DEEP_WATER);
        assert(m->fy == 10 && ABS((int)m->fx - 10) == 2);
    }
    visual_recognition = saved_recognition;
    puts("Flooded-corridor archers take paid water steps to shore before choosing firing posts PASS.");
}

static void check_archer_dry_shore_contact(void)
{
    bool saved_recognition = visual_recognition;
    visual_recognition = true;
    const int races[] = {51, 103, 115};
    for (int race = 0; race < (int)N_ELEMENTS(races); ++race)
    for (int visible = 0; visible < 2; ++visible)
    {
        monster_type* m = adaptive_fixture(races[race], 0);
        p_ptr->cur_light = 0;
        cave_info[p_ptr->py][p_ptr->px] &= ~CAVE_GLOW;
        for (int y = 1; y < p_ptr->cur_map_hgt - 1; ++y)
            for (int x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                if (y != 10) cave_set_feat(y, x, FEAT_WALL_PERM);
        for (int x = 7; x <= 9; ++x) cave_set_feat(10, x, FEAT_DEEP_WATER);
        m->ml = visible; m->stance = STANCE_AGGRESSIVE;
        m->min_range = 7; m->best_range = 8;
        assert(monster_ai_can_see_player(m));
        monster_type before = *m;
        u64b rng = Rand_state_export();
        assert(monster_tactical_move_utility(m) == 0);
        assert(!memcmp(m, &before, sizeof(before)) && Rand_state_export() == rng);
        int y, x; bool fear = false;
        assert(get_move(m, &y, &x, &fear, false));
        assert(y == p_ptr->py && x == p_ptr->px && !fear);
        assert(cave_feat[m->fy][m->fx] == FEAT_FLOOR);
    }
    visual_recognition = saved_recognition;
    puts("Healthy bows on a dry contact square fight instead of repeatedly re-entering water; ordinary/unique and visible/hidden actors preserve pure previews PASS.");
}

static void check_challenge_melee_range(void)
{
    monster_type* m = adaptive_fixture(51, 0);
    p_ptr->song1 = SNG_CHALLENGE;
    p_ptr->skill_use[S_SNG] = 0;
    m->stance = STANCE_AGGRESSIVE;
    int drained = 0, resisted = 0;
    for (int i = 0; i < 100; ++i)
    {
        m->mana = MON_MANA_MAX;
        m->min_range = 2; m->best_range = 3;
        find_range(m);
        assert(monster_ai_challenge_melee(m));
        if (m->mana == 0)
        {
            ++drained;
            assert(m->min_range == 0 && m->best_range == 0);
        }
        else
        {
            ++resisted;
            assert(m->min_range == 1 && m->best_range == 1);
        }
    }
    assert(resisted > 0);
    p_ptr->song1 = SNG_NOTHING;
    m->mana = MON_MANA_MAX;
    find_range(m);
    assert(!monster_ai_challenge_melee(m) && m->best_range > 1);
    printf("Real Challenge range: %d mana-drain successes, %d resisted checks retain melee approach; stopping restores bow spacing PASS.\n",
        drained, resisted);
}

static void check_squad_idle_scheduler_budget(void)
{
    monster_type* leader = adaptive_fixture(91, 0);
    bool saved_recognition = visual_recognition;
    visual_recognition = false;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->px = 17;
    cave_m_idx[p_ptr->py][p_ptr->px] = -1;
    assert(place_monster_one(9, 13, 41, false, true, NULL));
    assert(place_monster_one(10, 7, 61, false, true, NULL));
    for (int i = 1; i < mon_max; ++i)
    {
        monster_type* m = &mon_list[i];
        m->alertness = ALERTNESS_ALERT;
        m->stance = STANCE_CONFIDENT;
        m->skip_this_turn = m->skip_next_turn = false;
        m->min_range = m->best_range = 1;
        m->energy = 0;
        m->cdis = distance(m->fy, m->fx, p_ptr->py, p_ptr->px);
    }
    assert(monster_ai_can_see_player(leader));
    /* Empty pre/post-player scheduler polls must not use up the commander's
     * finite assembly window before the squad gets its first actual action. */
    playerturn = 100;
    for (int i = 0; i < 3; ++i)
    {
        process_monsters(100);
        ++playerturn;
    }
    for (int i = 1; i < mon_max; ++i) mon_list[i].energy = 100;
    process_monsters(100);
    assert(leader->squad.plan == MON_PLAN_HOLD && leader->squad.plan_age == 0);
    /* The commander's own withdrawal/lost sight must not renew this same
     * persistent assembly condition when it recognizes the player again. */
    cave_set_feat(10, 14, FEAT_WALL_PERM);
    assert(!monster_ai_can_see_player(leader));
    ++playerturn;
    monster_squad_prepare();
    playerturn += 3;
    cave_set_feat(10, 14, FEAT_FLOOR);
    assert(monster_ai_can_see_player(leader));
    monster_squad_prepare();
    assert(leader->squad.plan == MON_PLAN_ADVANCE);
    visual_recognition = saved_recognition;
    puts("Command budgets start with real actions and cannot renew through lost sight/reacquisition PASS.");
}

static void check_cooperative_movement(void)
{
    monster_type* m = adaptive_fixture(403, 0);
    monster_race saved = r_info[403];
    r_info[403].flags2 = RF2_SMART;
    r_info[403].flags3 = RF3_ORC;
    int archer = 0;
    for (int i = 1; i < z_info->r_max; ++i)
        if ((r_info[i].flags3 & RF3_ORC) && (r_info[i].flags4 & RF4_ARROW1))
        { archer = i; break; }
    assert(archer && place_monster_one(10, 8, archer, false, true, NULL));
    monster_type* ally = &mon_list[cave_m_idx[10][8]];
    ally->alertness = ALERTNESS_ALERT; ally->ml = true;
    ally->stance = STANCE_AGGRESSIVE;
    ally->min_range = 2; ally->best_range = 3;
    m->stance = STANCE_AGGRESSIVE;
    m->min_range = m->best_range = 1;
    playerturn = 600;
    monster_ai_begin_turn(m);
    int y, x;
    bool fear = false, bash = false;
    assert(get_move(m, &y, &x, &fear, false));
    assert(m->ai.feedback_reason == MON_TACTIC_GUARD);
    cave_info[y][x] |= CAVE_SEEN; /* Fixture lighting for production update_mon(). */
    assert(make_move(m, &y, &x, fear, &bash));
    process_move(m, y, x, bash);
    monster_ai_end_turn(m, 10, 10, false);
    assert(m->ai.feedback_cooldown == 8 && !m->ai.feedback_reason);
    assert(projectable(ally->fy, ally->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK)
        == PROJECT_CLEAR);
    assert(distance(m->fy, m->fx, p_ptr->py, p_ptr->px) == 1);
    r_info[403] = saved;
    puts("Production guard movement clears an orc archer's firing lane and reports only the completed move PASS.");

    m = adaptive_fixture(archer, 0);
    cave_m_idx[10][11] = 0;
    p_ptr->px = 13; cave_m_idx[10][13] = -1;
    m->cdis = 3; m->min_range = 2; m->best_range = 3;
    m->stance = STANCE_AGGRESSIVE;
    p_ptr->song1 = SNG_LORIEN;
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);
    ++playerturn;
    monster_ai_begin_turn(m);
    assert(get_move(m, &y, &x, &fear, false));
    assert(m->ai.feedback_reason == MON_TACTIC_SONG_DISTANCE);
    cave_info[y][x] |= CAVE_SEEN;
    assert(make_move(m, &y, &x, fear, &bash));
    process_move(m, y, x, bash);
    monster_ai_end_turn(m, 10, 10, false);
    assert(m->ai.feedback_cooldown == 8);
    assert(flow_dist(FLOW_PLAYER_NOISE, m->fy, m->fx) > 3);
    assert(projectable(m->fy, m->fx, p_ptr->py, p_ptr->px, PROJECT_CHCK));
    puts("Production archer buys sound distance from Lorien while retaining a shot PASS.");
}

static void check_visible_unarmed_disarm_utility(void)
{
    monster_type* m = adaptive_fixture(406, 0);
    bool saved_recognition = visual_recognition;
    visual_recognition = false;
    cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    p_ptr->py = 10; p_ptr->px = 13;
    cave_m_idx[10][13] = -1;
    m->cdis = distance(m->fy, m->fx, 10, 13);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    assert(monster_ai_can_see_player(m));
    int armed = monster_melee_utility(m, 1, true, false);
    assert(armed > 0);
    object_wipe(&inventory[INVEN_WIELD]);
    assert(monster_melee_utility(m, 1, true, false) == 0);
    m->blow_ds_reduction[0] = r_info[406].blow[0].ds - 1;
    assert(monster_melee_utility(m, 0, true, false) > 0);

    cave_set_feat(10, 11, FEAT_WALL_PERM);
    assert(!monster_ai_can_see_player(m));
    int unseen_empty = monster_melee_utility(m, 1, true, false);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    assert(monster_melee_utility(m, 1, true, false) == unseen_empty);
    cave_set_feat(10, 11, FEAT_FLOOR);
    assert(monster_melee_utility(m, 1, true, false) == armed);

    m = adaptive_fixture(241, 0);
    object_prep(&inventory[INVEN_WIELD], lookup_kind(TV_SWORD, SV_LONG_SWORD));
    assert(monster_ai_can_see_player(m));
    assert(monster_melee_utility(m, 1, true, false) > 0);
    object_wipe(&inventory[INVEN_WIELD]);
    assert(monster_melee_utility(m, 1, true, false) == 0);
    visual_recognition = saved_recognition;
    puts("Disarm utility: visible empty hand has no value, rearming restores it, unseen equipment stays private, whip rolls cannot supply phantom damage PASS.");
}

static void check_adaptive_integration(void)
{
    monster_type* m = adaptive_fixture(403, 0);
    m->hp = m->maxhp = 20000;
    p_ptr->skill_use[S_MEL] = 100;
    p_ptr->mdd = p_ptr->mds = 1;
    playerturn = 100;
    py_attack_aux(10, 10, ATT_FLANKING);
    assert(monster_ai_confidence(m, MON_AI_FLANKING) == 3);
    playerturn++;
    py_attack_aux(10, 10, ATT_RIPOSTE);
    assert(monster_ai_confidence(m, MON_AI_RIPOSTE) == 3);
    puts("Production player attacks teach only actual Flanking/Riposte reactions PASS.");

    int dragon = 0;
    for (int i = 1; i < z_info->r_max; ++i)
        if ((r_info[i].flags3 & RF3_DRAGON)
            && (r_info[i].flags4 & RF4_BRTH_FIRE)) { dragon = i; break; }
    assert(dragon);
    m = adaptive_fixture(dragon, 0);
    p_ptr->resist_fire = 3;
    p_ptr->cur_light = 4;
    for (int i = 0; i < 3; ++i) {
        playerturn++;
        assert(project_p(cave_m_idx[10][10], 10, 11, 10, 10, 0, GF_FIRE));
    }
    assert(monster_ai_confidence(m, MON_AI_FIRE) == 3);
    int learned = monster_ranged_utility(m, 96 + 3);
    monster_ai_reset(m);
    assert(monster_ranged_utility(m, 96 + 3) > learned);
    p_ptr->resist_fire = 1;
    m->ml = false; /* The player seeing the monster is not a learning gate. */
    playerturn++;
    assert(project_p(cave_m_idx[10][10], 10, 11, 10, 10, 0, GF_FIRE));
    assert(monster_ai_confidence(m, MON_AI_FIRE) == -1);
    puts("Production fire projection teaches coarse witnessed resistance and changes breath utility PASS.");

    m = adaptive_fixture(403, 0);
    int hp = m->hp;
    m->alertness = ALERTNESS_MIN;
    m->poisoned = 10;
    assert(!monster_poison_tick(cave_m_idx[10][10]));
    assert(m->hp == hp - 2 && m->poisoned == 8);
    assert(m->alertness >= ALERTNESS_ALERT);
    m->hp = 1; m->poisoned = 10;
    int index = cave_m_idx[10][10];
    assert(monster_poison_tick(index));
    assert(!mon_list[index].r_idx && !cave_m_idx[10][10]);
    puts("Real mon_take_hit poison route wakes sleeping creatures and removes lethal victims PASS.");

    m = adaptive_fixture(411, 0);
    m->hp = m->maxhp = 100;
    m->poisoned = 6;
    cave_feat[10][10] = FEAT_POISON;
    m->smite_recovery = 1; m->skip_next_turn = true;
    m->energy = 100;
    process_monsters(100);
    int recovery_tick = (6 + POISON_TERRAIN_DOSE + 4) / 5;
    assert(m->hp == 100 - recovery_tick);
    assert(m->poisoned == 6 + POISON_TERRAIN_DOSE - recovery_tick);
    assert(m->smite_recovery == 2 && m->fy == 10 && m->fx == 10);
    assert(!make_attack_reaction(m));
    m->energy = 100;
    process_monsters(100);
    assert(m->hp <= 94 && cave_feat[m->fy][m->fx] != FEAT_POISON);
    puts("Real scheduler: pool dose and poison tick run during Smite recovery, followed by escape PASS.");

    m = adaptive_fixture(407, 0);
    hp = m->hp;
    m->poisoned = 10; m->skip_next_turn = true;
    cave_feat[10][10] = FEAT_POISON;
    m->energy = 100;
    process_monsters(100);
    assert(m->hp == hp - 2 && m->poisoned == 8);
    puts("Flying poison-resistant creature avoids pool dose but retains previously applied poison PASS.");

    m = adaptive_fixture(403, 0);
    delete_monster_idx(cave_m_idx[m->fy][m->fx]);
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->resist_pois = 1;
    p_ptr->restoring = true; /* Suppress between-turn presentation only. */
    p_ptr->entranced = 1; p_ptr->energy = 100;
    cave_feat[p_ptr->py][p_ptr->px] = FEAT_POISON;
    playerturn = 200;
    process_player();
    assert(playerturn == 201 && p_ptr->energy == 0);
    int contact_tick = (POISON_TERRAIN_DOSE + 4) / 5;
    assert(p_ptr->chp == 100 - contact_tick);
    assert(p_ptr->poisoned == POISON_TERRAIN_DOSE - contact_tick);
    p_ptr->restoring = true; p_ptr->entranced = 1; p_ptr->energy = 100;
    cave_feat[p_ptr->py][p_ptr->px] = FEAT_FLOOR;
    process_player();
    int bank_tick = (POISON_TERRAIN_DOSE - contact_tick + 4) / 5;
    assert(p_ptr->chp == 100 - contact_tick - bank_tick);
    assert(p_ptr->poisoned == POISON_TERRAIN_DOSE - contact_tick - bank_tick);
    puts("Real complete player actions apply pool contact before poison tick and suppress regeneration PASS.");
    check_weapon_fear();
    check_territorial_defense();
    check_visible_unarmed_disarm_utility();
    check_protected_terrain_movement();
    check_archer_vacated_square();
    check_archer_crowded_detour();
    check_archer_recognition_range();
    check_archer_dim_contact();
    check_all_monster_recognition();
    check_deep_water_archer_escape();
    check_archer_dry_shore_contact();
    check_challenge_melee_range();
    check_squad_idle_scheduler_budget();
    check_cooperative_movement();
    check_commanded_squad();
    check_ordinary_squad();
    check_light_morale_perception();
    check_disguise_observation_range();
    check_song_tracking_compaction();
}
'''


def main():
    engine.OUT = engine.ROOT / "scripts/output/monster-ai-integration"
    engine.HARNESS = engine.HARNESS.replace(
        "int main(int argc,char** argv)", CHECKS + "\nint main(int argc,char** argv)")
    engine.HARNESS = engine.HARNESS.replace(
        "    check_combat();",
        "    check_see_invisible_identification();\n    check_revealing_visibility();\n    check_revealing_removal_before_disguise();\n    check_combat();\n    check_adaptive_integration();")
    engine.HARNESS = engine.HARNESS.replace(
        "    check_templates();", "    check_command_profiles();\n    check_templates();")
    engine.main()


if __name__ == "__main__":
    main()
