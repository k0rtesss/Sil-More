/* File: quest-ui.c */

#include "angband.h"
#include "externs.h"
#include "metarun.h"
#include "quest/quest.h"
#include "ui/ui-information-scene.h"
#include "log/log.h"

static cptr get_quest_title(int quest_idx)
{
    log_trace("QUEST TITLE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);
    
    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST TITLE: Invalid bounds check, returning Unknown Quest");
        return "Unknown Quest";
    }
    
    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST TITLE: q_ptr is NULL, returning Unknown Quest");
        return "Unknown Quest";
    }
    
    if (q_ptr->title_text && q_text) {
        log_trace("QUEST TITLE: Using title_text");
        return q_text + q_ptr->title_text;
    }
    
    /* Fallback to quest name */
    if (q_ptr->name && quest_name_text) {
        log_trace("QUEST TITLE: Using quest name fallback");
        return quest_name_text + q_ptr->name;
    }
    
    log_trace("QUEST TITLE: No valid text found, returning Unknown Quest");
    return "Unknown Quest";
}

/*
 * Get quest challenge description from quest data
 */
static cptr get_quest_challenge(int quest_idx)
{
    log_trace("QUEST CHALLENGE: quest_idx=%d, z_info->quest_max=%d", quest_idx, z_info ? z_info->quest_max : -1);
    
    if (!z_info || quest_idx <= 0 || quest_idx >= z_info->quest_max || !quest_info) {
        log_trace("QUEST CHALLENGE: Invalid bounds check, returning Unknown challenge");
        return "Unknown challenge";
    }
    
    quest_type* q_ptr = &quest_info[quest_idx];
    if (!q_ptr) {
        log_trace("QUEST CHALLENGE: q_ptr is NULL, returning Unknown challenge");
        return "Unknown challenge";
    }
    
    if (q_ptr->challenge_text && q_text) {
        log_trace("QUEST CHALLENGE: Using challenge_text");
        return q_text + q_ptr->challenge_text;
    }
    
    log_trace("QUEST CHALLENGE: No valid text found, returning default");
    return "Face the unknown challenge";
}

/*
 * Get oath name from oath ID using oath_info data
 */
static cptr get_oath_name_from_id(byte oath_id)
{
    if (oath_id <= 0 || oath_id >= z_info->oath_max) return "No oath";
    
    oath_type* o_ptr = &oath_info[oath_id];
    if (o_ptr->name) {
        return oath_name_text + o_ptr->name;
    }
    
    /* Fallback to hardcoded names if oath_info not loaded */
    switch(oath_id) {
        case 0: return "No oath";
        case 1: return "Mercy oath";
        case 2: return "Silence oath";
        case 3: return "Iron oath";  
        case 4: return "Smith oath";
        default: return "Unknown oath";
    }
}

static cptr process_quest_placeholders(cptr text, int quest_idx);
static cptr get_quest_reward_text(int quest_idx);

static app_ui_panel* quest_scene_begin_document(app_ui_scene* scene)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;
    return panel;
}

static bool quest_scene_add_text(app_ui_scene* scene, app_ui_panel* panel,
    s16b row, s16b col, byte color, cptr text)
{
    if (!text || !text[0])
        return true;

    return app_ui_panel_add_document_text(scene, panel, row, col, color, text);
}

static void quest_scene_add_line(app_ui_scene* scene, app_ui_panel* panel,
    int col, int* row, cptr text, byte color)
{
    if (!scene || !panel || !row || !text || !text[0])
        return;

    (void)quest_scene_add_text(scene, panel, (s16b)(*row), (s16b)col, color,
        text);
    (*row)++;
}

static void quest_scene_display_wrapped_text(app_ui_scene* scene,
    app_ui_panel* panel, int col, int* row, cptr text, byte color,
    int max_width)
{
    char line_buf[256];
    int line_pos = 0;
    int effective_width = max_width - col - 4;
    int text_len;
    int word_start = 0;
    int i = 0;
    int loop_count = 0;

    if (!scene || !panel || !row || !text)
        return;

    text_len = strlen(text);
    if (effective_width < 20)
        effective_width = 20;

    line_buf[0] = '\0';

    while (i <= text_len)
    {
        loop_count++;
        if (loop_count > 1000)
        {
            log_warn("quest_scene_display_wrapped_text: safety break");
            break;
        }

        if (i == text_len || text[i] == ' ')
        {
            int word_len = i - word_start;
            char word[128];

            if (word_len > 0 && word_len < (int)sizeof(word))
            {
                int copy_len = word_len;

                if (copy_len >= (int)sizeof(word))
                    copy_len = (int)sizeof(word) - 1;
                for (int j = 0; j < copy_len; j++)
                    word[j] = text[word_start + j];
                word[copy_len] = '\0';

                if (line_pos + (line_pos > 0 ? 1 : 0) + copy_len
                    > effective_width && line_pos > 0)
                {
                    (void)quest_scene_add_text(scene, panel, (s16b)(*row),
                        (s16b)(col + 2), color, line_buf);
                    (*row)++;

                    if (copy_len > effective_width)
                    {
                        int word_pos = 0;

                        while (word_pos < copy_len)
                        {
                            int chunk_len = effective_width;
                            char chunk[256];
                            int k;

                            if (word_pos + chunk_len > copy_len)
                                chunk_len = copy_len - word_pos;

                            for (k = 0; k < chunk_len
                                && word_pos + k < copy_len; k++)
                            {
                                chunk[k] = word[word_pos + k];
                            }
                            chunk[k] = '\0';

                            (void)quest_scene_add_text(scene, panel,
                                (s16b)(*row), (s16b)(col + 2), color, chunk);
                            (*row)++;
                            word_pos += chunk_len;
                        }

                        line_buf[0] = '\0';
                        line_pos = 0;
                    }
                    else
                    {
                        SDL_strlcpy(line_buf, word, sizeof(line_buf));
                        line_pos = copy_len;
                    }
                }
                else
                {
                    if (line_pos > 0)
                    {
                        SDL_strlcat(line_buf, " ", sizeof(line_buf));
                        line_pos++;
                    }
                    SDL_strlcat(line_buf, word, sizeof(line_buf));
                    line_pos += copy_len;
                }
            }

            while (i < text_len && text[i] == ' ')
                i++;
            word_start = i;
        }
        else
        {
            i++;
        }
    }

    if (line_pos > 0)
    {
        (void)quest_scene_add_text(scene, panel, (s16b)(*row),
            (s16b)(col + 2), color, line_buf);
        (*row)++;
    }
}

static void quest_scene_add_previous_completion(app_ui_scene* scene,
    app_ui_panel* panel, int col, int* row, int quest_id,
    int metarun_quest_id, int current_state, int rewarded_state,
    int quest_info_index, int wid,
    bool* has_previous_completions)
{
    int completed = metarun_quest_completion_count(metarun_quest_id);

    if (completed <= 0 || current_state == rewarded_state)
        return;

    if (!*has_previous_completions)
    {
        quest_scene_add_line(scene, panel, col, row,
            "Previously Completed in Metarun:", TERM_L_DARK);
        *has_previous_completions = true;
    }

    {
        cptr quest_title = get_quest_title(quest_id);
        cptr oath_name = get_oath_name_from_id(quest_info[quest_info_index].oath_id);
        char status_text[150];

        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)",
            quest_title, oath_name, completed);
        quest_scene_display_wrapped_text(scene, panel, col, row, status_text,
            TERM_SLATE, wid);
    }
}

static bool do_cmd_quest_status_information_scene(void)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    app_ui_panel* panel;
    char buf[128];
    int row = 1;
    int col = 2;
    bool any_quests = false;
    bool has_previous_completions = false;
    int wid;
    int hgt;

    if (!ui_information_scene_enter(&scope))
        return false;

    Term_get_size(&wid, &hgt);
    panel = quest_scene_begin_document(&scene);
    if (!panel)
    {
        ui_information_scene_leave(&scope);
        return false;
    }

    quest_scene_add_line(&scene, panel, col, &row, "=== Quest Status ===",
        TERM_YELLOW);
    row++;

    if (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED)
    {
        any_quests = true;
        quest_scene_add_line(&scene, panel, col, &row,
            get_quest_title(QUEST_ID_TULKAS), TERM_YELLOW);

        switch (p_ptr->tulkas_quest)
        {
        case TULKAS_QUEST_GIVER_PRESENT:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Available - Tulkas awaits", TERM_L_BLUE);
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                process_quest_placeholders(get_quest_challenge(QUEST_ID_TULKAS),
                    QUEST_ID_TULKAS), TERM_SLATE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_TULKAS));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case TULKAS_QUEST_ACTIVE:
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                process_quest_placeholders(get_quest_challenge(QUEST_ID_TULKAS),
                    QUEST_ID_TULKAS), TERM_WHITE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_TULKAS));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case TULKAS_QUEST_COMPLETE:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Complete - Return for reward", TERM_L_GREEN);
            break;

        case TULKAS_QUEST_REWARDED:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Completed by this character", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s received",
                get_quest_reward_text(QUEST_ID_TULKAS));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;
        }
        row++;
    }

    if (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED)
    {
        any_quests = true;
        quest_scene_add_line(&scene, panel, col, &row,
            get_quest_title(QUEST_ID_AULE), TERM_YELLOW);

        switch (p_ptr->aule_quest)
        {
        case AULE_QUEST_FORGE_PRESENT:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Available - Aule awaits", TERM_L_BLUE);
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                get_quest_challenge(QUEST_ID_AULE), TERM_SLATE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_AULE));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case AULE_QUEST_ACTIVE:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Active - Seek the forge-halls", TERM_WHITE);
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                get_quest_challenge(QUEST_ID_AULE), TERM_SLATE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_AULE));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case AULE_QUEST_SUCCESS:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Complete - Return for reward", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_AULE));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case AULE_QUEST_REWARDED:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Completed by this character", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s received",
                get_quest_reward_text(QUEST_ID_AULE));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;
        }
        row++;
    }

    if (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED)
    {
        any_quests = true;
        quest_scene_add_line(&scene, panel, col, &row,
            get_quest_title(QUEST_ID_MANDOS), TERM_YELLOW);

        switch (p_ptr->mandos_quest)
        {
        case MANDOS_QUEST_GIVER_PRESENT:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Available - Mandos waits beyond death", TERM_L_BLUE);
            quest_scene_add_line(&scene, panel, col + 2, &row,
                get_quest_challenge(QUEST_ID_MANDOS), TERM_SLATE);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_MANDOS));
            quest_scene_add_line(&scene, panel, col + 2, &row, buf,
                TERM_SLATE);
            break;

        case MANDOS_QUEST_ACTIVE:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Active - Escape the houses of waiting", TERM_WHITE);
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                get_quest_challenge(QUEST_ID_MANDOS), TERM_SLATE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_MANDOS));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case MANDOS_QUEST_SUCCESS:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Complete - Claim Mandos's favour", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_MANDOS));
            quest_scene_add_line(&scene, panel, col + 2, &row, buf,
                TERM_SLATE);
            break;

        case MANDOS_QUEST_REWARDED:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Completed by this character", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s received",
                get_quest_reward_text(QUEST_ID_MANDOS));
            quest_scene_add_line(&scene, panel, col + 2, &row, buf,
                TERM_SLATE);
            break;
        }
        row++;
    }

    if (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED)
    {
        any_quests = true;
        quest_scene_add_line(&scene, panel, col, &row,
            get_quest_title(QUEST_ID_NIENA), TERM_YELLOW);

        switch (p_ptr->niena_quest)
        {
        case NIENA_QUEST_GIVER_PRESENT:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Available - Niena offers mercy", TERM_L_BLUE);
            quest_scene_add_line(&scene, panel, col + 2, &row,
                get_quest_challenge(QUEST_ID_NIENA), TERM_SLATE);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_NIENA));
            quest_scene_add_line(&scene, panel, col + 2, &row, buf,
                TERM_SLATE);
            break;

        case NIENA_QUEST_ACTIVE:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Active - Walk the path of mercy", TERM_WHITE);
            strnfmt(buf, sizeof(buf), "Monsters seen: %d  killed: %d",
                p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
            quest_scene_add_line(&scene, panel, col + 4, &row, buf,
                TERM_SLATE);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_NIENA));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case NIENA_QUEST_SUCCESS:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Complete - Claim Niena's grace", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_NIENA));
            quest_scene_add_line(&scene, panel, col + 2, &row, buf,
                TERM_SLATE);
            break;

        case NIENA_QUEST_REWARDED:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Completed by this character", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s received",
                get_quest_reward_text(QUEST_ID_NIENA));
            quest_scene_add_line(&scene, panel, col + 2, &row, buf,
                TERM_SLATE);
            break;

        case NIENA_QUEST_FAILED:
            strnfmt(buf, sizeof(buf), "Failed: %d seen, %d killed",
                p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
            quest_scene_add_line(&scene, panel, col + 2, &row, buf, TERM_RED);
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "You took a life and lost Niena's mercy.", TERM_SLATE);
            break;
        }
        row++;
    }

    if (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED)
    {
        any_quests = true;
        quest_scene_add_line(&scene, panel, col, &row,
            get_quest_title(QUEST_ID_OROME), TERM_YELLOW);

        switch (p_ptr->orome_quest)
        {
        case OROME_QUEST_GIVER_PRESENT:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Available - Orome awaits", TERM_L_BLUE);
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                get_quest_challenge(QUEST_ID_OROME), TERM_SLATE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_OROME));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case OROME_QUEST_ACTIVE:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Active: Hunt the fell kindreds", TERM_WHITE);
            strnfmt(buf, sizeof(buf), "Wolves killed: %d/100",
                p_ptr->orome_wolves_killed);
            quest_scene_display_wrapped_text(&scene, panel, col + 2, &row, buf,
                p_ptr->orome_wolves_killed >= 100 ? TERM_L_GREEN : TERM_SLATE,
                wid);
            strnfmt(buf, sizeof(buf), "Spiders killed: %d/80",
                p_ptr->orome_spiders_killed);
            quest_scene_display_wrapped_text(&scene, panel, col + 2, &row, buf,
                p_ptr->orome_spiders_killed >= 80 ? TERM_L_GREEN : TERM_SLATE,
                wid);
            strnfmt(buf, sizeof(buf), "Serpents killed: %d/60",
                p_ptr->orome_serpents_killed);
            quest_scene_display_wrapped_text(&scene, panel, col + 2, &row, buf,
                p_ptr->orome_serpents_killed >= 60 ? TERM_L_GREEN : TERM_SLATE,
                wid);
            strnfmt(buf, sizeof(buf), "Vampires killed: %d/30",
                p_ptr->orome_vampires_killed);
            quest_scene_display_wrapped_text(&scene, panel, col + 2, &row, buf,
                p_ptr->orome_vampires_killed >= 30 ? TERM_L_GREEN : TERM_SLATE,
                wid);
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                get_quest_challenge(QUEST_ID_OROME), TERM_SLATE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_OROME));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case OROME_QUEST_SUCCESS:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Complete - Return for reward", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_OROME));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case OROME_QUEST_REWARDED:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Completed by this character", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s received",
                get_quest_reward_text(QUEST_ID_OROME));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;
        }
        row++;
    }

    if (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED)
    {
        any_quests = true;
        quest_scene_add_line(&scene, panel, col, &row,
            get_quest_title(QUEST_ID_VARDA), TERM_YELLOW);

        switch (p_ptr->varda_quest)
        {
        case VARDA_QUEST_GIVER_PRESENT:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Available - Varda waits in sunlight", TERM_L_BLUE);
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                get_quest_challenge(QUEST_ID_VARDA), TERM_SLATE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_VARDA));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case VARDA_QUEST_ACTIVE:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Active - Seek Duruin's bastion", TERM_WHITE);
            quest_scene_display_wrapped_text(&scene, panel, col, &row,
                get_quest_challenge(QUEST_ID_VARDA), TERM_SLATE, wid);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_VARDA));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case VARDA_QUEST_SUCCESS:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Complete - Claim Varda's blessing", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s",
                get_quest_reward_text(QUEST_ID_VARDA));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;

        case VARDA_QUEST_REWARDED:
            quest_scene_add_line(&scene, panel, col + 2, &row,
                "Completed by this character", TERM_L_GREEN);
            strnfmt(buf, sizeof(buf), "Reward: %s received",
                get_quest_reward_text(QUEST_ID_VARDA));
            quest_scene_display_wrapped_text(&scene, panel, col, &row, buf,
                TERM_SLATE, wid);
            break;
        }
        row++;
    }

    quest_scene_add_previous_completion(&scene, panel, col, &row,
        QUEST_ID_TULKAS,
        METARUN_QUEST_TULKAS, p_ptr->tulkas_quest, TULKAS_QUEST_REWARDED, 1,
        wid, &has_previous_completions);
    quest_scene_add_previous_completion(&scene, panel, col, &row, QUEST_ID_AULE,
        METARUN_QUEST_AULE, p_ptr->aule_quest, AULE_QUEST_REWARDED, 2, wid,
        &has_previous_completions);
    quest_scene_add_previous_completion(&scene, panel, col, &row,
        QUEST_ID_MANDOS,
        METARUN_QUEST_MANDOS, p_ptr->mandos_quest, MANDOS_QUEST_REWARDED, 3,
        wid, &has_previous_completions);
    quest_scene_add_previous_completion(&scene, panel, col, &row, QUEST_ID_NIENA,
        METARUN_QUEST_NIENA, p_ptr->niena_quest, NIENA_QUEST_REWARDED, 4, wid,
        &has_previous_completions);
    quest_scene_add_previous_completion(&scene, panel, col, &row, QUEST_ID_OROME,
        METARUN_QUEST_OROME, p_ptr->orome_quest, OROME_QUEST_REWARDED, 5, wid,
        &has_previous_completions);
    quest_scene_add_previous_completion(&scene, panel, col, &row, QUEST_ID_VARDA,
        METARUN_QUEST_VARDA, p_ptr->varda_quest, VARDA_QUEST_REWARDED, 6, wid,
        &has_previous_completions);

    if (has_previous_completions)
        row++;

    if (!any_quests)
    {
        quest_scene_add_line(&scene, panel, col, &row,
            "No active or completed quests this run.", TERM_SLATE);
        row++;
        quest_scene_add_line(&scene, panel, col, &row,
            "Quest vaults may appear as you delve deeper...", TERM_L_DARK);
    }

    row++;
    quest_scene_add_line(&scene, panel, col, &row, "Press any key to return.",
        TERM_L_WHITE);

    if (!ui_information_scene_present_ui(&scene))
    {
        ui_information_scene_leave(&scope);
        return false;
    }

    (void)ui_information_scene_wait_key_nonrepeat();
    ui_information_scene_leave(&scope);
    return true;
}

/*
 * Display wrapped text for quest status - simple word wrapping
 */
static void display_wrapped_text(int col, int *row, cptr text, byte color, int max_width)
{
    char line_buf[256];
    int line_pos = 0;
    int effective_width = max_width - col - 4; /* Leave margin for indentation */
    int text_len = strlen(text);
    int word_start = 0;
    int i = 0;
    int loop_count = 0; /* Safety counter for this function call */
    
    if (effective_width < 20) effective_width = 20; /* Minimum width */
    
    line_buf[0] = '\0';
    
    while (i <= text_len) {
        /* Safety check to prevent infinite loop */
        loop_count++;
        if (loop_count > 1000) {
            log_warn("display_wrapped_text: safety break, possible infinite loop (text_len=%d, i=%d)", text_len, i);
            break;
        }
        
        /* End of string or found a space */
        if (i == text_len || text[i] == ' ') {
            /* Extract the current word */
            int word_len = i - word_start;
            char word[128];
            
            if (word_len > 0 && word_len < (int)sizeof(word)) {
                /* Copy the word manually to avoid buffer issues */
                int copy_len = word_len;
                if (copy_len >= (int)sizeof(word)) copy_len = (int)sizeof(word) - 1;
                
                /* Manual copy to avoid strncpy issues */
                int j;
                for (j = 0; j < copy_len; j++) {
                    word[j] = text[word_start + j];
                }
                word[copy_len] = '\0';
                
                /* Check if adding this word would exceed the line width */
                int new_line_len = line_pos + (line_pos > 0 ? 1 : 0) + copy_len;
                
                if (new_line_len > effective_width && line_pos > 0) {
                    /* Output current line and start new line with this word */
                    Term_putstr(col + 2, (*row)++, -1, color, line_buf);
                    
                    /* Check if the word itself is too long for a line */
                    if (copy_len > effective_width) {
                        /* Break the word across multiple lines */
                        int word_pos = 0;
                        while (word_pos < copy_len) {
                            int chunk_len = effective_width;
                            if (word_pos + chunk_len > copy_len) {
                                chunk_len = copy_len - word_pos;
                            }
                            
                            /* Extract chunk of the word */
                            char chunk[256];
                            int k;
                            for (k = 0; k < chunk_len && word_pos + k < copy_len; k++) {
                                chunk[k] = word[word_pos + k];
                            }
                            chunk[k] = '\0';
                            
                            /* Output this chunk */
                            Term_putstr(col + 2, (*row)++, -1, color, chunk);
                            word_pos += chunk_len;
                        }
                        
                        /* Reset line buffer */
                        line_buf[0] = '\0';
                        line_pos = 0;
                    } else {
                        /* Word fits on a new line */
                        SDL_strlcpy(line_buf, word, sizeof(line_buf));
                        line_pos = copy_len;
                    }
                } else {
                    /* Add word to current line */
                    if (line_pos > 0) {
                        SDL_strlcat(line_buf, " ", sizeof(line_buf));
                        line_pos++;
                    }
                    SDL_strlcat(line_buf, word, sizeof(line_buf));
                    line_pos += copy_len;
                }
            }
            
            /* Skip spaces and move to next word */
            while (i < text_len && text[i] == ' ') {
                i++;
            }
            word_start = i;
        } else {
            i++;
        }
    }
    
    /* Output any remaining text in the buffer */
    if (line_pos > 0) {
        Term_putstr(col + 2, (*row)++, -1, color, line_buf);
    }
}

/*
 * Simple string search function - finds needle in haystack
 * Returns pointer to first occurrence, or NULL if not found
 */
static char* my_strstr(const char* haystack, const char* needle)
{
    if (!haystack || !needle) return NULL;
    
    int needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    for (const char* p = haystack; *p; p++) {
        int i;
        for (i = 0; i < needle_len && p[i] && p[i] == needle[i]; i++);
        if (i == needle_len) {
            return (char*)p;
        }
    }
    return NULL;
}

/*
 * Process placeholders in quest text (challenge, etc.) with actual values
 */
static cptr process_quest_placeholders(cptr text, int quest_idx)
{
    static char processed_buf[256];

    if (!text) {
        return "";
    }

    SDL_strlcpy(processed_buf, text, sizeof(processed_buf));
    
    if (quest_idx == QUEST_ID_TULKAS) {
        /* Replace [monster name] with actual monster name */
        char* monster_pos = my_strstr(processed_buf, "[monster name]");
        if (monster_pos && p_ptr->tulkas_target_r_idx > 0 && p_ptr->tulkas_target_r_idx < z_info->r_max) {
            monster_race* r_ptr = &r_info[p_ptr->tulkas_target_r_idx];
            char before[128], after[128];
            int before_len = monster_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, monster_pos + 14, sizeof(after)); /* 14 = strlen("[monster name]") */
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, r_name + r_ptr->name, after);
        }
        
        /* Replace [artifact name] with actual artifact name */
        char* artifact_pos = my_strstr(processed_buf, "[artifact name]");
        if (artifact_pos && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
            artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
            char before[128], after[128];
            int before_len = artifact_pos - processed_buf;
            SDL_strlcpy(before, processed_buf, before_len + 1);
            before[before_len] = '\0';
            SDL_strlcpy(after, artifact_pos + 15, sizeof(after)); /* 15 = strlen("[artifact name]") */
            
            /* Get proper artifact name using object_desc */
            char artifact_name[120];
            if (a_ptr->name[0] != '\0') {
                /* Create a temporary object to get proper description */
                object_type temp_obj;
                object_wipe(&temp_obj);
                
                /* Set up the object as the artifact */
                s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
                if (k_idx > 0) {
                    object_prep(&temp_obj, k_idx);
                    temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                    temp_obj.ident |= IDENT_KNOWN;
                    
                    /* Get the full artifact description */
                    object_desc(artifact_name, sizeof(artifact_name), &temp_obj, true, 0);
                } else {
                    SDL_strlcpy(artifact_name, a_ptr->name, sizeof(artifact_name));
                }
            } else {
                SDL_strlcpy(artifact_name, "a legendary weapon", sizeof(artifact_name));
            }
            
            strnfmt(processed_buf, sizeof(processed_buf), "%s%s%s", before, artifact_name, after);
        }
    }
    
    return processed_buf;
}

/*
 * Get quest reward description for status display using actual quest data
 */
static cptr get_quest_reward_text(int quest_idx)
{
    static char reward_buf[200];
    char temp_buf[100];
    
    if (quest_idx <= 0 || quest_idx >= z_info->quest_max) return "Unknown reward";
    
    quest_type* q_ptr = &quest_info[quest_idx];
    reward_buf[0] = '\0';
    
    /* Handle special Tulkas artifact reward */
    if (quest_idx == QUEST_ID_TULKAS && p_ptr->tulkas_prize_a_idx > 0 && p_ptr->tulkas_prize_a_idx < z_info->art_max) {
        artefact_type* a_ptr = &a_info[p_ptr->tulkas_prize_a_idx];
        if (a_ptr->name[0] != '\0') {
            /* Create a temporary object to get proper description */
            object_type temp_obj;
            object_wipe(&temp_obj);
            
            /* Set up the object as the artifact */
            s16b k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (k_idx > 0) {
                object_prep(&temp_obj, k_idx);
                temp_obj.name1 = p_ptr->tulkas_prize_a_idx;
                temp_obj.ident |= IDENT_KNOWN;
                
                /* Get the full artifact description */
                object_desc(reward_buf, sizeof(reward_buf), &temp_obj, true, 0);
                return reward_buf;
            } else {
                SDL_strlcpy(reward_buf, a_ptr->name, sizeof(reward_buf));
                return reward_buf;
            }
        }
    }
    
    /* Varda reward description */
    if (quest_idx == QUEST_ID_VARDA) {
        SDL_strlcpy(reward_buf, "Choose one radiant artefact and unlock the Oath of Light (+1 light radius)", sizeof(reward_buf));
        return reward_buf;
    }
    
    /* Build reward description from quest data */
    bool has_rewards = false;
    
    /* Check stat bonuses */
    if (q_ptr->stat_bonuses[0] || q_ptr->stat_bonuses[1] || q_ptr->stat_bonuses[2] || q_ptr->stat_bonuses[3]) {
        has_rewards = true;
        SDL_strlcat(reward_buf, "Stats: ", sizeof(reward_buf));
        
        if (q_ptr->stat_bonuses[0]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Str ", q_ptr->stat_bonuses[0]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[1]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Dex ", q_ptr->stat_bonuses[1]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[2]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Con ", q_ptr->stat_bonuses[2]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
        if (q_ptr->stat_bonuses[3]) {
            strnfmt(temp_buf, sizeof(temp_buf), "+%d Gra ", q_ptr->stat_bonuses[3]);
            SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
        }
    }
    
    /* Check skill bonuses */
    if (q_ptr->skill_type && q_ptr->skill_bonus) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;
        
        /* Convert skill type to name */
        cptr skill_name = "Unknown";
        switch (q_ptr->skill_type) {
            case 0: skill_name = "Melee"; break;
            case 1: skill_name = "Archery"; break;
            case 2: skill_name = "Evasion"; break;
            case 3: skill_name = "Stealth"; break;
            case 4: skill_name = "Perception"; break;
            case 5: skill_name = "Will"; break;
            case 6: skill_name = "Smithing"; break;
            case 7: skill_name = "Song"; break;
        }
        strnfmt(temp_buf, sizeof(temp_buf), "+%d %s ", q_ptr->skill_bonus, skill_name);
        SDL_strlcat(reward_buf, temp_buf, sizeof(reward_buf));
    }
    
    /* Check special abilities */
    if (q_ptr->ability_type && q_ptr->ability_id < ABILITIES_MAX) {
        if (has_rewards) SDL_strlcat(reward_buf, "| ", sizeof(reward_buf));
        has_rewards = true;
        
        /* Get ability name from ability database */
        cptr ability_name = "Special ability";
        if (q_ptr->ability_type == S_SPC) { /* Special abilities type */
            /* Use ability_index to find the ability and get its name */
            int idx = ability_index(S_SPC, q_ptr->ability_id);
            if (idx >= 0 && idx < z_info->b_max) {
                ability_type* b_ptr = &b_info[idx];
                if (b_ptr->name) {
                    ability_name = b_name + b_ptr->name;
                }
            }
        }
        
        SDL_strlcat(reward_buf, ability_name, sizeof(reward_buf));
    }
    
    /* Check oath association */
    if (q_ptr->oath_id) {
        if (has_rewards) SDL_strlcat(reward_buf, " | ", sizeof(reward_buf));
        has_rewards = true;
        SDL_strlcat(reward_buf, get_oath_name_from_id(q_ptr->oath_id), sizeof(reward_buf));
    }
    
    if (!has_rewards) {
        SDL_strlcpy(reward_buf, "Unknown reward", sizeof(reward_buf));
    }
    
    return reward_buf;
}

/*
 * Show quest status for current metarun - only active and completed quests
 * Now uses quest.txt data instead of hardcoded values
 */
void do_cmd_quest_status(void)
{
    char buf[128];
    int row = 1;
    int col = 2;
    bool any_quests = false;
    int wid, hgt;

    log_trace("QUEST STATUS: do_cmd_quest_status() called");
    /* The legacy quest-status renderer is no longer a live runtime path. */
    (void)buf;
    (void)row;
    (void)col;
    (void)any_quests;
    (void)wid;
    (void)hgt;

    /* Safety check: ensure we have a valid player and metarun */
    if (!p_ptr) {
        log_trace("QUEST STATUS: No player data available");
        msg_print("No character data available.");
        return;
    }

    log_trace("QUEST STATUS: Player exists, quest states - Tulkas: %d, Aule: %d, Mandos: %d",
              p_ptr->tulkas_quest, p_ptr->aule_quest, p_ptr->mandos_quest);

    if (!ui_information_scene_supported())
    {
        log_warn("quest status: snapshot renderer required; legacy quest-status renderer removed");
        msg_print("Quest status viewer requires the snapshot UI renderer.");
        return;
    }

    if (!do_cmd_quest_status_information_scene())
    {
        log_warn("quest status: information-scene presentation failed on the snapshot renderer path");
        msg_print("Quest status viewer unavailable.");
    }
    return;

    /* Check Tulkas quest */
    if (p_ptr->tulkas_quest > TULKAS_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr tulkas_status;
        byte color;
        
        log_trace("QUEST STATUS: Getting title and challenge for Tulkas quest");
        cptr quest_title = get_quest_title(QUEST_ID_TULKAS);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_TULKAS);
        log_trace("QUEST STATUS: Got title='%s', challenge='%s'", quest_title ? quest_title : "NULL", quest_challenge ? quest_challenge : "NULL");
        
        if (!quest_title) quest_title = "Tulkas Quest";
        if (!quest_challenge) quest_challenge = "Unknown challenge";
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->tulkas_quest) {
            case TULKAS_QUEST_GIVER_PRESENT:
                log_trace("QUEST STATUS: Tulkas GIVER_PRESENT case");
                tulkas_status = "Available - Tulkas awaits";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, tulkas_status);
                {
                    log_trace("QUEST STATUS: About to call process_quest_placeholders");
                    cptr processed_challenge = process_quest_placeholders(quest_challenge, QUEST_ID_TULKAS);
                    log_trace("QUEST STATUS: process_quest_placeholders returned: '%s'", processed_challenge ? processed_challenge : "NULL");
                    display_wrapped_text(col, &row, processed_challenge, TERM_SLATE, wid);
                }
                log_trace("QUEST STATUS: Calling get_quest_reward_text for TULKAS (GIVER_PRESENT)");
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_TULKAS));
                log_trace("QUEST STATUS: Reward text result: '%s'", buf);
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case TULKAS_QUEST_ACTIVE:
                log_trace("QUEST STATUS: Tulkas ACTIVE case");
                {
                    /* Use processed challenge text instead of hardcoded status */
                    log_trace("QUEST STATUS: About to call process_quest_placeholders for ACTIVE");
                    cptr processed_challenge = process_quest_placeholders(quest_challenge, QUEST_ID_TULKAS);
                    log_trace("QUEST STATUS: process_quest_placeholders returned: '%s'", processed_challenge ? processed_challenge : "NULL");
                    display_wrapped_text(col, &row, processed_challenge, TERM_WHITE, wid);
                    strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_TULKAS));
                    display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                    break;
                }
            case TULKAS_QUEST_COMPLETE:
                log_trace("QUEST STATUS: Tulkas COMPLETE case");
                tulkas_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, tulkas_status);
                break;
            case TULKAS_QUEST_REWARDED:
                /* For Tulkas quest (not location-specific), completed by this character means
                 * the character progressed through the quest states to REWARDED */
                tulkas_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, tulkas_status);
                log_trace("QUEST STATUS: Calling get_quest_reward_text for TULKAS (REWARDED)");
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_TULKAS));
                log_trace("QUEST STATUS: Reward text result: '%s'", buf);
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                tulkas_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, tulkas_status);
        }
        row++;
    }

    /* Check Aule quest */
    if (p_ptr->aule_quest > AULE_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr aule_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_AULE);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_AULE);
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->aule_quest) {
            case AULE_QUEST_FORGE_PRESENT:
                aule_status = "Available - Enter the forge";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, aule_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case AULE_QUEST_ACTIVE:
                /* Use challenge text instead of hardcoded status */
                display_wrapped_text(col, &row, quest_challenge, TERM_WHITE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case AULE_QUEST_SUCCESS:
                aule_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, aule_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_AULE));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case AULE_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                aule_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, aule_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_AULE));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                aule_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, aule_status);
        }
        row++;
    }

    /* Check Mandos quest */
    if (p_ptr->mandos_quest > MANDOS_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr mandos_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_MANDOS);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_MANDOS);
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->mandos_quest) {
            case MANDOS_QUEST_GIVER_PRESENT:
                mandos_status = "Available - Enter the tomb";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
                Term_putstr(col + 2, row++, -1, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_ACTIVE:
                mandos_status = "Active";
                color = TERM_WHITE;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case MANDOS_QUEST_SUCCESS:
                mandos_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_MANDOS));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case MANDOS_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                mandos_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_MANDOS));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                mandos_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, mandos_status);
        }
        row++;
    }

    /* Check Niena quest */
    if (p_ptr->niena_quest > NIENA_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr niena_status;
        byte color;
        
        cptr quest_title = get_quest_title(QUEST_ID_NIENA);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_NIENA);
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->niena_quest) {
            case NIENA_QUEST_GIVER_PRESENT:
                niena_status = "Available - Niena offers mercy";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, niena_status);
                Term_putstr(col + 2, row++, -1, TERM_SLATE, quest_challenge);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_ACTIVE:
                strnfmt(buf, sizeof(buf), "Active: %d seen, %d killed",
                        p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
                niena_status = buf;
                color = TERM_WHITE;
                Term_putstr(col + 2, row++, -1, color, niena_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case NIENA_QUEST_SUCCESS:
                niena_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, niena_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_NIENA));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_REWARDED:
                /* Universal quest attribution logic:
                 * If quest state is REWARDED, it was completed by this character */
                niena_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, niena_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_NIENA));
                Term_putstr(col + 2, row++, -1, TERM_SLATE, buf);
                break;
            case NIENA_QUEST_FAILED:
                strnfmt(buf, sizeof(buf), "Failed: %d seen, %d killed",
                        p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);
                color = TERM_RED;
                Term_putstr(col + 2, row++, -1, color, buf);
                Term_putstr(col + 2, row++, -1, TERM_SLATE,
                            "You took a life and lost Niena's mercy.");
                break;
            default:
                niena_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, niena_status);
        }
        row++;
    }

    /* Check Orome quest */
    if (p_ptr->orome_quest > OROME_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr orome_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_OROME);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_OROME);
        
        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);
        
        switch (p_ptr->orome_quest) {
            case OROME_QUEST_GIVER_PRESENT:
                orome_status = "Available - Orome awaits";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, orome_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case OROME_QUEST_ACTIVE:
                {
                    strnfmt(buf, sizeof(buf), "Active: Hunt the fell kindreds");
                    orome_status = buf;
                    color = TERM_WHITE;
                    Term_putstr(col + 2, row++, -1, color, orome_status);
                    
                    /* Show current kill counts for all monster types */
                    strnfmt(buf, sizeof(buf), "Wolves killed: %d/100", p_ptr->orome_wolves_killed);
                    display_wrapped_text(col + 4, &row, buf, 
                                       p_ptr->orome_wolves_killed >= 100 ? TERM_L_GREEN : TERM_SLATE, wid);
                    strnfmt(buf, sizeof(buf), "Spiders killed: %d/80", p_ptr->orome_spiders_killed);
                    display_wrapped_text(col + 4, &row, buf, 
                                       p_ptr->orome_spiders_killed >= 80 ? TERM_L_GREEN : TERM_SLATE, wid);
                    strnfmt(buf, sizeof(buf), "Serpents killed: %d/60", p_ptr->orome_serpents_killed);
                    display_wrapped_text(col + 4, &row, buf, 
                                       p_ptr->orome_serpents_killed >= 60 ? TERM_L_GREEN : TERM_SLATE, wid);
                    strnfmt(buf, sizeof(buf), "Vampires killed: %d/30", p_ptr->orome_vampires_killed);
                    display_wrapped_text(col + 4, &row, buf, 
                                       p_ptr->orome_vampires_killed >= 30 ? TERM_L_GREEN : TERM_SLATE, wid);
                    
                    display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                    strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                    display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                }
                break;
            case OROME_QUEST_SUCCESS:
                orome_status = "Complete - Return for reward";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, orome_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_OROME));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case OROME_QUEST_REWARDED:
                /* For Orome quest (not location-specific), completed by this character means
                 * the character progressed through the quest states to REWARDED */
                orome_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, orome_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_OROME));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                orome_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, orome_status);
        }
        row++;
    }

    /* Check Varda quest */
    if (p_ptr->varda_quest > VARDA_QUEST_NOT_STARTED) {
        any_quests = true;
        cptr varda_status;
        byte color;

        cptr quest_title = get_quest_title(QUEST_ID_VARDA);
        cptr quest_challenge = get_quest_challenge(QUEST_ID_VARDA);
        if (!quest_title) quest_title = "Varda Quest";
        if (!quest_challenge) quest_challenge = "Unknown challenge";

        Term_putstr(col, row++, -1, TERM_YELLOW, quest_title);

        switch (p_ptr->varda_quest) {
            case VARDA_QUEST_GIVER_PRESENT:
                varda_status = "Available - Varda waits in sunlight";
                color = TERM_L_BLUE;
                Term_putstr(col + 2, row++, -1, color, varda_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case VARDA_QUEST_ACTIVE:
                varda_status = "Active - Seek Duruin's bastion";
                color = TERM_WHITE;
                Term_putstr(col + 2, row++, -1, color, varda_status);
                display_wrapped_text(col, &row, quest_challenge, TERM_SLATE, wid);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case VARDA_QUEST_SUCCESS:
                varda_status = "Complete - Claim Varda's blessing";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, varda_status);
                strnfmt(buf, sizeof(buf), "Reward: %s", get_quest_reward_text(QUEST_ID_VARDA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            case VARDA_QUEST_REWARDED:
                varda_status = "Completed by this character";
                color = TERM_L_GREEN;
                Term_putstr(col + 2, row++, -1, color, varda_status);
                strnfmt(buf, sizeof(buf), "Reward: %s received", get_quest_reward_text(QUEST_ID_VARDA));
                display_wrapped_text(col, &row, buf, TERM_SLATE, wid);
                break;
            default:
                varda_status = "Unknown status";
                color = TERM_SLATE;
                Term_putstr(col + 2, row++, -1, color, varda_status);
        }
        row++;
    }

    /* Show previous metarun completions */
    bool has_previous_completions = false;
    int tulkas_completed = metarun_quest_completion_count(METARUN_QUEST_TULKAS);
    if (tulkas_completed > 0 && p_ptr->tulkas_quest != TULKAS_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_TULKAS);
        cptr oath_name = get_oath_name_from_id(quest_info[1].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, tulkas_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int aule_completed = metarun_quest_completion_count(METARUN_QUEST_AULE);
    if (aule_completed > 0 && p_ptr->aule_quest != AULE_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_AULE);
        cptr oath_name = get_oath_name_from_id(quest_info[2].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, aule_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int mandos_completed = metarun_quest_completion_count(METARUN_QUEST_MANDOS);
    if (mandos_completed > 0 && p_ptr->mandos_quest != MANDOS_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_MANDOS);
        cptr oath_name = get_oath_name_from_id(quest_info[3].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, mandos_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int niena_completed = metarun_quest_completion_count(METARUN_QUEST_NIENA);
    if (niena_completed > 0 && p_ptr->niena_quest != NIENA_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_NIENA);
        cptr oath_name = get_oath_name_from_id(quest_info[4].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, niena_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int orome_completed = metarun_quest_completion_count(METARUN_QUEST_OROME);
    if (orome_completed > 0 && p_ptr->orome_quest != OROME_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_OROME);
        cptr oath_name = get_oath_name_from_id(quest_info[5].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, orome_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    int varda_completed = metarun_quest_completion_count(METARUN_QUEST_VARDA);
    if (varda_completed > 0 && p_ptr->varda_quest != VARDA_QUEST_REWARDED) {
        if (!has_previous_completions) {
            Term_putstr(col, row++, -1, TERM_L_DARK, "Previously Completed in Metarun:");
            has_previous_completions = true;
        }
        cptr quest_title = get_quest_title(QUEST_ID_VARDA);
        cptr oath_name = get_oath_name_from_id(quest_info[6].oath_id);
        char status_text[150];
        strnfmt(status_text, sizeof(status_text), "%s - %s (metarun x%d)", quest_title, oath_name, varda_completed);
        display_wrapped_text(col, &row, status_text, TERM_SLATE, wid);
    }
    
    if (has_previous_completions) {
        row++;
    }

    /* If no quests are active or completed */
    if (!any_quests) {
        Term_putstr(col, row++, -1, TERM_SLATE, "No active or completed quests this run.");
        row++;
        Term_putstr(col, row++, -1, TERM_L_DARK, "Quest vaults may appear as you delve deeper...");
    }

    row++;
    Term_putstr(col, row, -1, TERM_L_WHITE, "Press any key to return.");
    inkey();
    
    screen_load();
}

/*
 * Quest typewriter menu function - displays quest dialog with typewriter effect
 * Based on print_story_intro() style
 */
void quest_typewriter_menu(cptr title, cptr texts[], int total_texts, byte title_color, byte text_color)
{
    int wid, h;
    const int indent = 2;
    bool skipped = false;
    ui_information_scene_scope info_scope;
    bool scene_active = ui_information_scene_enter(&info_scope);

    /* Disable fade/typewriter in information scene mode */
    if (scene_active)
        skipped = true;
    
    /* Get terminal size */
    Term_get_size(&wid, &h);
    int wrap_width = wid - indent * 2;
    
    /* Save screen and start fresh */
    if (!scene_active)
        screen_save();
    Term_clear();
    
    /* Display title */
    int title_y = 1;
    Term_putstr((wid - strlen(title)) / 2, title_y, -1, title_color, title);
    
    int row = 3, col = 0;
    
    for (int idx = 0; idx < total_texts; idx++) {
        const char *s = texts[idx];
        
        /* Handle empty lines as paragraph breaks */
        if (!s || strlen(s) == 0) {
            /* Empty line - just advance row for paragraph break */
            row++;
            col = 0;
            /* Short pause for empty line */
            Term_xtra(TERM_XTRA_DELAY, 200);
            continue;
        }
        
        /* Count lines needed for this paragraph */
        int lines_needed = 0;
        int temp_col = col;
        for (int i = 0; s[i]; i++) {
            if (s[i] == '\n' || temp_col >= wrap_width) {
                lines_needed++;
                temp_col = 0;
                if (s[i] == '\n') continue;
            }
            temp_col++;
        }
        lines_needed++; /* Add one for the text itself */
        
        /* Check if we have enough space for the whole paragraph */
        if (row + lines_needed >= h - 2) {
            Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(press any key to continue)");
            {
                char k;
                if (scene_active)
                {
                    (void)ui_information_scene_present_term();
                    k = (char)ui_information_scene_wait_key();
                }
                else
                {
                    k = inkey();
                }
                if (k == 'Q' || k == 'q') { /* Q/q skips remaining dialog */
                    Term_clear();
                    if (scene_active)
                        ui_information_scene_leave(&info_scope);
                    else
                        screen_load();
                    return;
                }
            }
            Term_clear();
            /* Redisplay title */
            Term_putstr((wid - strlen(title)) / 2, title_y, -1, title_color, title);
            row = 3;
        }
        
        col = 0;
        
        /* Print this string with proper word wrapping and typewriter effect */
        int i = 0;
        while (s[i]) {
            /* Handle explicit newlines */
            if (s[i] == '\n') {
                row++;
                col = 0;
                i++;
                continue;
            }
            
            /* Find the end of the current word (or until we hit wrap width) */
            int word_start = i;
            int word_len = 0;
            bool has_space_after = false;
            
            /* Build the current word/phrase until we hit whitespace, newline, or exceed reasonable length */
            while (s[i] && s[i] != '\n' && word_len < wrap_width) {
                if (s[i] == ' ' || s[i] == '\t') {
                    has_space_after = true;
                    break;
                }
                word_len++;
                i++;
            }
            
            log_trace("WRAP DEBUG: word='%.*s', word_len=%d, col=%d, wrap_width=%d", word_len, &s[word_start], word_len, col, wrap_width);
            
            /* Check if this word fits on the current line */
            if (col + word_len > wrap_width && col > 0) {
                /* Word doesn't fit, wrap to next line */
                log_trace("WRAP DEBUG: Wrapping word to next line (col=%d + word_len=%d > wrap_width=%d)", col, word_len, wrap_width);
                row++;
                col = 0;
            }
            
            /* Print the word character by character with typewriter effect */
            if (skipped) {
                /* Skip mode: print entire word instantly */
                for (int j = word_start; j < word_start + word_len; j++) {
                    Term_putch(indent + col, row, text_color, s[j]);
                    col++;
                }
            }
            else {
                /* Normal mode: typewriter effect with character-by-character */
                for (int j = word_start; j < word_start + word_len; j++) {
                    /* Check for ESC or Enter key press to skip typewriter effect */
                    char check_key;
                    if (Term_inkey(&check_key, false, false) == 0) {
                        /* Only respond to ESC or Enter - consume and check */
                        Term_inkey(&check_key, false, true);
                        if (check_key == ESCAPE || check_key == '\n' || check_key == '\r') {
                            skipped = true;
                            /* Print rest of current word instantly */
                            for (int k = j; k < word_start + word_len; k++) {
                                Term_putch(indent + col, row, text_color, s[k]);
                                col++;
                            }
                            break; /* Exit to continue with rest of text in skip mode */
                        }
                        /* Other keys are ignored (already consumed) */
                    }
                    
                    /* Print character with typewriter effect */
                    Term_putch(indent + col, row, text_color, s[j]);
                    Term_fresh();
                    col++;
                    
                    /* Delay 25 ms after each character for typewriter effect */
                    Term_xtra(TERM_XTRA_DELAY, 25);
                }
            }
            
            /* Handle the space/whitespace after the word */
            if (has_space_after) {
                if (s[i] == ' ') {
                    /* Only print space if we're not at the end of a line */
                    if (col < wrap_width) {
                        Term_putch(indent + col, row, text_color, ' ');
                        if (!skipped) Term_fresh();
                        col++;
                        
                        /* Delay for space too (unless skipped) */
                        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 25);
                    }
                    i++; /* Skip the space */
                }
                else if (s[i] == '\t') {
                    /* Handle tab - convert to spaces but respect wrap width */
                    int tab_spaces = 4 - (col % 4);
                    for (int t = 0; t < tab_spaces && col < wrap_width; t++) {
                        Term_putch(indent + col, row, text_color, ' ');
                        if (!skipped) Term_fresh();
                        col++;
                        
                        /* Delay for tab spaces (unless skipped) */
                        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 25);
                    }
                    i++; /* Skip the tab */
                }
            }
        }
        
        /* Move to next line after text */
        row++;
        col = 0;
        
        /* 400ms pause after each line of text (unless skipped) */
        if (!skipped) Term_xtra(TERM_XTRA_DELAY, 400);
    }
    
    /* Refresh screen to show all text if skipped */
    if (skipped) Term_fresh();
    
    /* Final prompt */
    Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(press any key to continue)");
    if (scene_active)
    {
        (void)ui_information_scene_present_term();
        (void)ui_information_scene_wait_key();
    }
    else
    {
        inkey();
    }
    
    /* Flush any queued keypresses that accumulated during the typewriter effect */
    Term_flush();
    
    Term_clear();
    if (scene_active)
        ui_information_scene_leave(&info_scope);
    else
        screen_load();
}
