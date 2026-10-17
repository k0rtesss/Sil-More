#ifndef SIL_ACHIEVEMENTS_H
#define SIL_ACHIEVEMENTS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stable indexes are also used by the platform bridges. Append, never reorder. */
typedef enum sil_achievement_id {
    SIL_ACH_INTO_THE_DEEP = 0,
    SIL_ACH_LIGHT_IN_DARKNESS,
    SIL_ACH_BACK_TO_STARLIGHT,
    SIL_ACH_THREE_LIGHTS_IN_EXILE,
    SIL_ACH_THE_SHADOW_FALLS,
    SIL_ACH_THE_QUIET_ROAD,
    SIL_ACH_WORD_UNBROKEN,
    SIL_ACH_FORGED_BY_YOUR_HAND,
    SIL_ACH_A_NAME_WORTH_REMEMBERING,
    SIL_ACH_SONG_BENEATH_THE_MOUNTAIN,
    SIL_ACH_THERE_AND_BACK_AGAIN,
    SIL_ACH_BY_A_THREAD,
    SIL_ACH_LIGHT_OF_THE_ELDAR,
    SIL_ACH_THE_LONG_TALE,
    SIL_ACH_COUNT
} sil_achievement_id;

#define SIL_ACH_ALL_MASK ((1u << SIL_ACH_COUNT) - 1u)
#define SIL_ACH_RUN_STARTED 0x01u
#define SIL_ACH_RUN_TAINTED 0x02u
#define SIL_ACH_RUN_COMEBACK 0x04u
#define SIL_ACH_RUN_FLAGS 0x07u

typedef struct sil_achievement_definition {
    const char *key;
    const char *title;
    const char *description;
} sil_achievement_definition;

const sil_achievement_definition *sil_achievement_get(int id);
uint64_t sil_achievement_unlocked_at(int id);
unsigned int sil_achievements_mask(void);
unsigned int sil_achievements_revision(void);
bool sil_achievements_saved(void);
void sil_achievements_init(void);
void sil_achievements_flush(void);
/* Called by the SDL event pump; platform responses are consumed on this thread. */
void sil_achievements_update(void);
void sil_achievements_on_depth(int depth);
void sil_achievements_on_silmaril(void);
void sil_achievements_on_escape(int silmarils);
void sil_achievements_begin_run(void);
void sil_achievements_on_player_turn(void);
void sil_achievements_on_singing_turn(void);
void sil_achievements_on_damage(int damage, int previous_hp);
void sil_achievements_on_healing(void);
void sil_achievements_on_level_entry(bool restoring);
void sil_achievements_on_monster_killed(int m_idx, int who);
void sil_achievements_on_light_kill(void);
void sil_achievements_on_smithing_completed(bool custom_artefact);
void sil_achievements_on_morgoth_defeated(void);
void sil_achievements_on_run_completed(void);
void sil_achievements_on_tale_completed(void);
void sil_achievements_on_tale_discarded(uint32_t id);
void sil_achievements_show(void);

#ifdef __cplusplus
}
#endif
#endif
