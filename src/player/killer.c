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

static void killer_set_pending(score_killer_kind kind, score_guid64 guid,
                               u16b race_index, s16b monster_index)
{
    pending_info.kind = kind;
    pending_info.guid = guid;
    pending_info.race_index = race_index;
    pending_info.monster_index = monster_index;
    pending_info.valid = true;
}

void killer_mark_monster(const monster_type* m_ptr)
{
    if (!m_ptr)
        return;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    guid64 guid = score_guid_from_u64(r_ptr->guid);
    killer_set_pending(SCORE_KILLER_MONSTER, guid, (u16b)m_ptr->r_idx,
        (s16b)(m_ptr - mon_list));
}

void killer_mark_other(score_killer_kind kind)
{
    killer_set_pending(kind, score_guid_from_u64(0), 0, 0);
}

void killer_commit(cptr cause)
{
    (void)cause;
    if (pending_info.valid) {
        last_info = pending_info;
        pending_info.valid = false;
    } else {
        killer_set_pending(SCORE_KILLER_OTHER, score_guid_from_u64(0), 0, 0);
        last_info = pending_info;
        pending_info.valid = false;
    }
    last_info.valid = true;
}

const killer_info* killer_last(void)
{
    return last_info.valid ? &last_info : NULL;
}

const monster_type* killer_last_monster(void)
{
    const killer_info* info = killer_last();
    const monster_type* m_ptr;

    if (!info || info->kind != SCORE_KILLER_MONSTER)
        return NULL;
    if (info->monster_index <= 0 || info->monster_index >= mon_max)
        return NULL;

    m_ptr = &mon_list[info->monster_index];
    if (m_ptr->r_idx != info->race_index)
        return NULL;
    if (m_ptr->hp <= 0)
        return NULL;

    return m_ptr;
}
