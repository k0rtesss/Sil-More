#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-define.h"
#include "init.h"
#include "item_set.h"
#include "level-generation/level-generation-themes.h"
#include "log/log.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "score/score_paths.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include "init2-internal.h"
#include "init-lifecycle.h"
#include <SDL3/SDL_filesystem.h>
#include <stdio.h>
static void note(cptr str)
{
    cptr p;

    if (!str)
        str = "";

    for (p = str; *p; p++)
    {
        if (*p != ' ')
            break;
    }
    if (!*p)
        str = "";

    (void)sdl_welcome_screen_set_status(str);
    Term_fresh();
    (void)Term_xtra(TERM_XTRA_EVENT, 0);
}

void init_progress_error(const char* path, const char* reason)
{
    char message[2304];
    strnfmt(message, sizeof(message), "%s\n\n%s\n\n"
        "Check that your save location is available and that Sil-More can access it.",
        reason, path);
    log_error("%s", message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sil-More save data", message, NULL);
    quit(message);
}

bool init_profile_entry_is_placeholder(const char* name)
{
    return streq(name, ".gitignore") || streq(name, ".gitkeep")
        || streq(name, ".keepalive") || streq(name, ".DS_Store")
        || SDL_strcasecmp(name, "desktop.ini") == 0
        || SDL_strcasecmp(name, "Thumbs.db") == 0;
}

static bool no_saved_characters(void)
{
    int count = 0;
    char** saves = SDL_GlobDirectory(ANGBAND_DIR_SAVE, NULL, 0, &count);
    if (!saves)
        return false;
    bool empty = true;
    for (int i = 0; i < count && empty; ++i)
        empty = init_profile_entry_is_placeholder(saves[i]);
    SDL_free(saves);
    return empty;
}

static void init_scores(void)
{
    char path[1024];
    if (!score_build_meta_path(path, sizeof(path), "scores.raw")) {
        init_progress_error(ANGBAND_DIR_APEX, "Could not resolve the score file used to resume saved characters.");
        return;
    }

    SDL_IOStream* fd = sdl_fopen(path, "rb");
    if (fd)
    {
        Sint64 size = SDL_GetIOSize(fd);
        sdl_fclose(fd);
        if (size >= (Sint64)sizeof(score_file_header))
            return;
        if (size != 0) {
            init_progress_error(path, "Could not read the score file used to resume saved characters.");
            return;
        }

        /* Initialize an empty file in place without truncating it. */
        fd = sdl_fopen(path, "r+b");
        if (fd && SDL_GetIOSize(fd) != 0) {
            sdl_fclose(fd);
            init_progress_error(path, "The score file changed while Sil-More was starting.");
            return;
        }
    }
    else
    {
        /* A failed read does not prove there is no existing ledger. */
        FILE_TYPE(FILE_TYPE_DATA);
        safe_setuid_grab();
        fd = sdl_fopen(path, "wbx");
        safe_setuid_drop();
    }

    if (!fd) {
        init_progress_error(path, "Could not open or create the score file used to resume saved characters.");
        return;
    }

    const score_file_header header = {
        .version_major = SCORE_FILE_VERSION_MAJOR,
        .version_minor = SCORE_FILE_VERSION_MINOR,
        .version_patch = SCORE_FILE_VERSION_PATCH,
        .version_extra = SCORE_FILE_VERSION_EXTRA,
    };
    bool written = SDL_WriteIO(fd, &header, sizeof(header)) == sizeof(header);
    if (sdl_fclose(fd) || !written) {
        init_progress_error(path, "Could not write the initial score file used to resume saved characters.");
        return;
    }
    if (!no_saved_characters())
    {
        const char* warning = "Sil-More created a score file because the previous file was missing or empty. "
            "Existing character files are still in your save folder. To resume them automatically, "
            "close the game and restore your previous scores.raw from a backup.";
        log_warn("%s Path: %s", warning, path);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Sil-More save data", warning, NULL);
    }
}

extern void display_introduction(void)
{
    int intro_style;

    if (sdl_config_should_force_intro_flame())
        intro_style = INTRO_STYLE_FLAME;
    else if (op_ptr->intro_style == INTRO_STYLE_RANDOM)
        intro_style = (int)(SDL_GetTicks() % 7u);
    else
        intro_style = (int)op_ptr->intro_style;

    (void)sdl_welcome_screen_show_intro(intro_style, arg_wizard);
    Term_fresh();
}

/*
 * Hack -- main Sil initialization entry point
 *
 * Verify some files, create
 * the high score file, and initialize all internal arrays.
 *
 * Be very careful to keep track of the order in which things
 * are initialized, in particular, the only thing *known* to
 * be available when this function is called is the "z-term.c"
 * package, and that may not be fully initialized until the
 * end of this function, when "Term_xtra(TERM_XTRA_REACT,0)" is called.
 *
 * Note that this function attempts to verify the "news" file,
 * and the game aborts (cleanly) on failure, since without the
 * "news" file, it is likely that the "lib" folder has not been
 * correctly located.  Otherwise, the news file is displayed for
 * the user.
 *
 * Note that this function attempts to verify (or create) the
 * "high score" file, and the game aborts (cleanly) on failure,
 * since one of the most common "extraction" failures involves
 * failing to extract all sub-directories (even empty ones), such
 * as by failing to use the "-d" option of "pkunzip", or failing
 * to use the "save empty directories" option with "Compact Pro".
 * This error will often be caught by the "high score" creation
 * code below, since the "lib/apex" directory, being empty in the
 * standard distributions, is most likely to be "lost", making it
 * impossible to create the high score file.
 *
 * Note that various things are initialized by this function,
 * including everything that was once done by "init_some_arrays".
 *
 * This initialization involves the parsing of special files
 * in the "lib/data" and sometimes the "lib/edit" directories.
 *
 * Note that the "template" files are initialized first, since they
 * often contain errors.  This means that macros and message recall
 * and things like that are not available until after they are done.
 *
 * Display defaults come from edit templates and JSON configuration.
 */
void init_angband(void)
{
    int i;

    /* Load app-wide settings before the first intro render so the welcome
     * screen uses the configured style and first-launch state. */
    sdl_config_load_app_options(get_sdl_config_path());
    if (!lore_init())
        op_ptr->opt[OPT_generic] = false;
    run_mode_reset();

    /*** Display the introduction ***/

    screen_set_startup_touch_pane_hidden(true);
    sdl_story_font_enable();
    display_introduction();
    sdl_story_font_reset();

    init_scores();

    log_info("Loading metarun...");
    // Load metarun
    if (load_metaruns(1) != 0) {
        init_progress_error(ANGBAND_DIR_APEX, "Could not load or create the Tale data.");
    }


    /*** Initialize some arrays ***/

    /* Initialize size info */
    note("[Initializing array sizes...]");
    if (init_z_info())
        quit("Cannot initialize sizes");

    /* runtypes.raw ------------------------------------------------------ */
    note("[Initializing arrays. (runtypes)]");
    if (init_rt_info()) quit("Cannot initialise run types");


    /* Initialize feature info */
    note("[Initializing arrays... (features)]");
    if (init_f_info())
        quit("Cannot initialize terrain: game data is incomplete or incompatible.\n"
            "Update the game's lib/edit folder along with the executable.\n"
            "See log.txt for details.");

    /* Initialize object info */
    note("[Initializing arrays... (objects)]");
    if (init_k_info())
        quit("Cannot initialize objects");

    /* Initialize ability info */
    note("[Initializing arrays... (abilities)]");
    if (init_b_info())
        quit("Cannot initialize abilities");

    /* Initialize artefact info */
    note("[Initializing arrays... (artefacts)]");
    if (init_a_info())
        quit("Cannot initialize artefacts");
    ensure_artifact_guids();
    ensure_artifact_spawn_numbers();

    /* Load item sets (data-driven bonuses + paired weapons) */
    note("[Initializing arrays... (item sets)]");
    {
        SDL_IOStream* fp;
        char path[1024];
        char linebuf[1024];
        header set_head;
        errr err;

        init_header(&set_head, 1, 1);
        item_sets_reset();

        path_build(path, sizeof(path), ANGBAND_DIR_EDIT, format("%s.txt", "set"));
        fp = sdl_fopen(path, "r");
        if (!fp)
        {
            log_warn("init_angband: No set.txt found at '%s' (item sets disabled)", path);
        }
        else
        {
            err = init_info_txt(fp, linebuf, &set_head, parse_set_info);
            sdl_fclose(fp);

            if (err)
                display_parse_error("set", err, linebuf);

            err = item_sets_finalize();
            if (err)
                display_parse_error("set", err, "set validation");
        }
    }

    /* Initialize special item info */
    note("[Initializing arrays... (special items)]");
    if (init_e_info())
        quit("Cannot initialize special items");

    /* Initialize monster info */
    note("[Initializing arrays... (monsters)]");
    if (init_r_info())
        quit("Cannot initialize monsters");

    /* Initialize feature info */
    note("[Initializing arrays... (vaults)]");
    if (init_v_info())
        quit("Cannot initialize vaults");

    /* Initialize history info */
    note("[Initializing arrays... (histories)]");
    if (init_h_info())
        quit("Cannot initialize histories");

    /* Initialize story info */
    note("[Initializing arrays... (stories)]");
    if (init_st_info())
        quit("Cannot initialize stories");

    /* Initialize style info (visual styles) */
    note("[Initializing arrays... (styles)]");
    if (init_style_info())
        quit("Cannot initialize styles");
    style_info = (style_type*)style_head.info_ptr;
    style_name = style_head.name_ptr;
    if (init_partition_info())
        quit("Cannot initialize partition rules");
    (void)terrain_themes_load();

    /* Initialize curses info */
    note("[Initializing arrays... (curses)]");
    if (init_cu_info())
        quit("Cannot initialize curses");

    /* Initialize major blessing info */
    note("[Initializing arrays... (blessings)]");
    if (init_mb_info())
        quit("Cannot initialize major blessings");
    /* Apply major blessing effects now that blessing data is loaded */
    metarun_apply_runtime_effects();

    /* Initialize race info */
    note("[Initializing arrays... (races)]");
    if (init_p_info())
        quit("Cannot initialize races");

    /* Initialize character info */
    note("[Initializing arrays... (characters)]");
    if (init_c_info())
        quit("Cannot initialize characters");

    /* Initialize flavor info */
    note("[Initializing arrays... (flavors)]");
    if (init_flavor_info())
        quit("Cannot initialize flavors");

    /* Initialize special effect graphics */
    note("[Initializing arrays... (effects)]");
    if (init_effect_info())
        quit("Cannot initialize effects");

    /* Initialize skeleton note templates */
    note("[Initializing arrays... (skeleton notes)]");
    if (init_skeleton_note_info())
        quit("Cannot initialize skeleton notes");

    /* Initialize quest info */
    note("[Initializing arrays... (quests)]");
    if (init_quest_info())
        quit("Cannot initialize quests");

    /* Initialize oath info */
    note("[Initializing arrays... (oaths)]");
    if (init_oath_info())
        quit("Cannot initialize oaths");

    /* Initialize some other arrays */
    note("[Initializing arrays... (other)]");
    if (init_other())
        quit("Cannot initialize other stuff");
    sdl_config_apply_keyboard_keymaps(&config);

    /* Initialize some other arrays */
    note("[Initializing arrays... (alloc)]");
    if (init_alloc())
        quit("Cannot initialize alloc stuff");

    note("[Initializing display defaults...]");

    /* Initialize feature info */
    note("[Initializing Random Artefact Tables...]");

    /* Initialize the random name table */
    if (init_n_info())
        quit("Cannot initialize random name generator stuff");

    /*Build the randart probability tables based on the standard Artefact Set*/
    build_randart_tables();

    /* Snapshot the fully initialized monster templates. This baseline must be
     * taken after prefs and mon_power setup so load can restore clean runtime
     * race data without replaying stale pre-init values. */
    if (!r_base)
    {
        r_base = mem_alloc_array(z_info->r_max, monster_race);
    }
    for (i = 0; i < z_info->r_max; i++)
    {
        r_base[i] = r_info[i];
    }

    /* Clean up old files if this is a fresh start (no existing metarun) */
    if (metarun_created) {
        cleanup_old_game_files();
    }

    /* Done */
    note("                                              ");
}


/* --- UPDATED MAIN-MENU HANDLER ---------------------------------------- */
extern NavResult initial_menu(bool *start_new)
{
    int ch;
    NavResult result = NAV_BACK;
    bool intro_story_font = false;

    log_info("initial_menu: ENTERED - showing main menu");

    /* A Blitz launch armed from the in-game menu skips the title screen and
     * routes the next play_game() straight into Blitz mode.  Still run the
     * shared menu cleanup: re_init_some_things() has just displayed the intro,
     * and leaving that welcome screen active hides the Blitz setup on mobile. */
    if (blitz_launch_requested())
    {
        log_info("initial_menu: blitz launch requested - entering Blitz mode");
        run_mode_set_pending(RUN_MODE_BLITZ);
        *start_new = true;
        result = NAV_OK;
        goto menu_done;
    }

    if (sdl_music_consume_welcome_main_once()
        || score_count_alive_entries() > 0)
        sdl_music_play_main();
    else
        sdl_music_play_main_full();

    intro_story_font = true;
    screen_set_startup_touch_pane_hidden(true);
    sdl_story_font_enable();

    bool show_wizard_line = arg_wizard;

    (void)sdl_welcome_screen_show_menu(show_wizard_line, metarun_created);

    while (true)
    {
        Term_fresh();

        /* Prevent inkey() from showing the cursor while waiting on the menu */
        bool _saved_hide_cursor = hide_cursor;
        hide_cursor = true;
        ch = inkey();
        hide_cursor = _saved_hide_cursor;

        /* q/Esc : EXIT */
        if (ch == 'q' || ch == 'Q' || ch == ESCAPE)
        {
            result = NAV_QUIT;                /* handled as quit in main-win.c */
            goto menu_done;
        }

        /* Any other key : CONTINUE */
        log_info("initial_menu: User pressed continue key - starting game");
        run_mode_set_pending(RUN_MODE_STORY);
        *start_new = true;
        result = NAV_OK;   /* start new game */
        goto menu_done;
    }

menu_done:
    ui_menu_click_clear();
    sdl_welcome_screen_hide();
    screen_set_startup_touch_pane_hidden(false);
    log_info("initial_menu: EXITING with result=%d", result);
    if (sdl_config_should_force_intro_flame()) {
        sdl_config_mark_intro_seen();
        save_pane_config_to_json();
    }
    if (intro_story_font)
        sdl_story_font_reset();
    return result;
}
/* ---------------------------------------------------------------------- */
