#include "angband.h"
#include "support/feedback.h"
#include "externs.h"
#include "sdl-sound.h"

#define MONSTER_IDLE_SOUND_MAX_PATH 10

/*
 * Flush the screen, make a noise
 */
void bell(cptr reason)
{
    /* Mega-Hack -- Flush the output */
    Term_fresh();

    if (character_generated && reason)
    {
        message_add(reason, MSG_BELL);

        /* Window stuff */
        p_ptr->window |= (PW_MESSAGE);

        /* Force window redraw */
        window_stuff();
    }

    /* Make a bell noise (if allowed) */
    if (system_beep)
        Term_xtra(TERM_XTRA_NOISE, 0);

    /* Flush the input (later!) */
    flush();
}

/*
 * Hack -- Make a (relevant?) sound
 */
void sound(int val)
{
    /* No sound */
    if (!use_sound)
        return;

    /* Route directly to SDL sound backend */
    sdl_sound_handle(val);
}

/* Play a one-shot environmental sound with the same distance curve as the
 * ambient water, lava, and forge loops. */
void sound_at_environment_level(int val, int level, int max_level)
{
    if (!use_sound)
        return;

    sdl_sound_handle_at_environment_level(val, level, max_level);
}

/* Nearby unseen monsters can be heard, but audio does not reveal their grid or
 * alter the gameplay noise/detection system. Idle sounds use the gameplay
 * noise flow for walls and doors; other monster sounds keep the broad audio
 * range used by their existing feedback. */
static void monster_sound_internal(const monster_type* m_ptr, int action,
    bool force_idle)
{
    if (!use_sound || !m_ptr || !m_ptr->r_idx)
        return;

    if (action == MONSTER_SOUND_IDLE)
    {
        if (!force_idle && (m_ptr->alertness < ALERTNESS_UNWARY
            || flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx)
                > MONSTER_IDLE_SOUND_MAX_PATH))
            return;
    }
    else if (distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) > MAX_SIGHT)
    {
        return;
    }

    if (force_idle)
        sdl_sound_monster_force(m_ptr->r_idx, action);
    else
        sdl_sound_monster(m_ptr->r_idx, action);
}

void monster_sound(const monster_type* m_ptr, int action)
{
    monster_sound_internal(m_ptr, action, false);
}

void monster_sound_force(const monster_type* m_ptr, int action)
{
    monster_sound_internal(m_ptr, action, true);
}

/*
 * Schedule a sound to play after delay_ms milliseconds without blocking.
 * Use this when you want an audio gap between sounds (e.g. weapon swing
 * then hit "thunk") without freezing the game loop.
 */
void sound_delayed(int val, unsigned int delay_ms)
{
    if (!use_sound)
        return;

    sdl_sound_handle_delayed(val, (Uint32)delay_ms);
}
