#define tutorial_game_menu test_tutorial_game_menu
#define tutorial_game_action_allowed test_tutorial_game_action_allowed
#define death_spectator_active test_death_spectator_active
#define steamdeck_controls_active test_steamdeck_controls_active
#define update_stuff test_update_stuff
#define ui_menu_click_begin test_ui_menu_click_begin
#define ui_menu_click_set_hover_enabled test_ui_menu_click_set_hover_enabled
#define ui_menu_click_clear test_ui_menu_click_clear
#define ui_menu_click_take_action test_ui_menu_click_take_action
#define birth_coach_show_once test_birth_coach_show_once
#define sdl_character_sheet_screen_hide test_sdl_character_sheet_screen_hide
#define sdl_character_sheet_screen_show_birth_skills test_show_birth_skills
#define inkey test_inkey
#define steamdeck_menu_key test_steamdeck_menu_key
#define steamdeck_alt_action_key test_steamdeck_alt_action_key
#define steamdeck_back_key test_steamdeck_back_key
#define birth_confirm_input test_birth_confirm_input
#define sdl_quick_access_suggest_skill_shortcut test_suggest_shortcut
#define bell test_bell
#define msg_print test_msg_print
#define log_log test_log_log
#include "angband.h"
#include "birth/birth-internal.h"
#include "tutorial/tutorial-game.h"
#include <assert.h>
#include "birth/birth-skills.c"

/* Only rendering/input and unrelated update hooks are mocked. The allocation
 * loop, rank cost arithmetic, recommendation and stat cost table are production. */
static player_type player;
static player_race race;
static character_profile profile;
static const char* keys;
static int key_index, frames, bells;
static bool spectator, controller, pending_defaults_click;
static void (*inspect_input)(void);

void tutorial_game_menu(const char* id, const char* description) {}
bool tutorial_game_action_allowed(const char* action, const object_type* item)
{ return true; }
bool death_spectator_active(void) { return spectator; }
bool steamdeck_controls_active(void) { return controller; }
void update_stuff(void) {}
void ui_menu_click_begin(void) {}
void ui_menu_click_set_hover_enabled(bool enabled) {}
void ui_menu_click_clear(void) {}
bool ui_menu_click_take_action(int* choice, int* action)
{
    if (!pending_defaults_click) return false;
    pending_defaults_click = false;
    *choice = -4;
    *action = UI_MENU_CLICK_PRIMARY;
    return true;
}
void birth_coach_show_once(int stage) {}
void sdl_character_sheet_screen_hide(void) {}
void sdl_character_sheet_screen_show_birth_skills(const int* old_base,
    const int* gains, const int* costs, int selected, int points_left)
{
    int total = 0;
    for (int i = 0; i < S_MAX; i++)
    {
        if (i == S_SPC) continue;
        assert(p_ptr->skill_base[i] == old_base[i] + gains[i]);
        assert(costs[i] == birth_skill_cost(old_base[i], gains[i]));
        total += costs[i];
    }
    assert(points_left == p_ptr->new_exp);
    assert(total >= 0);
    frames++;
}
char inkey(void)
{
    assert(skill_gain_in_progress);
    assert(keys[key_index]); /* An unexpected extra prompt must fail, not hang. */
    if (inspect_input) inspect_input();
    char key = keys[key_index++];
    if (key == '@') pending_defaults_click = true;
    return key;
}
int steamdeck_menu_key(int key, int prev, int next) { return key; }
int steamdeck_alt_action_key(void) { return 'x'; }
int steamdeck_back_key(void) { return ESCAPE; }
bool birth_confirm_input(int key, bool steamdeck) { return key == '\r'; }
void sdl_quick_access_suggest_skill_shortcut(int skill) { assert(false); }
void bell(cptr reason) { bells++; }
void msg_print(cptr msg) {}
void log_log(int level, const char* file, int line, const char* fmt, ...) {}

static void setup(const char* input)
{
    p_ptr = &player;
    rp_ptr = &race;
    current_character_profile = &profile;
    character_generated = false;
    turn = 0;
    memset(&player, 0, sizeof(player));
    player.new_exp = 5000;
    player.skill_base[S_SPC] = 7; /* Special abilities never enter purchases. */
    keys = input;
    key_index = frames = bells = 0;
    spectator = controller = pending_defaults_click = false;
    inspect_input = NULL;
    gain_skills_set_initial_skill(-1);
}

static void expect_defaults(void)
{
    for (int i = 0; i < S_MAX; i++)
    {
        int expected = i == S_MEL || i == S_EVN ? 5
            : i == S_PER || i == S_WIL ? 3 : i == S_SPC ? 7 : 0;
        assert(player.skill_base[i] == expected);
    }
    assert(player.new_exp == 800);
}

static void expect_zero(void)
{
    for (int i = 0; i < S_MAX; i++)
        assert(player.skill_base[i] == (i == S_SPC ? 7 : 0));
    assert(player.new_exp == 5000);
}

static void inspect_refund(void)
{
    if (key_index == 0) expect_defaults();
    else
    {
        assert(player.skill_base[S_MEL] == 4);
        assert(player.new_exp == 1300); /* Fifth rank refunds 500 XP. */
    }
}

static void skills(void)
{
    setup("\r");
    inspect_input = expect_defaults;
    assert(gain_skills_birth() == NAV_OK);
    expect_defaults();
    assert(!skill_gain_in_progress && !hide_cursor);

    setup("4\r");
    inspect_input = inspect_refund;
    assert(gain_skills_birth() == NAV_OK);
    assert(player.new_exp == 1300 && player.skill_base[S_MEL] == 4);

    /* Every proposed rank can be refunded, including the last rank to zero. */
    char refund_keys[128];
    char* end = refund_keys;
    for (int i = 0; i < S_MAX; i++)
    {
        if (i == S_SPC) continue;
        int ranks = i == S_MEL || i == S_EVN ? 5
            : i == S_PER || i == S_WIL ? 3 : 0;
        while (ranks--) *end++ = '4';
        *end++ = '2';
    }
    *end++ = '\r';
    *end = 0;
    setup(refund_keys);
    assert(gain_skills_birth() == NAV_OK);
    expect_zero();

    setup("4\033");
    assert(gain_skills_birth() == NAV_BACK);
    expect_zero();
    assert(!skill_gain_in_progress);
    setup("4q");
    assert(gain_skills_birth() == NAV_TO_CHARACTER);
    expect_zero();
    assert(!skill_gain_in_progress);

    setup("44n\r");
    assert(gain_skills_birth() == NAV_OK);
    expect_defaults();
    setup("44N\r");
    assert(gain_skills_birth() == NAV_OK);
    expect_defaults();
    setup("44@\r"); /* Synthetic primary click on Beginner defaults. */
    assert(gain_skills_birth() == NAV_OK);
    expect_defaults();
    setup("44x\r");
    controller = true;
    assert(gain_skills_birth() == NAV_OK);
    expect_defaults();

    setup("\r");
    inspect_input = expect_zero;
    assert(gain_skills() == NAV_OK);
    expect_zero();
    setup("nN@x\r");
    character_generated = true;
    turn = 100;
    controller = true;
    inspect_input = expect_zero;
    assert(gain_skills() == NAV_OK);
    expect_zero();
    setup("n\r"); /* The birth-only shortcut must not buy ranks in-game. */
    assert(gain_skills() == NAV_OK);
    expect_zero();

    setup("\r");
    spectator = true;
    assert(gain_skills_birth() == NAV_OK);
    expect_zero();
    setup("n\r");
    player.new_exp = 4199;
    assert(gain_skills_birth() == NAV_OK);
    assert(player.new_exp == 4199 && player.skill_base[S_MEL] == 0);
    assert(bells == 1);

    setup("4\033");
    player.skill_base[S_MEL] = 2;
    assert(gain_skills_birth() == NAV_BACK);
    assert(player.new_exp == 5000 && player.skill_base[S_MEL] == 2);
    puts("Birth skills: defaults cost 4200; refunds, reset, cancel and in-game isolation PASS");
}

static void reset_traits(void)
{
    memset(&race, 0, sizeof(race));
    memset(&profile, 0, sizeof(profile));
    for (int i = 0; i < CHARACTER_ABILITY_MAX; i++)
        profile.a_adj[i][0] = profile.a_adj[i][1] = -1;
}

static void expect_stats(bool strength)
{
    int stats[A_MAX], cost = 0;
    birth_recommended_stats(stats);
    assert(stats[A_STR] == (strength ? 2 : 1));
    assert(stats[A_DEX] == 2 && stats[A_CON] == 3);
    assert(stats[A_GRA] == (strength ? 1 : 2));
    for (int i = 0; i < A_MAX; i++) cost += birth_stat_costs[stats[i] + 4];
    assert(cost == 13 && cost == MAX_COST);
}

static void stats(void)
{
    reset_traits(); expect_stats(false);
    race.flags = RHF_MEL_AFFINITY; expect_stats(true);
    reset_traits(); profile.flags = RHF_ARC_AFFINITY; expect_stats(true);
    reset_traits(); profile.flags = RHF_SNG_AFFINITY; expect_stats(false);
    reset_traits(); race.r_adj[A_STR] = 1; expect_stats(true);
    profile.h_adj[A_GRA] = 2; expect_stats(false);
    reset_traits();
    profile.a_adj[0][0] = S_MEL; profile.a_adj[0][1] = 0;
    expect_stats(true);
    profile.flags = RHF_MEL_PENALTY; expect_stats(false);
    reset_traits();
    race.flags = RHF_MEL_AFFINITY;
    profile.a_adj[0][0] = S_SNG; profile.a_adj[0][1] = 0;
    profile.flags = RHF_SNG_AFFINITY;
    expect_stats(false);
    reset_traits();
    profile.a_adj[1][0] = S_MEL; profile.a_adj[1][1] = 0;
    expect_stats(false); /* Slots after the -1 terminator are ignored. */
    profile.a_adj[0][0] = S_MEL; profile.a_adj[0][1] = ABILITIES_MAX;
    profile.a_adj[1][0] = -1;
    expect_stats(false); /* An invalid ability cannot define a speciality. */
    puts("Birth attributes: combat/Grace affinities, abilities, penalties, stat tie-breaks and 13-point budget PASS");
}

static void advice(void)
{
    char text[1024];
    reset_traits();
    player.oath_type = 0;
    birth_skill_recommendation_text(text, sizeof(text));
    assert(strstr(text, "recover its XP"));
    assert(!strstr(text, "consider spending") && !strstr(text, "consider raising"));

    profile.flags = RHF_STL_AFFINITY;
    birth_skill_recommendation_text(text, sizeof(text));
    assert(strstr(text, "remaining XP on Stealth"));
    assert(!strstr(text, "consider raising Song"));

    profile.flags |= RHF_SNG_AFFINITY;
    birth_skill_recommendation_text(text, sizeof(text));
    assert(strstr(text, "remaining XP on Stealth"));
    assert(strstr(text, "consider raising Song"));
    assert(strstr(text, "know a song ability and actively sing it"));

    reset_traits();
    profile.a_adj[0][0] = S_SNG; profile.a_adj[0][1] = 0;
    birth_skill_recommendation_text(text, sizeof(text));
    assert(strstr(text, "consider raising Song"));

    reset_traits();
    profile.flags_u = UNQ_MINSTREL;
    birth_skill_recommendation_text(text, sizeof(text));
    assert(strstr(text, "consider raising Song"));
    player.oath_type = OATH_SILENCE;
    birth_skill_recommendation_text(text, sizeof(text));
    assert(strstr(text, "break your Oath of Silence"));
    assert(!strstr(text, "consider raising Song"));
    puts("Birth advice: conditional Stealth/Song, learned song plus active singing, and Silence oath PASS");
}

int main(void)
{
    skills();
    stats();
    advice();
    return 0;
}
