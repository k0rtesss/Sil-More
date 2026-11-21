#include "angband.h"
#include "quest/quest-rewards-beta.h"
#include "quest/quest-runtime.h"
#include "metarun-internal.h"

int any_curse_flag_active(u32b flag)
{
    /* Intended for CUR flags such as CUR_NOCHOICE (curse-only, not blessings). */
    if (!z_info || !cu_info) return 0;
    int count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        int stacks = CURSE_GET(id);
        if (stacks > 0 && (cu_info[id].flags_u & flag)) count += stacks;
    }
    return count;
}

static int weighted_random_curse(void)
{
    long total = 0;
    int  w_max = 1;

    /* Does the hero's lineage carry the flag? */
    bool tilt = (p_info[p_ptr->prace].flags  & RHF_CURSE) ||
                (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    /* Pass 1 - find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 - sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
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

    if (!total) return rand_int(z_info->cu_max);    /* safety net */

    /* Pass 3 - roulette wheel */
    long pick = rand_int(total), run = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
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

    return rand_int(z_info->cu_max);                /* unreachable */
}

static void curse_menu_fade_title(void)
{
    const int duration_ms = 450;
    const int frame_ms = 16;

    sdl_poetry_screen_set_alpha(0, 0, 0, 0);
    for (int elapsed = 0; elapsed < duration_ms; elapsed += frame_ms)
    {
        byte alpha = (byte)((elapsed * 255) / duration_ms);

        sdl_poetry_screen_set_alpha(alpha, 0, 0, 0);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY,
            MIN(frame_ms, duration_ms - elapsed));
    }
    sdl_poetry_screen_set_alpha(255, 0, 0, 0);
    Term_fresh();
}

static bool curse_menu_fade_choice(int choice, byte color)
{
    const int duration_ms = 600;
    const int frame_ms = 16;

    sdl_poetry_screen_set_choice_visible(choice, true,
        color, TERM_L_WHITE);
    sdl_poetry_screen_set_choice_alpha(choice, 0);
    for (int elapsed = 0; elapsed < duration_ms; elapsed += frame_ms)
    {
        char ch;
        byte alpha = (byte)((elapsed * 255) / duration_ms);

        if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
        {
            sdl_poetry_screen_set_choice_alpha(choice, 255);
            Term_fresh();
            return false;
        }
        sdl_poetry_screen_set_choice_alpha(choice, alpha);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY,
            MIN(frame_ms, duration_ms - elapsed));
    }
    sdl_poetry_screen_set_choice_alpha(choice, 255);
    Term_fresh();
    return true;
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

    if (quest_beta_cleanse_curse()) return;

    CURSE_ADD(idx, 1);
    log_info("Added curse stack: %s (now %d stacks)", cu_name + cu_info[idx].name, CURSE_GET(idx));
    save_metaruns();
}

/* Affinities use the same colours as the ability browser. */
static byte blessing_choice_color(const curse_type *cu)
{
    static const byte stat_colors[A_MAX] = {
        TERM_ORANGE, TERM_L_BLUE, TERM_L_GREEN, TERM_VIOLET
    };
    static const u32b affinities[S_SPC] = {
        RHF_MEL_AFFINITY, RHF_ARC_AFFINITY, RHF_EVN_AFFINITY, RHF_STL_AFFINITY,
        RHF_PER_AFFINITY, RHF_WIL_AFFINITY, RHF_SMT_AFFINITY, RHF_SNG_AFFINITY
    };
    u32b flags = cu->blessing_flags_u;

    for (int s = 0; s < A_MAX; s++)
        if (cu->cu_adj[s]) return stat_colors[s];
    for (int s = 0; s < S_SPC; s++) {
        if (cu->blessing_flags & affinities[s]) {
            byte color = ability_skill_color(s);
            if (color == TERM_RED) return TERM_L_RED;
            if (color == TERM_GREEN) return TERM_L_GREEN;
            return color;
        }
    }
    if (flags & (CUR_RES_FIRE_SHIFT | CUR_MDS_SHIFT)) return TERM_ORANGE;
    if (flags & (CUR_LIGHTR | CUR_LIGHTP | CUR_CHEST_WOOD)) return TERM_YELLOW;
    if (flags & (CUR_RES_COLD_SHIFT | CUR_RES_STUN_SHIFT | CUR_MON_ARM_SIDE
            | CUR_ARMOR_SIDE_SHIFT)) return TERM_L_BLUE;
    if (flags & (CUR_RES_FEAR_SHIFT | CUR_RES_CONFU_SHIFT | CUR_RES_HALLU_SHIFT
            | CUR_MON_WIL | CUR_IDENT_DIFF)) return TERM_VIOLET;
    if (flags & (CUR_HUNGER | CUR_RES_POIS_SHIFT | CUR_WEAK)) return TERM_L_GREEN;
    return TERM_L_UMBER;
}

/* The draft edits a local set of offer rows until Confirm. A curse still
 * resolves to one row (including the historical first choice on Escape). */
static u16b menu_choose_effects(cptr title, const int *offers, int count, bool blessings)
{
    static const byte curse_colors[CURSE_MENU_LINES] = { TERM_L_RED, TERM_ORANGE, TERM_L_UMBER };
    bool steamdeck = steamdeck_controls_active();
    bool letters = sdl_menu_letters_enabled();
    int focus = 0;
    bool follow = false;
    u16b marked = 0;
    int marked_count = 0;

    screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();
    sdl_poetry_screen_begin_choices(title);
    if (!sdl_poetry_screen_active())
        quit("Mandatory SDL effect menu renderer is unavailable");

    for (int row = 0; row < count; row++) {
        int id = offers[row];
        const curse_type *cu = &cu_info[id];
        cptr name = blessings ? blessing_display_name(id) : cu_name + cu->name;
        u32b power = blessings ? cu->blessing_power : (CURSE_SEEN(id) ? cu->power : 0);
        u32b lore = blessings ? cu->blessing_text : cu->text;
        char label[160];
        if (letters)
            strnfmt(label, sizeof(label), "%c) %s", 'a' + row, name);
        else
            SDL_strlcpy(label, name, sizeof(label));
        sdl_poetry_screen_add_choice(row, label, power ? cu_text + power : "",
            lore ? cu_text + lore : "");
        if (blessings) {
            sdl_poetry_screen_set_choice_visible(row, true, blessing_choice_color(cu), TERM_L_WHITE);
            sdl_poetry_screen_set_choice_alpha(row, 255);
        }
    }

    if (blessings) {
        sdl_poetry_screen_update(true, TERM_YELLOW, false, TERM_WHITE, false, TERM_WHITE, false);
        sdl_poetry_screen_set_alpha(255, 0, 0, 255);
        sdl_poetry_screen_set_prompt("", true);
    } else {
        bool fast_forward = false;
        curse_menu_fade_title();
        Term_xtra(TERM_XTRA_DELAY, 1000);
        for (int row = 0; row < count; row++) {
            if (fast_forward) {
                sdl_poetry_screen_set_choice_visible(row, true, curse_colors[row], TERM_L_WHITE);
                sdl_poetry_screen_set_choice_alpha(row, 255);
            } else if (!curse_menu_fade_choice(row, curse_colors[row])) {
                fast_forward = true;
            }
            if (row + 1 < count) Term_xtra(TERM_XTRA_DELAY, 1000);
        }
        char prompt[96];
        if (steamdeck) {
            char accept[16], back[16];
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept, sizeof(accept));
            metarun_prompt_label(steamdeck_back_key(), "B", back, sizeof(back));
            strnfmt(prompt, sizeof(prompt), "D-pad navigate  [%s] accept  [%s] cancel", accept, back);
        } else {
            SDL_strlcpy(prompt, sdl_touch_only_device_active() ? "Tap the highlighted curse to accept"
                : letters ? "Arrows navigate  Enter accept  a/b/c select" : "Arrows navigate  Enter accept",
                sizeof(prompt));
        }
        sdl_poetry_screen_set_prompt(prompt, true);
    }

    for (;;) {
        int clicked = -1, action = UI_MENU_CLICK_PRIMARY;
        bool confirm_button = false;
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        if (blessings)
            sdl_poetry_screen_set_selection(marked, marked_count, RUN_BLESSING_COUNT);
        sdl_poetry_screen_set_highlight(focus, follow);
        follow = false;
        Term_fresh();
        int key = metarun_inkey_hidden();

        if (ui_menu_click_take_action(&clicked, &action)) {
            ui_menu_click_clear();
            if (clicked >= 0 && clicked < count) {
                bool was_focused = focus == clicked;
                focus = clicked;
                if (action == UI_MENU_CLICK_HOVER
                    || (blessings ? action != UI_MENU_CLICK_PRIMARY : !was_focused))
                    continue;
                key = ' ';
            } else if (action != UI_MENU_CLICK_PRIMARY) {
                continue;
            } else if (clicked == -1) {
                key = ESCAPE;
            } else if (clicked == -2) {
                confirm_button = true;
                key = '\r';
            } else {
                continue;
            }
        }
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key())) {
            marked = blessings ? 0 : 1;
            break;
        }
        if (blessings && (confirm_button || (steamdeck ? key == steamdeck_secondary_key()
                                                       : key == '\r' || key == '\n'))) {
            if (marked_count == RUN_BLESSING_COUNT) break;
            continue;
        }

        int letter = tolower((unsigned char)key) - 'a';
        if (letters && letter >= 0 && letter < count) {
            focus = letter;
            follow = true;
            key = ' ';
        }
        if (key == '8' || key == 'k' || key == '2' || key == 'j') {
            int direction = key == '8' || key == 'k' ? -1 : 1;
            focus = (focus + count + direction) % count;
            follow = true;
        } else if (key == ' ' || key == '\r' || key == '\n' || key == '6'
            || (steamdeck && key == steamdeck_confirm_key())) {
            u16b bit = (u16b)(1U << focus);
            if (!blessings) {
                marked = bit;
                break;
            }
            if (marked & bit) {
                marked &= ~bit;
                marked_count--;
            } else if (marked_count < RUN_BLESSING_COUNT) {
                marked |= bit;
                marked_count++;
            }
        }
    }
    ui_menu_click_clear();
    sdl_poetry_screen_hide();
    screen_pop_supporting_panes_hidden();
    screen_load();
    return marked;
}

static int menu_choose_one_curse_aux(int n, bool force_menu)
{
    int pick[CURSE_MENU_LINES];
    char title[96];
    const char *ordinals[] = { "a", "the second", "the third", "the fourth" };

    if (!force_menu && any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool duplicate;
        do {
            pick[i] = weighted_random_curse();
            duplicate = CURSE_CURSE_CAP(pick[i])
                && CURSE_CURSE_STACK(pick[i]) >= CURSE_CURSE_CAP(pick[i]);
            for (int j = 0; j < i; j++)
                if (pick[j] == pick[i]) duplicate = true;
        } while (duplicate);
    }

    strnfmt(title, sizeof(title), "Choose %s curse",
        ordinals[(n >= 0 && n < (int)N_ELEMENTS(ordinals)) ? n : 0]);
    u16b choice = menu_choose_effects(title, pick, CURSE_MENU_LINES, false);
    for (int row = 0; row < CURSE_MENU_LINES; row++)
        if (choice & (1U << row)) return pick[row];
    return -1;
}

static bool run_blessing_offers(const metarun *tale, int offers[RUN_BLESSING_OFFER_COUNT])
{
    int eligible[METAR_CURSE_SLOTS];
    int count = 0;
    for (int id = 0; id < MIN(z_info->cu_max, METAR_CURSE_SLOTS); id++) {
        if (cu_info[id].blessing_name && cu_info[id].blessing_power)
            eligible[count++] = id;
    }
    if (count < RUN_BLESSING_OFFER_COUNT) {
        log_warn("Too few blessing templates to offer %s", RUN_BLESSING_MODE_NAME);
        return false;
    }

    u64b seed = ((u64b)tale->id << 32) | tale->next_blessing_seed;
    u64b saved_rng = Rand_state_push(seed ? seed : 1);
    for (int row = 0; row < RUN_BLESSING_OFFER_COUNT; row++) {
        int index = rand_int(count);
        offers[row] = eligible[index];
        eligible[index] = eligible[--count];
    }
    Rand_state_pop(saved_rng);
    return true;
}

u64b metarun_choose_run_blessings(void)
{
    int offers[RUN_BLESSING_OFFER_COUNT];
    if (!run_blessing_offers(&metar, offers)) return 0;
    u16b marked = menu_choose_effects(RUN_BLESSING_MODE_NAME " (Easy)",
        offers, RUN_BLESSING_OFFER_COUNT, true);
    u64b chosen = 0;
    for (int row = 0; row < RUN_BLESSING_OFFER_COUNT; row++)
        if (marked & (1U << row)) chosen |= 1ULL << offers[row];
    return chosen;
}

int menu_choose_one_curse(int n)
{
    return menu_choose_one_curse_aux(n, false);
}

int metarun_debug_preview_curse_menu(void)
{
    return menu_choose_one_curse_aux(0, true);
}


/* ------------------------------------------------------------------ *
 *  Debug helper - wipe every active curse for the current meta-run.  *
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

static int choose_escape_curses_ui_aux(int n, int out[4], bool preview)
{
    // int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;
    int taken = 0;

    /* Display intro with fade-in effect */
    screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();

    char intro_text[512];
    strnfmt(intro_text, sizeof(intro_text),
            "The Valar watch silently as Morgoth's malice reaches out from shadow-"
            "Your triumph has drawn his wrath. His dark will twists fate, "
            "forcing upon you the final choice-%s curse%s you must bear.",
            (n == 1) ? "a" : (n == 2) ? "two" : (n == 3) ? "three" : "four",
            (n == 1) ? "" : "s");

    metarun_show_poetry_scene("The Valar's Judgment", TERM_L_BLUE,
        intro_text, TERM_L_WHITE, "", TERM_SLATE,
        "[Press any key to face your destiny]");
    Term_clear();

    for (int i = 0; i < n; i++)
    {
        int idx = preview ? menu_choose_one_curse_aux(i, true)
                          : menu_choose_one_curse(i);

        if (idx < 0)
            break;
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        if (!preview)
            add_curse_stack(idx);            /* gameplay side-effect */
        if (taken < 4) out[taken++] = idx;
    }

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();

    /* Restore screen state to fix character_icky imbalance */
    screen_pop_supporting_panes_hidden();
    screen_load();

    return taken;
}

int choose_escape_curses_ui(int n, int out[4])
{
    return choose_escape_curses_ui_aux(n, out, false);
}

int metarun_debug_choose_escape_curses(int n, int out[4])
{
    return choose_escape_curses_ui_aux(n, out, true);
}

/****************  Oath-breaking curse chooser with fade ************/

/*
 * Shows the oath-specific consequence and Morgoth's response with fade-in,
 * then opens the permanent curse selection menu.
 * Returns the selected curse index.
 */
int choose_oath_breaking_curse_ui(int oath_id)
{
    /* Display curse message with fade-in effect */
    screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();

    /* Get oath-specific permanent message (E: field from oath.txt) */
    char* perm_msg = oath_permanent_message(oath_id);
    cptr consequence = (perm_msg && perm_msg[0])
        ? perm_msg : "Your oath is forever broken in this age.";

    /* Show Morgoth's attention text with fade in red */
    char intro_text[256];
    strnfmt(intro_text, sizeof(intro_text),
            "The breach of your sacred vow has drawn Morgoth's attention. "
            "His malice reaches out to compound your suffering with a curse you must bear.");

    metarun_show_poetry_scene("The Sundering of Sacred Vows", TERM_L_RED,
        consequence, TERM_L_RED, intro_text, TERM_RED,
        "[Press any key to face your judgment]");
    Term_clear();

    /* Let the player choose 1 curse from 3 options */
    int idx = menu_choose_one_curse(0);
    log_debug("Player selected curse %d for oath breaking", idx);

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();

    /* Restore screen state */
    screen_pop_supporting_panes_hidden();
    screen_load();

    return idx;
}
