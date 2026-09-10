#ifndef MONSTER_SENSES_H
#define MONSTER_SENSES_H

enum monster_sense_kind
{
    MON_SENSE_NONE, MON_SENSE_SIGHT, MON_SENSE_SOUND,
    MON_SENSE_SCENT, MON_SENSE_SEARCH, MON_SENSE_SHARED_TRACE
};

bool monster_has_sight(const monster_type* m_ptr);
int monster_scent_limit(const monster_race* r_ptr);
void monster_senses_see(monster_type* m_ptr, int y, int x);
void monster_senses_hear(monster_type* m_ptr, int y, int x);
void monster_senses_share_trace(monster_type* m_ptr, int y, int x);
void monster_senses_refresh(monster_type* m_ptr);
bool monster_senses_target(const monster_type* m_ptr, int* y, int* x);
bool monster_senses_advance(monster_type* m_ptr, int* y, int* x);

byte scent_export_cell(int y, int x);
void scent_restore_begin(void);
void scent_restore_cell(int y, int x, byte normalized);

#endif
