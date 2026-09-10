/* Included by monster-ai-test.c; production tactical code is compiled above. */
static void test_tactical_poison_preview(void)
{
    int pending;
    monster_type* m = reset_map(11, 11, 5, 4, 5, 5);
    CHECK(monster_ai_poison_preview(0, true, true, 1, &pending) == 3);
    CHECK(pending == 9); /* entry 6, action-start 6, ceil(12/5) */
    CHECK(monster_ai_poison_preview(9, true, false, 1, &pending) == 3);
    CHECK(pending == 12); /* pool-to-pool cannot double the action's contact */
    CHECK(monster_ai_poison_preview(100, true, true, 2, &pending) == 38);
    CHECK(pending == 68); /* caps before each of the two actual ticks */
    CHECK(monster_ai_poison_preview(9, false, false, 2, &pending) == 4);
    CHECK(pending == 5);
    tile(4, 4, FEAT_POISON);
    CHECK(monster_ai_poison_damage(m, 4, 4, 1) == 3);
    tile(5, 4, FEAT_POISON);
    m->poisoned = 9;
    CHECK(monster_ai_poison_damage(m, 4, 4, 1) == 3);
    CHECK(monster_ai_poison_damage(m, 5, 4, 2) == 7);
    m->hp = 7;
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

static void test_tactical_extension(void)
{
    test_tactical_poison_preview();
    test_tactical_light();
    test_tactical_history_and_evidence();
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
