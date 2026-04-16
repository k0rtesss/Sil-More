#ifndef INCLUDED_OBJECT_OBJECT_H
#define INCLUDED_OBJECT_OBJECT_H

#include "object-desc.h"
#include "object-display.h"
#include "object-flags.h"
#include "object-flavor.h"
#include "object-info.h"
#include "object-randart.h"
#include "object-slot.h"
#include "object-ui-display.h"
#include "object-ui-enhanced.h"
#include "object-ui-identify.h"
#include "object-ui-select.h"
#include "object-util.h"

void excise_object_idx(int o_idx);
void delete_object_idx(int o_idx);
void delete_object(int y, int x);
void compact_objects(int size);
void wipe_o_list(void);
s16b o_pop(void);
object_type* get_first_object(int y, int x);
object_type* get_next_object(const object_type* o_ptr);
void get_obj_num_prep(void);
s16b get_obj_num(int level);
void object_known(object_type* o_ptr);
void object_aware(object_type* o_ptr);
void object_tried(object_type* o_ptr);
void ident(object_type* o_ptr);
bool object_has_ego_flag4(const object_type* o_ptr, u32b flag);
s32b object_value(const object_type* o_ptr);
bool object_similar(const object_type* o_ptr, const object_type* j_ptr);
void object_absorb(object_type* o_ptr, object_type* j_ptr);
s16b lookup_kind(int tval, int sval);
void object_wipe(object_type* o_ptr);
void object_copy(object_type* o_ptr, const object_type* j_ptr);
void object_prep(object_type* o_ptr, int k_idx);
void object_refresh_weight(object_type* o_ptr);
void object_into_artefact(object_type* o_ptr, artefact_type* a_ptr);
u32b object_kind_pval_flags1(const object_kind* k_ptr);
u32b artefact_pval_flags1(const artefact_type* a_ptr);
u32b ego_item_pval_flags1(const ego_item_type* e_ptr);
u32b object_pval_flags1(const object_type* o_ptr);
void object_apply_pval_delta_with_mask(object_type* o_ptr, u32b mask,
    int delta);
bool object_apply_ego_affix(object_type* o_ptr, int e_idx, bool smithing);
void object_into_special(object_type* o_ptr, int lev, bool smithing);
void check_artifact_visibility(void);
void apply_magic(object_type* o_ptr, int lev, bool okay, bool good,
    bool great, bool allow_insta);
bool object_is_searched_skeleton(const object_type* o_ptr);
void pseudo_id(object_type* o_ptr);
void id_known_specials(void);
bool make_object(object_type* j_ptr, drop_quality quality, int objecttype);
bool make_object_with_profile(object_type* j_ptr, drop_quality quality,
    int objecttype, const drop_profile* profile);
bool make_guaranteed_artefact(object_type* j_ptr, drop_quality quality,
    int objecttype);
bool make_guaranteed_artefact_with_profile(object_type* j_ptr,
    drop_quality quality, int objecttype, const drop_profile* profile);
s16b floor_carry(int y, int x, object_type* j_ptr);
s16b drop_near(object_type* j_ptr, int chance, int y, int x);
void reveal_trap(int y, int x);
void place_secret_door(int y, int x);
void place_closed_door(int y, int x);
s16b inven_carry(object_type* o_ptr, bool combine_ammo);
bool inven_carry_okay(const object_type* o_ptr);
bool inven_carry_okay_after_removing(
    const object_type* o_ptr, int remove_item, int remove_amt);
bool inven_carry_limit_failed(void);
cptr inven_carry_limit_label(void);
int inven_carry_limit_value(void);
bool inven_carry_limit_can_replace(const object_type* o_ptr);
void inven_item_charges(int item);
void inven_item_describe(int item);
void inven_item_increase(int item, int num);
void inven_item_optimize(int item);
void floor_item_charges(int item);
void floor_item_describe(int item);
void floor_item_increase(int item, int num);
void floor_item_optimize(int item);
void check_pack_overflow(void);
s16b inven_takeoff(int item, int amt);
void inven_enforce_current_pack_limits(void);
void combine_pack(void);
void autoinscribe_ground(void);
void autoinscribe_pack(void);
void inven_drop(int item, int amt);
int object_stack_limit(const object_type* o_ptr);
bool build_object_kind_recall_ui_scene(app_ui_scene* scene, int k_idx,
    cptr prompt, bool overlay_dungeon);
void reorder_pack(bool display_message);
int apply_autoinscription(object_type* o_ptr);
void rearrange_stack(int y, int x);
void obliterate_autoinscription(s16b kind);

#endif /* INCLUDED_OBJECT_OBJECT_H */
