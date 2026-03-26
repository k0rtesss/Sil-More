/* File: runtime/runtime-game.c */

#include "runtime/runtime-game.h"

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "fs/savefile-name.h"
#include "log/log.h"
#include "metarun.h"
#include "player/killer.h"
#include "reliability-checks.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "score/score_runs.h"
#include "score/score_ui.h"
#include "ui/ui-death.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WINDOWS
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif

#define highscore_fd (score_file_active_ctx()->fd)
#define scores_file_entry_count (score_file_active_ctx()->entry_count)
#define scores_file_version_major (score_file_active_ctx()->version_major)
#define scores_file_version_minor (score_file_active_ctx()->version_minor)
#define scores_file_version_patch (score_file_active_ctx()->version_patch)
#define scores_file_version_extra (score_file_active_ctx()->version_extra)

bool save_game_quietly = false;
static bool death_processing = false;

bool death_processing_in_progress(void)
{
    return death_processing;
}

void do_cmd_save_game(void)
{
    disturb(1, 0);

    if (DEPLOYMENT && p_ptr->game_type != 0)
    {
        if (!save_game_quietly)
            msg_print("You cannot save games during the tutorial.");
        return;
    }

    message_flush();
    handle_stuff();

    if (!save_game_quietly)
        prt("Saving game...", 0, 0);

    Term_fresh();
    SDL_strlcpy(p_ptr->died_from, "(saved)", sizeof(p_ptr->died_from));

    signals_ignore_tstp();

    log_info("Saving game and updating metarun data");
    if (!run_mode_is_blitz())
        metarun_update_on_exit(false, false, 0, 0);

    if (save_player())
    {
        log_debug("Game saved successfully");
        if (!save_game_quietly)
            prt("Saving game... done.", 0, 0);

        time_t now = time(NULL);
        if (now == (time_t)-1)
            now = 0;
        if (!score_refresh_live_snapshot(now, "save_game")) {
            log_warn("Failed to persist live run snapshot for '%s'",
                op_ptr->full_name);
        }
    }
    else
    {
        log_error("Game save failed");
        prt("Saving game... failed!", 0, 0);
    }

    signals_handle_tstp();
    Term_fresh();

    SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));
    save_game_quietly = false;
}

static void close_game_aux(void)
{
    bool wants_to_quit = false;
    high_score the_score;
    int choice = 0, highlight = 1;

    if (death_processing)
    {
        log_debug("Death processing already in progress - skipping duplicate call");
        return;
    }
    death_processing = true;

    log_debug("Processing character death for '%s' (wizard=%d, noscore=0x%04X, savefile='%s')",
             op_ptr->full_name, p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore, savefile);

    log_info("saving dead player (noscore=0x%04X) -> '%s'",
        (unsigned)p_ptr->noscore, savefile);
    if (!save_player())
    {
        log_error("Death save failed - player data may be lost");
        msg_print("death save failed!");
        message_flush();
    }

    time_t death_time = time(NULL);

    Term_clear();

    log_info("entering score");
    create_score(&the_score);
    score_record_status final_status = p_ptr->escaped ? SCORE_RECORD_ESCAPED : SCORE_RECORD_DEAD;

    bool ranked_run = score_entry_is_ranked_run();
    bool legacy_score_written = !ranked_run;
    if (ranked_run) {
        char score_path[1024];
        build_current_score_path(score_path, sizeof(score_path));
        safe_setuid_grab();
        SDL_IOStream* score_fd = score_file_open(score_path, O_RDWR | O_CREAT);
        safe_setuid_drop();
        if (score_fd) {
            SDL_IOStream* previous_fd = score_file_active_ctx()->fd;
            score_file_active_ctx()->fd = score_fd;
            legacy_score_written = (score_entry_submit(&the_score) == 0);
            score_file_active_ctx()->fd = previous_fd;
        } else {
            log_warn("Unable to open score file for final score submission");
        }
    }

    if (reliability_should_update_runs_db(ranked_run, legacy_score_written)) {
        if (!score_runs_record_current_run(&the_score, death_time, final_status)) {
            log_warn("Failed to persist run statistics for '%s'", op_ptr->full_name);
        }
    } else {
        log_warn("Skipping runs.db update because ranked score submission failed for '%s'",
            op_ptr->full_name);
    }

    p_ptr->rage = 0;
    p_ptr->image = 0;

    char curr_time[30], sheet[90];
    time_t ct = time((time_t*)0);
    (void)strftime(curr_time, 30, "%Y%m%d-%H%M%S.txt", localtime(&ct));
    sprintf(sheet, "%s-%s", op_ptr->full_name, curr_time);
    errr err;
    screen_save();
    err = file_character(sheet, false);
    screen_load();
    if (err)
    {
        Term_clear();
        msg_print("Automatic character dump failed!");
        message_flush();
    }

    int final_score = score_points(&the_score);
    if (!run_mode_is_blitz())
    {
        if (p_ptr->morgoth_slain && !p_ptr->escaped)
        {
            log_info("Player achieved Morgoth victory - updating metarun data");
            metarun_update_on_exit(false, false, 3, final_score);
        }
        else
        {
            log_info("Player died - updating metarun data");
            if (!p_ptr->escaped)
                metarun_update_on_exit(true, false, 0, final_score);
        }
    }
    else
    {
        int blitz_silmarils = silmarils_possessed();

        if (blitz_silmarils < 0)
            blitz_silmarils = 0;
        if (p_ptr->morgoth_slain && blitz_silmarils < 3)
            blitz_silmarils = 3;

        blitz_show_end_summary((byte)blitz_silmarils);
    }

    death_spectator_view();
    Term_clear();

    ui_death_print_tomb(&the_score);

    flush();
    message_flush();

    while (!wants_to_quit)
    {
        choice = ui_death_final_menu(&highlight);

        switch (choice)
        {
        case 1:
            show_scores_interactive_highlight(true, &the_score);
            break;

        case 2:
            screen_save();
            death_spectator_view();
            screen_load();
            break;

        case 3:
            screen_save();
            do_cmd_messages();
            screen_load();
            break;

        case 4:
            screen_save();
            ui_death_show_character_info();
            screen_load();
            break;

        case 5:
            do_cmd_note("", p_ptr->depth);
            break;

        case 6:
        {
            char ftmp[80];

            strnfmt(ftmp, sizeof(ftmp), "%s.txt", op_ptr->base_name);

            if (term_get_string("File name: ", ftmp, sizeof(ftmp)))
            {
                if (ftmp[0] && (ftmp[0] != ' '))
                {
                    errr dump_err;
                    screen_save();
                    dump_err = file_character(ftmp, false);
                    screen_load();

                    if (dump_err)
                        msg_print("Character dump failed!");
                    else
                        msg_print("Character dump successful.");

                    message_flush();
                }
            }
            break;
        }

        case 7:
            wants_to_quit = true;
            break;
        }
    }

    death_processing = false;
}

void close_game(void)
{
    log_info("Starting game close sequence for player '%s'", op_ptr->full_name);

    handle_stuff();
    message_flush();
    flush();
    signals_ignore_tstp();

    character_icky++;
    log_debug("runtime-game: character_icky incremented to %d", character_icky);

    if (p_ptr->is_dead)
    {
        if (p_ptr->game_type == 0)
        {
            log_info("Player %s died at depth %d in %s.",
                op_ptr->full_name, p_ptr->depth, p_ptr->died_from);
            close_game_aux();
        }
        else if (p_ptr->game_type == -1)
        {
            monster_lore* l_ptr = &l_list[R_IDX_ORC_ARCHER];

            if (p_ptr->chp <= 0)
            {
                if (l_ptr->psights == 0)
                    pause_with_text(tutorial_early_death_text, 5, 10, NULL, 0);
                else
                    pause_with_text(tutorial_late_death_text, 5, 10, NULL, 0);
            }
        }

        wipe_o_list();
        wipe_mon_list();
        cave_m_idx[p_ptr->py][p_ptr->px] = 0;
    }
    else
    {
        do_cmd_save_game();

        Term_putstr(6, 0, -1, TERM_L_BLUE, "View high scores? (ESC to skip)");
        char prompt_key = inkey();
        Term_erase(0, 0, 255);
        if (prompt_key != ESCAPE)
        {
            high_score preview;
            if (build_live_preview_score(&preview))
                show_scores_interactive_highlight(true, &preview);
            else
                show_scores_interactive(true);
        }

        wipe_o_list();
        wipe_mon_list();
    }

    log_info("Game close sequence completed");
    character_icky--;
    log_debug("runtime-game: character_icky decremented to %d", character_icky);
    signals_handle_tstp();
}

void exit_game_panic(void)
{
    if (!character_generated || character_saved)
        quit("panic");

    msg_flag = false;
    prt("", 0, 0);
    disturb(1, 0);

    if (p_ptr->chp <= 0)
        p_ptr->is_dead = false;

    p_ptr->panic_save = 1;
    signals_ignore_tstp();
    SDL_strlcpy(p_ptr->died_from, "(panic save)", sizeof(p_ptr->died_from));

    if (!save_player())
        quit("panic save failed!");

    quit("panic save succeeded!");
}

bool autoload_alive_from_scores(void)
{
    log_info("===== autoload_alive_from_scores: FUNCTION CALLED =====");
    char score_path[1024];
    build_current_score_path(score_path, sizeof(score_path));

    score_file_ctx local_ctx;
    score_file_reset_ctx(&local_ctx);
    score_file_ctx* previous_ctx = score_file_set_active_ctx(&local_ctx);

    highscore_fd = score_file_open(score_path, O_RDWR | O_CREAT);
    if (!highscore_fd) {
        log_warn("autoload: could not open scorefile: %s", score_path);
        score_file_set_active_ctx(previous_ctx);
        return false;
    }

    int n_recs = (int)scores_file_entry_count;
    log_trace("autoload: scorefile n_recs=%d header_count=%u", n_recs,
        scores_file_entry_count);

    if (n_recs <= 0) {
        SDL_CloseIO(highscore_fd);
        highscore_fd = NULL;
        score_file_set_active_ctx(previous_ctx);
        return false;
    }

    for (int i = 0; i < n_recs; ++i) {
        if (highscore_seek(i))
            break;
        high_score entry;
        if (highscore_read(&entry))
            break;
        if (strcmp(entry.how, "(alive and well)") != 0)
            continue;

        char who_buf[sizeof entry.who + 1];
        memset(who_buf, 0, sizeof who_buf);
        SDL_strlcpy(who_buf, entry.who, sizeof(who_buf));
        for (int t = (int)strlen(who_buf) - 1; t >= 0; --t) {
            if (who_buf[t] == ' ' || who_buf[t] == '\t')
                who_buf[t] = '\0';
            else
                break;
        }
        if (!who_buf[0]) {
            log_warn("autoload: alive entry at index %d has empty name, skipping", i);
            continue;
        }
        log_info("autoload: found alive entry '%s' (index %d) - attempting load",
            who_buf, i);

        SDL_strlcpy(op_ptr->full_name, who_buf, sizeof(op_ptr->full_name));
        process_player_name(true);

        log_info("autoload: savefile path generated: '%s'", savefile);
        if (load_player()) {
            log_info("autoload: successfully loaded '%s' (normalized)", who_buf);
            SDL_CloseIO(highscore_fd);
            highscore_fd = NULL;
            score_file_set_active_ctx(previous_ctx);
            return true;
        }

        char savefile_backup[1024];
        char alt_temp[128];
        char alt_path[1024];
        SDL_strlcpy(savefile_backup, savefile, sizeof(savefile_backup));
        build_active_savefile_stem(who_buf, alt_temp, sizeof(alt_temp));
        path_build(alt_path, sizeof(alt_path), ANGBAND_DIR_SAVE, alt_temp);
        SDL_strlcpy(savefile, alt_path, sizeof(savefile));
        log_info("autoload: retrying with legacy spaced filename '%s'", savefile);
        if (load_player()) {
            log_info("autoload: successfully loaded '%s' (legacy spaced)", who_buf);
            SDL_strlcpy(op_ptr->full_name, who_buf, sizeof(op_ptr->full_name));
            process_player_name(true);
            SDL_CloseIO(highscore_fd);
            highscore_fd = NULL;
            score_file_set_active_ctx(previous_ctx);
            SDL_strlcpy(savefile, savefile_backup, sizeof(savefile));
            return true;
        }
        SDL_strlcpy(savefile, savefile_backup, sizeof(savefile));

#if ANTICHEAT
        log_warn("autoload: savefile missing/corrupt for '%s' - marking dead",
            who_buf);
        strnfmt(entry.how, sizeof entry.how, "%-.49s", "their own hand");
        if (highscore_seek(i) == 0)
            highscore_write(&entry);
        if (!run_mode_is_blitz()) {
            metarun_increment_deaths();
            (void)save_metaruns();
        }
        msg_format("Warning: Alive entry '%s' had no valid savefile. Marked as dead.", who_buf);
        msg_print("Please do not tamper with savefiles.");
        message_flush();
#else
        log_warn("autoload: savefile missing/corrupt for '%s' - skipping (ANTICHEAT disabled)", who_buf);
#endif
    }

    SDL_CloseIO(highscore_fd);
    highscore_fd = NULL;
    score_file_set_active_ctx(previous_ctx);
    return false;
}

void metarun_finalize_scores_and_saves(void)
{
    log_info("finalize: entry (wizard=%d, noscore=0x%04X, savefile='%s')",
             p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
             p_ptr ? (unsigned)p_ptr->noscore : 0,
             savefile);

    char score_path[1024];
    build_current_score_path(score_path, sizeof(score_path));

    score_file_ctx local_ctx;
    score_file_reset_ctx(&local_ctx);
    score_file_ctx* previous_ctx = score_file_set_active_ctx(&local_ctx);

    safe_setuid_grab();
    highscore_fd = score_file_open(score_path, O_RDWR | O_CREAT);
    safe_setuid_drop();
    if (!highscore_fd) {
        log_warn("finalize: could not open scorefile: %s", score_path);
        score_file_set_active_ctx(previous_ctx);
        return;
    }

    int patched = 0;
    int n_recs = (int)scores_file_entry_count;
    for (int i = 0; i < n_recs; i++) {
        high_score entry;
        if (highscore_seek(i))
            break;
        if (highscore_read(&entry))
            break;

        if (strcmp(entry.how, "(alive and well)") != 0)
            continue;

        strnfmt(entry.how, sizeof entry.how, "%-.49s", "their own hand");
        if (highscore_seek(i) == 0)
            (void)highscore_write(&entry);
        patched++;
    }

    safe_setuid_grab();
    SDL_CloseIO(highscore_fd);
    safe_setuid_drop();
    highscore_fd = NULL;
    score_file_set_active_ctx(previous_ctx);

    log_info("finalize: patched %d alive entries to 'their own hand'", patched);

    if (p_ptr && (p_ptr->wizard || (p_ptr->noscore & 0x0008))
        && (p_ptr->noscore & 0x000F)) {
        if (savefile[0]) {
            int rc;
            safe_setuid_grab();
            rc = fd_kill(savefile);
            safe_setuid_drop();
            if (rc == 0)
                log_info("finalize: deleted noscore wizard/debug savefile '%s'", savefile);
            else
                log_warn("finalize: failed to delete noscore wizard/debug savefile '%s'", savefile);
        }
    } else {
        log_info("finalize: no direct purge in finalize (wizard=%d, noscore=0x%04X)",
                 p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
                 p_ptr ? (unsigned)p_ptr->noscore : 0);
    }
}

void backup_and_clear_saves(void)
{
    char save_dir[1024];

    strnfmt(save_dir, sizeof(save_dir), "%s", ANGBAND_DIR_SAVE);
    log_info("Checking for save files to backup in: %s", save_dir);

    bool has_files = false;
    char test_patterns[][32] = {"*.sav", "*.dat", "*.txt", "character.sav",
        "save.dat", "Feanor", "player"};

    for (int i = 0; i < 7 && !has_files; i++) {
        char test_path[1024];
        path_build(test_path, sizeof(test_path), save_dir, test_patterns[i]);
        log_trace("Checking for save file pattern: %s", test_path);

        SDL_IOStream* test_fd = sdl_fopen(test_path, "rb");
        if (test_fd) {
            sdl_fclose(test_fd);
            has_files = true;
            log_trace("Found save file: %s", test_path);
            break;
        } else {
            log_trace("File not found: %s", test_path);
        }
    }

    if (!has_files) {
        log_trace("No specific patterns found, checking directory contents...");

        char common_patterns[][32] = {"save", "char", "game", "*"};

        for (int i = 0; i < 4 && !has_files; i++) {
            char test_path[1024];
            path_build(test_path, sizeof(test_path), save_dir,
                common_patterns[i]);

            log_trace("Checking directory pattern: %s", test_path);

            SDL_IOStream* test_fd = sdl_fopen(test_path, "rb");
            if (test_fd) {
                sdl_fclose(test_fd);
                has_files = true;
                log_trace("Found file with pattern: %s", test_path);
                break;
            }
        }
    }

    if (!has_files) {
        log_info("No save files found - skipping backup/clear process");
        log_trace("Backup skipped because no save files were detected");
        return;
    }

    char backup_folder[1024];
    char timestamp[64];
    time_t now;
    struct tm* timeinfo;

    prt("[Creating save file backup folder...]", 0, 0);
    Term_fresh();

    log_info("Found save files to backup and clear");
    log_trace("Starting folder-based backup process for save files");

    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", timeinfo);

    path_build(backup_folder, sizeof(backup_folder), save_dir,
        format("saves_metarun_%s", timestamp));

    log_info("Creating backup folder: %s", backup_folder);
    log_trace("Full backup folder path: %s", backup_folder);

#ifdef WINDOWS
    if (_mkdir(backup_folder) != 0) {
        log_warn("Failed to create backup folder: %s", backup_folder);
        return;
    }
#else
    if (mkdir(backup_folder, 0755) != 0) {
        log_warn("Failed to create backup folder: %s", backup_folder);
        return;
    }
#endif

    int files_moved = 0;

#ifdef WINDOWS
    WIN32_FIND_DATA findData;
    char search_path[1024];
    path_build(search_path, sizeof(search_path), save_dir, "*");

    HANDLE hFind = FindFirstFile(search_path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            char* filename = findData.cFileName;

            if (strcmp(filename, ".gitignore") == 0)
                continue;
            if (strstr(filename, "saves_metarun_"))
                continue;

            char old_path[1024], new_path[1024];
            path_build(old_path, sizeof(old_path), save_dir, filename);
            path_build(new_path, sizeof(new_path), backup_folder, filename);

            log_trace("Moving save file: %s -> %s", old_path, new_path);

            if (MoveFile(old_path, new_path)) {
                files_moved++;
                log_info("Moved save file to backup: %s", filename);
            } else {
                log_warn("Failed to move save file: %s", filename);
            }
        } while (FindNextFile(hFind, &findData));

        FindClose(hFind);
    }
#else
    DIR* dir = opendir(save_dir);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            char* filename = entry->d_name;

            if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
                continue;
            if (strcmp(filename, ".gitignore") == 0)
                continue;
            if (strstr(filename, "saves_metarun_"))
                continue;

            char old_path[1024], new_path[1024];
            path_build(old_path, sizeof(old_path), save_dir, filename);
            path_build(new_path, sizeof(new_path), backup_folder, filename);

            struct stat st;
            if (stat(old_path, &st) != 0 || !S_ISREG(st.st_mode))
                continue;

            log_trace("Moving save file: %s -> %s", old_path, new_path);

            if (rename(old_path, new_path) == 0) {
                files_moved++;
                log_info("Moved save file to backup: %s", filename);
            } else {
                log_warn("Failed to move save file: %s", filename);
            }
        }
        closedir(dir);
    }
#endif

    Term_erase(0, 0, 255);
    if (files_moved > 0) {
        msg_format("Backed up %d save file%s to %s",
            files_moved, (files_moved == 1) ? "" : "s", backup_folder);
        log_info("Successfully backed up %d save files to %s", files_moved,
            backup_folder);
    } else {
        msg_print("No save files were moved to backup.");
        log_info("No save files were moved to backup folder");
    }
    message_flush();
}
