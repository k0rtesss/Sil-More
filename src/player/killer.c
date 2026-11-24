#include "player/killer.h"

#include "angband.h"
#include "externs.h"
#include "score/score_guid.h"

static killer_info pending_info;
static killer_info last_info;

void killer_reset(void)
{
    memset(&pending_info, 0, sizeof(pending_info));
    memset(&last_info, 0, sizeof(last_info));
}

killer_info killer_monster_source(const monster_type* m_ptr)
{
    return (killer_info){
        .valid = true,
        .kind = SCORE_KILLER_MONSTER,
        .guid = score_guid_from_u64(r_info[m_ptr->r_idx].guid),
        .race_index = (u16b)m_ptr->r_idx,
        .first_wolf = m_ptr->achievement_first_wolf && m_ptr->r_idx == 11
    };
}

void killer_mark_monster(const monster_type* m_ptr)
{
    if (m_ptr)
        pending_info = killer_monster_source(m_ptr);
}

void killer_mark_other(score_killer_kind kind)
{
    pending_info = (killer_info){ .valid = true, .kind = kind };
}

killer_info killer_take_pending(void)
{
    killer_info source = pending_info;
    pending_info.valid = false;
    return source;
}

void killer_commit_source(killer_info source)
{
    last_info = source.valid ? source
        : (killer_info){ .valid = true, .kind = SCORE_KILLER_OTHER };
}

void killer_commit(cptr cause)
{
    (void)cause;
    killer_commit_source(killer_take_pending());
}

const killer_info* killer_last(void)
{
    return last_info.valid ? &last_info : NULL;
}
