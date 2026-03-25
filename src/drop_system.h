/* File: drop_system.h */

/*
 * Transitional public API for the drop-system split.
 *
 * The implementation now lives across src/drop/ with this header keeping the
 * shared declarations out of externs.h.
 */

#ifndef INCLUDED_DROP_SYSTEM_H
#define INCLUDED_DROP_SYSTEM_H

#include "h-basic.h"
#include "level-generation/level-generation.h"

typedef struct object_type object_type;

#ifndef DROP_QUALITY_T_DEFINED
#define DROP_QUALITY_T_DEFINED
typedef enum
{
    DROP_QUALITY_NORMAL = 0,
    DROP_QUALITY_GOOD = 1,
    DROP_QUALITY_GREAT = 2,
    DROP_QUALITY_SUPERB = 3,
    DROP_QUALITY_ARTEFACT = 4
} drop_quality;
#endif

#define DROP_BONUS_GOOD 5
#define DROP_BONUS_GREAT 10
#define DROP_BONUS_SUPERB 15
#define DROP_BONUS_ARTEFACT 20
#define DROP_GREAT_ARTEFACT_WEIGHT_MULTIPLIER 5
#define DROP_CHEST_NOBLE_RARITY_BONUS 20

#ifndef DROP_PROFILE_T_DEFINED
#define DROP_PROFILE_T_DEFINED
typedef struct
{
    int weight_weapon;
    int weight_armor;
    int weight_jewelry;
    int weight_supply;
    int supply_potion;
    int supply_herb;
    int supply_gem;
    int supply_staff;
    int supply_misc;
    int supply_tunneling;
    bool allow_damaged;
} drop_profile;
#endif

extern void drop_profile_for_partition_kind(
    level_partition_kind kind, drop_profile* out);
extern void drop_profile_for_partition_kind_source(
    level_partition_kind kind, partition_drop_source_t source,
    drop_profile* out);
extern bool drop_allow_noble;
extern bool drop_allow_evil;
extern bool drop_allow_noble_from_quality;
extern drop_quality drop_quality_from_flags(bool good, bool great, bool superb);
extern void drop_profile_default(drop_profile* profile);
extern void partition_config_reset(void);
extern void partition_config_set_drop_profile(level_partition_kind kind,
    partition_drop_source_t source, const drop_profile* profile);
extern void partition_config_set_floor_rules(
    level_partition_kind kind, bool allow_floor_drops);
extern void partition_config_set_base_monster_scale(
    level_partition_kind kind, int numerator, int denominator);
extern void partition_config_set_direct_monster_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count);
extern void partition_config_set_depth_monster_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count, int scale_pct_at_depth_20,
    int hard_cap_divisor);
extern void partition_config_set_object_rules(level_partition_kind kind,
    int room_divisor, int corridor_divisor);
extern void partition_config_set_metal_rule(level_partition_kind kind,
    int divisor, int min_count, int max_count, int min_depth);
extern bool object_uses_smithing_difficulty(const object_type* o_ptr);
extern int object_smithing_difficulty(const object_type* o_ptr);
extern int object_weight_rarity(const object_type* o_ptr, int depth);
extern void drop_system_init(void);
extern bool drop_generate_object(int depth, drop_quality quality, int droptype,
    bool allow_artefacts, object_type* out);
extern bool drop_generate_object_with_bonus(
    int depth, drop_quality quality, int droptype, int extra_bonus,
    bool allow_artefacts, object_type* out);
extern bool drop_generate_object_with_bonus_depths(
    int depth, int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, object_type* out);
extern bool drop_generate_object_profiled(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts,
    const drop_profile* profile, object_type* out);
extern bool drop_generate_object_profiled_depths(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, const drop_profile* profile,
    object_type* out);
extern bool drop_generate_object_profiled_depths_biased(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    int extra_bonus, bool allow_artefacts, int artefact_weight_multiplier,
    const drop_profile* profile, object_type* out);
extern bool drop_generate_guaranteed_artefact(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    const drop_profile* profile, object_type* out);
extern bool drop_generate_chasm_sanctum_object(int depth, object_type* out);
extern void drop_set_chest_vault_type(int vault_type);
extern void drop_set_chest_mode(int mode);
extern void drop_set_chest_material_weights(
    int wooden_pct, int steel_pct, int jewelled_pct);
extern void drop_clear_chest_material_weights(void);

#endif /* INCLUDED_DROP_SYSTEM_H */
