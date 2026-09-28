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

void sound_at(int val, int y, int x)
{
    if (use_sound)
        sdl_sound_handle_at(val, y, x);
}

/* Sleeping monsters remain silent. Forced trap cues skip the idle probability
 * and sleep gates, but never bypass spatial attenuation. */
static void monster_sound_internal(const monster_type* m_ptr, int action,
    bool force_idle)
{
    if (!use_sound || !m_ptr || !m_ptr->r_idx)
        return;

    if (action == MONSTER_SOUND_IDLE && !force_idle
        && m_ptr->alertness < ALERTNESS_UNWARY)
        return;

    sdl_sound_monster_at(m_ptr->r_idx, action, m_ptr->fy, m_ptr->fx, force_idle);
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

void sound_delayed_at(int val, unsigned int delay_ms, int y, int x)
{
    if (use_sound)
        sdl_sound_handle_delayed_at(val, (Uint32)delay_ms, y, x);
}
