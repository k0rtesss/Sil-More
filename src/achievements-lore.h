#ifndef SIL_ACHIEVEMENTS_LORE_H
#define SIL_ACHIEVEMENTS_LORE_H

/* Include angband.h before this internal gameplay interface. */
#define SIL_ACH_LORE_TEVILDO 0x01u
#define SIL_ACH_LORE_GORTHAUR 0x02u
#define SIL_ACH_LORE_DAILIR_KILL 0x04u
#define SIL_ACH_LORE_DAILIR_RECOVERED 0x08u
#define SIL_ACH_LORE_FLAGS 0x0Fu

void sil_achievement_lore_reset(void);
void sil_achievement_lore_attack_begin(int m_idx, const object_type* weapon,
    int damage, bool melee);
void sil_achievement_lore_attack_end(void);
void sil_achievement_lore_attack_add_weapon(const object_type* weapon);
void sil_achievement_lore_on_hit(int m_idx, int damage, int who);
void sil_achievement_lore_on_kill(int m_idx, int who);
void sil_achievement_lore_on_archery_kill(int race);
bool sil_achievement_lore_morgoth_asleep(void);
void sil_achievement_lore_on_silmaril_cut(bool morgoth_was_asleep);
void sil_achievement_lore_on_morgoth_alertness(int alertness);
void sil_achievement_lore_on_perception(void);
void sil_achievement_lore_on_recovered(int artefact);
void sil_achievement_lore_on_shatter(int silnum, const object_type* weapon);
void sil_achievement_lore_on_throne_entry(void);
void sil_achievement_lore_on_flee(const monster_type* monster);
void sil_achievement_lore_on_travel(int from_depth, int to_depth);
void sil_achievement_lore_on_escape(int silmarils);
void sil_achievement_lore_on_forged(const object_type* object, bool custom);
void sil_achievement_lore_on_curse_lifted(const object_type* source,
    const object_type* equipment);
bool sil_achievement_lore_has_mairon_ring(void);

#endif
