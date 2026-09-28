/* Included by monster-ai-test.c; production tactical code is compiled above. */
static void test_tactical_poison_preview(void)
{
    int pending;
    monster_type* m = reset_map(11, 11, 5, 4, 5, 5);
    CHECK(monster_ai_poison_preview(0, true, true, 1, &pending) == 4);
    CHECK(pending == 12); /* entry 8, action-start 8, ceil(16/5) */
    CHECK(monster_ai_poison_preview(9, true, false, 1, &pending) == 4);
    CHECK(pending == 13); /* pool-to-pool cannot double the action's contact */
    CHECK(monster_ai_poison_preview(100, true, true, 2, &pending) == 38);
    CHECK(pending == 70); /* caps before each of the two actual ticks */
    CHECK(monster_ai_poison_preview(9, false, false, 2, &pending) == 4);
    CHECK(pending == 5);
    tile(4, 4, FEAT_POISON);
    CHECK(monster_ai_poison_damage(m, 4, 4, 1) == 4);
    tile(5, 4, FEAT_POISON);
    m->poisoned = 9;
    CHECK(monster_ai_poison_damage(m, 4, 4, 1) == 4);
    CHECK(monster_ai_poison_damage(m, 5, 4, 2) == 9);
    m->hp = 9;
    CHECK(monster_ai_poison_safe(m, 5, 4, 1));
    CHECK(!monster_ai_poison_safe(m, 5, 4, 2)); /* unsafe recovery */
    CHECK(monster_ai_poison_safe(m, 4, 3, 2)); /* safe exit, existing poison ticks */
    races[1].flags2 |= RF2_FLYING;
    CHECK(monster_ai_poison_damage(m, 4, 4, 2) == 4);
    races[1].flags2 &= ~RF2_FLYING;
    races[1].flags3 |= RF3_RES_POIS;
    CHECK(monster_ai_poison_damage(m, 4, 4, 2) == 4);
    CHECK(!monster_ai_poison_safe(m, -1, 4, 1));

    tactical_label labels[TACTICAL_LABELS] = { 0 };
    tactical_add_label(labels, (tactical_label){ .cost=4, .damage=3, .pending=12, .steps=1, .used=true });
    tactical_add_label(labels, (tactical_label){ .cost=8, .damage=0, .pending=0, .steps=2, .used=true });
    int found_safe = 0, found_fast = 0;
    for (int i=0; i<TACTICAL_LABELS; ++i)
    {
        if (labels[i].used && !labels[i].pending) found_safe++;
        if (labels[i].used && labels[i].cost == 4) found_fast++;
    }
    CHECK(found_safe == 1 && found_fast == 1);
    tactical_add_label(labels, (tactical_label){ .cost=6, .damage=1, .pending=5, .steps=2, .used=true });
    tactical_add_label(labels, (tactical_label){ .cost=3, .damage=4, .pending=14, .steps=1, .used=true });
    found_safe = 0;
    for (int i=0; i<TACTICAL_LABELS; ++i)
        if (labels[i].used && !labels[i].pending && !labels[i].damage) found_safe++;
    CHECK(found_safe == 1); /* bounded pruning retains the survivable merge */
}

static void test_tactical_light(void)
{
    monster_type* m = reset_map(11, 11, 5, 2, 5, 5);
    CHECK(cave_light_contribution(-3, false, 5, 2, 5, 2) == -4);
    CHECK(cave_light_contribution(-3, true, 5, 2, 5, 2) == 1);
    CHECK(cave_light_contribution(-3, true, 5, 2, 5, 3) == -3);
    CHECK(cave_light_contribution(0, true, 5, 2, 5, 2) == 0); /* GLOW is not an emitter */
    CHECK(cave_light_contribution(3, false, 5, 2, 5, 5) == 1);
    CHECK(!cave_light_contribution(-3, false, 5, 2, -1, 2));
    tile(5, 3, FEAT_WALL_PERM);
    CHECK(!cave_light_contribution(-3, false, 5, 2, 5, 4));
    tile(5, 3, FEAT_FLOOR);
    int before, after;
    cave_light_remember_raw(5, 4, -12, 2);
    CHECK(cave_light_observed_change(m, 5, 4, -4, 0, &before, &after));
    CHECK(before == -5 && after == -5); /* removing one overlapping shadow changes nothing */
    cave_light_remember_raw(5, 4, 3, 0);
    CHECK(cave_light_observed_change(m, 5, 4, -1, -3, &before, &after));
    CHECK(before == 3 && after == 1);
    info[5][4] &= ~CAVE_VIEW;
    CHECK(!cave_light_observed_change(m, 5, 4, -1, -3, &before, &after));
    info[5][4] |= CAVE_VIEW;
    tile(5, 3, FEAT_WALL_PERM);
    CHECK(!cave_light_observed_change(m, 5, 4, -1, -3, &before, &after));
    tile(5, 3, FEAT_FLOOR);

    races[1].light = -3;
    monsters[2] = *m; monsters[2].r_idx = 2; monsters[2].fy=5; monsters[2].fx=4;
    occupants[5][4] = 2; races[2].flags3 = RF3_HURT_LITE;
    tactical_context c = { .actor=m, .py=5, .px=5, .shadow=true, .ally_count=1 };
    c.allies[0] = &monsters[2];
    cave_light_remember_raw(5, 4, 4, 0);
    cave_light_remember_raw(5, 5, 3, 0);
    CHECK(tactical_shadow_score(&c, 5, 3) > 0); /* real allied attack/evasion relief */
    cave_light_remember_raw(5, 4, -2, 0);
    CHECK(tactical_shadow_score(&c, 5, 3) == 0); /* no darkness stacking for its own sake */
    races[2].flags3 = 0; races[2].light = 1; races[2].d_char = 'G';
    cave_light_remember_raw(5, 5, 1, 0);
    CHECK(tactical_shadow_score(&c, 5, 3) < 0); /* darkening the giant's target hurts */
    c.shadow=false;
    CHECK(tactical_shadow_score(&c, 5, 3) == 0);
}

static void test_tactical_history_and_evidence(void)
{
    monster_type* m = reset_map(11, 11, 5, 4, 5, 5);
    tactical_context c = { .actor=m, .py=5, .px=5, .original_distance=1 };
    int ordinary_hold = tactical_position_score(&c, 5, 4);
    races[1].flags5 = RF5_CONCENTRATION; races[1].per=6;
    m->consecutive_attacks=3; m->ability_in_action=true;
    CHECK(tactical_position_score(&c, 5, 4) == ordinary_hold + 15);
    CHECK(monster_concentration_bonus_preview(m, true) == 3);
    CHECK(!lore[1].flags5); /* previews do not teach player lore */
    monster_abilities_forced_movement(m);
    CHECK(!monster_concentration_bonus_preview(m, true));
    races[1].flags5=RF5_BLOCKING; races[1].shield_dd=1; races[1].pd=3;
    CHECK(tactical_position_score(&c, 5, 4) == ordinary_hold + 5);
    m->previous_action[1]=6;
    CHECK(tactical_position_score(&c, 5, 4) == ordinary_hold);
    races[1].flags5=RF5_DODGING;
    int dodge_move=tactical_position_score(&c, 4, 4);
    races[1].flags5=0;
    CHECK(dodge_move == tactical_position_score(&c, 4, 4)+6);
    races[1].flags2 |= RF2_ZONE_OF_CONTROL;
    CHECK(tactical_position_score(&c, 5, 4) == ordinary_hold + 10);
    races[1].flags2 &= ~RF2_ZONE_OF_CONTROL;

    CHECK(!tactical_reaction_cost(&c, 5, 4, 4, 4));
    monster_ai_observe(m, MON_AI_ZONE, 3);
    CHECK(tactical_reaction_cost(&c, 5, 4, 4, 4) == 24);
    CHECK(!tactical_reaction_cost(&c, 5, 4, 5, 3));
    monster_ai_observe(m, MON_AI_OPPORTUNIST, 3);
    CHECK(tactical_reaction_cost(&c, 5, 4, 5, 3) == 24);
    CHECK(!tactical_reaction_cost(&c, 5, 3, 5, 4));
    monster_ai_observe(m, MON_AI_POLEARM, 3);
    CHECK(tactical_reaction_cost(&c, 5, 3, 5, 4) == 18);
    playerturn += 21;
    CHECK(!tactical_reaction_cost(&c, 5, 3, 5, 4)); /* expired reaction evidence */
    playerturn=0;

    m=reset_map(11, 11, 5, 4, 5, 5);
    races[1].flags2 |= RF2_KNOCK_BACK;
    tile(5, 6, FEAT_LAVA);
    CHECK(tactical_knockback(m, 5, 4) == 100);
    monster_ai_observe(m, MON_AI_FIRE, 3);
    CHECK(tactical_knockback(m, 5, 4) == 25);
    m->ai.observations[MON_AI_FIRE].ttl=0;
    p_ptr->resist_fire=9;
    CHECK(tactical_knockback(m, 5, 4) == 100); /* hidden resistance is not evidence */
    p_ptr->resist_fire=0;

    m=reset_map(11, 11, 5, 4, 5, 5);
    tile(5, 4, FEAT_ICE);
    tactical_choice first, repeated;
    monster_type snapshot=*m;
    monster_lore saved_lore=lore[1];
    u64b rng_before=Rand_state_export();
    CHECK(tactical_choose(m, &first));
    for (int i=0; i<30; ++i)
    {
        CHECK(tactical_choose(m, &repeated));
        CHECK(repeated.y == first.y && repeated.x == first.x);
    }
    CHECK(!memcmp(m, &snapshot, sizeof(snapshot)));
    CHECK(!memcmp(&lore[1], &saved_lore, sizeof(saved_lore)));
    CHECK(Rand_state_export() == rng_before);
    int y, x;
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(m->ai.goal_age == 3);
    monster_ai_begin_turn(m);
    CHECK(m->ai.goal_age == 2);
    move_monster(m, y, x);
    monster_ai_end_turn(m, 5, 4, false);
    CHECK(m->ai.previous_y == 5 && m->ai.previous_x == 4);
    CHECK(!get_move_tactical(m, &y, &x)); /* no immediate reversal to old footing */
}

static void test_tactical_attack_geometry(void)
{
    monster_type* m = reset_map(11, 11, 5, 4, 5, 5);
    monsters[2] = *m; monsters[2].fy = 5; monsters[2].fx = 3;
    occupants[5][3] = 2;
    tactical_context c = { .actor=m, .py=5, .px=5, .original_distance=1,
        .ally_count=1 };
    c.allies[0] = &monsters[2];
    int front = tactical_position_score(&c, 5, 4);
    int side = tactical_position_score(&c, 4, 4);
    monster_ai_observe(m, MON_AI_IMPALE, 3);
    CHECK(tactical_position_score(&c, 5, 4) < front);
    CHECK(tactical_position_score(&c, 4, 4) == side);
    int y, x;
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(!tactical_impale_pair(5, 5, y, x, 5, 3));
    CHECK(tactical_impale_pair(5, 5, 3, 3, 4, 4)); /* rear diagonal */
    CHECK(tactical_impale_pair(5, 5, 5, 3, 5, 4)); /* rear cardinal */
    CHECK(!tactical_impale_pair(5, 5, 5, 4, 5, 2)); /* out of reach */
    CHECK(!tactical_impale_pair(5, 5, 4, 3, 3, 1)); /* not an eight-way thrust */

    monster_ai_reset(m);
    monsters[2].fx = 6; occupants[5][3] = 0; occupants[5][6] = 2;
    int opposite = tactical_position_score(&c, 5, 4);
    monster_ai_observe(m, MON_AI_IMPALE, 3);
    CHECK(tactical_position_score(&c, 5, 4) == opposite);
    monster_ai_reset(m);
    monster_ai_observe(m, MON_AI_WHIRLWIND, 3);
    int swept = tactical_position_score(&c, 5, 4);
    CHECK(swept < opposite); /* opposite sides are still swept */
    monster_ai_reset(m);
    monster_ai_observe(m, MON_AI_FOLLOW_THROUGH, 3);
    int healthy = tactical_position_score(&c, 5, 4);
    CHECK(healthy < opposite && healthy > swept);
    monsters[2].hp = 1;
    CHECK(tactical_position_score(&c, 5, 4) < healthy);
    playerturn += 20;
    CHECK(tactical_position_score(&c, 5, 4) == opposite);

    /* A surrounded group can actually step out of a learned sweep. */
    monster_ai_reset(m);
    mon_max = 5;
    for (int i = 2; i <= 4; ++i)
    {
        monsters[i] = *m; monsters[i].fy = 3 + i; monsters[i].fx = 6;
        occupants[monsters[i].fy][6] = i;
    }
    monsters[4].fy = 4; occupants[7][6] = 0; occupants[4][6] = 4;
    monster_ai_observe(m, MON_AI_WHIRLWIND, 3);
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(distance(y, x, 5, 5) == 2);
}

static void test_tactical_protected_terrain(void)
{
    monster_type* m = reset_map(11, 11, 5, 4, 5, 5);
    int y, x;
    races[1].flags2 |= RF2_KNOCK_BACK | RF2_FLYING;
    tile(5, 6, FEAT_DEEP_WATER);
    CHECK(tactical_knockback(m, 5, 4) == 60); /* danger to player, not flyer */

    m = reset_map(11, 11, 5, 4, 5, 5);
    tile(4, 4, FEAT_POISON);
    CHECK(!get_move_tactical(m, &y, &x));
    races[1].flags3 = RF3_RES_POIS;
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(y == 4 && x == 4); /* immune creature takes useful pool footing */
    move_monster(m, y, x);
    CHECK(!get_move_tactical(m, &y, &x)); /* hold it instead of orbiting */

    m = reset_map(11, 11, 5, 4, 5, 5);
    tile(4, 4, FEAT_POISON);
    races[1].flags2 |= RF2_FLYING;
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(y == 4 && x == 4);
    monster_ai_reset(m);
    monster_ai_observe(m, MON_AI_POISON, 3);
    CHECK(!get_move_tactical(m, &y, &x)); /* witnessed protection reduces value */

    m = reset_map(11, 11, 5, 4, 5, 5);
    tile(4, 4, FEAT_LAVA);
    races[1].flags2 |= RF2_FLYING;
    CHECK(!get_move_tactical(m, &y, &x)); /* flight does not stop lava damage */
    races[1].flags3 = RF3_RES_FIRE;
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(y == 4 && x == 4);
    monster_ai_reset(m);
    tile(4, 4, FEAT_FLOOR);
    CHECK(!get_move_tactical(m, &y, &x)); /* no stale terrain objective */

    m = reset_map(11, 11, 5, 4, 5, 5);
    races[1].flags3 = RF3_RES_POIS;
    tile(4, 3, FEAT_POISON);
    CHECK(!get_move_tactical(m, &y, &x)); /* melee keeps contact */
    tile(4, 4, FEAT_POISON);
    races[1].flags2 |= RF2_MINDLESS;
    CHECK(!get_move_tactical(m, &y, &x));

    m = reset_map(11, 11, 5, 2, 5, 5);
    m->min_range = 2; m->best_range = 3;
    races[1].flags2 |= RF2_FLYING;
    tile(4, 2, FEAT_DEEP_WATER);
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(y == 4 && x == 2); /* ranged flyer keeps a shot from water */
    move_monster(m, y, x);
    CHECK(!get_move_tactical(m, &y, &x));
}

static void test_tactical_cooperation(void)
{
    monster_type* m = reset_map(15, 15, 6, 4, 6, 8);
    monsters[2] = *m;
    monsters[2].r_idx = 2; monsters[2].fx = 5;
    monsters[2].min_range = 2; monsters[2].best_range = 3;
    occupants[6][5] = 2;
    races[2].freq_ranged = 50;
    tactical_context c = { .actor=m, .py=6, .px=8, .sight=true,
        .ally_count=1 };
    c.allies[0] = &monsters[2];
    CHECK(tactical_guard_score(&c, 5, 6, false) > 0);
    CHECK(!tactical_guard_score(&c, 6, 6, false)); /* keep the archer's lane */
    CHECK(!tactical_guard_score(&c, 6, 4, false)); /* not behind the archer */
    m->hp = 40;
    CHECK(!tactical_guard_score(&c, 5, 6, false)); /* wounded guards preserve themselves */
    m->hp = 100;
    tactical_choice choice;
    CHECK(tactical_choose(m, &choice));
    CHECK(tactical_guard_score(&c, choice.goal_y, choice.goal_x, false) > 0);
    int y, x;
    for (int step = 0; step < 3; ++step)
    {
        int old_y = m->fy, old_x = m->fx;
        monster_ai_begin_turn(m);
        CHECK(get_move_tactical(m, &y, &x));
        move_monster(m, y, x);
        monster_ai_end_turn(m, old_y, old_x, false);
        if (tactical_guard_score(&c, y, x, false)) break;
    }
    CHECK(tactical_guard_score(&c, y, x, false) > 0);

    monsters[3] = *m; monsters[3].fy=5; monsters[3].fx=7;
    c.allies[1] = &monsters[3]; c.ally_count = 2;
    CHECK(!tactical_guard_score(&c, 7, 6, false)); /* another healthy guard covers it */
    c.ally_count = 1;
    monsters[2].hp = 30; monsters[2].stance = STANCE_FLEEING;
    CHECK(!tactical_guard_score(&c, 5, 6, false));
    CHECK(tactical_guard_score(&c, 5, 6, true) > 0);
    CHECK(!tactical_guard_score(&c, 6, 4, true)); /* leave the path away open */
    c.sight = false;
    CHECK(!tactical_guard_score(&c, 5, 6, true));

    m = reset_map(11, 11, 6, 5, 5, 5);
    monsters[2] = *m; monsters[2].r_idx=2;
    monsters[2].fy=4; monsters[2].fx=3; occupants[4][3]=2;
    monsters[2].hp=30; monsters[2].stance=STANCE_FLEEING;
    monsters[2].ml=true;
    CHECK(tactical_choose(m, &choice));
    CHECK(choice.reason == MON_TACTIC_WITHDRAWAL);
    CHECK(distance(choice.y, choice.x, 5, 5) == 1);
    CHECK(distance(choice.y, choice.x, 4, 3) <= 2);
    monsters[2].ml=false;
    tactical_choice hidden;
    CHECK(tactical_choose(m, &hidden));
    CHECK(hidden.y == choice.y && hidden.x == choice.x);
    CHECK(hidden.reason != MON_TACTIC_WITHDRAWAL); /* choice is unchanged; don't reveal ally */
}

static void test_tactical_feedback(void)
{
    monster_type* m = reset_map(11, 11, 5, 4, 5, 5);
    playerturn = 500;
    tactical_messages = 0;
    m->ml = true;
    tile(4, 4, FEAT_POISON); races[1].flags3 = RF3_RES_POIS;
    int y, x;
    CHECK(get_move_tactical(m, &y, &x));
    CHECK(m->ai.feedback_reason == MON_TACTIC_TERRAIN);
    CHECK(!tactical_messages); /* selecting or previewing never prints */
    monster_ai_end_turn(m, 5, 4, false);
    CHECK(!tactical_messages); /* move failed */
    CHECK(get_move_tactical(m, &y, &x));
    move_monster(m, y, x);
    monster_ai_end_turn(m, 5, 4, false);
    CHECK(tactical_messages == 1 && m->ai.feedback_cooldown == 8);
    CHECK(strstr(last_tactical_message, "hazardous ground") != NULL);
    ++playerturn;
    monster_ai_begin_turn(m);
    monster_ai_plan_feedback(m, MON_TACTIC_RETREAT_WAIT, m->fy, m->fx);
    monster_ai_end_turn(m, m->fy, m->fx, false);
    CHECK(tactical_messages == 1); /* per-creature cooldown */
    m->ai.feedback_cooldown = 0;
    monster_ai_plan_feedback(m, MON_TACTIC_RETREAT_WAIT, m->fy, m->fx);
    monster_ai_end_turn(m, m->fy, m->fx, true);
    CHECK(tactical_messages == 1); /* skipped turn is not deliberate waiting */
    monster_ai_plan_feedback(m, MON_TACTIC_RETREAT_WAIT, m->fy, m->fx);
    m->ml = false;
    monster_ai_end_turn(m, m->fy, m->fx, false);
    CHECK(tactical_messages == 1); /* became unseen during action */
    m->ml = true;
    monster_ai_plan_feedback(m, MON_TACTIC_RETREAT_WAIT, m->fy, m->fx);
    monster_ai_end_turn(m, m->fy, m->fx, false);
    CHECK(tactical_messages == 2);
    m->ai.feedback_cooldown = 0;
    monster_ai_plan_feedback(m, MON_TACTIC_RETREAT_WAIT, m->fy, m->fx);
    monster_ai_end_turn(m, m->fy, m->fx, false);
    CHECK(tactical_messages == 2); /* one message per player action */
}

static void test_tactical_songs(void)
{
    monster_type* m = reset_map(15, 15, 7, 4, 7, 7);
    m->min_range = 2; m->best_range = 3;
    update_flow(7, 7, FLOW_PLAYER_NOISE);
    tactical_choice choice, again;
    CHECK(!tactical_choose(m, &choice));
    p_ptr->song1 = SNG_MASTERY;
    CHECK(tactical_choose(m, &choice));
    CHECK(flow_dist(FLOW_PLAYER_NOISE, choice.y, choice.x)
        > flow_dist(FLOW_PLAYER_NOISE, m->fy, m->fx));
    CHECK(choice.reason == MON_TACTIC_SONG_DISTANCE);
    monster_type snapshot = *m;
    u64b rng = Rand_state_export();
    p_ptr->skill_use[S_SNG] = 100;
    CHECK(tactical_choose(m, &again));
    CHECK(again.y == choice.y && again.x == choice.x);
    CHECK(!memcmp(m, &snapshot, sizeof(snapshot)) && rng == Rand_state_export());
    p_ptr->song2 = SNG_CHALLENGE;
    CHECK(!tactical_choose(m, &again)); /* compelled aggressive approach is retained */
    p_ptr->song2 = SNG_NOTHING;
    p_ptr->song1 = SNG_LORIEN;
    CHECK(tactical_choose(m, &choice));
    CHECK(choice.reason == MON_TACTIC_SONG_DISTANCE);
    p_ptr->song1 = SNG_MASTERY;
    int steps = 0;
    while (steps < 8 && tactical_choose(m, &choice))
    {
        int old_y=m->fy, old_x=m->fx;
        move_monster(m, choice.y, choice.x);
        monster_ai_end_turn(m, old_y, old_x, false);
        ++steps;
    }
    CHECK(steps > 0 && steps < 8); /* a few steps, not endless retreat */
    CHECK(flow_dist(FLOW_PLAYER_NOISE, m->fy, m->fx) <= 6);

    m = reset_map(15, 15, 7, 4, 7, 7);
    update_flow(7, 7, FLOW_PLAYER_NOISE);
    tactical_context c = { .actor=m, .py=7, .px=7, .sight=true };
    int spacing, pressure;
    p_ptr->song1 = SNG_CONTEST;
    tactical_song_scores(&c, 7, 6, true, &spacing, &pressure);
    CHECK(!spacing && !pressure); /* an uninvolved bystander */
    m->song_contest_stacks = 1;
    tactical_song_scores(&c, 7, 6, true, &spacing, &pressure);
    CHECK(!spacing && pressure == 24);
    tactical_song_scores(&c, 7, 3, true, &spacing, &pressure);
    CHECK(!spacing && !pressure); /* moving away cannot break this duel */
    m->song_contest_completed = true;
    tactical_song_scores(&c, 7, 6, true, &spacing, &pressure);
    CHECK(!pressure);
    p_ptr->song1 = SNG_LAMENT; m->song_lament_stacks = 1;
    tactical_song_scores(&c, 7, 6, true, &spacing, &pressure);
    CHECK(pressure == 24);
    c.sight = false;
    tactical_song_scores(&c, 7, 6, true, &spacing, &pressure);
    CHECK(!spacing && !pressure); /* no access to an unseen singer's position */
    c.sight = true;
    p_ptr->song1 = SNG_MASTERY;
    c.ranged = true; m->min_range = 2; m->best_range = 3;
    tactical_song_scores(&c, 7, 3, false, &spacing, &pressure);
    CHECK(!spacing && !pressure); /* no reward for losing our shot */
}

static void test_tactical_extension(void)
{
    test_tactical_poison_preview();
    test_tactical_light();
    test_tactical_history_and_evidence();
    test_tactical_attack_geometry();
    test_tactical_protected_terrain();
    test_tactical_cooperation();
    test_tactical_feedback();
    test_tactical_songs();
    monster_type* morgoth = reset_map(11, 11, 5, 4, 5, 5);
    races[R_IDX_MORGOTH] = races[1];
    morgoth->r_idx = R_IDX_MORGOTH;
    tile(5, 4, FEAT_ICE);
    int y, x, old_y, old_x;
    CHECK(get_move_tactical_morgoth(morgoth, &old_y, &old_x));
    CHECK(get_move_tactical(morgoth, &y, &x));
    CHECK(y == old_y && x == old_x);
    CHECK(!morgoth->ai.goal_age); /* no learned objectives or new reserve */
}
