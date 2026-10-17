#include "angband.h"
#include "achievements.h"
#include "achievements-play-games.h"
#include "achievements-character.h"
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
    { "INTO_THE_DEEP", "To Dungeons Deep",
        "Reach a depth of 500 feet.", false },
    { "LIGHT_IN_DARKNESS", "The Fire of Fëanor",
        "Cut a Silmaril from Morgoth's Iron Crown.", true },
    { "BACK_TO_STARLIGHT", "The Quest of the Silmaril",
        "Escape Angband carrying at least one Silmaril.", false },
    { "THREE_LIGHTS_IN_EXILE", "Three Lights in Exile",
        "Escape carrying all three Silmarils.", true },
    { "THE_SHADOW_FALLS", "Dagor Dagorath",
        "Defeat Morgoth's final form.", true },
    { "THE_QUIET_ROAD", "A Very Polite Burglary",
        "Escape with a Silmaril without killing a creature during the run.", false },
    { "WORD_UNBROKEN", "When the Road Darkens",
        "Escape with a Silmaril while keeping your oath.", false },
    { "FORGED_BY_YOUR_HAND", "While Hammers Fell",
        "Complete a smithing project.", false },
    { "A_NAME_WORTH_REMEMBERING", "And Light They Caught",
        "Create a custom artefact.", true },
    { "SONG_BENEATH_THE_MOUNTAIN", "The Lay of Leithian",
        "Escape with a Silmaril, 20 trained Song and at least 100 turns spent singing.", false },
    { "THERE_AND_BACK_AGAIN", "There and Back Again",
        "Enter Morgoth's throne room and return to the Gates alive.", true },
    { "BY_A_THREAD", "Aurë Entuluva!",
        "Defeat an enemy after a hit leaves you at exactly 1 HP, before fully healing or leaving the floor.", true },
    { "LIGHT_OF_THE_ELDAR", "Get Out, You Old Wight!",
        "Defeat an undead enemy with a light-based attack.", true },
    { "THE_LONG_TALE", "Grew in the Telling",
        "Win a Tale with at least two different characters having completed adventures in it.", true },
    { "NO_RINGS_ATTACHED", "No Rings Attached",
        "Give a thrall the item they requested.", true },
    { "CABED_EN_ARAS", "Cabed-en-Aras",
        "Successfully leap over a chasm and land safely.", true },
    { "OFT_EVIL_WILL", "Oft Evil Will Shall Evil Mar",
        "Kill an enemy with a trap you rewired.", true },
    { "THE_HASTY_STROKE", "The Hasty Stroke Goes Oft Astray",
        "Kill an enemy with a Riposte.", true },
    { "A_FEW_LEAVES", "A Few Leaves in a Forest",
        "Kill an enemy with a Deadly Hail empowered shot.", true },
    { "TWO_WEAPONS", "Two Weapons, One Obituary",
        "Kill an enemy with a Power Throw in which both attacks hit.", true },
    { "FOR_EXTERNAL_USE_ONLY", "For External Use Only",
        "Kill an enemy with a thrown potion's effect, including its delayed poison.", true },
    { "FLY_YOU_FOOL", "Fly, You Fool!",
        "Knock a non-flying enemy into a chasm.", true },
    { "KNOCK_WITH_YOUR_HEAD", "Knock on the Doors with Your Head",
        "Jam a lock while picking it, then bash that same door open.", true },
    { "DOWN_TO_GOBLIN_TOWN", "Down, Down to Goblin-town",
        "Survive a fall through a false floor or into a chasm and arrive alive.", true },
    { "ATHELAS_OUT_OF_STOCK", "Athelas Was Out of Stock",
        "Cure a disease with an Herb of Sickness when it is the correct cure.", true },
    { "ANGBAND_MAINTENANCE", "Angband's Maintenance Department",
        "Witness an enemy dismantle a trap you rewired.", true },
    { "THE_ROAD_GOES_EVER_ON", "The Road Goes Ever On",
        "Reach a depth of 100 feet.", false },
    { "SOMETHING_TOOKISH", "Something Tookish",
        "Purchase your first ability with experience.", false },
    { "FLIES_AND_SPIDERS", "Flies and Spiders",
        "Break free of a web.", false },
    { "NOT_EVEN_CARCHAROTH", "Not Even Carcharoth",
        "Be killed by the first ordinary wolf you encounter before making your first kill.", true },
    { "THE_FORDS_OF_SIRION", "The Fords of Sirion",
        "Cross a stream from one dry bank to the other.", true },
    { "HELCARAXE", "Helcaraxë",
        "Freeze water yourself and cross the ice you created.", true },
    { "FIRE_AND_WATER", "Fire and Water",
        "Melt the ice beneath an enemy with your fire.", true },
    { "THE_FIRES_OF_THANGORODRIM", "The Fires of Thangorodrim",
        "Leap over lava and land alive after its heat.", true },
    { "THROUGH_NAN_DUNGORTHEB", "Through Nan Dungortheb",
        "Cross a poison pool and reach dry ground alive.", true },
    { "A_JOURNEY_IN_THE_DARK", "A Journey in the Dark",
        "Enter a labyrinth and leave through a different passage.", true },
    { "A_MAP_OF_THE_IRON_HELLS", "A Map of the Iron Hells",
        "Discover labyrinth, chasm, ice, fire and poison partitions within one Tale.", true },
    { "A_ELBERETH_GILTHONIEL", "A Elbereth Gilthoniel!",
        "Make an enemy flee with the Song of Elbereth.", true },
    { "RELEASE_FROM_BONDAGE", "Release from Bondage",
        "Clear rubble or disarm a trap with the Song of Freedom.", true },
    { "TWO_THEMES_ONE_VOICE", "Two Themes, One Voice",
        "Have both woven themes affect enemies in the same turn.", true },
    { "YOU_CANNOT_PASS", "You Cannot Pass",
        "Hold a door against a pursuer with the Song of Thresholds.", true },
    { "FELAGUNDS_ANSWER", "Felagund's Answer",
        "Permanently weaken an opponent with the Song of Contest.", true },
    { "A_WILL_OF_YOUR_OWN", "A Will of Your Own",
        "Resist a hostile effect in an opposed Will check.", false },
    { "HURINS_DEFIANCE", "Húrin's Defiance",
        "Use Indomitable to resist a hostile mental attack at a quarter health or less.", true },
    { "NO_LONGER_BINDING", "No Longer Binding",
        "Break an equipment curse with Curse Breaking.", true },
    { "A_FOOLS_HOPE", "A Fool's Hope",
        "Succeed at a genuine skill check whose chance was at most one in ten.", true },
    { "A_SINGLE_SPARK", "A Single Spark",
        "As Fëanor, create an artefact using a forge's last charge.", true },
    { "ON_THE_OTHER_HAND", "On the Other Hand",
        "As Maedhros, kill with a Subtlety critical using a hand-and-a-half weapon.", true },
    { "IRON_CLEAVER", "Iron-Cleaver",
        "As Telchar, forge a Sharpness II blade and cut a Silmaril with that same blade.", true },
    { "OUT_OF_THIN_ORE", "Out of Thin Ore",
        "As Gamil Zirak, forge mithril equipment without consuming mithril.", true },
    { "CLAD_IN_THE_NIGHT", "Clad in the Night",
        "As Eöl, forge and wear your own galvorn armour.", true },
    { "A_DREAM_UPON_THE_DARK_LORD", "A Dream Upon the Dark Lord",
        "As Lúthien, put Morgoth to sleep with the Song of Lórien.", true },
    { "NEITHER_VOICE_DIMINISHED", "Neither Voice Diminished",
        "As Daeron, have both full-strength woven themes affect enemies in the same turn.", true },
    { "DOOM_CAN_WAIT", "Doom Can Wait",
        "As Túrin, resist entrancement through your unique power and kill the attacker before the resulting rage ends.", true }
};

_Static_assert(SIL_ACH_COUNT < 64, "Achievement bridge masks must fit in 64 bits");

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

uint64_t sil_achievements_mask(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < SIL_ACH_COUNT; ++i)
        if (unlocked_at[i]) mask |= UINT64_C(1) << i;
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

bool sil_achievements_eligible(void) { return achievements_eligible(); }

bool sil_achievements_kill_eligible(void)
{
    /* monster_death() finalizes Morgoth victory before mon_take_hit() returns
     * to the weapon, trap or potion reporting the confirmed killing blow. */
    return achievements_eligible()
        || (achievement_run_eligible() && p_ptr->morgoth_slain
            && p_ptr->is_dead && !p_ptr->playing && p_ptr->leaving
            && !p_ptr->escaped);
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
    if (!achievements_eligible()) return;
    if (depth >= 2) unlock_achievement(SIL_ACH_THE_ROAD_GOES_EVER_ON, true);
    if (depth >= 10) unlock_achievement(SIL_ACH_INTO_THE_DEEP, true);
    sil_achievements_flush();
}

void sil_achievements_on_event(sil_achievement_id id)
{
    if (id < SIL_ACH_NO_RINGS_ATTACHED || id >= SIL_ACH_COUNT) return;
    switch (id)
    {
        case SIL_ACH_OFT_EVIL_WILL:
        case SIL_ACH_THE_HASTY_STROKE:
        case SIL_ACH_A_FEW_LEAVES:
        case SIL_ACH_TWO_WEAPONS:
        case SIL_ACH_FOR_EXTERNAL_USE_ONLY:
        case SIL_ACH_FLY_YOU_FOOL:
        case SIL_ACH_ON_THE_OTHER_HAND:
            if (!sil_achievements_kill_eligible()) return;
            break;
        default:
            if (!achievements_eligible()) return;
            break;
    }
    unlock_achievement(id, true);
    sil_achievements_flush();
}

static int achievement_door_index(int y, int x)
{
    if (y < 0 || x < 0 || y >= MAX_DUNGEON_HGT || x >= MAX_DUNGEON_WID)
        return -1;
    return y * MAX_DUNGEON_WID + x;
}

void sil_achievements_on_door_jammed(int y, int x)
{
    int index = achievement_door_index(y, x);
    if (!achievements_eligible() || index < 0) return;
    p_ptr->achievement_jammed_doors[index / 8] |= 1u << (index % 8);
}

void sil_achievements_forget_door(int y, int x)
{
    int index = achievement_door_index(y, x);
    if (p_ptr && index >= 0)
        p_ptr->achievement_jammed_doors[index / 8] &= ~(1u << (index % 8));
}

void sil_achievements_on_door_bashed(int y, int x)
{
    int index = achievement_door_index(y, x);
    if (!p_ptr || index < 0) return;
    if (p_ptr->achievement_jammed_doors[index / 8] & (1u << (index % 8)))
        sil_achievements_on_event(SIL_ACH_KNOCK_WITH_YOUR_HEAD);
    sil_achievements_forget_door(y, x);
}

void sil_achievements_on_fall(void)
{
    if (!achievements_eligible()) return;
    p_ptr->achievement_flags |= SIL_ACH_RUN_FALLING;
    p_ptr->achievement_fall_depth = p_ptr->depth;
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
    p_ptr->achievement_fall_depth = -1;
    memset(p_ptr->achievement_jammed_doors, 0,
        sizeof(p_ptr->achievement_jammed_doors));
    sil_achievements_discovery_reset(true);
    p_ptr->achievement_rage_epoch = 0;
    sil_achievements_song_begin();
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
    sil_achievement_character_on_player_turn();
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
    if (!restoring)
    {
        p_ptr->achievement_flags &= ~SIL_ACH_RUN_COMEBACK;
        memset(p_ptr->achievement_jammed_doors, 0,
            sizeof(p_ptr->achievement_jammed_doors));
        sil_achievements_discovery_reset(false);
    }
    if (p_ptr->achievement_flags & SIL_ACH_RUN_FALLING)
    {
        if (p_ptr->achievement_fall_depth >= 0
            && p_ptr->depth > p_ptr->achievement_fall_depth)
            sil_achievements_on_event(SIL_ACH_DOWN_TO_GOBLIN_TOWN);
        p_ptr->achievement_flags &= ~SIL_ACH_RUN_FALLING;
        p_ptr->achievement_fall_depth = -1;
    }
    if (achievements_eligible() && !p_ptr->depth && p_ptr->morgoth_hall_entered)
    {
        unlock_achievement(SIL_ACH_THERE_AND_BACK_AGAIN, true);
        sil_achievements_flush();
    }
    sil_achievements_on_partition_entry(p_ptr->py, p_ptr->px);
}

void sil_achievements_on_monster_killed(int m_idx, int who)
{
    if (!achievements_eligible() || who >= 0) return;
    sil_achievement_character_on_monster_killed(m_idx, who);
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

void sil_achievements_on_partition_discovered(unsigned mask)
{
    if (!achievements_eligible() || run_mode_is_blitz()) return;
    sil_achievements_init();
    cJSON *tale = current_tale_characters(true);
    if (!tale) return;
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(tale, "_partitions");
    unsigned old = cJSON_IsNumber(entry) && entry->valuedouble >= 0
        && entry->valuedouble <= 31 ? (unsigned)entry->valuedouble : 0;
    unsigned discovered = old | (mask & 31u);
    if (discovered != old)
    {
        if (entry) cJSON_SetNumberValue(entry, discovered);
        else if (!cJSON_AddNumberToObject(tale, "_partitions", discovered)) return;
        dirty = true;
    }
    if (discovered == 31u)
        unlock_achievement(SIL_ACH_A_MAP_OF_THE_IRON_HELLS, true);
    sil_achievements_flush();
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
    uint64_t mask = sil_achievements_mask();
    sil_steamworks_sync_achievements(mask);
    sil_play_games_sync(mask);
    uint64_t remote = sil_steamworks_achievements_unlocked()
        | sil_play_games_unlocked();
    for (int i = 0; i < SIL_ACH_COUNT; ++i)
        if (remote & (UINT64_C(1) << i)) unlock_achievement(i, false);
    if (dirty && now >= next_save) sil_achievements_flush();
}
