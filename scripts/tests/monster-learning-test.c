#include "angband.h"
#include "monster/monster-ai.h"
#include "monster/monster-senses.h"
#include <assert.h>
#include <stdio.h>

static player_type player;
static monster_type monsters[5];
static monster_race races[R_IDX_MORGOTH + 1];
player_type* p_ptr = &player;
monster_type* mon_list = monsters;
monster_race* r_info = races;
s16b mon_max = 5;
s32b playerturn = 100;
static bool audible = true;
static int active_song;

bool monster_has_sight(const monster_type* m)
{
    return m->fy != 9; /* Perception is tested by monster-senses-test.c. */
}
bool los(int y1, int x1, int y2, int x2)
{
    (void)y1; (void)x1; (void)y2; (void)x2;
    return audible;
}
int distance(int y1, int x1, int y2, int x2)
{
    return MAX(ABS(y1-y2), ABS(x1-x2));
}
bool singing(int song) { return active_song == song; }

static void reset(void)
{
    memset(&player, 0, sizeof(player));
    memset(monsters, 0, sizeof(monsters));
    memset(races, 0, sizeof(races));
    playerturn = 100; active_song = SNG_NOTHING; audible = true;
    player.py = 5; player.px = 5;
    for (int i = 1; i < 5; ++i)
    {
        monsters[i].r_idx = i;
        monsters[i].fy = 5; monsters[i].fx = 5 + i;
        monsters[i].alertness = ALERTNESS_ALERT;
        races[i].flags2 = RF2_SMART; races[i].flags3 = RF3_ORC;
        races[i].d_char = 'o';
    }
}

int main(void)
{
    reset();
    monster_type* m = &monsters[1];
    monster_ai_observe(m, MON_AI_FIRE, 1);
    monster_ai_observe(m, MON_AI_FIRE, 1);
    assert(monster_ai_confidence(m, MON_AI_FIRE) == 1);
    playerturn++;
    monster_ai_observe(m, MON_AI_FIRE, 1);
    playerturn++;
    monster_ai_observe(m, MON_AI_FIRE, 1);
    assert(monster_ai_confidence(m, MON_AI_FIRE) == 3);
    playerturn++;
    monster_ai_observe(m, MON_AI_FIRE, -1);
    assert(monster_ai_confidence(m, MON_AI_FIRE) == 2);
    playerturn += 40;
    assert(monster_ai_confidence(m, MON_AI_FIRE) == 0);
    monster_ai_observe(m, MON_AI_FIRE, -1);
    assert(monster_ai_confidence(m, MON_AI_FIRE) == -1);

    reset(); m = &monsters[1];
    m->ml = false;
    monster_ai_observe(m, MON_AI_FLANKING, 3);
    assert(monster_ai_confidence(m, MON_AI_FLANKING) == 3);
    m->fy = 9; m->ml = true;
    monster_ai_observe(m, MON_AI_RIPOSTE, 3);
    assert(!monster_ai_confidence(m, MON_AI_RIPOSTE));
    m->fy = 5; m->confused = 1;
    monster_ai_observe(m, MON_AI_RIPOSTE, 3);
    assert(!monster_ai_confidence(m, MON_AI_RIPOSTE));
    m->confused = 0; races[1].flags2 |= RF2_MINDLESS;
    monster_ai_observe(m, MON_AI_RIPOSTE, 3);
    assert(!monster_ai_confidence(m, MON_AI_FLANKING));
    m->r_idx = R_IDX_MORGOTH;
    monster_ai_observe(m, MON_AI_RIPOSTE, 3);
    assert(!monster_ai_confidence(m, MON_AI_RIPOSTE));

    reset(); m = &monsters[1];
    monster_ai_observe(m, MON_AI_FIRE, 3);
    monster_ai_observe(m, MON_AI_POISON, 3);
    monster_ai_observe(m, MON_AI_RIPOSTE, 3);
    monsters[2].fy = 9; /* May hear a warning without seeing the player. */
    monster_ai_share_warning(m);
    assert(monster_ai_confidence(&monsters[2], MON_AI_FIRE) == 2);
    assert(monster_ai_confidence(&monsters[2], MON_AI_POISON) == 2);
    assert(!monster_ai_confidence(&monsters[2], MON_AI_RIPOSTE));
    playerturn += 10;
    monster_ai_share_warning(m);
    assert(monsters[2].ai.observations[MON_AI_FIRE].ttl == 20);
    playerturn += 10;
    assert(!monster_ai_confidence(&monsters[2], MON_AI_FIRE));
    monster_ai_share_warning(m);
    assert(!monster_ai_confidence(&monsters[2], MON_AI_FIRE));
    monster_ai_reset(&monsters[2]); active_song = SNG_SILENCE;
    monster_ai_share_warning(m);
    assert(!monster_ai_confidence(&monsters[2], MON_AI_FIRE));
    active_song = SNG_NOTHING; audible = false;
    monster_ai_share_warning(m);
    assert(!monster_ai_confidence(&monsters[2], MON_AI_FIRE));

    reset(); m = &monsters[1];
    player.previous_action[0] = 5;
    monster_ai_player_action(); playerturn++;
    monster_ai_player_attack(m, ATT_FLANKING);
    assert(monster_ai_confidence(m, MON_AI_FLANKING) == 3);
    assert(monster_ai_confidence(m, MON_AI_FOCUS) == 1);
    playerturn++;
    monster_ai_player_attack(m, ATT_MAIN);
    assert(monster_ai_confidence(m, MON_AI_CONCENTRATION) == 1);
    playerturn += 2; /* A wait does not erase the observed repeated engagement. */
    monster_ai_player_attack(m, ATT_MAIN);
    assert(monster_ai_confidence(m, MON_AI_CONCENTRATION) == 2);
    monster_ai_player_attack(m, ATT_RIPOSTE);
    assert(monster_ai_confidence(m, MON_AI_RIPOSTE) == 3);

    m->ai.cast_reserve = 3; m->ai.goal_age = 3; m->confused = 1;
    monster_ai_begin_turn(m);
    assert(!m->ai.cast_reserve && m->ai.goal_age == 2);
    m->ai.sense.kind = 255; m->ai.waits = 255;
    m->ai.cast_reserve = 255;
    monster_ai_sanitize(m);
    assert(!m->ai.sense.kind && m->ai.waits == 2 && m->ai.cast_reserve == 3);
    puts("Monster learning: bounded evidence, perception, warning expiry, reactions, action history and reset: PASS");
    return 0;
}
