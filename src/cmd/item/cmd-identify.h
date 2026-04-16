/* File: cmd-identify.h */

#ifndef INCLUDED_CMD_IDENTIFY_H
#define INCLUDED_CMD_IDENTIFY_H

#include "h-basic.h"

void ident_on_wield(object_type* o_ptr);
void ident_resist(u32b flag);
void ident_passive(void);
void ident_see_invisible(const monster_type* m_ptr);
void ident_haunted(void);
void ident_hunger(void);
void ident_f2(u32b flag, object_type* supplied_object);
void ident_f3(u32b flag, object_type* supplied_object);
void ident_weapon_by_use(
    object_type* o_ptr, const monster_type* m_ptr, u32b flag);
void ident_bow_arrow_by_use(object_type* j_ptr, object_type* i_ptr,
    object_type* o_ptr, const monster_type* m_ptr, u32b bow_flag,
    u32b arrow_flag);
void apply_weapon_combat_effects(object_type* o_ptr, monster_type* m_ptr,
    int skill_type, int net_dam, bool fatal_blow, cptr armor_shatter_noun);
int slay_bonus(
    const object_type* o_ptr, const monster_type* m_ptr, u32b* noticed_flag);
int prt_after_sharpness(const object_type* o_ptr, u32b* noticed_flag);

#endif /* INCLUDED_CMD_IDENTIFY_H */
