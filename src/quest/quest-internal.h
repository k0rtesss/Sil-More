#ifndef INCLUDED_QUEST_INTERNAL_H
#define INCLUDED_QUEST_INTERNAL_H

#include "h-basic.h"

int get_quest_oath_id(int quest_idx);
void grant_followup_quest_rewards(int quest_id);
void remove_quest_giver_silent(int quest_giver_r_idx);
bool tulkas_has_valid_target(int depth);

#endif /* INCLUDED_QUEST_INTERNAL_H */
