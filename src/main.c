/* File: main.c */
/*
 * Copyright (c) 1997 Ben Harrison, and others
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
#include "app/app-session.h"
#include "fs/file.h"
#include "init/init-lifecycle.h"
#include "log/bootstrap.h"
#include "level-generation/gen-log.h"

/*
 * Some machines have a "main()" function in their "main-xxx.c" file,
 * all the others use this file for their "main()" function.
 */

#include "main.h"
#include "log/log.h"
#include "platform-frame.h"
#include "runtime/runtime-game.h"
#include "runtime/runtime-cli.h"
#include "platform-audio.h"
#include <SDL3/SDL_filesystem.h>

#ifdef SIL_IOS
#include <SDL3/SDL_main.h>
#endif

/*
 * Sil-y: game in progress
 */
bool game_in_progress = false;

/*
 * A hook for "quit()".
 *
 * Close down, then fall back into "quit()".
 */
static void quit_hook(cptr s)
{
    /* Unused parameter */
    (void)s;

    platform_frame_shutdown_views();
}

/*
 * Initialize and verify the file paths, and the score file.
 *
 * Use the ANGBAND_PATH environment var if possible, else use
 * DEFAULT_PATH, and in either case, branch off appropriately.
 *
 * First, we'll look for the ANGBAND_PATH environment variable,
 * and then look for the files in there.  If that doesn't work,
 * we'll try the DEFAULT_PATH constant.  So be sure that one of
 * these two things works...
 *
 * We must ensure that the path ends with "PATH_SEP" if needed,
 * since the "init_file_paths()" function will simply append the
 * relevant "sub-directory names" to the given path.
 *
 * Make sure that the path doesn't overflow the buffer.  We have
 * to leave enough space for the path separator, directory, and
 * filenames.
 */
static void init_stuff(void)
{
    char path[1024];

    cptr tail = NULL;

#ifdef SIL_IOS
    {
        char* base = SDL_GetBasePath();
        if (base)
        {
            SDL_strlcpy(path, base, sizeof(path));
            SDL_strlcat(path, "lib/", sizeof(path));
            SDL_free(base);
            tail = path;
        }
    }
#endif

#ifndef FIXED_PATHS

    /* Get the environment variable */
    if (!tail)
        tail = getenv("ANGBAND_PATH");

#endif /* FIXED_PATHS */

    /* Use the angband_path, or a default */
    if (!tail)
        SDL_strlcpy(path, DEFAULT_PATH, sizeof(path));
    else if (tail != path)
        SDL_strlcpy(path, tail, sizeof(path));

    /* Make sure it's terminated */
    path[511] = '\0';

    /* Hack -- Add a path separator (only if needed) */
    if (!suffix(path, PATH_SEP))
        SDL_strlcat(path, PATH_SEP, sizeof(path));

    /* Initialize */
    init_file_paths(path);
}

/*
 * Handle a "-d<what>=<path>" option
 *
 * The "<what>" can be any string starting with the same letter as the
 * name of a subdirectory of the "lib" folder (i.e. "i" or "info").
 *
 * The "<path>" can be any legal path for the given system, and should
 * not end in any special path separator (i.e. "/tmp" or "~/.ang-info").
 */
static void change_path(cptr info)
{
    cptr s;

    /* Find equal sign */
    s = strchr(info, '=');

    /* Verify equal sign */
    if (!s)
        quit(format("Try '-d<what>=<path>' not '-d%s'", info));

    /* Analyze */
    switch (tolower((unsigned char)info[0]))
    {
#ifndef FIXED_PATHS
    case 'a':
    {
        str_free(ANGBAND_DIR_APEX);
        ANGBAND_DIR_APEX = str_dup(s + 1);
        break;
    }

    case 'f':
    {
        // str_free(ANGBAND_DIR_FILE);
        // ANGBAND_DIR_FILE = str_dup(s+1);
        break;
    }

    case 'h':
    {
        // str_free(ANGBAND_DIR_HELP);
        // ANGBAND_DIR_HELP = str_dup(s+1);
        break;
    }

    case 'i':
    {
        // str_free(ANGBAND_DIR_INFO);
        // ANGBAND_DIR_INFO = str_dup(s+1);
        break;
    }

    case 'x':
    {
        str_free(ANGBAND_DIR_XTRA);
        ANGBAND_DIR_XTRA = str_dup(s + 1);
        break;
    }

#ifdef VERIFY_SAVEFILE

    case 'b':
    case 'd':
    case 'e':
    case 's':
    {
        quit(format("Restricted option '-d%s'", info));
    }

#else /* VERIFY_SAVEFILE */

    case 'b':
    {
        // str_free(ANGBAND_DIR_BONE);
        // ANGBAND_DIR_BONE = str_dup(s+1);
        break;
    }

    case 'd':
    {
        str_free(ANGBAND_DIR_DATA);
        ANGBAND_DIR_DATA = str_dup(s + 1);
        break;
    }

    case 'e':
    {
        str_free(ANGBAND_DIR_EDIT);
        ANGBAND_DIR_EDIT = str_dup(s + 1);
        break;
    }

    case 's':
    {
        str_free(ANGBAND_DIR_SAVE);
        ANGBAND_DIR_SAVE = str_dup(s + 1);
        break;
    }

#endif /* VERIFY_SAVEFILE */

#endif /* FIXED_PATHS */

    case 'u':
    {
        str_free(ANGBAND_DIR_USER);
        ANGBAND_DIR_USER = str_dup(s + 1);
        break;
    }

    default:
    {
        quit(format("Bad semantics in '-d%s'", info));
    }
    }
}

/*
 * Simple "main" function for multiple platforms.
 *
 * Note the special "--" option which terminates the processing of
 * standard options. Any remaining options are passed directly to the
 * SDL runtime config parser.
 */
int main(int argc, char* argv[])
{
    int i;
    app_session* session = NULL;

    int show_score = 0;

    bool args = true;
    // Initialise logger in 'quiet' mode (don't write to stdout) on desktop.
    // On Android, keep stdout enabled so diagnostics are visible in logcat.
#if defined(__ANDROID__) || defined(SIL_IOS)
    init_logger(false, argv[0]);
#else
    init_logger(true, argv[0]);
#endif
    
    // Initialize dedicated generation log (generation.txt)
    gen_log_init(argv[0]);
    runtime_cli_reset();

    /* Initialize character_icky to ensure it starts at 0 */
    character_icky = 0;
    log_debug("main: character_icky initialized to %d", character_icky);

#ifdef SET_UID

    /* Default permissions on files */
    (void)umask(022);

#endif /* SET_UID */

    /* Get the file paths */
    init_stuff();


#ifdef SET_UID

    /* Get the user id (?) */
    player_uid = getuid();

#ifdef SAFE_SETUID

#if defined(HAVE_SETEGID) || defined(SAFE_SETUID_POSIX)

    /* Save some info for later */
    player_euid = geteuid();
    player_egid = getegid();

#endif /* defined(HAVE_SETEGID) || defined(SAFE_SETUID_POSIX) */

#endif /* SAFE_SETUID */

#endif /* SET_UID */

    /* Drop permissions */
    safe_setuid_drop();

#ifdef SET_UID

    /* Get the "user name" as a default player name */
    user_name(op_ptr->full_name, sizeof(op_ptr->full_name), player_uid);

#endif /* SET_UID */

    /* Process the command line arguments */
    for (i = 1; args && (i < argc); i++)
    {
        cptr arg = argv[i];

        /* Require proper options */
        if (*arg++ != '-')
            goto usage;

        /* Analyze option */
        switch (*arg++)
        {
        case 'N':
        case 'n':
        {
            // Sil-y:
            game_in_progress = true;
            break;
        }

        case 'F':
        case 'f':
        {
            runtime_cli_set_fiddle(true);
            break;
        }

        case 'W':
        case 'w':
        {
            runtime_cli_set_wizard(true);
            break;
        }

        case 'V':
        case 'v':
        {
            runtime_cli_set_sound(true);
            break;
        }

        case 'G':
        case 'g':
        {
            /* Default graphics tile */
            runtime_cli_set_graphics_mode(GRAPHICS_MICROCHASM);
            break;
        }

        case 'R':
        case 'r':
        {
            runtime_cli_set_force_roguelike(true);
            break;
        }

        case 'O':
        case 'o':
        {
            runtime_cli_set_force_original(true);
            break;
        }

        case 'S':
        case 's':
        {
            show_score = atoi(arg);
            if (show_score <= 0)
                show_score = 10;
            continue;
        }

        case 'u':
        case 'U':
        {
            if (!*arg)
                goto usage;

            /* Get the savefile name */
            SDL_strlcpy(op_ptr->full_name, arg, sizeof(op_ptr->full_name));

            // Sil-y:
            game_in_progress = true;
            continue;
        }

        case 'd':
        case 'D':
        {
            change_path(arg);
            continue;
        }

        case '-':
        {
            argv[i] = argv[0];
            argc = argc - i;
            argv = argv + i;
            args = false;
            break;
        }

        default:
        usage:
        {
            /* Dump usage information */
            runtime_cli_print_usage(argv[0]);

            /* Actually abort the process */
            quit(NULL);
        }
        }
        if (*arg)
            goto usage;
    }

    /* Hack -- Forget standard args */
    if (args)
    {
        argc = 1;
        argv[1] = NULL;
    }

    /* Note: process_player_name() is NOT called here anymore.
     * It will be called later when we actually know which character we're playing:
     * - By autoload_alive_from_scores() when loading from scorefile
     * - By character creation when creating a new character
     * - After load_player() succeeds
     */

    /* Install "quit" hook */
    log_register_quit_hook(quit_hook);

    if (0 != init_sdl(argc, argv))
        quit("Unable to initialize the SDL runtime.");
    ANGBAND_SYS = "sdl";

    /* Catch nasty signals */
    signals_init();

    {
        app_session_config session_config;

        memset(&session_config, 0, sizeof(session_config));
        session_config.api_version = APP_SESSION_API_VERSION;
        session_config.flags = APP_SESSION_FLAG_ALLOW_LEGACY_INPUT
            | APP_SESSION_FLAG_ALLOW_INTENT_INPUT
            | APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT;

        session = app_session_create(&session_config);
        if (!session)
            quit("Unable to create UI session.");

        app_session_make_current(session);
        app_session_set_state(session, APP_SESSION_STATE_IDLE);
    }

    /* Initialize */
    init_angband();

    /* Initialize sound system (requires ANGBAND_DIR_XTRA to be set) */
    platform_sound_init();

    /* Hack -- If requested, display scores and quit */
    if (show_score > 0)
        display_scores(0, show_score);

    /* Wait for response */
    // pause_line(platform_frame_main_view_rows() - 1);

    /* Play the game */
    // play_game(new_game);

    // Sil-y: There is now a text menu that can play repeated games
    while (1)
    {
        /* Let the player choose a savefile or start a new game */
        if (!game_in_progress) {
            bool      start_new = false;
            NavResult mn;

            /* loop until the player chooses a valid action */
            while (!game_in_progress) {
                mn = initial_menu(&start_new);
                if (mn == NAV_QUIT) quit(NULL);          /* immediate exit   */
                if (mn == NAV_OK) {                      /* play or load     */
                    game_in_progress = true;
                }
                /* NAV_BACK ⇒ redraw + loop again */
            }
        }

        /* Handle pending events (most notably update) and flush input */
        input_byte_queue_clear();
        platform_frame_flush_events();

        /* Play a game */
        PlayResult pr = play_game();   /* play and capture result */

        // rerun the first initialization routine
        init_stuff();

        // do some more between-games initialization
        re_init_some_things();

        // game no longer in progress
        game_in_progress = false;
        if (pr == PLAY_QUIT) quit(NULL);       /* honour in-game quit     */
    }

    /* Free resources */
    app_session_destroy(session);
    cleanup_angband();

    /* Quit */
    quit(NULL);

    /* Exit */
    return (0);
}

/*
 * Android entrypoint
 *
 * On Android, SDL's Java launcher calls SDL_main() in the native shared library.
 * On iOS, SDL_main.h handles the redirection automatically.
 */

#ifdef __ANDROID__
int SDL_main(int argc, char* argv[])
{
    return main(argc, argv);
}
#endif
