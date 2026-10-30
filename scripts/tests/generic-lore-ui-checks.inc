/* Included after the production browser, object-lore, help and tutorial code. */
static void generic_normalize(cptr text, char* out, size_t size)
{
    size_t used = 0;
    bool space = false;
    while (*text && used + 1 < size) {
        if (isspace((unsigned char)*text)) {
            space = used != 0;
        } else {
            if (space && used + 1 < size)
                out[used++] = ' ';
            if (used + 1 < size)
                out[used++] = *text;
            space = false;
        }
        ++text;
    }
    out[used] = '\0';
}

static char generic_object_output[32768];
static void generic_capture_output(byte attr, cptr text)
{
    (void)attr;
    SDL_strlcat(generic_object_output, text, sizeof(generic_object_output));
}

static void check_generic_ui(void)
{
    int original_mode = config.input_ui_mode;
    int viewed = 0;
    p_ptr->oath_type = 0;
    p_ptr->oaths_broken = 0;
    for (int controller = 0; controller < 2; ++controller) {
        config.input_ui_mode = controller ? SDL_INPUT_UI_MODE_CONTROLLER : SDL_INPUT_UI_MODE_PLATFORM;
        assert(steamdeck_controls_active() == (controller != 0));
        for (int i = 0; i < z_info->b_max; ++i) {
            ability_type* ability = &b_info[i];
            if (!ability->text || !ability->effect)
                continue;
            ability_browser_entry entry = {0};
            entry.b_ptr = ability;
            entry.abilitynum = ability->abilitynum;
            SDL_strlcpy(entry.name, b_name + ability->name, sizeof(entry.name));
            for (int small = 0; small < 2; ++small) {
                ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
                assert(lore_set_generic(true));
                int count = ability_browser_build_description(ability->skilltype,
                    &entry, lines, small ? 40 : 100);
                char joined[32768] = "", normalized[32768];
                for (int line = 0; line < count; ++line) {
                    SDL_strlcat(joined, lines[line].text, sizeof(joined));
                    SDL_strlcat(joined, " ", sizeof(joined));
                }
                generic_normalize(joined, normalized, sizeof(normalized));
                u32b offsets[] = { ability->text, ability->effect };
                for (size_t field = 0; field < N_ELEMENTS(offsets); ++field) {
                    char adapted[8192], expected[8192];
                    cptr raw = b_text + offsets[field];
                    cptr display = ability_menu_controller_text(raw, adapted, sizeof(adapted));
                    assert(strcmp(raw, display));
                    if (!controller)
                        assert(!strcmp(display, lore_text(raw)));
                    else {
                        assert(!strstr(display, "(z/5)"));
                        assert(!strstr(display, "Use '-' to"));
                        assert(!strstr(display, "Use 'X' to"));
                    }
                    generic_normalize(display, expected, sizeof(expected));
                    if (!strstr(normalized, expected)) {
                        fprintf(stderr, "Ability UI lost generic prose: %d field %u controller %d width %d\nExpected: %s\nActual: %s\n",
                            i, (unsigned)field, controller, small ? 40 : 100, expected, normalized);
                        abort();
                    }
                }
                ++viewed;
            }
        }
    }
    config.input_ui_mode = original_mode;
    printf("Actual ability UI: %d views, complete lore/effects, keyboard/controller, narrow/wide PASS\n", viewed);

    int kinds = 0, egos = 0;
    assert(lore_set_generic(true));
    for (int i = 1; i < z_info->k_max; ++i) {
        if (!k_info[i].text)
            continue;
        object_type item;
        object_prep(&item, i);
        item.ident |= IDENT_SPOIL | IDENT_KNOWN;
        char expected[8192], actual[8192], old[8192];
        object_lore_profile profile;
        object_lore_profile_for_object(&item, &profile);
        object_lore_add_alignment_keyword(&profile, object_lore_base_alignment(&item));
        cptr full = lore_text(k_text + k_info[i].text);
        if (!object_lore_select_segment(full, &profile, expected, sizeof(expected)))
            SDL_strlcpy(expected, full, sizeof(expected));
        cptr selected = object_lore_select_base_text(&item, actual, sizeof(actual));
        assert(selected && !strcmp(selected, expected));
        assert(lore_set_generic(false));
        cptr canonical = object_lore_select_base_text(&item, old, sizeof(old));
        assert(canonical && strcmp(canonical, expected));
        assert(lore_set_generic(true));
        ++kinds;
    }
    void (*original_hook)(byte, cptr) = text_out_hook;
    text_out_hook = generic_capture_output;
    for (int i = 1; i < z_info->e_max; ++i) {
        if (!e_info[i].text)
            continue;
        int kind = 0;
        for (int k = 1; k < z_info->k_max && !kind; ++k)
            for (size_t slot = 0; slot < N_ELEMENTS(e_info[i].tval); ++slot)
                if (e_info[i].tval[slot] == k_info[k].tval
                    && k_info[k].sval >= e_info[i].min_sval[slot]
                    && k_info[k].sval <= e_info[i].max_sval[slot]) {
                    kind = k;
                    break;
                }
        if (!kind)
            kind = lookup_kind(TV_SWORD, SV_DAGGER);
        assert(kind);
        object_type item;
        object_prep(&item, kind);
        item.name2 = (byte)i;
        item.ident |= IDENT_SPOIL | IDENT_KNOWN;
        object_lore_profile profile;
        object_lore_profile_for_object(&item, &profile);
        cptr full = lore_text(e_text + e_info[i].text);
        char expected[8192], normalized[32768];
        if (!object_lore_select_segment(full, &profile, expected, sizeof(expected)))
            SDL_strlcpy(expected, full, sizeof(expected));
        generic_object_output[0] = '\0';
        assert(screen_out_ego_lore(&item));
        generic_normalize(generic_object_output, normalized, sizeof(normalized));
        char clean[8192];
        generic_normalize(expected, clean, sizeof(clean));
        assert(!strcmp(normalized, clean));
        ++egos;
    }
    text_out_hook = original_hook;
    printf("Actual item UI: %d base kinds and %d egos select rewritten variants before formatting PASS\n", kinds, egos);
}

static void check_generic_quest_ui(void)
{
    int fields = 0;
    for (int generic = 0; generic < 2; ++generic) {
        assert(lore_set_generic(generic != 0));
        for (int i = 1; i < z_info->quest_max; ++i) {
            u32b offsets[] = { quest_info[i].init_text, quest_info[i].completion_text };
            for (int field = 0; field < 2; ++field) {
                if (!offsets[field]) continue;
                cptr expected = lore_text(q_text + offsets[field]);
                int count = 0;
                cptr* texts = field ? extract_quest_completion_texts(i, &count)
                                   : extract_quest_init_texts(i, &count);
                assert(texts && count);
                for (int line = 0; line < count; ++line) {
                    cptr newline = strchr(expected, '\n');
                    size_t length = newline ? (size_t)(newline - expected) : strlen(expected);
                    assert(strlen(texts[line]) == length);
                    assert(!strncmp(texts[line], expected, length));
                    expected += length + (newline != NULL);
                    mem_free((void*)texts[line]);
                }
                assert(!*expected);
                mem_free(texts);
                ++fields;
            }
        }
    }
    assert(lore_set_generic(true));
    printf("Actual quest UI: %d on/off introductions/completions preserve every paragraph and marker PASS\n", fields);
}

static void check_generic_help_ui(void)
{
    bool rows[HELP_DOC_MAX_ROWS], headings[HELP_DOC_MAX_ROWS];
    int height;
    assert(lore_set_generic(false));
    help_build_document_ops(&height, rows, headings);
    int count = g_help_doc_ops_n;
    cptr* original = calloc((size_t)count, sizeof(*original));
    assert(original);
    for (int i = 0; i < count; ++i) original[i] = str_dup(g_help_doc_ops[i].text);
    assert(lore_set_generic(true));
    help_build_document_ops(&height, rows, headings);
    assert(g_help_doc_ops_n == count);
    int changed = 0;
    for (int i = 0; i < count; ++i) {
        cptr expected = lore_text(original[i]);
        if (strcmp(g_help_doc_ops[i].text, expected))
            fprintf(stderr, "Help span %d: canonical [%s] expected [%s] actual [%s]\n",
                i, original[i], expected, g_help_doc_ops[i].text);
        assert(!strcmp(g_help_doc_ops[i].text, expected));
        changed += strcmp(original[i], expected) != 0;
        mem_free((void*)original[i]);
    }
    free(original);
    assert(changed >= 7);
    Term_clear();
    show_help_screen_compact_document(1, 3, 24, 0, -1);
    cptr header = "EMBERFALL - HELP [1/3]";
    for (size_t x = 0; header[x]; ++x) {
        byte attr;
        char ch;
        assert(!Term_what(1 + (int)x, 0, &attr, &ch));
        assert(ch == header[x]);
    }
    printf("Actual help UI: %d rewritten spans and formatted generic header PASS\n", changed);
}

static void check_generic_tutorial_ui(cptr path)
{
    assert(tutorial_load_catalogue(path));
    int viewed = 0;
    tutorial_context context = {0};
    SDL_strlcpy(context.text, "Observed context.", sizeof(context.text));
    SDL_strlcpy(context.subject, "This creature", sizeof(context.subject));
    for (int lesson = 0; lesson < catalogue_count; ++lesson) {
        for (int step = 0; step < catalogue[lesson].count; ++step) {
            cptr raw = catalogue[lesson].steps[step].text;
            if (!lore_has_description(raw)) continue;
            tutorial_view view;
            char expected[sizeof(view.body)];
            assert(lore_set_generic(false));
            fill_view(&view, lesson, step, &context);
            render_text(expected, sizeof(expected), raw, &context);
            assert(!strcmp(view.body, expected));
            char original[sizeof(view.body)];
            SDL_strlcpy(original, view.body, sizeof(original));
            assert(lore_set_generic(true));
            render_text(expected, sizeof(expected), lore_text(raw), &context);
            fill_view(&view, lesson, step, &context);
            assert(!strcmp(view.body, expected) && strcmp(view.body, original));
            assert(!strstr(view.body, "{detail}"));
            ++viewed;
        }
    }
    /* Older catalog passages also cover some mechanical tutorial steps. */
    assert(viewed >= 60);
    tutorial_shutdown();
    printf("Actual tutorial UI: %d complete rewrites before context expansion, reversible PASS\n", viewed);
}

/* Exercise real description builders with the mode toggled, including the
 * boundary between visible appearances and unidentified properties. */
static void check_minimum_text_ui(void)
{
    void (*old_hook)(byte, cptr) = text_out_hook;
    int ability_count = 0, kind_count = 0;
    bool old_aware;
    u32b old_flags2, old_flags4;
    char buffer[8192];
    object_type item;

    assert(lore_set_generic(false));
    assert(object_minimum_catalog());
    op_ptr->opt[OPT_minimum_text_mode] = true;
    p_ptr->oath_type = p_ptr->oaths_broken = 0;
    for (int i = 0; i < z_info->b_max; ++i) {
        ability_type* ability = &b_info[i];
        if (!ability->name || !ability->effect || !ability->text)
            continue;
        ability_browser_entry entry = {0};
        ability_browser_desc_line lines[ABILITY_BROWSER_DESC_MAX_LINES];
        entry.b_ptr = ability;
        entry.abilitynum = ability->abilitynum;
        SDL_strlcpy(entry.name, b_name + ability->name, sizeof(entry.name));
        int count = ability_browser_build_description(ability->skilltype,
            &entry, lines, 100);
        char joined[32768] = "", normalized[32768], expected[8192], adapted[8192];
        for (int line = 0; line < count; ++line) {
            assert(strcmp(lines[line].text, "Lore"));
            SDL_strlcat(joined, lines[line].text, sizeof(joined));
            SDL_strlcat(joined, " ", sizeof(joined));
        }
        generic_normalize(joined, normalized, sizeof(normalized));
        generic_normalize(ability_menu_controller_text(b_text + ability->effect,
            adapted, sizeof(adapted)), expected, sizeof(expected));
        if (!strstr(normalized, expected)) {
            fprintf(stderr, "Missing minimum-text ability effect %d: %s\n", i, expected);
            abort();
        }
        ++ability_count;
    }
    assert(ability_count > 100);

    for (int i = 1; i < z_info->k_max; ++i) {
        if (!k_info[i].text) continue;
        object_prep(&item, i);
        if (!object_minimum_needs_kind_text(&item)) continue;
        assert(object_minimum_lookup('K', i, buffer, sizeof(buffer)));
        assert(object_lore_select_base_text(&item, buffer, sizeof(buffer)));
        /* Some descriptions are already entirely gameplay information. */
        assert(buffer[0]);
        ++kind_count;
    }
    assert(kind_count > 50);
    object_prep(&item, 96);
    assert(object_lore_select_base_text(&item, buffer, sizeof(buffer)));
    assert(strstr(buffer, "equipped as a weapon") && strstr(buffer, "dig through rubble"));
    object_prep(&item, 98);
    assert(object_lore_select_base_text(&item, buffer, sizeof(buffer)));
    assert(strstr(buffer, "damaged walls or quartz"));

    text_out_hook = generic_capture_output;
    object_prep(&item, 344); /* Potion of Poison */
    old_aware = k_info[344].aware;
    k_info[344].aware = false;
    item.ident = 0;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(!strstr(generic_object_output, "5d4 points of poison"));
    k_info[344].aware = true;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(strstr(generic_object_output, "5d4 points of poison"));
    k_info[344].aware = old_aware;

    /* Every special with lore is visible when known, but not while hidden. */
    int special_count = 0;
    for (int i = 1; i < z_info->e_max; ++i) {
        if (!e_info[i].text) continue;
        int kind = 0;
        for (int k = 1; k < z_info->k_max && !kind; ++k)
            for (size_t slot = 0; slot < N_ELEMENTS(e_info[i].tval); ++slot)
                if (e_info[i].tval[slot] == k_info[k].tval
                    && k_info[k].sval >= e_info[i].min_sval[slot]
                    && k_info[k].sval <= e_info[i].max_sval[slot]) {
                    kind = k;
                    break;
                }
        assert(kind);
        assert(object_minimum_lookup('E', i, buffer, sizeof(buffer)));
        object_prep(&item, kind);
        object_set_ego_suffix(&item, i);
        old_aware = k_info[kind].aware;
        k_info[kind].aware = false;
        item.ident = 0;
        generic_object_output[0] = '\0';
        screen_out_head(&item);
        assert(!strstr(generic_object_output, buffer));
        item.ident = IDENT_KNOWN;
        generic_object_output[0] = '\0';
        screen_out_head(&item);
        if (!strstr(generic_object_output, buffer)) {
            fprintf(stderr, "Missing short special description %d: %s\n", i, buffer);
            abort();
        }
        k_info[kind].aware = old_aware;
        ++special_count;
    }
    assert(special_count > 100);

    /* Golden Flower lore keeps the original idea, and its exact gameplay
     * description remains the same in both modes. */
    int cloak_kind = lookup_kind(TV_CLOAK, SV_CLOAK);
    assert(cloak_kind);
    object_prep(&item, cloak_kind);
    object_set_ego_suffix(&item, 88);
    old_aware = k_info[cloak_kind].aware;
    k_info[cloak_kind].aware = true;
    item.ident = 0;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(!strstr(generic_object_output, "It brings courage"));
    item.ident = IDENT_KNOWN;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(strstr(generic_object_output,
        "It brings courage and a grace that does not readily bow to fear."));
    object_info_out_flags = object_flags_known;
    generic_object_output[0] = '\0';
    new_paragraph = true;
    object_info_out(&item);
    assert(strstr(generic_object_output, "It provides resistance to fear."));
    assert(strstr(generic_object_output, "It sustains your grace."));
    char golden_flower_mechanics[sizeof(generic_object_output)];
    SDL_strlcpy(golden_flower_mechanics, generic_object_output,
        sizeof(golden_flower_mechanics));
    op_ptr->opt[OPT_minimum_text_mode] = false;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(strstr(generic_object_output,
        "bright courage and a grace that does not readily bow to fear"));
    generic_object_output[0] = '\0';
    new_paragraph = true;
    object_info_out(&item);
    assert(!strcmp(generic_object_output, golden_flower_mechanics));
    op_ptr->opt[OPT_minimum_text_mode] = true;
    k_info[cloak_kind].aware = old_aware;

    /* Prefix and suffix descriptions coexist, and the ordinary mechanics
     * output is identical when the mode is toggled. */
    int short_sword = lookup_kind(TV_SWORD, SV_SHORT_SWORD);
    assert(short_sword);
    object_prep(&item, short_sword);
    object_set_ego_prefix(&item, 47); /* Balanced */
    object_set_ego_suffix(&item, 21); /* Gondolin */
    item.ident = IDENT_KNOWN;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(strstr(generic_object_output, "Its weight is carefully balanced."));
    assert(strstr(generic_object_output,
        "It recalls Gondolin's craft and defiance of Orcs and Raukar."));
    object_info_out_flags = object_flags_known;
    generic_object_output[0] = '\0';
    new_paragraph = true;
    object_info_out(&item);
    char compact_mechanics[sizeof(generic_object_output)];
    SDL_strlcpy(compact_mechanics, generic_object_output, sizeof(compact_mechanics));
    assert(compact_mechanics[0]);
    op_ptr->opt[OPT_minimum_text_mode] = false;
    generic_object_output[0] = '\0';
    new_paragraph = true;
    object_info_out(&item);
    assert(!strcmp(generic_object_output, compact_mechanics));
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(strstr(generic_object_output, "the easy obedience"));
    assert(strstr(generic_object_output, "Its edge keeps the cold bright pride of Gondolin"));
    op_ptr->opt[OPT_minimum_text_mode] = true;

    /* The damaged-lantern special has an actual rule in its prose. */
    object_prep(&item, 129);
    object_set_ego_prefix(&item, EGO_BROKEN_BRASS_LANTERN);
    old_aware = k_info[129].aware;
    k_info[129].aware = false;
    item.ident = 0;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(!strstr(generic_object_output, "until a smith mends"));
    item.ident = IDENT_KNOWN;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(strstr(generic_object_output, "no steady light can be kept within it until a smith mends"));
    k_info[129].aware = old_aware;

    object_prep(&item, 101); /* Great Helm: explicit appearance clauses. */
    old_aware = k_info[101].aware;
    old_flags2 = k_info[101].flags2;
    old_flags4 = k_info[101].flags4;
    k_info[101].aware = true;
    k_info[101].flags4 |= TR4_EVIL_ITEM;
    k_info[101].flags2 |= TR2_FEAR;
    item.ident = 0;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(strstr(generic_object_output, "Appearance: evil."));
    object_info_out_flags = object_flags_known;
    new_paragraph = true;
    object_info_out(&item);
    assert(!strstr(generic_object_output, "panic"));
    item.ident = IDENT_KNOWN;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    new_paragraph = true;
    object_info_out(&item);
    assert(strstr(generic_object_output, "Appearance: evil."));
    assert(strstr(generic_object_output, "panic"));
    k_info[101].aware = old_aware;
    k_info[101].flags2 = old_flags2;
    k_info[101].flags4 = old_flags4;

    /* An aware but unidentified plain-looking item must not reveal an evil
     * ego's alignment when its normal lore has no evil appearance clause. */
    object_prep(&item, 104);
    old_aware = k_info[104].aware;
    old_flags4 = k_info[104].flags4;
    k_info[104].aware = true;
    k_info[104].flags4 |= TR4_EVIL_ITEM;
    item.ident = 0;
    generic_object_output[0] = '\0';
    screen_out_head(&item);
    assert(!strstr(generic_object_output, "Appearance: evil."));
    k_info[104].aware = old_aware;
    k_info[104].flags4 = old_flags4;
    text_out_hook = old_hook;
    new_paragraph = false;

    object_prep(&item, 344);
    op_ptr->opt[OPT_minimum_text_mode] = false;
    cptr original = object_lore_select_base_text(&item, buffer, sizeof(buffer));
    assert(original && strstr(original, "draught"));
    printf("Minimum text: %d ability effects, %d functional kinds, %d specials, identification and toggle PASS\n",
        ability_count, kind_count, special_count);
}

extern void quest_minimum_summary(int quest_idx, bool complete, char* out, size_t size);
extern void describe_monster_mode(int r_idx, bool spoilers,
    const monster_type* live, bool compact);

static void check_minimum_quests_and_monsters(void)
{
    char summary[768];
    int race_id = 51;
    monster_race saved_race = r_info[race_id];
    monster_lore saved_lore = l_list[race_id];
    void (*old_hook)(byte, cptr) = text_out_hook;
    bool old_cheat = op_ptr->opt[OPT_cheat_know];
    bool old_know = op_ptr->opt[OPT_know_monster_info];
    u64b random_before = Rand_state_export();
    s16b old_target = p_ptr->tulkas_target_r_idx;
    s16b old_prize = p_ptr->tulkas_prize_a_idx;

    p_ptr->tulkas_target_r_idx = race_id;
    p_ptr->tulkas_prize_a_idx = 1;
    for (int id = QUEST_ID_TULKAS; id <= QUEST_ID_VARDA; ++id) {
        quest_minimum_summary(id, false, summary, sizeof(summary));
        assert(strstr(summary, "Objective:") && strstr(summary, "Reward:"));
        assert(strstr(summary, "300 XP"));
        assert(!strstr(summary, "[monster name]") && !strstr(summary, "Unknown reward"));
    }
    quest_minimum_summary(QUEST_ID_TULKAS, false, summary, sizeof(summary));
    assert(strstr(summary, r_name + r_info[race_id].name));
    assert(strstr(summary, "+1 Str") && strstr(summary, "future characters"));
    quest_minimum_summary(QUEST_ID_AULE, false, summary, sizeof(summary));
    assert(strstr(summary, "difficulty above 20") && strstr(summary, "+2 Smithing"));
    quest_minimum_summary(QUEST_ID_OROME, false, summary, sizeof(summary));
    assert(strstr(summary, "100 wolves") && strstr(summary, "30 vampires"));
    quest_minimum_summary(QUEST_ID_VARDA, false, summary, sizeof(summary));
    assert(strstr(summary, "500 ft") && strstr(summary, "leaving"));
    quest_minimum_summary(QUEST_ID_VARDA, true, summary, sizeof(summary));
    assert(strstr(summary, "Completed.") && strstr(summary, "radiant artefact"));
    p_ptr->tulkas_target_r_idx = p_ptr->tulkas_prize_a_idx = 0;
    quest_minimum_summary(QUEST_ID_TULKAS, false, summary, sizeof(summary));
    assert(strstr(summary, "named target") && strstr(summary, "an artefact"));
    p_ptr->tulkas_target_r_idx = old_target;
    p_ptr->tulkas_prize_a_idx = old_prize;

    op_ptr->opt[OPT_minimum_text_mode] = true;
    op_ptr->opt[OPT_cheat_know] = op_ptr->opt[OPT_know_monster_info] = false;
    r_info[race_id].flags3 = RF3_RES_FIRE | RF3_HURT_COLD | RF3_NO_SLEEP;
    r_info[race_id].flags4 = RF4_BRTH_FIRE | RF4_SCARE;
    r_info[race_id].flags2 = RF2_CHARGE;
    r_info[race_id].speed = 4;
    r_info[race_id].blow[0] = (monster_blow){.method=RBM_CLAW,
        .effect=RBE_POISON, .att=9, .dd=2, .ds=5};
    memset(&l_list[race_id], 0, sizeof(monster_lore));
    text_out_hook = generic_capture_output;
    generic_object_output[0] = '\0';
    describe_monster(race_id, false, NULL);
    assert(strstr(generic_object_output, "An orc of Morgoth"));
    assert(strstr(generic_object_output, "Speed: 4"));
    assert(strstr(generic_object_output, "Resists: unknown"));
    assert(strstr(generic_object_output, "Vulnerable: unknown"));
    assert(strstr(generic_object_output, "Immune to: unknown"));
    assert(strstr(generic_object_output, "Melee: unknown"));
    assert(strstr(generic_object_output, "Ranged and spells: unknown"));
    assert(!strstr(generic_object_output, "fire breath"));
    assert(!strstr(generic_object_output, "poison (+9"));
    assert(!strstr(generic_object_output, r_text + r_info[race_id].text));

    l_list[race_id].flags3 = RF3_RES_FIRE | RF3_NO_SLEEP;
    l_list[race_id].flags4 = RF4_BRTH_FIRE;
    l_list[race_id].flags2 = RF2_CHARGE;
    l_list[race_id].blows[0] = 1;
    monster_lore observed = l_list[race_id];
    generic_object_output[0] = '\0';
    describe_monster(race_id, false, NULL);
    assert(strstr(generic_object_output, "Resists: fire"));
    assert(strstr(generic_object_output, "Vulnerable: unknown"));
    assert(strstr(generic_object_output, "Immune to: sleep"));
    assert(strstr(generic_object_output, "claw: poison (+9, 2d5)"));
    assert(strstr(generic_object_output, "fire breath"));
    /* The original mechanics remain below the compact summary. */
    assert(strstr(generic_object_output, "can claw to poison"));
    assert(strstr(generic_object_output, "may breathe fire"));
    assert(strstr(generic_object_output, "Abilities: Charge"));
    assert(!memcmp(&l_list[race_id], &observed, sizeof(observed)));

    op_ptr->opt[OPT_know_monster_info] = true;
    generic_object_output[0] = '\0';
    describe_monster(race_id, false, NULL);
    assert(strstr(generic_object_output, "Vulnerable: cold"));
    assert(!memcmp(&l_list[race_id], &observed, sizeof(observed)));
    op_ptr->opt[OPT_know_monster_info] = false;
    generic_object_output[0] = '\0';
    describe_monster_mode(race_id, false, NULL, false);
    assert(strstr(generic_object_output, r_text + r_info[race_id].text));
    assert(Rand_state_export() == random_before);
    r_info[race_id] = saved_race;
    l_list[race_id] = saved_lore;
    text_out_hook = old_hook;
    op_ptr->opt[OPT_cheat_know] = old_cheat;
    op_ptr->opt[OPT_know_monster_info] = old_know;
    op_ptr->opt[OPT_minimum_text_mode] = false;
    puts("Minimum quest objectives/rewards and monster known-only traits/full view PASS");
}
