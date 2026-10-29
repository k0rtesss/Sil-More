#include "angband.h"
#include "achievements.h"
#include "achievements-play-games.h"
#include "achievements-game-center.h"
#include "achievements-character.h"
#include "achievements-lore.h"
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
        "Cut a Silmaril from Morgoth's Iron Crown.", false },
    { "BACK_TO_STARLIGHT", "The Quest of the Silmaril",
        "Escape Angband carrying at least one Silmaril.", false },
    { "THREE_LIGHTS_IN_EXILE", "Three Lights in Exile",
        "Escape carrying all three Silmarils.", false },
    { "THE_SHADOW_FALLS", "Dagor Dagorath",
        "Defeat Morgoth's final form.", true },
    { "THE_QUIET_ROAD", "Not to Strike Without Need",
        "Escape with a Silmaril without killing a creature during the adventure. Your traps and poison count.", false },
    { "WORD_UNBROKEN", "When the Road Darkens",
        "Escape with a Silmaril after choosing an oath and keeping it unbroken.", false },
    { "FORGED_BY_YOUR_HAND", "While Hammers Fell",
        "Complete a smithing project.", false },
    { "A_NAME_WORTH_REMEMBERING", "And Light They Caught",
        "Create a custom artefact.", false },
    { "SONG_BENEATH_THE_MOUNTAIN", "The Lay of Leithian",
        "Escape with a Silmaril and at least 20 trained Song.", false },
    { "THERE_AND_BACK_AGAIN", "There and Back for Nothing",
        "Enter Morgoth's throne room and return to the Gates alive without carrying any Silmarils.", true },
    { "BY_A_THREAD", "Aurë Entuluva!",
        "Kill an enemy while you have exactly 1 HP.", true },
    { "LIGHT_OF_THE_ELDAR", "Get Out, You Old Wight!",
        "Defeat an undead enemy with a light-based attack.", true },
    { "THE_LONG_TALE", "Grew in the Telling",
        "Win a Tale with at least two different characters having completed adventures in it.", false },
    { "NO_RINGS_ATTACHED", "No Rings Attached",
        "Give a thrall the item they requested.", false },
    { "CABED_EN_ARAS", "Cabed-en-Aras",
        "Successfully leap over a chasm and land safely.", false },
    { "OFT_EVIL_WILL", "Oft Evil Will Shall Evil Mar",
        "Kill an enemy with a trap you rewired.", false },
    { "THE_HASTY_STROKE", "The Hasty Stroke Goes Oft Astray",
        "Kill an enemy with a Riposte.", false },
    { "A_FEW_LEAVES", "A Few Leaves in a Forest",
        "Kill an enemy with a Deadly Hail empowered shot.", false },
    { "TWO_WEAPONS", "Two Blows, One Doom",
        "Kill an enemy with a Power Throw in which both attacks hit.", false },
    { "FOR_EXTERNAL_USE_ONLY", "For External Use Only",
        "Kill an enemy with a thrown potion's effect, including its delayed poison.", false },
    { "FLY_YOU_FOOL", "Fly, You Fool!",
        "Knock a non-flying enemy into a chasm. It need not die.", true },
    { "KNOCK_WITH_YOUR_HEAD", "Knock on the Doors with Your Head",
        "Jam a lock while picking it, then bash that same door open.", true },
    { "DOWN_TO_GOBLIN_TOWN", "Down, Down to Goblin-town",
        "Survive a fall through a false floor or down a chasm.", true },
    { "ATHELAS_OUT_OF_STOCK", "A Bitter Cure",
        "Cure a disease with an Herb of Sickness when it is the correct cure.", true },
    { "ANGBAND_MAINTENANCE", "Angband's Maintenance Department",
        "Witness an enemy dismantle a trap you rewired.", true },
    { "THE_ROAD_GOES_EVER_ON", "The Road Goes Ever On",
        "Reach a depth of 150 feet.", false },
    { "SOMETHING_TOOKISH", "Something Tookish",
        "Purchase your first ability with experience.", false },
    { "FLIES_AND_SPIDERS", "Flies and Spiders",
        "Break free of a web.", false },
    { "NOT_EVEN_CARCHAROTH", "Not Even Carcharoth",
        "Be killed by the first ordinary wolf you encounter before making your first kill.", true },
    { "THE_FORDS_OF_SIRION", "The Fords of Sirion",
        "Cross at least three water tiles from one dry bank to the other.", false },
    { "HELCARAXE", "Helcaraxë",
        "Freeze water with a cold-branded attack, then cross your ice from one bank to the other.", true },
    { "FIRE_AND_WATER", "Fire and Water",
        "Melt the ice beneath an enemy with fire.", true },
    { "THE_FIRES_OF_THANGORODRIM", "The Fires of Thangorodrim",
        "Leap across lava and survive its heat.", false },
    { "THROUGH_NAN_DUNGORTHEB", "Through Nan Dungortheb",
        "Cross at least three poison tiles from one dry bank to the other and survive.", false },
    { "A_JOURNEY_IN_THE_DARK", "A Journey in the Dark",
        "Enter a labyrinth and leave through a different passage.", false },
    { "A_MAP_OF_THE_IRON_HELLS", "A Map of the Iron Hells",
        "Enter labyrinth, chasm, ice, fire and poison regions within one run.", false },
    { "A_ELBERETH_GILTHONIEL", "A Elbereth Gilthoniel!",
        "Make an enemy flee with the Song of Elbereth.", false },
    { "RELEASE_FROM_BONDAGE", "Release from Bondage",
        "Clear rubble or disarm a trap with the Song of Freedom.", false },
    { "TWO_THEMES_ONE_VOICE", "Two Themes, One Voice",
        "Have both woven themes affect enemies in the same turn.", false },
    { "YOU_CANNOT_PASS", "You Cannot Pass",
        "Hold a door against a pursuer with the Song of Thresholds.", false },
    { "FELAGUNDS_ANSWER", "Felagund's Answer",
        "Permanently weaken an opponent with the Song of Contest.", false },
    { "A_WILL_OF_YOUR_OWN", "A Will of Your Own",
        "Resist a hostile effect in an opposed Will check.", false },
    { "HURINS_DEFIANCE", "Húrin's Defiance",
        "Resist a hostile mental attack at 25% HP or less when Indomitable makes the difference.", true },
    { "NO_LONGER_BINDING", "No Longer Binding",
        "Break an equipment curse with Curse Breaking.", false },
    { "A_FOOLS_HOPE", "A Fool's Hope",
        "Succeed at a skill check with a success chance of 10% or less.", true },
    { "A_SINGLE_SPARK", "A Single Spark",
        "As Fëanor, create an artefact using a forge's last charge. “His spirit burned as a flame.”", false },
    { "ON_THE_OTHER_HAND", "More Deadly Than His Right",
        "As Maedhros, kill with a Subtlety critical using a hand-and-a-half weapon.", false },
    { "IRON_CLEAVER", "Iron-Cleaver",
        "As Telchar, forge a blade that ignores all armour and cut a Silmaril with that same blade.", false },
    { "OUT_OF_THIN_ORE", "Out of Thin Ore",
        "As Gamil Zirak, forge mithril equipment without consuming mithril.", false },
    { "CLAD_IN_THE_NIGHT", "Clad in the Night",
        "As Eöl, forge and wear your own galvorn armour.", false },
    { "A_DREAM_UPON_THE_DARK_LORD", "A Dream Upon the Dark Lord",
        "As Lúthien, put Morgoth to sleep with the Song of Lórien.", false },
    { "NEITHER_VOICE_DIMINISHED", "Neither Voice Diminished",
        "As Daeron, have both full-strength woven themes affect enemies in the same turn.", false },
    { "DOOM_CAN_WAIT", "Doom Can Wait",
        "As Túrin, resist entrancement through your unique power and kill the attacker before the resulting rage ends.", false },
    { "THE_HIGH_CAPTAIN_FALLS", "The High Captain Falls",
        "Slay Gothmog, High Captain of Balrogs.", true },
    { "DAGNIR_GLAURUNGA", "Dagnir Glaurunga",
        "Slay Glaurung, the Deceiver.", true },
    { "THE_ABHORRED", "The Abhorred",
        "Slay Gorthaur, Servant of Morgoth.", true },
    { "UNLIGHT_UNWOVEN", "Unlight Unwoven",
        "Slay Ungoliant, the Gloomweaver.", true },
    { "THE_DRAGON_HOST_BROKEN", "The Mightiest of the Dragon-host",
        "Slay Ancalagon the Black.", true },
    { "THE_RED_MAW", "The Red Maw",
        "Slay Carcharoth, the Jaws of Thirst.", true },
    { "LORD_OF_THE_FOUNTAIN", "Lord of the Fountain",
        "As Ecthelion, slay Gothmog.", false },
    { "MASTER_OF_DOOM", "Master of Doom",
        "As Túrin Turambar, slay Glaurung.", false },
    { "FOR_IDRIL_AND_EARENDIL", "For Idril and Eärendil",
        "As Tuor, slay Maeglin, Betrayer of Gondolin.", false },
    { "BEFORE_THE_RISING_OF_THE_SUN", "Before the Rising of the Sun",
        "As Eärendil, slay Ancalagon the Black.", false },
    { "TULKAS_LAST_LAUGH", "Tulkas Has the Last Laugh",
        "Slay the unique enemy named by Tulkas and collect his artefact reward.", true },
    { "AULES_APPRENTICE", "Apprentice of the Maker",
        "Forge an item of difficulty 21 or higher for Aulë and receive his blessing.", true },
    { "THE_DOOMSMANS_DUE", "The Doomsman's Due",
        "Release Brodda's bound spirit for Mandos and receive his blessing.", true },
    { "PITY_FOR_THE_FORSAKEN", "Pity for the Forsaken",
        "Reach a down stair or shaft without a creature dying during Nienna's trial and receive her blessing.", true },
    { "THE_GREAT_HUNT", "The Great Hunt",
        "Complete Oromë's hunt and receive his blessing.", true },
    { "A_STAR_AGAINST_THE_BALROG", "A Star Against the Balrog",
        "Slay Duruin in his Bastion for Varda and choose a radiant gift.", true },
    { "NO_STEPS_REQUIRED", "No Steps Required",
        "From the start of the adventure, reach Morgoth's level using only chasms and down shafts for descent. Upward stairs are allowed.", false },
    { "THERE_AND_ALMOST_BACK_AGAIN", "There and Almost Back Again",
        "Die at the Gates of Angband, on the surface.", true },
    { "THE_HALLS_OF_UTUMNO", "The Halls of Utumno",
        "Reach the frozen ruins of Utumno.", true },
    { "WHERE_GROND_WAS_WROUGHT", "Where Grond Was Wrought",
        "Discover Orodruth, the Great Forge of Grond in Utumno.", true },
    { "NOT_THE_DOOM_OF_THE_SILMARILS", "Not the Doom of the Silmarils",
        "Have Angrist shatter while trying to cut the second Silmaril.", true },
    { "THE_KING_WHO_KEPT_HIS_OATH", "The King Who Kept His Oath",
        "As Finrod Felagund, kill a werewolf with an unarmed attack.", false },
    { "THURINGWETHIL_I_AM", "Thuringwethil I Am",
        "As Lúthien, enter Morgoth's throne room wearing Thuringwethil's Bat-Fell.", false },
    { "HIS_DEATH_AND_FINAL_END", "His Death and Final End",
        "As Túrin, slay Morgoth's final form with Gurthang.", false },
    { "SEVEN_TIMES_HE_CRIED", "Seven Times He Cried",
        "Land at least seven damaging blows on Morgoth with Ringil, then escape with a Silmaril while he lives.", false },
    { "THE_CAT_AND_THE_NECROMANCER", "The Cat and the Necromancer",
        "Defeat Tevildo and Gorthaur in the same adventure.", true },
    { "EVER_FOUND_AGAIN", "Ever Found Again",
        "Kill with Dailir, recover the arrow, and kill another enemy with it.", true },
    { "THE_GOLDEN_FLOWER_STANDS", "The Golden Flower Stands",
        "As Glorfindel, slay a Balrog in close quarters beside a chasm, then leave the level alive.", false },
    { "BELEGOST_REMEMBERS", "Belegost Remembers",
        "As a Dwarf, slay Glaurung with Azaghâl's dagger.", false },
    { "BITER_AND_BEATER", "Biter and Beater",
        "Make an orc flee while wielding Orcrist and Glamdring together.", true },
    { "AT_MOST_SEVEN", "At Most Seven",
        "Slay all seven named Balrogs within one adventure.", false },
    { "HEAVY_IS_THE_STOLEN_CROWN", "Heavy Is the Stolen Crown",
        "Escape with Morgoth's Iron Crown and at least one Silmaril, loose or still set in the Crown.", true },
    { "THESE_THINGS_ARE_MINE", "These Things Are Mine",
        "As Fëanor, escape with all three Silmarils.", false },
    { "THE_OATH_HE_SHOULD_HAVE_SWORN", "The Oath He Should Have Sworn",
        "As Fëanor, escape with a Silmaril and an unbroken Oath of Mercy.", false },
    { "HIS_FATHERS_LIGHT", "His Father's Light",
        "As Fëanor, use Finwë's lamp to lift the curse on Fëanor's crown.", false },
    { "HEIRLOOMS_OF_THE_HIDDEN_KING", "Heirlooms of the Hidden King",
        "As a Man, escape with a Silmaril, Narsil and the Ring of Barahir.", false },
    { "THE_RING_MAKER_OVERRULED", "The Ring-maker Overruled",
        "Permanently weaken Gorthaur with the Song of Contest while wearing the Ring of Mairon.", true },
    { "NO_PHIAL_REQUIRED", "When All Other Lights Go Out",
        "Slay Shelob with Sting while wielding a Silmaril as your light.", true },
    { "THREE_UNTOUCHED", "Three Untouched",
        "As Celebrimbor, forge three separate custom artefact rings during one adventure, none evil or cursed.", false },
    { "A_BURGLARS_BUSINESS", "A Burglar's Business",
        "Cut a Silmaril without waking Morgoth during the attempt.", true },
    { "STRONGBOW", "Strongbow",
        "As Beleg, slay a hostile unique with an arrow.", false },
    { "ONE_JEWEL_IS_ENOUGH", "One Jewel Is Enough",
        "As Beren, escape with exactly one Silmaril while Morgoth lives.", false },
    { "A_DESPERATE_VICTORY", "A Desperate Victory",
        "Slay a hostile unique while at 25% HP or less.", true },
    { "THEIR_WORK_HAD_NO_RIVAL", "Their Work Had No Rival",
        "Create a custom artefact of smithing difficulty 65 or higher.", false },
    { "FIND_NAUGLAMIR", "The Necklace of the Dwarves",
        "Nauglamír, the Dwarven masterpiece made for Finrod, resting on its wearer 'as a strand of flax'.", true },
    { "FIND_GURTHANG", "The Black Sword of Túrin",
        "Gurthang, the black sword that slew Glaurung and answered Túrin: 'I will drink thy blood gladly'.", true },
    { "FIND_MELKOR", "The Spear of Melkor",
        "The Great Spear of Melkor, the black weapon that pierced the Two Trees before Ungoliant drank their light.", true },
    { "FIND_GOTHMOG_AXE", "The Black Axe of Gothmog",
        "Gothmog's black axe, which struck down Fingon in the Battle of Unnumbered Tears.", true },
    { "FIND_ANGRIST", "Angrist, the Iron-Cleaver",
        "Angrist, Telchar's knife that Beren took into Angband: 'iron it would cleave as if it were green wood'.", true },
    { "FIND_BELTHRONDING", "The Black Bow of Beleg",
        "Belthronding, Beleg's bow of black yew, laid beside him after his death at Túrin's hand.", true },
    { "FIND_IDRIL_ROBE", "The Raiment of Silverfoot",
        "The Robe of Idril Celebrindal, a remembrance of Silverfoot and the secret way that saved Gondolin's survivors.", true },
    { "FIND_CELEBRIMBOR_GLOVES", "The Hands of the Jewel-Smith",
        "Celebrimbor's gloves, recalling Fëanor's grandson, whose greatest works would be the Three Rings of the Elves.", true },
    { "FIND_MELIAN_RING", "The Foresight of Melian",
        "The Ring of Melian, a remembrance of the Maia whose Girdle sheltered Doriath from Morgoth.", true },
    { "FIND_ULMO_CLOAK", "The Mantle of the Lord of Waters",
        "The cloak Ulmo gave Tuor at Vinyamar, concealing the bearer of his warning on the road to Gondolin.", true },
    { "FIND_FINGOLFIN_SHIELD", "The Shield of the High King",
        "Fingolfin's blue, crystal-set shield, borne when he rode alone to challenge Morgoth.", true },
    { "FIND_TURGON_STAFF", "The Staff of the Hidden King",
        "The Quarterstaff of Turgon, the 'staff of doom' before which Eöl faced judgment in Gondolin.", true },
    { "SWIFT_AS_THE_WIND", "Swift as the Wind",
        "Win a Blitz run by escaping with a Silmaril or defeating Morgoth.", false },
    { "TENFOLD_DOOM", "Tenfold Doom",
        "Win a Blitz run with at least 10 net curse stacks (curses minus blessings).", false }
};

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
    return sil_achievement_enabled(id) ? &catalogue[id] : NULL;
}

uint64_t sil_achievement_unlocked_at(int id)
{
    return sil_achievement_get(id) ? unlocked_at[id] : 0;
}

uint64_t sil_achievements_mask(unsigned word)
{
    uint64_t mask = 0;
    if (word >= SIL_ACH_WORD_COUNT) return 0;
    for (unsigned bit = 0; bit < 64 && word * 64 + bit < SIL_ACH_COUNT; ++bit)
        if (unlocked_at[word * 64 + bit]) mask |= UINT64_C(1) << bit;
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
                if (!sil_achievement_enabled(i)) continue;
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
        if (!sil_achievement_enabled(i) || !unlocked_at[i]) continue;
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
    if (!initialized || !sil_achievement_get(id) || unlocked_at[id]) return;
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
    if (depth >= 3) unlock_achievement(SIL_ACH_THE_ROAD_GOES_EVER_ON, true);
    if (depth >= 10) unlock_achievement(SIL_ACH_INTO_THE_DEEP, true);
    sil_achievements_flush();
}

void sil_achievements_on_event(sil_achievement_id id)
{
    if (id < SIL_ACH_NO_RINGS_ATTACHED || id >= SIL_ACH_COUNT) return;
    switch (id)
    {
        case SIL_ACH_SWIFT_AS_THE_WIND:
        case SIL_ACH_TENFOLD_DOOM:
            /* Only a confirmed victorious Blitz completion can grant these. */
            return;
        case SIL_ACH_OFT_EVIL_WILL:
        case SIL_ACH_THE_HASTY_STROKE:
        case SIL_ACH_A_FEW_LEAVES:
        case SIL_ACH_TWO_WEAPONS:
        case SIL_ACH_FOR_EXTERNAL_USE_ONLY:
        case SIL_ACH_FLY_YOU_FOOL:
        case SIL_ACH_ON_THE_OTHER_HAND:
        case SIL_ACH_HIS_DEATH_AND_FINAL_END:
        case SIL_ACH_STRONGBOW:
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
    if (p_ptr->skill_base[S_SNG] >= 20)
        unlock_achievement(SIL_ACH_SONG_BENEATH_THE_MOUNTAIN, true);
    sil_achievement_lore_on_escape(silmarils);
    sil_achievements_flush();
}

void sil_achievements_begin_run(void)
{
    p_ptr->achievement_flags = SIL_ACH_RUN_STARTED | SIL_ACH_RUN_CHASM_SHAFT_ROUTE;
    p_ptr->achievement_kills = 0;
    p_ptr->achievement_singing_turns = 0;
    p_ptr->achievement_balrogs = 0;
    p_ptr->achievement_comeback_depth = -1;
    p_ptr->achievement_fall_depth = -1;
    memset(p_ptr->achievement_jammed_doors, 0,
        sizeof(p_ptr->achievement_jammed_doors));
    sil_achievements_discovery_reset(true);
    p_ptr->achievement_rage_epoch = 0;
    sil_achievement_lore_reset();
    sil_achievements_song_begin();
}

void sil_achievements_on_healing(void)
{
    if (p_ptr && p_ptr->chp > 1)
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
    if (achievements_eligible() && p_ptr->depth == UTUMNO_DEPTH)
    {
        unlock_achievement(SIL_ACH_THE_HALLS_OF_UTUMNO, true);
        sil_achievements_flush();
    }
    if (achievements_eligible() && p_ptr->depth == MORGOTH_DEPTH
        && (p_ptr->achievement_flags
                & (SIL_ACH_RUN_STARTED | SIL_ACH_RUN_CHASM_SHAFT_ROUTE))
            == (SIL_ACH_RUN_STARTED | SIL_ACH_RUN_CHASM_SHAFT_ROUTE))
    {
        unlock_achievement(SIL_ACH_NO_STEPS_REQUIRED, true);
        sil_achievements_flush();
    }
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
    if (achievements_eligible() && !p_ptr->depth && p_ptr->morgoth_hall_entered
        && silmarils_possessed() == 0)
    {
        unlock_achievement(SIL_ACH_THERE_AND_BACK_AGAIN, true);
        sil_achievements_flush();
    }
    sil_achievements_on_partition_entry(p_ptr->py, p_ptr->px);
}

void sil_achievements_on_level_travel(int from_depth, int to_depth, int feature)
{
    sil_achievement_lore_on_travel(from_depth, to_depth);
    /* Report only committed travel. Upward travel never spoils the route.
     * The cause matters: a false floor or teleport while standing on a shaft
     * is still not a shaft descent. Legacy saves never acquire provenance. */
    if (p_ptr && to_depth > from_depth
        && feature != FEAT_CHASM && feature != FEAT_MORE_SHAFT)
        p_ptr->achievement_flags &= ~SIL_ACH_RUN_CHASM_SHAFT_ROUTE;
}

void sil_achievements_on_death(void)
{
    if (p_ptr && p_ptr->chp <= 0)
        p_ptr->achievement_glorfindel_depth = -1;
    /* take_hit() calls this after last-chance equipment, before closing the
     * living run. Escape, victory, quitting and suicide never use this hook. */
    if (!achievements_eligible() || p_ptr->depth != 0 || p_ptr->chp > 0
        || p_ptr->escaped || p_ptr->morgoth_slain) return;
    unlock_achievement(SIL_ACH_THERE_AND_ALMOST_BACK_AGAIN, true);
    sil_achievements_flush();
}

void sil_achievements_on_monster_killed(int m_idx, int who)
{
    if (!achievements_eligible() || who >= 0) return;
    if (mon_list && r_info && z_info && m_idx > 0 && m_idx < mon_max)
    {
        int race = mon_list[m_idx].r_idx;
        if (race > 0 && race < z_info->r_max
            && !(r_info[race].flags1 & RF1_PEACEFUL))
        {
            if (p_ptr->chp == 1)
                unlock_achievement(SIL_ACH_BY_A_THREAD, true);
            if (r_info[race].flags1 & RF1_UNIQUE)
            {
                if (p_ptr->mhp > 0 && p_ptr->chp > 0
                    && (s32b)p_ptr->chp * 4 <= p_ptr->mhp)
                    unlock_achievement(SIL_ACH_A_DESPERATE_VICTORY, true);
                switch (race)
                {
                    case R_IDX_GOTHMOG: unlock_achievement(SIL_ACH_THE_HIGH_CAPTAIN_FALLS, true); break;
                    case R_IDX_GLAURUNG: unlock_achievement(SIL_ACH_DAGNIR_GLAURUNGA, true); break;
                    case R_IDX_GORTHAUR: unlock_achievement(SIL_ACH_THE_ABHORRED, true); break;
                    case R_IDX_UNGOLIANT: unlock_achievement(SIL_ACH_UNLIGHT_UNWOVEN, true); break;
                    case R_IDX_ANCALAGON: unlock_achievement(SIL_ACH_THE_DRAGON_HOST_BROKEN, true); break;
                    case R_IDX_CARCHAROTH: unlock_achievement(SIL_ACH_THE_RED_MAW, true); break;
                }
            }
        }
    }
    sil_achievement_character_on_monster_killed(m_idx, who);
    sil_achievement_lore_on_kill(m_idx, who);
    if (p_ptr->achievement_kills < UINT32_MAX) ++p_ptr->achievement_kills;
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

void sil_achievements_on_artefact_created(int difficulty)
{
    if (!achievements_eligible() || difficulty < 65) return;
    unlock_achievement(SIL_ACH_THEIR_WORK_HAD_NO_RIVAL, true);
    sil_achievements_flush();
}

void sil_achievements_on_artefact_found(int artefact)
{
    if (!achievements_eligible()) return;
    switch (artefact)
    {
        case ART_NAUGLAMIR: unlock_achievement(SIL_ACH_FIND_NAUGLAMIR, true); break;
        case ART_GURTHANG: unlock_achievement(SIL_ACH_FIND_GURTHANG, true); break;
        case ART_MELKOR: unlock_achievement(SIL_ACH_FIND_MELKOR, true); break;
        case ART_GOTHMOG_AXE: unlock_achievement(SIL_ACH_FIND_GOTHMOG_AXE, true); break;
        case ART_ANGRIST: unlock_achievement(SIL_ACH_FIND_ANGRIST, true); break;
        case ART_BELTHRONDING: unlock_achievement(SIL_ACH_FIND_BELTHRONDING, true); break;
        case ART_IDRIL_ROBE: unlock_achievement(SIL_ACH_FIND_IDRIL_ROBE, true); break;
        case ART_CELEBRIMBOR_GLOVES: unlock_achievement(SIL_ACH_FIND_CELEBRIMBOR_GLOVES, true); break;
        case ART_MELIAN_RING: unlock_achievement(SIL_ACH_FIND_MELIAN_RING, true); break;
        case ART_ULMO_CLOAK: unlock_achievement(SIL_ACH_FIND_ULMO_CLOAK, true); break;
        case ART_FINGOLFIN_SHIELD: unlock_achievement(SIL_ACH_FIND_FINGOLFIN_SHIELD, true); break;
        case ART_TURGON_STAFF: unlock_achievement(SIL_ACH_FIND_TURGON_STAFF, true); break;
        default: return;
    }
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

static const struct { int race; const char* name; } achievement_balrogs[] = {
    { R_IDX_DURUIN, "Duruin" }, { 145, "Delthaur" }, { 166, "Belegwath" },
    { 186, "Turkano" }, { 206, "Vallach" }, { 225, "Lungorthin" },
    { R_IDX_GOTHMOG, "Gothmog" }
};

static unsigned achievement_bit_count(unsigned mask)
{
    unsigned count = 0;
    while (mask) { count += mask & 1u; mask >>= 1; }
    return count;
}

bool sil_achievement_revealed(int id)
{
    const sil_achievement_definition* entry = sil_achievement_get(id);
    if (!entry) return false;
    return !entry->hidden || sil_achievement_unlocked_at(id);
}

bool sil_achievement_progress_text(int id, char* out, size_t size)
{
    if (!out || !size) return false;
    out[0] = '\0';
    if (!p_ptr || !character_generated || !p_ptr->playing || p_ptr->is_dead)
        return false;
    switch (id)
    {
        case SIL_ACH_AT_MOST_SEVEN:
            strnfmt(out, size, "This adventure: %u/7 Balrogs.",
                achievement_bit_count(p_ptr->achievement_balrogs));
            for (unsigned i = 0; i < N_ELEMENTS(achievement_balrogs); ++i)
            {
                size_t used = strlen(out);
                strnfmt(out + used, size - used, "\n[%s] %s",
                    p_ptr->achievement_balrogs & (1u << i) ? "x" : " ",
                    achievement_balrogs[i].name);
            }
            return true;
        case SIL_ACH_THREE_UNTOUCHED:
        {
            unsigned count = 0;
            for (int i = 0; i < 3; ++i)
                if (p_ptr->achievement_artefact_rings[i]) ++count;
            strnfmt(out, size, "This adventure: %u/3 artefact rings forged.", count);
            return true;
        }
        case SIL_ACH_SEVEN_TIMES_HE_CRIED:
            strnfmt(out, size, "This adventure: %u/7 damaging blows with Ringil.",
                (unsigned)p_ptr->achievement_ringil_wounds);
            return true;
        case SIL_ACH_NO_STEPS_REQUIRED:
            strnfmt(out, size, "%s",
                !(p_ptr->achievement_flags & SIL_ACH_RUN_STARTED)
                    ? "Route tracking is unavailable for this adventure."
                    : p_ptr->achievement_flags & SIL_ACH_RUN_CHASM_SHAFT_ROUTE
                        ? "This adventure: route intact." : "This adventure: route broken.");
            return true;
        case SIL_ACH_THE_QUIET_ROAD:
            strnfmt(out, size, "%s", !(p_ptr->achievement_flags & SIL_ACH_RUN_STARTED)
                ? "Kill history is unavailable for this adventure."
                : p_ptr->achievement_kills ? "This adventure: a creature has been killed."
                    : "This adventure: no creatures killed.");
            return true;
        case SIL_ACH_WORD_UNBROKEN:
            strnfmt(out, size, "%s", !p_ptr->oath_type ? "No oath chosen."
                : oath_invalid(p_ptr->oath_type) ? "This adventure: oath broken."
                    : "This adventure: oath unbroken.");
            return true;
        case SIL_ACH_A_MAP_OF_THE_IRON_HELLS:
        {
            unsigned mask = (p_ptr->achievement_discovery_flags
                & SIL_ACH_DISC_REGIONS) >> SIL_ACH_DISC_REGIONS_SHIFT;
            static const char* const names[] = { "Labyrinth", "Chasm", "Ice", "Fire", "Poison" };
            strnfmt(out, size, "This run: %u/5 regions.", achievement_bit_count(mask));
            for (unsigned i = 0; i < N_ELEMENTS(names); ++i)
            {
                size_t used = strlen(out);
                strnfmt(out + used, size - used, "\n[%s] %s",
                    mask & (1u << i) ? "x" : " ", names[i]);
            }
            return true;
        }
        case SIL_ACH_THE_LONG_TALE:
        {
            if (run_mode_is_blitz()) return false;
            cJSON* tale = current_tale_characters(false);
            unsigned count = 0;
            for (const cJSON* item = tale ? tale->child : NULL; item; item = item->next)
                if (cJSON_IsTrue(item)) ++count;
            strnfmt(out, size, "This Tale: %u/2 different characters have completed adventures.",
                MIN(count, 2u));
            return true;
        }
        default: return false;
    }
}

void sil_achievements_on_balrog_killed(int race)
{
    if (!achievements_eligible()) return;
    for (unsigned i = 0; i < N_ELEMENTS(achievement_balrogs); ++i)
        if (achievement_balrogs[i].race == race)
        {
            p_ptr->achievement_balrogs |= 1u << i;
            if (p_ptr->achievement_balrogs == 0x7F)
                unlock_achievement(SIL_ACH_AT_MOST_SEVEN, true);
            break;
        }
    sil_achievements_flush();
}

void sil_achievements_on_partition_discovered(unsigned mask)
{
    if (!achievements_eligible()) return;
    /* This existing saved flag word belongs to the current player/run. Old
     * Tale-wide JSON history deliberately cannot supply any of these visits. */
    p_ptr->achievement_discovery_flags |= (mask & 31u) << SIL_ACH_DISC_REGIONS_SHIFT;
    if ((p_ptr->achievement_discovery_flags & SIL_ACH_DISC_REGIONS)
        == SIL_ACH_DISC_REGIONS)
        unlock_achievement(SIL_ACH_A_MAP_OF_THE_IRON_HELLS, true);
    sil_achievements_flush();
}

void sil_achievements_on_run_completed(void)
{
    if (!achievement_run_eligible() || !p_ptr->is_dead) return;
    if (run_mode_is_blitz())
    {
        /* Both victory routes close the living run before close_game_aux()
         * reports completion. A death, abandoned run or empty-handed escape
         * is not a Blitz win. Use the saved run history and actual signed
         * stacks, never the setup sliders or a Tale's curse history. */
        if (p_ptr->playing || !p_ptr->leaving || p_ptr->chp <= 0
            || !(p_ptr->achievement_flags & SIL_ACH_RUN_STARTED)
            || (!p_ptr->morgoth_slain
                && !(p_ptr->escaped && p_ptr->depth == 0
                    && silmarils_possessed() > 0))) return;
        int net_curses = 0;
        for (int id = 0; id < METAR_CURSE_SLOTS; ++id)
            net_curses += CURSE_GET(id);
        unlock_achievement(SIL_ACH_SWIFT_AS_THE_WIND, true);
        if (net_curses >= 10) unlock_achievement(SIL_ACH_TENFOLD_DOOM, true);
        sil_achievements_flush();
        return;
    }
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
    uint64_t masks[SIL_ACH_WORD_COUNT];
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
        masks[word] = sil_achievements_mask(word);
    sil_steamworks_sync_achievements(masks);
    sil_play_games_sync(masks);
    sil_game_center_sync(masks);
    for (unsigned word = 0; word < SIL_ACH_WORD_COUNT; ++word)
    {
        uint64_t remote = sil_steamworks_achievements_unlocked(word)
            | sil_play_games_unlocked(word) | sil_game_center_unlocked(word);
        for (unsigned bit = 0; bit < 64 && word * 64 + bit < SIL_ACH_COUNT; ++bit)
            if (remote & (UINT64_C(1) << bit)) unlock_achievement(word * 64 + bit, false);
    }
    if (dirty && now >= next_save) sil_achievements_flush();
}
