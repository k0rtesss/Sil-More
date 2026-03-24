/* File: drop-system-internal.h */

/*
 * Internal declarations shared by the active drop catalog/selection split.
 */

#ifndef INCLUDED_DROP_SYSTEM_INTERNAL_H
#define INCLUDED_DROP_SYSTEM_INTERNAL_H

#include "drop_system.h"

#include <stddef.h>

/* Default weights used when no drop_profile overrides are provided. */
#define DROP_DEFAULT_CAT_WEIGHT 25
#define DROP_DEFAULT_SUPPLY_WEIGHT 1

typedef enum
{
    DROP_CAT_WEAPON = 0,
    DROP_CAT_ARMOR = 1,
    DROP_CAT_JEWELRY = 2,
    DROP_CAT_SUPPLY = 3,
    DROP_CAT_MAX = 4
} drop_category;

typedef enum
{
    DROP_GROUP_NORMAL = 0,
    DROP_GROUP_EGO = 1,
    DROP_GROUP_ARTIFACT = 2
} drop_group_kind;

#define DROP_ALLOC_MAX 8

typedef struct
{
    object_type obj; /* fully specified object template */
    drop_category category;
    drop_group_kind group_kind;
    s16b group_id; /* k_idx for normal, e_idx for ego, a_idx for artifact */
    s16b difficulty; /* smithing difficulty (baseline, penalised separately) */
    s16b min_depth;
    s16b max_depth;
    byte num_allocations; /* number of depth/rarity allocation pairs */
    byte alloc_depth[DROP_ALLOC_MAX]; /* depth thresholds where rarity changes */
    byte alloc_rarity[DROP_ALLOC_MAX]; /* rarity value from this depth onward (0 allowed) */
    bool noble; /* NOBLE_ITEM alignment flag used for chest/vault rules */
    bool evil; /* EVIL_ITEM alignment flag used for special themed pools */
} drop_entry;

/* Drop catalog global state. */
extern drop_entry* g_drop_entries;
extern size_t g_drop_count;
extern size_t g_drop_capacity;

/* Shared helpers used by catalog building and selection. */
extern int smithing_difficulty_baseline(const object_type* o_ptr);

#endif /* INCLUDED_DROP_SYSTEM_INTERNAL_H */
