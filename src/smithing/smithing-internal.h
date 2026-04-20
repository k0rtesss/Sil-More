/* File: smithing-internal.h */
/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

/*
 * Internal smithing declarations shared across smithing translation units.
 *
 * This header is not meant to be included outside of the smithing subsystem
 * and the smithing UI implementation.
 */

#ifndef INCLUDED_SMITHING_INTERNAL_H
#define INCLUDED_SMITHING_INTERNAL_H

#include "smithing/smithing.h"
#include "thrall_quest.h"

typedef struct artefact_type artefact_type;
typedef struct object_kind object_kind;
typedef struct ability_type ability_type;

/*
 * Smithing UI categories for base items.
 */
#define CAT_WEAPON 0
#define CAT_ARMOUR 1
#define CAT_JEWELRY 2

#define MAX_SMITHING_TVALS 17

#define SMT_NUM_MENU_I_ATT 1
#define SMT_NUM_MENU_D_ATT 2
#define SMT_NUM_MENU_I_DS 3
#define SMT_NUM_MENU_D_DS 4
#define SMT_NUM_MENU_I_EVN 5
#define SMT_NUM_MENU_D_EVN 6
#define SMT_NUM_MENU_I_PS 7
#define SMT_NUM_MENU_D_PS 8
#define SMT_NUM_MENU_I_WGT 9
#define SMT_NUM_MENU_D_WGT 10
#define SMT_NUM_MENU_ALLOY_CYCLE 11
#define SMT_NUM_MENU_ALLOY_CLEAR 12
#define SMT_NUM_MENU_EDIT_BONUSES 13

#define SMT_NUM_MENU_MAX 13

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

typedef struct reforge_preview_type
{
    int scaled_difficulty;
    int raw_delta_difficulty;
    int turns;
    smithing_cost_type cost;
    bool affordable;
} reforge_preview_type;

typedef enum
{
    SMT_BONUS_ENTRY_STAT = 0,
    SMT_BONUS_ENTRY_SKILL = 1,
    SMT_BONUS_ENTRY_SPECIAL = 2,
} smith_bonus_entry_kind;

typedef enum
{
    SMT_BONUS_SPECIAL_DAMAGE_SIDES = 0,
    SMT_BONUS_SPECIAL_TUNNEL = 1,
} smith_bonus_special_kind;

typedef struct
{
    smith_bonus_entry_kind kind;
    int index;
    u32b flag_pos;
    u32b flag_neg;
    u32b flag;
} smith_bonus_entry;

typedef struct
{
    smith_bonus_entry entry;
    int delta;
} smith_bonus_action;

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
byte smith_default_stack_size(const object_type* o_ptr);

/*
 * UI-facing smithing core helpers.
 */
int att_valid(void);
int att_max(void);
int att_min(void);
int ds_valid(void);
int ds_max(void);
int ds_min(void);
int evn_valid(void);
int evn_max(void);
int evn_min(void);
int ps_valid(void);
int ps_max(void);
int ps_min(void);
bool smithing_variable_protection_dice(const object_type* o_ptr);
bool smithing_can_increase_protection(const object_type* o_ptr);
bool smithing_can_decrease_protection(const object_type* o_ptr);
void smithing_increase_protection(object_type* o_ptr);
void smithing_decrease_protection(object_type* o_ptr);
int pval_valid(void);
int pval_max(void);
int pval_min(void);
int wgt_valid(void);
int wgt_max(void);
int wgt_min(void);
void create_base_object(int tval, int sval);
bool smith_base_item_kind_allowed(const object_kind* k_ptr);
void modify_numbers(int choice);
int smith_collect_bonus_actions(smith_bonus_action* actions, int max_actions);
bool smith_adjust_bonus_entry(const smith_bonus_entry* entry, int delta);

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
bool reforge_preview_build(const object_type* source, int prefix_idx,
    reforge_preview_type* preview);
void pay_smithing_cost_struct(const smithing_cost_type* cost);

/*
 * Ego, reforge, and artefact helpers used by the smithing UI.
 */
bool ego_forbids_prefix_combo(int e_idx);
bool smith_ego_can_apply_to_object(const object_type* o_ptr, int e_idx,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix);
bool ego_prefix_can_apply_to_object(const object_type* o_ptr, int e_idx);
void create_special(int ego_prefix, int ego_suffix);
bool enchant_menu_has_applicable_affix(const object_type* base_o_ptr,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix);
bool object_can_reforge_prefix(const object_type* o_ptr);
int find_reforge_target_item(void);
void prepare_artefact(void);
bool applicable_flag(u32b f, int flagset, object_type* o_ptr);
void add_artefact_flag(u32b f, int flagset);
void remove_artefact_flag(u32b f, int flagset);
bool ability_can_be_smithed(ability_type* b_ptr);
bool applicable_ability(ability_type* b_ptr, object_type* o_ptr);
void add_artefact_ability(int skilltype, int abilitynum);
void remove_artefact_ability(int skilltype, int abilitynum);
bool has_ability(artefact_type* a_ptr, int skilltype, int abilitynum);

/*
 * Artefact utility used by smithing menus and finalize step.
 */
void artefact_copy(artefact_type* a1_ptr, artefact_type* a2_ptr);
void add_artefact_details(void);

#endif /* INCLUDED_SMITHING_INTERNAL_H */
