#ifndef STORY_BRANCH_H
#define STORY_BRANCH_H

#include "metarun.h"

typedef enum {
    STORY_BRANCH_RUN_NORMAL = 0,
    STORY_BRANCH_RUN_MANWE_DECEPTION,
    STORY_BRANCH_RUN_LIGHT_CUTSCENE,
    STORY_BRANCH_RUN_UNLIGHT_ALLY,
    STORY_BRANCH_RUN_UNLIGHT_FINAL
} story_branch_run_kind_type;

story_branch_run_kind_type story_branch_run_kind(void);
bool story_branch_is_manwe_deception_run(void);
bool story_branch_is_light_cutscene_run(void);
bool story_branch_is_unlight_ally_run(void);
bool story_branch_is_unlight_final_run(void);
bool story_branch_allows_valar_quests(void);
bool story_branch_allows_oath_selection(void);
void story_branch_prepare_new_character(void);

#endif /* STORY_BRANCH_H */
