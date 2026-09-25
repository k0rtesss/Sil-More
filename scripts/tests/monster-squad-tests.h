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
