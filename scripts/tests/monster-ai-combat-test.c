#include "angband.h"
#include "monster/monster-ai.h"
#include "monster/monster-tactics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Separate fixture translation units expose private production selectors
 * without changing their linkage or including unguarded externs.h twice. */
#if defined(COMBAT_PROCESS_SELECTOR)
#include "../../src/melee/melee-process.c"
int test_choose_ranged(int m_idx) { return choose_ranged_attack(m_idx); }
#elif defined(COMBAT_MELEE_SELECTOR)
#include "../../src/melee/melee-attack.c"
int test_choose_blow(const monster_type* m, bool ordinary)
{ return monster_choose_blow(m, ordinary); }
bool test_choose_smite(const monster_type* m, int blow, bool ordinary)
{ return monster_choose_smite(m, blow, ordinary); }
#else
#include "externs.h"
#include "melee/melee-attack.h"
int test_choose_ranged(int m_idx);
int test_choose_blow(const monster_type* m, bool ordinary);
bool test_choose_smite(const monster_type* m, int blow, bool ordinary);

static byte features[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static u16b info[MAX_DUNGEON_HGT][256];
static s16b occupants[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static monster_type monsters[4];
static monster_race races[600];
static monster_lore lore[600];
static artefact_type artifacts[512];
static int checks, random_calls;
static unsigned random_value;
static bool visible;
static byte firing_line;

#define CHECK(test) do { ++checks; if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); exit(1); } } while (0)

u32b Rand_div(u32b n) { ++random_calls; return random_value++ % n; }
bool monster_has_sight(const monster_type* m) { (void)m; return visible; }
bool singing(int song) { (void)song; return false; }
byte projectable(int y1, int x1, int y2, int x2, u32b flags)
{ (void)y1; (void)x1; (void)y2; (void)x2; (void)flags; return firing_line; }
/* Displacement terrain is covered by the tactical fixture; these combat
 * fixtures do not assign Knockback to their races. */
int monster_tactical_displacement_utility(monster_type* m, int y, int x, bool exchange)
{ (void)m; (void)y; (void)x; (void)exchange; CHECK(false); return 0; }
int flow_dist(int flow, int y, int x)
{ (void)flow; (void)y; (void)x; return 40; }
int ability_bonus(int skill, int ability) { (void)skill; (void)ability; return 0; }
int skill_check(monster_type* a, int skill, int difficulty, monster_type* b)
{ (void)a; (void)skill; (void)difficulty; (void)b; return -10; }
void monster_desc(char* out, size_t len, const monster_type* m, int mode)
{ (void)m; (void)mode; if (len) out[0] = 0; }
void msg_format(cptr fmt, ...) { (void)fmt; }
void msg_print(cptr msg) { (void)msg; }
void disturb(int stop, int unused) { (void)stop; (void)unused; }
void set_alertness(monster_type* m, int alertness) { m->alertness = alertness; }

static monster_type* setup(void)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(monsters, 0, sizeof(monsters));
    memset(races, 0, sizeof(races));
    memset(lore, 0, sizeof(lore));
    memset(artifacts, 0, sizeof(artifacts));
    memset(info, 0, sizeof(info));
    memset(occupants, 0, sizeof(occupants));
    p_ptr->cur_map_hgt = p_ptr->cur_map_wid = 25;
    p_ptr->py = 10; p_ptr->px = 12;
    for (int y = 0; y < 25; ++y)
        for (int x = 0; x < 25; ++x) features[y][x] = FEAT_FLOOR;
    cave_feat = features; cave_info = info; cave_m_idx = occupants;
    mon_list = monsters; r_info = races; l_list = lore; a_info = artifacts;
    mon_max = 3; mon_cnt = 2; playerturn = 100;
    monsters[1].r_idx = 1; monsters[1].fy = 10; monsters[1].fx = 10;
    monsters[1].hp = monsters[1].maxhp = 100;
    monsters[1].mana = 50;
    monsters[1].alertness = ALERTNESS_ALERT;
    monsters[1].stance = STANCE_AGGRESSIVE;
    monsters[1].cdis = 2;
    occupants[10][10] = 1; occupants[10][12] = -1;
    races[1].flags2 = RF2_SMART;
    races[1].spell_power = 8;
    visible = true; firing_line = PROJECT_CLEAR;
    random_value = 0; random_calls = 0;
    return &monsters[1];
}

static void test_opportunities(void)
{
    monster_type* m = setup();
    monster_ai_begin_turn(m);
    CHECK(monster_ai_casting_opportunity(m, 100));
    CHECK(m->ai.cast_reserve == 3 && random_calls == 1);
    CHECK(monster_ai_casting_opportunity(m, 100));
    CHECK(random_calls == 1);
    monster_ai_begin_turn(m);
    CHECK(monster_ai_casting_opportunity(m, 100));
    CHECK(m->ai.cast_reserve == 2 && random_calls == 2);
    monster_ai_begin_turn(m);
    CHECK(monster_ai_casting_opportunity(m, 100));
    CHECK(m->ai.cast_reserve == 1);
    monster_ai_begin_turn(m);
    random_value = 99;
    CHECK(!monster_ai_casting_opportunity(m, 1));
    CHECK(m->ai.cast_reserve == 0);
    monster_ai_begin_turn(m);
    CHECK(monster_ai_casting_opportunity(m, 100));
    monster_ai_spend_casting_opportunity(m);
    CHECK(!monster_ai_casting_opportunity(m, 100));
    CHECK(m->ai.cast_reserve == 0);
    monster_ai_begin_turn(m);
    CHECK(monster_ai_casting_opportunity(m, 100));
    m->confused = 1;
    CHECK(!monster_ai_casting_opportunity(m, 100));
    CHECK(m->ai.cast_reserve == 0);
    m = setup();
    monster_ai_begin_turn(m);
    CHECK(monster_ai_casting_opportunity(m, 100));
    int rolls = random_calls;
    /* Escape and multiplication consume eligible turns without selecting RF4. */
    monster_ai_begin_turn(m); CHECK(m->ai.cast_reserve == 2);
    monster_ai_begin_turn(m); CHECK(m->ai.cast_reserve == 1);
    monster_ai_begin_turn(m); CHECK(m->ai.cast_reserve == 0);
    CHECK(random_calls == rolls);
    m = setup(); races[1].flags2 = RF2_MINDLESS;
    monster_ai_begin_turn(m);
    CHECK(monster_ai_casting_opportunity(m, 100));
    CHECK(m->ai.cast_reserve == 0);
    m->r_idx = R_IDX_MORGOTH;
    monster_ai_begin_turn(m);
    CHECK(monster_ai_casting_opportunity(m, 100));
    CHECK(m->ai.cast_reserve == 0);
}

static void test_legality_and_payment(void)
{
    monster_type* m = setup();
    races[1].flags4 = RF4_RALLY | RF4_SHRIEK | RF4_THROW_WEB | RF4_ARROW1;
    visible = false; firing_line = PROJECT_NO;
    CHECK(monster_ranged_attack_legal(m, 120));
    CHECK(monster_ranged_attack_legal(m, 104));
    CHECK(!monster_ranged_attack_legal(m, 96));
    CHECK(monster_ranged_commit(m, 120));
    CHECK(m->mana == 40);
    m->smite_recovery = 1;
    CHECK(!monster_ranged_commit(m, 120));
    CHECK(m->mana == 40);
    m->smite_recovery = 0;
    visible = true; firing_line = PROJECT_CLEAR;
    CHECK(monster_ranged_commit(m, 119));
    CHECK(m->mana == 30);
    m->mana = 9;
    CHECK(!monster_ranged_commit(m, 120));
    CHECK(m->mana == 9);
    m->mana = 50;
    races[1].flags4 = RF4_SNG_BINDING;
    visible = false; firing_line = PROJECT_NO;
    CHECK(monster_ranged_commit(m, 114));
    CHECK(m->mana == 50); /* Song pulses pay in their existing scheduler. */
    m->song_lockout_timer = 1;
    CHECK(!monster_ranged_attack_legal(m, 114));
    m->song = SNG_BINDING;
    CHECK(monster_ranged_attack_legal(m, 114));
    m = setup();
    races[1].flags2 = RF2_MINDLESS;
    races[1].flags4 = RF4_ARROW1;
    p_ptr->px = 11;
    CHECK(test_choose_ranged(1) == 0); /* Singleton cannot bypass range. */
    p_ptr->px = 22;
    CHECK(test_choose_ranged(1) == 0);
    p_ptr->px = 12; m->stance = STANCE_FLEEING;
    CHECK(test_choose_ranged(1) == 0);
    m = setup(); m->r_idx = R_IDX_MORGOTH;
    races[R_IDX_MORGOTH].flags4 = RF4_SNG_BINDING | RF4_SNG_PIERCING;
    CHECK(!monster_ranged_attack_legal(m, 115));
    artifacts[ART_MORGOTH_3].cur_num = 1;
    CHECK(monster_ranged_attack_legal(m, 115));
    p_ptr->depth = MORGOTH_DEPTH; p_ptr->morgoth_hall_entered = true;
    CHECK(!monster_ranged_attack_legal(m, 114));
    firing_line = PROJECT_NO;
    CHECK(test_choose_ranged(1) == 0); /* Keep ordinary Morgoth song selection. */
    firing_line = PROJECT_CLEAR;
    CHECK(test_choose_ranged(1) == 115);
}

static void test_blow_choice_and_smite(void)
{
    monster_type* m = setup();
    for (int i = 0; i < MONSTER_BLOW_MAX; ++i)
    {
        races[1].blow[i].method = RBM_HIT;
        races[1].blow[i].effect = RBE_HURT;
        races[1].blow[i].att = 20;
        races[1].blow[i].dd = 2;
        races[1].blow[i].ds = 6;
    }
    races[1].blow[0].dd = races[1].blow[0].ds = 0;
    CHECK(monster_melee_utility(m, 0, true, false) == 0);
    races[1].blow[0].dd = 2; races[1].blow[0].ds = 6;
    races[1].blow[2].dd = 8;
    CHECK(test_choose_blow(m, true) == 2);
    races[1].flags2 = RF2_MINDLESS;
    unsigned selected = 0;
    random_value = 0;
    for (int i = 0; i < MONSTER_BLOW_MAX + 1; ++i)
        selected |= 1U << test_choose_blow(m, true);
    CHECK(selected == (1U << MONSTER_BLOW_MAX) - 1);
    races[1].flags2 = RF2_SMART;
    races[1].flags5 = RF5_SMITE;
    races[1].blow[0].dd = 6; races[1].blow[0].ds = 12;
    monster_abilities_begin_action(m);
    CHECK(test_choose_smite(m, 0, true));
    CHECK(!monster_commit_smite(m, true, false));
    CHECK(m->mana == 50 && m->smite_recovery == 0);
    CHECK(monster_commit_smite(m, true, true));
    CHECK(m->mana == 40 && m->smite_recovery == 1 && m->skip_next_turn);
    CHECK(!monster_abilities_can_react(m));
    monster_abilities_end_action(m, 10, 10, false);
    monster_abilities_begin_action(m);
    CHECK(!monster_can_smite(m, true));
    monster_abilities_end_action(m, 10, 10, true);
    CHECK(m->smite_recovery == 2);
    m->skip_next_turn = false;
    monster_abilities_begin_action(m);
    m->hp = 3; m->poisoned = 10;
    CHECK(!test_choose_smite(m, 0, true));
    CHECK(!test_choose_smite(m, 0, false));
}

static void test_observed_choices_and_pure_previews(void)
{
    monster_type* m = setup();
    races[1].flags4 = RF4_BRTH_FIRE;
    races[1].blow[0] = (monster_blow){ RBM_HIT, RBE_FIRE, 20, 2, 8 };
    races[1].flags5 = RF5_CONCENTRATION | RF5_DODGING | RF5_BLOCKING | RF5_VENGEANCE;
    races[1].shield_dd = 1; races[1].pd = 3; races[1].per = 6;
    m->vengeance = 1; m->consecutive_attacks = 2;
    monster_abilities_begin_action(m);
    int raw = monster_ranged_utility(m, 99);
    int melee_raw = monster_melee_utility(m, 0, true, false);
    p_ptr->chp = 1; p_ptr->mhp = 1000; p_ptr->resist_fire = 50;
    p_ptr->skill_use[S_EVN] = 50; p_ptr->poisoned = 100;
    CHECK(monster_ranged_utility(m, 99) == raw);
    CHECK(monster_melee_utility(m, 0, true, false) == melee_raw);
    monster_ai_observe(m, MON_AI_FIRE, 3);
    CHECK(monster_ranged_utility(m, 99) < raw);
    monsters[2].r_idx = 2; monsters[2].fy = 10; monsters[2].fx = 11;
    int crowded = monster_ranged_utility(m, 99);
    CHECK(crowded < raw);
    races[1].flags3 = RF3_SERPENT;
    CHECK(monster_ranged_utility(m, 99) > crowded);
    races[1].flags3 = RF3_DRAGON;
    races[2].flags3 = RF3_RES_FIRE;
    CHECK(monster_ranged_utility(m, 99) > crowded);
    CHECK(monster_breath_hits_grid(m, 99, 10, 11));
    CHECK(!monster_breath_hits_grid(m, 99, 10, 9));
    CHECK(!monster_breath_hits_grid(m, 99, 10, 16));
    monster_type before = *m;
    monster_lore before_lore = lore[1];
    int calls = random_calls;
    for (int i = 0; i < 10; ++i)
    {
        (void)monster_melee_utility(m, 0, true, true);
        (void)monster_ranged_utility(m, 99);
        (void)monster_sprinting_preview(m);
        (void)monster_dodging_bonus_preview(m);
        (void)monster_blocking_bonus_dice_preview(m);
    }
    CHECK(memcmp(&before, m, sizeof(before)) == 0);
    CHECK(memcmp(&before_lore, &lore[1], sizeof(before_lore)) == 0);
    CHECK(calls == random_calls);
}

static void test_offscreen_piercing(void)
{
    monster_type* m = setup();
    m->r_idx = R_IDX_MORGOTH; m->ml = false;
    song_of_piercing(m);
    CHECK(m->song == SNG_PIERCING);
}

int main(void)
{
    test_opportunities();
    test_legality_and_payment();
    test_blow_choice_and_smite();
    test_observed_choices_and_pure_previews();
    test_offscreen_piercing();
    printf("Monster AI combat: %d checks passed.\n", checks);
    return 0;
}

#endif
