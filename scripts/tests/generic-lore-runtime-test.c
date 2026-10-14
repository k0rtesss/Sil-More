#include "angband.h"
#include "sdl/main-sdl-private.h"
#include "init/init2-internal.h"
#include <assert.h>

static term test_term;
static cptr settings_keys;
static int setting_saves;
char __wrap_inkey(void)
{
    assert(settings_keys && *settings_keys);
    return *settings_keys++;
}
bool __wrap_save_pane_config_to_json(void)
{
    ++setting_saves;
    return true;
}
static errr terminal_extra(int action, int value)
{ (void)action; (void)value; return 0; }

static void check_terminal(cptr canonical)
{
    Term_clear();
    Term_putstr(0, 0, -1, TERM_WHITE, canonical);
    cptr expected = lore_text(canonical);
    size_t length = MIN(strlen(expected), 79);
    for (size_t i = 0; i < length; ++i) {
        byte attr;
        char ch;
        assert(!Term_what((int)i, 0, &attr, &ch));
        assert(ch == expected[i]);
    }
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    setbuf(stdout, NULL);
    log_set_level(LOG_ERROR);
    assert(SDL_Init(SDL_INIT_EVENTS));
    ANGBAND_DIR_EDIT = argv[1];
    ANGBAND_DIR_DATA = ANGBAND_DIR_USER = ANGBAND_DIR_PREF = argv[2];
    assert(!term_init(&test_term, 80, 24, 256));
    test_term.xtra_hook = terminal_extra;
    angband_term[0] = &test_term;
    Term_activate(&test_term);
    assert(!init_z_info() && !init_f_info());
    assert(!init_k_info() && !init_b_info());
    assert(!init_a_info() && !init_e_info());
    assert(!init_r_info() && !init_v_info());
    assert(!init_p_info() && !init_c_info());
    assert(!init_oath_info() && !init_cu_info() && !init_mb_info());
    assert(!init_h_info() && !init_st_info());
    assert(!init_style_info() && !init_other() && !init_alloc());
    rp_ptr = &p_info[0];
    current_character_profile = &c_info[0];
    char pref[1024];
    assert(path_build(pref, sizeof(pref), argv[1], "../pref"));
    ANGBAND_DIR_PREF = pref;
    assert(lore_init());
    u64b random_state;

    int changed = 0;
    for (int i = 1; i < z_info->art_max && i < 256; ++i) {
        object_type item;
        if (!a_info[i].name[0] || !make_fake_artefact(&item, (byte)i))
            continue;
        random_state = Rand_state_export();
        item.ident |= IDENT_SPOIL;
        char identity[sizeof(a_info[i].name)];
        memcpy(identity, a_info[i].name, sizeof(identity));
        char original[1024], generic[1024], stored[1024];
        assert(lore_set_generic(false));
        object_desc(original, sizeof(original), &item, true, 3);
        object_desc_display(stored, sizeof(stored), &item, true, 3);
        assert(!strcmp(original, stored));
        assert(random_state == Rand_state_export());
        assert(lore_set_generic(true));
        object_desc_display(generic, sizeof(generic), &item, true, 3);
        object_desc(stored, sizeof(stored), &item, true, 3);
        assert(!strcmp(original, stored));
        assert(!memcmp(identity, a_info[i].name, sizeof(identity)));
        changed += strcmp(original, generic) != 0;
        assert(lore_set_generic(false));
        object_desc_display(stored, sizeof(stored), &item, true, 3);
        assert(!strcmp(original, stored));
    }
    assert(changed > 40);
    printf("Actual artefacts: %d generic display names, canonical stats/names and reversibility PASS\n", changed);
    random_state = Rand_state_export();

    char monster_identity[128];
    SDL_strlcpy(monster_identity, r_name + r_info[R_IDX_MORGOTH].name,
        sizeof(monster_identity));
    assert(lore_set_generic(true));
    check_terminal(monster_identity);
    assert(!strcmp(monster_identity, r_name + r_info[R_IDX_MORGOTH].name));
    assert(lore_set_generic(false));
    check_terminal(monster_identity);
    assert(lore_set_generic(true));
    check_terminal(monster_identity);
    puts("Terminal rendering: immediate on/off/on with canonical monster identity PASS");

    int quotes = 0;
    for (int style = 0; style < INTRO_STYLE_RANDOM; ++style) {
        const sdl_welcome_intro_line* lines = sdl_welcome_intro_lines_for_style(style);
        for (; lines->text; ++lines) {
            if (lines->role == SDL_WELCOME_LINE_QUOTE
                || lines->role == SDL_WELCOME_LINE_BODY) {
                assert(strcmp(lines->text, lore_text(lines->text)) != 0);
                ++quotes;
            }
        }
    }
    assert(quotes >= 14);
    printf("SDL opening content: %d complete prose/poem blocks rewritten before layout PASS\n", quotes);

    p_ptr->playing = false;
    settings_keys = "igo";
    do_cmd_options();
    if (*settings_keys || op_ptr->opt[OPT_generic] || setting_saves != 1)
        fprintf(stderr, "Settings result: remaining=%d generic=%d saves=%d letters=%d\n",
            *settings_keys, op_ptr->opt[OPT_generic], setting_saves, sdl_menu_letters_enabled());
    assert(!*settings_keys && !op_ptr->opt[OPT_generic] && setting_saves == 1);
    settings_keys = "ig\033";
    do_cmd_options();
    assert(!*settings_keys && op_ptr->opt[OPT_generic] && setting_saves == 2);
    puts("Other Options: g toggles both directions, saves preference, Return/Escape work PASS");

    for (int i = 0; i < z_info->st_max; ++i)
        if (st_info[i].text)
            assert(strcmp(st_text + st_info[i].text,
                lore_text(st_text + st_info[i].text)) != 0);
    assert(random_state == Rand_state_export());
    puts("Parsed story pools and unchanged gameplay RNG PASS");
    lore_cleanup();
    term_nuke(&test_term);
    SDL_Quit();
    return 0;
}
