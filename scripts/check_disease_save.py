#!/usr/bin/env python3
"""Roundtrip disease immunity through complete production game saves.

Uses the initialized engine fixture with temporary profiles, not player data.
Run after building the standard Windows target.
"""
from pathlib import Path

import check_song_target_save as fixture

ROOT = Path(__file__).resolve().parents[1]

CHECKS = r'''
static void checks(void)
{
    log_set_quiet(false);
    log_set_level(LOG_INFO);
    const struct { byte immunity; int countdown; bool hardiness; bool indomitable; } cases[] = {
        {0, 0, false, false}, {255, 0, false, false}, {255, 0, true, false},
        {1, 300, false, false}, {85, 151, false, false}, {127, 1, false, false},
        {1, 300, true, false}, {85, 151, true, false}, {127, 1, true, false},
        {1, 300, false, true}, {1, 300, true, true}
    };
    build_randart_tables();
    flavor_init();
    for (unsigned n = 0; n < N_ELEMENTS(cases); n++)
    {
        printf("Full-save case %u: immunity=%u countdown=%d Hardiness=%d\n",
            n, cases[n].immunity, cases[n].countdown, cases[n].hardiness);
        player_wipe();
        assert(!p_ptr->diseased && !p_ptr->disease_immunity);
        reset_map(5);
        turn = playerturn = 100;
        character_generated = true;
        p_ptr->playing = true;
        p_ptr->chp = p_ptr->mhp = 100;
        p_ptr->csp = p_ptr->msp = 100;
        p_ptr->food = PY_FOOD_FULL;
        cave_m_idx[p_ptr->py][p_ptr->px] = -1;
        SDL_strlcpy(op_ptr->full_name, "Disease immunity", sizeof(op_ptr->full_name));
        p_ptr->disease_immunity = cases[n].immunity;
        p_ptr->diseased = cases[n].countdown;
        p_ptr->disease_name = cases[n].countdown ? 8 : 0;
        p_ptr->disease_cure = cases[n].countdown ? SV_FOOD_SICKNESS : 0;
        p_ptr->disease_knowledge = cases[n].countdown
            ? DISEASE_KNOWN_NAME | DISEASE_KNOWN_CURE : 0;
        p_ptr->stat_disease[A_CON] = cases[n].countdown ? -1 : 0;
        p_ptr->stat_drain[A_CON] = -2;
        p_ptr->innate_ability[S_WIL][WIL_HARDINESS] = cases[n].hardiness;
        p_ptr->have_ability[S_WIL][WIL_HARDINESS] = cases[n].hardiness;
        p_ptr->active_ability[S_WIL][WIL_HARDINESS] = cases[n].hardiness;
        p_ptr->innate_ability[S_WIL][WIL_INDOMITABLE] = cases[n].indomitable;
        p_ptr->have_ability[S_WIL][WIL_INDOMITABLE] = cases[n].indomitable;
        p_ptr->active_ability[S_WIL][WIL_INDOMITABLE] = cases[n].indomitable;
        strnfmt(savefile, sizeof(savefile), "disease-%u", n);
        assert(save_player());
        p_ptr->disease_immunity = 0;
        p_ptr->diseased = 0;
        p_ptr->disease_name = 0;
        assert(load_player() && character_loaded && !p_ptr->is_dead);
        assert(p_ptr->disease_immunity == cases[n].immunity);
        assert(p_ptr->diseased == cases[n].countdown);
        assert(p_ptr->disease_name == (cases[n].countdown ? 8 : 0));
        assert(p_ptr->disease_cure == (cases[n].countdown ? SV_FOOD_HEALING : 0));
        assert(p_ptr->stat_disease[A_CON] == (cases[n].countdown ? -1 : 0));
        assert(p_ptr->stat_drain[A_CON] == -2);
        assert(!!p_ptr->active_ability[S_WIL][WIL_HARDINESS] == cases[n].hardiness);
        assert(!!p_ptr->active_ability[S_WIL][WIL_INDOMITABLE] == cases[n].indomitable);
        byte expected_immunity = cases[n].immunity;
        if (cases[n].countdown)
        {
            int remaining = cases[n].countdown - (cases[n].hardiness ? 1 : 2);
            process_disease();
            assert(p_ptr->diseased == (remaining > 0 ? remaining : 300));
            assert(cure_disease());
            expected_immunity |= 128;
            assert(p_ptr->disease_immunity == expected_immunity);
            assert(p_ptr->stat_drain[A_CON] == -2);
        }
        assert(save_player());
        p_ptr->disease_immunity = 0;
        assert(load_player() && p_ptr->disease_immunity == expected_immunity);
        assert(!p_ptr->diseased);
    }
    player_wipe();
    assert(!p_ptr->diseased && !p_ptr->disease_immunity);
    puts("PASS: 22 full game-save roundtrips; healthy/all-immune/active infections, odd timers, Hardiness at the original Poison Resistance slot, independent Indomitable, cure immunity, unrelated drain, and real new-character reset.");
}
'''


def main():
    fixture.OUT = ROOT / "scripts/output/disease-save-check"
    initialization = fixture.CHECKS[fixture.CHECKS.index("int main("):]
    fixture.CHECKS = CHECKS + initialization
    fixture.main()


if __name__ == "__main__":
    main()
