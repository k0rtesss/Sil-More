#!/usr/bin/env python3
"""Check move-to-popup ordering through the real player turn, using temporary data.

Run after build-incremental.ps1. Reuses the isolated engine/template fixture;
never opens a player save or configuration.
"""
import check_new_monsters as engine

CHECKS = r'''
#include "tutorial/tutorial.h"
void process_player(void);

static int input_requests;
static int hint_refreshes;
static errr popup_terminal_extra(int action, int value)
{
    (void)value;
    if (action == TERM_XTRA_FRESH
        && sdl_question_menu_context_hint_active()) {
        assert(p_ptr->py == 10 && p_ptr->px == 12);
        hint_refreshes++;
    }
    if (action == TERM_XTRA_EVENT) {
        assert(++input_requests < 20);
        Term_keypress(';');
        Term_keypress('5');
    }
    return 0;
}

static void popup_move_fixture(bool item)
{
    memset(p_ptr, 0, sizeof(*p_ptr));
    memset(inventory, 0, INVEN_TOTAL * sizeof(*inventory));
    reset_map(5);
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
            if (!y || !x || y == p_ptr->cur_map_hgt - 1
                || x == p_ptr->cur_map_wid - 1) {
                cave_feat[y][x] = FEAT_WALL_PERM;
                cave_info[y][x] |= CAVE_WALL;
            }
    p_ptr->py = 10; p_ptr->px = 11;
    cave_m_idx[10][11] = -1;
    p_ptr->chp = p_ptr->mhp = 100;
    p_ptr->food = PY_FOOD_FULL;
    p_ptr->playing = p_ptr->restoring = true;
    p_ptr->energy = 100;
    character_dungeon = true;
    tutorial_set_enabled(false);
    character_icky = 0;
    playerturn = 100;
    hjkl_movement = angband_keyset = false;
    set_sdl_show_context_square_popups(true);
    sdl_question_menu_clear();
    Term_flush();
    inkey_next_set(NULL);
    inkey_xtra = false;
    input_requests = 0;
    hint_refreshes = 0;
    test_term.xtra_hook = popup_terminal_extra;
    if (item) {
        int kind = 0;
        for (int i = 1; i < z_info->k_max; ++i)
            if (k_info[i].tval == TV_SWORD) { kind = i; break; }
        assert(kind);
        object_prep(&o_list[1], kind);
        o_list[1].iy = 10; o_list[1].ix = 12;
        o_list[1].marked = true;
        o_list[1].pickup = false;
        cave_o_idx[10][12] = 1;
        o_max = 2; o_cnt = 1;
    }
}

static void move_once(void)
{
    Term_keypress(';');
    Term_keypress('6');
    process_player();
    assert(p_ptr->py == 10 && p_ptr->px == 12);
    assert(playerturn == 101 && p_ptr->energy == 0);
    assert(!inkey_flag);
}

static void check_popup_timing(void)
{
    puts("Checking real movement popup ordering...");
    popup_move_fixture(true);
    move_once();
    /* No monster/world pass or second player input turn has run yet. */
    assert(sdl_question_menu_context_hint_active());
    assert(hint_refreshes > 0);
    assert(cave_o_idx[10][12] == 1);
    puts("PASS: real movement refreshes the prepared popup before monster/world processing.");

    /* Reproduce the logged case: the player frame has already consumed all
     * terminal damage, so only the native popup is dirty. */
    Term_fresh();
    assert(Term->y1 > Term->y2);
    hint_refreshes = 0;
    assert(do_cmd_context_square_action_popup());
    assert(hint_refreshes == 1);
    puts("PASS: a popup-only change refreshes even with a clean terminal buffer.");

    popup_move_fixture(false);
    move_once();
    assert(!sdl_question_menu_is_active());
    assert(hint_refreshes == 0);

    popup_move_fixture(true);
    set_sdl_show_context_square_popups(false);
    move_once();
    assert(!sdl_question_menu_is_active());

    popup_move_fixture(true);
    do_cmd_suppress_context_square_popups();
    move_once();
    assert(!sdl_question_menu_is_active());
    /* Expire suppression for the remaining independent cases. */
    playerturn = 110;
    assert(do_cmd_context_square_action_popup());
    sdl_question_menu_clear();

    popup_move_fixture(true);
    cave_feat[10][12] = FEAT_WALL_EXTRA;
    cave_info[10][12] |= CAVE_WALL | CAVE_MARK;
    Term_keypress(';');
    Term_keypress('6');
    Term_keypress(';');
    Term_keypress('5'); /* A known wall can refund the attempted step. */
    process_player();
    assert(p_ptr->py == 10 && p_ptr->px == 11);
    assert(!sdl_question_menu_is_active());

    popup_move_fixture(false);
    cave_feat[10][12] = FEAT_MORE;
    move_once();
    assert(sdl_question_menu_context_hint_active());
    assert(hint_refreshes > 0);
    puts("PASS: empty/blocked squares, disabled/suppressed hints, and stairs.");
}
'''


def main():
    engine.OUT = engine.ROOT / "scripts/output/context-popup-timing"
    prefix, main_body = engine.HARNESS.split("int main(int argc,char** argv)", 1)
    initialization = main_body.split("    check_templates();", 1)[0]
    engine.HARNESS = (prefix + CHECKS + "\nint main(int argc,char** argv)"
                      + initialization + "    check_popup_timing();\n"
                      + "    SDL_Quit();\n    return 0;\n}\n")
    engine.check_layouts = lambda: None
    engine.main()


if __name__ == "__main__":
    main()
