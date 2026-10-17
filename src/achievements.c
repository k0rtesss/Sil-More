#include "angband.h"
#include "achievements.h"
#include "achievements-play-games.h"
#include "blitz.h"
#include "cJSON.h"
#include "externs.h"
#include "fs/path.h"
#include "log/log.h"
#include "metarun.h"
#include "steamworks.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

/* Read older profiles whose optional Tale history exceeded the original 64 KiB
 * limit. Keep newly written profiles within this same bound. */
#define ACHIEVEMENTS_MAX_FILE (4 * 1024 * 1024)
#define ACHIEVEMENTS_TALE_BUDGET (48 * 1024)

static const sil_achievement_definition catalogue[SIL_ACH_COUNT] = {
    { "INTO_THE_DEEP", "Into the Deep", "Reach a depth of 500 feet." },
    { "LIGHT_IN_DARKNESS", "A Light in the Darkness",
        "Cut a Silmaril from Morgoth's Iron Crown." },
    { "BACK_TO_STARLIGHT", "Back to the Starlight",
        "Escape Angband carrying at least one Silmaril." },
    { "THREE_LIGHTS_IN_EXILE", "Three Lights in Exile", "Escape carrying all three Silmarils." },
    { "THE_SHADOW_FALLS", "The Shadow Falls", "Defeat Morgoth's final form." },
    { "THE_QUIET_ROAD", "The Quiet Road", "Escape with a Silmaril without killing a creature during the run." },
    { "WORD_UNBROKEN", "Word Unbroken", "Escape with a Silmaril while keeping your oath." },
    { "FORGED_BY_YOUR_HAND", "Forged by Your Hand", "Complete a smithing project." },
    { "A_NAME_WORTH_REMEMBERING", "A Name Worth Remembering", "Create a custom artefact." },
    { "SONG_BENEATH_THE_MOUNTAIN", "Song Beneath the Mountain", "Escape with a Silmaril, 20 trained Song and at least 100 turns spent singing." },
    { "THERE_AND_BACK_AGAIN", "There and Back Again", "Enter Morgoth's throne room and return to the Gates alive." },
    { "BY_A_THREAD", "By a Thread", "Defeat an enemy after a hit leaves you at exactly 1 HP, before fully healing or leaving the floor." },
    { "LIGHT_OF_THE_ELDAR", "Light of the Eldar", "Defeat an undead enemy with a light-based attack." },
    { "THE_LONG_TALE", "The Long Tale", "Win a Tale with at least two different characters having completed adventures in it." }
};

_Static_assert(SIL_ACH_COUNT < 32, "Achievement bridge masks must fit in 32 bits");

static uint64_t unlocked_at[SIL_ACH_COUNT];
static cJSON *progress;
static char progress_path[1024];
static bool initialized;
static bool writable;
static bool dirty;
static unsigned int revision;
static Uint64 next_sync;
static Uint64 next_save;

const sil_achievement_definition *sil_achievement_get(int id)
{
    return id >= 0 && id < SIL_ACH_COUNT ? &catalogue[id] : NULL;
}

uint64_t sil_achievement_unlocked_at(int id)
{
    return id >= 0 && id < SIL_ACH_COUNT ? unlocked_at[id] : 0;
}

unsigned int sil_achievements_mask(void)
{
    unsigned int mask = 0;
    for (int i = 0; i < SIL_ACH_COUNT; ++i)
        if (unlocked_at[i]) mask |= 1u << i;
    return mask;
}

unsigned int sil_achievements_revision(void) { return revision; }
bool sil_achievements_saved(void) { return initialized && writable && !dirty; }

static uint64_t achievement_time(void)
{
    time_t now = time(NULL);
    return now > 0 ? (uint64_t)now : 1;
}

static cJSON *read_progress(void)
{
    SDL_IOStream *file = SDL_IOFromFile(progress_path, "rb");
    SDL_PathInfo info;
    cJSON *root = NULL;
    if (!file)
    {
        /* A missing file is normal on first launch. Preserve unreadable files. */
        if (SDL_GetPathInfo(progress_path, &info)) writable = false;
        return NULL;
    }
    Sint64 size = SDL_GetIOSize(file);
    if (size > 0 && size <= ACHIEVEMENTS_MAX_FILE)
    {
        char *data = malloc((size_t)size + 1);
        if (data)
        {
            if (SDL_ReadIO(file, data, (size_t)size) == (size_t)size)
            {
                data[size] = '\0';
                root = cJSON_ParseWithOpts(data, NULL, true);
            }
            free(data);
        }
    }
    SDL_CloseIO(file);
    if (!root) writable = false;
    return root;
}

void sil_achievements_init(void)
{
    if (initialized || !ANGBAND_DIR_APEX || !ANGBAND_DIR_APEX[0]) return;
    if (!path_build(progress_path, sizeof(progress_path), ANGBAND_DIR_APEX,
            "achievements.json")) return;
    initialized = true;
    writable = SDL_CreateDirectory(ANGBAND_DIR_APEX);
    progress = read_progress();
    if (progress)
    {
        cJSON *version = cJSON_GetObjectItemCaseSensitive(progress, "version");
        cJSON *unlocked = cJSON_GetObjectItemCaseSensitive(progress, "unlocked");
        if (!cJSON_IsObject(progress) || !cJSON_IsNumber(version)
            || version->valuedouble != 1 || !cJSON_IsObject(unlocked))
            writable = false;
        else
        {
            for (int i = 0; i < SIL_ACH_COUNT; ++i)
            {
                const cJSON *entry = cJSON_GetObjectItemCaseSensitive(
                    unlocked, catalogue[i].key);
                if (cJSON_IsNumber(entry) && isfinite(entry->valuedouble)
                    && entry->valuedouble >= 1
                    && entry->valuedouble <= 253402300799.0)
                    unlocked_at[i] = (uint64_t)entry->valuedouble;
            }
        }
    }
    else if (writable)
    {
        progress = cJSON_CreateObject();
        if (!progress || !cJSON_AddNumberToObject(progress, "version", 1)
            || !cJSON_AddObjectToObject(progress, "unlocked"))
            writable = false;
    }
    if (!writable)
        log_warn("Achievement progress cannot be written; preserving '%s'", progress_path);
}

static void trim_tale_history(void)
{
    cJSON *tales = cJSON_GetObjectItemCaseSensitive(progress, "tales");
    if (!cJSON_IsObject(tales)) return;
    char *data = cJSON_PrintUnformatted(tales);
    if (!data) return;
    size_t size = strlen(data);
    cJSON_free(data);
    char current[32];
    strnfmt(current, sizeof(current), "%u", (unsigned)metar.id);
    /* Old profiles can contain failed/abandoned Tales from before lifecycle
     * cleanup. Drop oldest optional records under pressure, retaining the
     * current Tale's exact character set. Never infer completed adventures. */
    cJSON *entry = tales->child;
    int discarded = 0;
    while (entry && size > ACHIEVEMENTS_TALE_BUDGET)
    {
        cJSON *next = entry->next;
        if (entry->string && strcmp(entry->string, current))
        {
            data = cJSON_PrintUnformatted(entry);
            if (!data) return;
            /* Escaping can only increase the actual key size, so this is a
             * conservative reduction of the remaining serialized size. */
            size_t removed = strlen(data) + strlen(entry->string) + 4;
            cJSON_free(data);
            size = removed < size ? size - removed : 0;
            cJSON_Delete(cJSON_DetachItemViaPointer(tales, entry));
            ++discarded;
        }
        entry = next;
    }
    if (discarded)
        log_warn("Discarded %d old optional achievement Tale records to bound profile size", discarded);
}

void sil_achievements_flush(void)
{
    if (!initialized || !writable || !dirty || !progress) return;
    cJSON *unlocked = cJSON_GetObjectItemCaseSensitive(progress, "unlocked");
    for (int i = 0; i < SIL_ACH_COUNT; ++i)
    {
        if (!unlocked_at[i]) continue;
        cJSON *entry = cJSON_CreateNumber((double)unlocked_at[i]);
        if (!entry) return;
        if (cJSON_HasObjectItem(unlocked, catalogue[i].key))
            cJSON_ReplaceItemInObjectCaseSensitive(unlocked, catalogue[i].key, entry);
        else
            cJSON_AddItemToObject(unlocked, catalogue[i].key, entry);
    }
    trim_tale_history();
    char *data = cJSON_PrintUnformatted(progress);
    if (!data) return;
    if (strlen(data) > ACHIEVEMENTS_MAX_FILE)
    {
        cJSON_free(data);
        next_save = SDL_GetTicks() + 30000;
        log_warn("Achievement profile exceeds the supported size; preserving the previous file");
        return;
    }
    char temporary[1100];
    strnfmt(temporary, sizeof(temporary), "%s.tmp", progress_path);
    SDL_IOStream *file = SDL_IOFromFile(temporary, "wb");
    bool ok = file && SDL_WriteIO(file, data, strlen(data)) == strlen(data);
    if (file && !SDL_FlushIO(file)) ok = false;
    if (file && !SDL_CloseIO(file)) ok = false;
    cJSON_free(data);
    if (ok) ok = SDL_RenamePath(temporary, progress_path);
    if (ok)
    {
        dirty = false;
        ++revision;
    }
    else
    {
        SDL_RemovePath(temporary);
        log_warn("Could not save achievements to '%s': %s", progress_path, SDL_GetError());
    }
    next_save = SDL_GetTicks() + 30000;
}

static bool achievement_run_eligible(void)
{
    if (!p_ptr || !op_ptr || !character_generated
        || p_ptr->game_type != 0 || p_ptr->wizard
        || (p_ptr->achievement_flags & SIL_ACH_RUN_TAINTED)
        || (p_ptr->noscore & 0x000F)) return false;
    /* Match the score system's persistent cheat history, and active debug aids. */
    for (int i = OPT_CHEAT; i < OPT_ADULT; ++i)
        if (op_ptr->opt[i]) return false;
    for (int i = OPT_SCORE; i < OPT_MAX; ++i)
        if (op_ptr->opt[i]) return false;
    return true;
}

static bool achievements_eligible(void)
{
    return achievement_run_eligible() && p_ptr->playing && !p_ptr->is_dead;
}

static void unlock_achievement(int id, bool announce)
{
    sil_achievements_init();
    if (!initialized || id < 0 || id >= SIL_ACH_COUNT || unlocked_at[id]) return;
    unlocked_at[id] = achievement_time();
    dirty = true;
    ++revision;
    next_sync = 0;
    log_info("Achievement unlocked: %s", catalogue[id].key);
    if (announce)
    {
        char text[160];
        strnfmt(text, sizeof(text), "Achievement unlocked: %s", catalogue[id].title);
        msg_print(text);
        sdl_popup_notification_show(text);
    }
}

void sil_achievements_on_depth(int depth)
{
    if (!achievements_eligible() || depth < 10) return;
    unlock_achievement(SIL_ACH_INTO_THE_DEEP, true);
    sil_achievements_flush();
}

void sil_achievements_on_silmaril(void)
{
    if (!achievements_eligible()) return;
    unlock_achievement(SIL_ACH_LIGHT_IN_DARKNESS, true);
    sil_achievements_flush();
}

void sil_achievements_on_escape(int silmarils)
{
    if (!achievements_eligible() || p_ptr->depth != 0 || silmarils < 1) return;
    unlock_achievement(SIL_ACH_BACK_TO_STARLIGHT, true);
    if (silmarils == 3) unlock_achievement(SIL_ACH_THREE_LIGHTS_IN_EXILE, true);
    if ((p_ptr->achievement_flags & SIL_ACH_RUN_STARTED) && !p_ptr->achievement_kills)
        unlock_achievement(SIL_ACH_THE_QUIET_ROAD, true);
    if (p_ptr->oath_type > 0 && !oath_invalid(p_ptr->oath_type))
        unlock_achievement(SIL_ACH_WORD_UNBROKEN, true);
    if (p_ptr->skill_base[S_SNG] >= 20 && p_ptr->achievement_singing_turns >= 100)
        unlock_achievement(SIL_ACH_SONG_BENEATH_THE_MOUNTAIN, true);
    if (p_ptr->morgoth_hall_entered)
        unlock_achievement(SIL_ACH_THERE_AND_BACK_AGAIN, true);
    sil_achievements_flush();
}

void sil_achievements_begin_run(void)
{
    p_ptr->achievement_flags = SIL_ACH_RUN_STARTED;
    p_ptr->achievement_kills = 0;
    p_ptr->achievement_singing_turns = 0;
    p_ptr->achievement_comeback_depth = -1;
}

void sil_achievements_on_healing(void)
{
    if (p_ptr && p_ptr->chp >= p_ptr->mhp)
        p_ptr->achievement_flags &= ~SIL_ACH_RUN_COMEBACK;
}

void sil_achievements_on_player_turn(void)
{
    if (!p_ptr || !p_ptr->playing || p_ptr->is_dead || p_ptr->energy_use <= 0) return;
    if (!achievement_run_eligible())
    {
        p_ptr->achievement_flags |= SIL_ACH_RUN_TAINTED;
        return;
    }
    sil_achievements_on_healing();
}

void sil_achievements_on_singing_turn(void)
{
    if (!achievements_eligible() || p_ptr->energy_use <= 0) return;
    if (p_ptr->song1 < SNG_MAX && p_ptr->achievement_singing_turns < UINT32_MAX)
        ++p_ptr->achievement_singing_turns;
}

void sil_achievements_on_damage(int damage, int previous_hp)
{
    if (!achievements_eligible() || damage <= 0) return;
    if (previous_hp - damage <= 0)
        p_ptr->achievement_flags &= ~SIL_ACH_RUN_COMEBACK;
    else if (previous_hp > 1 && previous_hp - damage == 1 && p_ptr->chp == 1)
    {
        p_ptr->achievement_flags |= SIL_ACH_RUN_COMEBACK;
        p_ptr->achievement_comeback_depth = p_ptr->depth;
    }
}

void sil_achievements_on_level_entry(bool restoring)
{
    if (!restoring) p_ptr->achievement_flags &= ~SIL_ACH_RUN_COMEBACK;
    if (achievements_eligible() && !p_ptr->depth && p_ptr->morgoth_hall_entered)
    {
        unlock_achievement(SIL_ACH_THERE_AND_BACK_AGAIN, true);
        sil_achievements_flush();
    }
}

void sil_achievements_on_monster_killed(int m_idx, int who)
{
    (void)m_idx; /* Event owner has already confirmed the death, before deletion. */
    if (!achievements_eligible() || who >= 0) return;
    if (p_ptr->achievement_kills < UINT32_MAX) ++p_ptr->achievement_kills;
    if ((p_ptr->achievement_flags & SIL_ACH_RUN_COMEBACK)
        && p_ptr->achievement_comeback_depth == p_ptr->depth
        && p_ptr->chp > 0 && p_ptr->chp < p_ptr->mhp)
        unlock_achievement(SIL_ACH_BY_A_THREAD, true);
    p_ptr->achievement_flags &= ~SIL_ACH_RUN_COMEBACK;
    sil_achievements_flush();
}

void sil_achievements_on_light_kill(void)
{
    if (!achievements_eligible()) return;
    unlock_achievement(SIL_ACH_LIGHT_OF_THE_ELDAR, true);
    sil_achievements_flush();
}

void sil_achievements_on_smithing_completed(bool custom_artefact)
{
    if (!achievements_eligible()) return;
    unlock_achievement(SIL_ACH_FORGED_BY_YOUR_HAND, true);
    if (custom_artefact) unlock_achievement(SIL_ACH_A_NAME_WORTH_REMEMBERING, true);
    sil_achievements_flush();
}

void sil_achievements_on_morgoth_defeated(void)
{
    if (!achievements_eligible()) return;
    unlock_achievement(SIL_ACH_THE_SHADOW_FALLS, true);
    sil_achievements_flush();
}

/* Distinct character templates, scoped to the local Tale, make repeats and
 * Eru resurrection idempotent without counting living snapshots as adventures. */
static cJSON *current_tale_characters(bool create)
{
    if (!progress || !writable) return NULL;
    cJSON *tales = cJSON_GetObjectItemCaseSensitive(progress, "tales");
    if (!tales && create) tales = cJSON_AddObjectToObject(progress, "tales");
    if (!cJSON_IsObject(tales)) return NULL;
    char key[32];
    strnfmt(key, sizeof(key), "%u", (unsigned)metar.id);
    cJSON *characters = cJSON_GetObjectItemCaseSensitive(tales, key);
    if (!characters && create) characters = cJSON_AddObjectToObject(tales, key);
    return cJSON_IsObject(characters) ? characters : NULL;
}

void sil_achievements_on_run_completed(void)
{
    if (!achievement_run_eligible() || !p_ptr->is_dead || run_mode_is_blitz()) return;
    sil_achievements_init();
    if (unlocked_at[SIL_ACH_THE_LONG_TALE]) return;
    cJSON *characters = current_tale_characters(true);
    char key[32];
    strnfmt(key, sizeof(key), "%u:%u", (unsigned)p_ptr->prace, (unsigned)p_ptr->pcharacter);
    if (!characters || cJSON_HasObjectItem(characters, key)) return;
    if (cJSON_AddBoolToObject(characters, key, true)) dirty = true;
    sil_achievements_flush();
}

void sil_achievements_on_tale_completed(void)
{
    if (run_mode_is_blitz()) return;
    cJSON *characters = current_tale_characters(false);
    int completed = 0;
    for (const cJSON *entry = characters ? characters->child : NULL; entry; entry = entry->next)
        if (cJSON_IsTrue(entry)) ++completed;
    if (completed >= 2 && achievement_run_eligible())
    {
        unlock_achievement(SIL_ACH_THE_LONG_TALE, true);
    }
    sil_achievements_on_tale_discarded(metar.id);
}

void sil_achievements_on_tale_discarded(uint32_t id)
{
    sil_achievements_init();
    /* Finished Tales no longer need per-character bookkeeping. */
    cJSON *tales = progress ? cJSON_GetObjectItemCaseSensitive(progress, "tales") : NULL;
    if (cJSON_IsObject(tales))
    {
        char key[32];
        strnfmt(key, sizeof(key), "%u", (unsigned)id);
        cJSON_DeleteItemFromObjectCaseSensitive(tales, key);
        dirty = true;
    }
    sil_achievements_flush();
}

void sil_achievements_update(void)
{
    if (!initialized) return;
    Uint64 now = SDL_GetTicks();
    if (now < next_sync) return;
    next_sync = now + 1000;
    unsigned int mask = sil_achievements_mask();
    sil_steamworks_sync_achievements(mask);
    sil_play_games_sync(mask);
    unsigned int remote = sil_steamworks_achievements_unlocked()
        | sil_play_games_unlocked();
    for (int i = 0; i < SIL_ACH_COUNT; ++i)
        if (remote & (1u << i)) unlock_achievement(i, false);
    if (dirty && now >= next_save) sil_achievements_flush();
}
