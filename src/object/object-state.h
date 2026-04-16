#ifndef INCLUDED_OBJECT_OBJECT_STATE_H
#define INCLUDED_OBJECT_OBJECT_STATE_H

#include "h-basic.h"

extern s16b o_max;
extern s16b o_cnt;
extern object_type* o_list;
extern object_type* inventory;

extern s16b alloc_kind_size;
extern alloc_entry* alloc_kind_table;
extern s16b alloc_ego_size;
extern alloc_entry* alloc_ego_table;
extern s16b alloc_race_size;
extern alloc_entry* alloc_race_table;

extern byte object_generation_mode;

#endif /* INCLUDED_OBJECT_OBJECT_STATE_H */
