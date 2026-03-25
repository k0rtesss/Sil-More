#ifndef INCLUDED_PLAYER_BANE_H
#define INCLUDED_PLAYER_BANE_H

#include "h-basic.h"

typedef struct monster_type monster_type;

enum
{
    PLAYER_BANE_TYPES = 13
};

extern char* bane_name[];

int bane_type_killed(int bane_type);
int elf_bane_bonus(monster_type* m_ptr);
int bane_bonus(monster_type* m_ptr);
int bane_bonus_for_type(int bane_type_idx);
int artifact_bane_bonus(monster_type* m_ptr);
int spider_bane_bonus(void);
int artifact_spider_bane_bonus(void);
int unique_bane_bonus(monster_type* m_ptr);
int unique_bane_type_killed(void);

#endif /* INCLUDED_PLAYER_BANE_H */
