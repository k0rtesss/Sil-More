#ifndef SIL_ACHIEVEMENTS_H
#define SIL_ACHIEVEMENTS_H

#include <stdbool.h>
#include <stddef.h>
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
    SIL_ACH_NO_RINGS_ATTACHED,
    SIL_ACH_CABED_EN_ARAS,
    SIL_ACH_OFT_EVIL_WILL,
    SIL_ACH_THE_HASTY_STROKE,
    SIL_ACH_A_FEW_LEAVES,
    SIL_ACH_TWO_WEAPONS,
    SIL_ACH_FOR_EXTERNAL_USE_ONLY,
    SIL_ACH_FLY_YOU_FOOL,
    SIL_ACH_KNOCK_WITH_YOUR_HEAD,
    SIL_ACH_DOWN_TO_GOBLIN_TOWN,
    SIL_ACH_ATHELAS_OUT_OF_STOCK,
    SIL_ACH_ANGBAND_MAINTENANCE,
    SIL_ACH_THE_ROAD_GOES_EVER_ON,
    SIL_ACH_SOMETHING_TOOKISH,
    SIL_ACH_FLIES_AND_SPIDERS,
    SIL_ACH_NOT_EVEN_CARCHAROTH,
    SIL_ACH_THE_FORDS_OF_SIRION,
    SIL_ACH_HELCARAXE,
    SIL_ACH_FIRE_AND_WATER,
    SIL_ACH_THE_FIRES_OF_THANGORODRIM,
    SIL_ACH_THROUGH_NAN_DUNGORTHEB,
    SIL_ACH_A_JOURNEY_IN_THE_DARK,
    SIL_ACH_A_MAP_OF_THE_IRON_HELLS,
    SIL_ACH_A_ELBERETH_GILTHONIEL,
    SIL_ACH_RELEASE_FROM_BONDAGE,
    SIL_ACH_TWO_THEMES_ONE_VOICE,
    SIL_ACH_YOU_CANNOT_PASS,
    SIL_ACH_FELAGUNDS_ANSWER,
    SIL_ACH_A_WILL_OF_YOUR_OWN,
    SIL_ACH_HURINS_DEFIANCE,
    SIL_ACH_NO_LONGER_BINDING,
    SIL_ACH_A_FOOLS_HOPE,
    SIL_ACH_A_SINGLE_SPARK,
    SIL_ACH_ON_THE_OTHER_HAND,
    SIL_ACH_IRON_CLEAVER,
    SIL_ACH_OUT_OF_THIN_ORE,
    SIL_ACH_CLAD_IN_THE_NIGHT,
    SIL_ACH_A_DREAM_UPON_THE_DARK_LORD,
    SIL_ACH_NEITHER_VOICE_DIMINISHED,
    SIL_ACH_DOOM_CAN_WAIT,
    SIL_ACH_THE_HIGH_CAPTAIN_FALLS,
    SIL_ACH_DAGNIR_GLAURUNGA,
    SIL_ACH_THE_ABHORRED,
    SIL_ACH_UNLIGHT_UNWOVEN,
    SIL_ACH_THE_DRAGON_HOST_BROKEN,
    SIL_ACH_THE_RED_MAW,
    SIL_ACH_LORD_OF_THE_FOUNTAIN,
    SIL_ACH_MASTER_OF_DOOM,
    SIL_ACH_FOR_IDRIL_AND_EARENDIL,
    SIL_ACH_BEFORE_THE_RISING_OF_THE_SUN,
    SIL_ACH_TULKAS_LAST_LAUGH,
    SIL_ACH_AULES_APPRENTICE,
    SIL_ACH_THE_DOOMSMANS_DUE,
    SIL_ACH_PITY_FOR_THE_FORSAKEN,
    SIL_ACH_THE_GREAT_HUNT,
    SIL_ACH_A_STAR_AGAINST_THE_BALROG,
    SIL_ACH_NO_STEPS_REQUIRED,
    SIL_ACH_THERE_AND_ALMOST_BACK_AGAIN,
    SIL_ACH_THE_HALLS_OF_UTUMNO,
    SIL_ACH_WHERE_GROND_WAS_WROUGHT,
    SIL_ACH_NOT_THE_DOOM_OF_THE_SILMARILS,
    SIL_ACH_THE_KING_WHO_KEPT_HIS_OATH,
    SIL_ACH_THURINGWETHIL_I_AM,
    SIL_ACH_HIS_DEATH_AND_FINAL_END,
    SIL_ACH_SEVEN_TIMES_HE_CRIED,
    SIL_ACH_THE_CAT_AND_THE_NECROMANCER,
    SIL_ACH_EVER_FOUND_AGAIN,
    SIL_ACH_THE_GOLDEN_FLOWER_STANDS,
    SIL_ACH_BELEGOST_REMEMBERS,
    SIL_ACH_BITER_AND_BEATER,
    SIL_ACH_AT_MOST_SEVEN,
    SIL_ACH_HEAVY_IS_THE_STOLEN_CROWN,
    SIL_ACH_THESE_THINGS_ARE_MINE,
    SIL_ACH_THE_OATH_HE_SHOULD_HAVE_SWORN,
    SIL_ACH_HIS_FATHERS_LIGHT,
    SIL_ACH_HEIRLOOMS_OF_THE_HIDDEN_KING,
    SIL_ACH_THE_RING_MAKER_OVERRULED,
    SIL_ACH_NO_PHIAL_REQUIRED,
    SIL_ACH_THREE_UNTOUCHED,
    SIL_ACH_A_BURGLARS_BUSINESS,
    SIL_ACH_STRONGBOW,
    SIL_ACH_ONE_JEWEL_IS_ENOUGH,
    SIL_ACH_A_DESPERATE_VICTORY,
    SIL_ACH_THEIR_WORK_HAD_NO_RIVAL,
    SIL_ACH_FIND_NAUGLAMIR,
    SIL_ACH_FIND_GURTHANG,
    SIL_ACH_FIND_MELKOR,
    SIL_ACH_FIND_GOTHMOG_AXE,
    SIL_ACH_FIND_ANGRIST,
    SIL_ACH_FIND_BELTHRONDING,
    SIL_ACH_FIND_IDRIL_ROBE,
    SIL_ACH_FIND_CELEBRIMBOR_GLOVES,
    SIL_ACH_FIND_MELIAN_RING,
    SIL_ACH_FIND_ULMO_CLOAK,
    SIL_ACH_FIND_FINGOLFIN_SHIELD,
    SIL_ACH_FIND_TURGON_STAFF,
    SIL_ACH_SWIFT_AS_THE_WIND,
    SIL_ACH_TENFOLD_DOOM,
    SIL_ACH_COUNT
} sil_achievement_id;

/* Steam omits these published Play achievements without moving their IDs. */
static inline bool sil_achievement_enabled(int id)
{
    if (id < 0 || id >= SIL_ACH_COUNT) return false;
#ifdef SIL_STEAMWORKS
    switch (id)
    {
#define SIL_ACH_STEAM_EXCLUDED(key) case SIL_ACH_##key:
#include "achievement-steam-exclusions.h"
#undef SIL_ACH_STEAM_EXCLUDED
        return false;
        default: break;
    }
#endif
    return true;
}
#ifdef SIL_STEAMWORKS
enum {
    SIL_ACH_STEAM_EXCLUDED_COUNT = 0
#define SIL_ACH_STEAM_EXCLUDED(key) + 1
#include "achievement-steam-exclusions.h"
#undef SIL_ACH_STEAM_EXCLUDED
};
#define SIL_ACH_ACTIVE_COUNT (SIL_ACH_COUNT - SIL_ACH_STEAM_EXCLUDED_COUNT)
#else
#define SIL_ACH_ACTIVE_COUNT SIL_ACH_COUNT
#endif

/* Platform masks are an array of words, preserving every existing bit/index. */
#define SIL_ACH_WORD_COUNT ((SIL_ACH_COUNT + 63) / 64)
static inline uint64_t sil_achievement_word_mask(unsigned word)
{
    if (word >= SIL_ACH_WORD_COUNT) return 0;
    unsigned remaining = SIL_ACH_COUNT - word * 64;
    uint64_t mask = remaining >= 64 ? UINT64_MAX : (UINT64_C(1) << remaining) - 1;
#ifdef SIL_STEAMWORKS
#define SIL_ACH_STEAM_EXCLUDED(key) \
    if (word == SIL_ACH_##key / 64) mask &= ~(UINT64_C(1) << (SIL_ACH_##key % 64));
#include "achievement-steam-exclusions.h"
#undef SIL_ACH_STEAM_EXCLUDED
#endif
    return mask;
}
#define SIL_ACH_RUN_STARTED 0x01u
#define SIL_ACH_RUN_TAINTED 0x02u
#define SIL_ACH_RUN_COMEBACK 0x04u
#define SIL_ACH_RUN_FALLING 0x08u
/* Positive provenance: only a newly tracked run starts with this bit. */
#define SIL_ACH_RUN_CHASM_SHAFT_ROUTE 0x10u
#define SIL_ACH_RUN_FLAGS 0x1Fu
#define SIL_ACH_DISC_TRACKED 0x01u
#define SIL_ACH_DISC_WOLF_SEEN 0x02u
#define SIL_ACH_DISC_CROSSING 0x04u
#define SIL_ACH_DISC_OWN_ICE 0x08u
#define SIL_ACH_DISC_WATER 0x10u
#define SIL_ACH_DISC_POISON 0x20u
#define SIL_ACH_DISC_LABYRINTH 0x40u
/* Five region visits survive level changes and saves, but never a new run.
 * These were unused bits in the existing saved u32b, so its layout is unchanged. */
#define SIL_ACH_DISC_REGIONS_SHIFT 7u
#define SIL_ACH_DISC_REGIONS 0xF80u
#define SIL_ACH_DISC_RUN_FLAGS (SIL_ACH_DISC_TRACKED | SIL_ACH_DISC_WOLF_SEEN | SIL_ACH_DISC_REGIONS)
#define SIL_ACH_DISC_FLAGS 0xFFFu

typedef struct sil_achievement_definition {
    const char *key;
    const char *title;
    const char *description;
    bool hidden;
} sil_achievement_definition;

const sil_achievement_definition *sil_achievement_get(int id);
bool sil_achievement_revealed(int id);
bool sil_achievement_progress_text(int id, char* out, size_t size);
uint64_t sil_achievement_unlocked_at(int id);
uint64_t sil_achievements_mask(unsigned word);
bool sil_achievements_eligible(void);
/* A winning killing blow can finish reporting after victory closes the run. */
bool sil_achievements_kill_eligible(void);
unsigned int sil_achievements_revision(void);
bool sil_achievements_saved(void);
void sil_achievements_init(void);
void sil_achievements_flush(void);
/* Called by the SDL event pump; platform responses are consumed on this thread. */
void sil_achievements_update(void);
void sil_achievements_on_depth(int depth);
/* Single, confirmed gameplay events; all eligibility checks remain here. */
void sil_achievements_on_event(sil_achievement_id id);
void sil_achievements_discovery_reset(bool new_run);
void sil_achievements_on_movement(int from_y, int from_x, int to_y, int to_x);
void sil_achievements_on_partition_entry(int y, int x);
void sil_achievements_on_partition_discovered(unsigned mask);
void sil_achievements_on_monster_seen(int m_idx);
void sil_achievements_on_terrain_seen(int y, int x);
void sil_achievements_on_wolf_death(bool first_wolf);
void sil_achievements_on_elemental_terrain(int y, int x, int old_feat, int typ);
void sil_achievements_forget_ice(int y, int x);
void sil_achievements_on_skill_check(bool player_attacks, int skill, int difficulty,
    int skill_sides, int difficulty_sides, bool cursed, int result);
void sil_achievements_song_begin(void);
void sil_achievements_on_song_effect(int song, int m_idx, int previous_alertness);
void sil_achievements_song_end(int cost);
void sil_achievements_on_door_jammed(int y, int x);
void sil_achievements_on_door_bashed(int y, int x);
void sil_achievements_forget_door(int y, int x);
void sil_achievements_on_fall(void);
void sil_achievements_on_silmaril(void);
void sil_achievements_on_escape(int silmarils);
void sil_achievements_begin_run(void);
void sil_achievements_on_player_turn(void);
void sil_achievements_on_singing_turn(void);
void sil_achievements_on_damage(int damage, int previous_hp);
void sil_achievements_on_healing(void);
void sil_achievements_on_level_entry(bool restoring);
void sil_achievements_on_level_travel(int from_depth, int to_depth, int feature);
void sil_achievements_on_death(void);
void sil_achievements_on_monster_killed(int m_idx, int who);
void sil_achievements_on_light_kill(void);
void sil_achievements_on_smithing_completed(bool custom_artefact);
void sil_achievements_on_artefact_created(int difficulty);
void sil_achievements_on_artefact_found(int artefact);
void sil_achievements_on_morgoth_defeated(void);
void sil_achievements_on_run_completed(void);
void sil_achievements_on_tale_completed(void);
void sil_achievements_on_tale_discarded(uint32_t id);
void sil_achievements_on_balrog_killed(int race);
void sil_achievements_show(void);
/* Acknowledged startup panel containing only the current hero's deeds. */
void sil_achievements_show_character_deeds(void);

#ifdef __cplusplus
}
#endif
#endif
