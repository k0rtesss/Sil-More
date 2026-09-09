#include "angband.h"
#include "support/feedback.h"
#include "externs.h"
#include "sdl-sound.h"

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

/* Nearby unseen monsters can be heard, but audio does not reveal their grid or
 * alter the gameplay noise/detection system. */
void monster_sound(const monster_type* m_ptr, int action)
{
    int range = action == MONSTER_SOUND_IDLE ? 10 : MAX_SIGHT;
    if (!use_sound || !m_ptr || !m_ptr->r_idx
        || distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) > range)
        return;
    if (action == MONSTER_SOUND_IDLE && m_ptr->alertness < ALERTNESS_UNWARY)
        return;
    sdl_sound_monster(m_ptr->r_idx, action);
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
