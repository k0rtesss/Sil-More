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

#include "metarun-internal.h"

static bool metarun_show_active_effects_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    int active_count = 0;
    int active_ids[64];
    int selected = 0;
    bool first_present = true;

    for (int id = 0; id < z_info->cu_max && active_count < 64; id++)
    {
        if (CURSE_GET(id) != 0)
            active_ids[active_count++] = id;
    }

    if (active_count == 0)
    {
        const char* lines[] = {
            "No active curses or blessings remain on this saga."
        };
        const byte attrs[] = { TERM_L_DARK };

        return metarun_ui_show_notice_modal("All Active Effects", TERM_YELLOW,
            lines, attrs, (int)N_ELEMENTS(lines), steamdeck, accept_label);
    }

    while (true)
    {
        static const ui_browser_shell_button_key buttons[] = {
            { 1, '\r' },
            { 3, ESCAPE }
        };
        app_ui_scene scene;
        app_ui_panel* panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        strnfmt(subtitle, sizeof(subtitle), "%d active effect%s",
            active_count, (active_count == 1) ? "" : "s");
        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "All Active Effects", TERM_SLATE, subtitle);
        if (!panel)
            return false;

        for (int i = 0; i < active_count; i++)
        {
            if (!metarun_ui_add_effect_row_ex(panel, active_ids[i],
                    i == selected))
            {
                return false;
            }
        }

        if (!metarun_ui_add_effect_detail_lines(panel, active_ids[selected]))
            return false;

        {
            ui_browser_shell_footer_action actions[3];
            size_t count = 0;

            actions[count++] = (ui_browser_shell_footer_action){
                1, TERM_L_BLUE, true, steamdeck ? accept_label : "Any",
                "Close"
            };
            actions[count++] = (ui_browser_shell_footer_action){
                2, TERM_WHITE, true, "8/2", "Move"
            };
            if (steamdeck)
            {
                actions[count++] = (ui_browser_shell_footer_action){
                    3, TERM_WHITE, true, back_label, "Back"
                };
            }
            (void)ui_browser_shell_add_footer_actions(panel, actions, count);
        }

        if (!metarun_ui_present_scene(&scene, first_present))
            return false;
        first_present = false;

        key = metarun_ui_wait_browser_key(active_ids, active_count,
            &selected, NULL, 0, '\0', buttons, N_ELEMENTS(buttons));
        if (!key)
            continue;
        if (key == '8' || key == 'k' || key == '-')
        {
            selected = (selected + active_count - 1) % active_count;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+')
        {
            selected = (selected + 1) % active_count;
            continue;
        }
        if (steamdeck)
        {
            if (key == steamdeck_back_key()
                || key == steamdeck_confirm_key()
                || key == '\r' || key == '\n' || key == ESCAPE)
            {
                metarun_ui_clear_pending_input();
                return true;
            }
            continue;
        }

        metarun_ui_clear_pending_input();
        return true;
    }
}

bool metarun_show_known_curses_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    int known_ids[METAR_CURSE_SLOTS];
    int known_count = 0;
    int selected = 0;
    int row_offset = 0;
    int limit;
    bool first_present = true;

    if (!z_info || !cu_info)
        return false;

    limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
    for (int id = 0; id < limit; id++)
    {
        if (!cu_info[id].name || !CURSE_SEEN(id))
            continue;
        known_ids[known_count++] = id;
    }

    if (known_count == 0)
    {
        const char* lines[] = {
            "No curse lore has been uncovered in this story run yet."
        };
        const byte attrs[] = { TERM_L_DARK };

        return metarun_ui_show_notice_modal("Known Curses", TERM_L_RED,
            lines, attrs, (int)N_ELEMENTS(lines), steamdeck, accept_label);
    }

    while (true)
    {
        static const ui_browser_shell_button_key buttons[] = {
            { 1, '\r' },
            { 4, ESCAPE }
        };
        app_ui_scene scene;
        app_ui_panel* panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        if (selected < 0)
            selected = 0;
        if (selected >= known_count)
            selected = known_count - 1;
        if (row_offset > selected)
            row_offset = selected;
        if (selected >= row_offset + METARUN_KNOWN_CURSE_PAGE_SIZE)
            row_offset = selected - METARUN_KNOWN_CURSE_PAGE_SIZE + 1;
        if (row_offset < 0)
            row_offset = 0;

        strnfmt(subtitle, sizeof(subtitle), "%d known curse%s",
            known_count, (known_count == 1) ? "" : "s");
        panel = metarun_ui_begin_browser_scene(&scene, TERM_L_RED,
            "Known Curses", TERM_SLATE, subtitle);
        if (!panel)
            return false;

        for (int i = 0; i < known_count; i++)
        {
            const char* blessing_name =
                metarun_blessing_display_name(known_ids[i]);
            const char* curse_name =
                metarun_curse_display_name(known_ids[i]);
            char meta[APP_UI_META_MAX];
            byte meta_attr = TERM_SLATE;

            meta[0] = '\0';
            if (blessing_name && blessing_name[0]
                && strcmp(blessing_name, curse_name) != 0)
            {
                SDL_strlcpy(meta, blessing_name, sizeof(meta));
                meta_attr = TERM_L_GREEN;
            }

            if (!app_ui_panel_add_row_ex(panel, known_ids[i], TERM_L_RED,
                    meta_attr, 0, '\0', true, i == selected, "", curse_name,
                    meta))
            {
                return false;
            }
        }

        app_ui_panel_set_row_offset(panel, (s16b)row_offset);
        if (!metarun_ui_add_known_curse_detail_lines(panel, known_ids[selected]))
            return false;

        {
            ui_browser_shell_footer_action actions[4];
            size_t count = 0;

            actions[count++] = (ui_browser_shell_footer_action){
                1, TERM_L_BLUE, true, steamdeck ? accept_label : "Enter",
                "Close"
            };
            actions[count++] = (ui_browser_shell_footer_action){
                2, TERM_WHITE, true, "8/2", "Move"
            };
            if (known_count > METARUN_KNOWN_CURSE_PAGE_SIZE)
            {
                actions[count++] = (ui_browser_shell_footer_action){
                    3, TERM_WHITE, true, "4/6", "Page"
                };
            }
            actions[count++] = (ui_browser_shell_footer_action){
                4, TERM_WHITE, true, steamdeck ? back_label : "Esc", "Back"
            };
            (void)ui_browser_shell_add_footer_actions(panel, actions, count);
        }

        if (!metarun_ui_present_scene(&scene, first_present))
            return false;
        first_present = false;

        key = metarun_ui_wait_browser_key(known_ids, known_count, &selected,
            &row_offset, METARUN_KNOWN_CURSE_PAGE_SIZE, '\0', buttons,
            N_ELEMENTS(buttons));
        if (!key)
            continue;
        if (key == '8' || key == 'k' || key == '-')
        {
            selected = (selected + known_count - 1) % known_count;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+')
        {
            selected = (selected + 1) % known_count;
            continue;
        }
        if (known_count > METARUN_KNOWN_CURSE_PAGE_SIZE
            && (key == '4' || key == 'h' || key == 'H'))
        {
            selected -= METARUN_KNOWN_CURSE_PAGE_SIZE;
            if (selected < 0)
                selected = 0;
            continue;
        }
        if (known_count > METARUN_KNOWN_CURSE_PAGE_SIZE && key == '6')
        {
            selected += METARUN_KNOWN_CURSE_PAGE_SIZE;
            if (selected >= known_count)
                selected = known_count - 1;
            continue;
        }
        if (key == ESCAPE || key == '\r' || key == '\n'
            || (steamdeck
                && (key == steamdeck_confirm_key()
                    || key == steamdeck_back_key())))
        {
            metarun_ui_clear_pending_input();
            break;
        }

        metarun_ui_clear_pending_input();
        break;
    }

    return true;
}

static int blessing_points_remaining(void)
{
    int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    int spent = metar.blessing_points_spent;
    if (spent > earned) spent = earned;
    int available = earned - spent;
    if (available < 0) available = 0;
    return available;
}

static void blessing_spend_points(int cost)
{
    if (cost <= 0) return;
    int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    int spent = metar.blessing_points_spent + cost;
    if (spent > earned) spent = earned;
    if (spent < 0) spent = 0;
    metar.blessing_points_spent = (u16b)spent;
}

static void blessing_commit_changes(bool apply_runtime)
{
    if (!sync_current_metarun_slot(false)) {
        log_warn("blessing_commit_changes: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    refresh_current_metar_score();
    if (apply_runtime) {
        metarun_apply_runtime_effects();
    }
    save_metaruns();
}

typedef struct metarun_major_blessing_choice {
    int idx;
    char key;
    int cost;
} metarun_major_blessing_choice;

typedef enum metarun_blessing_scene_mode {
    METARUN_BLESSING_SCENE_MAIN = 0,
    METARUN_BLESSING_SCENE_REMOVE = 1,
    METARUN_BLESSING_SCENE_MINOR = 2,
    METARUN_BLESSING_SCENE_MAJOR = 3
} metarun_blessing_scene_mode;

static const char* metarun_blessing_scene_mode_name(int mode)
{
    switch (mode) {
    case METARUN_BLESSING_SCENE_MAIN:
        return "main";
    case METARUN_BLESSING_SCENE_REMOVE:
        return "remove";
    case METARUN_BLESSING_SCENE_MINOR:
        return "minor";
    case METARUN_BLESSING_SCENE_MAJOR:
        return "major";
    default:
        return "unknown";
    }
}

static void metarun_log_blessing_key(const char* context, int mode, int key)
{
    char printable = ((key >= 32) && (key <= 126)) ? (char)key : '?';

    log_debug("[metarun-esc-trace] %s mode=%s key=%d char=%c esc=%d active=%d",
        context ? context : "blessing", metarun_blessing_scene_mode_name(mode),
        key, printable, key == ESCAPE ? 1 : 0,
        ui_information_scene_is_active() ? 1 : 0);
}

static int blessing_collect_removable_curses(int *ids, int capacity)
{
    int count = 0;

    if (!ids || capacity <= 0)
        return 0;

    for (int id = 0; id < z_info->cu_max && count < capacity; id++) {
        if (CURSE_CURSE_STACK(id) > 0)
            ids[count++] = id;
    }

    return count;
}

static bool blessing_apply_remove_curse_choice(int curse_id, char *result_msg,
    size_t msg_size, byte *result_attr)
{
    int current_stacks;

    if (curse_id < 0 || curse_id >= z_info->cu_max
        || CURSE_CURSE_STACK(curse_id) <= 0)
    {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "No curses cling to this saga.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    current_stacks = CURSE_CURSE_STACK(curse_id);
    if (current_stacks > 1)
        CURSE_SET(curse_id, current_stacks - 1);
    else
        CURSE_SET(curse_id, 0);
    CURSE_SEEN_SET(curse_id);

    blessing_spend_points(1);

    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++)
        metar.pending_blessing_choices[i] = 255;

    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        if (current_stacks > 1) {
            snprintf(result_msg, msg_size,
                "One stack of %s is lifted. (%d remain%s)",
                metarun_curse_display_name(curse_id), current_stacks - 1,
                (current_stacks - 1 == 1) ? "s" : "");
        } else {
            snprintf(result_msg, msg_size, "The curse of %s is lifted.",
                metarun_curse_display_name(curse_id));
        }
        if (result_attr) *result_attr = TERM_L_BLUE;
    }

    return true;
}

static bool blessing_prepare_minor_choices(int *options, int *out_picks,
    char *result_msg, size_t msg_size, byte *result_attr)
{
    int picks = 0;
    bool have_valid_pending = false;

    if (!options || !out_picks)
        return false;

    if (metar.pending_blessing_count > 0) {
        for (int i = 0; i < metar.pending_blessing_count && i < 3; i++) {
            int id = metar.pending_blessing_choices[i];
            curse_type *c;
            int stacks;
            int blessing_stacks;

            if (id == 255) continue;
            c = &cu_info[id];
            if (!c->blessing_name) continue;

            stacks = CURSE_GET(id);
            if (stacks > 0) continue;

            blessing_stacks = (stacks < 0) ? -stacks : 0;
            if (CURSE_BLESSING_CAP(id) > 0
                && blessing_stacks >= CURSE_BLESSING_CAP(id))
            {
                continue;
            }

            options[picks++] = id;
        }

        if (picks > 0)
            have_valid_pending = true;
    }

    if (!have_valid_pending) {
        int eligible[METAR_CURSE_SLOTS];
        int weights[METAR_CURSE_SLOTS];
        int count = 0;
        int total_weight = 0;

        for (int id = 0; id < z_info->cu_max; id++) {
            curse_type *c = &cu_info[id];
            int stacks;
            int blessing_stacks;
            int base_weight;
            int effective_weight;

            if (!c->blessing_name) continue;

            stacks = CURSE_GET(id);
            if (stacks > 0) continue;

            blessing_stacks = (stacks < 0) ? -stacks : 0;
            if (CURSE_BLESSING_CAP(id) > 0
                && blessing_stacks >= CURSE_BLESSING_CAP(id))
            {
                continue;
            }

            if (count >= METAR_CURSE_SLOTS)
                continue;

            eligible[count] = id;
            base_weight = c->weight > 0 ? c->weight : 1;
            effective_weight = base_weight / (blessing_stacks + 1);
            weights[count] = (effective_weight > 0) ? effective_weight : 1;
            total_weight += weights[count];
            count++;
        }

        if (count == 0) {
            if (result_msg && msg_size > 0) {
                SDL_strlcpy(result_msg,
                    "No blessings are presently available.", msg_size);
                if (result_attr) *result_attr = TERM_L_DARK;
            }
            *out_picks = 0;
            return false;
        }

        picks = MIN(3, count);
        for (int i = 0; i < picks; i++) {
            int roll = rand_int(total_weight);
            int sum = 0;
            int selected = 0;

            for (int j = 0; j < count; j++) {
                sum += weights[j];
                if (roll < sum) {
                    selected = j;
                    break;
                }
            }

            options[i] = eligible[selected];
            total_weight -= weights[selected];
            eligible[selected] = eligible[count - 1];
            weights[selected] = weights[count - 1];
            count--;
        }

        metar.pending_blessing_count = picks;
        for (int i = 0; i < 3; i++) {
            metar.pending_blessing_choices[i] = (i < picks)
                ? options[i] : 255;
        }
        save_metaruns();
    }

    *out_picks = picks;
    return picks > 0;
}

static bool blessing_apply_minor_choice(int blessing_id, char *result_msg,
    size_t msg_size, byte *result_attr)
{
    int stacks = CURSE_GET(blessing_id);
    int blessing_stacks = (stacks < 0) ? -stacks : 0;

    if (blessing_id < 0 || blessing_id >= z_info->cu_max
        || !cu_info[blessing_id].blessing_name)
    {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "That blessing is no longer available.",
                msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    if (CURSE_BLESSING_CAP(blessing_id) > 0
        && blessing_stacks >= CURSE_BLESSING_CAP(blessing_id))
    {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg,
                "That blessing cannot grow any stronger.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    CURSE_ADD(blessing_id, -1);
    CURSE_SEEN_SET(blessing_id);
    blessing_spend_points(1);

    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++)
        metar.pending_blessing_choices[i] = 255;

    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        snprintf(result_msg, msg_size, "You receive the %s.",
            metarun_blessing_display_name(blessing_id));
        if (result_attr) *result_attr = TERM_L_GREEN;
    }

    return true;
}

static int blessing_collect_major_choices(
    metarun_major_blessing_choice *options, int capacity, int *out_min_cost)
{
    int option_count = 0;
    int min_cost = INT_MAX;
    int cap;

    if (out_min_cost)
        *out_min_cost = 0;

    metarun_sanitize_major_blessing_bits(&metar);
    cap = major_blessing_capacity();
    if (cap <= 0 || !mb_info || !options || capacity <= 0)
        return 0;

    for (int i = 0; i < cap && option_count < capacity; i++) {
        int cost;

        if (metarun_has_major_blessing_index(i)) continue;
        if (!major_blessing_def(i)) continue;

        cost = major_blessing_cost(i);
        if (cost < 0) cost = 0;
        options[option_count].idx = i;
        options[option_count].key = (char)('a' + option_count);
        options[option_count].cost = cost;
        if (cost < min_cost)
            min_cost = cost;
        option_count++;
    }

    if (out_min_cost) {
        *out_min_cost = (min_cost == INT_MAX) ? 0 : min_cost;
    }

    return option_count;
}

static bool blessing_apply_major_choice(int choice_idx, char *result_msg,
    size_t msg_size, byte *result_attr)
{
    metar.major_blessings |= (1U << choice_idx);
    blessing_spend_points(major_blessing_cost(choice_idx));
    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        const char *msg = major_blessing_unlock_msg(choice_idx);

        if (msg && *msg) {
            SDL_strlcpy(result_msg, msg, msg_size);
        } else {
            snprintf(result_msg, msg_size, "You seal the %s.",
                major_blessing_name_str(choice_idx));
        }
        if (result_attr) *result_attr = TERM_YELLOW;
    }

    return true;
}

static bool open_blessing_exchange_information_scene(bool steamdeck,
    const char* accept_label, const char* back_label)
{
    metarun_blessing_scene_mode mode = METARUN_BLESSING_SCENE_MAIN;
    int selected_main = 0;
    int selected_remove = 0;
    int selected_minor = 0;
    int selected_major = 0;
    char status_msg[256] = "";
    byte status_attr = TERM_WHITE;
    bool clear_status_on_next_key = false;
    bool first_present = true;
    const ui_browser_shell_button_key buttons[] = {
        { 1, '\r' },
        { 3, ESCAPE }
    };

    log_debug("[metarun-esc-trace] blessing exchange semantic enter");

    while (true) {
        int available;
        int earned;
        int spent;
        int main_option_count = 3;
        int min_major_cost = 0;
        metarun_major_blessing_choice major_options[16];
        int major_option_count;
        bool major_available;
        bool major_affordable;

        compute_blessing_pool();
        available = blessing_points_remaining();
        earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        spent = metar.blessing_points_spent;
        major_option_count = blessing_collect_major_choices(major_options,
            (int)N_ELEMENTS(major_options), &min_major_cost);
        major_available = major_option_count > 0;
        major_affordable = major_available && (min_major_cost <= available);

        if (mode == METARUN_BLESSING_SCENE_MAIN) {
            app_ui_scene scene;
            app_ui_panel *panel;
            char subtitle[APP_UI_TEXT_MAX];
            char buf[APP_UI_TEXT_MAX];
            int key;
            u32b threshold = metarun_threshold_value(&metar);

            if (threshold == 0) threshold = 1;
            if (selected_main < 0) selected_main = 0;
            if (selected_main >= main_option_count)
                selected_main = main_option_count - 1;

            strnfmt(subtitle, sizeof(subtitle),
                "%d available (%d spent / %d earned)", available, spent,
                earned);
            panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
                "Blessing Exchange", TERM_SLATE, subtitle);
            if (!panel)
                return false;

            strnfmt(buf, sizeof(buf),
                "Fallen score pool: %lu total, %lu / %lu to next blessing",
                (unsigned long)metar.fallen_score_total,
                (unsigned long)metar.fallen_score_pool,
                (unsigned long)threshold);
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE, buf);
            if (status_msg[0] != '\0')
                (void)app_ui_panel_add_body_line(panel, status_attr, status_msg);

            if (!app_ui_panel_add_row(panel, 0, TERM_WHITE, true,
                    selected_main == 0, "r", "Remove a curse", "cost 1"))
            {
                return false;
            }
            if (!app_ui_panel_add_row(panel, 1, TERM_WHITE, true,
                    selected_main == 1, "m", "Gain a minor blessing",
                    "cost 1"))
            {
                return false;
            }
            if (major_available) {
                strnfmt(buf, sizeof(buf), "cost %d", min_major_cost);
                if (!app_ui_panel_add_row(panel, 2,
                        major_affordable ? TERM_WHITE : TERM_L_DARK, true,
                        selected_main == 2, "u",
                        "Unlock a major blessing", buf))
                {
                    return false;
                }
            } else {
                if (!app_ui_panel_add_row(panel, 2, TERM_L_DARK, true,
                        selected_main == 2, "u",
                        "Unlock a major blessing", "none available"))
                {
                    return false;
                }
            }

            app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Option");
            if (selected_main == 0) {
                (void)app_ui_panel_add_detail_line(panel, TERM_WHITE,
                    "Lift one stack from an active curse.");
                (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
                    "Available immediately if you have a blessing point.");
            } else if (selected_main == 1) {
                (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                    "Accept one of three offered minor blessings.");
                (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
                    "Offered gifts persist until chosen or invalidated.");
            } else {
                if (major_available) {
                    strnfmt(buf, sizeof(buf), "Lowest available cost: %d",
                        min_major_cost);
                    (void)app_ui_panel_add_detail_line(panel,
                        major_affordable ? TERM_YELLOW : TERM_L_DARK,
                        "Seal a permanent major blessing for this metarun.");
                    (void)app_ui_panel_add_detail_line(panel,
                        major_affordable ? TERM_WHITE : TERM_L_DARK, buf);
                    if (!major_affordable) {
                        strnfmt(buf, sizeof(buf),
                            "You need %d blessing points to unlock one.",
                            min_major_cost);
                        (void)app_ui_panel_add_detail_line(panel, TERM_ORANGE,
                            buf);
                    }
                } else {
                    (void)app_ui_panel_add_detail_line(panel, TERM_L_DARK,
                        "All major blessings have already been sealed.");
                }
            }

            {
                const ui_browser_shell_footer_action actions[] = {
                    { 1, TERM_L_BLUE, true,
                        steamdeck ? accept_label : "Enter", "Choose" },
                    { 2, TERM_WHITE, true, "8/2", "Move" },
                    { 3, TERM_WHITE, true,
                        steamdeck ? back_label : "Esc", "Leave" }
                };

                (void)ui_browser_shell_add_footer_actions(panel, actions,
                    N_ELEMENTS(actions));
            }

            if (!metarun_ui_present_scene(&scene, first_present))
                return false;
            first_present = false;

            key = metarun_ui_wait_browser_key(NULL, main_option_count,
                &selected_main, NULL, 0, '\r', buttons, N_ELEMENTS(buttons));
            metarun_log_blessing_key("blessing-scene-read", mode, key);
            if (!key)
                continue;
            if (clear_status_on_next_key || key == '8' || key == 'k'
                || key == '-' || key == '2' || key == 'j' || key == '+')
            {
                status_msg[0] = '\0';
                clear_status_on_next_key = false;
            }

            if (key == ESCAPE || key == '4'
                || (steamdeck && key == steamdeck_back_key())
                || (!steamdeck && (key == 'h' || key == 'H')))
            {
                metarun_ui_clear_pending_input();
                return true;
            }
            if (key == '8' || key == 'k' || key == '-') {
                selected_main = (selected_main + main_option_count - 1)
                    % main_option_count;
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                selected_main = (selected_main + 1) % main_option_count;
                continue;
            }
            if (key == '\r' || key == '\n'
                || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
            {
                key = (selected_main == 0) ? 'r'
                    : (selected_main == 1) ? 'm' : 'u';
            }

            if (key == 'r' || key == 'R') {
                if (available < 1) {
                    SDL_strlcpy(status_msg,
                        "You need at least one blessing point to lift a curse.",
                        sizeof(status_msg));
                    status_attr = TERM_ORANGE;
                    clear_status_on_next_key = true;
                } else {
                    log_debug("[metarun-esc-trace] blessing mode main->remove");
                    mode = METARUN_BLESSING_SCENE_REMOVE;
                }
                continue;
            }
            if (key == 'm' || key == 'M') {
                if (available < 1) {
                    SDL_strlcpy(status_msg,
                        "You need at least one blessing point to receive a gift.",
                        sizeof(status_msg));
                    status_attr = TERM_ORANGE;
                    clear_status_on_next_key = true;
                } else {
                    log_debug("[metarun-esc-trace] blessing mode main->minor");
                    mode = METARUN_BLESSING_SCENE_MINOR;
                }
                continue;
            }
            if (key == 'u' || key == 'U') {
                if (!major_available) {
                    SDL_strlcpy(status_msg,
                        "All major blessings have already been sealed.",
                        sizeof(status_msg));
                    status_attr = TERM_L_DARK;
                    clear_status_on_next_key = true;
                } else if (!major_affordable) {
                    snprintf(status_msg, sizeof(status_msg),
                        "You need %d blessing points to unlock a major blessing.",
                        min_major_cost);
                    status_attr = TERM_ORANGE;
                    clear_status_on_next_key = true;
                } else {
                    log_debug("[metarun-esc-trace] blessing mode main->major");
                    mode = METARUN_BLESSING_SCENE_MAJOR;
                }
                continue;
            }

            bell("Unrecognised option.");
            continue;
        }

        if (mode == METARUN_BLESSING_SCENE_REMOVE) {
            int ids[METAR_CURSE_SLOTS];
            int count = blessing_collect_removable_curses(ids,
                (int)N_ELEMENTS(ids));
            app_ui_scene scene;
            app_ui_panel *panel;
            int key;

            if (count <= 0) {
                SDL_strlcpy(status_msg, "No curses cling to this saga.",
                    sizeof(status_msg));
                status_attr = TERM_L_DARK;
                clear_status_on_next_key = true;
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            if (selected_remove < 0) selected_remove = 0;
            if (selected_remove >= count) selected_remove = count - 1;

            panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
                "Remove a Curse", TERM_SLATE, "Cost: 1 blessing point");
            if (!panel)
                return false;

            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                "Choose which curse to lift.");

            for (int i = 0; i < count; i++) {
                char key_buf[APP_UI_KEY_MAX];
                char meta[APP_UI_META_MAX];

                strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
                strnfmt(meta, sizeof(meta), "stacks: %d",
                    CURSE_CURSE_STACK(ids[i]));
                if (!app_ui_panel_add_row(panel, ids[i], TERM_RED, true,
                        i == selected_remove, key_buf,
                        metarun_curse_display_name(ids[i]), meta))
                {
                    return false;
                }
            }

            if (!metarun_ui_add_effect_detail_lines(panel, ids[selected_remove]))
                return false;

            {
                const ui_browser_shell_footer_action actions[] = {
                    { 1, TERM_L_BLUE, true,
                        steamdeck ? accept_label : "Enter", "Lift" },
                    { 2, TERM_WHITE, true, "8/2", "Move" },
                    { 3, TERM_WHITE, true,
                        steamdeck ? back_label : "Esc", "Back" }
                };

                (void)ui_browser_shell_add_footer_actions(panel, actions,
                    N_ELEMENTS(actions));
            }

            if (!metarun_ui_present_scene(&scene, first_present))
                return false;
            first_present = false;

            key = metarun_ui_wait_browser_key(ids, count, &selected_remove,
                NULL, 0, '\r', buttons, N_ELEMENTS(buttons));
            metarun_log_blessing_key("blessing-scene-read", mode, key);
            if (!key)
                continue;
            if (key == ESCAPE || key == '4'
                || (steamdeck && key == steamdeck_back_key())
                || (!steamdeck && (key == 'h' || key == 'H')))
            {
                log_debug("[metarun-esc-trace] blessing mode remove->main via back");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }
            if (key == '8' || key == 'k' || key == '-') {
                selected_remove = (selected_remove + count - 1) % count;
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                selected_remove = (selected_remove + 1) % count;
                continue;
            }
            if (key == '\r' || key == '\n'
                || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
            {
                (void)blessing_apply_remove_curse_choice(ids[selected_remove],
                    status_msg, sizeof(status_msg), &status_attr);
                clear_status_on_next_key = true;
                log_debug("[metarun-esc-trace] blessing mode remove->main via apply");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            {
                int idx = (key >= 'A' && key <= 'Z') ? key - 'A' : key - 'a';
                if (idx >= 0 && idx < count) {
                    selected_remove = idx;
                    (void)blessing_apply_remove_curse_choice(ids[idx],
                        status_msg, sizeof(status_msg), &status_attr);
                    clear_status_on_next_key = true;
                    log_debug("[metarun-esc-trace] blessing mode remove->main via letter");
                    mode = METARUN_BLESSING_SCENE_MAIN;
                    continue;
                }
            }

            bell("Invalid selection.");
            continue;
        }

        if (mode == METARUN_BLESSING_SCENE_MINOR) {
            int options[3];
            int picks = 0;
            app_ui_scene scene;
            app_ui_panel *panel;
            int key;

            if (!blessing_prepare_minor_choices(options, &picks, status_msg,
                    sizeof(status_msg), &status_attr))
            {
                clear_status_on_next_key = true;
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            if (selected_minor < 0) selected_minor = 0;
            if (selected_minor >= picks) selected_minor = picks - 1;

            panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
                "Receive a Blessing", TERM_SLATE, "Cost: 1 blessing point");
            if (!panel)
                return false;

            (void)app_ui_panel_add_body_line(panel, TERM_WHITE,
                "Select a gift to accept.");

            for (int i = 0; i < picks; i++) {
                int blessing_stacks = 0;
                char key_buf[APP_UI_KEY_MAX];
                char meta[APP_UI_META_MAX];
                int stacks = CURSE_GET(options[i]);

                if (stacks < 0)
                    blessing_stacks = -stacks;
                strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
                strnfmt(meta, sizeof(meta), "current: %d", blessing_stacks);
                if (!app_ui_panel_add_row(panel, options[i], TERM_L_GREEN, true,
                        i == selected_minor, key_buf,
                        metarun_blessing_display_name(options[i]), meta))
                {
                    return false;
                }
            }

            {
                curse_type *c = &cu_info[options[selected_minor]];
                app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                    "Selected Blessing");
                (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                    metarun_blessing_display_name(options[selected_minor]));
                if (c->blessing_text) {
                    if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_WHITE,
                            cu_text + c->blessing_text))
                    {
                        return false;
                    }
                }
                if (c->blessing_power) {
                    if (!metarun_ui_add_wrapped_detail_lines(panel,
                            TERM_L_GREEN, cu_text + c->blessing_power))
                    {
                        return false;
                    }
                }
            }

            {
                const ui_browser_shell_footer_action actions[] = {
                    { 1, TERM_L_BLUE, true,
                        steamdeck ? accept_label : "Enter", "Accept" },
                    { 2, TERM_WHITE, true, "8/2", "Move" },
                    { 3, TERM_WHITE, true,
                        steamdeck ? back_label : "Esc", "Back" }
                };

                (void)ui_browser_shell_add_footer_actions(panel, actions,
                    N_ELEMENTS(actions));
            }

            if (!metarun_ui_present_scene(&scene, first_present))
                return false;
            first_present = false;

            key = metarun_ui_wait_browser_key(options, picks,
                &selected_minor, NULL, 0, '\r', buttons, N_ELEMENTS(buttons));
            metarun_log_blessing_key("blessing-scene-read", mode, key);
            if (!key)
                continue;
            if (key == ESCAPE || key == '4'
                || (steamdeck && key == steamdeck_back_key())
                || (!steamdeck && (key == 'h' || key == 'H')))
            {
                log_debug("[metarun-esc-trace] blessing mode minor->main via back");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }
            if (key == '8' || key == 'k' || key == '-') {
                selected_minor = (selected_minor + picks - 1) % picks;
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                selected_minor = (selected_minor + 1) % picks;
                continue;
            }
            if (key == '\r' || key == '\n'
                || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
            {
                (void)blessing_apply_minor_choice(options[selected_minor],
                    status_msg, sizeof(status_msg), &status_attr);
                clear_status_on_next_key = true;
                log_debug("[metarun-esc-trace] blessing mode minor->main via apply");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            {
                int idx = (key >= 'A' && key <= 'Z') ? key - 'A' : key - 'a';
                if (idx >= 0 && idx < picks) {
                    selected_minor = idx;
                    (void)blessing_apply_minor_choice(options[idx], status_msg,
                        sizeof(status_msg), &status_attr);
                    clear_status_on_next_key = true;
                    log_debug("[metarun-esc-trace] blessing mode minor->main via letter");
                    mode = METARUN_BLESSING_SCENE_MAIN;
                    continue;
                }
            }

            bell("Invalid selection.");
            continue;
        }

        if (mode == METARUN_BLESSING_SCENE_MAJOR) {
            app_ui_scene scene;
            app_ui_panel *panel;
            int key;
            int first_affordable = -1;

            if (!major_available) {
                SDL_strlcpy(status_msg,
                    "All major blessings have already been sealed.",
                    sizeof(status_msg));
                status_attr = TERM_L_DARK;
                clear_status_on_next_key = true;
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            for (int i = 0; i < major_option_count; i++) {
                if (major_options[i].cost <= available) {
                    first_affordable = i;
                    break;
                }
            }
            if (first_affordable < 0) {
                snprintf(status_msg, sizeof(status_msg),
                    "You need %d blessing points to unlock a major blessing.",
                    min_major_cost);
                status_attr = TERM_ORANGE;
                clear_status_on_next_key = true;
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }
            if (selected_major < 0 || selected_major >= major_option_count
                || major_options[selected_major].cost > available)
            {
                selected_major = first_affordable;
            }

            panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
                "Unlock a Major Blessing", TERM_SLATE,
                "Forge a covenant for this saga.");
            if (!panel)
                return false;

            for (int i = 0; i < major_option_count; i++) {
                bool affordable = (major_options[i].cost <= available);
                char key_buf[APP_UI_KEY_MAX];
                char meta[APP_UI_META_MAX];

                strnfmt(key_buf, sizeof(key_buf), "%c", major_options[i].key);
                strnfmt(meta, sizeof(meta), "cost %d", major_options[i].cost);
                if (!app_ui_panel_add_row(panel, i,
                        affordable ? TERM_L_GREEN : TERM_L_DARK, true,
                        i == selected_major, key_buf,
                        major_blessing_name_str(major_options[i].idx), meta))
                {
                    return false;
                }
            }

            {
                int idx = major_options[selected_major].idx;
                const char *detail = major_blessing_detail_desc(idx);
                char line[APP_UI_TEXT_MAX];

                app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                    "Selected Covenant");
                (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                    major_blessing_name_str(idx));
                strnfmt(line, sizeof(line), "Cost: %d blessing point%s",
                    major_options[selected_major].cost,
                    (major_options[selected_major].cost == 1) ? "" : "s");
                (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, line);
                strnfmt(line, sizeof(line), "Available: %d", available);
                (void)app_ui_panel_add_detail_line(panel, TERM_L_BLUE, line);
                if (detail && *detail) {
                    if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_WHITE,
                            detail))
                    {
                        return false;
                    }
                }
            }

            {
                const ui_browser_shell_footer_action actions[] = {
                    { 1, TERM_L_BLUE, true,
                        steamdeck ? accept_label : "Enter", "Unlock" },
                    { 2, TERM_WHITE, true, "8/2", "Move" },
                    { 3, TERM_WHITE, true,
                        steamdeck ? back_label : "Esc", "Back" }
                };

                (void)ui_browser_shell_add_footer_actions(panel, actions,
                    N_ELEMENTS(actions));
            }

            if (!metarun_ui_present_scene(&scene, first_present))
                return false;
            first_present = false;

            key = metarun_ui_wait_browser_key(NULL, major_option_count,
                &selected_major, NULL, 0, '\r', buttons, N_ELEMENTS(buttons));
            metarun_log_blessing_key("blessing-scene-read", mode, key);
            if (!key)
                continue;
            if (key == ESCAPE || key == '4'
                || (steamdeck && key == steamdeck_back_key())
                || (!steamdeck && (key == 'h' || key == 'H')))
            {
                log_debug("[metarun-esc-trace] blessing mode major->main via back");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }
            if (key == '8' || key == 'k' || key == '-') {
                int start = selected_major;
                do {
                    selected_major = (selected_major + major_option_count - 1)
                        % major_option_count;
                    if (major_options[selected_major].cost <= available)
                        break;
                } while (selected_major != start);
                continue;
            }
            if (key == '2' || key == 'j' || key == '+') {
                int start = selected_major;
                do {
                    selected_major = (selected_major + 1) % major_option_count;
                    if (major_options[selected_major].cost <= available)
                        break;
                } while (selected_major != start);
                continue;
            }
            if (key == '\r' || key == '\n'
                || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
            {
                if (major_options[selected_major].cost > available) {
                    bell("Not enough blessing points for that covenant.");
                    continue;
                }
                (void)blessing_apply_major_choice(
                    major_options[selected_major].idx, status_msg,
                    sizeof(status_msg), &status_attr);
                clear_status_on_next_key = true;
                log_debug("[metarun-esc-trace] blessing mode major->main via apply");
                mode = METARUN_BLESSING_SCENE_MAIN;
                continue;
            }

            {
                int choice_idx = -1;
                char lowered = tolower((unsigned char)key);

                if (lowered >= 'a' && lowered <= 'z') {
                    for (int i = 0; i < major_option_count; i++) {
                        if (lowered != major_options[i].key)
                            continue;
                        if (major_options[i].cost > available) {
                            bell("Not enough blessing points for that covenant.");
                            choice_idx = -2;
                            break;
                        }
                        choice_idx = i;
                        break;
                    }
                }

                if (choice_idx == -2)
                    continue;
                if (choice_idx >= 0) {
                    selected_major = choice_idx;
                    (void)blessing_apply_major_choice(
                        major_options[choice_idx].idx, status_msg,
                        sizeof(status_msg), &status_attr);
                    clear_status_on_next_key = true;
                    log_debug("[metarun-esc-trace] blessing mode major->main via letter");
                    mode = METARUN_BLESSING_SCENE_MAIN;
                    continue;
                }
            }

            bell("Invalid selection.");
            continue;
        }
    }
}

typedef struct metarun_stats_view_model {
    const char* diff_name;
    const char* threshold_mode;
    int win_goal;
    int remaining_silmarils;
    int required_survivors;
    int alive;
    u32b best_run;
    u32b total_pool;
    u32b remainder;
    u32b threshold;
    int earned_points;
    int spent_points;
    int available_points;
    int unlocked_major;
    int major_total;
    bool steamdeck;
    bool blitz_enabled;
    char sil_bar[32];
    char death_marks[32];
    char spend_label[16];
    char threshold_label[16];
    char diff_label[16];
    char full_label[16];
    char history_label[16];
    char blitz_label[16];
    char back_label[16];
    char continue_label[16];
} metarun_stats_view_model;

static void metarun_stats_prepare_view_model(metarun_stats_view_model* view)
{
    if (!view)
        return;

    memset(view, 0, sizeof(*view));
    view->diff_name = "Unknown";
    view->win_goal = WINCON_SILMARILS;

    if (runtype_info && metar.type < z_info->rt_max
        && runtype_info[metar.type].name[0])
    {
        view->diff_name = runtype_info[metar.type].name;
        view->win_goal = runtype_info[metar.type].win_con
            ? runtype_info[metar.type].win_con
            : WINCON_SILMARILS;
    }

    if (view->win_goal <= 0)
        view->win_goal = WINCON_SILMARILS;

    view->remaining_silmarils = view->win_goal - metar.silmarils;
    if (view->remaining_silmarils < 0)
        view->remaining_silmarils = 0;

    build_symbol_bar(view->sil_bar, sizeof(view->sil_bar), metar.silmarils,
        view->win_goal, '*');
    build_death_marks(view->death_marks, sizeof(view->death_marks),
        metar.deaths);

    view->required_survivors = required_survivor_target(view->win_goal);
    view->alive = metar.alive_characters;
    view->best_run = get_best_run_score_from_highscores();
    view->total_pool = metar.fallen_score_total;
    view->remainder = metar.fallen_score_pool;
    view->threshold = metarun_threshold_value(&metar);
    if (view->threshold == 0)
        view->threshold = 1;
    view->threshold_mode = threshold_mode_name(
        metarun_get_threshold_mode(&metar));
    view->earned_points = metar.blessing_points;
    view->spent_points = metar.blessing_points_spent;
    view->available_points = view->earned_points - view->spent_points;
    view->steamdeck = steamdeck_controls_active();
    view->blitz_enabled = (op_ptr && op_ptr->opt[OPT_unlock_blitz_mode]);
    view->major_total = metarun_major_blessing_count();

    for (int i = 0; i < view->major_total; i++)
    {
        if (metarun_has_major_blessing_index(i))
            view->unlocked_major++;
    }

    if (view->steamdeck)
    {
        int confirm_key = steamdeck_confirm_key();
        int back_key = steamdeck_back_key();
        int alt_key = steamdeck_alt_action_key();
        int secondary_key = steamdeck_secondary_key();

        metarun_prompt_label(confirm_key, "A", view->continue_label,
            sizeof(view->continue_label));
        metarun_prompt_label(back_key, "B", view->back_label,
            sizeof(view->back_label));
        metarun_prompt_label(alt_key, "X", view->spend_label,
            sizeof(view->spend_label));
        metarun_prompt_label(secondary_key, "Y", view->history_label,
            sizeof(view->history_label));
        metarun_prompt_label('c', "L1", view->diff_label,
            sizeof(view->diff_label));
        metarun_prompt_label('f', "R1", view->threshold_label,
            sizeof(view->threshold_label));
        metarun_prompt_label('u', "Start", view->full_label,
            sizeof(view->full_label));
        metarun_prompt_label('x', "RS Right", view->blitz_label,
            sizeof(view->blitz_label));
    }
    else
    {
        SDL_strlcpy(view->continue_label, "Enter",
            sizeof(view->continue_label));
        SDL_strlcpy(view->back_label, "Esc", sizeof(view->back_label));
        SDL_strlcpy(view->spend_label, "b", sizeof(view->spend_label));
        SDL_strlcpy(view->threshold_label, "f",
            sizeof(view->threshold_label));
        SDL_strlcpy(view->diff_label, "c", sizeof(view->diff_label));
        SDL_strlcpy(view->full_label, "u", sizeof(view->full_label));
        SDL_strlcpy(view->history_label, "s", sizeof(view->history_label));
        SDL_strlcpy(view->blitz_label, "x", sizeof(view->blitz_label));
    }
}

static bool metarun_build_stats_browser_scene(app_ui_scene* scene,
    const metarun_stats_view_model* view)
{
    app_ui_panel* panel;
    char subtitle[APP_UI_TEXT_MAX];
    char line[APP_UI_TEXT_MAX];
    char meta[APP_UI_META_MAX];
    byte alive_attr;
    byte blessing_attr;
    int active_count = 0;

    if (!scene || !view)
        return false;

    strnfmt(subtitle, sizeof(subtitle), "Run-ID %u", metar.id);
    panel = metarun_ui_begin_browser_scene(scene, TERM_YELLOW,
        "Current Story Statistics", TERM_L_BLUE, subtitle);
    if (!panel)
        return false;

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Blessing Pool");

    alive_attr = (view->alive < view->required_survivors)
        ? TERM_RED
        : TERM_L_GREEN;
    blessing_attr = (view->available_points > 0) ? TERM_L_GREEN : TERM_WHITE;

    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Difficulty",
            TERM_L_BLUE, view->diff_name))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%lu", (unsigned long)metar.score);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Meta Score", TERM_WHITE,
            meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%lu", (unsigned long)view->best_run);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Best Run Score",
            TERM_WHITE, meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%s  %d / %d (remaining %d)", view->sil_bar,
        metar.silmarils, view->win_goal, view->remaining_silmarils);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Silmarils", TERM_WHITE,
            meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%d (need >= %d)", view->alive,
        view->required_survivors);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Living Heroes",
            alive_attr, meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%s (%d total)", view->death_marks,
        metar.deaths);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Deaths", TERM_WHITE,
            meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%d available (%d spent / %d earned)",
        view->available_points, view->spent_points, view->earned_points);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Blessing Points",
            blessing_attr, meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%lu total, %lu / %lu to next",
        (unsigned long)view->total_pool, (unsigned long)view->remainder,
        (unsigned long)view->threshold);
    if (!metarun_ui_add_value_row(panel, TERM_WHITE, "Blessing Pool",
            TERM_WHITE, meta))
    {
        return false;
    }
    strnfmt(meta, sizeof(meta), "%d unlocked", view->unlocked_major);
    if (!metarun_ui_add_value_row(panel, TERM_YELLOW, "Major Blessings",
            TERM_YELLOW, meta))
    {
        return false;
    }

    if (!metarun_ui_add_section_row(panel, TERM_YELLOW,
            "Active Curses & Blessings"))
    {
        return false;
    }

    for (int id = 0; id < z_info->cu_max; id++)
    {
        if (CURSE_GET(id) == 0)
            continue;

        active_count++;
        if (!metarun_ui_add_effect_row(panel, id))
            return false;
    }

    if (active_count == 0
        && !metarun_ui_add_value_row(panel, TERM_L_DARK, "None active",
            TERM_L_DARK, ""))
    {
        return false;
    }

    strnfmt(line, sizeof(line), "Total score: %lu",
        (unsigned long)view->total_pool);
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
        return false;
    strnfmt(line, sizeof(line), "Next blessing: %lu / %lu",
        (unsigned long)view->remainder, (unsigned long)view->threshold);
    if (!app_ui_panel_add_detail_line(panel, TERM_L_BLUE, line))
        return false;
    strnfmt(line, sizeof(line), "Threshold mode: %s", view->threshold_mode);
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
        return false;
    strnfmt(line, sizeof(line), "Available points: %d", view->available_points);
    if (!app_ui_panel_add_detail_line(panel, blessing_attr, line))
        return false;
    strnfmt(line, sizeof(line), "Earned / spent: %d / %d", view->earned_points,
        view->spent_points);
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
        return false;
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
        return false;
    if (!app_ui_panel_add_detail_line(panel, TERM_YELLOW, "Major Blessings"))
        return false;

    if (view->unlocked_major == 0)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_L_DARK,
                "None unlocked yet"))
        {
            return false;
        }
    }
    else
    {
        for (int i = 0; i < view->major_total; i++)
        {
            const char* desc;
            const char* name;
            char desc_buf[96];

            if (!metarun_has_major_blessing_index(i))
                continue;

            name = major_blessing_name_str(i);
            desc = major_blessing_short_desc(i);
            metarun_trim_first_line(desc_buf, sizeof(desc_buf), desc);
            if (desc_buf[0])
                strnfmt(line, sizeof(line), "%s: %s", name, desc_buf);
            else
                strnfmt(line, sizeof(line), "%s", name);

            if (!app_ui_panel_add_detail_line(panel, TERM_L_GREEN, line))
                return false;
        }
    }

    {
        ui_browser_shell_footer_action actions[8];
        size_t count = 0;

        actions[count++] = (ui_browser_shell_footer_action){
            1, TERM_L_BLUE, true, view->continue_label, "Continue"
        };
        actions[count++] = (ui_browser_shell_footer_action){
            2, TERM_L_GREEN, true, view->spend_label, "Spend"
        };
        actions[count++] = (ui_browser_shell_footer_action){
            3, TERM_WHITE, true, view->threshold_label, "Threshold"
        };
        actions[count++] = (ui_browser_shell_footer_action){
            4, TERM_WHITE, true, view->diff_label, "Difficulty"
        };
        actions[count++] = (ui_browser_shell_footer_action){
            5, TERM_WHITE, true, view->full_label, "Effects"
        };
        actions[count++] = (ui_browser_shell_footer_action){
            6, TERM_WHITE, true, view->history_label, "History"
        };
        if (view->blitz_enabled)
        {
            actions[count++] = (ui_browser_shell_footer_action){
                7, TERM_WHITE, true, view->blitz_label, "Blitz"
            };
        }
        actions[count++] = (ui_browser_shell_footer_action){
            8, TERM_WHITE, true, view->back_label, "Back"
        };
        (void)ui_browser_shell_add_footer_actions(panel, actions, count);
    }

    return true;
}

static bool metarun_adjust_blessing_threshold_information_scene(
    bool steamdeck, const char* accept_label, const char* back_label)
{
    const metarun_blessing_threshold_mode order[] = {
        METARUN_BLESSING_THRESHOLD_EASIER,
        METARUN_BLESSING_THRESHOLD_NORMAL,
        METARUN_BLESSING_THRESHOLD_HARDER
    };
    const char *labels[] = { "Easier", "Normal", "Harder" };
    const char *descs[] = {
        "If the game feels too hard, use this to earn blessings sooner.",
        "Default level.",
        "Pick this if you want fewer blessings by raising the threshold."
    };
    const int option_count = (int)N_ELEMENTS(order);

    metarun_blessing_threshold_mode current_mode =
        metarun_get_threshold_mode(&metar);
    int selection = 0;
    for (int i = 0; i < option_count; i++) {
        if (order[i] == current_mode) {
            selection = i;
            break;
        }
    }

    bool accepted = false;
    metarun_blessing_threshold_mode chosen_mode = current_mode;
    bool semantic_ok = true;
    bool first_present = true;
    const ui_browser_shell_button_key buttons[] = {
        { 1, '\r' },
        { 3, ESCAPE }
    };

    while (true) {
        app_ui_scene scene;
        app_ui_panel *panel;
        char subtitle[APP_UI_TEXT_MAX];
        int key;

        strnfmt(subtitle, sizeof(subtitle), "Current: %s",
            threshold_mode_name(current_mode));
        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "Blessing Threshold", TERM_SLATE, subtitle);
        if (!panel) {
            semantic_ok = false;
            break;
        }

        for (int i = 0; i < option_count; i++) {
            metarun_blessing_threshold_mode mode = order[i];
            u32b mode_threshold = runtype_threshold_for_mode(metar.type, mode);
            byte attr = (mode == METARUN_BLESSING_THRESHOLD_EASIER)
                ? TERM_L_GREEN
                : (mode == METARUN_BLESSING_THRESHOLD_HARDER)
                    ? TERM_ORANGE : TERM_WHITE;
            char key_buf[APP_UI_KEY_MAX];
            char meta[APP_UI_META_MAX];

            strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
            strnfmt(meta, sizeof(meta), "%lu pts",
                (unsigned long)mode_threshold);
            if (!app_ui_panel_add_row(panel, i, attr, true,
                    i == selection, key_buf, labels[i], meta))
            {
                semantic_ok = false;
                break;
            }
        }
        if (!semantic_ok)
            break;

        {
            metarun_blessing_threshold_mode mode = order[selection];
            u32b mode_threshold = runtype_threshold_for_mode(metar.type, mode);
            char buf[APP_UI_TEXT_MAX];

            app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
                "Selected Mode");
            if (!app_ui_panel_add_detail_line(panel, TERM_WHITE,
                    labels[selection]))
            {
                semantic_ok = false;
                break;
            }
            strnfmt(buf, sizeof(buf), "Requires %lu points per blessing.",
                (unsigned long)mode_threshold);
            if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, buf))
            {
                semantic_ok = false;
                break;
            }
            if (mode == current_mode
                && !app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
                    "Current setting."))
            {
                semantic_ok = false;
                break;
            }
            if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_SLATE,
                    descs[selection]))
            {
                semantic_ok = false;
                break;
            }
        }

        {
            const ui_browser_shell_footer_action actions[] = {
                { 1, TERM_L_BLUE, true,
                    steamdeck ? accept_label : "Enter", "Apply" },
                { 2, TERM_WHITE, true, "8/2", "Move" },
                { 3, TERM_WHITE, true,
                    steamdeck ? back_label : "Esc", "Cancel" }
            };

            (void)ui_browser_shell_add_footer_actions(panel, actions,
                N_ELEMENTS(actions));
        }

        if (!metarun_ui_present_scene(&scene, first_present)) {
            semantic_ok = false;
            break;
        }
        first_present = false;

        key = metarun_ui_wait_browser_key(NULL, option_count, &selection,
            NULL, 0, '\r', buttons, N_ELEMENTS(buttons));
        if (!key)
            continue;
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key())
            || (!steamdeck && (key == 'h' || key == 'H')))
        {
            metarun_ui_clear_pending_input();
            break;
        }
        if (key == '\r' || key == '\n'
            || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
        {
            accepted = true;
            chosen_mode = order[selection];
            metarun_ui_clear_pending_input();
            break;
        }
        if (key == '8' || key == 'k' || key == '-') {
            selection = (selection + option_count - 1) % option_count;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+') {
            selection = (selection + 1) % option_count;
            continue;
        }
        if (key >= 'a' && key < 'a' + option_count) {
            selection = key - 'a';
            continue;
        }
        if (key >= 'A' && key < 'A' + option_count) {
            selection = key - 'A';
            continue;
        }
    }

    if (accepted && chosen_mode != current_mode) {
        metarun_set_threshold_mode(&metar, chosen_mode);
        update_blessing_ledger(&metar);
        if (!sync_current_metarun_slot(false)) {
            log_warn("Threshold change: unable to sync metarun slot (idx=%d, max=%d)", current_run, metarun_max);
        }
        save_metaruns();
    }

    if (accepted) {
        char line1[APP_UI_TEXT_MAX];
        char line2[APP_UI_TEXT_MAX];
        const char *lines[2];
        byte attrs[2];
        int line_count = 0;

        if (chosen_mode != current_mode) {
            u32b new_threshold = metarun_threshold_value(&metar);
            strnfmt(line1, sizeof(line1), "Blessing threshold set to %s.",
                threshold_mode_name(chosen_mode));
            strnfmt(line2, sizeof(line2),
                "New requirement: %lu points per blessing.",
                (unsigned long)new_threshold);
            lines[line_count] = line1;
            attrs[line_count++] = TERM_L_GREEN;
            lines[line_count] = line2;
            attrs[line_count++] = TERM_WHITE;
        } else {
            lines[line_count] = "Blessing threshold remains unchanged.";
            attrs[line_count++] = TERM_L_DARK;
        }
        if (!metarun_ui_show_notice_modal("Blessing Threshold",
                TERM_YELLOW, lines, attrs, line_count, steamdeck,
                accept_label))
        {
            semantic_ok = false;
        }
    }

    return semantic_ok;
}

void print_metarun_stats(void)
{
    ui_information_scene_scope info_scope;
    app_ui_scene metarun_scene;
    metarun_stats_view_model view;
    bool first_present = true;
    bool use_information_scene = ui_information_scene_enter(&info_scope);

    if (!use_information_scene) {
        log_error("print_metarun_stats: semantic scene unavailable");
        return;
    }

metarun_redraw:
    refresh_current_metar_score();

    if (current_run < 0 || current_run >= metarun_max) {
        const char *lines[] = {
            "Error: No metarun data available.",
            "Please start a new game first."
        };
        const byte attrs[] = { TERM_RED, TERM_WHITE };

        (void)metarun_ui_show_notice_modal("Current Story Statistics",
            TERM_YELLOW, lines, attrs, (int)N_ELEMENTS(lines),
            steamdeck_controls_active(), "A");
        ui_information_scene_leave(&info_scope);
        return;
    }

    compute_blessing_pool();
    metarun_sanitize_major_blessing_bits(&metar);
    metarun_stats_prepare_view_model(&view);

    if (!metarun_build_stats_browser_scene(&metarun_scene, &view)
        || !metarun_ui_present_scene(&metarun_scene, first_present))
    {
        log_error("print_metarun_stats: failed to publish semantic scene");
        ui_information_scene_leave(&info_scope);
        return;
    }
    first_present = false;

    {
        static const ui_browser_shell_button_key buttons[] = {
            { 1, '\r' },
            { 2, 'b' },
            { 3, 'f' },
            { 4, 'c' },
            { 5, 'u' },
            { 6, 's' },
            { 7, 'x' },
            { 8, ESCAPE }
        };
        ui_browser_shell_command_map map;
        int key;
        bool steamdeck = view.steamdeck;

        ui_browser_shell_command_map_init(&map);
        map.button_keys = buttons;
        map.button_key_count = N_ELEMENTS(buttons);
        map.row_activate_key = '\0';

        key = ui_browser_shell_wait_key(&map, APP_INPUT_FLAG_REPEAT, NULL);
        if (!key)
            goto metarun_redraw;

        if (steamdeck) {
            int back_key = steamdeck_back_key();
            int confirm_key = steamdeck_confirm_key();
            int alt_key = steamdeck_alt_action_key();
            int secondary_key = steamdeck_secondary_key();

            if (key == back_key || key == confirm_key || key == '\r'
                || key == '\n')
            {
                ui_information_scene_leave_without_restore(&info_scope);
                return;
            } else if (key == alt_key) {
                key = 'b';
            } else if (key == secondary_key) {
                key = 's';
            }
        }

        if (key == 'b' || key == 'B') {
            if (!open_blessing_exchange_information_scene(view.steamdeck,
                    view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: blessing exchange scene failed");
            }
            goto metarun_redraw;
        } else if (key == 'c' || key == 'C') {
            metarun_choose_difficulty_menu(false);
            goto metarun_redraw;
        } else if (key == 'f' || key == 'F') {
            if (!metarun_adjust_blessing_threshold_information_scene(
                    view.steamdeck, view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: threshold scene failed");
            }
            goto metarun_redraw;
        } else if (key == 'u' || key == 'U') {
            if (!metarun_show_active_effects_information_scene(
                    view.steamdeck, view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: active effects scene failed");
            }
            goto metarun_redraw;
        } else if (key == 's' || key == 'S') {
            if (!metarun_list_history_information_scene(view.steamdeck,
                    view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: history scene failed");
            }
            goto metarun_redraw;
        } else if (key == 't' || key == 'T') {
            if (!metarun_show_completed_quests_information_scene(
                    view.steamdeck, view.continue_label, view.back_label))
            {
                log_error("print_metarun_stats: completed quests scene failed");
            }
            goto metarun_redraw;
        } else if ((key == 'x' || key == 'X') && view.blitz_enabled) {
            ui_information_scene_leave_without_restore(&info_scope);
            run_mode_set_pending(RUN_MODE_BLITZ);
            run_mode_set_current(RUN_MODE_BLITZ);
            return;
        }
    }

    ui_information_scene_leave_without_restore(&info_scope);
}
