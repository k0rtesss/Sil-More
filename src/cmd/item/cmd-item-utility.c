#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"

void do_cmd_exchange(void)
{
    int y, x, dir;

    monster_type* m_ptr;
    monster_race* r_ptr;
    char m_name[80];

    if (!p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
    {
        msg_print(
            "You need the ability 'exchange places' to use this command.");
        return;
    }

    /*
     * Let the SDL frontend show and handle one-click exchange targets while
     * the classic direction prompt is active. Keyboard direction input still
     * follows the normal get_rep_dir() path.
     */
    sdl_player_exchange_begin_direction_prompt();

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
    {
        sdl_player_exchange_cancel_direction_prompt();
        return;
    }

    sdl_player_exchange_cancel_direction_prompt();

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    // deal with overburdened characters
    if (p_ptr->total_weight > weight_limit() * 3 / 2)
    {
        /* Abort */
        msg_print("You are too burdened to move.");

        return;
    }

    // Can't exchange from within pits
    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        /* Message */
        msg_print(
            "You would have to escape the pit before being able to exchange "
            "places.");

        return;
    }
    // Can't exchange from within webs
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        /* Message */
        msg_print(
            "You would have to escape the web before being able to exchange "
            "places.");

        return;
    }
    else if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
    {
        /* Message */
        msg_print("You cannot see a monster there to exchange places with.");

        return;
    }
    else if (cave_wall_bold(y, x))
    {
        /* Message */
        msg_print("You cannot enter the wall.");

        return;
    }
    else if (cave_any_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("You cannot enter the closed door.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Message */
        msg_print("You cannot enter the rubble.");

        return;
    }
    else
    {
        m_ptr = &mon_list[cave_m_idx[y][x]];
        r_ptr = &r_info[m_ptr->r_idx];

        if ((r_ptr->flags1 & (RF1_NEVER_MOVE))
            || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)))
        {
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Message */
            msg_format("You cannot get past %s.", m_name);

            return;
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];
    }

    // re-check for a visible monster (in case confusion changed the move)
    if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
    {
        /* Message */
        msg_print("You cannot see a monster there to exchange places with.");

        return;
    }

    else if (cave_wall_bold(y, x))
    {
        /* Message */
        msg_print("There is a wall in the way.");

        return;
    }
    else if (cave_any_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("There is a door in the way.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Message */
        msg_print("There is a pile of rubble in the way.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_CHASM)
    {
        /* Message */
        msg_print("You cannot exchange places over the chasm.");

        return;
    }

    // recalculate the monster info (in case confusion changed the move)
    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Message */
    msg_format("You exchange places with %s.", m_name);

    // attack of opportunity
    if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused
        && !(r_ptr->flags2 & (RF2_MINDLESS)))
    {
        msg_print("It attacks you as you slip past.");
        make_attack_normal(m_ptr);
    }

    // Alert the monster
    make_alert(m_ptr);

    // Swap positions with the monster
    monster_swap(p_ptr->py, p_ptr->px, y, x);

    /* Set off traps */
    if (cave_trap_bold(y, x) || (cave_feat[y][x] == FEAT_CHASM))
    {
        // If it is hidden
        if (cave_info[y][x] & (CAVE_HIDDEN))
        {
            /* Reveal the trap */
            reveal_trap(y, x);
        }

        /* Hit the trap */
        hit_trap(y, x);
    }
}


void do_cmd_swap_quivers(void)
{
    /* Compatibility shortcut: there is one mixed Quiver now, and its active
     * arrow stack is chosen through the shared active-weapon menu. */
    do_cmd_toggle_active_weapon();
}


/* Compatibility entry point for the retired staff-swap shortcut. */
void do_cmd_swap_staff(void)
{
    do_cmd_activate_staff(NULL, 0);
}
