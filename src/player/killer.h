#ifndef INCLUDED_PLAYER_KILLER_H
#define INCLUDED_PLAYER_KILLER_H

#include "h-basic.h"
#include "score/score_format.h"

struct monster_type;

typedef struct killer_info {
    bool valid;
    score_killer_kind kind;
    score_guid64 guid;
    u16b race_index;
    s16b monster_index;
} killer_info;

void killer_reset(void);
void killer_mark_monster(const struct monster_type* m_ptr);
void killer_mark_other(score_killer_kind kind);
void killer_commit(cptr cause);
const killer_info* killer_last(void);
const struct monster_type* killer_last_monster(void);

#endif /* INCLUDED_PLAYER_KILLER_H */
