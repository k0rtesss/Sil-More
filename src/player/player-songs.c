/* File: player/player-songs.c */

#include "angband.h"
#include "monster/monster-ai.h"
#include "tutorial/tutorial-game.h"
#include "externs.h"
#include "player/player-song-internal.h"
#include "log/log.h"
#include "meta_state.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

static int trees_song_light_bonus(void)
{
    return singing(SNG_TREES) ? ability_bonus(S_SNG, SNG_TREES) : 0;
}

static void update_trees_song_light(int previous_bonus)
{
    if (trees_song_light_bonus() != previous_bonus)
        p_ptr->update |= PU_TORCH;
}

void change_song(int song)
{
    if (song != SNG_NOTHING && !tutorial_game_action_allowed("song", NULL)) return;
    int song_to_change;
    int previous_trees_bonus = trees_song_light_bonus();
    int old_song;
    bool new_song_is_duel;
    bool old_song_is_duel;

    if (song == SNG_EXCHANGE_THEMES
        && (song_is_duel(p_ptr->song1) || song_is_duel(p_ptr->song2)))
    {
        msg_print("That song must remain your main theme.");
        return;
    }

    if (p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]
        && (p_ptr->song1 != SNG_NOTHING) && (song != SNG_NOTHING))
    {
        song_to_change = 2;
        old_song = p_ptr->song2;
    }
    else
    {
        song_to_change = 1;
        old_song = p_ptr->song1;
    }

    // attempting to change to the main song again stops singing
    if (p_ptr->song1 == song)
    {
        song_to_change = 1;
        old_song = p_ptr->song1;
        song = SNG_NOTHING;
    }

    // attempting to change minor theme to itself cancels the minor theme
    else if ((song_to_change == 2) && (p_ptr->song2 == song))
    {
        song = SNG_NOTHING;
    }

    new_song_is_duel = song_is_duel(song);
    old_song_is_duel = song_is_duel(old_song);

    if ((song_to_change == 2) && new_song_is_duel)
    {
        msg_print("That song cannot be woven as a minor theme.");
        return;
    }

    if ((song_to_change == 1) && new_song_is_duel)
    {
        if (p_ptr->song_lockout_timer > 0)
        {
            msg_print("Your voice has not yet recovered for such a song.");
            return;
        }
        if (!song_duel_select_target(song))
            return;
    }

    if ((song_to_change == 1) && old_song_is_duel && !new_song_is_duel)
    {
        song_duel_clear_player_target();
        song_duel_reset_player_stack();
    }

    // Recalculate various bonuses
    p_ptr->redraw |= (PR_SONG);
    p_ptr->update |= (PU_BONUS);

    // swap the minor and major themes
    if (song == SNG_EXCHANGE_THEMES)
    {
        p_ptr->song2 = p_ptr->song1;
        p_ptr->song1 = old_song;
        update_trees_song_light(previous_trees_bonus);

        msg_print("You change the order of your themes.");

        /* Take time */
        p_ptr->energy_use = 100;

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        return;
    }

    // Reset the song duration counter if changing major theme
    if (song_to_change == 1)
    {
        p_ptr->song_duration = 0;
    }

    switch (song)
    {
    case SNG_NOTHING:
    {
        if (song_disguise_is_active())
            song_disguise_stop();

        if ((song_to_change == 1) && (p_ptr->song1 != SNG_NOTHING))
        {
            msg_print("You end your song.");
        }
        else if ((song_to_change == 2) && (p_ptr->song2 != SNG_NOTHING))
        {
            msg_print("You end your minor theme.");
        }
        break;
    }
    case SNG_ELBERETH:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song to the Queen of the Stars.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme about the Queen of the Stars.");
        }
        else
        {
            msg_print("You change your minor theme to one about the Queen of "
                      "the Stars.");
        }
        break;
    }
    case SNG_CHALLENGE:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a strident song of mockery and scorn.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of mockery and scorn.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one of mockery and scorn.");
        }
        break;
    }
    case SNG_FREEDOM:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song of freedom and safe passage.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of freedom and safe passage.");
        }
        else
        {
            msg_print("You change your minor theme to one of freedom and safe "
                      "passage.");
        }
        break;
    }
    case SNG_STAUNCHING:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a murmuring song of soft and soothing words.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of soft and soothing words.");
        }
        else
        {
            msg_print("You change your minor theme to one of soft and soothing "
                      "words.");
        }
        msg_print("You feel your wounds close and your body heal.");
        break;
    }
    case SNG_SILENCE:
    {
        if (song_to_change == 1)
        {
            msg_print("You whisper a song of silence.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of silence.");
        }
        else
        {
            msg_print("You change your minor theme to one of silence.");
        }
        break;
    }
    case SNG_DELVINGS:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song about the rocky bones of the earth.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print(
                "You add a minor theme about the rocky bones of the earth.");
        }
        else
        {
            msg_print("You change your minor theme to one about the rocky "
                      "bones of the "
                      "earth.");
        }
        break;
    }
    case SNG_REVEALING:
    {
        if (song_to_change == 1)
        {
            msg_print("You weave a song to unveil hidden life and treasure.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme that seeks what lies concealed.");
        }
        else
        {
            msg_print("You shift your minor theme toward revealing secrets.");
        }
        break;
    }
    case SNG_TREES:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song about the Two Trees of Valinor.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme about the Two Trees of Valinor.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one about the Two Trees of "
                "Valinor.");
        }
        msg_print("A memory of their light wells up around you.");
        break;
    }
    case SNG_ELVENESS:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a lilting song celebrating the grace of the Eldar.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme celebrating the grace of the Eldar.");
        }
        else
        {
            msg_print("You change your minor theme to honor the grace of the Eldar.");
        }
        break;
    }
    case SNG_DISGUISE:
    {
        if (old_song != SNG_DISGUISE)
        {
            if (song_disguise_any_monster_observes_player())
            {
                msg_print("You cannot begin the Song of Disguise while observed.");
                return;
            }
        }

        if (song_to_change == 1)
        {
            msg_print("You begin a soft song of misdirection and guile.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme weaving subtle disguises.");
        }
        else
        {
            msg_print("You change your minor theme to one of misdirection and guile.");
        }
        break;
    }
    case SNG_STAYING:
    {
        if (song_to_change == 1)
        {
            msg_print(
                "You begin a song about the courage of great heroes past.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme about the courage of great heroes "
                      "past.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one about the courage of great "
                "heroes past.");
        }
        break;
    }
    case SNG_SLAYING:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song of fury and dread.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of fury and dread.");
        }
        else
        {
            msg_print("You change your minor theme to one of fury and dread.");
        }
        break;
    }
    case SNG_LORIEN:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a soothing song about weariness and rest.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme about weariness and rest.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one about weariness and rest.");
        }
        break;
    }
    case SNG_THRESHOLDS:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song of ways guarded and impassable.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of ways guarded and impassable.");
        }
        else
        {
            msg_print("You change your minor theme to one of ways guarded and "
                      "impassable.");
        }
        break;
    }
    case SNG_MASTERY:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a song of mastery and command.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of mastery and command.");
        }
        else
        {
            msg_print(
                "You change your minor theme to one of mastery and command.");
        }
        break;
    }
    case SNG_SHATTERING:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a fell song of breaking and sundering.");
        }
        else if (old_song == SNG_NOTHING)
        {
            msg_print("You add a minor theme of breaking and sundering.");
        }
        else
        {
            msg_print("You change your minor theme to one of breaking and sundering.");
        }
        break;
    }
    case SNG_CONTEST:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a piercing song of contest and rivalry.");
        }
        break;
    }
    case SNG_LAMENT:
    {
        if (song_to_change == 1)
        {
            msg_print("You begin a sorrowful song of loss and lament.");
        }
        break;
    }
    }

    // Actually set the song
    if (song_to_change == 1)
    {
        p_ptr->song1 = song;
    }
    if ((song_to_change == 2) || (song == SNG_NOTHING))
    {
        p_ptr->song2 = song;
    }

    update_trees_song_light(previous_trees_bonus);

    if ((song_to_change == 1) && new_song_is_duel && (song != SNG_NOTHING))
    {
        monster_type* m_ptr = song_duel_get_target(song);
        if (m_ptr)
        {
            song_duel_learn_target_stats(m_ptr, song);
            song_duel_reveal_target_stats(m_ptr, song);
        }
    }

    // Display synergy message if a woven theme pair is detected
    tutorial_game_action_done("song", NULL);

    // Display synergy message if a woven theme pair is detected
    if (song != SNG_NOTHING && song_to_change == 2)
    {
        display_synergy_message(p_ptr->song1, p_ptr->song2);
    }

    if (!singing(SNG_DISGUISE) && song_disguise_is_active())
        song_disguise_stop();

    // beginning/changing songs takes time
    if (song != SNG_NOTHING)
    {
        /* Take time */
        p_ptr->energy_use = 100;
        monster_ai_witness(MON_AI_SONG, 1, p_ptr->py, p_ptr->px);

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;
    }
}

bool singing(int song)
{
    if (song == SNG_NOTHING)
    {
        if (p_ptr->song1 == song)
            return (true);
    }
    else
    {
        if (p_ptr->song1 == song)
            return (true);
        if (p_ptr->song2 == song)
            return (true);
    }

    return (false);
}

static const ability_type* song_ability_data(int song)
{
    int index;
    ability_type* b_ptr;

    if (song < 0 || song >= SNG_MAX || !b_info || !z_info)
        return NULL;

    index = ability_index(S_SNG, song);
    if (index < 0 || index >= z_info->b_max)
        return NULL;

    b_ptr = &b_info[index];
    if (b_ptr->skilltype != S_SNG || b_ptr->abilitynum != song)
        return NULL;

    return b_ptr;
}

cptr song_voice_cost_desc(int song)
{
    static char desc[64];
    const ability_type* b_ptr = song_ability_data(song);

    if (!b_ptr || !b_ptr->voice_cost || !b_ptr->voice_cost_interval)
        return NULL;

    if (b_ptr->voice_cost_interval == 1)
    {
        strnfmt(desc, sizeof(desc), "%d Voice per turn", b_ptr->voice_cost);
    }
    else
    {
        strnfmt(desc, sizeof(desc), "%d Voice per %d turns",
            b_ptr->voice_cost, b_ptr->voice_cost_interval);
    }

    return desc;
}

static int song_voice_cost_for_turn(int song, int theme_slot, int song_duration)
{
    const ability_type* b_ptr = song_ability_data(song);

    if (!b_ptr || !b_ptr->voice_cost || !b_ptr->voice_cost_interval)
        return 0;

    /* Duel songs cannot be minor themes, but keep their primary-only cost
     * here as a safe guard for any stale or externally restored song state. */
    if (song_is_duel(song) && theme_slot != 1)
        return 0;

    if (b_ptr->voice_cost_interval == 1)
        return b_ptr->voice_cost;

    /* Stagger slower costs between the primary and minor theme. */
    return ((song_duration % b_ptr->voice_cost_interval) == theme_slot - 1)
        ? b_ptr->voice_cost : 0;
}

static bool player_can_sustain_song(int song)
{
    if (song == SNG_NOTHING)
        return true;
    if (song < 0 || song >= SNG_MAX)
        return false;

    return p_ptr->active_ability[S_SNG][song]
        || legendary_area_song_is_available(song);
}

void sing(void)
{
    int type;
    int previous_trees_bonus = trees_song_light_bonus();
    int song = p_ptr->song1; // a default to soothe compilation warnings
    int score = 0;
    int effective_score = 0;
    int cost = 0;
    bool abort_song = false;

    song_revealing_decay();

    if (p_ptr->song1 == SNG_NOTHING)
    {
        song_revealing_clear();

        if (song_disguise_is_active())
            song_disguise_stop();
        return;
    }

    // Losing a minor theme or its weaving source does not invalidate the
    // independently sustained main theme (as with switching Woven Themes off).
    if ((p_ptr->song2 != SNG_NOTHING)
        && (!p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]
            || !player_can_sustain_song(p_ptr->song2)))
    {
        p_ptr->song2 = SNG_NOTHING;
        p_ptr->redraw |= PR_SONG;
        p_ptr->update |= PU_BONUS;
        update_trees_song_light(previous_trees_bonus);
    }

    // Abort both themes if voice or the main song itself is unavailable.
    if ((p_ptr->csp < 1)
        || (!player_can_sustain_song(p_ptr->song1)))
    {
        /* Stop singing */
        if (song_disguise_is_active())
            song_disguise_stop();
        change_song(SNG_NOTHING);

        /* Disturb */
        disturb(0, 0);
        return;
    }
    else
    {
        p_ptr->song_duration++;
    }

    if (singing(SNG_DISGUISE))
    {
        if (!song_disguise_is_active())
            song_disguise_start();
    }
    else if (song_disguise_is_active())
    {
        song_disguise_stop();
    }

    for (type = 1; type <= 2; type++)
    {
        if (type == 1)
            song = p_ptr->song1;
        if (type == 2)
            song = p_ptr->song2;

        score = ability_bonus(S_SNG, song);
        effective_score = (song != SNG_NOTHING)
            ? song_effective_skill(song) : 0;
        if (song != SNG_NOTHING)
            legendary_song_observe_begin(song, effective_score);
        cost += song_voice_cost_for_turn(song, type, p_ptr->song_duration);

        switch (song)
        {
        case SNG_ELBERETH:
        {
            sing_song_of_elbereth(score);

            // Maintain the lingering effect counter while singing
            // Duration scales with song skill: 15 turns at skill 20
            // Formula: (skill * 3) / 4
            int duration = (score * 3) / 4;
            if (duration < 3) duration = 3; // Minimum 3 turns
            p_ptr->song_elbereth_effect = duration;

            break;
        }
        case SNG_CHALLENGE:
        {
            sing_song_of_challenge(score);

            // Maintain the lingering effect counter while singing
            // Duration scales with song skill: 15 turns at skill 20
            // Formula: (skill * 3) / 4
            int duration = (score * 3) / 4;
            if (duration < 3) duration = 3; // Minimum 3 turns
            p_ptr->song_challenge_effect = duration;

            break;
        }
        case SNG_FREEDOM:
        {
            sing_song_of_freedom(score);
            break;
        }
        case SNG_STAUNCHING:
        {
            int cycle = p_ptr->song_duration % 12;
            int song_frac = score % 12;
            int bonus_hp = 0;

            set_cut(0);

            if ((cycle * song_frac) % 12 < song_frac)
                bonus_hp = 1;

            bonus_hp += (score / 12);

            p_ptr->chp += bonus_hp;

            if (p_ptr->chp > p_ptr->mhp)
                p_ptr->chp = p_ptr->mhp;

            break;
        }
        case SNG_SILENCE:
        {
            break;
        }
        case SNG_THRESHOLDS:
        {
            break;
        }
        case SNG_DELVINGS:
        {
            sing_song_of_delvings(score);

            break;
        }
        case SNG_REVEALING:
        {
            sing_song_of_revealing(score);

            break;
        }
        case SNG_TREES:
        {
            sing_song_of_trees(effective_score);
            
            break;
        }
        case SNG_ELVENESS:
        {
            break;
        }
        case SNG_STAYING:
        {
            break;
        }
        case SNG_DISGUISE:
        {
            sing_song_of_disguise(score);
            break;
        }
        case SNG_SLAYING:
        {
            break;
        }
        case SNG_LORIEN:
        {
            sing_song_of_lorien(score);

            break;
        }
        case SNG_CONTEST:
        {
            if (type == 1)
            {
                if (!song_duel_process_contest(score))
                    abort_song = true;
            }
            break;
        }
        case SNG_LAMENT:
        {
            if (type == 1)
            {
                if (!song_duel_process_lament(score))
                    abort_song = true;
            }
            break;
        }
        case SNG_MASTERY:
        {
            break;
        }
        case SNG_SHATTERING:
        {
            sing_song_of_shattering(score);
            break;
        }
        }

        if (song != SNG_NOTHING)
            legendary_song_observe_end(song, effective_score);

        if (abort_song)
            break;
    }

    // pay the price of the singing
    if (p_ptr->csp >= cost)
        p_ptr->csp -= cost;
    else
        p_ptr->csp = 0;

    p_ptr->redraw |= (PR_VOICE);
    p_ptr->redraw |= (PR_HP);
}
