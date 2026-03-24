#ifndef INCLUDED_PLAYER_ABILITY_LOG_H
#define INCLUDED_PLAYER_ABILITY_LOG_H

#include "h-basic.h"

void ability_log_reset(void);
void ability_log_record_gain(int skilltype, int abilitynum);
void ability_log_sync_missing(void);

#endif /* INCLUDED_PLAYER_ABILITY_LOG_H */
