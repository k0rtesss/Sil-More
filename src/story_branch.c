#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "story_branch.h"

static bool story_branch_has_manwe_run_state(void)
{
    return p_ptr && ((p_ptr->manwe_deception_flags & MANWE_DECEPTION_FLAG_MASK) != 0);
}

story_branch_run_kind_type story_branch_run_kind(void)
{
    if (run_mode_is_blitz())
        return STORY_BRANCH_RUN_NORMAL;

    metarun_branch_state_type state = metarun_branch_state();

    /* The branch may be chosen before the current deception run fully ends.
     * Keep that live character in Manwe-run semantics until its run-local
     * flags are gone, so depth/chase logic can still recognize the context. */
    if (state == METARUN_BRANCH_MANWE_QUEST_ACTIVE ||
        (state != METARUN_BRANCH_NONE && story_branch_has_manwe_run_state()))
    {
        return STORY_BRANCH_RUN_MANWE_DECEPTION;
    }

    switch (state) {
        case METARUN_BRANCH_LIGHT_ENDGAME_ACTIVE:
            return STORY_BRANCH_RUN_LIGHT_CUTSCENE;
        case METARUN_BRANCH_UNLIGHT_CHOSEN:
            return STORY_BRANCH_RUN_UNLIGHT_ALLY;
        case METARUN_BRANCH_UNLIGHT_FINAL_ACTIVE:
            return STORY_BRANCH_RUN_UNLIGHT_FINAL;
        default:
            return STORY_BRANCH_RUN_NORMAL;
    }
}

bool story_branch_is_manwe_deception_run(void)
{
    return story_branch_run_kind() == STORY_BRANCH_RUN_MANWE_DECEPTION;
}

bool story_branch_is_light_cutscene_run(void)
{
    return story_branch_run_kind() == STORY_BRANCH_RUN_LIGHT_CUTSCENE;
}

bool story_branch_is_unlight_ally_run(void)
{
    return story_branch_run_kind() == STORY_BRANCH_RUN_UNLIGHT_ALLY;
}

bool story_branch_is_unlight_final_run(void)
{
    return story_branch_run_kind() == STORY_BRANCH_RUN_UNLIGHT_FINAL;
}

bool story_branch_allows_valar_quests(void)
{
    return story_branch_run_kind() == STORY_BRANCH_RUN_NORMAL;
}

bool story_branch_allows_oath_selection(void)
{
    metarun_branch_state_type state = metarun_branch_state();

    return state != METARUN_BRANCH_UNLIGHT_CHOSEN &&
        state != METARUN_BRANCH_UNLIGHT_FINAL_ACTIVE;
}

void story_branch_prepare_new_character(void)
{
    if (run_mode_is_blitz() || !p_ptr)
        return;

    metarun_branch_state_type state = metarun_branch_state();

    switch (state) {
        case METARUN_BRANCH_MANWE_QUEST_PENDING:
            p_ptr->manwe_deception_flags = 0;
            metarun_set_branch_state(METARUN_BRANCH_MANWE_QUEST_ACTIVE);
            log_info("Story branch: Manwe deception run activated for new character");
            break;
        case METARUN_BRANCH_MANWE_QUEST_ACTIVE:
            p_ptr->manwe_deception_flags = 0;
            log_info("Story branch: repeating active Manwe deception run for new character");
            break;
        case METARUN_BRANCH_LIGHT_ENDGAME_ACTIVE:
            log_info("Story branch: light endgame cutscene run active for new character");
            break;
        case METARUN_BRANCH_UNLIGHT_CHOSEN:
            log_info("Story branch: unlight ally run active for new character");
            break;
        case METARUN_BRANCH_UNLIGHT_FINAL_ACTIVE:
            log_info("Story branch: unlight final battle run active for new character");
            break;
        default:
            break;
    }
}
