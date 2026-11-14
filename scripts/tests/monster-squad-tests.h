/* Central assignments and their actual production tactical consumer. */
static monster_type* squad_add(int idx, int race, int y, int x)
{
    monster_type* m = &monsters[idx];
    *m = monsters[1];
    memset(&m->squad, 0, sizeof(m->squad));
    m->r_idx = race; m->fy = y; m->fx = x;
    m->cdis = distance(y, x, p_ptr->py, p_ptr->px);
    occupants[y][x] = idx;
    mon_max = MAX(mon_max, idx + 1);
    return m;
}

static void squad_fixture(void)
{
    reset_map(15, 15, 7, 3, 7, 8);
    races[1].flags1 = RF1_ESCORT;
    races[1].flags3 = RF3_ORC;
    races[1].command_grade = MON_COMMAND_COMMANDER;
    races[1].command_kin = MON_KIN_ORC;
    races[1].command_authority = 1UL << MON_KIN_ORC;
    races[2] = races[1]; races[2].flags1 = 0;
    races[2].command_grade = MON_COMMAND_ORDINARY;
    squad_add(2, 2, 5, 4);
    squad_add(3, 2, 9, 4);
    races[3] = races[2]; races[3].flags4 = RF4_ARROW1; races[3].freq_ranged = 50;
    monster_type* archer = squad_add(4, 3, 7, 2);
    archer->min_range = 2; archer->best_range = 4;
}

static void test_squads(void)
{
    squad_fixture();
    monster_squad_prepare();
    CHECK(monsters[1].squad.role == MON_SQUAD_COMMANDER);
    for (int i = 1; i <= 4; ++i)
    {
        monster_type* m = &monsters[i];
        CHECK(m->squad.commander == 1);
        CHECK(monster_squad_order_valid(m));
        if (i > 1) CHECK(m->squad.role == MON_SQUAD_SOLDIER);
        CHECK(tactical_enterable(m, m->squad.y, m->squad.x));
        for (int j = 1; j < i; ++j)
            CHECK(m->squad.y != monsters[j].squad.y || m->squad.x != monsters[j].squad.x);
    }
    CHECK(distance(monsters[4].squad.y, monsters[4].squad.x, 7, 8) >= 2);
    /* Planning is deterministic, bounded, and does not spend game randomness. */
    monster_squad_order saved[5];
    for (int i = 1; i <= 4; ++i) saved[i] = monsters[i].squad;
    u64b rng_before = Rand_state_export();
    monster_squad_prepare();
    CHECK(Rand_state_export() == rng_before);
    for (int i = 1; i <= 4; ++i)
    {
        CHECK(monsters[i].squad.y == saved[i].y && monsters[i].squad.x == saved[i].x);
        CHECK(monsters[i].squad.commander == saved[i].commander);
    }
    tactical_choice choice;
    CHECK(tactical_choose(&monsters[2], &choice));
    CHECK(distance(choice.goal_y, choice.goal_x, monsters[2].squad.y, monsters[2].squad.x)
        < distance(monsters[2].fy, monsters[2].fx, monsters[2].squad.y, monsters[2].squad.x));
    /* Utility previews must not change the common assignments. */
    monster_tactical_move_utility(&monsters[2]);
    CHECK(!memcmp(&saved[2], &monsters[2].squad, sizeof(saved[2])));

    squad_fooled = &monsters[1];
    CHECK(!monster_squad_order_valid(&monsters[2]));
    squad_fooled = NULL;
    p_ptr->truce = true;
    monster_squad_prepare();
    CHECK(!monsters[1].squad.role && !monsters[2].squad.role);
    p_ptr->truce = false;
    monster_squad_prepare();

    monsters[1].confused = 1;
    CHECK(!monster_squad_order_valid(&monsters[2]));
    monster_squad_prepare();
    CHECK(monsters[2].squad.commander != 1 && monsters[2].squad.role);
    monsters[1].confused = 0;
    monsters[1].stance = STANCE_FLEEING;
    monster_squad_prepare();
    CHECK(monsters[2].squad.commander != 1 && monsters[2].squad.role);

    squad_fixture();
    monsters[2].stance = STANCE_FLEEING;
    monsters[3].alertness = ALERTNESS_UNWARY;
    monster_squad_prepare();
    CHECK(!monsters[2].squad.role && !monsters[3].squad.role);
    CHECK(monsters[4].squad.commander == 1);
    monsters[4].social_group = MON_GROUP_MAN;
    CHECK(!monster_squad_order_valid(&monsters[4]));
    monster_squad_prepare();
    CHECK(!monsters[1].squad.role && !monsters[4].squad.role);

    /* Higher-ranking allies coordinate one squad, not conflicting commands. */
    squad_fixture();
    races[2].flags4 = RF4_RALLY;
    races[2].command_grade = MON_COMMAND_COMMANDER; races[2].level = 11;
    monster_squad_prepare();
    CHECK(monsters[2].squad.role == MON_SQUAD_COMMANDER);
    CHECK(monsters[1].squad.commander == 2 && monsters[3].squad.commander == 2);
    monster_social_remap(2, 0);
    for (int i = 1; i <= 4; ++i) CHECK(!monsters[i].squad.role);
    occupants[monsters[2].fy][monsters[2].fx] = 0;
    memset(&monsters[2], 0, sizeof(monsters[2]));
    monster_squad_prepare();
    CHECK(monsters[3].squad.role == MON_SQUAD_COMMANDER);

    squad_fixture();
    monster_squad_prepare();
    monster_social_remap(1, 6);
    monsters[6] = monsters[1]; memset(&monsters[1], 0, sizeof(monsters[1]));
    occupants[7][3] = 6; mon_max = 7;
    CHECK(monsters[2].squad.commander == 6);
    CHECK(monster_squad_order_valid(&monsters[2]));
    monster_ai_reset(&monsters[2]);
    CHECK(!monsters[2].squad.role);
    monster_ai_sanitize(&monsters[3]);
    CHECK(!monsters[3].squad.role);

    /* Soldiers can follow a visible commander around a corner, but cannot
     * acquire an unseen player's new location after that contact is lost. */
    monster_type* leader = reset_map(15, 15, 5, 3, 5, 8);
    races[1].flags1 = RF1_ESCORT; races[1].flags3 = RF3_ORC;
    races[1].command_grade = MON_COMMAND_LEADER; races[1].command_kin = MON_KIN_ORC;
    races[1].command_authority = 1UL << MON_KIN_ORC;
    races[2] = races[1]; races[2].flags1 = 0;
    monster_type* soldier = squad_add(2, 2, 3, 3);
    tile(4, 5, FEAT_WALL_EXTRA);
    CHECK(monster_ai_can_see_player(leader));
    CHECK(!monster_ai_can_see_player(soldier));
    monster_squad_prepare();
    CHECK(monster_squad_order_valid(soldier));
    CHECK(soldier->ai.sense.kind == MON_SENSE_SHARED_TRACE);
    CHECK(tactical_choose(soldier, &choice));
    tile(5, 5, FEAT_WALL_EXTRA);
    CHECK(!monster_squad_order_valid(soldier));
    monster_squad_prepare();
    CHECK(!soldier->squad.role);
    tile(5, 5, FEAT_FLOOR);
    monster_squad_prepare();
    p_ptr->px = 9;
    CHECK(!monster_squad_order_valid(soldier));

    /* A useful ranged post is held instead of immediately wandering off. */
    squad_fixture();
    monster_squad_prepare();
    monster_type* archer = &monsters[4];
    occupants[archer->fy][archer->fx] = 0;
    archer->fy = archer->squad.y; archer->fx = archer->squad.x;
    occupants[archer->fy][archer->fx] = 4;
    CHECK(monster_squad_hold_position(archer));
    tile(archer->fy, archer->fx, FEAT_POISON);
    CHECK(!monster_squad_hold_position(archer));

    /* A one-cell corridor cannot yield overlapping or wall-bound orders. */
    leader = reset_map(11, 15, 5, 4, 5, 10);
    races[1].flags1 = RF1_ESCORT; races[1].flags3 = RF3_ORC;
    races[1].command_grade = MON_COMMAND_LEADER; races[1].command_kin = MON_KIN_ORC;
    races[1].command_authority = 1UL << MON_KIN_ORC;
    races[2] = races[1]; races[2].flags1 = 0;
    squad_add(2, 2, 5, 5); squad_add(3, 2, 5, 6);
    for (int y = 1; y < 10; ++y)
        for (int x = 1; x < 14; ++x)
            if (y != 5) tile(y, x, FEAT_WALL_EXTRA);
    monster_squad_prepare();
    for (int i = 1; i <= 3; ++i)
    {
        CHECK(monsters[i].squad.y == 5);
        CHECK(cave_feat[5][monsters[i].squad.x] == FEAT_FLOOR);
        for (int j = 1; j < i; ++j) CHECK(monsters[i].squad.x != monsters[j].squad.x);
    }
    CHECK(monsters[3].squad.x == 9);
    CHECK(squad_on_lane(3, 5, 2, 2, 4, 8));
    CHECK(!squad_on_lane(5, 3, 2, 2, 4, 8));
    puts("Commander/soldier assignments, contact, lifecycle and tactical movement: PASS.");
}

static void test_squad_action_messages(void)
{
    squad_fixture();
    monster_squad_prepare();
    monster_type* leader = &monsters[1];
    playerturn = 900;
    tactical_messages = 0;
    leader->ml = true;
    monster_ai_end_turn(leader, leader->fy, leader->fx, false);
    CHECK(!tactical_messages); /* No visible allies: no disclosure of a group. */
    monsters[2].ml = true;
    monster_ai_end_turn(leader, leader->fy, leader->fx, true);
    CHECK(!tactical_messages);
    leader->ability_displaced = true;
    monster_ai_end_turn(leader, leader->fy, leader->fx, false);
    CHECK(!tactical_messages);
    leader->ability_displaced = false;
    p_ptr->image = 1;
    u64b rng_before = Rand_state_export();
    monster_ai_end_turn(leader, leader->fy, leader->fx, false);
    CHECK(!tactical_messages && Rand_state_export() == rng_before);
    p_ptr->image = 0;
    monster_ai_end_turn(leader, leader->fy, leader->fx, false);
    CHECK(tactical_messages == 1 && strstr(last_tactical_message, "advance"));
    ++playerturn;
    leader->ai.feedback_cooldown = 0;
    monster_ai_end_turn(leader, leader->fy, leader->fx, false);
    CHECK(tactical_messages == 1); /* Unchanged commands are not repeated. */
    leader->squad.plan = MON_PLAN_HOLD;
    monster_ai_end_turn(leader, leader->fy, leader->fx, false);
    CHECK(tactical_messages == 2 && strstr(last_tactical_message, "hold formation"));
    ++playerturn;
    leader->squad.plan = MON_PLAN_REGROUP;
    monster_ai_end_turn(leader, leader->fy, leader->fx, false);
    CHECK(tactical_messages == 3 && strstr(last_tactical_message, "regroup"));
    monster_ai_sanitize(leader);
    CHECK(!leader->ai.reported_role && !leader->ai.reported_plan);

    squad_fixture();
    monster_squad_prepare();
    monster_type* archer = &monsters[4];
    move_monster(archer, archer->squad.y, archer->squad.x);
    archer->ml = true;
    ++playerturn;
    tactical_messages = 0;
    CHECK(monster_squad_hold_position(archer));
    monster_ai_plan_feedback(archer, MON_TACTIC_ORDER, archer->fy, archer->fx);
    monster_ai_end_turn(archer, archer->fy, archer->fx, false);
    CHECK(tactical_messages == 1 && strstr(last_tactical_message, "firing position"));
    monster_type* soldier = &monsters[2];
    soldier->ml = true;
    soldier->squad.job = MON_JOB_REGROUP;
    soldier->squad.plan = MON_PLAN_REGROUP;
    soldier->squad.y = soldier->fy; soldier->squad.x = soldier->fx;
    CHECK(monster_squad_hold_position(soldier));
    ++playerturn;
    monster_ai_plan_feedback(soldier, MON_TACTIC_ORDER, soldier->fy, soldier->fx);
    monster_ai_end_turn(soldier, soldier->fy, soldier->fx, false);
    CHECK(tactical_messages == 2 && !strcmp(last_tactical_message, "holds its ground."));
}

static void test_squad_hybrid_melee_roles(void)
{
    squad_fixture();
    for (int race = 1; race <= 3; ++race)
    {
        races[race].flags4 = RF4_BOULDER;
        races[race].freq_ranged = race == 1 ? 10 : 15;
    }
    for (int i = 1; i <= 4; ++i)
        monsters[i].min_range = monsters[i].best_range = 1;
    /* Rare boulders supplement melee; they do not make every giant a rear
     * archer. The production range selector specializes only above 15%. */
    CHECK(!squad_ranged(&monsters[1]));
    CHECK(!squad_ranged(&monsters[2]));
    CHECK(!squad_ranged(&monsters[4]));
    monster_squad_prepare();
    CHECK(monster_squad_order_valid(&monsters[1]));
    CHECK(distance(monsters[1].squad.y, monsters[1].squad.x,
        p_ptr->py, p_ptr->px) == 1);

    /* A current long-range preference and bow specialization retain their
     * firing jobs, including before the first range refresh. */
    monsters[1].min_range = 2;
    CHECK(squad_ranged(&monsters[1]));
    races[3].flags4 = RF4_ARROW1;
    races[3].freq_ranged = 75;
    monsters[4].min_range = monsters[4].best_range = 0;
    CHECK(squad_ranged(&monsters[4]));
    races[3].freq_ranged = 15;
    races[3].flags1 |= RF1_NEVER_BLOW;
    CHECK(squad_ranged(&monsters[4]));
    p_ptr->song1 = SNG_CHALLENGE;
    monsters[4].stance = STANCE_AGGRESSIVE;
    races[3].flags1 &= ~RF1_NEVER_BLOW;
    CHECK(!squad_ranged(&monsters[4]));
}

static void test_squad_grades(void)
{
    squad_fixture();
    races[1].flags1 = 0; races[1].command_grade = MON_COMMAND_ORDINARY;
    monster_squad_prepare();
    CHECK(monsters[1].squad.role == MON_SQUAD_COMMANDER);
    for (int i = 1; i <= 4; ++i) CHECK(monsters[i].squad.job == MON_JOB_ADVANCE);
    /* A stronger same-grade newcomer does not displace an established leader. */
    races[2].level = 200;
    monster_squad_prepare();
    CHECK(monsters[1].squad.role == MON_SQUAD_COMMANDER);
    races[4] = races[1]; races[4].command_grade = MON_COMMAND_LEADER; races[4].level = 1;
    squad_add(5, 4, 6, 3);
    monster_squad_prepare();
    CHECK(monsters[5].squad.role == MON_SQUAD_COMMANDER);
    CHECK(monsters[1].squad.commander == 5 && monsters[2].squad.commander == 5);
    races[5] = races[4]; races[5].command_grade = MON_COMMAND_COMMANDER;
    squad_add(6, 5, 8, 3);
    monster_squad_prepare();
    CHECK(monsters[6].squad.role == MON_SQUAD_COMMANDER);
    monsters[6].confused = 1;
    monster_squad_prepare();
    CHECK(monsters[5].squad.role == MON_SQUAD_COMMANDER);
    /* Level and a shared wandering band never confer authority over trolls. */
    races[5].command_kin = MON_KIN_DRAGON; races[5].command_authority = 1UL << MON_KIN_DRAGON;
    races[5].flags3 = RF3_DRAGON; races[5].level = 255; monsters[6].confused = 0;
    races[6] = races[1]; races[6].flags3 = RF3_TROLL;
    races[6].command_kin = MON_KIN_TROLL; races[6].command_authority = 1UL << MON_KIN_TROLL;
    squad_add(7, 6, 8, 4);
    monsters[6].wandering_idx = monsters[7].wandering_idx = FLOW_WANDERING_HEAD;
    CHECK(monster_social_allies(&monsters[6], &monsters[7]));
    CHECK(!squad_contact(&monsters[6], &monsters[7]));
    monster_squad_prepare();
    CHECK(!monsters[6].squad.role && !monsters[7].squad.role);
    CHECK(monsters[5].squad.role == MON_SQUAD_COMMANDER);
    /* Explicit authority works, but cannot override an actual hostile relation. */
    races[5].command_authority |= 1UL << MON_KIN_TROLL;
    CHECK(squad_contact(&monsters[6], &monsters[7]));
    monsters[6].social_rival = 7; monsters[6].social_memory = 3;
    CHECK(!squad_contact(&monsters[6], &monsters[7]));
    puts("Ordinary fallback, stable elections, grade succession and explicit command authority: PASS.");
}

static void test_squad_jobs(void)
{
    squad_fixture();
    races[1].command_grade = MON_COMMAND_LEADER;
    monster_squad_prepare();
    int guards = 0, flankers = 0;
    for (int i = 1; i <= 4; ++i)
    { guards += monsters[i].squad.job == MON_JOB_GUARD; flankers += monsters[i].squad.job == MON_JOB_FLANK; }
    CHECK(guards == 1); CHECK(flankers <= 1);
    monsters[3].stance = STANCE_FLEEING; monsters[3].hp = 20;
    monster_squad_prepare();
    int covering = 0;
    for (int i = 1; i <= 4; ++i) covering += monsters[i].squad.job == MON_JOB_COVER;
    CHECK(covering == 1 && !monsters[3].squad.role);

    squad_fixture();
    for (int i = 1; i <= 3; ++i)
    {
        races[i].command_grade = MON_COMMAND_ORDINARY;
        races[i].command_style = MON_COMMAND_PACK;
        races[i].command_kin = MON_KIN_WOLF; races[i].command_authority = 1UL << MON_KIN_WOLF;
        races[i].flags3 = RF3_WOLF; races[i].flags2 = 0;
        races[i].freq_ranged = 0; races[i].flags4 = 0;
    }
    monsters[4].min_range = monsters[4].best_range = 1;
    monster_squad_prepare();
    flankers = 0;
    for (int i = 1; i <= 4; ++i)
    {
        CHECK(monsters[i].squad.role);
        CHECK(monsters[i].squad.job == MON_JOB_ADVANCE || monsters[i].squad.job == MON_JOB_FLANK);
        flankers += monsters[i].squad.job == MON_JOB_FLANK;
    }
    CHECK(flankers == 1);
    puts("Leader guards/withdrawal cover and non-smart wolf pack pursuit/surrounding: PASS.");
}

static void test_squad_challenge_melee(void)
{
    squad_fixture();
    monster_type* archer = &monsters[4];
    /* Ordinary invalidated range and empty mana do not remove an archer role. */
    archer->min_range = archer->best_range = 0;
    archer->mana = 0;
    CHECK(squad_ranged(archer));
    p_ptr->song1 = SNG_CHALLENGE;
    archer->stance = STANCE_AGGRESSIVE;
    CHECK(!squad_ranged(archer));
    archer->min_range = 2; archer->best_range = 3; archer->mana = 15;
    CHECK(!squad_ranged(archer)); /* old range after a failed mana-drain roll */
    races[3].flags3 |= RF3_NO_CONF;
    CHECK(squad_ranged(archer));
    races[3].flags3 &= ~RF3_NO_CONF;

    /* A freshly compelled actor must not obey an already-issued guard post. */
    squad_fixture();
    monster_squad_prepare();
    monster_type* guard = NULL;
    for (int i = 1; i <= 4; ++i)
        if (monsters[i].squad.job == MON_JOB_GUARD) guard = &monsters[i];
    CHECK(guard != NULL && monster_squad_order_valid(guard));
    p_ptr->song1 = SNG_CHALLENGE;
    guard->stance = STANCE_AGGRESSIVE;
    guard->min_range = guard->best_range = guard->mana = 0;
    CHECK(!monster_squad_order_valid(guard));
    CHECK(!monster_squad_hold_position(guard));

    /* A commander still organizes an advance, without post/assembly jobs
     * overriding any member's compulsory melee action. */
    squad_fixture();
    p_ptr->song1 = SNG_CHALLENGE;
    occupants[monsters[3].fy][monsters[3].fx] = 0;
    monsters[3].fx = 1; occupants[monsters[3].fy][1] = 3;
    for (int i = 1; i <= 4; ++i)
    {
        monsters[i].stance = STANCE_AGGRESSIVE;
        monsters[i].min_range = monsters[i].best_range = monsters[i].mana = 0;
    }
    monster_squad_prepare();
    CHECK(monsters[1].squad.plan == MON_PLAN_ADVANCE);
    for (int i = 1; i <= 4; ++i)
    {
        CHECK(monsters[i].squad.role);
        CHECK(monsters[i].squad.job == MON_JOB_ADVANCE
            || monsters[i].squad.job == MON_JOB_FLANK);
        CHECK(monster_squad_order_valid(&monsters[i]));
    }
    /* An unaffected archer may keep shooting, without holding the others back. */
    monsters[4].stance = STANCE_CONFIDENT;
    monsters[4].min_range = 2; monsters[4].best_range = 4; monsters[4].mana = 15;
    monster_squad_prepare();
    CHECK(squad_ranged(&monsters[4]));
    for (int i = 1; i <= 3; ++i)
        CHECK(monsters[i].squad.job != MON_JOB_GUARD
            && monsters[i].squad.job != MON_JOB_COVER
            && monsters[i].squad.job != MON_JOB_HOLD);
    puts("Challenge-forced melee keeps coordinated advance without firing/guard/assembly posts; unaffected archers retain their role PASS.");
}

static void test_squad_archer_recognition_posts(void)
{
    squad_fixture();
    occupants[monsters[1].fy][monsters[1].fx] = 0;
    monsters[1].fy = 6; monsters[1].fx = 7; occupants[6][7] = 1;
    monster_type* archer = &monsters[4];
    occupants[archer->fy][archer->fx] = 0;
    archer->fy = 7; archer->fx = 5; occupants[7][5] = 4;
    visual_recognition = true;
    p_ptr->cur_light = -3; /* Real recognition rule gives this fixture range 2. */
    monster_squad_prepare();
    CHECK(monster_ai_can_see_player(&monsters[1]));
    CHECK(!monster_ai_can_see_player(archer));
    CHECK(distance(archer->squad.y, archer->squad.x, 7, 8) <= 2);
    /* A post from before a lighting/formation change must not hold a blind bow. */
    archer->squad.y = archer->fy; archer->squad.x = archer->fx;
    CHECK(monster_squad_order_valid(archer));
    CHECK(!monster_squad_hold_position(archer));

    squad_fixture();
    occupants[monsters[1].fy][monsters[1].fx] = 0;
    monsters[1].fy = 6; monsters[1].fx = 6; occupants[6][6] = 1;
    archer = &monsters[4];
    occupants[archer->fy][archer->fx] = 0;
    archer->fx = 1; occupants[7][1] = 4;
    visual_recognition = true; p_ptr->cur_light = -3;
    monster_squad_prepare();
    CHECK(monster_ai_can_see_player(&monsters[1]));
    CHECK(!monster_ai_can_see_player(archer));
    CHECK(!archer->squad.job); /* No recognized post within the local search. */
    CHECK(archer->ai.sense.kind == MON_SENSE_SHARED_TRACE);
    CHECK(archer->ai.sense.y == p_ptr->py && archer->ai.sense.x == p_ptr->px);
    puts("Archer posts require personal recognition; distant members receive shared sightings even before a post is reachable PASS.");
}

static void test_archer_retreat_from_melee_ring(void)
{
    monster_type* archer = reset_map(15, 15, 7, 8, 7, 7);
    races[1].flags3 = RF3_ORC;
    races[1].flags4 = RF4_ARROW1; races[1].freq_ranged = 75;
    archer->min_range = 2; archer->best_range = 3;
    races[2] = races[1]; races[2].flags4 = 0; races[2].freq_ranged = 0;
    const int allies[][2] = {{6,6},{6,7},{6,8},{7,6},{8,6},{8,7}};
    for (int i = 0; i < 6; ++i)
    {
        monster_type* m = squad_add(i + 2, 2, allies[i][0], allies[i][1]);
        m->min_range = m->best_range = 1;
    }
    int y, x;
    CHECK(get_move_tactical(archer, &y, &x));
    CHECK(distance(y, x, 7, 7) > 1);
    CHECK(tactical_has_shot(archer, y, x, 7, 7));
    puts("A bow leaves a crowded melee ring for a reachable shot instead of collecting melee formation rewards PASS.");
}

static void test_warned_squad_posts(void)
{
    monster_type* m = reset_map(15, 15, 7, 3, 7, 8);
    const int hazards[] = {FEAT_LAVA, FEAT_CHASM, FEAT_DEEP_WATER,
        FEAT_RUBBLE, FEAT_POISON};
    for (int i = 0; i < (int)N_ELEMENTS(hazards); ++i)
    {
        pending_features[6][4] = hazards[i];
        CHECK(monster_terrain_penalty(m, 6, 4) >= 100);
        CHECK(monster_step_cost(m, 7, 3, 6, 4) == 0);
    }
    races[1].flags2 |= RF2_FLYING;
    for (int i = 0; i < (int)N_ELEMENTS(hazards); ++i)
    {
        pending_features[6][4] = hazards[i];
        CHECK((monster_terrain_penalty(m, 6, 4) >= 100) == (hazards[i] == FEAT_LAVA));
    }
    pending_features[6][4] = FEAT_LAVA;
    races[1].flags3 = RF3_RES_FIRE;
    CHECK(monster_terrain_penalty(m, 6, 4) == 0);
    CHECK(monster_step_cost(m, 7, 3, 6, 4) > 0);
    races[1].flags2 &= ~RF2_FLYING;
    races[1].flags3 = RF3_RES_POIS;
    pending_features[6][4] = FEAT_POISON;
    CHECK(monster_terrain_penalty(m, 6, 4) == 0);
    races[1].flags3 = 0;
    pending_features[6][4] = FEAT_WATER;
    CHECK(monster_terrain_penalty(m, 6, 4) == 0);

    squad_fixture();
    monster_squad_prepare();
    monster_type* archer = &monsters[4];
    int y = archer->squad.y, x = archer->squad.x;
    pending_features[y][x] = FEAT_LAVA;
    CHECK(!tactical_enterable(archer, y, x));
    monster_squad_prepare();
    CHECK(archer->squad.y != y || archer->squad.x != x);
    races[archer->r_idx].flags3 |= RF3_RES_FIRE;
    CHECK(tactical_enterable(archer, y, x));
    m = reset_map(15, 15, 7, 3, 7, 11);
    for (int yy = 6; yy <= 8; ++yy)
        for (int xx = 4; xx <= 6; ++xx)
            pending_features[yy][xx] = FEAT_LAVA;
    bool engaged = false;
    for (int step = 0; step < 16; ++step)
    {
        get_move_advance(m, &y, &x);
        CHECK(pending_features[y][x] == 0);
        if (y == p_ptr->py && x == p_ptr->px) { engaged = true; break; }
        CHECK(y != m->fy || x != m->fx);
        move_monster(m, y, x);
    }
    CHECK(engaged);
    pending_features[p_ptr->py][p_ptr->px] = FEAT_LAVA;
    CHECK(monster_step_cost(m, m->fy, m->fx, p_ptr->py, p_ptr->px) == 1);
    puts("Warned harmful terrain cannot become a squad post or tactical step; flight and immunities preserve safe warnings PASS.");
}

static void test_water_firing_posts(void)
{
    monster_type* m = reset_map(15, 15, 7, 3, 7, 8);
    races[1].flags4 = RF4_ARROW1;
    races[1].freq_ranged = 75;
    m->min_range = 3; m->best_range = 4;
    cave_feat[7][4] = FEAT_DEEP_WATER;
    CHECK(!tactical_has_shot(m, 7, 4, 7, 8));
    cave_feat[7][4] = FEAT_WATER;
    CHECK(tactical_has_shot(m, 7, 4, 7, 8));
    cave_feat[7][4] = FEAT_DEEP_WATER;
    races[1].flags2 |= RF2_FLYING;
    CHECK(tactical_has_shot(m, 7, 4, 7, 8));
    puts("Grounded firing posts reject deep water; shallow water and flight retain clear shots PASS.");
}

static void test_squad_death_preserves_personal_budget(void)
{
    squad_fixture();
    for (int i = 1; i <= 4; ++i) monsters[i].hp = 20;
    playerturn = 100;
    for (int i = 0; i <= 4; ++i)
    {
        monster_squad_prepare();
        if (i < 4) ++playerturn;
    }
    monster_type* original = &monsters[1];
    CHECK(original->squad.plan == MON_PLAN_ADVANCE);
    monster_squad_order saved = original->squad;
    CHECK(saved.budget_plan == MON_PLAN_REGROUP && saved.plan_age >= 3);

    /* An unseen original leader can follow a currently visible replacement. */
    move_monster(original, 7, 4);
    visual_recognition = true; p_ptr->cur_light = -3;
    races[4] = races[1]; races[4].level = 200;
    monster_type* replacement = squad_add(5, 4, 7, 6);
    monster_squad_prepare();
    CHECK(original->squad.commander == 5);
    CHECK(original->squad.budget_plan == saved.budget_plan);
    CHECK(original->squad.plan_age == saved.plan_age);
    CHECK(original->squad.plan_turn == saved.plan_turn);

    monster_social_remap(5, 0);
    CHECK(!original->squad.role && !original->squad.commander && !original->squad.job);
    CHECK(original->squad.budget_plan == saved.budget_plan);
    CHECK(original->squad.plan_age == saved.plan_age);
    CHECK(original->squad.plan_turn == saved.plan_turn);
    occupants[replacement->fy][replacement->fx] = 0;
    memset(replacement, 0, sizeof(*replacement));
    visual_recognition = false;
    ++playerturn;
    monster_squad_prepare();
    CHECK(original->squad.role == MON_SQUAD_COMMANDER);
    CHECK(original->squad.plan == MON_PLAN_ADVANCE);
    CHECK(original->squad.plan_age >= saved.plan_age);
    monster_ai_reset(original);
    CHECK(!original->squad.budget_plan && !original->squad.plan_age);
    puts("Commander deletion cancels orders without renewing a survivor's own regroup budget; actor reset still clears it PASS.");
}

static void test_squad_group_plans(void)
{
    reset_map(15, 15, 5, 4, 5, 5);
    races[1].command_grade = MON_COMMAND_COMMANDER;
    races[1].command_kin = MON_KIN_ORC; races[1].command_authority = 1UL << MON_KIN_ORC;
    races[1].flags3 = RF3_ORC;
    races[2] = races[1]; races[2].command_grade = MON_COMMAND_ORDINARY;
    squad_add(2, 2, 4, 3); squad_add(3, 2, 6, 3);
    for (int i = 1; i <= 3; ++i) monsters[i].hp = 20;
    playerturn = 100;
    monster_squad_prepare();
    CHECK(monsters[1].squad.plan == MON_PLAN_REGROUP);
    tactical_choice choice;
    CHECK(tactical_choose(&monsters[1], &choice));
    CHECK(distance(choice.y, choice.x, 5, 5) > 1);
    CHECK(monsters[2].squad.job == MON_JOB_REGROUP);
    for (int round = 1; round <= 5; ++round)
    {
        ++playerturn; monster_squad_prepare();
        if (round >= 3) CHECK(monsters[1].squad.plan == MON_PLAN_ADVANCE);
    }
    for (int i = 1; i <= 3; ++i) monsters[i].hp = 100;
    ++playerturn; monster_squad_prepare();
    CHECK(monsters[1].squad.plan_age == 0);

    reset_map(19, 19, 9, 9, 9, 14);
    races[1].command_grade = MON_COMMAND_COMMANDER;
    races[1].command_kin = MON_KIN_ORC; races[1].command_authority = 1UL << MON_KIN_ORC;
    races[1].flags3 = RF3_ORC;
    races[2] = races[1]; races[2].command_grade = MON_COMMAND_ORDINARY;
    squad_add(2, 2, 8, 9); squad_add(3, 2, 9, 4);
    monster_squad_prepare();
    CHECK(monsters[1].squad.plan == MON_PLAN_HOLD);
    CHECK(monster_squad_hold_position(&monsters[1]));
    for (int round = 1; round <= 4; ++round)
    {
        ++playerturn; monster_squad_prepare();
        if (round >= 2) CHECK(monsters[1].squad.plan == MON_PLAN_ADVANCE);
    }
    puts("Commander assembly, real adjacent withdrawal choice and bounded group-plan budgets: PASS.");
}
