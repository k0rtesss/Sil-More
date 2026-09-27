#ifndef SIL_QUEST_OPTIONS_H
#define SIL_QUEST_OPTIONS_H

#include "angband.h"

/* Quest settings are numbered to match the Q: IDs in quest.txt. */
bool quest_enabled(int id);

/* Beta quest-system switches are independent and default to disabled. */
bool quest_rules_enabled(void);
bool quest_rewards_enabled(void);
bool quest_challenges_enabled(void);
bool quest_lineage_enabled(void);

/* Small debug/test hook; normal UI code edits op_ptr through the settings page. */
void quest_set_enabled(int id, bool enabled);

#endif /* SIL_QUEST_OPTIONS_H */
