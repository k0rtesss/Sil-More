/* File: quest-varda.c */

#include "angband.h"
#include "metarun.h"
#include "quest/quest.h"
#include "quest/quest-internal.h"
#include "log/log.h"
#include "ui/ui-information-scene.h"

void ensure_varda_ungoliant_active(void)
{
    byte state = quest_get_state(QUEST_ID_VARDA_UNGOLIANT);
    if (state >= QUEST_STATE_REWARDED) return;

    bool oath_active = (p_ptr->oath_type == OATH_LIGHT && !oath_invalid(OATH_LIGHT));
    bool unlocked = metarun_quest_completion_count(METARUN_QUEST_VARDA_SHADOW) > 0 ||
        quest_get_state(QUEST_ID_VARDA_SHADOW) >= QUEST_STATE_REWARDED ||
        metarun_quest_completion_count(METARUN_QUEST_VARDA) > 0 ||
        p_ptr->varda_quest >= VARDA_QUEST_REWARDED;

    if (!oath_active || !unlocked)
    {
        if (state != QUEST_STATE_NOT_STARTED) quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_NOT_STARTED);
        return;
    }

    if (state == QUEST_STATE_NOT_STARTED)
    {
        quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_ACTIVE);
    }

    if (quest_get_state(QUEST_ID_VARDA_UNGOLIANT) == QUEST_STATE_ACTIVE &&
        r_info[R_IDX_UNGOLIANT].max_num == 0)
    {
        quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_REWARDED);
    }
}

/*
 * Filter radiant artefacts that make sense as Varda rewards.
 */
static bool artefact_is_radiant_candidate(artefact_type* a_ptr)
{
    if (!a_ptr) return false;
    if (!(a_ptr->flags2 & (TR2_LIGHT | TR2_RADIANCE))) return false;
    if (a_ptr->flags2 & TR2_DARKNESS) return false;
    if (a_ptr->flags4 & TR4_UNLIGHT) return false;
    if (a_ptr->flags3 & TR3_LIGHT_CURSE) return false;
    return true;
}

static int build_varda_reward_options(int* choices, int max_choices)
{
    if (!choices || max_choices <= 0 || !z_info) return 0;

    /* Ensure reservation table exists so we can filter reserved artefacts */
    if (!valar_reserved_artifacts && z_info && z_info->art_max > 0) {
        valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
        for (int j = 0; j < z_info->art_max; j++) {
            valar_reserved_artifacts[j] = false;
        }
        log_trace("Varda reward: initialized valar_reserved_artifacts for %d artefacts", z_info->art_max);
    }

    int candidates[256];
    int candidate_count = 0;

    /* Early-light reward: bias toward items not far above current depth */
    int depth_cap = 25;
    if (p_ptr) {
        depth_cap = MIN(p_ptr->depth + 6, depth_cap);
    }
    depth_cap = MAX(depth_cap, 15); /* keep at least mid-depth options */

    for (int i = 1; i < z_info->art_max && candidate_count < (int)N_ELEMENTS(candidates); i++) {
        artefact_type* a_ptr = &a_info[i];

        if (!a_ptr || a_ptr->tval == 0) continue;
        if (a_ptr->name[0] == '\0') continue;
        if (!artefact_is_radiant_candidate(a_ptr)) continue;
        if (a_ptr->cur_num != 0) continue; /* already created */
        if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;
        if (a_ptr->level > depth_cap) continue;

        candidates[candidate_count++] = i;
    }

    /* Relax depth cap if we still need more options */
    if (candidate_count < max_choices) {
        for (int i = 1; i < z_info->art_max && candidate_count < (int)N_ELEMENTS(candidates); i++) {
            artefact_type* a_ptr = &a_info[i];

            if (!a_ptr || a_ptr->tval == 0) continue;
            if (a_ptr->name[0] == '\0') continue;
            if (!artefact_is_radiant_candidate(a_ptr)) continue;
            if (a_ptr->cur_num != 0) continue;
            if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;

            candidates[candidate_count++] = i;
        }
    }

    if (candidate_count == 0) return 0;

    /* Shuffle candidates */
    for (int i = candidate_count - 1; i > 0; i--) {
        int swap_idx = rand_int(i + 1);
        int tmp = candidates[i];
        candidates[i] = candidates[swap_idx];
        candidates[swap_idx] = tmp;
    }

    int final_count = MIN(max_choices, candidate_count);
    for (int i = 0; i < final_count; i++) {
        choices[i] = candidates[i];
    }
    return final_count;
}

static void describe_varda_choice(int a_idx, char* buf, size_t buf_len)
{
    artefact_type* a_ptr = &a_info[a_idx];
    object_type temp_obj;
    object_wipe(&temp_obj);

    s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
    if (k_idx > 0) {
        object_prep(&temp_obj, k_idx);
        temp_obj.name1 = a_idx;
        temp_obj.ident |= IDENT_KNOWN;
        object_desc(buf, buf_len, &temp_obj, true, 0);
    } else if (a_ptr->name[0] != '\0') {
        SDL_strlcpy(buf, a_ptr->name, buf_len);
    } else {
        SDL_strlcpy(buf, "a radiant artefact", buf_len);
    }
}

static void varda_reward_scene_add_wrapped_text(app_ui_panel* panel, byte attr,
    cptr text, size_t wrap_chars)
{
    const char* cursor = text;

    if (!panel || !text || !text[0])
        return;
    if (wrap_chars < 8)
        wrap_chars = 8;

    while (*cursor && panel->body_line_count < APP_UI_BODY_LINE_MAX)
    {
        const char* line_start;
        const char* line_end;
        const char* last_space = NULL;
        char line[APP_UI_TEXT_MAX];
        size_t line_len;

        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n')
            cursor++;
        if (!*cursor)
            break;

        line_start = cursor;
        while (*cursor && *cursor != '\n'
            && (size_t)(cursor - line_start) < wrap_chars)
        {
            if (*cursor == ' ' || *cursor == '\t')
                last_space = cursor;
            cursor++;
        }

        if (*cursor == '\n')
        {
            line_end = cursor;
            cursor++;
        }
        else if (*cursor && last_space && last_space > line_start)
        {
            line_end = last_space;
            cursor = last_space + 1;
        }
        else
        {
            line_end = cursor;
        }

        while (line_end > line_start
            && (line_end[-1] == ' ' || line_end[-1] == '\t'))
        {
            line_end--;
        }

        line_len = (size_t)(line_end - line_start);
        if (line_len == 0)
            continue;
        if (line_len >= sizeof(line))
            line_len = sizeof(line) - 1;

        memcpy(line, line_start, line_len);
        line[line_len] = '\0';
        (void)app_ui_panel_add_body_line(panel, attr, line);
    }
}

static bool varda_reward_choice_scene_build(app_ui_scene* scene,
    const int* choices, int choice_count, cptr* completion_texts, int text_count,
    int selection)
{
    app_ui_panel* panel;
    int i;

    if (!scene || !choices || choice_count <= 0)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->accent_attr = TERM_L_GREEN;
    app_ui_panel_set_widths(panel, 980, 1700);
    app_ui_panel_set_title(panel, TERM_L_GREEN, "Starlight Triumph");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "Choose your radiant gift.");

    for (i = 0; i < text_count; i++)
    {
        if (!completion_texts[i] || !completion_texts[i][0])
        {
            (void)app_ui_panel_add_body_line(panel, TERM_WHITE, "");
            continue;
        }

        varda_reward_scene_add_wrapped_text(panel, TERM_WHITE,
            completion_texts[i], 80);
    }

    (void)app_ui_panel_add_body_line(panel, TERM_L_BLUE,
        "Choose your radiant gift:");

    for (i = 0; i < choice_count; i++)
    {
        char key[APP_UI_KEY_MAX];
        char label[APP_UI_LABEL_MAX];
        char desc[120];
        bool highlighted = (i == selection);

        describe_varda_choice(choices[i], desc, sizeof(desc));
        strnfmt(key, sizeof(key), "%c", 'a' + i);
        strnfmt(label, sizeof(label), "%s", desc);
        if (!app_ui_panel_add_row(panel, (s16b)i,
                highlighted ? TERM_YELLOW : TERM_L_WHITE, true, highlighted,
                key, label, NULL))
        {
            return false;
        }
    }

    {
        char desc[120];

        describe_varda_choice(choices[selection], desc, sizeof(desc));
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected");
        (void)app_ui_panel_add_detail_line(panel, TERM_L_WHITE, desc);
        (void)app_ui_panel_add_detail_line(panel, TERM_SLATE,
            "Press x to inspect the highlighted reward.");
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Enter", "Accept");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "x", "Inspect");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "Esc", "Cancel");
    return true;
}

/*
 * Display Varda's reward selection in a scrollable menu integrated with quest completion text.
 * Returns the selected artefact index, or 0 if cancelled.
 */
static int prompt_varda_reward_choice_menu(const int* choices, int choice_count, cptr* completion_texts, int text_count)
{
    ui_information_scene_scope scope;
    int selection = 0;
    int selected_artifact = 0;

    if (!ui_information_scene_enter(&scope))
        return 0;

    while (true) {
        app_ui_scene scene;
        int key;
        int dir;

        if (!varda_reward_choice_scene_build(&scene, choices, choice_count,
                completion_texts, text_count, selection)
            || !ui_information_scene_present_ui(&scene))
        {
            ui_information_scene_leave(&scope);
            return 0;
        }

        key = ui_information_scene_wait_key();
        dir = target_dir((char)key);
        if (dir == 8) {
            selection = (selection + choice_count - 1) % choice_count;
            continue;
        }
        if (dir == 2) {
            selection = (selection + 1) % choice_count;
            continue;
        }
        if (key == '\r' || key == '\n' || key == ' ' || key == '6') {
            selected_artifact = choices[selection];
            break;
        }
        if (key == 'x' || key == 'X' || key == '?') {
            ui_information_scene_leave(&scope);
            desc_art_fake(choices[selection]);
            if (!ui_information_scene_enter(&scope))
                return 0;
            continue;
        }
        if (key >= 'a' && key < 'a' + choice_count) {
            selected_artifact = choices[key - 'a'];
            break;
        }
        if (key >= 'A' && key < 'A' + choice_count) {
            selected_artifact = choices[key - 'A'];
            break;
        }
        if (key == ESCAPE) {
            selected_artifact = 0;
            break;
        }
    }

    ui_information_scene_leave(&scope);
    return selected_artifact;
}

static bool grant_varda_reward(int quest_id, cptr* completion_texts, int completion_count)
{
    int choices[3] = {0};
    int available = build_varda_reward_options(choices, (int)N_ELEMENTS(choices));
    if (available <= 0) {
        msg_print("Varda has no radiant artefacts left to offer.");
        log_trace("Varda reward: no available radiant artefacts");
        return false;
    }

    /* Use the integrated scrollable menu with completion text */
    int selected = prompt_varda_reward_choice_menu(choices, available, completion_texts, completion_count);
    if (selected <= 0) {
        msg_print("The starlight gifts wait until you are ready to choose.");
        return false;
    }

    create_chosen_artefact(selected, p_ptr->py, p_ptr->px, true);
    msg_print("Starlight gathers at your feet, coalescing into a shining relic.");
    p_ptr->quest_reserved[0] = 1;
    if (quest_id == QUEST_ID_VARDA_SHADOW) {
        quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_REWARDED);
        p_ptr->varda_shadow_ready = 0;
        p_ptr->varda_shadow_placed = 1;
        p_ptr->varda_shadow_restricted = 0;
        do_cmd_note("Varda blessed me with a radiant artefact after Belegwath's fall.", p_ptr->depth);
    } else {
        p_ptr->varda_quest = VARDA_QUEST_REWARDED;
        p_ptr->varda_vault_ready = 0;
        p_ptr->varda_vault_placed = 1;
        do_cmd_note("Varda blessed me with a radiant artefact and the Oath of Light.", p_ptr->depth);
    }

    grant_followup_quest_rewards(quest_id);

    return true;
}

static void varda_make_light_pool(int y, int x)
{
    for (int ny = y - 1; ny <= y + 1; ny++) {
        for (int nx = x - 1; nx <= x + 1; nx++) {
            if (!in_bounds(ny, nx)) continue;
            if (cave_info[ny][nx] & CAVE_ICKY) continue;
            if (cave_feat[ny][nx] != FEAT_FLOOR && cave_feat[ny][nx] != FEAT_RAGE_FLOOR
                && cave_feat[ny][nx] != FEAT_SUNLIGHT) continue;
            cave_set_feat(ny, nx, FEAT_SUNLIGHT);
        }
    }
}

static void try_place_varda_near_player(void)
{
    /* Avoid double-spawning */
    for (int i = 1; i < mon_max; i++) {
        monster_type* m_ptr = &mon_list[i];
        if (m_ptr->r_idx == R_IDX_VARDA) {
            log_trace("Varda reward: quest giver already exists on level");
            return;
        }
    }

    for (int y = p_ptr->py - 2; y <= p_ptr->py + 2; y++) {
        for (int x = p_ptr->px - 2; x <= p_ptr->px + 2; x++) {
            if (!in_bounds(y, x)) continue;
            if (y == p_ptr->py && x == p_ptr->px) continue;
            if (distance(p_ptr->py, p_ptr->px, y, x) < 2) continue;
            if (!cave_floor_bold(y, x)) continue;
            if (cave_m_idx[y][x] != 0) continue;

            if (place_monster_one(y, x, R_IDX_VARDA, true, true, NULL)) {
                varda_make_light_pool(y, x);
                p_ptr->varda_level = p_ptr->depth;
                log_trace("Varda reward: placed quest giver at (%d,%d)", y, x);
                return;
            }
        }
    }

    /* Mark this depth as attempted even if placement failed, to avoid infinite spawn loops */
    p_ptr->varda_level = p_ptr->depth;
    log_trace("Varda reward: failed to place quest giver near player (no valid space), will retry on new depth");
}

void check_varda_quest_completion(int r_idx)
{
    if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE && r_idx == R_IDX_DURUIN) {
        p_ptr->varda_quest = VARDA_QUEST_SUCCESS;
        p_ptr->varda_vault_ready = 0;
        p_ptr->varda_level = p_ptr->depth;
        msg_print("Duruin falls. The Bastion's shadows unravel under starlight!");
        try_place_varda_near_player();
    }

    if (quest_get_state(QUEST_ID_VARDA_SHADOW) == QUEST_STATE_ACTIVE && r_idx == R_IDX_BELEGWATH) {
        quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_SUCCESS);
        p_ptr->varda_shadow_ready = 0;
        p_ptr->varda_shadow_restricted = 0;
        p_ptr->varda_level = p_ptr->depth;
        msg_print("Belegwath falls. The Shadow Bastion breaks beneath returning starlight!");
        try_place_varda_near_player();
    }

    if (quest_get_state(QUEST_ID_VARDA_UNGOLIANT) == QUEST_STATE_ACTIVE && r_idx == R_IDX_UNGOLIANT) {
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_VARDA_UNGOLIANT, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_VARDA_UNGOLIANT, completion_texts, &completion_count, true);

        if (completion_texts && completion_count > 0) {
            quest_typewriter_menu("Varda, Gloomweaver's Doom", completion_texts, completion_count, TERM_WHITE, TERM_L_BLUE);
            free_quest_texts(completion_texts, completion_count);
        } else {
            cptr fallback[] = {
                "Ungoliant's unlight gutters and dies before Varda's stars.",
                "The Queen of the Stars marks the doom complete."
            };
            quest_typewriter_menu("Varda, Gloomweaver's Doom", fallback, N_ELEMENTS(fallback), TERM_WHITE, TERM_L_BLUE);
        }

        quest_set_state(QUEST_ID_VARDA_UNGOLIANT, QUEST_STATE_REWARDED);
        grant_followup_quest_rewards(QUEST_ID_VARDA_UNGOLIANT);
    }
}

void varda_quest_interaction(void)
{
    static s32b last_interaction_turn = -1;
    byte shadow_state;
    if (last_interaction_turn == turn) return;
    last_interaction_turn = turn;

    shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);

    if (shadow_state == QUEST_STATE_GIVER_PRESENT) {
        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(QUEST_ID_VARDA_SHADOW, &text_count);
        init_texts = prepend_repeat_context(QUEST_ID_VARDA_SHADOW, init_texts, &text_count, false);

        quest_set_state(QUEST_ID_VARDA_SHADOW, QUEST_STATE_ACTIVE);
        p_ptr->quest_reserved[0] = 1;
        p_ptr->varda_shadow_restricted = 1;
        remove_quest_giver(R_IDX_VARDA);

        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Varda, Shadow's Bastion", init_texts, text_count, TERM_WHITE, TERM_L_BLUE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Varda's gaze is clear even in the deep dark.",
                "\"Go down and break Belegwath's Shadow Bastion beneath the stars.\""
            };
            quest_typewriter_menu("Varda, Shadow's Bastion", fallback, N_ELEMENTS(fallback), TERM_WHITE, TERM_L_BLUE);
        }

        do_cmd_note("Varda sent me to destroy Belegwath in his Shadow Bastion.", p_ptr->depth);
        return;
    }

    if (shadow_state == QUEST_STATE_ACTIVE) {
        msg_print("Varda's whisper: \"Belegwath's bastion waits beyond the fifteenth delving.\"");
        return;
    }

    if (shadow_state == QUEST_STATE_SUCCESS) {
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_VARDA_SHADOW, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_VARDA_SHADOW, completion_texts, &completion_count, true);
        cptr fallback[] = {
            "Varda's voice rings clear through the shattered shadow:",
            "\"Choose your radiant gift anew, and carry the stars deeper still.\""
        };
        cptr* texts_to_use = (completion_texts && completion_count > 0) ? completion_texts : fallback;
        int text_count = (completion_texts && completion_count > 0) ? completion_count : 2;
        bool rewarded = grant_varda_reward(QUEST_ID_VARDA_SHADOW, texts_to_use, text_count);

        if (completion_texts) {
            free_quest_texts(completion_texts, completion_count);
        }
        if (rewarded) {
            remove_quest_giver(R_IDX_VARDA);
        }
        return;
    }

    if (shadow_state == QUEST_STATE_REWARDED) {
        msg_print("Varda's deeper blessing still shines in your wake.");
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_GIVER_PRESENT) {
        log_trace("Varda quest: accepting quest");
        p_ptr->varda_quest = VARDA_QUEST_ACTIVE;
        p_ptr->quest_reserved[0] = 1;
        p_ptr->varda_level = p_ptr->depth;

        /* Remove quest giver for roulette quests */
        remove_quest_giver(R_IDX_VARDA);

        int text_count = 0;
        cptr* init_texts = extract_quest_init_texts(QUEST_ID_VARDA, &text_count);
        init_texts = prepend_repeat_context(QUEST_ID_VARDA, init_texts, &text_count, false);
        if (init_texts && text_count > 0) {
            quest_typewriter_menu("Varda, Lady of the Stars", init_texts, text_count, TERM_WHITE, TERM_L_BLUE);
            free_quest_texts(init_texts, text_count);
        } else {
            cptr fallback[] = {
                "Varda's voice is clear as starlight:",
                "\"Seek Duruin's bastion of shadow and break it open to the Sun.\""
            };
            quest_typewriter_menu("Varda, Lady of the Stars", fallback, 2, TERM_WHITE, TERM_L_BLUE);
        }

        do_cmd_note("Varda sent me to destroy Duruin and cleanse his bastion.", p_ptr->depth);
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE) {
        msg_print("Varda's whisper: \"Find Duruin's bastion at the turning of the deeps, where your journey nears its halfway point.\"");
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_SUCCESS) {
        log_trace("Varda quest: delivering reward");
        int completion_count = 0;
        cptr* completion_texts = extract_quest_completion_texts(QUEST_ID_VARDA, &completion_count);
        completion_texts = prepend_repeat_context(QUEST_ID_VARDA, completion_texts, &completion_count, true);
        
        /* Fallback texts if quest data not available */
        cptr fallback[] = {
            "Varda inclines her head in silent approval:",
            "\"The stolen light is free. Choose your blessing.\""
        };
        cptr* texts_to_use = (completion_texts && completion_count > 0) ? completion_texts : fallback;
        int text_count = (completion_texts && completion_count > 0) ? completion_count : 2;
        
        /* Display quest completion and reward selection in one integrated menu */
        bool rewarded = grant_varda_reward(QUEST_ID_VARDA, texts_to_use, text_count);
        
        if (completion_texts) {
            free_quest_texts(completion_texts, completion_count);
        }
        
        if (rewarded) {
            remove_quest_giver(R_IDX_VARDA);
        }
        return;
    }

    if (p_ptr->varda_quest == VARDA_QUEST_REWARDED) {
        msg_print("Varda's blessing still follows the path you walk.");
    }
}

void check_varda_quest_interaction(void)
{
    int i, y, x;
    byte shadow_state = quest_get_state(QUEST_ID_VARDA_SHADOW);

    if ((p_ptr->varda_quest < VARDA_QUEST_GIVER_PRESENT || p_ptr->varda_quest > VARDA_QUEST_SUCCESS) &&
        shadow_state != QUEST_STATE_GIVER_PRESENT &&
        shadow_state != QUEST_STATE_ACTIVE &&
        shadow_state != QUEST_STATE_SUCCESS) return;

    for (i = 1; i < 9; i++) {
        y = p_ptr->py + ddy[i];
        x = p_ptr->px + ddx[i];

        if (!in_bounds(y, x)) continue;
        if (cave_m_idx[y][x] <= 0) continue;

        int m_idx = cave_m_idx[y][x];
        if (m_idx >= mon_max) continue;

        monster_type* m_ptr = &mon_list[m_idx];
        if (m_ptr->r_idx == R_IDX_VARDA) {
            varda_quest_interaction();
            return;
        }
    }

    /* If Varda should be present for a reward but isn't adjacent, try to place her */
    /* Only attempt placement once per depth to avoid infinite spawn loops */
    if ((p_ptr->varda_quest == VARDA_QUEST_SUCCESS || shadow_state == QUEST_STATE_SUCCESS) &&
        p_ptr->varda_level != p_ptr->depth) {
        log_trace("Varda quest: success state without nearby quest giver - attempting to place Varda");
        try_place_varda_near_player();
    }
}
