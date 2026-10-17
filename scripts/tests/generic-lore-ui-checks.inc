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
