#include "metarun-internal.h"

/* Generate curse description for a runtype */
static void get_curse_description(int runtype_id, char *buf, size_t buf_size)
{
    if (!runtype_info || runtype_id >= z_info->rt_max || buf_size < 64)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }

    runtype_type *rt = &runtype_info[runtype_id];

    if (!rt->start_curses)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }

    /* Count curses and determine stack ranges */
    int curse_count = 0;
    int min_stacks = 255, max_stacks = 0;

    int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
    for (int curse_id = 0; curse_id < limit; curse_id++)
    {
        if (rt->start_curses & (1ULL << curse_id))
        {
            curse_count++;
            int stacks = rt->curse_stacks[curse_id];
            if (stacks < min_stacks) min_stacks = stacks;
            if (stacks > max_stacks) max_stacks = stacks;
        }
    }

    if (curse_count == 0)
    {
        strncpy(buf, "No curses", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }

    /* Format the description */
    if (min_stacks == max_stacks)
    {
        if (min_stacks == 1)
            snprintf(buf, buf_size, "Curses: %d x %d stack", curse_count, min_stacks);
        else
            snprintf(buf, buf_size, "Curses: %d x %d stacks", curse_count, min_stacks);
    }
    else
    {
        snprintf(buf, buf_size, "Curses: %d (%d-%d stacks)", curse_count, min_stacks, max_stacks);
    }
}

/* Difficulty selection menu */
static void choose_difficulty_menu(bool reopen_stats_on_exit)
{
    int choice = metar.type;
    int max_difficulty = (runtype_info && z_info->rt_max > 0)
        ? z_info->rt_max - 1 : 0;
    ui_information_scene_scope info_scope;
    bool use_information_scene = ui_information_scene_enter(&info_scope);
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    char back_label[16] = "";
    char status_msg[APP_UI_TEXT_MAX] = "";
    byte status_attr = TERM_WHITE;
    bool first_present = true;

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
    }
    if (!use_information_scene) {
        log_error("choose_difficulty_menu: semantic scene unavailable");
        if (reopen_stats_on_exit)
            print_metarun_stats();
        return;
    }

    while (true)
    {
        app_ui_scene scene;
        app_ui_panel *panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        strnfmt(subtitle, sizeof(subtitle), "Current difficulty: %s",
            (runtype_info && metar.type < z_info->rt_max
                && runtype_info[metar.type].name[0])
                ? runtype_info[metar.type].name : "Unknown");
        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "Select Difficulty Level", TERM_SLATE, subtitle);
        if (!panel) {
            log_error("choose_difficulty_menu: failed to build semantic scene");
            break;
        }

        for (int i = 0; i <= max_difficulty; i++)
        {
            bool is_locked = (i < metar.max_difficulty_reached);
            byte attr = TERM_WHITE;
            char key_buf[APP_UI_KEY_MAX];
            char meta[APP_UI_META_MAX];
            int win_goal = WINCON_SILMARILS;
            u32b blessing_thresh = runtype_threshold_for_mode(i,
                METARUN_BLESSING_THRESHOLD_NORMAL);
            char curse_buf[64];
            const char *rt_name = "Unknown";

            if (runtype_info && i < z_info->rt_max && runtype_info[i].name[0]) {
                rt_name = runtype_info[i].name;
                win_goal = runtype_info[i].win_con
                    ? runtype_info[i].win_con : WINCON_SILMARILS;
                attr = runtype_info[i].colour;
            }
            if (is_locked)
                attr = TERM_L_DARK;

            get_curse_description(i, curse_buf, sizeof(curse_buf));
            strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
            strnfmt(meta, sizeof(meta), "Win %d  Threshold %lu  %s",
                win_goal, (unsigned long)blessing_thresh, curse_buf);
            if (!app_ui_panel_add_row(panel, i, attr, !is_locked,
                    i == choice, key_buf, rt_name, meta))
            {
                log_error("choose_difficulty_menu: failed to append row");
                ui_information_scene_leave(&info_scope);
                if (reopen_stats_on_exit)
                    print_metarun_stats();
                return;
            }
        }

        {
            bool is_locked = (choice < metar.max_difficulty_reached);
            int win_goal = WINCON_SILMARILS;
            u32b blessing_thresh = runtype_threshold_for_mode(choice,
                METARUN_BLESSING_THRESHOLD_NORMAL);
            char curse_buf[64];
            char line[APP_UI_TEXT_MAX];
            const char *rt_name = "Unknown";

            if (runtype_info && choice < z_info->rt_max
                && runtype_info[choice].name[0])
            {
                rt_name = runtype_info[choice].name;
                win_goal = runtype_info[choice].win_con
                    ? runtype_info[choice].win_con : WINCON_SILMARILS;
            }

            get_curse_description(choice, curse_buf, sizeof(curse_buf));
            app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                "Selected Difficulty");
            (void)app_ui_panel_add_detail_line(panel,
                is_locked ? TERM_L_DARK : TERM_WHITE, rt_name);
            strnfmt(line, sizeof(line), "Win condition: %d Silmarils", win_goal);
            (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, line);
            strnfmt(line, sizeof(line), "Threshold: %lu points",
                (unsigned long)blessing_thresh);
            (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, line);
            (void)app_ui_panel_add_detail_line(panel,
                is_locked ? TERM_L_DARK : TERM_SLATE, curse_buf);
            if (status_msg[0] != '\0')
                (void)app_ui_panel_add_detail_line(panel, status_attr,
                    status_msg);
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            steamdeck ? accept_label : "Enter", "Accept");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            steamdeck ? back_label : "Esc", "Cancel");

        if (!metarun_ui_present_scene(&scene, first_present)) {
            log_error("choose_difficulty_menu: failed to publish semantic scene");
            break;
        }
        first_present = false;

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key())
            || (!steamdeck && (key == 'h' || key == 'H')))
        {
            ui_information_scene_leave(&info_scope);
            if (reopen_stats_on_exit)
                print_metarun_stats();
            return;
        }
        if (key == '\r' || key == '\n'
            || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
        {
            if (choice < metar.max_difficulty_reached) {
                SDL_strlcpy(status_msg,
                    "Cannot select easier difficulty - locked for this story run!",
                    sizeof(status_msg));
                status_attr = TERM_RED;
                continue;
            }
            break;
        }
        if (key == '8' || key == 'k' || key == '-') {
            int new_choice = choice - 1;
            while (new_choice >= 0 && new_choice < metar.max_difficulty_reached)
                new_choice--;
            if (new_choice >= 0)
                choice = new_choice;
            status_msg[0] = '\0';
            continue;
        }
        if (key == '2' || key == 'j' || key == '+') {
            if (choice < max_difficulty)
                choice++;
            status_msg[0] = '\0';
            continue;
        }
        if (key >= 'a' && key <= 'z') {
            int new_choice = key - 'a';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    SDL_strlcpy(status_msg,
                        "Cannot select easier difficulty - locked for this story run!",
                        sizeof(status_msg));
                    status_attr = TERM_RED;
                } else {
                    choice = new_choice;
                    status_msg[0] = '\0';
                }
            }
            continue;
        }
        if (key >= 'A' && key <= 'Z') {
            int new_choice = key - 'A';
            if (new_choice <= max_difficulty) {
                if (new_choice < metar.max_difficulty_reached) {
                    SDL_strlcpy(status_msg,
                        "Cannot select easier difficulty - locked for this story run!",
                        sizeof(status_msg));
                    status_attr = TERM_RED;
                } else {
                    choice = new_choice;
                    status_msg[0] = '\0';
                }
            }
        }
    }

    if (choice != metar.type)
    {
        if (choice > metar.type) {
            const char *lines[] = {
                "If you increase the difficulty level, you will not be able to return to an easier level for the rest of this story run.",
                "This change is permanent for this meta-run."
            };
            const byte attrs[] = { TERM_WHITE, TERM_L_RED };
            bool confirm = metarun_ui_confirm_modal("Increase Difficulty",
                TERM_YELLOW, lines, attrs, (int)N_ELEMENTS(lines), steamdeck,
                accept_label, back_label);

            if (!confirm) {
                ui_information_scene_leave(&info_scope);
                if (reopen_stats_on_exit)
                    print_metarun_stats();
                return;
            }
        }

        log_info("Changing difficulty from %d to %d", metar.type, choice);

        {
            int8_t preserved_stacks[METAR_CURSE_SLOTS];
            u64b preserved_seen = metar.curses_seen;

            memcpy(preserved_stacks, metar.curse_stacks,
                sizeof(preserved_stacks));
            memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
            metar.curses_seen = 0;
            metar.type = (byte)choice;
            apply_difficulty_curses(&metar);

            {
                int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
                for (int curse_id = 0; curse_id < limit; curse_id++) {
                    int preserved = preserved_stacks[curse_id];
                    int combined;
                    int curse_cap;
                    int blessing_cap;

                    if (!preserved) continue;
                    combined = preserved + CURSE_GET(curse_id);
                    curse_cap = CURSE_CURSE_CAP(curse_id);
                    blessing_cap = CURSE_BLESSING_CAP(curse_id);
                    if (curse_cap > 0 && combined > curse_cap)
                        combined = curse_cap;
                    if (blessing_cap > 0 && combined < -blessing_cap)
                        combined = -blessing_cap;
                    CURSE_SET(curse_id, combined);
                }
            }

            if (choice > metar.max_difficulty_reached)
                metar.max_difficulty_reached = (byte)choice;
            metar.curses_seen |= preserved_seen;
        }

        if (!sync_current_metarun_slot(false))
            log_warn("Difficulty change failed to sync metarun slot");
        save_metaruns();

        {
            const char *new_name = "Unknown";
            if (runtype_info && choice < z_info->rt_max
                && runtype_info[choice].name[0])
            {
                new_name = runtype_info[choice].name;
            }
            msg_print(format("Difficulty changed to: %s", new_name));
        }
    }

    ui_information_scene_leave(&info_scope);
    if (reopen_stats_on_exit)
        print_metarun_stats();
}

/* compact table of all meta-runs */
static bool metarun_list_history_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    refresh_current_metar_score();

    if (metarun_max > 0 && metaruns) {
        for (s16b i = 0; i < metarun_max; i++) {
            metaruns[i].score = compute_metarun_score(&metaruns[i]);
        }
    }

    s16b *order = NULL;
    if (metarun_max > 0 && metaruns) {
        order = mem_alloc_array(metarun_max, s16b);
        for (s16b i = 0; i < metarun_max; i++) order[i] = i;
        qsort(order, metarun_max, sizeof(s16b), compare_metarun_indices);
    }

    {
        bool semantic_ok = true;
        int highlight = 0;
        bool first_present = true;

        for (s16b i = 0; i < metarun_max; i++) {
            s16b idx = order ? order[i] : i;
            if (idx == current_run) {
                highlight = i;
                break;
            }
        }

        if (metarun_max <= 0 || !metaruns) {
            const char *lines[] = { "No metaruns have been recorded yet." };
            const byte attrs[] = { TERM_L_DARK };
            semantic_ok = metarun_ui_show_notice_modal("Meta-run History",
                TERM_L_GREEN, lines, attrs, (int)N_ELEMENTS(lines), steamdeck,
                accept_label);
            order = mem_free(order);
            return semantic_ok;
        }

        while (true) {
            app_ui_scene scene;
            app_ui_panel *panel;
            int page_start;
            int page_end;
            char subtitle[APP_UI_TEXT_MAX];
            int key;

            if (highlight < 0)
                highlight = 0;
            if (highlight >= metarun_max)
                highlight = metarun_max - 1;

            page_start = (highlight / METARUN_HISTORY_PAGE_SIZE)
                * METARUN_HISTORY_PAGE_SIZE;
            page_end = MIN(metarun_max, page_start + METARUN_HISTORY_PAGE_SIZE);

            strnfmt(subtitle, sizeof(subtitle), "%d-%d of %d",
                page_start + 1, page_end, metarun_max);
            panel = metarun_ui_begin_browser_scene(&scene, TERM_L_GREEN,
                "Meta-run History", TERM_SLATE, subtitle);
            if (!panel) {
                semantic_ok = false;
                break;
            }

            for (int i = page_start; i < page_end; i++) {
                s16b idx = order ? order[i] : i;
                const metarun *m = &metaruns[idx];
                int win_goal = WINCON_SILMARILS;
                int death_limit = LOSECON_DEATHS;
                char label[APP_UI_LABEL_MAX];
                char meta[APP_UI_META_MAX];
                char date[16];
                char res;
                byte attr;

                if (runtype_info && m->type < z_info->rt_max)
                {
                    win_goal = runtype_info[m->type].win_con
                        ? runtype_info[m->type].win_con : WINCON_SILMARILS;
                }

                res = (m->silmarils >= win_goal) ? 'W'
                    : (m->deaths >= death_limit) ? 'L' : ' ';
                strftime(date, sizeof date, "%Y-%m-%d",
                    localtime((time_t*)&m->last_played));
                attr = (idx == current_run) ? TERM_YELLOW : TERM_WHITE;
                strnfmt(label, sizeof(label), "%c%08u",
                    (idx == current_run) ? '*' : ' ', (unsigned)m->id);
                strnfmt(meta, sizeof(meta), "%lu  S:%d D:%d %c  %s",
                    (unsigned long)m->score, m->silmarils, m->deaths, res,
                    date);
                if (!app_ui_panel_add_row(panel, idx, attr, true,
                        i == highlight, "", label, meta))
                {
                    semantic_ok = false;
                    break;
                }
            }
            if (!semantic_ok)
                break;

            if (highlight >= page_start && highlight < page_end) {
                s16b idx = order ? order[highlight] : highlight;
                const metarun *m = &metaruns[idx];
                const char *diff_name = "Unknown";
                int win_goal = WINCON_SILMARILS;
                int death_limit = LOSECON_DEATHS;
                char line[APP_UI_TEXT_MAX];
                char date[16];
                char res;

                if (runtype_info && m->type < z_info->rt_max) {
                    if (runtype_info[m->type].name[0])
                        diff_name = runtype_info[m->type].name;
                    win_goal = runtype_info[m->type].win_con
                        ? runtype_info[m->type].win_con : WINCON_SILMARILS;
                }
                res = (m->silmarils >= win_goal) ? 'W'
                    : (m->deaths >= death_limit) ? 'L' : ' ';
                strftime(date, sizeof date, "%Y-%m-%d",
                    localtime((time_t*)&m->last_played));

                app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Run");
                strnfmt(line, sizeof(line), "Run %08u", (unsigned)m->id);
                if (!app_ui_panel_add_detail_line(panel,
                        (idx == current_run) ? TERM_YELLOW : TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Difficulty: %s", diff_name);
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Score: %lu",
                    (unsigned long)m->score);
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Silmarils / Deaths: %d / %d",
                    m->silmarils, m->deaths);
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Result: %c", res ? res : ' ');
                if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
                {
                    semantic_ok = false;
                    break;
                }
                strnfmt(line, sizeof(line), "Last played: %s", date);
                if (!app_ui_panel_add_detail_line(panel, TERM_SLATE, line))
                {
                    semantic_ok = false;
                    break;
                }
            }

            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                steamdeck ? accept_label : "Enter", "Close");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "8/2", "Move");
            if (metarun_max > METARUN_HISTORY_PAGE_SIZE) {
                (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                    "4/6", "Page");
            }
            if (steamdeck) {
                (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                    back_label, "Back");
            }

            if (!metarun_ui_present_scene(&scene, first_present)) {
                semantic_ok = false;
                break;
            }
            first_present = false;

            key = ui_information_scene_wait_key_nonrepeat();
            if (key == '8' || key == 'k' || key == '-') {
                highlight = (highlight + metarun_max - 1) % metarun_max;
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                highlight = (highlight + 1) % metarun_max;
                continue;
            }
            if (metarun_max > METARUN_HISTORY_PAGE_SIZE && key == '4') {
                highlight -= METARUN_HISTORY_PAGE_SIZE;
                if (highlight < 0)
                    highlight = 0;
                continue;
            }
            if (metarun_max > METARUN_HISTORY_PAGE_SIZE && key == '6') {
                highlight += METARUN_HISTORY_PAGE_SIZE;
                if (highlight >= metarun_max)
                    highlight = metarun_max - 1;
                continue;
            }
            metarun_ui_clear_pending_input();
            break;
        }

        order = mem_free(order);
        return semantic_ok;
    }
}

void list_metaruns(void)
{
    ui_information_scene_scope info_scope;
    bool use_information_scene = ui_information_scene_enter(&info_scope);
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=ok */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
    }
    if (!use_information_scene) {
        log_error("list_metaruns: semantic scene unavailable");
        return;
    }

    if (!metarun_list_history_information_scene(steamdeck, accept_label,
            back_label))
    {
        log_error("list_metaruns: failed to publish semantic scene");
    }

    ui_information_scene_leave(&info_scope);
}

void show_known_curses_menu(void)
{
    ui_information_scene_scope info_scope;
    bool use_information_scene;
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck)
    {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
    }

    use_information_scene = ui_information_scene_enter(&info_scope);
    if (!use_information_scene)
    {
        log_error("show_known_curses_menu: semantic scene unavailable");
        return;
    }

    if (!metarun_show_known_curses_information_scene(steamdeck, accept_label,
            back_label))
    {
        log_error("show_known_curses_menu: failed to publish semantic scene");
    }

    ui_information_scene_leave(&info_scope);
}

/* Public wrapper for difficulty selection menu */
void choose_difficulty_level(void)
{
    choose_difficulty_menu(false);
}
