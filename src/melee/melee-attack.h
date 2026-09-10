#ifndef INCLUDED_MELEE_ATTACK_H
#define INCLUDED_MELEE_ATTACK_H

#include "../h-basic.h"

typedef struct monster_type monster_type;
typedef struct object_type object_type;

extern bool blocking_bonus_active(void);
extern int elem_bonus(int effect);
extern int protection_roll(int typ, bool melee);
extern int p_min(int typ, bool melee);
extern int p_max(int typ, bool melee);
extern int dodging_bonus(void);
extern bool monster_charge(monster_type* m_ptr);
extern bool is_traitor_item(int item_slot);
extern void do_betrayal_ring_amulet(void);
extern void do_betrayal_helm_crown(void);
/* Pure AI candidate descriptions. Scores never reveal lore or spend RNG. */
extern int monster_melee_utility(const monster_type* m_ptr, int blow,
    bool ordinary, bool smite);
extern int monster_best_melee_utility(const monster_type* m_ptr);
extern bool monster_ranged_is_song(int attack);
extern bool monster_ranged_targets_player(int attack);
extern bool monster_ranged_attack_legal(const monster_type* m_ptr, int attack);
extern bool monster_ranged_commit(monster_type* m_ptr, int attack);
extern int monster_ranged_utility(const monster_type* m_ptr, int attack);
extern bool monster_breath_hits_grid(const monster_type* m_ptr, int attack,
    int y, int x);
extern bool monster_ai_casting_opportunity(monster_type* m_ptr, int chance);
extern void monster_ai_spend_casting_opportunity(monster_type* m_ptr);

extern bool make_attack_normal(monster_type* m_ptr);
extern int get_sides(int attack);
extern void mon_cloud(int m_idx, int typ, int dd, int ds, int dif, int rad);
extern void shriek(monster_type* m_ptr);
extern bool make_attack_ranged(monster_type* m_ptr, int attack);
extern void cloud_surround(int r_idx, int* typ, int* dd, int* ds, int* rad);

#endif /* INCLUDED_MELEE_ATTACK_H */
