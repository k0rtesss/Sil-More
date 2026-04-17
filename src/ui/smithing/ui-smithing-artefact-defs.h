/* File: ui-smithing-artefact-defs.h */
/* Lane-local implementation fragment included by ui-smithing-screen.c. */

typedef struct smithing_flag_cat
{
    int category;
    cptr desc;
} smithing_flag_cat;

#define CAT_STAT 1
#define CAT_SUST 2
#define CAT_SKILL 3
#define CAT_MEL 4
#define CAT_SLAY 5
#define CAT_RES 6
#define CAT_MISC 7

#define MAX_CATS 7

#define MAX_SMITHING_FLAGS (32 * 4)

static const smithing_flag_cat smithing_flag_cats[]
    = { { CAT_STAT, "Stat bonuses" }, { CAT_SUST, "Sustains" },
          { CAT_SKILL, "Skill bonuses" }, { CAT_MEL, "Melee powers" },
          { CAT_SLAY, "Slays" }, { CAT_RES, "Resistances" },
          { CAT_MISC, "Misc" } };

/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_desc
{
    int category;
    u32b flag;
    int flagset;
    cptr desc;
} smithing_flag_desc;

/*
 * A list of tvals and their textual names
 */
static const smithing_flag_desc smithing_flag_types[] = { { CAT_STAT, TR1_STR,
                                                              1, "Str bonus" },
    { CAT_STAT, TR1_DEX, 1, "Dex bonus" },
    { CAT_STAT, TR1_CON, 1, "Con bonus" },
    { CAT_STAT, TR1_GRA, 1, "Gra bonus" },
    { CAT_STAT, TR1_NEG_STR, 1, "Str penalty" },
    { CAT_STAT, TR1_NEG_DEX, 1, "Dex penalty" },
    { CAT_STAT, TR1_NEG_CON, 1, "Con penalty" },
    { CAT_STAT, TR1_NEG_GRA, 1, "Gra penalty" },
    { CAT_SKILL, TR1_ARC, 1, "Archery" }, { CAT_SKILL, TR1_STL, 1, "Stealth" },
    { CAT_SKILL, TR1_PER, 1, "Perception" }, { CAT_SKILL, TR1_WIL, 1, "Will" },
    { CAT_SKILL, TR1_SMT, 1, "Smithing" }, { CAT_SKILL, TR1_SNG, 1, "Song" },
    { CAT_MISC, TR1_DAMAGE_SIDES, 1, "Damage bonus" },
    { CAT_MISC, TR2_LIGHT, 2, "Light" },
    { CAT_MISC, TR2_SLOW_DIGEST, 2, "Sustenance" },
    { CAT_MISC, TR2_REGEN, 2, "Regeneration" },
    { CAT_MISC, TR2_SEE_INVIS, 2, "See Invisible" },
    { CAT_MISC, TR2_FREE_ACT, 2, "Free Action" },
    { CAT_MISC, TR2_SPEED, 2, "Speed" },
    { CAT_MISC, TR2_RADIANCE, 2, "Radiance" },
    { CAT_MISC, TR3_CHEAT_DEATH, 3, "Cheat Death" },
    { CAT_MISC, TR3_STAND_FAST, 3, "Stand Fast" },
    { CAT_MISC, TR3_AVOID_TRAPS, 3, "Avoid Traps" },
    { CAT_MISC, TR3_MEDIC, 3, "Medicine Bonus" },
    { CAT_MISC, TR4_PROT_FIRE, 4, "Protection vs Fire" },
    { CAT_MISC, TR4_PROT_COLD, 4, "Protection vs Cold" },
    { CAT_MISC, TR4_PROT_POIS, 4, "Protection vs Poison" },
    { CAT_MISC, TR4_PROT_DARK, 4, "Protection vs Darkness" },
    { CAT_MEL, TR1_TUNNEL, 1, "Tunneling Bonus" },
    { CAT_MEL, TR1_SHARPNESS, 1, "Sharpness" },
    { CAT_MEL, TR1_SHARPNESS2, 1, "Sharpness2" },
    { CAT_MEL, TR1_VAMPIRIC, 1, "Vampiric" },
    { CAT_MEL, TR3_ACCURATE, 3, "Accurate" },
    { CAT_SLAY, TR1_SLAY_ORC, 1, "Slay Orc" },
    { CAT_SLAY, TR1_SLAY_TROLL, 1, "Slay Troll" },
    { CAT_SLAY, TR1_SLAY_WOLF, 1, "Slay Wolf" },
    { CAT_SLAY, TR1_SLAY_SPIDER, 1, "Slay Spider" },
    { CAT_SLAY, TR1_SLAY_UNDEAD, 1, "Slay Undead" },
    { CAT_SLAY, TR1_SLAY_RAUKO, 1, "Slay Rauko" },
    { CAT_SLAY, TR1_SLAY_DRAGON, 1, "Slay Dragon" },
    { CAT_SLAY, TR4_SLAY_SERPENT, 4, "Slay Serpent" },
    { CAT_SLAY, TR4_SLAY_VAMPIRE, 4, "Slay Vampire" },
    { CAT_SLAY, TR4_SLAY_HORROR, 4, "Slay Horror" },
    { CAT_SLAY, TR4_SLAY_CAT, 4, "Slay Cat" },
    { CAT_SLAY, TR4_SLAY_GIANT, 4, "Slay Giant" },
    { CAT_SLAY, TR1_BRAND_COLD, 1, "Brand with Cold" },
    { CAT_SLAY, TR1_BRAND_FIRE, 1, "Brand with Fire" },
    { CAT_SLAY, TR1_BRAND_POIS, 1, "Brand with Poison" },
    { CAT_SUST, TR2_SUST_STR, 2, "Sustain Str" },
    { CAT_SUST, TR2_SUST_DEX, 2, "Sustain Dex" },
    { CAT_SUST, TR2_SUST_CON, 2, "Sustain Con" },
    { CAT_SUST, TR2_SUST_GRA, 2, "Sustain Gra" },
    { CAT_RES, TR2_RES_COLD, 2, "Resist Cold" },
    { CAT_RES, TR2_RES_FIRE, 2, "Resist Fire" },
    { CAT_RES, TR2_RES_POIS, 2, "Resist Poison" },
    { CAT_RES, TR2_RES_BLEED, 2, "Resist Bleeding" },
    { CAT_RES, TR2_RES_FEAR, 2, "Resist Fear" },
    { CAT_RES, TR2_RES_BLIND, 2, "Resist Blindness" },
    { CAT_RES, TR2_RES_CONFU, 2, "Resist Confusion" },
    { CAT_RES, TR2_RES_STUN, 2, "Resist Stunning" },
    { CAT_RES, TR2_RES_HALLU, 2, "Resist Hallucination" }, { 0, 0, 0, "" } };

typedef struct smith_ui_artefact_flag_menu_state
{
    int count;
    u32b flags[MAX_SMITHING_FLAGS];
    int flagsets[MAX_SMITHING_FLAGS];
    const char* labels[MAX_SMITHING_FLAGS];
    bool present[MAX_SMITHING_FLAGS];
    bool valid[MAX_SMITHING_FLAGS];
    bool affordable[MAX_SMITHING_FLAGS];
    byte row_attr[MAX_SMITHING_FLAGS];
} smith_ui_artefact_flag_menu_state;

typedef struct smith_ui_artefact_ability_menu_state
{
    int count;
    int ability_nums[64];
    const char* labels[64];
    bool present[64];
    bool valid[64];
    bool affordable[64];
    byte row_attr[64];
} smith_ui_artefact_ability_menu_state;
