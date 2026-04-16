/* File: app-scene-birth.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "app/app-session.h"
#include "app/app-scene-menu.h"
#include "app/app-ui.h"
#include "externs.h"
#include "blitz.h"
#include "fs/path.h"
#include "log/log.h"
#include "platform-config.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "player/killer.h"
#include "metarun.h"
#include "runtime/runtime-cli.h"
#include "score/score_entry.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-information-scene.h"

static bool skill_gain_in_progress = false;

typedef struct birth_compact_flag_line {
    cptr txt;
    byte attr;
} birth_compact_flag_line;

/* Three-column layout constants (same as cmd4.c) */
#define COL_SKILL 2
#define COL_ABILITY 15
#define COL_DESCRIPTION 25

/* Forward declaration of wipe_screen_from function */
extern void wipe_screen_from(int col);

/* Locations of the tables on the screen */
#define HEADER_ROW 0
#define QUESTION_ROW 1
#define TABLE_ROW 2
#define DESCRIPTION_ROW 15
#define INSTRUCT_ROW 22

#define QUESTION_COL 2
#define RACE_COL 2
#define RACE_AUX_COL 19
#define CLASS_COL 17
#define CLASS_AUX_COL 27
#define TOTAL_AUX_COL 35
#define INVALID_CHOICE 255

static int find_named_artifact_for_character(void);
static void grant_starting_artifact(void);
static bool starting_artifact_is_eligible(int art_idx, int k_idx);

static void copy_start_items(start_item dest[MAX_START_ITEMS],
    const start_item src[MAX_START_ITEMS]);
static void replace_start_food(start_item list[MAX_START_ITEMS], byte from_sval,
    byte to_sval);
static const int birth_stat_costs[11];
static int skill_cost(int base, int points);
static int collect_character_trait_lines(int race, int character,
    bool short_labels, birth_compact_flag_line out[], int out_max,
    int* max_line_len);

#define BLITZ_MAX_EFFECT_COUNT 9

static bool starting_artifact_is_eligible(int art_idx, int k_idx)
{
    artefact_type *a_ptr;
    object_type object_type_body;
    object_type *o_ptr = &object_type_body;

    if (art_idx <= 0 || art_idx >= z_info->art_max)
        return false;

    a_ptr = &a_info[art_idx];
    if (!a_ptr->name[0])
        return false;

    if (a_ptr->level > 10)
        return false;

    if (!k_idx)
        return false;

    object_prep(o_ptr, k_idx);
    o_ptr->name1 = art_idx;
    apply_magic(o_ptr, -1, true, true, true, true);

    return (object_smithing_difficulty(o_ptr) <= 45);
}

/* Character ability names */
static const char *character_ability_names[S_MAX][ABILITIES_MAX] =
{
    [S_MEL] = {
        [MEL_POWER]            = "Power",
        [MEL_FINESSE]          = "Finesse",
        [MEL_KNOCK_BACK]       = "Knock Back",
        [MEL_THROWING]         = "Throwing",
        [MEL_POLEARMS]         = "Polearm Mastery",
        [MEL_CHARGE]           = "Charge",
        [MEL_FOLLOW_THROUGH]   = "Follow-Through",
        [MEL_IMPALE]           = "Impale",
        [MEL_CONTROL]          = "Subtlety",
        [MEL_WHIRLWIND_ATTACK] = "Whirlwind Attack",
        [MEL_ZONE_OF_CONTROL]  = "Zone of Control",
        [MEL_SMITE]            = "Smite",
        [MEL_TWO_WEAPON]       = "Two Weapon Fighting",
        [MEL_RAPID_ATTACK]     = "Rapid Attack",
        [MEL_STR]              = NULL,  /* if you care about STR */
    },
    [S_ARC] = {
        [ARC_ROUT]        = "Rout",
        [ARC_FLETCHERY]   = "Fletchery",
        [ARC_POINT_BLANK] = "Point Blank Archery",
        [ARC_PUNCTURE]    = "Puncture",
        [ARC_AMBUSH]      = "Ambush",
        [ARC_VERSATILITY] = "Versatility",
        [ARC_CRIPPLING]   = "Crippling Shot",
        [ARC_DEADLY_HAIL] = "Deadly Hail",
        [ARC_DEX]         = NULL,
    },
    [S_EVN] = {
        [EVN_DODGING]            = "Dodging",
        [EVN_BLOCKING]           = "Blocking",
        [EVN_PARRY]              = "Parry",
        [EVN_CROWD_FIGHTING]     = "Crowd Fighting",
        [EVN_LEAPING]            = "Leaping",
        [EVN_SPRINTING]          = "Sprinting",
        [EVN_FLANKING]           = "Flanking",
        [EVN_HEAVY_ARMOUR]       = "Heavy Armour Use",
        [EVN_RIPOSTE]            = "Riposte",
        [EVN_CONTROLLED_RETREAT] = "Controlled Retreat",
        [EVN_DEX]                = NULL,
    },
    [S_STL] = {
        [STL_DISGUISE]          = "Disguise",
        [STL_ASSASSINATION]     = "Assassination",
        [STL_CRUEL_BLOW]        = "Cruel Blow",
        [STL_EXCHANGE_PLACES]   = "Exchange Places",
        [STL_OPPORTUNIST]       = "Opportunist",
        [STL_VANISH]            = "Vanish",
        [STL_DEX]               = NULL,
    },
    [S_PER] = {
        [PER_QUICK_STUDY]    = "Quick Study",
        [PER_FOCUSED_ATTACK] = "Focused Attack",
        [PER_KEEN_SENSES]    = "Keen Senses",
        [PER_CONCENTRATION]  = "Concentration",
        [PER_ALCHEMY]        = "Alchemy",
        [PER_BANE]           = "Bane",
        [PER_OUTWIT]         = "Outwit",
        [PER_LISTEN]         = "Resonance",
        [PER_MASTER_HUNTER]  = "Master Hunter",
        [PER_GRA]            = NULL,
    },
    [S_WIL] = {
        [WIL_CURSE_BREAKING]        = "Curse Breaking",
        [WIL_CHANNELING]            = "Channeling",
        [WIL_STRENGTH_IN_ADVERSITY] = "Strength in Adversity",
        [WIL_FORMIDABLE]            = "Formidable",
        [WIL_INNER_LIGHT]           = "Inner Light",
        [WIL_INDOMITABLE]           = "Indomitable",
        [WIL_OATH]                  = "Oath",
        [WIL_POISON_RESISTANCE]     = "Poison Resistance",
        [WIL_VENGEANCE]             = "Vengeance",
        [WIL_MAJESTY]               = "Majesty",
        [WIL_CON]                   = NULL,
    },
    [S_SMT] = {
        [SMT_WEAPONSMITH]   = "Weaponsmith",
        [SMT_ARMOURSMITH]   = "Armoursmith",
        [SMT_JEWELLER]      = "Jeweller",
        [SMT_ENCHANTMENT]   = "Enchantment",
        [SMT_EXPERTISE]     = "Expertise",
        [SMT_ARTEFACT]      = "Artifice",
        [SMT_MASTERPIECE]   = "Masterpiece",
        [SMT_ALLOY_MASTERY] = "Alloy mastery",
        [SMT_GRA]           = NULL,
    },
    [S_SNG] = {
        [SNG_ELBERETH]      = "Song of Elbereth",
        [SNG_CHALLENGE]     = "Song of Challenge",
        [SNG_DELVINGS]      = "Song of Delvings",
        [SNG_FREEDOM]       = "Song of Freedom",
        [SNG_SILENCE]       = "Song of Silence",
        [SNG_STAUNCHING]    = "Song of Staunching",
        [SNG_THRESHOLDS]    = "Song of Thresholds",
        [SNG_TREES]         = "Song of the Trees",
        [SNG_REVEALING]     = "Song of Revealing",
        [SNG_WOVEN_THEMES]  = "Woven Themes",
        [SNG_SLAYING]       = "Song of Slaying",
        [SNG_ELVENESS]      = "Song of Elveness",
        [SNG_STAYING]       = "Song of Staying",
        [SNG_DISGUISE]      = "Song of Disguise",
        [SNG_LORIEN]        = "Song of Lorien",
        [SNG_SHATTERING]    = "Song of Shattering",
        [SNG_MASTERY]       = "Song of Mastery",
        [SNG_CONTEST]       = "Song of Contest",
        [SNG_LAMENT]        = "Song of Lament",
        [SNG_GRA]           = NULL,
    },
    [S_SPC] = {
        [SPC_MANDOS] = "Mandos' Doom", /* immunity reward */
        [SPC_AULE] = "Aule's Forge", /* improved masterpiece forging */
        [SPC_OATH_MERCY] = "Oath of Mercy",
        [SPC_OATH_SILENCE] = "Oath of Silence",
        [SPC_OATH_IRON] = "Oath of Iron",
        [SPC_NIENA_MERCY] = "Niena's Gift of Mercy", /* Enhanced stealth from mercy quest */
        [SPC_OATH_SMITH] = "Oath of the Smith",
        [SPC_OATH_VALOROUS] = "Oath of the Valorous Heart",
        [SPC_UNIQUE_BANE] = "Unique Bane", /* Enhanced effectiveness against unique monsters */
        [SPC_OATH_LIGHT] = "Oath of Light",
    },
};

/*
 * Forward declare
 */
typedef struct birther birther;
typedef struct birth_menu birth_menu;

/*
 * A structure to hold "rolled" information
 */
struct birther
{
    s16b age;
    s16b wt;
    s16b ht;
    s16b sc;

    s16b stat[A_MAX];

    char history[550];
};

/*
 * A structure to hold the menus
 */
struct birth_menu
{
    bool ghost;
    cptr name;
    cptr text;
};

// s16b adj_c[A_MAX];

static int get_start_xp(void)
{
    if (birth_fixed_exp)
    {
        return PY_FIXED_EXP;
    }
    else
    {
        return PY_START_EXP;
    }
}

/* -----------------------------------------------------------
 * new: delegate to the (i386-safe) 2-bit accessor in metarun.h
 * --------------------------------------------------------- */
static int curse_count(int id)           /* 0-31 */
{
    return CURSE_GET(id);
}


/* Return net adjustment for a primary stat from EVERY active metarun curse */
static int curses_stat_adj(int s)   /* s = 0-3  (STR-DEX-CON-GRA) */
{
    int delta = 0;
    for (int bit = 0; bit < z_info->cu_max; bit++) {
        int cnt = curse_count(bit);
        if (cnt)
            delta += cnt * cu_info[bit].cu_adj[s];
    }
    return delta;
}



/*
 * Generate some info that the auto-roller ignores
 */
static void get_extra(void)
{
    int i, j;
    
    p_ptr->new_exp = p_ptr->exp = get_start_xp();
    p_ptr->discovery_lore_flags = 0;
    log_debug("Set starting experience to %d", p_ptr->exp);

    /* Player is not singing */
    p_ptr->song1 = SNG_NOTHING;
    p_ptr->song2 = SNG_NOTHING;
    p_ptr->song_target_idx = 0;
    p_ptr->song_target_song = SNG_NOTHING;
    p_ptr->song_lockout_timer = 0;
    p_ptr->song_contest_player_stacks = 0;
    p_ptr->song_duel_pad = 0;
    p_ptr->song_contest_last_turn = 0;
    
    /* Clear the abilities and add character abilities - but preserve oath abilities */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            /* Preserve oath abilities (SPC_OATH_MERCY, SPC_OATH_SILENCE, SPC_OATH_IRON, SPC_OATH_SMITH, SPC_OATH_VALOROUS, SPC_OATH_LIGHT) */
            if (i == S_SPC && (j == SPC_OATH_MERCY || j == SPC_OATH_SILENCE || j == SPC_OATH_IRON || j == SPC_OATH_SMITH || j == SPC_OATH_VALOROUS || j == SPC_OATH_LIGHT))
            {
                /* Keep existing oath abilities intact */
                continue;
            }
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }
    
    /* Grant all parsed character abilities */
    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[p_ptr->pcharacter].a_adj[slot][0];
        /* sentinel: no more entries */
        if (stat < 0) break;

        int ab = c_info[p_ptr->pcharacter].a_adj[slot][1];
        /* sanity-check bounds */
        if (stat < S_MAX && ab < ABILITIES_MAX)
        {
            p_ptr->innate_ability[stat][ab] = true;
            p_ptr->active_ability[stat][ab] = true;
            log_debug("Assigned character ability: stat=%d, ability=%d", stat, ab);
        }
    }
}

/*
 * Clear all the global "character" data
 */
void player_wipe(void)
{
    /* We are about to wipe the old hero, so there is no fully-generated
     * character any more.  This must be cleared **before** we enter the
     * next character-creation cycle; otherwise helpers such as
     * show_scores() believe a character still exists. */
    character_generated = false;
    log_debug("birth.c: character_generated set to false - starting character wipe");
    int i;
    char history[550];
    int stat[A_MAX];

    log_debug("Wiping player data for new character creation");

    /* Backup the player choices */
    // Initialized to soothe compilation warnings
    byte prace = 0;
    byte pcharacter = 0;
    int age = 0;
    int height = 0;
    int weight = 0;

    // only save the old information if there was a character loaded
    if (character_loaded_dead)
    {
        log_debug("Restoring previous character choices from dead character");
        /* Backup the player choices */
        prace = p_ptr->prace;
        pcharacter = p_ptr->pcharacter;
        age = p_ptr->age;
        height = p_ptr->ht;
        weight = p_ptr->wt;
        sprintf(history, "%s", p_ptr->history);

        for (i = 0; i < A_MAX; i++)
        {
            if (!(p_ptr->noscore & 0x0008))
                stat[i] = p_ptr->stat_base[i]
                    - (rp_ptr->r_adj[i] + current_character_profile->h_adj[i]);
            else
                stat[i] = 0;
        }
    }

    /* Wipe the player */
    memset(p_ptr, 0, sizeof(player_type));

    supplies_reset_store();

    // only save the old information if there was a character loaded
    if (character_loaded_dead)
    {
        /* Restore the choices */
        p_ptr->prace = prace;
        p_ptr->pcharacter = pcharacter;
        p_ptr->game_type = 0;
        p_ptr->age = age;
        p_ptr->ht = height;
        p_ptr->wt = weight;
        sprintf(p_ptr->history, "%s", history);
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = stat[i];
        }
    }
    else
    {
        /* Reset */
        p_ptr->prace = 0;
        p_ptr->pcharacter = 0;
        p_ptr->game_type = 0;
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        p_ptr->history[0] = '\0';
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }

    /* Clear the inventory */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_wipe(&inventory[i]);
    }

    /* Start with no artefacts made yet */
    /* and clear the slots for in-game randarts */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        a_ptr->cur_num = 0;
        a_ptr->found_num = 0;
        a_ptr->seen = 0;
    }
    
    /* Initialize Valar artifact reservation array */
    if (!valar_reserved_artifacts)
    {
        valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
    }
    for (i = 0; i < z_info->art_max; i++)
    {
        valar_reserved_artifacts[i] = false;
    }

    /*re-set the object_level*/
    object_level = 0;

    /* Reset the "objects" */
    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Reset "tried" */
        k_ptr->tried = false;

        /* Reset "aware" */
        k_ptr->aware = false;
    }

    /* Reset the "monsters" */
    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Hack -- Reset the counter */
        r_ptr->cur_num = 0;

        /* Hack -- Reset the max counter */
        r_ptr->max_num = 100;

        /* Hack -- Reset the max counter */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            r_ptr->max_num = 1;

        /* Clear player sights/kills */
        l_ptr->psights = 0;
        l_ptr->pkills = 0;
    }

    /*No current player ghosts*/
    bones_selector = 0;

    // give the player the most food possible without a message showing
    p_ptr->food = PY_FOOD_FULL - 1;

    // reset the stair info
    p_ptr->stairs_taken = 0;
    p_ptr->staircasiness = 0;

    // reset the forge info
    p_ptr->fixed_forge_count = 0;
    p_ptr->forge_count = 0;

    // No vengeance at birth
    p_ptr->vengeance = 0;

    // Morgoth unhurt
    p_ptr->morgoth_state = 0;
    p_ptr->morgoth_second_wind = 0;

    p_ptr->killed_enemy_with_arrow = false;
    p_ptr->orome_bow_hit_streak = 0;
    p_ptr->orome_spear_ready = 0;

    p_ptr->oath_type = 0;
    p_ptr->oaths_broken = 0;

    p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    p_ptr->tulkas_target_r_idx = 0;
    p_ptr->tulkas_prize_a_idx = 0;
    p_ptr->tulkas_quest_complete = 0;
    p_ptr->tulkas_stronghold_level = 0;
    p_ptr->tulkas_stronghold_placed = 0;
    p_ptr->tulkas_second_roll_done = 0;
    p_ptr->tulkas_orc_mask = 0;
    p_ptr->tulkas_orc_restricted = 0;
    p_ptr->tulkas_second_spawn_pending = 0;
    p_ptr->tulkas_morgoth_progress = 0;

    /* Aule quest init */
    p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
    p_ptr->aule_forge_y = 0;
    p_ptr->aule_forge_x = 0;
    p_ptr->aule_reserved = 0;
    p_ptr->aule_level = 0;
    p_ptr->aule_last_object_diff = 0;
    
    /* Mandos quest init */
    p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
    p_ptr->mandos_vault_y = 0;
    p_ptr->mandos_vault_x = 0;
    p_ptr->mandos_monsters_remaining = 0;
    p_ptr->mandos_level = 0;
    p_ptr->mandos_reserved = 0;
    p_ptr->mandos_resurrection_primed = 0;
    p_ptr->mandos_resurrection_used = 0;
    
    /* Niena quest init */
    p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
    p_ptr->niena_monsters_seen = 0;
    p_ptr->niena_monsters_killed = 0;
    p_ptr->niena_reserved = 0;
    p_ptr->niena_level = 0;
    p_ptr->niena_reserved2 = 0;
    
    /* Orome quest init */
    p_ptr->orome_quest = OROME_QUEST_NOT_STARTED;
    p_ptr->orome_killed_count = 0;
    p_ptr->orome_target_type = 0;
    p_ptr->orome_target_count = 0;
    p_ptr->orome_wolves_killed = 0;
    p_ptr->orome_spiders_killed = 0;
    p_ptr->orome_serpents_killed = 0;
    p_ptr->orome_vampires_killed = 0;
    p_ptr->orome_dragons_killed = 0;
    p_ptr->orome_great_hunt_mask = 0;
    /* Varda quest init */
    p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
    p_ptr->varda_vault_ready = 0;
    p_ptr->varda_vault_placed = 0;
    p_ptr->varda_shadow_restricted = 0;
    p_ptr->varda_level = 0;
    p_ptr->varda_shadow_ready = 0;
    p_ptr->varda_shadow_placed = 0;
    p_ptr->varda_shadow_pad = 0;
    p_ptr->varda_shadow_level = 0;

    for (i = 0; i < VALA_MAX; i++)
    {
        p_ptr->vala_quest_stage2[i] = 0;
        p_ptr->vala_quest_stage3[i] = 0;
    }
    
    p_ptr->quest_vault_used = 0;
    
    /* Quest states should always start at NOT_STARTED for new characters */
    /* Metarun completion is checked separately via metarun_is_quest_completed() */
    log_trace("Birth: All quest states initialized to NOT_STARTED for new character");
    for (i = 0; i < (int)N_ELEMENTS(p_ptr->quest_reserved); i++) p_ptr->quest_reserved[i] = 0; /* quest_reserved[0] = any quest spawned flag; quest_reserved[1..n] = per-run quest completion markers */

    // reset some unique flags
    p_ptr->unique_forge_made = false;
    p_ptr->unique_forge_seen = false;
    for (i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        p_ptr->greater_vaults[i] = 0;
    }
}

/* ------------------------------------------------------------------
 * Hand out one start-item list (race or character template).
 * ------------------------------------------------------------------ */
static void give_start_items(const start_item *list)
{
    int i, slot, inven_slot;
    object_type object_type_body, *i_ptr, *o_ptr;

    for (i = 0; i < MAX_START_ITEMS && list[i].tval; i++)
    {
        const start_item *e_ptr = &list[i];

        /* Look up kind */
        s16b k_idx = lookup_kind(e_ptr->tval, e_ptr->sval);
        if (!k_idx) continue;

        object_kind *k_ptr = &k_info[k_idx];
        i_ptr = &object_type_body;

        /* Prepare object */
        object_prep(i_ptr, k_idx);
        i_ptr->number = (byte)rand_range(e_ptr->min, e_ptr->max);
        i_ptr->weight = k_ptr->weight;

        /* Where would this be wielded? */
        slot = wield_slot(i_ptr);

        /* Light sources start with their standard default fuel. */
        if (slot == INVEN_LITE)
        {
            if (i_ptr->sval == SV_LIGHT_TORCH)
                i_ptr->timeout = 1000;
            else if (i_ptr->sval == SV_LIGHT_LANTERN)
                i_ptr->timeout = 3000;
            else if (i_ptr->sval == SV_LIGHT_MALLORN)
                i_ptr->timeout = 100;
        }

        bool start_known = true;
        if ((i_ptr->tval == TV_POTION)
            || (i_ptr->tval == TV_FOOD && i_ptr->sval <= SV_FOOD_SICKNESS)
            || (i_ptr->tval == TV_GEM))
        {
            if (!player_auto_identifies_object(i_ptr))
                start_known = false;
        }

        if (start_known)
            object_known(i_ptr);

        /* Carry it */
        int carry_slot = inven_carry(i_ptr, true);

        if (carry_slot == SUPPLIES_INDEX)
        {
            object_type copy;
            object_copy(&copy, i_ptr);
            char name[80];
            object_desc(name, sizeof(name), &copy, true, 3);
            char label = supplies_label_char();
            if (!label)
                label = 'a';
            log_info("Starting item went to supplies: %s (%c)", name, label);

            if ((slot == INVEN_LITE) && (inventory[INVEN_LITE].tval == 0))
            {
                int supply_idx = supplies_first_entry_for_kind(i_ptr->k_idx);
                object_type equip_light;

                if ((supply_idx >= 0) && supplies_take_one(supply_idx, &equip_light))
                {
                    object_copy(&inventory[INVEN_LITE], &equip_light);
                    if (inventory[INVEN_LITE].sval == SV_LIGHT_LANTERN)
                        inventory[INVEN_LITE].timeout = 0;
                    p_ptr->equip_cnt++;
                }
            }
            continue;
        }

        if (carry_slot < 0)
            continue;

        inven_slot = carry_slot;

        /* Auto-wield if slot empty */
        if (slot >= INVEN_WIELD && inventory[slot].tval == 0)
        {
            o_ptr = &inventory[slot];
            object_copy(o_ptr, i_ptr);

            if (o_ptr->tval != TV_ARROW) o_ptr->number = 1;

            inven_item_increase(inven_slot, -(o_ptr->number));
            inven_item_optimize(inven_slot);
            p_ptr->equip_cnt++;
        }

        object_wipe(i_ptr); /* avoid dupes */
    }
}

static void copy_start_items(start_item dest[MAX_START_ITEMS],
    const start_item src[MAX_START_ITEMS])
{
    int item_idx;

    for (item_idx = 0; item_idx < MAX_START_ITEMS; item_idx++)
    {
        dest[item_idx] = src[item_idx];
    }
}

static void replace_start_food(start_item list[MAX_START_ITEMS], byte from_sval,
    byte to_sval)
{
    int item_idx;

    for (item_idx = 0; item_idx < MAX_START_ITEMS && list[item_idx].tval;
         item_idx++)
    {
        if (list[item_idx].tval == TV_FOOD && list[item_idx].sval == from_sval)
        {
            list[item_idx].sval = to_sval;
        }
    }
}

/*
 * Find a named artifact matching the current character.
 * Returns artifact index if found, otherwise 0.
 * 
 * Matches artifacts with "of {CharacterName}" in their name.
 * For example: "Ring of Barahir" matches character "Barahir",
 *              "Crown of Feanor" matches character "Feanor".
 */
static int find_named_artifact_for_character(void)
{
    character_profile *current_character_profile = &c_info[p_ptr->pcharacter];
    const char *character_name = c_name + current_character_profile->name;
    
    /* Build pattern: "of {CharacterName}" */
    char pattern[64];
    char art_lower[MAX_LEN_ART_NAME];
    char pattern_lower[64];
    
    strnfmt(pattern, sizeof(pattern), "of %s", character_name);
    
    /* Convert pattern to lowercase for case-insensitive comparison */
    for (int i = 0; pattern[i] && i < (int)sizeof(pattern_lower) - 1; i++) {
        pattern_lower[i] = tolower((unsigned char)pattern[i]);
    }
    pattern_lower[strlen(pattern)] = '\0';
    
    /* Search all artifacts for one matching this character's name */
    for (int i = 1; i < z_info->art_max; i++) {
        artefact_type *a_ptr = &a_info[i];
        
        /* Skip artifacts without names or already created */
        if (!a_ptr->name[0]) continue;
        if (a_ptr->cur_num > 0) continue;
        if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;

        /* Convert artifact name to lowercase */
        for (int j = 0; a_ptr->name[j] && j < MAX_LEN_ART_NAME - 1; j++) {
            art_lower[j] = tolower((unsigned char)a_ptr->name[j]);
        }
        art_lower[strlen(a_ptr->name)] = '\0';
        
        /* Check if artifact name contains "of {CharacterName}" */
        if (strstr(art_lower, pattern_lower)) {
            /* Verify it's a valid base kind */
            int k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (starting_artifact_is_eligible(i, k_idx)) {
                log_info("Found named artifact for %s: %s (idx=%d)", 
                         character_name, a_ptr->name, i);
                return i;
            }
        }
    }
    
    log_debug("No named artifact found for character: %s", character_name);
    return 0;
}

static void grant_starting_artifact(void)
{
    int art_idx = 0;
    int k_idx = 0;
    
    /* First, try to find a named artifact for this character */
    art_idx = find_named_artifact_for_character();
    
    if (art_idx > 0) {
        /* Found a named artifact - validate and grant it */
        artefact_type *a_ptr = &a_info[art_idx];
        k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
        
        if (!k_idx) {
            log_warn("Named artifact has invalid base kind (idx=%d)", art_idx);
            art_idx = 0;  /* Fall through to random selection */
        } else if (valar_reserved_artifacts && valar_reserved_artifacts[art_idx]) {
            log_info("Named artifact already reserved (idx=%d)", art_idx);
            art_idx = 0;  /* Fall through to random selection */
        } else if (!starting_artifact_is_eligible(art_idx, k_idx)) {
            log_info("Named artifact does not meet starting thresholds (idx=%d)", art_idx);
            art_idx = 0;  /* Fall through to random selection */
        }
    }
    
    /* If no named artifact, use the original random selection logic */
    if (art_idx == 0) {
        int candidates[512];
        int candidate_kinds[512];
        int count = 0;
        for (int i = 1; i < z_info->art_max && count < (int)N_ELEMENTS(candidates); i++) {
            artefact_type *a_ptr = &a_info[i];
            int k;

            if (!a_ptr->name[0]) continue;
            if (a_ptr->cur_num > 0) continue;
            if (valar_reserved_artifacts && valar_reserved_artifacts[i]) continue;

            k = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (!starting_artifact_is_eligible(i, k)) continue;

            candidates[count] = i;
            candidate_kinds[count] = k;
            count++;
        }

        if (count == 0) {
            log_warn("No artefacts available for starting blessing under the lvl<=10 and difficulty<=45 filter.");
            msg_print("No artefact could be granted.");
            return;
        }

        int pick = rand_int(count);
        art_idx = candidates[pick];
        k_idx = candidate_kinds[pick];
    }
    
    /* Grant the selected artifact */
    artefact_type *a_ptr = &a_info[art_idx];

    object_type object_type_body;
    object_type *o_ptr = &object_type_body;
    object_prep(o_ptr, k_idx);
    o_ptr->name1 = art_idx;
    apply_magic(o_ptr, -1, true, true, true, true);
    object_aware(o_ptr);
    object_known(o_ptr);
    int slot = inven_carry(o_ptr, true);
    if (slot < 0) {
        log_warn("Starting artefact could not be carried (idx=%d)", art_idx);
        msg_print("You have no room for a starting artefact.");
        return;
    }
    a_ptr->cur_num = 1;
    if (valar_reserved_artifacts) valar_reserved_artifacts[art_idx] = true;

    log_info("Starting artefact granted: %s (idx=%d)", a_ptr->name, art_idx);
}

static void player_outfit(void)
{
    /* ---------- locals ---------- */
    time_t      c;
    struct tm  *tp;

    log_debug("Starting player equipment setup");

    /* skip all starting‐gear on load */
    if (character_loaded) return;

    /* ---------- escape-curse check ---------- */
    if (curse_flag_count_cur(CUR_NOSTART)) return;

    /* ---------- pointers into info arrays ---------- */
    player_race  *rp_ptr = &p_info[p_ptr->prace];
    character_profile *current_character_profile = &c_info[p_ptr->pcharacter];
    start_item race_start_items[MAX_START_ITEMS];

    copy_start_items(race_start_items, rp_ptr->start_items);

    if (current_character_profile->flags_u & UNQ_SMT_EOL)
    {
        replace_start_food(race_start_items, SV_FOOD_LEMBAS, SV_FOOD_BREAD);
    }

    /* ---------- hand out gear ---------- */
    log_debug("Giving starting items for race: %s", p_name + rp_ptr->name);
    give_start_items(race_start_items);   /* race first  */
    log_debug("Giving starting items for character: %s", c_name + current_character_profile->name);
    give_start_items(current_character_profile->start_items);   /* character kit */

    if (!run_mode_is_blitz()
        && metarun_has_major_blessing_effect(METARUN_MAJOR_EFFECT_START_ARTIFACT)) {
        grant_starting_artifact();
    }

    /* ---------- Christmas present (unchanged) ---------- */
    c  = time((time_t*)0);
    tp = localtime(&c);
    if ((tp->tm_mon == 11) && (tp->tm_mday >= 25))
    {
        object_type object_type_body, *i_ptr = &object_type_body;

        s16b k_idx = lookup_kind(TV_CHEST, SV_CHEST_PRESENT);
        object_prep(i_ptr, k_idx);
        i_ptr->number = 1;
        i_ptr->pval   = -20;

        (void)inven_carry(i_ptr, true);
    }

    /* ---------- bookkeeping ---------- */
    p_ptr->update |= (PU_BONUS | PU_MANA);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);

    log_debug("Player equipment setup completed");
}

static void birth_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    platform_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static bool birth_pending_compact_description_confirm = false;

static bool birth_confirm_input(int ch, bool steamdeck)
{
    if (ch == '\r' || ch == '\n' || ch == ' ' || ch == INPUT_BIND_CONFIRM)
        return true;

    if (steamdeck && ch == steamdeck_confirm_key())
        return true;

    return false;
}

static void birth_trimmed_stat_label(int stat, char* buf, size_t buflen)
{
    const char* label;
    size_t len;

    if (!buf || !buflen)
        return;

    label = (stat >= 0 && stat < A_MAX) ? stat_names[stat] : "";
    SDL_strlcpy(buf, label ? label : "", buflen);
    len = strlen(buf);
    while (len > 0 && buf[len - 1] == ' ')
    {
        buf[--len] = '\0';
    }
}

static void birth_build_stats_prompt(bool steamdeck, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];
        char quit_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        birth_prompt_label('q', "Start", quit_label, sizeof(quit_label));
        strnfmt(buf, buflen, "D-pad allocate  %s back  %s confirm  %s quit",
            back_label, confirm_label, quit_label);
        return;
    }

    strnfmt(buf, buflen,
        "Arrows allocate  ESC back  SPACE/ENTER confirm  q quit");
}

static void birth_build_skills_prompt(bool steamdeck, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];
        char quit_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));
        strnfmt(buf, buflen, "D-pad allocate  %s back  %s confirm  %s quit",
            back_label, confirm_label, quit_label);
        return;
    }

    strnfmt(buf, buflen,
        "Arrows allocate  ESC back  SPACE/ENTER confirm  q quit");
}

static void birth_build_review_prompt(bool steamdeck, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(buf, buflen, "%s back to assignment  %s continue", back_label,
            confirm_label);
        return;
    }

    strnfmt(buf, buflen, "ESC back to assignment  SPACE/ENTER continue");
}

static bool birth_build_stats_allocation_ui_scene(app_ui_scene* scene,
    const int stats[A_MAX], int selected_stat, int points_left, bool steamdeck)
{
    app_ui_panel* panel;
    char prompt[160];
    char subtitle[64];
    int i;

    if (!scene || !stats)
        return false;

    birth_build_stats_prompt(steamdeck, prompt, sizeof(prompt));
    if (!build_character_sheet_ui_scene(scene, prompt))
        return false;

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->selected_row = selected_stat;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 420, 560);
    app_ui_panel_set_title(panel, TERM_L_BLUE, "Allocate Stats");
    strnfmt(subtitle, sizeof(subtitle), "Points Left: %d", points_left);
    app_ui_panel_set_subtitle(panel, TERM_L_GREEN, subtitle);

    for (i = 0; i < A_MAX; i++)
    {
        char label[32];
        char meta[16];
        bool selected = (i == selected_stat);

        birth_trimmed_stat_label(i, label, sizeof(label));
        strnfmt(meta, sizeof(meta), "%d", birth_stat_costs[stats[i] + 4]);
        if (!app_ui_panel_add_row(panel, i, selected ? TERM_L_BLUE : TERM_WHITE,
                true, selected, "", label, meta))
        {
            return false;
        }
    }

    return true;
}

static bool birth_present_stats_allocation_ui_scene(const int stats[A_MAX],
    int selected_stat, int points_left, bool steamdeck)
{
    app_ui_scene scene;

    if (!birth_build_stats_allocation_ui_scene(&scene, stats, selected_stat,
            points_left, steamdeck)
        || !ui_information_scene_present_ui(&scene))
    {
        log_warn("birth stats allocation: semantic scene presentation failed");
        return false;
    }

    return true;
}

static bool birth_build_skills_allocation_ui_scene(app_ui_scene* scene,
    int selected_skill, const int old_base[S_MAX], const int skill_gain[S_MAX],
    int exp_left, bool steamdeck)
{
    app_ui_panel* panel;
    char prompt[160];
    char subtitle[64];
    int i;
    int selected_row = 0;
    int row_index = 0;

    if (!scene || !old_base || !skill_gain)
        return false;

    birth_build_skills_prompt(steamdeck, prompt, sizeof(prompt));
    if (!build_character_sheet_ui_scene(scene, prompt))
        return false;

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 420, 640);
    app_ui_panel_set_title(panel, TERM_L_BLUE, "Allocate Skills");
    strnfmt(subtitle, sizeof(subtitle), "Experience Left: %d", exp_left);
    app_ui_panel_set_subtitle(panel, TERM_L_GREEN, subtitle);

    for (i = 0; i < S_MAX; i++)
    {
        char meta[16];
        bool selected;

        if (i == S_SPC)
            continue;

        selected = (i == selected_skill);
        if (selected)
            selected_row = row_index;
        strnfmt(meta, sizeof(meta), "%d", skill_cost(old_base[i],
            skill_gain[i]));
        if (!app_ui_panel_add_row(panel, i, selected ? TERM_L_BLUE : TERM_WHITE,
                true, selected, "", skill_names_full[i], meta))
        {
            return false;
        }
        row_index++;
    }

    panel->selected_row = selected_row;
    return true;
}

static bool birth_present_skills_allocation_ui_scene(int selected_skill,
    const int old_base[S_MAX], const int skill_gain[S_MAX], int exp_left,
    bool steamdeck)
{
    app_ui_scene scene;

    if (!birth_build_skills_allocation_ui_scene(&scene, selected_skill,
            old_base, skill_gain, exp_left, steamdeck)
        || !ui_information_scene_present_ui(&scene))
    {
        log_warn("birth skills allocation: semantic scene presentation failed");
        return false;
    }

    return true;
}

static bool birth_build_assignment_review_ui_scene(app_ui_scene* scene,
    bool steamdeck)
{
    app_ui_panel* panel;
    char prompt[160];

    if (!scene)
        return false;

    birth_build_review_prompt(steamdeck, prompt, sizeof(prompt));
    if (!build_character_sheet_ui_scene(scene, prompt))
        return false;

    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 420, 560);
    app_ui_panel_set_title(panel, TERM_L_BLUE, "Character Review");
    if (!app_ui_panel_add_body_line(panel, TERM_WHITE,
            "Review the character sheet before you start."))
    {
        return false;
    }
    return app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Continue to start, or go back to adjust your assignments.");
}

static bool birth_show_semantic_assignment_review(bool steamdeck)
{
    char ch;

    while (1)
    {
        app_ui_scene scene;

        if (!birth_build_assignment_review_ui_scene(&scene, steamdeck)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("birth assignment review: semantic scene presentation failed");
            return false;
        }

        ch = (char)ui_information_scene_wait_key_hidden_with_wait_reason(
            APP_WAIT_REASON_CONFIRM);

        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;

        if ((ch == ESCAPE) || (ch == '4') || (ch == 'q') || (ch == 'Q'))
            return false;

        if (birth_confirm_input(ch, steamdeck) || (ch == '6'))
            return true;
    }
}

static bool birth_ui_panel_add_wrapped_lines(app_ui_panel* panel, byte attr,
    cptr text, bool detail_lines)
{
    char line_buffer[APP_UI_TEXT_MAX];
    int line_pos = 0;
    const char* text_ptr = text;
    int max_width = APP_UI_TEXT_MAX - 1;

    if (!panel || !text || !text[0])
        return true;

    while (*text_ptr)
    {
        while (*text_ptr == ' ' && line_pos == 0)
            text_ptr++;

        if (*text_ptr == '\n')
        {
            line_buffer[line_pos] = '\0';
            if (line_pos > 0)
            {
                if (detail_lines)
                {
                    if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                        return false;
                }
                else if (!app_ui_panel_add_body_line(panel, attr, line_buffer))
                {
                    return false;
                }
            }
            else if (detail_lines)
            {
                if (!app_ui_panel_add_detail_line(panel, attr, " "))
                    return false;
            }
            else if (!app_ui_panel_add_body_line(panel, attr, " "))
            {
                return false;
            }

            line_pos = 0;
            text_ptr++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;

            while (wrap_pos > 0 && line_buffer[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                int remaining = line_pos - wrap_pos - 1;

                line_buffer[wrap_pos] = '\0';
                if (detail_lines)
                {
                    if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                        return false;
                }
                else if (!app_ui_panel_add_body_line(panel, attr, line_buffer))
                {
                    return false;
                }

                for (int i = 0; i < remaining; i++)
                    line_buffer[i] = line_buffer[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_buffer[line_pos] = '\0';
                if (detail_lines)
                {
                    if (!app_ui_panel_add_detail_line(panel, attr, line_buffer))
                        return false;
                }
                else if (!app_ui_panel_add_body_line(panel, attr, line_buffer))
                {
                    return false;
                }
                line_pos = 0;
            }

            continue;
        }

        line_buffer[line_pos++] = *text_ptr++;
    }

    if (line_pos > 0)
    {
        line_buffer[line_pos] = '\0';
        if (detail_lines)
            return app_ui_panel_add_detail_line(panel, attr, line_buffer);
        return app_ui_panel_add_body_line(panel, attr, line_buffer);
    }

    return true;
}

static int character_choice_index_by_name(cptr choice_name)
{
    int character_idx;

    if (!choice_name)
        return -1;

    for (character_idx = 0; character_idx < z_info->c_max; character_idx++)
    {
        if (!strcmp(choice_name, c_name + c_info[character_idx].name))
            return character_idx;
    }

    return -1;
}

static cptr character_selection_header_text(bool character_phase)
{
    (void)character_phase;
    return "Character Selection:";
}

static void birth_choice_full_name(birth_menu choice, char* full_name,
    size_t full_name_len)
{
    int character_idx;

    if (!full_name || full_name_len == 0)
        return;

    character_idx = character_choice_index_by_name(choice.name);
    if (character_idx >= 0)
    {
        strnfmt(full_name, full_name_len, "%s%s",
            c_name + c_info[character_idx].name,
            c_name + c_info[character_idx].alt_name);
    }
    else
    {
        strnfmt(full_name, full_name_len, "%s",
            choice.name ? choice.name : "");
    }
}

static bool birth_description_scene_add_footer(app_ui_panel* panel)
{
    if (!panel)
        return false;

    if (steamdeck_controls_active())
    {
        char back_label[16];

        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            back_label, "Return");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Any key", "Return");
}

static bool birth_build_description_ui_scene(app_ui_scene* scene,
    cptr title, cptr text)
{
    app_ui_panel* panel;
    byte story = platform_story_font_enabled() ? STORY_FLAG_USE : 0;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_USE_BACKDROP
        | APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 900, 1600);
    app_ui_panel_set_title(panel, TERM_L_BLUE, title ? title : "");
    if (text && text[0])
    {
        if (!app_ui_panel_begin_rich_paragraph(scene, panel))
            return false;
        if (!app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE, story,
                text))
        {
            return false;
        }
    }

    return birth_description_scene_add_footer(panel);
}

static bool birth_show_description_ui_scene(birth_menu choice)
{
    ui_information_scene_scope scope;
    app_ui_scene scene;
    char full_name[64];

    if (!ui_information_scene_enter(&scope))
    {
        log_warn("birth description: semantic scene unavailable");
        return false;
    }

    birth_choice_full_name(choice, full_name, sizeof(full_name));
    if (!birth_build_description_ui_scene(&scene, full_name, choice.text)
        || !ui_information_scene_present_ui(&scene))
    {
        ui_information_scene_leave(&scope);
        log_warn("birth description: semantic scene presentation failed");
        return false;
    }

    (void)ui_information_scene_wait_key_nonrepeat();
    ui_information_scene_leave(&scope);
    return true;
}

static int collect_character_starting_abilities(int character, cptr out[], int out_max)
{
    int count = 0;

    if (character <= 0)
        return 0;

    if (c_info[character].flags_u & UNQ_MIM)
        return 0;

    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[character].a_adj[slot][0];
        int ability = c_info[character].a_adj[slot][1];
        cptr name;

        if (stat < 0)
            break;

        if (stat >= S_MAX || ability < 0 || ability >= ABILITIES_MAX)
            continue;

        name = character_ability_names[stat][ability];
        if (!name)
            continue;

        if (out && count < out_max)
            out[count] = name;

        count++;
    }

    return count;
}

static void birth_selection_row_key(int index, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    if (index >= 0 && index < 26)
    {
        strnfmt(buf, buflen, "%c", I2A(index));
        return;
    }
    if (index >= 26 && index < 52)
    {
        strnfmt(buf, buflen, "%c", (char)('A' + (index - 26)));
        return;
    }

    buf[0] = '\0';
}

static void birth_character_power_stars(int character_idx, char* buf,
    size_t buflen, byte* attr)
{
    byte power = 1;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (attr)
        *attr = TERM_WHITE;

    if (character_idx >= 0 && character_idx < z_info->c_max)
        power = c_info[character_idx].power;

    switch (power)
    {
    case 0:
        if (attr)
            *attr = TERM_RED;
        strnfmt(buf, buflen, "*");
        break;
    case 1:
        if (attr)
            *attr = TERM_WHITE;
        strnfmt(buf, buflen, "**");
        break;
    case 2:
        if (attr)
            *attr = TERM_GREEN;
        strnfmt(buf, buflen, "***");
        break;
    case 3:
    case 4:
        if (attr)
            *attr = TERM_L_GREEN;
        strnfmt(buf, buflen, "***");
        break;
    default:
        if (attr)
            *attr = TERM_WHITE;
        strnfmt(buf, buflen, "**");
        break;
    }
}

static bool birth_selection_add_race_detail_lines(app_ui_panel* panel,
    birth_menu choice)
{
    int race;

    if (!panel)
        return false;

    for (race = 0; race < z_info->p_max; race++)
    {
        if (!strcmp(choice.name, p_name + p_info[race].name))
            break;
    }
    if (race >= z_info->p_max)
        return false;

    for (int i = 0; i < A_MAX; i++)
    {
        char line[64];
        int adj = p_info[race].r_adj[i];
        byte attr = TERM_L_DARK;

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else if (adj > 2)
            attr = TERM_L_BLUE;

        strnfmt(line, sizeof(line), "%s %+d", stat_names[i], adj);
        if (!app_ui_panel_add_detail_line(panel, attr, line))
            return false;
    }

    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
        return false;
    return birth_ui_panel_add_wrapped_lines(panel, TERM_WHITE, choice.text, true);
}

static bool birth_selection_add_character_detail_lines(app_ui_panel* panel,
    birth_menu choice)
{
    int character_idx = character_choice_index_by_name(choice.name);
    birth_compact_flag_line trait_lines[64];
    cptr ability_lines[CHARACTER_ABILITY_MAX];
    int ability_count;
    int trait_max_len = 0;
    int trait_count;

    if (!panel || character_idx < 0 || character_idx >= z_info->c_max)
        return false;

    for (int i = 0; i < A_MAX; i++)
    {
        char line[64];
        int adj = c_info[character_idx].h_adj[i] + rp_ptr->r_adj[i]
            + curses_stat_adj(i);
        byte attr = TERM_L_DARK;

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else if (adj > 2)
            attr = TERM_L_BLUE;

        strnfmt(line, sizeof(line), "%s %+d", stat_names[i], adj);
        if (!app_ui_panel_add_detail_line(panel, attr, line))
            return false;
    }

    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;
    ability_count = collect_character_starting_abilities(character_idx,
        ability_lines, (int)N_ELEMENTS(ability_lines));
    if (ability_count > 0)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " ")
            || !app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
                "Starting abilities"))
        {
            return false;
        }
        for (int i = 0; i < ability_count; i++)
        {
            if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
                return true;
            if (!app_ui_panel_add_detail_line(panel, TERM_YELLOW,
                    ability_lines[i]))
            {
                return false;
            }
        }
    }

    trait_count = collect_character_trait_lines(p_ptr->prace, character_idx,
        false, trait_lines, (int)N_ELEMENTS(trait_lines), &trait_max_len);
    if (trait_count > 0)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " ")
            || !app_ui_panel_add_detail_line(panel, TERM_L_BLUE,
                "Traits and modifiers"))
        {
            return false;
        }
        for (int i = 0; i < trait_count; i++)
        {
            if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
                return true;
            if (!app_ui_panel_add_detail_line(panel, trait_lines[i].attr,
                    trait_lines[i].txt))
            {
                return false;
            }
        }
    }

    if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
        return true;
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
        return false;
    return birth_ui_panel_add_wrapped_lines(panel, TERM_WHITE, choice.text, true);
}

static bool birth_selection_build_ui_scene(app_ui_scene* scene,
    birth_menu* choices, int num, int top, int cur,
    bool allow_full_description_screen)
{
    app_ui_panel* panel;
    bool steamdeck = steamdeck_controls_active();
    bool character_phase = allow_full_description_screen;
    char subtitle[80];
    int selected_row = cur - top;

    if (!scene || !choices || num <= 0 || cur < 0 || cur >= num)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, TERM_L_BLUE,
        character_selection_header_text(character_phase));

    if (character_phase)
    {
        strnfmt(subtitle, sizeof(subtitle), "Race: %s",
            p_name + p_info[p_ptr->prace].name);
        app_ui_panel_set_subtitle(panel, TERM_WHITE, subtitle);
    }

    for (int i = 0; i < num; i++)
    {
        char keybuf[8];
        char meta[16];
        byte meta_attr = TERM_WHITE;

        birth_selection_row_key(i, keybuf, sizeof(keybuf));
        meta[0] = '\0';
        if (character_phase)
            birth_character_power_stars(character_choice_index_by_name(
                choices[i].name), meta, sizeof(meta), &meta_attr);

        if (!app_ui_panel_add_row_ex(panel, i,
                (i == cur) ? TERM_L_BLUE
                           : (choices[i].ghost ? TERM_SLATE : TERM_WHITE),
                meta_attr,
                0, 0,
                !choices[i].ghost, i == cur,
                keybuf, choices[i].name, meta))
        {
            return false;
        }
    }

    if (selected_row < 0)
        selected_row = 0;
    if (selected_row >= panel->row_count)
        selected_row = panel->row_count - 1;
    panel->selected_row = (s16b)selected_row;
    app_ui_panel_set_row_offset(panel, (s16b)top);
    app_ui_panel_set_detail_title(panel, TERM_WHITE, choices[cur].name);

    if (character_phase)
    {
        if (!birth_selection_add_character_detail_lines(panel, choices[cur]))
            return false;
    }
    else
    {
        if (!birth_selection_add_race_detail_lines(panel, choices[cur]))
            return false;
    }

    if (steamdeck)
    {
        char confirm_label[16];
        char detail_label[16];
        char back_label[16];
        char random_label[16];
        char help_label[16];
        char quit_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A",
            confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_alt_action_key(), "X",
            detail_label, sizeof(detail_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        birth_prompt_label('r', "r", random_label, sizeof(random_label));
        birth_prompt_label('?', "?", help_label, sizeof(help_label));
        if (streq(help_label, "?"))
            birth_prompt_label('h', "h", help_label, sizeof(help_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Select")
            || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back")
            || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                random_label, "Random"))
        {
            return false;
        }
        if (allow_full_description_screen
            && !app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                detail_label, "Description"))
        {
            return false;
        }
        if (!app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                "o", "Options")
            || !app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
                "s", "Scores")
            || !app_ui_panel_add_footer_action(panel, 7, TERM_WHITE, true,
                help_label, "Help")
            || !app_ui_panel_add_footer_action(panel, 8, TERM_WHITE, true,
                quit_label, "Quit"))
        {
            return false;
        }
    }
    else
    {
        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                "Enter", "Select")
            || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "Esc", "Back")
            || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "r", "Random"))
        {
            return false;
        }
        if (allow_full_description_screen
            && !app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                "f", "Description"))
        {
            return false;
        }
        if (!app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
                "o", "Options")
            || !app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
                "s", "Scores")
            || !app_ui_panel_add_footer_action(panel, 7, TERM_WHITE, true,
                "h/?", "Help")
            || !app_ui_panel_add_footer_action(panel, 8, TERM_WHITE, true,
                "q", "Quit"))
        {
            return false;
        }
    }
    return true;
}

static void display_character_description_screen(birth_menu choice)
{
    (void)birth_show_description_ui_scene(choice);
}

/*
 * Generic "get choice from menu" function
 */
static int get_player_choice(birth_menu* choices, int num, int def, int col,
    int wid, void (*hook)(birth_menu), bool allow_full_description_screen)
{
    int next;
    int i, dir;
    char c;
    bool done = false;
    int cur = (def) ? def : 0;
    bool steamdeck = steamdeck_controls_active();

    (void)col;
    (void)wid;
    (void)hook;

    /* Autoselect if able */
    // if (num == 1) done = true;

    /* Choose */
    while (true)
    {
        app_ui_scene scene;

        if (!birth_selection_build_ui_scene(&scene, choices, num, 0, cur,
                allow_full_description_screen)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("birth selection: semantic scene presentation failed");
            return INVALID_CHOICE;
        }

        if (done)
            return (cur);

        c = (char)ui_information_scene_wait_key_hidden_with_wait_reason(
            APP_WAIT_REASON_LIST_SELECTION);

        /* Exit the game */
        if ((c == 'Q') || (c == 'q'))
            quit(NULL);

        /* Hack - go back */
        if ((c == ESCAPE) || (c == '4')
            || (steamdeck && c == steamdeck_back_key()))
            return (INVALID_CHOICE);

        /* Make a choice */
        if (birth_confirm_input(c, steamdeck) || (c == '6')) {
            if (choices[cur].ghost)
                bell("Your race cannot choose that character.");
            else
                return (cur);
        }
        // Show scores (short): accept both 's' and 'S'
        if (c == 's' || c == 'S')
        {
            show_scores_interactive(false);
            continue; /* Return to the selection loop after showing scores */
        }
        
        // Show help: accept both 'h' and 'H', plus '?'
        if (c == 'h' || c == 'H' || c == '?')
        {
            do_cmd_help();
            continue; /* Return to the selection loop after showing help */
        }

        if (allow_full_description_screen
            && (c == 'f' || c == 'F'
                || (steamdeck && c == steamdeck_alt_action_key())))
        {
            display_character_description_screen(choices[cur]);
            continue;
        }

        /* Random choice */
        if (c == 'r')
        {
            /* Ensure legal choice */
            do
            {
                cur = rand_int(num);
            } while (choices[cur].ghost);

            /* Done */
            done = true;
        }

        /* Alphabetic choice */
    else if (isalpha(c))
        {
            /* Options */
            if ((c == 'O') || (c == 'o'))
            {
                do_cmd_options();
            }

            else
            {
                int choice;

                if (islower(c))
                    choice = A2I(c);
                else
                    choice = c - 'A' + 26;

        /* Validate input */
        if ((choice > -1) && (choice < num) && !(choices[choice].ghost))
                {
                    cur = choice;

                    /* Done */
                    done = true;
                }
        else if ((choice > -1) && (choice < num) && choices[choice].ghost)
                {
                    bell("Your race cannot choose that character.");
                }
                else
                {
                    bell("Illegal response to question!");
                }
            }
        }

        /* Move */
        else if (isdigit(c))
        {
            /* Get a direction from the key */
            dir = target_dir(c);

            /* Going up? */
            if (dir == 8)
            {
                next = -1;
                for (i = 0; i < cur; i++)
                {
                    // if (!(choices[i].ghost))
                    // {
                        next = i;
                    // }
                }

                /* Move selection */
                if (next != -1)
                    cur = next;
            }

            /* Going down? */
            if (dir == 2)
            {
                next = -1;
                for (i = num - 1; i > cur; i--)
                {
                    // if (!(choices[i].ghost))
                    //
                        next = i;
                    // }
                }

                /* Move selection */
                if (next != -1)
                    cur = next;
            }
        }

        /* Invalid input */
        else
            bell("Illegal response to question!");
    }

    return (INVALID_CHOICE);
}

/* OR of every flag carried by the active metarun curses */
u32b curse_flag_mask(void)
{
    u32b m = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        if (CURSE_CURSE_STACK(id) > 0) m |= cu_info[id].flags;
    }
    return m;
}

/* Count active curse STACKS that carry an RHF flag (cu_info[].flags) */
int curse_flag_count_rhf(u32b rhf_flag)
{
    int count = 0;
    /* Iterate over every defined curse */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        /* Get the stack count for this curse */
        int stacks = CURSE_GET(i);
        if (stacks > 0) {
            if (cu_info[i].flags & rhf_flag) count += stacks;
        } else if (stacks < 0) {
            if (cu_info[i].blessing_flags & rhf_flag) count += -stacks;
        }
    }
    return count;
}

/* Count active curse STACKS that carry a CUR flag (cu_info[].flags_u) */
int curse_flag_count_cur(u32b cur_flag)
{
    int count = 0;

    /* Iterate over every defined curse */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        /* Get the stack count for this curse */
        int stacks = CURSE_GET(i);
        if (stacks > 0) {
            if (cu_info[i].flags_u & cur_flag) count += stacks;
        } else if (stacks < 0) {
            if (cu_info[i].blessing_flags_u & cur_flag) count += -stacks;
        }
    }

    return count;
}

/* Signed delta for CUR flags: positive for curses, negative for blessings */
int curse_flag_delta_cur(u32b cur_flag)
{
    int delta = 0;

    for (int i = 0; i < z_info->cu_max; i++)
    {
        int stacks = CURSE_GET(i);
        if (stacks > 0) {
            if (cu_info[i].flags_u & cur_flag) delta += stacks;
        } else if (stacks < 0) {
            if (cu_info[i].blessing_flags_u & cur_flag) delta -= (-stacks);
        }
    }

    return delta;
}

static int collect_character_trait_lines(int race, int character, bool short_labels,
    birth_compact_flag_line out[], int out_max, int* max_line_len)
{
    int total = 0;

    byte attr_affinity = TERM_GREEN;
    byte attr_mastery = TERM_L_GREEN;
    byte attr_penalty = TERM_RED;
    byte attr_gr_penalty = TERM_L_RED;

    birth_compact_flag_line uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
    int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH(arr, n, text, color)                                             \
    do {                                                                      \
        if ((text) && (n) < (int)N_ELEMENTS(arr))                             \
        {                                                                     \
            (arr)[(n)].txt = (text);                                          \
            (arr)[(n)++].attr = (color);                                      \
        }                                                                     \
    } while (0)

#define HANDLE_SKILL_EX(LABEL_LONG, LABEL_SHORT, AFF_FLAG, PEN_FLAG)          \
    do {                                                                      \
        int score = 0;                                                        \
        if (p_info[race].flags & (AFF_FLAG)) score++;                         \
        if (c_info[character].flags & (AFF_FLAG)) score++;                    \
        if ((PEN_FLAG) && (p_info[race].flags & (PEN_FLAG))) score--;         \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) score--;    \
        score += curse_flag_count_rhf(AFF_FLAG);                              \
        if ((PEN_FLAG)) score -= curse_flag_count_rhf(PEN_FLAG);              \
        if (score > 2) score = 2;                                             \
        if (score < -2) score = -2;                                           \
        if (score == 2)                                                       \
            PUSH(ma_buf, ma_n,                                                \
                short_labels ? LABEL_SHORT "++" : LABEL_LONG " mastery",      \
                attr_mastery);                                                \
        else if (score == 1)                                                  \
            PUSH(af_buf, af_n,                                                \
                short_labels ? LABEL_SHORT "+ " : LABEL_LONG " affinity",     \
                attr_affinity);                                               \
        else if (score == -1)                                                 \
            PUSH(pen_buf, pen_n,                                              \
                short_labels ? LABEL_SHORT "- " : LABEL_LONG " penalty",      \
                attr_penalty);                                                \
        else if (score == -2)                                                 \
            PUSH(pen_buf, pen_n,                                              \
                short_labels ? LABEL_SHORT "--" : LABEL_LONG " grand penalty",\
                attr_gr_penalty);                                             \
    } while (0)

#define HANDLE_UNIQUE_EX(LABEL_LONG, LABEL_SHORT, FLAG, COLOR)                \
    do {                                                                      \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG))) \
            PUSH(uniq_buf, uniq_n, short_labels ? LABEL_SHORT : LABEL_LONG, (COLOR)); \
    } while (0)

#define HANDLE_UNIQUE_U_EX(LABEL_LONG, LABEL_SHORT, FLAG, COLOR)              \
    do {                                                                      \
        if (c_info[character].flags_u & (FLAG))                               \
            PUSH(uniq_buf, uniq_n, short_labels ? LABEL_SHORT : LABEL_LONG, (COLOR)); \
    } while (0)

#define EMIT(arr, n)                                                          \
    do {                                                                      \
        for (int _i = 0; _i < (n); ++_i)                                      \
        {                                                                     \
            cptr _txt = (arr)[_i].txt ? (arr)[_i].txt : "";                  \
            if (max_line_len && (int)strlen(_txt) > *max_line_len)            \
                *max_line_len = (int)strlen(_txt);                            \
            if (out && total < out_max)                                       \
                out[total] = (arr)[_i];                                       \
            total++;                                                          \
        }                                                                     \
    } while (0)

    HANDLE_SKILL_EX("melee", "melee", RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion", "evasion", RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth", "stealth", RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery", "archery", RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will", "will", RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", "perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing", "smithing", RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song", "song", RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow", "bow", RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe", "axe", RHF_AXE_PROFICIENCY, 0);

    HANDLE_UNIQUE_U_EX("Master Artisan", "Master Artisan", UNQ_SMT_FEANOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Creator of Galvorn", "Galvorn Maker", UNQ_SMT_EOL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("One Handed", "One Handed", UNQ_MEL_MAEDHROS, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Agarwaen", "Agarwaen", UNQ_WIL_TURIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Hidden city", "Hidden City", UNQ_SNG_TURGON, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Chosen of Ulmo", "Ulmo's Chosen", UNQ_WIL_TUOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Indominable Will", "Indom. Will", UNQ_EARENDIL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Orome Himself", "Orome", UNQ_WIL_FIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Songs of Power", "Songs of Power", UNQ_SNG_FIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Elven Dance", "Elven Dance", UNQ_SNG_LUT, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Girdle of Melian", "Melian's Girdle", UNQ_SNG_MEL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Creator of Angrist", "Angrist Maker", UNQ_SMT_TELCHAR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Old Master", "Old Master", UNQ_SMT_GAMIL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Ring Master", "Ring Master", UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Aure entuluva", "Aure Entuluva", UNQ_SNG_HURIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Voice of Girdle", "Girdle Voice", UNQ_SNG_THINGOL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Forgotten", "Forgotten", UNQ_MIM, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Minstrel", "Minstrel", UNQ_MINSTREL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Woven Master", "Woven Master", UNQ_WOVEN_MASTER, TERM_VIOLET);

    HANDLE_UNIQUE_EX("Gift of Eru", "Gift of Eru", RHF_GIFTERU, TERM_VIOLET);
    HANDLE_UNIQUE_EX("Seafarer", "Seafarer", RHF_FREE, TERM_VIOLET);
    HANDLE_UNIQUE_EX("Kinslayer", "Kinslayer", RHF_KINSLAYER, TERM_UMBER);
    HANDLE_UNIQUE_EX("Treacherous", "Treacherous", RHF_TREACHERY, TERM_UMBER);
    HANDLE_UNIQUE_EX("Doom of Mandos", "Mandos' Doom", RHF_CURSE, TERM_UMBER);
    HANDLE_UNIQUE_EX("Morgoth Curse", "Morgoth Curse", RHF_MOR_CURSE, TERM_UMBER);

    EMIT(uniq_buf, uniq_n);
    EMIT(ma_buf, ma_n);
    EMIT(af_buf, af_n);
    EMIT(pen_buf, pen_n);

#undef EMIT
#undef HANDLE_UNIQUE_U_EX
#undef HANDLE_UNIQUE_EX
#undef HANDLE_SKILL_EX
#undef PUSH

    return total;
}


/*
 * Player race
 */
static bool get_player_race(void)
{
    int i;
    birth_menu* races;
    int race;

    races = mem_alloc_array(z_info->p_max, birth_menu);

    /* Tabulate races */
    for (i = 0; i < z_info->p_max; i++)
    {
        races[i].name = p_name + p_info[i].name;
        races[i].ghost = false;
        races[i].text = p_text + p_info[i].text;
    }

    race = get_player_choice(
        races, z_info->p_max, p_ptr->prace, RACE_COL, 15, NULL, false);

    /* No selection? */
    if (race == INVALID_CHOICE)
    {
        return (false);
    }

    // if different race to last time, then wipe the history, age, height,
    // weight
    if (race != p_ptr->prace)
    {
        p_ptr->history[0] = '\0';
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }
    p_ptr->prace = race;

    /* Save the race pointer */
    rp_ptr = &p_info[p_ptr->prace];

    races = mem_free(races);

    /* Success */
    return (true);
}

// Check character flags
static int is_set(int bit) {
    if (bit < 0 || bit >= FLAG_COUNT) return 0;  // Out of bounds
    int word = bit / 32;
    int shift = bit % 32;
    return (rp_ptr->choice[word] & (1U << shift)) != 0;
}

/*
 * Player character template selection
 */
static bool get_character_profile(void)
{
    int i;
    int character = 0;
    int character_choice;
    int previous_choice = 0;
    birth_menu* character_menu;

    int no_character_flags = 1;
    for (int idx = 0; idx < FLAG_WORDS; ++idx) {
        if (rp_ptr->choice[idx] != 0) {
            no_character_flags = 0;
            break;  // At least one flag is set
        }
    }
    // default to the baseline character automatically if no choices are available
    if (no_character_flags)
    {
        p_ptr->pcharacter = 0;
        current_character_profile = &c_info[p_ptr->pcharacter];
        return (true);
    }

    character_menu = mem_alloc_array(z_info->c_max, birth_menu);

    /* Tabulate characters */

    for (i = 0; i < z_info->c_max; i++)
    {

        /* Analyze */
        if (is_set(i))
        {
            if (highscore_dead(c_name + c_info[i].name)) character_menu[character].ghost = true;
            else character_menu[character].ghost = false;

            character_menu[character].name = c_name + c_info[i].name;
            character_menu[character].text = c_text + c_info[i].text;
            if (p_ptr->pcharacter == i)
                previous_choice = character;
            character++;
        }
    }

    character_choice = get_player_choice(
        character_menu, character, previous_choice, CLASS_COL, 22, NULL, true);

    /* No selection? */
    if (character_choice == INVALID_CHOICE)
    {
        return (false);
    }

    /* Get character from choice number */
    character = 0;
    for (i = 0; i < z_info->c_max; i++)
    {
        if (is_set(i))
        {
            if (character_choice == character)
            {
                // if different character to last time, then wipe the history, age,
                // height, weight
                if (i != p_ptr->pcharacter)
                {
                    int j;

                    p_ptr->history[0] = '\0';
                    p_ptr->age = 0;
                    p_ptr->ht = 0;
                    p_ptr->wt = 0;
                    for (j = 0; j < A_MAX; j++)
                    {
                        p_ptr->stat_base[j] = 0;
                    }
                }
                p_ptr->pcharacter = i;
            }
            character++;
        }
    }

    /* Cache the selected character template */
    current_character_profile = &c_info[p_ptr->pcharacter];

    character_menu = mem_free(character_menu);

    return (true);
}

static cptr blitz_character_mode_name(byte mode)
{
    switch (mode)
    {
    case BLITZ_CHARACTER_RANDOM_STATS: return "Random with stats";
    case BLITZ_CHARACTER_SELECTED: return "Selected";
    default: return "Random";
    }
}

static cptr blitz_effect_mode_name(byte mode)
{
    switch (mode)
    {
    case BLITZ_EFFECT_SELECTED: return "Selected";
    case BLITZ_EFFECT_SELECTED_DESCR: return "Selected + descriptions";
    default: return "Random";
    }
}

typedef struct birth_menu_scene_scope {
    bool active;
    app_wait_scope wait_scope;
} birth_menu_scene_scope;

static bool birth_menu_scene_enter(birth_menu_scene_scope* scope, u16b reason)
{
    app_session* session;

    if (!scope)
        return false;

    memset(scope, 0, sizeof(*scope));
    session = app_session_current();
    if (!session
        || !app_session_has_flag(session, APP_SESSION_FLAG_BRIDGE_LEGACY_INPUT))
        return false;

    app_session_push_wait_scope(session, &scope->wait_scope, reason, 0, 0);
    app_session_clear_inputs(session);
    scope->active = true;
    return true;
}

static void birth_menu_scene_leave(birth_menu_scene_scope* scope)
{
    app_session* session = app_session_current();

    if (!scope || !scope->active || !session)
        return;

    app_session_clear_inputs(session);
    app_session_clear_menu_snapshot(session);
    app_session_set_snapshot(session, NULL);
    app_session_pop_wait_scope(session, &scope->wait_scope);
    scope->active = false;
}

static void blitz_setup_clamp(blitz_setup* setup)
{
    if (!setup)
        return;

    if (setup->character_mode > BLITZ_CHARACTER_SELECTED)
        setup->character_mode = BLITZ_CHARACTER_RANDOM;
    if (setup->effect_mode > BLITZ_EFFECT_SELECTED_DESCR)
        setup->effect_mode = BLITZ_EFFECT_RANDOM;
    if (setup->blessing_count > BLITZ_MAX_EFFECT_COUNT)
        setup->blessing_count = BLITZ_MAX_EFFECT_COUNT;
    if (setup->curse_count > BLITZ_MAX_EFFECT_COUNT)
        setup->curse_count = BLITZ_MAX_EFFECT_COUNT;
    if (setup->curse_count < setup->blessing_count)
        setup->curse_count = setup->blessing_count;
}

static void blitz_pick_random_race_and_character(void)
{
    int race = 0;
    int available[64];
    int available_count = 0;

    if (!z_info)
        return;

    race = rand_int(z_info->p_max);
    p_ptr->prace = race;
    rp_ptr = &p_info[p_ptr->prace];

    for (int i = 0; i < z_info->c_max && available_count < (int)N_ELEMENTS(available); i++)
    {
        if (is_set(i))
            available[available_count++] = i;
    }

    if (available_count <= 0)
        p_ptr->pcharacter = 0;
    else
        p_ptr->pcharacter = available[rand_int(available_count)];

    current_character_profile = &c_info[p_ptr->pcharacter];
}

static bool blitz_setup_build_ui_scene(app_ui_scene* scene,
    const blitz_setup* setup, int selected)
{
    static const char* const titles[] = {
        "Character", "Oaths", "Blessings", "Curses", "Effect Picks"
    };
    app_ui_panel* panel;
    cptr detail = "";
    char meta[64];
    char keybuf[16];
    bool steamdeck = steamdeck_controls_active();

    if (!scene || !setup)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->selected_row = selected;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 980, 1800);
    app_ui_panel_set_title(panel, TERM_YELLOW, "Blitz Setup");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "Configure a self-contained Blitz run. Story progress stays untouched.");

    strnfmt(meta, sizeof(meta), "%s",
        blitz_character_mode_name(setup->character_mode));
    if (!app_ui_panel_add_row(panel, 0, selected == 0 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 0, "", titles[0], meta))
    {
        return false;
    }

    strnfmt(meta, sizeof(meta), "%s", setup->oaths_enabled ? "Yes" : "No");
    if (!app_ui_panel_add_row(panel, 1, selected == 1 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 1, "", titles[1], meta))
    {
        return false;
    }

    strnfmt(meta, sizeof(meta), "%d", setup->blessing_count);
    if (!app_ui_panel_add_row(panel, 2, selected == 2 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 2, "", titles[2], meta))
    {
        return false;
    }

    strnfmt(meta, sizeof(meta), "%d", setup->curse_count);
    if (!app_ui_panel_add_row(panel, 3, selected == 3 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 3, "", titles[3], meta))
    {
        return false;
    }

    strnfmt(meta, sizeof(meta), "%s", blitz_effect_mode_name(setup->effect_mode));
    if (!app_ui_panel_add_row(panel, 4, selected == 4 ? TERM_L_BLUE : TERM_WHITE,
            true, selected == 4, "", titles[4], meta))
    {
        return false;
    }

    switch (selected)
    {
    case 0:
        detail = "Choose a selected character, a fully random character, or a random character with manual stat assignment.";
        break;
    case 1:
        detail = "Enable oath selection during birth, or skip oaths entirely for this Blitz run.";
        break;
    case 2:
        detail = "Pick how many blessings will be applied before the run starts.";
        break;
    case 3:
        detail = "Pick how many curses will be applied before the run starts. Curses can never be fewer than blessings.";
        break;
    case 4:
        detail = "Choose whether effects are random, selected from a list, or selected with full descriptions and effect text.";
        break;
    default:
        break;
    }

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE,
        (selected >= 0 && selected < 5) ? titles[selected] : "Blitz Setup");
    if (!birth_ui_panel_add_wrapped_lines(panel, TERM_WHITE, detail, true))
        return false;

    if (steamdeck)
    {
        birth_prompt_label(steamdeck_confirm_key(), "A", keybuf,
            sizeof(keybuf));
        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                keybuf, "Begin"))
        {
            return false;
        }
        birth_prompt_label(steamdeck_back_key(), "B", keybuf,
            sizeof(keybuf));
        if (!app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                keybuf, "Back"))
        {
            return false;
        }
        if (!app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "D-pad", "Move/Change"))
        {
            return false;
        }
    }
    else
    {
        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                "Enter", "Begin")
            || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "Esc", "Back")
            || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "8/2", "Move")
            || !app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
                "4/6", "Change"))
        {
            return false;
        }
    }

    return true;
}

static NavResult blitz_setup_menu(void)
{
    blitz_setup* setup = blitz_current_setup_mutable();
    int selected = 0;
    bool steamdeck = steamdeck_controls_active();
    birth_menu_scene_scope scene_scope;

    blitz_setup_clamp(setup);
    if (!birth_menu_scene_enter(&scene_scope, APP_WAIT_REASON_LIST_SELECTION))
    {
        log_warn("blitz setup: semantic menu scene unavailable");
        return NAV_TO_MAIN;
    }

    while (1)
    {
        app_ui_scene scene;
        char key;

        if (!blitz_setup_build_ui_scene(&scene, setup, selected)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("blitz setup: semantic scene presentation failed");
            birth_menu_scene_leave(&scene_scope);
            return NAV_TO_MAIN;
        }
        key = (char)ui_information_scene_wait_key_hidden_with_wait_reason(
            APP_WAIT_REASON_LIST_SELECTION);

        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()))
        {
            birth_menu_scene_leave(&scene_scope);
            return NAV_TO_MAIN;
        }

        if (key == '\n' || key == '\r' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()))
        {
            birth_menu_scene_leave(&scene_scope);
            return NAV_OK;
        }

        if (key == '8')
        {
            selected = (selected + 4) % 5;
            continue;
        }

        if (key == '2')
        {
            selected = (selected + 1) % 5;
            continue;
        }

        if (key != '4' && key != '6')
            continue;

        switch (selected)
        {
        case 0:
            if (key == '4')
                setup->character_mode = (setup->character_mode == BLITZ_CHARACTER_RANDOM)
                    ? BLITZ_CHARACTER_SELECTED
                    : setup->character_mode - 1;
            else
                setup->character_mode = (setup->character_mode == BLITZ_CHARACTER_SELECTED)
                    ? BLITZ_CHARACTER_RANDOM
                    : setup->character_mode + 1;
            break;
        case 1:
            setup->oaths_enabled = !setup->oaths_enabled;
            break;
        case 2:
            if (key == '4' && setup->blessing_count > 0)
                setup->blessing_count--;
            else if (key == '6' && setup->blessing_count < BLITZ_MAX_EFFECT_COUNT)
                setup->blessing_count++;
            break;
        case 3:
            if (key == '4' && setup->curse_count > 0)
                setup->curse_count--;
            else if (key == '6' && setup->curse_count < BLITZ_MAX_EFFECT_COUNT)
                setup->curse_count++;
            break;
        case 4:
            if (key == '4')
                setup->effect_mode = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
                    ? BLITZ_EFFECT_SELECTED_DESCR
                    : setup->effect_mode - 1;
            else
                setup->effect_mode = (setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR)
                    ? BLITZ_EFFECT_RANDOM
                    : setup->effect_mode + 1;
            break;
        default:
            break;
        }

        blitz_setup_clamp(setup);
    }
}

static void finalize_character_creation_selection(void)
{
    int i, j;

    /* Clear the base values of the skills */
    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_base[i] = 0;

    /* Clear the abilities and add bonus ability*/
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }

    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[p_ptr->pcharacter].a_adj[slot][0];
        int ab;

        if (stat < 0) break;
        ab = c_info[p_ptr->pcharacter].a_adj[slot][1];
        if (stat < S_MAX && ab < ABILITIES_MAX)
        {
            p_ptr->innate_ability[stat][ab] = true;
            p_ptr->active_ability[stat][ab] = true;
        }
    }

    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
        op_ptr->opt[OPT_ADULT + (i - OPT_BIRTH)] = op_ptr->opt[i];

    for (i = OPT_CHEAT; i < OPT_ADULT; i++)
        op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = op_ptr->opt[i];

    if (strlen(op_ptr->full_name) == 0)
    {
        op_ptr->vault_drop_frequency = VDF_NORMAL;
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
    }

    if (op_ptr->main_combat_rolls > 4)
        op_ptr->main_combat_rolls = 0;
    if (op_ptr->narrative_banner_seconds > NARRATIVE_BANNER_SECONDS_MAX)
        op_ptr->narrative_banner_seconds = NARRATIVE_BANNER_SECONDS_DEFAULT;
    if (op_ptr->ability_desc_mode > 2)
        op_ptr->ability_desc_mode = 0;
    if (op_ptr->vault_drop_frequency > VDF_PLENTIFUL)
        op_ptr->vault_drop_frequency = VDF_NORMAL;
    if (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_OFF)
        op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
    if (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_OFF)
        op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER;
    if (op_ptr->intro_style > INTRO_STYLE_RANDOM)
        op_ptr->intro_style = INTRO_STYLE_RANDOM;
    if (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;

    for (i = 0; i < z_info->e_max; i++)
    {
        e_info[i].aware = false;
    }

    log_debug("Character creation step completed: %s %s",
        p_name + p_info[p_ptr->prace].name,
        c_name + c_info[p_ptr->pcharacter].name);
}

NavResult blitz_character_creation(void)
{
    blitz_runtime_reset();

    if (blitz_setup_menu() != NAV_OK)
        return NAV_TO_MAIN;

    if (blitz_current_setup()->character_mode == BLITZ_CHARACTER_SELECTED)
        return character_creation();

    blitz_pick_random_race_and_character();
    finalize_character_creation_selection();
    return NAV_OK;
}

/*
 * Helper function for 'player_birth()'.
 *
 * This function allows the player to select a race and character template, and
 * modify options (including the birth options).
 */
NavResult character_creation(void)
{
    int i;
    birth_menu_scene_scope scene_scope;
    int phase = 1;

    if (!birth_menu_scene_enter(&scene_scope, APP_WAIT_REASON_LIST_SELECTION))
    {
        log_warn("character creation: semantic menu scene unavailable");
        return NAV_TO_MAIN;
    }
    while (phase <= 2)
    {
        if (phase == 1)
        {
            /* Choose the player's race */
            if (!get_player_race())
            {
                birth_menu_scene_leave(&scene_scope);
                return NAV_TO_MAIN; /* Esc at first screen → back to main menu */
            }

            phase++;
        }

        if (phase == 2)
        {
            /* Choose the player's character template */
            if (!get_character_profile())
            {
                phase = 1;          /* Esc here → go back to race */
                continue;
            }

            phase++;
        }
    }
    (void)i;

    birth_menu_scene_leave(&scene_scope);
    finalize_character_creation_selection();

    /* Done */
    return NAV_OK;

}

static int oath_selectable_max_id(void)
{
    int max_oath_id = OATH_LIGHT;

    if (!z_info)
        return max_oath_id;
    if (z_info->oath_max <= 1)
        return 0;
    if (max_oath_id >= z_info->oath_max)
        max_oath_id = z_info->oath_max - 1;
    if (max_oath_id < 0)
        max_oath_id = 0;

    return max_oath_id;
}

static int oath_collect_visible(int available_mask, int* visible_oaths, int max_visible)
{
    int visible_count = 0;
    int max_oath_id = oath_selectable_max_id();

    if (visible_oaths && visible_count < max_visible)
        visible_oaths[visible_count] = 0;
    visible_count++;

    for (int i = 1; i <= max_oath_id; i++)
    {
        if (!(available_mask & (1 << (i - 1))) && !oath_banned(i))
            continue;

        if (visible_oaths && visible_count < max_visible)
            visible_oaths[visible_count] = i;

        visible_count++;
    }

    return visible_count;
}

static bool oath_option_selectable(int oath_id, int available_mask)
{
    if (oath_id == 0)
        return true;

    return ((available_mask & (1 << (oath_id - 1))) != 0) && !oath_banned(oath_id);
}

static void oath_move_highlight(int* highlight, int direction, int available_mask)
{
    int oath_max = oath_selectable_max_id() + 1;
    int original = *highlight;
    int next = *highlight;

    if (oath_max <= 0)
    {
        *highlight = 0;
        return;
    }

    do
    {
        next += direction;
        if (next < 0)
            next = oath_max - 1;
        if (next >= oath_max)
            next = 0;

        if ((next == 0)
            || (available_mask & (1 << (next - 1)))
            || oath_banned(next))
        {
            *highlight = next;
            return;
        }
    } while (next != original);
}

static bool oath_build_ui_scene(app_ui_scene* scene, int available_mask,
    int highlight)
{
    app_ui_panel* panel;
    int visible_oaths[16];
    int visible_count;
    bool steamdeck = steamdeck_controls_active();

    if (!scene)
        return false;

    visible_count = oath_collect_visible(available_mask, visible_oaths,
        (int)N_ELEMENTS(visible_oaths));
    if (visible_count > (int)N_ELEMENTS(visible_oaths))
        visible_count = (int)N_ELEMENTS(visible_oaths);

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, TERM_L_BLUE, "Choose your Oath");

    for (int i = 0; i < visible_count; i++)
    {
        int oath_id = visible_oaths[i];
        char keybuf[8];
        byte attr;
        bool enabled = oath_option_selectable(oath_id, available_mask);

        birth_selection_row_key(i, keybuf, sizeof(keybuf));
        if (oath_banned(oath_id) && oath_id > 0)
            attr = (highlight == oath_id) ? TERM_L_RED : TERM_RED;
        else if (highlight == oath_id)
            attr = TERM_L_BLUE;
        else if (oath_id == 0)
            attr = TERM_WHITE;
        else
            attr = TERM_WHITE;

        if (!app_ui_panel_add_row(panel, oath_id, attr, enabled,
                highlight == oath_id, keybuf, oath_name_str(oath_id), ""))
        {
            return false;
        }
        if (highlight == oath_id)
            panel->selected_row = (s16b)i;
    }

    app_ui_panel_set_detail_title(panel,
        oath_banned(highlight) ? TERM_L_RED
            : (highlight == 0 ? TERM_WHITE : TERM_L_BLUE),
        oath_name_str(highlight));

    if (oath_banned(highlight) && highlight > 0)
    {
        char* banned_text = oath_banned_text(highlight);

        if (!app_ui_panel_add_detail_line(panel, TERM_L_RED, "OATH BROKEN"))
            return false;
        if (!birth_ui_panel_add_wrapped_lines(panel, TERM_RED,
                (banned_text && banned_text[0]) ? banned_text
                    : "Thy oath lies shattered, and thy name is marked in shame for this age.",
                true))
        {
            return false;
        }
    }
    else if (highlight == 0)
    {
        if (!birth_ui_panel_add_wrapped_lines(panel, TERM_SLATE,
                "Walk free of binding words.", true)
            || !birth_ui_panel_add_wrapped_lines(panel, TERM_SLATE,
                "Take no oath and remain unbound by sacred vows.", true))
        {
            return false;
        }
    }
    else
    {
        char line_buf[768];

        if (oath_description(highlight) && oath_description(highlight)[0])
        {
            strnfmt(line_buf, sizeof(line_buf), "Description: %s",
                oath_description(highlight));
            if (!birth_ui_panel_add_wrapped_lines(panel, TERM_SLATE, line_buf,
                    true))
            {
                return false;
            }
        }
        if (oath_pledge(highlight) && oath_pledge(highlight)[0])
        {
            strnfmt(line_buf, sizeof(line_buf), "Pledge: %s",
                oath_pledge(highlight));
            if (!birth_ui_panel_add_wrapped_lines(panel, TERM_L_BLUE, line_buf,
                    true))
            {
                return false;
            }
        }
        if (oath_reward_text(highlight) && oath_reward_text(highlight)[0])
        {
            strnfmt(line_buf, sizeof(line_buf), "Reward: %s",
                oath_reward_text(highlight));
            if (!birth_ui_panel_add_wrapped_lines(panel, TERM_L_GREEN, line_buf,
                    true))
            {
                return false;
            }
        }
        if (oath_forbidden(highlight) && oath_forbidden(highlight)[0])
        {
            strnfmt(line_buf, sizeof(line_buf), "Forbidden: %s",
                oath_forbidden(highlight));
            if (!birth_ui_panel_add_wrapped_lines(panel, TERM_L_RED, line_buf,
                    true))
            {
                return false;
            }
        }
    }

    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Oaths grant power, but they bind your actions.");
    (void)app_ui_panel_add_body_line(panel, TERM_SLATE,
        "Breaking an oath brings curse and shame.");

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A",
            confirm_label, sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Select")
            || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back")
            || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "D-pad", "Navigate"))
        {
            return false;
        }
    }
    else if (!app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Select")
        || !app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Back")
        || !app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "8/2", "Navigate"))
    {
        return false;
    }

    return true;
}

/*
 * Oath selection screen.
 *
 * Wide screens keep the split list/details layout. Compact screens use a
 * dedicated list page plus a full-width details page with vertical scrolling.
 */
static NavResult select_oath(void)
{
    int available_mask = get_available_oaths_mask();

    /* If no oaths are available, skip oath selection */
    if (available_mask == 0)
    {
        p_ptr->oath_type = 0; /* No oath */
        log_debug("No oaths available, skipping oath selection");
        return NAV_OK;
    }

    int highlight = 1; /* Start highlighting first available oath */
    int choice = 0;
    bool steamdeck = steamdeck_controls_active();

    /* Find first available oath to highlight */
    for (int i = 1; i <= oath_selectable_max_id(); i++)
    {
        if (available_mask & (1 << (i - 1)))
        {
            highlight = i;
            break;
        }
    }

    while (true)
    {
        int visible_oaths[16];
        int visible_count;
        char key;
        app_ui_scene scene;

        visible_count = oath_collect_visible(available_mask, visible_oaths,
            (int)N_ELEMENTS(visible_oaths));
        if (visible_count > (int)N_ELEMENTS(visible_oaths))
            visible_count = (int)N_ELEMENTS(visible_oaths);

        if (!oath_build_ui_scene(&scene, available_mask, highlight)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("oath selection: semantic scene presentation failed");
            return NAV_BACK;
        }
        key = (char)ui_information_scene_wait_key_hidden_with_wait_reason(
            APP_WAIT_REASON_LIST_SELECTION);

        if (steamdeck && key == steamdeck_back_key())
            return NAV_BACK; /* Go back to character creation */
        if (key == ESCAPE || key == 'q')
        {
            return NAV_BACK; /* Go back to character creation */
        }

        if (birth_confirm_input(key, steamdeck) || key == '6')
        {
            /* Select current highlighted option */
            if (oath_option_selectable(highlight, available_mask))
            {
                choice = highlight;
                break;
            }
        }

        if (key >= 'a' && key < 'a' + visible_count)
        {
            int display_pos = key - 'a';

            if (display_pos >= 0 && display_pos < visible_count
                && oath_option_selectable(visible_oaths[display_pos], available_mask))
            {
                choice = visible_oaths[display_pos];
                break;
            }

            continue;
        }

        if (key == '8')
        {
            oath_move_highlight(&highlight, -1, available_mask);
        }

        if (key == '2')
        {
            oath_move_highlight(&highlight, 1, available_mask);
        }
    }

    /* Set the chosen oath */
    p_ptr->oath_type = choice;
    
    /* Grant corresponding oath special ability using oath.txt data */
    if (choice > 0 && choice < z_info->oath_max) 
    {
        oath_type *oath_ptr = &oath_info[choice];
        
        /* Apply ability reward from A: field in oath.txt */
        if (oath_ptr->reward_type > 0 && oath_ptr->reward_value > 0)
        {
            int skill_category = oath_ptr->reward_type;
            int ability_id = oath_ptr->reward_value;
            
            if (skill_category >= 0 && skill_category < S_MAX
                && ability_id >= 0 && ability_id < ABILITIES_MAX)
            {
                /* Grant the ability specified in oath.txt */
                p_ptr->have_ability[skill_category][ability_id] = true;
                p_ptr->innate_ability[skill_category][ability_id] = true;
                p_ptr->active_ability[skill_category][ability_id] = true;

                log_debug("Granted oath %d abilities from data: skill=%d, ability=%d",
                          choice, skill_category, ability_id);
            }
            else
            {
                log_warn("Oath %d ability out of bounds: skill=%d (max %d), ability=%d (max %d)",
                         choice, skill_category, S_MAX - 1, ability_id, ABILITIES_MAX - 1);
            }
        }
        else
        {
            log_debug("No ability reward found for oath %d", choice);
        }
    }
    
    if (choice == 0) {
        log_debug("No oath selected");
    } else {
        log_debug("Oath selected: %s (%d)", oath_name_str(choice), choice);
    }
    
    return NAV_OK;
}

/*
 * Initial stat costs.
 */
static const int birth_stat_costs[11]
    = { -4, -3, -2, -1, 0, 1, 3, 6, 10, 15, 21 };

#define MAX_COST 13

/* Forward declaration: used by compact skill allocation rendering. */
static int skill_cost(int base, int points);

static cptr blitz_curse_name_str(int id)
{
    cptr raw = cu_name + cu_info[id].name;
    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    return raw;
}

static cptr blitz_blessing_name_str(int id)
{
    if (cu_info[id].blessing_name)
    {
        cptr raw = cu_name + cu_info[id].blessing_name;
        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;
        return raw;
    }

    return blitz_curse_name_str(id);
}

static int blitz_collect_eligible_effect_ids(bool blessing, int ids[], int max_ids)
{
    int count = 0;

    for (int id = 0; z_info && id < z_info->cu_max && count < max_ids; id++)
    {
        int stacks = CURSE_GET(id);
        int blessing_stacks = (stacks < 0) ? -stacks : 0;
        int curse_stacks = (stacks > 0) ? stacks : 0;
        byte curse_cap = (byte)CURSE_CURSE_CAP(id);
        byte blessing_cap = (byte)CURSE_BLESSING_CAP(id);

        if (blessing)
        {
            if (!cu_info[id].blessing_name)
                continue;
            if (stacks > 0)
                continue;
            if (blessing_cap > 0 && blessing_stacks >= blessing_cap)
                continue;
        }
        else
        {
            if (!cu_info[id].name)
                continue;
            if (curse_cap > 0 && curse_stacks >= curse_cap)
                continue;
        }

        ids[count++] = id;
    }

    return count;
}

static int blitz_weighted_random_curse_pick(void)
{
    long total = 0;
    int w_max = 1;
    bool tilt = (p_info[p_ptr->prace].flags & RHF_CURSE)
        || (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name)
            continue;
        if (cu_info[i].weight > w_max)
            w_max = cu_info[i].weight;
    }

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        byte w = cu_info[i].weight ? cu_info[i].weight : 1;
        int cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        long base;

        if (!cu_info[i].name)
            continue;
        if (cap && cnt >= cap)
            continue;
        if (tilt && w == w_max)
            continue;

        base = tilt ? w + ((w_max + 1 - w) >> 1) : w;
        total += base / (cnt + 1);
    }

    if (!total)
        return -1;

    long pick = rand_int(total);
    long run = 0;
    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        byte w = cu_info[i].weight ? cu_info[i].weight : 1;
        int cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        long base;
        long eff;

        if (!cu_info[i].name)
            continue;
        if (cap && cnt >= cap)
            continue;
        if (tilt && w == w_max)
            continue;

        base = tilt ? w + ((w_max + 1 - w) >> 1) : w;
        eff = base / (cnt + 1);
        run += eff;
        if (pick < run)
            return i;
    }

    return -1;
}

static int blitz_weighted_random_blessing_pick(void)
{
    int eligible[METAR_CURSE_SLOTS];
    int weights[METAR_CURSE_SLOTS];
    int count = 0;
    int total_weight = 0;

    for (int id = 0; z_info && id < z_info->cu_max && count < METAR_CURSE_SLOTS; id++)
    {
        int stacks = CURSE_GET(id);
        int blessing_stacks = (stacks < 0) ? -stacks : 0;
        int base_weight;
        int effective_weight;

        if (!cu_info[id].blessing_name)
            continue;
        if (stacks > 0)
            continue;
        if (CURSE_BLESSING_CAP(id) > 0
            && blessing_stacks >= CURSE_BLESSING_CAP(id))
            continue;

        eligible[count] = id;
        base_weight = cu_info[id].weight > 0 ? cu_info[id].weight : 1;
        effective_weight = base_weight / (blessing_stacks + 1);
        weights[count] = (effective_weight > 0) ? effective_weight : 1;
        total_weight += weights[count];
        count++;
    }

    if (count <= 0 || total_weight <= 0)
        return -1;

    int roll = rand_int(total_weight);
    int sum = 0;
    for (int i = 0; i < count; i++)
    {
        sum += weights[i];
        if (roll < sum)
            return eligible[i];
    }

    return eligible[0];
}

static bool blitz_effect_picker_build_ui_scene(app_ui_scene* scene,
    bool blessing, bool show_effects, int ordinal, int total,
    const int ids[], int count, int selected)
{
    app_ui_panel* panel;
    int selected_id;
    curse_type* cu;
    cptr desc;
    cptr power;
    bool steamdeck = steamdeck_controls_active();
    char title[80];

    if (!scene || !ids || count <= 0 || selected < 0 || selected >= count)
        return false;

    selected_id = ids[selected];
    cu = &cu_info[selected_id];
    desc = blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : "")
        : (cu->text ? cu_text + cu->text : "");
    power = blessing
        ? (cu->blessing_power ? cu_text + cu->blessing_power : "")
        : (cu->power ? cu_text + cu->power : "");

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SHOW_DETAIL
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->focus_area = APP_UI_FOCUS_ROWS;
    panel->selected_row = selected;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 980, 1800);
    strnfmt(title, sizeof(title), "Choose %s %d of %d",
        blessing ? "Blessing" : "Curse", ordinal, total);
    app_ui_panel_set_title(panel, TERM_YELLOW, title);
    app_ui_panel_set_subtitle(panel, blessing ? TERM_L_GREEN : TERM_L_RED,
        blessing ? "Blessings" : "Curses");

    for (int row = 0; row < count; row++)
    {
        int id = ids[row];
        cptr name = blessing ? blitz_blessing_name_str(id)
                             : blitz_curse_name_str(id);
        byte attr = (row == selected)
            ? TERM_L_BLUE
            : (blessing ? TERM_L_GREEN : TERM_L_RED);

        if (!app_ui_panel_add_row(panel, id, attr, true,
                row == selected, "", name, ""))
        {
            return false;
        }
    }

    app_ui_panel_set_detail_title(panel, TERM_WHITE,
        blessing ? blitz_blessing_name_str(selected_id)
                 : blitz_curse_name_str(selected_id));
    if (desc && desc[0]
        && !birth_ui_panel_add_wrapped_lines(panel, TERM_SLATE, desc, true))
    {
        return false;
    }

    if (show_effects && power && power[0])
    {
        char power_line[512];

        if (panel->detail_line_count > 0
            && !app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
        {
            return false;
        }

        strnfmt(power_line, sizeof(power_line), "Effect: %s", power);
        if (!birth_ui_panel_add_wrapped_lines(panel,
                blessing ? TERM_L_GREEN : TERM_L_RED, power_line, true))
        {
            return false;
        }
    }

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                confirm_label, "Select")
            && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Back")
            && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "D-pad", "Navigate");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Enter", "Select")
        && app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "Esc", "Back")
        && app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "8/2", "Navigate");
}

static int blitz_select_effect_from_list(bool blessing, bool show_effects,
    int ordinal, int total)
{
    int ids[METAR_CURSE_SLOTS];
    int count = blitz_collect_eligible_effect_ids(blessing, ids,
        METAR_CURSE_SLOTS);
    int selected = 0;
    bool steamdeck = steamdeck_controls_active();

    if (count <= 0)
        return -1;

    while (1)
    {
        app_ui_scene scene;
        int selected_id = ids[selected];
        char key;

        if (!blitz_effect_picker_build_ui_scene(&scene, blessing,
                show_effects, ordinal, total, ids, count, selected)
            || !ui_information_scene_present_ui(&scene))
        {
            log_warn("blitz effect picker: semantic scene presentation failed");
            return -1;
        }

        key = (char)ui_information_scene_wait_key_hidden_with_wait_reason(
            APP_WAIT_REASON_LIST_SELECTION);

        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()))
            return -1;
        if (key == '\n' || key == '\r' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()))
            return selected_id;
        if (key == '8')
        {
            selected = (selected + count - 1) % count;
            continue;
        }
        if (key == '2')
        {
            selected = (selected + 1) % count;
            continue;
        }
    }
}

static void blitz_apply_effect_pick(int id, bool blessing)
{
    CURSE_ADD(id, blessing ? -1 : 1);
    CURSE_SEEN_SET(id);
}

static bool blitz_effect_summary_build_ui_scene(app_ui_scene* scene)
{
    app_ui_panel* panel;
    bool steamdeck = steamdeck_controls_active();

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 900, 1600);
    app_ui_panel_set_title(panel, TERM_YELLOW, "Blitz Effects");
    app_ui_panel_set_subtitle(panel, TERM_SLATE,
        "Starting blessings and curses for this Blitz run.");

    for (int id = 0; z_info && id < z_info->cu_max; id++)
    {
        int stacks = CURSE_GET(id);
        char line[128];

        if (stacks == 0)
            continue;

        strnfmt(line, sizeof(line), "%s x%d",
            (stacks < 0) ? blitz_blessing_name_str(id)
                         : blitz_curse_name_str(id),
            (stacks < 0) ? -stacks : stacks);
        if (!app_ui_panel_add_body_line(panel,
                stacks < 0 ? TERM_L_GREEN : TERM_L_RED, line))
        {
            return false;
        }
    }

    if (panel->body_line_count == 0
        && !app_ui_panel_add_body_line(panel, TERM_SLATE,
            "No blessings or curses selected."))
    {
        return false;
    }

    if (steamdeck)
    {
        char confirm_label[16];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            confirm_label, "Continue");
    }

    return app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Any key", "Continue");
}

static void blitz_show_effect_summary(void)
{
    app_ui_scene scene;

    if (!blitz_effect_summary_build_ui_scene(&scene)
        || !ui_information_scene_present_ui(&scene))
    {
        log_warn("blitz effect summary: semantic scene presentation failed");
        return;
    }

    (void)ui_information_scene_wait_key_hidden_with_wait_reason(
        APP_WAIT_REASON_INFORMATIONAL_PAUSE);
}

static NavResult blitz_configure_effects(void)
{
    const blitz_setup* setup = blitz_current_setup();

    blitz_runtime_reset();

    for (int i = 0; i < setup->curse_count; i++)
    {
        int id = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
            ? blitz_weighted_random_curse_pick()
            : blitz_select_effect_from_list(false,
                setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR, i + 1, setup->curse_count);
        if (id < 0)
            return NAV_BACK;
        blitz_apply_effect_pick(id, false);
    }

    for (int i = 0; i < setup->blessing_count; i++)
    {
        int id = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
            ? blitz_weighted_random_blessing_pick()
            : blitz_select_effect_from_list(true,
                setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR, i + 1, setup->blessing_count);
        if (id < 0)
            return NAV_BACK;
        blitz_apply_effect_pick(id, true);
    }

    if (setup->curse_count > 0 || setup->blessing_count > 0)
        blitz_show_effect_summary();

    return NAV_OK;
}

static void blitz_auto_assign_stats(int stats[A_MAX])
{
    int cost = 0;

    for (int i = 0; i < A_MAX; i++)
        stats[i] = 0;

    while (cost < MAX_COST)
    {
        int choices[A_MAX];
        int choice_count = 0;

        for (int i = 0; i < A_MAX; i++)
        {
            int next = stats[i] + 1;
            int next_cost;

            if (next > 6)
                continue;
            next_cost = cost - birth_stat_costs[stats[i] + 4]
                + birth_stat_costs[next + 4];
            if (next_cost <= MAX_COST)
                choices[choice_count++] = i;
        }

        if (choice_count <= 0)
            break;

        int pick = choices[rand_int(choice_count)];
        cost -= birth_stat_costs[stats[pick] + 4];
        stats[pick]++;
        cost += birth_stat_costs[stats[pick] + 4];
    }
}

static void blitz_auto_assign_skills(void)
{
    int old_base[S_MAX];
    int gains[S_MAX];
    int budget;

    for (int i = 0; i < S_MAX; i++)
    {
        old_base[i] = p_ptr->skill_base[i];
        gains[i] = 0;
    }

    budget = p_ptr->new_exp;

    while (budget > 0)
    {
        int choices[S_MAX];
        int weights[S_MAX];
        int choice_count = 0;
        int total_weight = 0;

        for (int i = 0; i < S_MAX; i++)
        {
            int delta;
            int weight = 2;

            if (i == S_SPC)
                continue;

            delta = skill_cost(old_base[i], gains[i] + 1)
                - skill_cost(old_base[i], gains[i]);
            if (delta <= 0 || delta > budget)
                continue;

            for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
            {
                int skill_idx = c_info[p_ptr->pcharacter].a_adj[slot][0];
                if (skill_idx < 0)
                    break;
                if (skill_idx == i)
                    weight += 3;
            }

            choices[choice_count] = i;
            weights[choice_count] = weight;
            total_weight += weight;
            choice_count++;
        }

        if (choice_count <= 0)
            break;

        int roll = rand_int(total_weight);
        int sum = 0;
        int chosen = choices[0];
        for (int i = 0; i < choice_count; i++)
        {
            sum += weights[i];
            if (roll < sum)
            {
                chosen = choices[i];
                break;
            }
        }

        budget -= skill_cost(old_base[chosen], gains[chosen] + 1)
            - skill_cost(old_base[chosen], gains[chosen]);
        gains[chosen]++;
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (i == S_SPC)
            continue;
        p_ptr->skill_base[i] = old_base[i] + gains[i];
    }

    p_ptr->new_exp = budget;
}

static NavResult blitz_auto_build_character(void)
{
    int stats[A_MAX];

    get_extra();
    blitz_auto_assign_stats(stats);

    for (int i = 0; i < A_MAX; i++)
    {
        int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);
        p_ptr->stat_base[i] = stats[i] + bonus;
        p_ptr->stat_drain[i] = 0;
    }

    p_ptr->update |= (PU_BONUS | PU_HP);
    update_stuff();
    p_ptr->chp = p_ptr->mhp;
    calc_voice();
    p_ptr->csp = p_ptr->msp;

    blitz_auto_assign_skills();
    p_ptr->update |= (PU_BONUS);
    update_stuff();
    p_ptr->chp = p_ptr->mhp;
    calc_voice();
    p_ptr->csp = p_ptr->msp;

    return NAV_OK;
}

/*
 * Helper function for 'player_birth()'.
 */
static NavResult player_birth_aux_2(void)
{
    int i;

    int stat = 0;

    int stats[A_MAX];

    int cost;

    char ch;

    /* Initialize stats */
    for (i = 0; i < A_MAX; i++)
    {
        /* Initial stats */
        stats[i] = p_ptr->stat_base[i];
    }

    /* Determine experience and things */
    get_extra();

    /* Show tutorial for first-time players (when scorefile is empty) */
    /* Do this AFTER get_extra() so character has stats/abilities to display */
    /* Check every time - show tutorial for every new character if scores file is empty */
    log_debug("Checking if tutorial should be shown...");
    bool is_empty = highscore_is_empty();
    log_debug("highscore_is_empty() returned: %s", is_empty ? "true" : "false");
    if (!run_mode_is_blitz() && is_empty)
    {
        log_info("First-time player detected - showing character screen tutorial");
        
        /* Initialize character stats for display - same as first iteration of stats loop */
        for (i = 0; i < A_MAX; i++)
        {
            /* Obtain bonuses for race/character */
            int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);
            
            /* Set base stats (0 + racial/character bonuses) */
            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;
        }
        
        /* Calculate bonuses and hitpoints */
        p_ptr->update |= (PU_BONUS | PU_HP);
        update_stuff();
        
        /* Fully healed */
        p_ptr->chp = p_ptr->mhp;
        
        /* Fully rested */
        calc_voice();
        p_ptr->csp = p_ptr->msp;
        
        /* Now show the tutorial with a realistic character sheet */
        display_character_tutorial();
        log_info("Character screen tutorial completed");
    }
    else
    {
        log_info("Not showing tutorial - scores file has entries");
    }

    log_trace("Starting stats allocation interface");

    /* Interact */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();

        /* Reset cost */
        cost = 0;

        /* Process stats */
        for (i = 0; i < A_MAX; i++)
        {
            /* Obtain a "bonus" for "race" */
            int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);

            /* Apply the racial bonuses */
            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;

            /* Total cost */
            cost += birth_stat_costs[stats[i] + 4];
        }

        /* Restrict cost */
        if (cost > MAX_COST)
        {
            /* Warning */
            bell("Excessive stats!");

            /* Reduce stat */
            stats[stat]--;

            /* Recompute costs */
            continue;
        }

        p_ptr->new_exp = p_ptr->exp = get_start_xp();

        /* Calculate the bonuses and hitpoints */
        p_ptr->update |= (PU_BONUS | PU_HP);

        /* Update stuff */
        update_stuff();

        /* Fully healed */
        p_ptr->chp = p_ptr->mhp;

        /* Fully rested */
        calc_voice();
        p_ptr->csp = p_ptr->msp;

        if (!birth_present_stats_allocation_ui_scene(stats, stat,
                MAX_COST - cost, steamdeck))
        {
            log_warn("birth stats allocation: semantic scene unavailable");
            return NAV_TO_MAIN;
        }

        /* Get key */
        ch = (char)ui_information_scene_wait_key_hidden_with_wait_reason(
            APP_WAIT_REASON_LIST_SELECTION);

        /* Quit -> return to main menu before the game starts */
        if ((ch == 'Q') || (ch == 'q')) {
            if (turn == 0) return NAV_TO_MAIN;
            return NAV_QUIT;
        }

        /* Back to Character Selection */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
            return NAV_BACK;

        /* Done */
        if (birth_confirm_input(ch, steamdeck))
            return NAV_OK;

        /* Prev stat */
        if (ch == '8')
        {
            stat = (stat + A_MAX - 1) % A_MAX;
        }

        /* Next stat */
        if (ch == '2')
        {
            stat = (stat + 1) % A_MAX;
        }

        /* Decrease stat */
        if ((ch == '4') && (stats[stat] > 0))
        {
            stats[stat]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            stats[stat]++;
        }
    }

    /* Shouldn't reach; default to back */
    return NAV_BACK;
}

/*
 * Skill point costs.
 *
 * The nth skill point costs (100*n) experience points
 */
static int skill_cost(int base, int points)
{
    int total_cost = (points + base) * (points + base + 1) / 2;
    int prev_cost = (base) * (base + 1) / 2;
    return ((total_cost - prev_cost) * 100);
}

/*
 * Increase your skills by spending experience points
 */
extern NavResult gain_skills(void)
{
    int i;

    int skill = 0;

    int old_base[S_MAX];
    int skill_gain[S_MAX];

    int old_new_exp = p_ptr->new_exp;
    int total_cost = 0;

    char ch;

    NavResult result = NAV_OK;

    log_debug("Starting skills allocation with %d experience points", p_ptr->new_exp);

    // hack global variable
    skill_gain_in_progress = true;

    /* save the old skills */
    for (i = 0; i < S_MAX; i++)
        old_base[i] = p_ptr->skill_base[i];

    /* initialise the skill gains */
    for (i = 0; i < S_MAX; i++)
        skill_gain[i] = 0;

    /* Interact */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();

        /* Recompute points/costs and apply the temporary skill increases */
        total_cost = 0;

        for (i = 0; i < S_MAX; i++)
        {
            /* Skip Special abilities skill - not trainable */
            if (i == S_SPC) continue;
            total_cost += skill_cost(old_base[i], skill_gain[i]);
        }

        p_ptr->new_exp = old_new_exp - total_cost;

        if (p_ptr->new_exp < 0)
        {
            bell("Excessive skills!");
            skill_gain[skill]--;
            continue;
        }

        p_ptr->update |= (PU_BONUS);
        p_ptr->redraw |= (PR_EXP | PR_BASIC);

        for (i = 0; i < S_MAX; i++)
        {
            if (i == S_SPC) continue;
            p_ptr->skill_base[i] = old_base[i] + skill_gain[i];
        }

        update_stuff();

        if (!birth_present_skills_allocation_ui_scene(skill, old_base,
                skill_gain, p_ptr->new_exp, steamdeck))
        {
            log_warn("birth skills allocation: semantic scene unavailable");
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC)
                    p_ptr->skill_base[i] = old_base[i];
            }
            skill_gain_in_progress = false;
            return NAV_TO_MAIN;
        }

        /* Get key */
        ch = (char)ui_information_scene_wait_key_hidden_with_wait_reason(
            APP_WAIT_REASON_LIST_SELECTION);

        /* Quit -> back to main menu before the game starts */
        if (((ch == 'Q') || (ch == 'q')) && (turn == 0)) {
            /* restore state before leaving */
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            skill_gain_in_progress = false;
            return NAV_TO_MAIN;
        }

        /* Done */
        if (birth_confirm_input(ch, steamdeck))
        {
            if (birth_pending_compact_description_confirm)
            {
                if (!birth_show_semantic_assignment_review(steamdeck))
                    continue;
                birth_pending_compact_description_confirm = false;
            }
            result = NAV_OK;
            break;
        }

        /* Abort */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
        {
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            result = NAV_BACK;   /* go back to Character Selection */
            break;
        }

        /* Prev skill */
        if (ch == '8')
        {
            do {
                skill = (skill + S_MAX - 1) % S_MAX;
            } while (skill == S_SPC); /* Skip Special abilities skill */
        }

        /* Next skill */
        if (ch == '2')
        {
            do {
                skill = (skill + 1) % S_MAX;
            } while (skill == S_SPC); /* Skip Special abilities skill */
        }

        /* Decrease skill */
        if ((ch == '4') && (skill_gain[skill] > 0))
        {
            skill_gain[skill]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            /* Don't allow increasing Special abilities skill */
            if (skill != S_SPC) {
                skill_gain[skill]++;
            }
        }
    }

    // reset hack global variable
    skill_gain_in_progress = false;

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Update stuff */
    update_stuff();

    log_debug("Skills allocation completed, spent %d experience", old_new_exp - p_ptr->new_exp);

    /* Done */
    return result;
}

#define BASE_COLUMN 7
#define STAT_TITLE_ROW 14
#define BASE_STAT_ROW 16

/*
 * Helper function for 'player_birth()'.
 *
 * See "display_player" for screen layout code.
 */
static NavResult player_birth_aux(void)
{
    NavResult result = NAV_OK;
    ui_information_scene_scope semantic_scope;


    log_debug("Initializing character data and history");
    birth_pending_compact_description_confirm = true;

    SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name, sizeof(op_ptr->full_name));
    process_player_name(true);  /* CRITICAL: Must pass true to update savefile path! */
    /* Clear the previous history strings */
    p_ptr->history[0] = '\0';
    SDL_strlcat(
                p_ptr->history, (c_text + c_info[p_ptr->pcharacter].text), sizeof(p_ptr->history));

    p_ptr->wt = 0;
    p_ptr->ht = 0;
    p_ptr->age = 0;

    if (!ui_information_scene_enter(&semantic_scope))
    {
        log_error("player_birth_aux: semantic assignment scene unavailable");
        return NAV_TO_MAIN;
    }

    /* Oath selection (after character creation, before tutorial/stats) */
    if (run_mode_is_blitz() && !blitz_oaths_enabled())
    {
        p_ptr->oath_type = 0;
    }
    else
    {
        log_debug("Entering oath selection");
        NavResult oath_result = select_oath();
        if (oath_result != NAV_OK)
        {
            result = oath_result;
            goto cleanup_semantic_birth_ui;
        }
        log_debug("Oath selection completed");
    }

    if (run_mode_is_blitz())
    {
        NavResult blitz_effects = blitz_configure_effects();
        if (blitz_effects != NAV_OK)
        {
            result = blitz_effects;
            goto cleanup_semantic_birth_ui;
        }
    }

    /* Point-based flow */
    if (blitz_auto_allocates_stats())
    {
        NavResult auto_result = blitz_auto_build_character();
        if (auto_result != NAV_OK)
        {
            result = auto_result;
            goto cleanup_semantic_birth_ui;
        }
    }
    else
    {
        for (;;)
        {
            /* Stats allocation screen */
            log_debug("Entering stats allocation");
            NavResult s = player_birth_aux_2();
            if (s == NAV_OK) {
                /* Skill allocation: may return NAV_BACK / NAV_TO_MAIN */
                log_debug("Stats accepted, entering skills allocation");
                NavResult g = gain_skills();
                if (g != NAV_OK)
                {
                    result = g;
                    break;
                }
                log_debug("Skills allocation completed");
                break; /* accepted */
            }
            if (s == NAV_BACK)
            {
                result = NAV_BACK;
                break;
            }
            if (s == NAV_TO_MAIN)
            {
                result = NAV_TO_MAIN;
                break;
            }
            if (s == NAV_QUIT)
            {
                result = NAV_QUIT;
                break;
            }
            /* any other value: loop again */
        }
    }

    // Reset the number of artefacts
    p_ptr->artefacts = 0;

    log_trace("Final character stats: Str=%d Dex=%d Con=%d Gra=%d",
              p_ptr->stat_base[A_STR], p_ptr->stat_base[A_DEX],
              p_ptr->stat_base[A_CON], p_ptr->stat_base[A_GRA]);

cleanup_semantic_birth_ui:
    ui_information_scene_leave(&semantic_scope);

    if (result != NAV_OK)
        return result;

    /* Accept */
    return result;
}

/*
 * Create a new character.
 *
 * Note that we may be called with "junk" leftover in the various
 * fields, so we must be sure to clear them first.
 */
NavResult player_birth()
{
    int i;

    char raw_date[25];
    char clean_date[25];
    char month[4];
    time_t ct = time((time_t*)0);

    log_info("Starting character creation process");
    killer_reset();

    /* Create a new character */
    while (1)
    {
        NavResult r = player_birth_aux();
        if (r == NAV_OK) break;
        if (r == NAV_BACK) return NAV_BACK;         /* back to character_selection */
        if (r == NAV_TO_MAIN) return NAV_TO_MAIN;   /* back to main menu */
        if (r == NAV_QUIT) return NAV_QUIT;         /* hard exit */
        /* Any other value -> retry loop */
    }

    for (i = 0; i < NOTES_LENGTH; i++)
    {
        notes_buffer[i] = '\0';
    }

    /* Get date */
    (void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));

    sprintf(month, "%.2s", raw_date + 5);
    atomonth(atoi(month), month);

    if (*(raw_date + 7) == '0')
        sprintf(
            clean_date, "%.1s %.3s %.4s", raw_date + 8, month, raw_date + 1);
    else
        sprintf(
            clean_date, "%.2s %.3s %.4s", raw_date + 7, month, raw_date + 1);

    /* Add in "character start" information */
    SDL_strlcat(notes_buffer,
        format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name),
        sizeof(notes_buffer));
    SDL_strlcat(notes_buffer, format("Entered Angband on %s\n", clean_date),
        sizeof(notes_buffer));
    SDL_strlcat(
        notes_buffer, "\n   Turn     Depth   Note\n\n", sizeof(notes_buffer));

    /* Note player birth in the message recall */
    message_add(" ", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add("====================", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add(" ", MSG_GENERIC);

    /* Hack -- outfit the player */
    player_outfit();

    /* Load persistent settings from metarun if this is a continuing metarun */
    if (!run_mode_is_blitz())
        metarun_load_persistent_settings();

    /* Reapply app-wide settings after character creation so UI preferences are
     * not sourced from the metarun or savefile. */
    platform_load_app_options();

    log_info("Character creation completed: %s the %s", op_ptr->full_name, p_name + rp_ptr->name);

    return NAV_OK;
}















