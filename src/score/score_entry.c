/* File: score/score_entry.c */
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

#include "score/score_entry.h"

#include "angband.h"
#include "blitz.h"
#include "fs/file.h"
#include "log/log.h"
#include "metarun.h"
#include "player/killer.h"
#include "score/score_io.h"

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define highscore_fd (score_file_active_ctx()->fd)
#define scores_file_entry_count (score_file_active_ctx()->entry_count)

static const int race_priority[] = {
    3,
    2,
    1,
    5,
    6,
    0
};

#define RACE_PRIORITIES (sizeof(race_priority) / sizeof(race_priority[0]))

static errr create_score_internal(high_score* the_score, time_t score_time,
    const char* death_text)
{
    if (!the_score)
        return 1;

    memset(the_score, 0, sizeof(high_score));
    strnfmt(the_score->what, sizeof(the_score->what), "%s", VERSION_STRING);

    int curse_total = 0;
    for (int id = 0; id < METAR_CURSE_SLOTS; ++id)
        curse_total += CURSE_GET(id);
    strnfmt(the_score->pts, sizeof(the_score->pts), "%4d", curse_total);

    strnfmt(the_score->turns, sizeof(the_score->turns), "%9lu",
        (long)playerturn);
    the_score->turns[9] = '\0';

    if (score_time == (time_t)0)
        score_time = time(NULL);
    strftime(the_score->day, sizeof(the_score->day), "@%Y%m%d",
        localtime(&score_time));

    const char* score_name = op_ptr->full_name;
    if (!score_name || !score_name[0]) {
        score_name = op_ptr->base_name[0] ? op_ptr->base_name : "nameless";
        log_warn("create_score: full_name empty, using fallback '%s'",
            score_name);
    }
    strnfmt(the_score->who, sizeof(the_score->who), "%-.15s", score_name);

    strnfmt(the_score->uid, sizeof(the_score->uid), "%7u", player_uid);
    strnfmt(the_score->p_r, sizeof(the_score->p_r), "%2d", p_ptr->prace);
    strnfmt(the_score->p_h, sizeof(the_score->p_h), "%2d", p_ptr->pcharacter);

    strnfmt(the_score->cur_dun, sizeof(the_score->cur_dun), "%3d",
        p_ptr->depth);
    the_score->cur_dun[3] = '\0';
    strnfmt(the_score->max_dun, sizeof(the_score->max_dun), "%3d",
        p_ptr->max_depth);
    the_score->max_dun[3] = '\0';

    int uniques_killed = unique_bane_type_killed();
    strnfmt(the_score->cur_lev, sizeof(the_score->cur_lev), "%3d",
        uniques_killed);
    the_score->cur_lev[3] = '\0';

    const char* how = death_text ? death_text : p_ptr->died_from;
    strnfmt(the_score->how, sizeof(the_score->how), "%-.49s", how);

    int recorded_silmarils = silmarils_possessed();
    if (p_ptr->morgoth_slain && recorded_silmarils < 3)
        recorded_silmarils = 3;
    strnfmt(the_score->silmarils, sizeof(the_score->silmarils), "%1d",
        recorded_silmarils);
    the_score->silmarils[1] = '\0';

    strnfmt(the_score->morgoth_slain, sizeof(the_score->morgoth_slain), "%s",
        p_ptr->morgoth_slain ? "t" : "f");
    strnfmt(the_score->escaped, sizeof(the_score->escaped), "%s",
        p_ptr->escaped ? "t" : "f");

    return 0;
}

bool highscore_is_empty(void)
{
    bool opened_here = false;

    if (!highscore_fd) {
        char buf[1024];
        build_current_score_path(buf, sizeof(buf));
        safe_setuid_grab();
        highscore_fd = score_file_open(buf, O_RDONLY);
        safe_setuid_drop();
        if (!highscore_fd) {
            log_debug("highscore_is_empty: cannot open scores file, treating as empty");
            return true;
        }
        opened_here = true;
    }

    bool is_empty = (scores_file_entry_count == 0);
    if (opened_here) {
        (void)ang_file_close_compat(highscore_fd);
        highscore_fd = NULL;
    }
    log_debug("highscore_is_empty: entry_count=%u, returning %s",
        scores_file_entry_count, is_empty ? "true" : "false");
    return is_empty;
}

errr create_score(high_score* the_score)
{
    return create_score_internal(the_score, time(NULL), p_ptr->died_from);
}

bool build_live_preview_score(high_score* out)
{
    if (!out || !character_generated)
        return false;

    return (create_score_internal(out, time(NULL), "(alive and well)") == 0);
}

bool score_entry_is_ranked_run(void)
{
#ifndef SCORE_CHEATERS
    int j;
#endif

#ifndef SCORE_WIZARDS
    if (p_ptr->noscore & 0x000F)
    {
        return false;
    }
#endif

    if (!p_ptr->escaped && streq(p_ptr->died_from, "Interrupting"))
    {
        return false;
    }

#ifndef SCORE_CHEATERS
    for (j = OPT_SCORE; j < OPT_MAX; ++j)
    {
        if (!op_ptr->opt[j])
            continue;

        return false;
    }

    if (p_ptr->noscore & 0x0001)
    {
        return false;
    }
#endif

    return true;
}

errr score_entry_submit(high_score* the_score)
{
    if (!highscore_fd)
    {
        log_warn("score_entry_submit: no high score file found");
        return 1;
    }

    if (!score_entry_is_ranked_run())
        return 0;

    safe_setuid_grab();
    safe_setuid_drop();

    int result = highscore_add(the_score);

    if (highscore_fd)
    {
        safe_setuid_grab();
        (void)ang_file_close_compat(highscore_fd);
        highscore_fd = NULL;
        safe_setuid_drop();
    }

    return (result < 0) ? 1 : 0;
}

static int race_has_character(uint16_t race, uint16_t character)
{
    if (character >= z_info->c_max)
        return 0;
    const uint16_t word = character / 32U;
    const uint16_t shift = character % 32U;
    return (p_info[race].choice[word] & (1U << shift)) != 0U;
}

static int parse_score_id(const char field[3])
{
    if (!field)
        return -1;
    if (!isdigit((unsigned char)field[0]) || !isdigit((unsigned char)field[1]))
        return -1;
    return (field[0] - '0') * 10 + (field[1] - '0');
}

static void build_dummy_entry(high_score* e, uint16_t race, uint16_t character)
{
    memset(e, 0, sizeof(*e));

    strnfmt(e->what, sizeof e->what, "%s", "Hero of the First Age");

    const char* hname = c_name + c_info[character].name;
    strnfmt(e->who, sizeof e->who, "%-.15s", hname);

    strnfmt(e->p_r, sizeof e->p_r, "%02u", race);
    strnfmt(e->p_h, sizeof e->p_h, "%02u", character);

    time_t now = time(NULL);
    strftime(e->day, sizeof(e->day), "@%Y%m%d", localtime(&now));

    strnfmt(e->how, sizeof e->how, "%s", op_ptr->base_name);
}

const char* kinslayer_try_kill(uint8_t n_sils, bool do_roll)
{
    log_info("Kinslayer attempt: n_sils=%u", n_sils);

    static const int pct_tab[4] = { 0, 20, 50, 95 };
    if (do_roll) {
        if (n_sils == 0)
            return NULL;
        if (n_sils > 3)
            n_sils = 3;
        int roll = rand_int(100);
        if (roll >= pct_tab[n_sils]) {
            log_debug("Kinslayer roll failed: %d >= %d (n_sils=%d)", roll,
                pct_tab[n_sils], n_sils);
            return NULL;
        }
    }

    char score_path[1024];
    build_current_score_path(score_path, sizeof(score_path));

    if (!highscore_fd) {
        log_trace("highscore_fd < 0, opening %s (version-aware)", score_path);
        safe_setuid_grab();
        highscore_fd = score_file_open(score_path, O_RDWR);
        safe_setuid_drop();
        if (!highscore_fd) {
            quit(format("Cannot open %s (%d)", score_path, errno));
            return NULL;
        }
        log_trace("opened highscore_fd (score file loaded)");
    }

    ang_file_seek_compat(highscore_fd, 0, ANG_FILE_SEEK_END);
    ang_file_off_t file_end = ang_file_tell_compat(highscore_fd);
    ang_file_off_t payload = file_end - (ang_file_off_t)sizeof(score_file_header);
    int n_recs = (int)(payload / (ang_file_off_t)sizeof(high_score));
    log_trace("hi-score file size=%lld, payload=%lld, records=%d",
        (long long)file_end, (long long)payload, n_recs);

    bool* hero_ineligible = calloc(z_info->c_max, sizeof(*hero_ineligible));
    if (!hero_ineligible) {
        safe_setuid_grab();
        if (!ang_file_close_compat(highscore_fd))
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        quit("Out of memory in kinslayer_try_kill()");
    }

    if (n_recs > 0 && highscore_seek(0) == 0) {
        high_score entry;
        for (int r = 0; r < n_recs; ++r) {
            if (highscore_read(&entry))
                break;
            int character = parse_score_id(entry.p_h);
            if (character < 0 || character >= (int)z_info->c_max)
                continue;
            bool escaped = (tolower((unsigned char)entry.escaped[0]) == 't');
            bool dead = (strcmp(entry.how, "(alive and well)") != 0);
            if (escaped || dead)
                hero_ineligible[character] = true;
        }
    }

    uint16_t eligible_races[RACE_PRIORITIES];
    size_t eligible_count = 0;

    for (size_t i = 0; i < RACE_PRIORITIES && eligible_count < RACE_PRIORITIES; ++i) {
        uint16_t race = race_priority[i];

        bool has_eligible = false;
        for (uint16_t h = 0; h < z_info->c_max; ++h) {
            if (!race_has_character(race, h))
                continue;
            if (hero_ineligible[h])
                continue;
            const char* hname = c_name + c_info[h].name;
            if (strcmp(hname, op_ptr->base_name) == 0)
                continue;
            has_eligible = true;
            break;
        }

        if (has_eligible) {
            eligible_races[eligible_count++] = race;
            log_trace("race priority[%zu]=%u added to eligible list (position %zu)",
                i, race, eligible_count - 1);
        } else {
            log_trace("race priority[%zu]=%u has no eligible characters, skipping",
                i, race);
        }
    }

    if (eligible_count == 0) {
        log_debug("No eligible races found - no kill performed");
        free(hero_ineligible);
        safe_setuid_grab();
        if (!ang_file_close_compat(highscore_fd))
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        return NULL;
    }

    static const int weights[3] = { 50, 30, 20 };
    int total_weight = 0;
    int applicable_races = (eligible_count < 3) ? (int)eligible_count : 3;

    for (int i = 0; i < applicable_races; ++i)
        total_weight += weights[i];

    int roll = rand_int(total_weight);
    int cumulative = 0;
    uint16_t selected_race = eligible_races[0];

    for (int i = 0; i < applicable_races; ++i) {
        cumulative += weights[i];
        if (roll < cumulative) {
            selected_race = eligible_races[i];
            log_info("Weighted race selection: chose race %u (position %d, weight %d%%)",
                selected_race, i, weights[i]);
            break;
        }
    }

    uint16_t race = selected_race;
    log_trace("Processing selected race=%u", race);

    uint16_t* pool = malloc(z_info->c_max * sizeof *pool);
    if (!pool) {
        free(hero_ineligible);
        (void)ang_file_close_compat(highscore_fd);
        quit("Out of memory in kinslayer_try_kill()");
    }

    size_t pool_n = 0;
    for (uint16_t h = 0; h < z_info->c_max; ++h) {
        if (!race_has_character(race, h))
            continue;
        if (hero_ineligible[h])
            continue;
        const char* hname = c_name + c_info[h].name;
        if (strcmp(hname, op_ptr->base_name) == 0)
            continue;
        pool[pool_n++] = h;
    }
    log_trace("race %u: %zu eligible characters", race, pool_n);
    if (pool_n == 0) {
        free(pool);
        free(hero_ineligible);
        safe_setuid_grab();
        if (!ang_file_close_compat(highscore_fd))
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        return NULL;
    }

    uint16_t character_sel = pool[rand_int((int)pool_n)];
    const char* hname = c_name + c_info[character_sel].name;
    free(pool);
    pool = NULL;
    free(hero_ineligible);
    hero_ineligible = NULL;
    log_info("Kinslayer selected character %u (%s) for elimination",
        character_sel, hname);

    int hit = -1;
    high_score entry;
    for (int r = 0; r < n_recs; ++r) {
        if (highscore_seek(r))
            break;
        if (highscore_read(&entry))
            break;
        if (entry.p_r[0] == '0' + (race / 10)
            && entry.p_r[1] == '0' + (race % 10)
            && entry.p_h[0] == '0' + (character_sel / 10)
            && entry.p_h[1] == '0' + (character_sel % 10)) {
            hit = r;
            break;
        }
    }
    log_trace("scan: entry_offset=%d", hit);

    if (hit >= 0) {
        if (highscore_dead(entry.who)) {
            log_debug("hero already dead - no kill performed");
            safe_setuid_grab();
            if (!ang_file_close_compat(highscore_fd))
                log_warn("fclose(highscore_fd) failed, errno=%d", errno);
            safe_setuid_drop();
            highscore_fd = NULL;
            return NULL;
        }

        if (entry.escaped[0] == 't') {
            log_debug("hero has escaped - no kill performed");
            safe_setuid_grab();
            if (!ang_file_close_compat(highscore_fd))
                log_warn("fclose(highscore_fd) failed, errno=%d", errno);
            safe_setuid_drop();
            highscore_fd = NULL;
            return NULL;
        }

        if (highscore_seek(hit) == 0 && highscore_read(&entry) == 0) {
            strnfmt(entry.how, sizeof entry.how, "%s", op_ptr->base_name);
            highscore_seek(hit);
            highscore_write(&entry);
            log_info("Kinslayer killed existing hero: \"%s\"", entry.who);
        } else {
            log_warn("Failed to re-read existing entry at slot %d", hit);
        }
    } else {
        high_score dummy;
        build_dummy_entry(&dummy, race, character_sel);
        log_trace("no existing record - inserting dummy \"%s\"", dummy.who);

        highscore_seek(0);
        int slot = highscore_add(&dummy);
        if (slot < 0)
            log_error("highscore_add() failed");
        else
            log_info("Kinslayer inserted dummy entry \"%s\" at slot %d",
                dummy.who, slot);
    }

    static char killed_character[32];
    SDL_strlcpy(killed_character, hname, sizeof killed_character);

    safe_setuid_grab();
    if (!ang_file_close_compat(highscore_fd))
        log_warn("fclose(highscore_fd) failed, errno=%d", errno);
    safe_setuid_drop();
    highscore_fd = NULL;
    return killed_character;
}
