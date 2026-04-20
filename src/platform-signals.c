/* File: signals.c */
/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"
#include "platform-signals.h"
#include "log/log.h"
#include "platform-frame.h"
#include "runtime/runtime-game.h"

#ifdef HANDLE_SIGNALS

#include <errno.h>
#include <signal.h>

/*
 * Wrapper around signal() which it is safe to take the address
 * of, in case signal itself is hidden by some some macro magic.
 */
static signal_handler_t wrap_signal(int sig, signal_handler_t handler)
{
    return signal(sig, handler);
}

/* Call this instead of calling signal() directly. */
signal_handler_t (*signal_aux)(int, signal_handler_t) = wrap_signal;

static void signal_present_status_message(cptr message)
{
    if (!message || !character_generated || !p_ptr)
        return;

    msg_print(message);
    handle_stuff();
    platform_frame_present();
}

/*
 * Handle signals -- suspend
 *
 * Actually suspend the game, and then resume cleanly
 */
static void handle_signal_suspend(int sig)
{
    /* Protect errno from library calls in signal handler */
    int save_errno = errno;

    /* Disable handler */
    (void)(*signal_aux)(sig, SIG_IGN);

#ifdef SIGSTOP

    if (character_generated && p_ptr)
        handle_stuff();
    platform_frame_present();

    /* Suspend the active host frame. */
    platform_frame_set_active(false);

    /* Suspend ourself */
    (void)kill(0, SIGSTOP);

    /* Resume and rebuild the visible frame. */
    platform_frame_set_active(true);
    if (character_generated && p_ptr)
        do_cmd_redraw();
    platform_frame_present();

#endif

    /* Restore handler */
    (void)(*signal_aux)(sig, handle_signal_suspend);

    /* Restore errno */
    errno = save_errno;
}

/*
 * Handle signals -- simple (interrupt and quit)
 *
 * This function was causing a *huge* number of problems, so it has
 * been simplified greatly.  We keep a global variable which counts
 * the number of times the user attempts to kill the process, and
 * we commit suicide if the user does this a certain number of times.
 *
 * We attempt to give "feedback" to the user as he approaches the
 * suicide thresh-hold, but without penalizing accidental keypresses.
 *
 * To prevent messy accidents, we should reset this global variable
 * whenever the user enters a keypress, or something like that.
 */
static void handle_signal_simple(int sig)
{
    /* Protect errno from library calls in signal handler */
    int save_errno = errno;

    /* Disable handler */
    (void)(*signal_aux)(sig, SIG_IGN);

    /* Nothing to save, just quit */
    if (!character_generated || character_saved)
        quit(NULL);

    /* Count the signals */
    signal_count++;

    /* Terminate dead characters */
    if (p_ptr->is_dead)
    {
        /* Mark the savefile */
        SDL_strlcpy(p_ptr->died_from, "Aborting", sizeof(p_ptr->died_from));

        /* Skip duplicate close/death handling if the tomb flow is already active. */
        if (!death_processing_in_progress())
        {
            /* Close stuff */
            close_game();
        }

        /* Quit */
        quit("interrupt");
    }

    /* Allow suicide (after 5) */
    else if (signal_count >= 5)
    {
        /* Cause of "death" */
        SDL_strlcpy(p_ptr->died_from, "Interrupting", sizeof(p_ptr->died_from));

        /* Commit suicide */
        p_ptr->is_dead = true;

        /* Stop playing */
        p_ptr->playing = false;

        /* Leaving */
        p_ptr->leaving = true;

        /* Close stuff */
        close_game();

        /* Quit */
        quit("interrupt");
    }

    /* Give warning (after 4) */
    else if (signal_count >= 4)
    {
        platform_frame_notify_noise();
        log_warn("handle_signal_simple: signal_count=%d", signal_count);
        signal_present_status_message("Contemplating suicide!");
    }

    /* Give warning (after 2) */
    else if (signal_count >= 2)
    {
        platform_frame_notify_noise();
    }

    /* Restore handler */
    (void)(*signal_aux)(sig, handle_signal_simple);

    /* Restore errno */
    errno = save_errno;
}

/*
 * Handle signal -- abort, kill, etc
 */
static void handle_signal_abort(int sig)
{
    char signal_text[64];
    char panic_from[64];

    strnfmt(signal_text, sizeof(signal_text), "signal %d", sig);
    log_error("handle_signal_abort: received %s", signal_text);

    /* Disable handler */
    (void)(*signal_aux)(sig, SIG_IGN);

    /* Nothing to save, just quit */
    if (!character_generated || character_saved)
        quit(NULL);

    signal_present_status_message(
        "A gruesome software bug LEAPS out at you! Panic save...");

    /* Panic save */
    p_ptr->panic_save = 1;
    strnfmt(panic_from, sizeof(panic_from), "(panic save: %s)", signal_text);
    SDL_strlcpy(p_ptr->died_from, panic_from, sizeof(p_ptr->died_from));

    /* Forbid suspend */
    signals_ignore_tstp();

    /* Attempt to save */
    if (save_player())
    {
        signal_present_status_message("Panic save succeeded!");
    }
    else
    {
        signal_present_status_message("Panic save failed!");
    }

    /* Quit */
    quit(format("software bug (%s)", signal_text));
}

/*
 * Ignore SIGTSTP signals (keyboard suspend)
 */
void signals_ignore_tstp(void)
{
#ifdef SIGTSTP
    (void)(*signal_aux)(SIGTSTP, SIG_IGN);
#endif
}

/*
 * Handle SIGTSTP signals (keyboard suspend)
 */
void signals_handle_tstp(void)
{
#ifdef SIGTSTP
    (void)(*signal_aux)(SIGTSTP, handle_signal_suspend);
#endif
}

/*
 * Prepare to handle the relevant signals
 */
void signals_init(void)
{
#ifdef SIGHUP
    (void)(*signal_aux)(SIGHUP, SIG_IGN);
#endif

#ifdef SIGTSTP
    (void)(*signal_aux)(SIGTSTP, handle_signal_suspend);
#endif

#ifdef SIGINT
    (void)(*signal_aux)(SIGINT, handle_signal_simple);
#endif

#ifdef SIGQUIT
    (void)(*signal_aux)(SIGQUIT, handle_signal_simple);
#endif

#if defined(__ANDROID__) || defined(SIL_IOS)
    log_warn("signals_init: mobile fatal signal panic interception disabled");
    return;
#endif

#ifdef SIGFPE
    (void)(*signal_aux)(SIGFPE, handle_signal_abort);
#endif

#ifdef SIGILL
    (void)(*signal_aux)(SIGILL, handle_signal_abort);
#endif

#ifdef SIGTRAP
    (void)(*signal_aux)(SIGTRAP, handle_signal_abort);
#endif

#ifdef SIGIOT
    (void)(*signal_aux)(SIGIOT, handle_signal_abort);
#endif

#ifdef SIGKILL
    (void)(*signal_aux)(SIGKILL, handle_signal_abort);
#endif

#ifdef SIGBUS
    (void)(*signal_aux)(SIGBUS, handle_signal_abort);
#endif

#ifdef SIGSEGV
    (void)(*signal_aux)(SIGSEGV, handle_signal_abort);
#endif

#ifdef SIGTERM
    (void)(*signal_aux)(SIGTERM, handle_signal_abort);
#endif

#ifdef SIGPIPE
    (void)(*signal_aux)(SIGPIPE, handle_signal_abort);
#endif

#ifdef SIGEMT
    (void)(*signal_aux)(SIGEMT, handle_signal_abort);
#endif

/*
 * SIGDANGER:
 * This is not a common (POSIX, SYSV, BSD) signal, it is used by AIX(?) to
 * signal that the system will soon be out of memory.
 */
#ifdef SIGDANGER
    (void)(*signal_aux)(SIGDANGER, handle_signal_abort);
#endif

#ifdef SIGSYS
    (void)(*signal_aux)(SIGSYS, handle_signal_abort);
#endif

#ifdef SIGXCPU
    (void)(*signal_aux)(SIGXCPU, handle_signal_abort);
#endif

#ifdef SIGPWR
    (void)(*signal_aux)(SIGPWR, handle_signal_abort);
#endif
}

#else /* HANDLE_SIGNALS */

/*
 * Do nothing
 */
void signals_ignore_tstp(void) { }

/*
 * Do nothing
 */
void signals_handle_tstp(void) { }

/*
 * Do nothing
 */
void signals_init(void) { }

#endif /* HANDLE_SIGNALS */
