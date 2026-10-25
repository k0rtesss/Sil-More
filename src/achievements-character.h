#ifndef SIL_ACHIEVEMENTS_CHARACTER_H
#define SIL_ACHIEVEMENTS_CHARACTER_H

/* Include angband.h before this internal gameplay interface. */
bool sil_achievement_for_current_character(int id);
void sil_achievement_character_on_start(bool first_entry);
void sil_achievement_character_on_forged(object_type* object);
void sil_achievement_character_on_smithing_completed(
    const object_type* object, bool custom_artefact, int forge_uses_before,
    int forge_uses_paid, int mithril_paid);
void sil_achievement_character_on_player_turn(void);
void sil_achievement_character_on_silmaril(const object_type* weapon);
void sil_achievement_character_on_melee_kill(
    const object_type* weapon, int critical_dice);
void sil_achievement_character_on_rage_started(void);
void sil_achievement_character_on_entrancement_resisted(monster_type* attacker);
void sil_achievement_character_on_monster_killed(int m_idx, int who);
void sil_achievement_character_song_begin(void);
bool sil_achievement_character_song_effect(
    int song, int m_idx, int previous_alertness);
bool sil_achievement_character_song_end(int cost);

#endif
