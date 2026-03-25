/* File: smithing-internal.h */

/*
 * Internal smithing declarations shared across smithing translation units.
 *
 * This header is not meant to be included outside of the smithing subsystem
 * and the smithing UI implementation.
 */

#ifndef INCLUDED_SMITHING_INTERNAL_H
#define INCLUDED_SMITHING_INTERNAL_H

#include "smithing/smithing.h"

typedef struct artefact_type artefact_type;

/*
 * Smithing UI categories for base items.
 */
#define CAT_WEAPON 0
#define CAT_ARMOUR 1
#define CAT_JEWELRY 2

#define MAX_SMITHING_TVALS 17

typedef struct smithing_tval_desc
{
    int category;
    int tval;
    cptr desc;
} smithing_tval_desc;

typedef enum
{
    SMITH_ALLOY_NONE = 0,
    SMITH_ALLOY_MITHRIL,
    SMITH_ALLOY_STAR_IRON,
} smith_alloy_type;

typedef struct
{
    smith_alloy_type type;
    byte bonus_att;
    byte bonus_ds;
    byte bonus_evn;
    byte bonus_ps;
} smith_alloy_state;

/* Internal smithing state (backups used by menus/preview). */
extern object_type* smith2_o_ptr;
extern object_type* smith3_o_ptr;

extern smith_alloy_state smith_alloy;
extern smith_alloy_state smith2_alloy;
extern smith_alloy_state smith3_alloy;

extern bool enchant_then_numbers;

/*
 * Temporary artefact slots used while crafting self-made artefacts.
 */
#define smith_a_name (z_info->art_self_made_max - 1)
#define smith_a_ptr (&a_info[smith_a_name])

#define smith2_a_name (z_info->art_self_made_max - 2)
#define smith2_a_ptr (&a_info[smith2_a_name])

/*
 * Costs computed as a side-effect of object_difficulty().
 */
typedef struct smithing_cost_type
{
    int str;
    int dex;
    int con;
    int gra;
    int exp;
    int smt;
    int mithril;
    int star_iron;
    int alloy_weight;
    int alloy_metal;
    int alloy_mastery;
    int uses;
    int drain;
    int weaponsmith;
    int armoursmith;
    int jeweller;
    int enchantment;
    int artifice;
} smithing_cost_type;

extern smithing_cost_type smithing_cost;

/*
 * Base item list shared by smithing UI and alloy/difficulty helpers.
 */
extern const smithing_tval_desc smithing_tvals[MAX_SMITHING_TVALS];

/*
 * Alloy helpers.
 */
bool object_has_evil_alignment(const object_type* o_ptr);
void smith_clear_alloy_state(smith_alloy_state* state);
void smith_remove_alloy_bonus(object_type* o_ptr, smith_alloy_state* state);
int smith_item_category(const object_type* o_ptr);
bool smith_alloy_applicable(const object_type* o_ptr);
bool smith_apply_alloy(
    object_type* o_ptr, smith_alloy_state* state, smith_alloy_type new_type);
int smith_alloy_weight_required(const object_type* o_ptr);

/*
 * Metal + forge helpers.
 */
bool melt_metal_item(int item_num);
int meltable_metal_items_carried(void);
int mithril_carried(void);
int star_iron_carried(void);
void use_mithril(int cost);
void use_star_iron(int cost);
int forge_uses(int y, int x);
int forge_bonus(int y, int x);

/*
 * Costs (computed by object_difficulty()) and payment helpers.
 */
int too_difficult(object_type* o_ptr);
bool affordable(object_type* o_ptr);
void pay_costs(void);

/*
 * Artefact utility used by smithing menus and finalize step.
 */
void artefact_copy(artefact_type* a1_ptr, artefact_type* a2_ptr);
void add_artefact_details(void);
void artefact_menu(void);

#endif /* INCLUDED_SMITHING_INTERNAL_H */
