#include "angband.h"
#include "log/log.h"
#include "player/killer.h"

#include <time.h>

void do_cmd_escape(int silmarils)
{
    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[120];

    p_ptr->escaped = true;
    input_clear_pending();
    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;

    (void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));
    sprintf(buf, "You escaped the Iron Hells on %s.", long_day);
    do_cmd_note(buf, p_ptr->depth);

    switch (silmarils)
    {
    case 0:
        do_cmd_note("You returned empty handed.", p_ptr->depth);
        break;
    case 1:
        do_cmd_note("You brought back a Silmaril from Morgoth's crown!",
            p_ptr->depth);
        break;
    case 2:
        do_cmd_note("You brought back two Silmarils from Morgoth's crown!",
            p_ptr->depth);
        break;
    case 3:
        do_cmd_note(
            "You brought back all three Silmarils from Morgoth's crown!",
            p_ptr->depth);
        break;
    default:
        do_cmd_note("You brought back so many Silmarils that people should be suspicious!",
            p_ptr->depth);
        break;
    }

    if (p_ptr->oath_type > 0)
    {
        if (oath_invalid(p_ptr->oath_type))
        {
            char* death_msg = oath_death_message(p_ptr->oath_type);
            if (death_msg && death_msg[0])
            {
                do_cmd_note(death_msg, p_ptr->depth);
            }
            else
            {
                do_cmd_note(
                    "You passed from the world, but the stain of a faithless heart remains. You will be remembered not for your deeds, but as a shameful Oathbreaker.",
                    p_ptr->depth);
            }
        }
        else
        {
            do_cmd_note("You kept your oath to the very end.", p_ptr->depth);
        }
    }

    SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));
    SDL_strlcpy(p_ptr->died_from, "ripe old age", sizeof(p_ptr->died_from));

    log_info("Player escaped with %d Silmarils (metarun processing deferred until close_game_aux)",
        silmarils);
}

void do_cmd_suicide(void)
{
    char ch;

    input_clear_pending();

    if (!get_check("This will destroy the current character: are you sure? "))
        return;

    if (!get_com("Please verify ABORTING by typing the '~' sign: ", &ch))
        return;
    if (ch != '~')
        return;

    p_ptr->is_dead = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;

    SDL_strlcpy(p_ptr->died_from, "their own hand", sizeof(p_ptr->died_from));
    killer_mark_other(SCORE_KILLER_SELF);
    killer_commit(p_ptr->died_from);
}
