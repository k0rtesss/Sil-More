/* --------------------------------------------------------------------
 *  src/metarun/metarun.c   (2025-07-06)   â€“ final, crash-free, warning-free
 * --------------------------------------------------------------------
 *  Tracks a â€œmeta-runâ€ that ends after 15 Silmarils (win) or
 *  15 deaths (lose).  Finished runs are appended to meta.raw so
 *  the entire history is preserved.  Includes:
 *     â€¢ list_metaruns()  â€“ compact history view
 *     â€¢ print_metarun_stats() â€“ details for current run
 * -------------------------------------------------------------------- */

#ifndef WINDOWS
#define _DEFAULT_SOURCE  /* For DT_DIR and other POSIX extensions */
#define _BSD_SOURCE      /* For setregid on older systems */
#endif

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "platform-audio.h"
#include "platform-config.h"
#include "platform-frame.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "platform-time.h"
#include "support/reliability-checks.h"
#include "app/app-ui.h"
#include "metarun.h"
#include "metarun/metarun-internal.h"
#include "runtime/runtime-game.h"
#include "score/score_entry.h"
#include "score/score_io.h"
#include "ui/ui-information-scene.h"
#include "h-define.h"
#include "platform.h"    /* MKDIR helper                      */
#include "supplies.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#ifdef WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif  

/* --------------------------------------------------------------- */
/*  metarun.c : quick-and-dirty logger                             */
/* --------------------------------------------------------------- */

/* Enable this to delete old save/score files on fresh metarun start.
 * Currently disabled to prevent accidental data loss during debugging. */
/* #define METARUN_CLEANUP_OLD_FILES */

/* =========================  globals  =========================== */

/* forward declarations */
static void start_new_metarun(void);
static bool metarun_ui_show_story_texts(const char* title, byte title_attr,
    cptr texts[], int total_texts, byte text_attr, bool steamdeck,
    const char* accept_label, const char* action_label);
static cptr metarun_curse_choice_label(int n);

static int rng_int(int max) { return max ? (int)(rand() % max) : 0; }

/* ---------------------------------------------------------------
 * Pick a curse at random, respecting weights, stacks, caps,
 * and the RHF_CURSE tail-lift and exclusion of most weighted curses.
 * ------------------------------------------------------------- */
static int weighted_random_curse(void)
{
    long total = 0;
    int  w_max = 1;

    /* Does the hero's lineage carry the flag? */
    bool tilt = (p_info[p_ptr->prace].flags  & RHF_CURSE) ||
                (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    /* Pass 1 â€“ find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* â† unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 â€” sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* â† unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        int  cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        if (cap && cnt >= cap) continue;           /* cap reached */

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)           /* lift the tail */
            : w;

        total += base / (cnt + 1);
    }

    if (!total) return rng_int(z_info->cu_max);    /* safety net */

    /* Pass 3 â€” roulette wheel */
    long pick = rng_int(total), run = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* â† unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        int  cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        if (cap && cnt >= cap) continue;

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)
            : w;

        long eff = base / (cnt + 1);
        run += eff;
        if (pick < run) return i;
    }

    return rng_int(z_info->cu_max);                /* unreachable */
}

void add_curse_stack(int idx)
{
    /* respect per-curse stack cap */
    if (CURSE_CURSE_CAP(idx) &&
        CURSE_CURSE_STACK(idx) >= CURSE_CURSE_CAP(idx))
    {
        log_debug("Curse %d (%s) already at max stacks", idx, cu_name + cu_info[idx].name);
        return;
    }

    CURSE_ADD(idx, 1);
    log_info("Added curse stack: %s (now %d stacks)", cu_name + cu_info[idx].name, CURSE_GET(idx));
    save_metaruns();
}

static cptr metarun_curse_choice_label(int n)
{
    switch (n) {
        case 1: return "the second";
        case 2: return "the third";
        case 3: return "the fourth";
        default: return "a";
    }
}

int menu_choose_one_curse(int n)
{
    /* if any active curse has the "noâ€choice" flag, skip the menu */
    if (any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    int pick[CURSE_MENU_LINES];
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    ui_information_scene_scope info_scope;

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool dup;
        do {
            dup     = false;
            pick[i] = weighted_random_curse();
            for (int j = 0; j < i; j++)
                if (pick[i] == pick[j]) { dup = true; break; }
            
            byte cap = (byte)CURSE_CURSE_CAP(pick[i]);
            if (cap && CURSE_CURSE_STACK(pick[i]) >= cap) { dup = true; continue; }

        } while (dup);
    }

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }
    if (!ui_information_scene_acquire(&info_scope)) {
        log_error("menu_choose_one_curse: semantic scene unavailable");
        return -1;
    }

    {
        int semantic_choice = metarun_ui_choose_curse_scene(n, pick, steamdeck,
            accept_label);

        ui_information_scene_leave(&info_scope);
        if (semantic_choice >= 0)
            return semantic_choice;
    }

    log_error("menu_choose_one_curse: semantic scene failed");
    return -1;
}


/* ------------------------------------------------------------------ *
 *  Debug helper â€“ wipe every active curse for the current meta-run.  *
 * ------------------------------------------------------------------ */
void metarun_clear_all_curses(void)
{
    log_info("Clearing all curses for current metarun");
    memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
    metar.curses_seen = 0;
    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_clear_all_curses: unable to sync current slot");
    }
    save_metaruns();
}

/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 *  NOTE: save_metaruns() comes **after** check_run_end() so that     *
 *  any realloc in start_new_metarun() has already finished.          *
 * ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 * ------------------------------------------------------------------ */
/*
 * Metarun narrative & exit logic - refactor **v4** (30â€¯Julâ€¯2025)
 * ------------------------------------------------------------------
 *  âœ§Â Reâ€‘orders the sequence so NOTHING is overwritten:
 *      0.Â Escapeâ€‘curse chooser (UI)  â†’ clears screen once finished.
 *      1.Â Chosenâ€‘curse line(s).
 *      2.Â Victoryâ€¯banner & Silmaril count paragraph.
 *      3.Â TemptationÂ ofÂ Treachery (escalating 1â€‘3 lines).
 *      4.Â StoryÂ Fragment (depends on Silmarils & Treachery flag).
 *      5.Â EchoesÂ ofÂ Kinslaying (escalating 1â€‘3 lines)
 *      6.Â Final pause, then deferred sideâ€‘effects.
 *
 *  âœ§Â `choose_escape_curses_ui()` now **returns** the indices chosen and
 *    does NOT leave the menu clutter on screen. We reâ€‘render the
 *    â€œThe curse of X binds your fate.â€ lines after a clean clear.
 *
 *  âœ§Â Adds `print_story_fragment()` â€“ a short narrative bridge keyed off
 *    Silmaril count (1â€‘3) and whether treachery was overcome.
 *
 *  âœ§Â Tested matrix: {treachery flagâ€¯Ã—â€¯kinslayer flagâ€¯Ã—â€¯silmarils (1â€‘3)}
 *    All show in the intended order with no garbled overlaps.
 */

cptr metarun_curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;
    /* Strip common prefixes for cleaner display */
    if (strncmp(raw, "Curse of ", 9) == 0) raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0) raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0) raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0) raw += 8;
    return raw;
}

cptr metarun_blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name) {
        cptr raw = cu_name + cu_info[idx].blessing_name;
        /* Strip "Blessing of " prefix for consistency */
        if (strncmp(raw, "Blessing of ", 12) == 0) raw += 12;
        return raw;
    }
    return metarun_curse_display_name(idx);
}

/****************  Escapeâ€‘curse chooser (clean version) ************/

/*
 * Presents the menu *n* times (or once if CUR_NOCHOICE). Returns the
 * number of curses actually chosen and fills `out` with their indices.
 * The display is cleared afterwards so we can start narrative fresh.
 */
int choose_escape_curses_ui(int n, int out[4])
{
    // int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;
    int taken = 0;
    char intro_text[512];
    const char* paragraphs[] = { intro_text };

    strnfmt(intro_text, sizeof(intro_text),
            "The Valar watch silently as Morgoth's malice reaches out from shadow-"
            "Your triumph has drawn his wrath. His dark will twists fate, "
            "forcing upon you the final choice-%s curse%s you must bear.",
            (n == 1) ? "a" : (n == 2) ? "two" : (n == 3) ? "three" : "four",
            (n == 1) ? "" : "s");

    metarun_present_story_texts("The Valar's Judgment", (cptr*)paragraphs, 1,
        TERM_L_BLUE, TERM_WHITE, "Continue");

    for (int i = 0; i < n; i++)
    {
        int idx = menu_choose_one_curse(i);   /* weighted picker, UI */
        if (idx < 0)
        {
            log_error("choose_escape_curses_ui: curse selection failed after %d of %d picks",
                taken, n);
            return taken ? taken : -1;
        }
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        add_curse_stack(idx);                /* gameplay sideâ€‘effect */
        if (taken < 4) out[taken++] = idx;
    }
    
    return taken;
}

/****************  Oath-breaking curse chooser with fade ************/

/*
 * Shows the oath-specific curse message with fade-in, waits 3 seconds,
 * then shows the permanent consequence message and curse selection menu.
 * Returns the selected curse index.
 */
int choose_oath_breaking_curse_ui(int oath_id)
{
    /* Get oath-specific permanent message (E: field from oath.txt) */
    char* perm_msg = oath_permanent_message(oath_id);
    char intro_text[256];
    strnfmt(intro_text, sizeof(intro_text),
            "The breach of your sacred vow has drawn Morgoth's attention. "
            "His malice reaches out to compound your suffering with a curse you must bear.");

    {
        cptr first_paragraphs[] = {
            (perm_msg && perm_msg[0]) ? perm_msg
                : "Your oath is forever broken in this age."
        };
        cptr second_paragraphs[] = { intro_text };

        metarun_present_story_texts("The Sundering of Sacred Vows",
            first_paragraphs, 1, TERM_L_RED, TERM_L_RED, "Continue");
        metarun_present_story_texts("The Sundering of Sacred Vows",
            second_paragraphs, 1, TERM_L_RED, TERM_RED, "Choose");
    }

    /* Let the player choose 1 curse from 3 options */
    int idx = menu_choose_one_curse(0);
    if (idx < 0)
        log_error("choose_oath_breaking_curse_ui: curse selection failed");
    log_debug("Player selected curse %d for oath breaking", idx);
    
    return idx;
}

/* ------------------------------------------------------------------ */
/*  Standard â€œPress any keyâ€¦â€ prompts â€“ use enum, not raw strings     */
/* ------------------------------------------------------------------ */
typedef enum {
    PROMPT_CONTINUE_TALE,
    PROMPT_FACE_TEMPTATION,
    PROMPT_CONTINUE_GENERIC,
    PROMPT_FACE_ECHOES,
    PROMPT_CONCLUDE_TALE,
    PROMPT_WITNESS_CONSEQUENCES,
    PROMPT_RETURN_MIDDLE_EARTH
} prompt_t;

static const char* metarun_prompt_action_label(prompt_t id)
{
    switch (id) {
        case PROMPT_FACE_TEMPTATION: return "Temptation";
        case PROMPT_FACE_ECHOES: return "Echoes";
        case PROMPT_CONCLUDE_TALE: return "Conclude";
        case PROMPT_WITNESS_CONSEQUENCES: return "Witness";
        case PROMPT_RETURN_MIDDLE_EARTH: return "Return";
        case PROMPT_CONTINUE_TALE:
        case PROMPT_CONTINUE_GENERIC:
        default:
            return "Continue";
    }
}

static bool metarun_ui_show_story_texts(const char* title, byte title_attr,
    cptr texts[], int total_texts, byte text_attr, bool steamdeck,
    const char* accept_label, const char* action_label)
{
    byte* attrs = NULL;
    bool presented = false;

    if (total_texts > 0)
    {
        attrs = mem_alloc_array(total_texts, byte);
        if (!attrs)
            return false;
        for (int i = 0; i < total_texts; i++)
            attrs[i] = text_attr;
    }

    presented = metarun_ui_show_story_modal(title, title_attr,
        (const char* const*)texts, attrs, total_texts, steamdeck,
        accept_label, action_label);
    mem_free_null(attrs);
    return presented;
}

void metarun_present_story_texts(const char* title,
    cptr texts[], int total_texts, byte title_attr,
    byte text_attr, const char* action_label)
{
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    ui_information_scene_scope info_scope;
    bool presented = false;

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }
    if (!ui_information_scene_acquire(&info_scope)) {
        log_error("metarun: semantic story scene unavailable for '%s'",
            title ? title : "(untitled)");
        return;
    }

    presented = metarun_ui_show_story_texts(title, title_attr, texts,
        total_texts, text_attr, steamdeck, accept_label,
        action_label ? action_label : "Continue");
    if (!presented) {
        log_error("metarun: failed to present story texts '%s'",
            title ? title : "(untitled)");
    }

    ui_information_scene_leave(&info_scope);
}

static void show_mandos_third_unlock_message(void)
{
    const char *lines[] = {
        "You endured the challenge of disconnected stairs.",
        "Mandos acknowledges your feat and sets a final doom:",
        "Seek Maeglin's hidden vault between delvings seventeen and nineteen and end his false life.",
        "This third quest stands apart and will not bar other callings."
    };
    metarun_present_story_texts("Mandos' Final Doom", lines,
        N_ELEMENTS(lines), TERM_L_BLUE, TERM_WHITE, "Continue");
}

static void activate_orome_great_hunt(void)
{
    byte mask = metarun_orome_great_hunt_mask();
    p_ptr->orome_great_hunt_mask = mask;
    metarun_set_orome_great_hunt_mask(mask);
    metarun_set_orome_great_hunt_active(true);
    if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) < QUEST_STATE_ACTIVE) {
        quest_set_state(QUEST_ID_OROME_GREAT_HUNT, QUEST_STATE_ACTIVE);
    }
    log_trace("Metarun: Orome great hunt activated (mask=0x%02x, state=%d)", mask, quest_get_state(QUEST_ID_OROME_GREAT_HUNT));
}

static void show_orome_great_hunt_unlock_message(void)
{
    const char *lines[] = {
        "You endured the single-stair challenge.",
        "Orome offers the Wraith for purchase and names a hunt that spans your line:",
        "Scatha, Smaug, Draugluin, Gostir, Shelob, Thuringwethil. Slay them in any delve.",
        "This hunt runs alongside all other quests until the last mark falls."
    };
    metarun_present_story_texts("Orome's Great Hunt", lines,
        N_ELEMENTS(lines), TERM_GREEN, TERM_WHITE, "Continue");
}

static void maybe_unlock_orome_great_hunt(bool challenge_single_stair_active)
{
    if (!challenge_single_stair_active) return;
    if (quest_get_state(QUEST_ID_OROME_GREAT_HUNT) >= QUEST_STATE_REWARDED) return;
    if (metarun_quest_completion_count(METARUN_QUEST_OROME_GREAT_HUNT) >= quest_completion_cap(QUEST_ID_OROME_GREAT_HUNT)) return;
    if (metarun_orome_great_hunt_active()) return;

    activate_orome_great_hunt();
    show_orome_great_hunt_unlock_message();
}

static void show_niena_mercy_unlock_message(void)
{
    const char *lines[] = {
        "You completed the fixed 50,000 XP challenge.",
        "Nienna smiles through tears: her Gift of Mercy may now be learned for 5000 experience.",
        "Find it in the Special abilities list if you would carry her pity into future delvings."
    };
    metarun_present_story_texts("Nienna's Gift of Mercy", lines,
        N_ELEMENTS(lines), TERM_L_BLUE, TERM_WHITE, "Continue");
}

static void show_tulkas_blunt_unlock_message(void)
{
    const char *lines[] = {
        "You endured the blunt-arms challenge.",
        "Tulkas roars with laughter and offers the lore of Unique Bane to all your line.",
        "Seek it among your Special abilities for 5000 experience in future delvings."
    };
    metarun_present_story_texts("Tulkas' Orc-Bane", lines,
        N_ELEMENTS(lines), TERM_YELLOW, TERM_WHITE, "Continue");
}

static void show_torchlight_unlock_message(void)
{
    const char *lines[] = {
        "You conquered Varda's torches-only challenge.",
        "The Queen of the Stars will now teach her radiance to any who pay 5000 experience.",
        "Find Queen of the Stars among your Special abilities in future delves."
    };
    metarun_present_story_texts("Queen of the Stars", lines,
        N_ELEMENTS(lines), TERM_WHITE, TERM_L_BLUE, "Continue");
}

static int total_player_kills_this_run(void)
{
    if (!z_info || !l_list) return 0;

    int total = 0;
    for (int i = 0; i < z_info->r_max; i++) {
        int kills = l_list[i].pkills;
        if (kills > 0) total += kills;
    }

    return total;
}

static void maybe_unlock_niena_mercy_purchase(bool challenge_fixed_active, int fixed_count_before)
{
    if (!challenge_fixed_active) return;
    if (fixed_count_before > 0) return; /* Already unlocked */
    int fixed_after = metarun_challenge_completion_count(CHALLENGE_FIXED_50K_XP);
    if (fixed_after <= fixed_count_before) return;
    show_niena_mercy_unlock_message();
}

static void maybe_unlock_queen_of_stars_purchase(bool challenge_torch_active, int torch_count_before)
{
    if (!challenge_torch_active) return;
    if (torch_count_before > 0) return; /* Already unlocked */
    int torch_after = metarun_challenge_completion_count(CHALLENGE_TORCHLIGHT);
    if (torch_after <= torch_count_before) return;
    show_torchlight_unlock_message();
}

static void resolve_niena_morgoth_quest_on_exit(bool escaped_with_sils)
{
    byte state = quest_get_state(QUEST_ID_NIENA_MORGOTH);
    if (state != QUEST_STATE_ACTIVE) return;

    bool violated = (p_ptr->niena_reserved & NIENA_FLAG_MORGOTH_ATTACKED) != 0;
    if (!escaped_with_sils || violated) {
        if (violated) {
            msg_print("You struck Morgoth; Nienna's mercy quest is lost.");
        }
        quest_set_state(QUEST_ID_NIENA_MORGOTH, QUEST_STATE_NOT_STARTED);
        niena_revoke_temp_mercy_gift(true);
        p_ptr->niena_reserved &= ~(NIENA_FLAG_MORGOTH_ATTACKED);
        return;
    }

    int completion_count = 0;
    cptr *completion_texts = extract_quest_completion_texts(QUEST_ID_NIENA_MORGOTH, &completion_count);
    if (completion_texts && completion_count > 0) {
        metarun_present_story_texts("Nienna's Mercy", completion_texts,
            completion_count, TERM_L_BLUE, TERM_WHITE, "Continue");
        free_quest_texts(completion_texts, completion_count);
    } else {
        const char *fallback[] = {
            "Nienna's presence returns as you escape with the Silmaril, untouched by your hand.",
            "'Your restraint has spared even the Black Foe a blow. Carry this mercy into the tales to come.'"
        };
        metarun_present_story_texts("Nienna's Mercy", fallback,
            N_ELEMENTS(fallback), TERM_L_BLUE, TERM_WHITE, "Continue");
    }

    quest_set_state(QUEST_ID_NIENA_MORGOTH, QUEST_STATE_REWARDED);
    p_ptr->niena_reserved &= ~(NIENA_FLAG_MORGOTH_ATTACKED);
    niena_revoke_temp_mercy_gift(true);
    metarun_mark_quest_completed(METARUN_QUEST_NIENA_MORGOTH);
    metarun_unlock_challenge_fixed_exp();
    p_ptr->quest_reserved[0] = 1;
    msg_print("The fixed 50k XP challenge is now unlocked.");
}

static void resolve_nienia_pacifist_quest_on_exit(bool died, bool escaped)
{
    int completion_cap = quest_completion_cap(QUEST_ID_NIENA_PACIFIST);
    if (completion_cap < 1) completion_cap = METARUN_QUEST_COMPLETION_CAP;
    int completion_count = metarun_quest_completion_count(METARUN_QUEST_NIENA_PACIFIST);

    if (completion_count >= completion_cap) return;

    byte state = quest_get_state(QUEST_ID_NIENA_PACIFIST);
    if (state >= QUEST_STATE_REWARDED) return;

    /* Only escapes can finish the quest */
    if (!escaped || died) {
        /* If kills accrued, make sure the failure flag is set for logging/UI */
        if (total_player_kills_this_run() > 0) {
            p_ptr->niena_reserved |= NIENA_FLAG_PACIFIST_FAILED;
        }
        return;
    }

    int kills = total_player_kills_this_run();
    bool failed = (kills > 0) || ((p_ptr->niena_reserved & NIENA_FLAG_PACIFIST_FAILED) != 0);
    if (failed) {
        log_trace("Niena pacifist quest not completed - kills=%d, flag=%d", kills, (p_ptr->niena_reserved & NIENA_FLAG_PACIFIST_FAILED));
        return;
    }

    int completion_count_text = 0;
    cptr *completion_texts = extract_quest_completion_texts(QUEST_ID_NIENA_PACIFIST, &completion_count_text);
    if (completion_texts && completion_count_text > 0) {
        metarun_present_story_texts("Nienna, Lady of Pity",
            completion_texts, completion_count_text, TERM_L_BLUE,
            TERM_WHITE, "Continue");
        free_quest_texts(completion_texts, completion_count_text);
    } else {
        const char *fallback[] = {
            "You return to the surface with no blood on your hands.",
            "Nienna's relief washes over you: the curses that followed you may be lifted once and for all."
        };
        metarun_present_story_texts("Nienna, Lady of Pity", fallback,
            N_ELEMENTS(fallback), TERM_L_BLUE, TERM_WHITE, "Continue");
    }

    quest_set_state(QUEST_ID_NIENA_PACIFIST, QUEST_STATE_REWARDED);
    p_ptr->niena_reserved &= ~NIENA_FLAG_PACIFIST_FAILED;
    metarun_mark_quest_completed(METARUN_QUEST_NIENA_PACIFIST);
    metarun_add_niena_curse_cleansing_charge();
    msg_print("Nienna grants you a single cleansing; use the quest menu to cast off every curse.");
}

/* ------------------------------------------------------------------
 * metarun_update_on_exit() â€“ v5, 30â€¯Julâ€¯2025
 * ------------------------------------------------------------------
 * Implements the finalised story/logic flow discussed in chat:
 *   0.  Escape check (silmarils? giftâ€‘ofâ€‘Eru?)
 *   1.  Escapeâ€‘curse chooser UI
 *   2.  Victory banner & Silmaril paragraph
 *   3.  Temptation of Treachery (3 rolls â€“ stolen Silmarils don't count)
 *   4.  Story Fragment (pure vs tainted, 1â€‘3 jewels)
 *   5.  Echoes of Kinslaying / "Kill a Kin" (stop at first kill)
 *   6.  Final pause â†’ apply deferred effects
 *   7.  Persist silmaril/death counters, check run end, save
 *
 *  All narrative helpers (print_heading(), print_paragraph(),
 *  choose_escape_curses_ui(), kinslayer_try_kill(), etc.) are reused.
 * ------------------------------------------------------------------ */
static void announce_blessing_gain(int previous_points)
{
    int current_points = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    if (current_points <= previous_points) return;
    int delta = current_points - previous_points;
    int available = current_points - metar.blessing_points_spent;
    if (available < 0) available = 0;
    msg_format("You gain %d blessing point%s. (%d available)",
               delta, (delta == 1) ? "" : "s", available);
    message_flush();
}

static bool metarun_ui_show_story_modal_auto(const char* title,
    byte title_attr, const char* const* paragraphs, const byte* attrs,
    int paragraph_count, bool steamdeck, const char* accept_label,
    const char* action_label)
{
    ui_information_scene_scope info_scope;

    if (!ui_information_scene_acquire(&info_scope))
    {
        log_error("metarun: semantic story modal unavailable for '%s'",
            title ? title : "(untitled)");
        return false;
    }

    if (!metarun_ui_show_story_modal(title, title_attr, paragraphs, attrs,
            paragraph_count, steamdeck, accept_label, action_label))
    {
        log_error("metarun: failed to present story modal '%s'",
            title ? title : "(untitled)");
        ui_information_scene_leave(&info_scope);
        return false;
    }

    ui_information_scene_leave(&info_scope);
    return true;
}

static int metarun_collect_story_indices(int* indices, int capacity)
{
    int total = 0;
    int max_st;
    byte rt = metar.type;
    int sils = metar.silmarils;

    if (!indices || capacity <= 0 || !z_info || !st_info)
        return 0;

    max_st = z_info->st_max;
    if (max_st > capacity)
        max_st = capacity;

    for (int i = 0; i < max_st; i++)
    {
        story_type* st = &st_info[i];

        if (!st->name && !st->text)
            continue;
        if (st->st_type != 0)
            continue;
        if (!(st->runtypes == 0
            || (rt < 32 && (st->runtypes & (1UL << rt)))))
        {
            continue;
        }
        if (st->order <= (byte)sils)
            indices[total++] = i;
    }

    for (int i = 1; i < total; i++)
    {
        int key = indices[i];
        byte key_ord = st_info[key].order;
        int j = i - 1;

        while (j >= 0 && st_info[indices[j]].order > key_ord)
        {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }

    return total;
}

static void metarun_show_recent_story_parts_semantic(int last_parts)
{
    int selected[1024];
    int total;
    int start;
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";

    total = metarun_collect_story_indices(selected, N_ELEMENTS(selected));
    if (total <= 0)
    {
        log_debug("metarun: no story parts available for semantic recap");
        return;
    }

    if (steamdeck)
    {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    start = (last_parts > 0 && last_parts < total) ? total - last_parts : 0;
    for (int idx = start; idx < total; idx++)
    {
        story_type* st = &st_info[selected[idx]];
        const char* story_title = st->name ? st_name + st->name : NULL;
        const char* story_text = st->text ? st_text + st->text : NULL;
        const char* paragraphs[2];
        byte attrs[2];
        int paragraph_count = 0;
        int page = idx - start + 1;
        int page_total = total - start;
        char title[APP_UI_TEXT_MAX];

        if (!story_text || !story_text[0])
            continue;

        if (page_total > 1)
        {
            strnfmt(title, sizeof(title), "The Tale So Far (%d/%d)", page,
                page_total);
        }
        else
        {
            SDL_strlcpy(title, "The Tale So Far", sizeof(title));
        }

        if (story_title && story_title[0])
        {
            paragraphs[paragraph_count] = story_title;
            attrs[paragraph_count++] = TERM_L_BLUE;
        }
        paragraphs[paragraph_count] = story_text;
        attrs[paragraph_count++] = TERM_WHITE;

        if (!metarun_ui_show_story_modal_auto(title, TERM_YELLOW,
                paragraphs, attrs, paragraph_count, steamdeck, accept_label,
                metarun_prompt_action_label(PROMPT_CONTINUE_TALE)))
        {
            log_warn("metarun: failed to present semantic story recap part %d/%d",
                page, page_total);
            break;
        }
    }
}

static byte metarun_play_escape_victory_semantic_sequence(int sil_count,
    bool allow_treachery, bool allow_kinslay)
{
    bool steamdeck = steamdeck_controls_active();
    char accept_label[16] = "";
    int curse_count = sil_count;
    int chosen[4] = { -1, -1, -1, -1 };
    int chosen_cnt;
    byte stolen = 0;
    byte final_sils;
    bool treachery_occurred;
    bool deferred_kill[3] = { false, false, false };
    int kinslaying_victims = 0;

    if (steamdeck) {
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    if (sil_count == 3)
        curse_count = 4;
    chosen_cnt = choose_escape_curses_ui(curse_count, chosen);
    if (chosen_cnt < 0)
    {
        log_error("metarun: escape curse selection failed");
        chosen_cnt = 0;
    }

    if (chosen_cnt > 0)
    {
        char curse_lines[4][128];
        const char* paragraphs[4];
        byte attrs[4];

        for (int i = 0; i < chosen_cnt; i++)
        {
            strnfmt(curse_lines[i], sizeof(curse_lines[i]),
                "The curse of %s binds your fate.",
                metarun_curse_display_name(chosen[i]));
            paragraphs[i] = curse_lines[i];
            attrs[i] = TERM_RED;
        }

        metarun_ui_show_story_modal_auto("The Binding of Fate",
            TERM_L_RED, paragraphs, attrs, chosen_cnt, steamdeck,
            accept_label, metarun_prompt_action_label(PROMPT_CONTINUE_TALE));
    }

    {
        const char* victory_text;
        const char* paragraphs[1];
        byte attrs[1] = { TERM_WHITE };

        switch (sil_count)
        {
            case 1:
                victory_text = "You emerge victorious from darkness, one holy jewel blazing in your grasp. Morgoth's crown is diminished, yet hope is rekindled, though shadow lingers.";
                break;
            case 2:
                victory_text = "You escape triumphant, two Silmarils blazing fiercely in your hands. Morgoth roars in wrath; his pride is wounded deeply. Your spirit exults, yet your heart begins to feel their burning weight.";
                break;
            case 3:
                victory_text = "All three stolen stars blaze now in your hands; Morgoth's crown lies darkened. Such triumph has not been known since Feanor himself dreamed it-but even as victory soars, your heart trembles beneath their burning glory.";
                break;
            default:
                victory_text = "You have achieved the impossible, claiming more Silmarils than should exist. Reality itself bends before your triumph.";
                break;
        }

        paragraphs[0] = victory_text;
        metarun_ui_show_story_modal_auto("Victory Amid Shadow",
            TERM_YELLOW, paragraphs, attrs, 1, steamdeck, accept_label,
            metarun_prompt_action_label(allow_treachery
                ? PROMPT_FACE_TEMPTATION
                : PROMPT_CONTINUE_GENERIC));
    }

    if (allow_treachery)
    {
        static const int pct[3] = { 20, 50, 95 };
        static const char *success_msgs[3] = {
            "The first jewel shines brightly, its pure light uncorrupted. You master desire, choosing honor.",
            "The second jewel blazes defiant, temptation growing strong-but once more, you cling to honor.",
            "The third Silmaril's holy flame burns fiercely. Yet against all odds, your will resists corruption."
        };
        static const char *failure_msgs[3] = {
            "Greed whispers softly, and you listen. Secretly you withhold the jewel's light, betraying even yourself.",
            "Desire gnaws deeper; you falter, concealing its brilliance in shame, light darkened by your betrayal.",
            "Consumed by lust for its beauty, you claim it secretly, sealing its radiance from all others-a betrayal of all trust."
        };
        const char* shadow_text =
            "In shadows your deeds are recorded-tainted victory shall diminish the jewel's blessing.";
        const char* paragraphs[4];
        byte attrs[4];
        int paragraph_count = 0;

        for (int i = 0; i < sil_count; ++i)
        {
            bool fail = (rand_int(100) < pct[i]);

            if (fail)
                stolen++;
            paragraphs[paragraph_count] = fail ? failure_msgs[i]
                                               : success_msgs[i];
            attrs[paragraph_count++] = fail ? TERM_RED : TERM_WHITE;
        }

        if (stolen)
        {
            paragraphs[paragraph_count] = shadow_text;
            attrs[paragraph_count++] = TERM_L_DARK;
        }

        metarun_ui_show_story_modal_auto("Temptation of Treachery",
            TERM_L_UMBER, paragraphs, attrs, paragraph_count, steamdeck,
            accept_label, metarun_prompt_action_label(PROMPT_CONTINUE_GENERIC));
    }

    final_sils = sil_count - stolen;
    treachery_occurred = (stolen > 0);

    {
        const char* pure_frag[3] = {
            "A single star reclaimed, hope rekindled faintly in Middle-earth. Yet Morgoth laughs still, for two remain bound in shadow.",
            "Two jewels shine again beneath sky; Morgoth's power falters greatly. Yet you feel their brilliance burning; temptation ever near.",
            "All three jewels, radiant and pure, blaze again beneath stars. Morgoth's power breaks. Triumph is absolute, your soul soaring."
        };
        const char* tainted_frag[3] = {
            "Though victory is yours, its memory darkens. Trust is fragile, and your spirit heavy beneath secret betrayal.",
            "Your heart trembles: Morgoth sees clearly your treachery-he smiles grimly, knowing darkness still dwells in you.",
            "Greatest triumph now mingled with darkest shame. Morgoth's laughter echoes bitterly-he senses your fall."
        };
        const char* paragraphs[1];
        byte attrs[1];

        paragraphs[0] = treachery_occurred
            ? tainted_frag[sil_count - 1]
            : pure_frag[final_sils - 1];
        attrs[0] = treachery_occurred ? TERM_RED : TERM_L_WHITE;
        metarun_ui_show_story_modal_auto("The Weight of Victory",
            TERM_L_BLUE, paragraphs, attrs, 1, steamdeck, accept_label,
            metarun_prompt_action_label(allow_kinslay
                ? PROMPT_FACE_ECHOES
                : PROMPT_CONCLUDE_TALE));
    }

    if (allow_kinslay)
    {
        static const int kin_pct[3] = { 20, 50, 95 };
        const char* doom_text =
            "Blood now stains your triumph, your fate forever woven with grief and shame.";
        const char* paragraphs[4];
        byte attrs[4];
        int paragraph_count = 0;

        for (int k = 0; k < sil_count; ++k)
        {
            bool fail = (rand_int(100) < kin_pct[k]);
            const char* echo_text = NULL;

            deferred_kill[k] = fail;
            if (fail)
                kinslaying_victims++;

            switch (k)
            {
                case 0:
                    echo_text = fail ?
                        "\"Alqualonde's Grief\"\nBlood stains starlit waves. Your hand remembers the swords at Alqualonde-first grief, first guilt." :
                        "The sorrow of Alqualonde passes over you-your spirit holds fast, blood unstained.";
                    break;
                case 1:
                    echo_text = fail ?
                        "\"Ruin of Doriath\"\nAgain your hand recalls tragedy-fallen halls of Menegroth, Dior's blood shed beneath stolen starlight." :
                        "Memory of Doriath rises briefly, but your blade remains clean, honour upheld.";
                    break;
                case 2:
                    echo_text = fail ?
                        "\"Tragedy at Sirion\"\nEchoes rise from Sirion-Elwing's flight, blood and betrayal. Once more your blade draws innocent blood, sealing doom anew." :
                        "You resist dark whispers recalling Sirion-your sword is stayed, mercy unbroken.";
                    break;
            }

            paragraphs[paragraph_count] = echo_text;
            attrs[paragraph_count++] = fail ? TERM_RED : TERM_L_WHITE;

            if (fail)
                break;
        }

        if (kinslaying_victims > 0)
        {
            paragraphs[paragraph_count] = doom_text;
            attrs[paragraph_count++] = TERM_L_DARK;
        }

        metarun_ui_show_story_modal_auto("Echoes of Kinslaying",
            TERM_L_RED, paragraphs, attrs, paragraph_count, steamdeck,
            accept_label, metarun_prompt_action_label(PROMPT_CONCLUDE_TALE));
    }

    {
        const char* paragraphs[1];
        byte attrs[1] = { TERM_L_GREEN };
        char summary[256];

        strnfmt(summary, sizeof(summary),
            "Your legend is written: %d Silmaril%s claimed, %s, %s.",
            final_sils, (final_sils == 1) ? "" : "s",
            treachery_occurred ? "tainted by treachery" : "pure of heart",
            (kinslaying_victims > 0) ? "stained by kinslaying"
                                     : "with honour intact");
        paragraphs[0] = summary;
        metarun_ui_show_story_modal_auto("The Tale Concludes",
            TERM_YELLOW, paragraphs, attrs, 1, steamdeck, accept_label,
            metarun_prompt_action_label((allow_kinslay && kinslaying_victims > 0)
                ? PROMPT_CONTINUE_GENERIC
                : PROMPT_RETURN_MIDDLE_EARTH));
    }

    if (allow_kinslay && kinslaying_victims > 0)
    {
        const char* paragraphs[1];
        byte attrs[1] = { TERM_RED };
        char kill_msg[128];

        strnfmt(kill_msg, sizeof(kill_msg),
            "Your kinslaying echoes through time. %d innocent%s will fall by your hand...",
            kinslaying_victims, (kinslaying_victims == 1) ? "" : "s");
        paragraphs[0] = kill_msg;
        metarun_ui_show_story_modal_auto("The Price of Blood",
            TERM_RED, paragraphs, attrs, 1, steamdeck, accept_label,
            metarun_prompt_action_label(PROMPT_WITNESS_CONSEQUENCES));

        {
            char kill_lines[3][96];
            const char* kill_paragraphs[3];
            byte kill_attrs[3];
            int kill_count = 0;

            for (int k = 0; k < 3; k++)
            {
                const char* character;

                if (!deferred_kill[k])
                    continue;

                character = kinslayer_try_kill(k + 1, false);
                if (!character)
                    continue;

                metarun_increment_deaths();
                log_info("Metarun: kinslaying victim counted as death (%u total)",
                    (unsigned)metar.deaths);

                strnfmt(kill_lines[kill_count], sizeof(kill_lines[kill_count]),
                    "A hero %s has fallen beneath your blade.", character);
                kill_paragraphs[kill_count] = kill_lines[kill_count];
                kill_attrs[kill_count++] = TERM_RED;
            }

            if (kill_count > 0)
            {
                metarun_ui_show_story_modal_auto("Blood Is Demanded",
                    TERM_RED, kill_paragraphs, kill_attrs, kill_count,
                    steamdeck, accept_label,
                    metarun_prompt_action_label(PROMPT_RETURN_MIDDLE_EARTH));
            }
        }
    }

    return final_sils;
}

void metarun_update_on_exit(bool died, bool escaped, byte sil_count, s32b final_score)
{
    if (run_mode_is_blitz())
    {
        log_info("Suppressing metarun end-of-run processing for Blitz");
        if (escaped || (p_ptr && p_ptr->morgoth_slain && !died))
        {
            byte summary_sils = sil_count;
            if (p_ptr && p_ptr->morgoth_slain && summary_sils < 3)
                summary_sils = 3;
            blitz_show_end_summary(summary_sils);
        }
        return;
    }

    log_info("Metarun update: died=%s, escaped=%s, sil_count=%d, final_score=%ld", 
             died ? "true" : "false", escaped ? "true" : "false", sil_count, (long)final_score);
    int blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    if (escaped)
    {
        platform_music_play_main();
    }
    bool challenge_disconnected = (op_ptr && op_ptr->opt[OPT_adult_discon_stair]);
    bool challenge_single_stair = (op_ptr && op_ptr->opt[OPT_adult_single_stair]);
    bool challenge_fixed_exp = (op_ptr && op_ptr->opt[OPT_birth_fixed_exp] && metarun_challenge_fixed_exp_unlocked());
    bool challenge_tulkas_blunt = (op_ptr && op_ptr->opt[OPT_adult_tulkas_blunt]);
    bool challenge_torchlight = (op_ptr && op_ptr->opt[OPT_adult_torchlight] && metarun_challenge_torchlight_unlocked());
    int fixed_challenge_before = metarun_challenge_completion_count(CHALLENGE_FIXED_50K_XP);
    int blunt_challenge_before = metarun_challenge_completion_count(CHALLENGE_TULKAS_BLUNT);
    int torchlight_challenge_before = metarun_challenge_completion_count(CHALLENGE_TORCHLIGHT);
    bool challenge_disconnected_success = false;
             
    /* -------- Lineage flags -------------------------------------- */
    u32b character_flags = c_info[p_ptr->pcharacter].flags;
    u32b f_race  = p_info[p_ptr->prace].flags;

    bool has_gift_eru   = (character_flags | f_race) & RHF_GIFTERU;
    bool allow_treachery = (character_flags | f_race) & RHF_TREACHERY;
    bool allow_kinslay   = (character_flags | f_race) & RHF_KINSLAYER;

    bool escaped_with_sils = escaped && (sil_count > 0);
    bool morgoth_victory = (p_ptr->morgoth_slain && !escaped && !died);

    resolve_niena_morgoth_quest_on_exit(escaped_with_sils);
    resolve_nienia_pacifist_quest_on_exit(died, escaped);

    /* Treat as a death unless Eru intervenes */
    if (died && !has_gift_eru)
        metarun_increment_deaths();

    /* ------------------------------------------------------------- */
    /* 0. Branch: did we return with Silmarils?                      */
    /*    â€“ any path that reaches here counts as a "run end" event  */
    /* ------------------------------------------------------------- */
    if (morgoth_victory)
    {
        bool steamdeck = steamdeck_controls_active();
        char accept_label[16] = "";
        const char* paragraphs[] = {
            "The illusion of Morgoth lies shattered at your feet.",
            "From Valinor, the Valar proclaim your impossible triumph and pour out their blessing.",
            "Though the true Dark Enemy waits beyond this trial, three Silmarils are counted to your name."
        };
        const byte attrs[] = { TERM_WHITE, TERM_L_BLUE, TERM_L_BLUE };

        log_info("Metarun: Morgoth victory branch (sil_count=%d)", sil_count);
        if (steamdeck) {
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
                sizeof(accept_label));
        }
        (void)metarun_ui_show_story_modal_auto("Beyond Fate",
            TERM_YELLOW, paragraphs, attrs, (int)N_ELEMENTS(paragraphs),
            steamdeck, accept_label, "Continue");

        if (challenge_disconnected) {
            metarun_mark_challenge_completed(CHALLENGE_DISCONNECTED);
            challenge_disconnected_success = true;
        }
        if (challenge_single_stair) {
            metarun_mark_challenge_completed(CHALLENGE_SINGLE_STAIR);
        }
        if (challenge_fixed_exp) {
            metarun_mark_challenge_completed(CHALLENGE_FIXED_50K_XP);
        }
        if (challenge_tulkas_blunt) {
            metarun_mark_challenge_completed(CHALLENGE_TULKAS_BLUNT);
        }
        if (challenge_torchlight) {
            metarun_mark_challenge_completed(CHALLENGE_TORCHLIGHT);
        }

        byte awarded = (sil_count < 3) ? 3 : sil_count;
        metarun_gain_silmarils(awarded);
        log_info("Metarun: Morgoth victory awarded %d Silmarils (total now %d)",
                 awarded, (int)metar.silmarils);
        refresh_current_metar_score();
        compute_blessing_pool();
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        if (challenge_disconnected_success &&
            quest_get_state(QUEST_ID_MANDOS_BETRAYER) < QUEST_STATE_REWARDED &&
            metarun_quest_completion_count(METARUN_QUEST_MANDOS_BETRAYER) < quest_completion_cap(QUEST_ID_MANDOS_BETRAYER)) {
            show_mandos_third_unlock_message();
        }
        maybe_unlock_orome_great_hunt(challenge_single_stair);
        maybe_unlock_niena_mercy_purchase(challenge_fixed_exp, fixed_challenge_before);
        if (challenge_tulkas_blunt &&
            metarun_challenge_completion_count(CHALLENGE_TULKAS_BLUNT) > blunt_challenge_before) {
            show_tulkas_blunt_unlock_message();
        }
        if (challenge_torchlight) {
            maybe_unlock_queen_of_stars_purchase(challenge_torchlight, torchlight_challenge_before);
        }
        check_run_end();
        metarun_save_persistent_settings();
        save_metaruns();
        return;
    }
    else if (died)
    {
        bool steamdeck = steamdeck_controls_active();
        char accept_label[16] = "";

        log_info("Player died - displaying death narrative");
        /*****  NEW DEATH-NARRATIVE *****/
        if (steamdeck) {
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
                sizeof(accept_label));
        }

        /* Pick correct sequence number: 0 when Gift-of-Eru fires,
         * otherwise 1-based death counter that was just incremented. */
        byte target_order = has_gift_eru ? 0 : metar.deaths;

        /* Build a pool of candidate story entries.                    */
        int *pool = mem_alloc_array(z_info->st_max, int);
        int pool_sz = 0;
        if (!pool) {
            u32b pool_before = metar.fallen_score_total;
            refresh_current_metar_score();
            compute_blessing_pool();
            if (final_score > 0 && metar.fallen_score_total == pool_before) {
                metar.fallen_score_total += (u32b)final_score;
                update_blessing_ledger(&metar);
                (void)sync_current_metarun_slot(false);
            }
            announce_blessing_gain(blessing_points_before);
            blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
            check_run_end();
            save_metaruns();
            return;
        }
        for (int i = 0; i < z_info->st_max && pool; i++) {
            story_type *st = &st_info[i];
            if (!st->name)            continue;                /* unused slot   */
            if (st->st_type != 1)     continue;                /* not â€œdeathâ€   */
            if (st->order != target_order) continue;           /* wrong order   */
            if (st->runtypes &&
               !(st->runtypes & (1u << metar.type))) continue; /* wrong run-type*/
            pool[pool_sz++] = i;
        }

        /* Fallback â€“ allow any order-0 message if nothing matched.   */
        if (!pool_sz && target_order) {
            for (int i = 0; i < z_info->st_max && pool; i++) {
                story_type *st = &st_info[i];
                if (!st->name || st->st_type != 1) continue;
                if (st->order != 0)   continue;
                if (st->runtypes &&
                   !(st->runtypes & (1u << metar.type))) continue;
                pool[pool_sz++] = i;
            }
        }

        /* Display the chosen fragment with the usual fade-in style.  */
        if (pool_sz) {
            story_type *pick = &st_info[ pool[rng_int(pool_sz)] ];
            cptr title = st_name + pick->name;
            cptr text  = st_text + pick->text;
            char transition_text[256];

            strnfmt(transition_text, sizeof(transition_text),
                    "The hero whose mantle you took has fallen, their tale ends in shadow. "
                    "Yet your spirit returns, for the Valar's trial is not yet complete.");

            {
                const char* paragraphs[] = { text, transition_text };
                const byte attrs[] = { TERM_WHITE, TERM_L_BLUE };

                (void)metarun_ui_show_story_modal_auto(title, TERM_RED,
                    paragraphs, attrs, 2, steamdeck, accept_label, "Return");
            }
        }

        pool = mem_free(pool);
        u32b pool_before = metar.fallen_score_total;
        refresh_current_metar_score();
        compute_blessing_pool();
        if (final_score > 0 && metar.fallen_score_total == pool_before) {
            metar.fallen_score_total += (u32b)final_score;
            update_blessing_ledger(&metar);
            (void)sync_current_metarun_slot(false);
        }
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        check_run_end();
        save_metaruns();
        return;
    }
    else if (!escaped_with_sils) {
        log_debug("Player escaped without Silmarils - no narrative needed");
        refresh_current_metar_score();
        compute_blessing_pool();
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        save_metaruns();
        return;                        /* no further narrative needed  */
    }

    /* ------------------------------------------------------------- */
    /*        Enhanced Narrative Path â€“ escaped with â‰¥1 Silmaril     */
    /* ------------------------------------------------------------- */
    log_info("Player escaped with %d Silmarils - displaying victory narrative",
        sil_count);
    byte final_sils = metarun_play_escape_victory_semantic_sequence(
        sil_count, allow_treachery, allow_kinslay);

    metarun_gain_silmarils(final_sils);
    log_info("Added %d Silmarils to metarun total (now %d)", final_sils,
        metar.silmarils);
    refresh_current_metar_score();
    metarun_show_recent_story_parts_semantic(3);

    if (challenge_disconnected) {
        metarun_mark_challenge_completed(CHALLENGE_DISCONNECTED);
        challenge_disconnected_success = true;
    }
    if (challenge_single_stair) {
        metarun_mark_challenge_completed(CHALLENGE_SINGLE_STAIR);
    }
    if (challenge_fixed_exp) {
        metarun_mark_challenge_completed(CHALLENGE_FIXED_50K_XP);
    }
    if (challenge_tulkas_blunt) {
        metarun_mark_challenge_completed(CHALLENGE_TULKAS_BLUNT);
    }
    if (challenge_torchlight) {
        metarun_mark_challenge_completed(CHALLENGE_TORCHLIGHT);
    }
    if (challenge_disconnected_success &&
        quest_get_state(QUEST_ID_MANDOS_BETRAYER) < QUEST_STATE_REWARDED &&
        metarun_quest_completion_count(METARUN_QUEST_MANDOS_BETRAYER) < quest_completion_cap(QUEST_ID_MANDOS_BETRAYER)) {
        show_mandos_third_unlock_message();
    }
    maybe_unlock_orome_great_hunt(challenge_single_stair);
    maybe_unlock_niena_mercy_purchase(challenge_fixed_exp, fixed_challenge_before);
    if (challenge_tulkas_blunt &&
        metarun_challenge_completion_count(CHALLENGE_TULKAS_BLUNT) > blunt_challenge_before) {
        show_tulkas_blunt_unlock_message();
    }
    if (challenge_torchlight) {
        maybe_unlock_queen_of_stars_purchase(challenge_torchlight, torchlight_challenge_before);
    }

    compute_blessing_pool();
    announce_blessing_gain(blessing_points_before);
    blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    check_run_end();
    /* Save persistent settings when exiting */
    metarun_save_persistent_settings();
    
    /* Save metarun data (deaths, silmarils, etc.) */
    save_metaruns();
}


int required_survivor_target(int win_goal)
{
    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    int required = 0;
    if (remaining_silmarils > 0) {
        required = (remaining_silmarils + 2) / 3;
        required += CURSE_GET(CUR_DEATH);
        if (required < 1) required = 1;
    }

    if (required < 0) required = 0;
    return required;
}


/* ======================  run-state logic  ====================== */
/* ------------------------------------------------------------------ *
 *  Decide whether the current run just ended, and react accordingly. *
 *  Message text adapts automatically if you set LOSECON_DEATHS = 1.  *
 *  Loss condition takes precedence over win condition.               *
 * ------------------------------------------------------------------ */
void check_run_end(void)
{
    int win_goal = WINCON_SILMARILS;   /* fallback */

    if (runtype_info && metar.type < z_info->rt_max)
    {
        win_goal = runtype_info[metar.type].win_con ? runtype_info[metar.type].win_con : WINCON_SILMARILS;
    }

    /* Keep blessing and survivor data aligned with the score file */
    compute_blessing_pool();
    int alive = metar.alive_characters;

    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    int required_survivors = required_survivor_target(win_goal);

    /* Loss takes precedence over victory */
    if (alive < required_survivors) {
        bool steamdeck = steamdeck_controls_active();
        char accept_label[16] = "";
        char defeat_text[256];

        log_info("Metarun DEFEAT: alive=%d required=%d (remaining silmarils=%d)",
                 alive, required_survivors, remaining_silmarils);
        strnfmt(defeat_text, sizeof defeat_text,
                "Only %d hero%s remain, yet %d must endure to reclaim the remaining Silmarils. "
                "This tale falls into shadow; begin anew to kindle hope once more.",
                alive, (alive == 1) ? "" : "es",
                required_survivors);
        if (steamdeck) {
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
                sizeof(accept_label));
        }
        {
            const char* paragraphs[] = { defeat_text };
            const byte attrs[] = { TERM_WHITE };

            (void)metarun_ui_show_story_modal_auto("The Trial's End",
                TERM_RED, paragraphs, attrs, 1, steamdeck, accept_label,
                "Begin Anew");
        }

        start_new_metarun();
        return;
    }

    if (metar.silmarils >= win_goal) {
        bool steamdeck = steamdeck_controls_active();
        char accept_label[16] = "";
        char victory_text[256];
        const char *implementation_note = "(This final trial is yet to be implemented.)";

        log_info("Metarun VICTORY: %d Silmarils collected (goal: %d)", metar.silmarils, win_goal);
        strnfmt(victory_text, sizeof victory_text,
                "%d Silmarils reclaimed from Morgoth's crown! "
                "Hope kindles anew; your long trial approaches its end. "
                "Yet one final ordeal awaits: your ultimate destiny, "
                "as your true self faces the Last Trial.",
                win_goal);
        if (steamdeck) {
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
                sizeof(accept_label));
        }
        {
            const char* paragraphs[] = { victory_text, implementation_note };
            const byte attrs[] = { TERM_L_GREEN, TERM_L_DARK };

            (void)metarun_ui_show_story_modal_auto("The Trial's End",
                TERM_YELLOW, paragraphs, attrs, 2, steamdeck, accept_label,
                "Begin Anew");
        }

        start_new_metarun();
    }
}





/* ------------------------------------------------------------------
 *  Start a brand-new meta-run.
 *  We snapshot the finished run **after** the array has been grown,
 *  so we only write once and always with the final pointer.
 * ------------------------------------------------------------------ */
static void start_new_metarun(void)
{
    log_info("Starting new metarun (previous run ID: %d)", metar.id);
    log_debug("metarun: pre-finalize state (wizard=%d, noscore=0x%04X, savefile='%s')",
              p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
              p_ptr ? (unsigned)p_ptr->noscore : 0,
              savefile);

    u32b previous_id = metar.id;
    if (!sync_current_metarun_slot(true)) {
        log_warn("metarun: unable to snapshot current run before rollover (idx=%d, max=%d)",
                 current_run, metarun_max);
    }

     /* Before wiping scores for the next run, backup and clear save files */
     backup_and_clear_saves();
     
     /* Before wiping scores for the next run, finalize current ones:
         - mark all alive entries as dead by their own hand
         - save any corresponding savefiles as dead
         Then archive/clear the score file so the next run starts clean. */
     metarun_finalize_scores_and_saves();
     clear_scorefile();

    /* Hard purge the current savefile if this was a noscore wizard/debug run */
    if (p_ptr && (p_ptr->wizard || (p_ptr->noscore & 0x0008)) && (p_ptr->noscore & 0x000F)) {
        if (savefile[0]) {
            bool deleted;
            safe_setuid_grab();
            deleted = fd_kill(savefile);
            safe_setuid_drop();
            if (deleted) {
                log_info("metarun: deleted noscore savefile '%s'", savefile);
            } else {
                log_warn("metarun: failed to delete noscore savefile '%s'", savefile);
            }
        }
    } else {
        log_info("metarun: purge skipped (wizard=%d, noscore=0x%04X, savefile='%s')",
                 p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
                 p_ptr ? (unsigned)p_ptr->noscore : 0,
                 savefile);
    }
    /* Save old state */
    s16b old_max   = metarun_max;
    metarun *old   = metaruns;

    /* Try to allocate a new array for one more run */
    metarun *tmp = mem_alloc_array(old_max + 1, metarun);
    if (!tmp) {
        /* Allocation failed - keep everything as is */
        return;
    }

    /* Copy over the previous runs (if any) */
    if (old) {
        memcpy(tmp, old, sizeof(metarun) * old_max);
    }

    /* Free the old array just once */
    old = mem_free(old);

    /* Commit the new array and size */
    metaruns    = tmp;
    metarun_max = old_max + 1;

    /* Initialize the brand-new slot */
    reset_defaults(&metaruns[metarun_max - 1]);
    metaruns[metarun_max - 1].id = previous_id + 1;
    metaruns[metarun_max - 1].type = 0; /* Default to type 0 (Normal) for new metaruns */

    /* Update globals */
    current_run      = metarun_max - 1;
    metar             = metaruns[current_run];
    metarun_created  = true;  /* Set flag to show story intro for new metarun */

    /* Apply difficulty curses based on the runtype */
    apply_difficulty_curses(&metar);

    /* Persist and prepare */
    save_metaruns();      /* safe now that metarunsâ‰ NULL */ 
    ensure_run_dir(&metar);
    log_info("New metarun %d created and initialized", metar.id);
}

bool metarun_ui_add_section_row(app_ui_panel* panel, byte attr,
    const char* text)
{
    if (!panel || !text || !text[0]
        || !app_ui_panel_add_row(panel, 0, attr, true, false, "", text, ""))
    {
        return false;
    }

    panel->rows[panel->row_count - 1].flags |= APP_UI_ITEM_FLAG_SECTION;
    return true;
}

bool metarun_ui_add_value_row(app_ui_panel* panel, byte label_attr,
    const char* label, byte value_attr, const char* value)
{
    return app_ui_panel_add_row_ex(panel, 0, label_attr, value_attr, 0, '\0',
        true, false, "", label ? label : "", value ? value : "");
}

bool metarun_ui_add_effect_row_ex(app_ui_panel* panel, int id,
    bool selected)
{
    const curse_type* cu;
    const char* effect = NULL;
    const char* name;
    char label[APP_UI_LABEL_MAX];
    char meta[APP_UI_META_MAX];
    byte attr;
    char icon;
    int stacks;
    int magnitude;
    bool is_blessing;

    if (!panel || id < 0 || id >= z_info->cu_max)
        return false;

    stacks = CURSE_GET(id);
    if (!stacks)
        return true;

    is_blessing = (stacks < 0);
    magnitude = is_blessing ? -stacks : stacks;
    name = is_blessing ? metarun_blessing_display_name(id)
                       : metarun_curse_display_name(id);
    attr = is_blessing ? TERM_L_GREEN : TERM_RED;
    icon = is_blessing ? '+' : '-';
    cu = &cu_info[id];

    if (CURSE_SEEN(id))
    {
        effect = is_blessing
            ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
            : (cu->power ? cu_text + cu->power : NULL);
    }

    strnfmt(label, sizeof(label), "%s", name);
    if (effect && *effect)
        strnfmt(meta, sizeof(meta), "%c%d  %s", icon, magnitude, effect);
    else
        strnfmt(meta, sizeof(meta), "%c%d", icon, magnitude);

    return app_ui_panel_add_row_ex(panel, id, attr, attr, attr, icon, true,
        selected, "", label, meta);
}

bool metarun_ui_add_effect_row(app_ui_panel* panel, int id)
{
    return metarun_ui_add_effect_row_ex(panel, id, false);
}

void metarun_trim_first_line(char* dst, size_t dst_size,
    const char* source)
{
    char* newline;

    if (!dst || dst_size == 0)
        return;

    SDL_strlcpy(dst, source ? source : "", dst_size);
    newline = strchr(dst, '\n');
    if (newline)
        *newline = '\0';
}

#define METARUN_UI_WRAP_WIDTH 68
#define METARUN_UI_FADE_DURATION_MS 250u
#define METARUN_UI_FADE_FRAME_SLICE_MS 16u

static byte metarun_ui_fade_alpha(u32b elapsed_ms, u32b duration_ms)
{
    if (duration_ms == 0 || elapsed_ms >= duration_ms)
        return 0xFFu;

    return (byte)((elapsed_ms * 255u) / duration_ms);
}

static void metarun_ui_set_scene_alpha(app_ui_scene* scene, byte alpha)
{
    if (!scene)
        return;

    for (u16b i = 0; i < scene->panel_count; i++)
        scene->panels[i].alpha = alpha;
}

bool metarun_ui_present_scene(app_ui_scene* scene, bool fade_in)
{
    u64b start_ms;

    if (!scene)
        return false;
    if (!fade_in)
    {
        metarun_ui_set_scene_alpha(scene, 0xFFu);
        return ui_information_scene_present_ui(scene);
    }

    start_ms = platform_monotonic_ms();
    while (true)
    {
        u64b now_ms = platform_monotonic_ms();
        u64b elapsed_ms64 = (now_ms > start_ms) ? (now_ms - start_ms) : 0;
        u32b elapsed_ms = (elapsed_ms64 > METARUN_UI_FADE_DURATION_MS)
            ? METARUN_UI_FADE_DURATION_MS
            : (u32b)elapsed_ms64;
        byte alpha = metarun_ui_fade_alpha(elapsed_ms,
            METARUN_UI_FADE_DURATION_MS);

        metarun_ui_set_scene_alpha(scene, alpha);
        if (!ui_information_scene_present_ui(scene))
            return false;
        if (elapsed_ms >= METARUN_UI_FADE_DURATION_MS)
            break;

        platform_frame_delay_ms(MIN(METARUN_UI_FADE_FRAME_SLICE_MS,
            METARUN_UI_FADE_DURATION_MS - elapsed_ms));
    }

    metarun_ui_set_scene_alpha(scene, 0xFFu);
    return true;
}

app_ui_panel* metarun_ui_begin_browser_scene(app_ui_scene* scene,
    byte title_attr, const char* title, byte subtitle_attr,
    const char* subtitle)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_SCROLL_ROWS;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, title_attr, title ? title : "");
    if (subtitle && subtitle[0])
        app_ui_panel_set_subtitle(panel, subtitle_attr, subtitle);

    return panel;
}

static app_ui_panel* metarun_ui_begin_modal_scene(app_ui_scene* scene,
    byte title_attr, const char* title)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 340, 620);
    app_ui_panel_set_title(panel, title_attr, title ? title : "");
    return panel;
}

app_ui_panel* metarun_ui_begin_story_scene(app_ui_scene* scene,
    byte title_attr, const char* title)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    scene->flags = APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    app_ui_panel_set_widths(panel, 720, 1180);
    app_ui_panel_set_title(panel, title_attr, title ? title : "");
    return panel;
}

static bool metarun_ui_add_wrapped_text_lines(app_ui_panel* panel, byte attr,
    const char* text, bool detail)
{
    const char* cursor = text;

    if (!panel || !text || !text[0])
        return true;

    while (*cursor)
    {
        const char* start;
        const char* split = NULL;
        size_t len = 0;
        char line[APP_UI_TEXT_MAX];

        while (*cursor == ' ' || *cursor == '\t')
            cursor++;

        if (*cursor == '\n')
        {
            cursor++;
            if (detail)
            {
                if (!app_ui_panel_add_detail_line(panel, attr, " "))
                    return false;
            }
            else if (!app_ui_panel_add_body_line(panel, attr, " "))
            {
                return false;
            }
            continue;
        }

        if (detail)
        {
            if (panel->detail_line_count >= APP_UI_DETAIL_LINE_MAX)
                break;
        }
        else if (panel->body_line_count >= APP_UI_BODY_LINE_MAX)
        {
            break;
        }

        start = cursor;
        while (cursor[len] && cursor[len] != '\n')
        {
            if (len < METARUN_UI_WRAP_WIDTH)
            {
                if (cursor[len] == ' ')
                    split = cursor + len;
            }
            else if (split)
            {
                break;
            }
            else if (len >= (sizeof(line) - 1))
            {
                break;
            }
            len++;
        }

        if (cursor[len] && cursor[len] != '\n' && split && split > start)
            len = (size_t)(split - start);
        else if (len == 0 && cursor[len] && cursor[len] != '\n')
            len = 1;

        if (len >= sizeof(line))
            len = sizeof(line) - 1;

        memcpy(line, start, len);
        line[len] = '\0';
        while (len > 0 && isspace((unsigned char)line[len - 1]))
            line[--len] = '\0';

        if (line[0])
        {
            if (detail)
            {
                if (!app_ui_panel_add_detail_line(panel, attr, line))
                    return false;
            }
            else if (!app_ui_panel_add_body_line(panel, attr, line))
            {
                return false;
            }
        }

        cursor = start + len;
        while (*cursor == ' ' || *cursor == '\t')
            cursor++;
        if (*cursor == '\n')
            cursor++;
    }

    return true;
}

static bool metarun_ui_add_wrapped_body_lines(app_ui_panel* panel, byte attr,
    const char* text)
{
    return metarun_ui_add_wrapped_text_lines(panel, attr, text, false);
}

bool metarun_ui_add_wrapped_detail_lines(app_ui_panel* panel, byte attr,
    const char* text)
{
    return metarun_ui_add_wrapped_text_lines(panel, attr, text, true);
}

bool metarun_ui_add_story_paragraphs(app_ui_scene* scene,
    app_ui_panel* panel, const char* const* paragraphs, const byte* attrs,
    int paragraph_count)
{
    bool wrote_any = false;

    if (!scene || !panel)
        return false;
    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;

    for (int i = 0; i < paragraph_count; i++)
    {
        const char* text = paragraphs ? paragraphs[i] : NULL;
        byte attr = attrs ? attrs[i] : TERM_WHITE;

        if (!text || !text[0])
            continue;
        if (wrote_any
            && !app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE,
                STORY_FLAG_USE, "\n\n"))
        {
            return false;
        }
        if (!app_ui_panel_add_rich_text_ex(scene, panel, attr, STORY_FLAG_USE,
                text))
        {
            return false;
        }
        wrote_any = true;
    }

    if (!wrote_any
        && !app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE,
            STORY_FLAG_USE, " "))
    {
        return false;
    }

    return true;
}

void metarun_ui_clear_pending_input(void)
{
    app_session* session = app_session_current();

    log_debug("[metarun-esc-trace] clear_pending_input active=%d",
        ui_information_scene_is_active() ? 1 : 0);
    if (session)
        app_session_clear_inputs(session);
    input_byte_queue_clear();
    input_clear_movement_commands();
}

bool metarun_ui_add_effect_detail_lines(app_ui_panel* panel, int id)
{
    const curse_type* cu;
    const char* desc = NULL;
    const char* power = NULL;
    const char* name;
    char line[APP_UI_TEXT_MAX];
    int stacks;
    int magnitude;
    bool is_blessing;
    byte attr;

    if (!panel || id < 0 || id >= z_info->cu_max)
        return false;

    stacks = CURSE_GET(id);
    if (!stacks)
        return true;

    is_blessing = (stacks < 0);
    magnitude = is_blessing ? -stacks : stacks;
    name = is_blessing ? metarun_blessing_display_name(id)
                       : metarun_curse_display_name(id);
    attr = is_blessing ? TERM_L_GREEN : TERM_RED;
    cu = &cu_info[id];

    app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Effect");
    strnfmt(line, sizeof(line), "%s", name);
    if (!app_ui_panel_add_detail_line(panel, attr, line))
        return false;
    strnfmt(line, sizeof(line), "%s stacks: %d",
        is_blessing ? "Blessing" : "Curse", magnitude);
    if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, line))
        return false;

    desc = is_blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : NULL)
        : (cu->text ? cu_text + cu->text : NULL);
    if (desc && *desc)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_WHITE, desc))
            return false;
    }

    if (CURSE_SEEN(id))
    {
        power = is_blessing
            ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
            : (cu->power ? cu_text + cu->power : NULL);
    }

    if (power && *power)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        strnfmt(line, sizeof(line), "Effect: %s", power);
        if (!metarun_ui_add_wrapped_detail_lines(panel, attr, line))
            return false;
    }
    else if (!CURSE_SEEN(id))
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_L_DARK,
                "(Effect not yet identified)"))
        {
            return false;
        }
    }

    return true;
}

bool metarun_ui_add_known_curse_detail_lines(app_ui_panel* panel, int id)
{
    curse_type* cu;
    const char* curse_name;
    const char* blessing_name;
    const char* curse_desc = NULL;
    const char* curse_power = NULL;
    const char* blessing_desc = NULL;
    const char* blessing_power = NULL;
    bool show_blessing_name = false;
    char line[APP_UI_TEXT_MAX];

    if (!panel || !z_info || !cu_info || id < 0 || id >= z_info->cu_max
        || !CURSE_SEEN(id) || !cu_info[id].name)
    {
        return false;
    }

    cu = &cu_info[id];
    curse_name = metarun_curse_display_name(id);
    blessing_name = metarun_blessing_display_name(id);
    show_blessing_name = blessing_name && blessing_name[0]
        && strcmp(blessing_name, curse_name) != 0;

    app_ui_panel_set_detail_title(panel, TERM_L_RED, curse_name);

    if (show_blessing_name)
    {
        strnfmt(line, sizeof(line), "Blessing: %s", blessing_name);
        if (!app_ui_panel_add_detail_line(panel, TERM_L_GREEN, line))
            return false;
    }

    curse_desc = cu->text ? cu_text + cu->text : NULL;
    curse_power = cu->power ? cu_text + cu->power : NULL;
    blessing_desc = cu->blessing_text ? cu_text + cu->blessing_text : NULL;
    blessing_power = cu->blessing_power ? cu_text + cu->blessing_power : NULL;

    if (curse_desc && *curse_desc)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_WHITE, curse_desc))
            return false;
    }

    if (curse_power && *curse_power)
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        strnfmt(line, sizeof(line), "Effect: %s", curse_power);
        if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_RED, line))
            return false;
    }

    if ((blessing_desc && *blessing_desc) || (blessing_power && *blessing_power))
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_WHITE, " "))
            return false;
        if (show_blessing_name)
        {
            if (!app_ui_panel_add_detail_line(panel, TERM_L_GREEN,
                    "Blessing Aspect"))
            {
                return false;
            }
        }

        if (blessing_desc && *blessing_desc
            && !metarun_ui_add_wrapped_detail_lines(panel, TERM_L_GREEN,
                blessing_desc))
        {
            return false;
        }

        if (blessing_power && *blessing_power)
        {
            strnfmt(line, sizeof(line), "Blessing effect: %s", blessing_power);
            if (!metarun_ui_add_wrapped_detail_lines(panel, TERM_L_GREEN, line))
                return false;
        }
    }

    if ((!curse_desc || !*curse_desc) && (!curse_power || !*curse_power)
        && (!blessing_desc || !*blessing_desc)
        && (!blessing_power || !*blessing_power))
    {
        if (!app_ui_panel_add_detail_line(panel, TERM_L_DARK,
                "(No lore text recorded)"))
        {
            return false;
        }
    }

    return true;
}

bool metarun_ui_show_story_modal(const char* title, byte title_attr,
    const char* const* paragraphs, const byte* attrs, int paragraph_count,
    bool steamdeck, const char* accept_label, const char* action_label)
{
    app_ui_scene scene;
    app_ui_panel* panel;

    panel = metarun_ui_begin_story_scene(&scene, title_attr, title);
    if (!panel)
        return false;
    if (!metarun_ui_add_story_paragraphs(&scene, panel, paragraphs, attrs,
            paragraph_count))
    {
        return false;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        steamdeck ? accept_label : "Any",
        (action_label && action_label[0]) ? action_label : "Continue");

    if (!metarun_ui_present_scene(&scene, true))
        return false;

    (void)ui_information_scene_wait_key_nonrepeat();
    metarun_ui_clear_pending_input();
    return true;
}

bool metarun_ui_show_notice_modal(const char* title, byte title_attr,
    const char* const* lines, const byte* attrs, int line_count, bool steamdeck,
    const char* accept_label)
{
    app_ui_scene scene;
    app_ui_panel* panel;

    panel = metarun_ui_begin_modal_scene(&scene, title_attr, title);
    if (!panel)
        return false;

    for (int i = 0; i < line_count; i++)
    {
        byte attr = attrs ? attrs[i] : TERM_WHITE;

        if (!lines || !lines[i] || !lines[i][0])
            continue;
        if (!metarun_ui_add_wrapped_body_lines(panel, attr, lines[i]))
            return false;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        steamdeck ? accept_label : "Any", "Continue");

    if (!metarun_ui_present_scene(&scene, true))
        return false;

    (void)ui_information_scene_wait_key_nonrepeat();
    metarun_ui_clear_pending_input();
    return true;
}

bool metarun_ui_confirm_modal(const char* title, byte title_attr,
    const char* const* lines, const byte* attrs, int line_count, bool steamdeck,
    const char* accept_label, const char* back_label)
{
    bool first_present = true;

    while (true)
    {
        app_ui_scene scene;
        app_ui_panel* panel;
        int key;

        panel = metarun_ui_begin_modal_scene(&scene, title_attr, title);
        if (!panel)
            return false;

        for (int i = 0; i < line_count; i++)
        {
            byte attr = attrs ? attrs[i] : TERM_WHITE;

            if (!lines || !lines[i] || !lines[i][0])
                continue;
            if (!metarun_ui_add_wrapped_body_lines(panel, attr, lines[i]))
                return false;
        }

        if (steamdeck)
        {
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                accept_label, "Confirm");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                back_label, "Cancel");
        }
        else
        {
            (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
                "Y/Enter", "Confirm");
            (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
                "N/Esc", "Cancel");
        }

        if (!metarun_ui_present_scene(&scene, first_present))
            return false;
        first_present = false;

        key = ui_information_scene_wait_key_nonrepeat();
        if (steamdeck)
        {
            if (key == steamdeck_confirm_key() || key == '\r' || key == '\n'
                || key == '6')
            {
                metarun_ui_clear_pending_input();
                return true;
            }
            if (key == steamdeck_back_key() || key == ESCAPE || key == '4')
            {
                metarun_ui_clear_pending_input();
                return false;
            }
        }
        else
        {
            if (key == 'y' || key == 'Y' || key == '\r' || key == '\n'
                || key == '6')
            {
                metarun_ui_clear_pending_input();
                return true;
            }
            if (key == 'n' || key == 'N' || key == ESCAPE || key == '4'
                || key == 'h' || key == 'H')
            {
                metarun_ui_clear_pending_input();
                return false;
            }
        }
    }
}

int metarun_ui_choose_curse_scene(int n,
    const int* picks, bool steamdeck,
    const char* accept_label)
{
    int selected = 0;
    char subtitle[APP_UI_TEXT_MAX];
    bool first_present = true;

    strnfmt(subtitle, sizeof(subtitle),
        "Dark powers demand their price. Choose %s curse.",
        metarun_curse_choice_label(n));

    while (true)
    {
        app_ui_scene scene;
        app_ui_panel* panel;
        const curse_type* cu;
        int key;

        panel = metarun_ui_begin_browser_scene(&scene, TERM_YELLOW,
            "The Valar's Judgment", TERM_SLATE, subtitle);
        if (!panel)
            return -1;

        (void)app_ui_panel_add_body_line(panel, TERM_L_DARK,
            "Choose the curse you must bear.");

        for (int i = 0; i < CURSE_MENU_LINES; i++)
        {
            char key_buf[APP_UI_KEY_MAX];

            strnfmt(key_buf, sizeof(key_buf), "%c", (char)('a' + i));
            if (!app_ui_panel_add_row(panel, picks[i], TERM_L_RED, true,
                    i == selected, key_buf,
                    cu_name + cu_info[picks[i]].name, ""))
            {
                return -1;
            }
        }

        cu = &cu_info[picks[selected]];
        app_ui_panel_set_detail_title(panel, TERM_L_BLUE, "Selected Curse");
        if (!app_ui_panel_add_detail_line(panel, TERM_L_RED,
                cu_name + cu->name))
        {
            return -1;
        }
        if (cu->text
            && !metarun_ui_add_wrapped_detail_lines(panel, TERM_SLATE,
                cu_text + cu->text))
        {
            return -1;
        }

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            steamdeck ? accept_label : "Enter", "Accept");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            "8/2", "Move");
        if (!steamdeck) {
            (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
                "A-C", "Select");
        }

        if (!metarun_ui_present_scene(&scene, first_present))
            return -1;
        first_present = false;

        key = ui_information_scene_wait_key_nonrepeat();
        if (key == '\r' || key == '\n' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()) || key == '6')
        {
            metarun_ui_clear_pending_input();
            return picks[selected];
        }
        if (key == '8' || key == 'k' || key == '-') {
            selected = (selected + CURSE_MENU_LINES - 1) % CURSE_MENU_LINES;
            continue;
        }
        if (key == '2' || key == 'j' || key == '+') {
            selected = (selected + 1) % CURSE_MENU_LINES;
            continue;
        }
        if (key == ESCAPE
            || (steamdeck && key == steamdeck_back_key())
            || (!steamdeck && (key == 'h' || key == 'H')))
        {
            metarun_ui_clear_pending_input();
            return picks[0];
        }
        if (key >= 'a' && key < 'a' + CURSE_MENU_LINES) {
            metarun_ui_clear_pending_input();
            return picks[key - 'a'];
        }
        if (key >= 'A' && key < 'A' + CURSE_MENU_LINES) {
            metarun_ui_clear_pending_input();
            return picks[key - 'A'];
        }
    }
}

