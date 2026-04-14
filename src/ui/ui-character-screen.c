/* File: ui/ui-character-screen.c */

#include "angband.h"
#include "app/app-ui.h"
#include "externs.h"

#include "log/log.h"
#include "platform-input.h"
#include "platform-story-font.h"
#include "ui/story_font.h"
#include "ui/ui-character-screen.h"
#include "ui/ui-information-scene.h"

#include <ctype.h>

static void display_player_get_layout_size(int* wid, int* hgt)
{
    int current_wid = (Term && Term->wid > 0) ? Term->wid : 80;
    int current_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;

    if (current_wid < 1)
        current_wid = 80;
    if (current_hgt < 1)
        current_hgt = 24;

    if (wid)
        *wid = current_wid;
    if (hgt)
        *hgt = current_hgt;
}

static void display_skill(int skill, int row, int col)
{
    /* Enable story font for skill name (if enabled) */
    if (story_character_enabled()) {
        sdl_story_font_enable();
    }

    put_str(skill_names_full[skill], row, col);

    /* Disable story font - all numbers must use monospace */
    sdl_story_font_disable();

    /* All numbers in monospace font */
    c_put_str(
        TERM_L_GREEN, format("%3d", p_ptr->skill_use[skill]), row, col + 11);
    c_put_str(TERM_SLATE, "=", row, col + 15);
    c_put_str(
        TERM_GREEN, format("%2d", p_ptr->skill_base[skill]), row, col + 17);
    if (p_ptr->skill_stat_mod[skill] != 0)
        c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_stat_mod[skill]), row,
            col + 20);
    if (p_ptr->skill_equip_mod[skill] != 0)
        c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_equip_mod[skill]),
            row, col + 24);
    if (p_ptr->skill_misc_mod[skill] != 0)
        c_put_str(TERM_SLATE, format("%+3d", p_ptr->skill_misc_mod[skill]), row,
            col + 28);
}

/* ----- story-font aware helpers ---------------------------------------- */

/* ===== 20-column, right-anchored stat lines ============================= */

#define LINEW20 20
#define COMPACT_RIGHT_PAD 2

static int compact_right_column_start(int wid)
{
    int col = wid - COMPACT_RIGHT_PAD - LINEW20;
    if (col < 1)
        col = 1;
    return col;
}

static bool display_player_compact_tight_spacing(void)
{
    int wid = 80;
    int hgt = 24;

    display_player_get_layout_size(&wid, &hgt);
    (void)wid;
    if (hgt < 1)
        hgt = 24;

    return (hgt <= 18);
}

static int display_player_compact_start_row(void)
{
    return display_player_compact_tight_spacing() ? 1 : 2;
}

static int display_player_compact_scroll = 0;
static int display_player_compact_max_scroll = 0;

static void display_player_putch(int x, int y, byte attr, char ch)
{
    char buf[2];

    buf[0] = ch;
    buf[1] = '\0';
    c_put_str(attr, buf, y, x);
}

static void display_player_clear_cells(int x, int y, int width)
{
    char buf[64];

    if (width <= 0)
        return;

    memset(buf, ' ', sizeof(buf) - 1u);
    buf[sizeof(buf) - 1u] = '\0';

    while (width > 0)
    {
        int chunk = width;

        if (chunk > (int)sizeof(buf) - 1)
            chunk = (int)sizeof(buf) - 1;

        buf[chunk] = '\0';
        put_str(buf, y, x);
        buf[chunk] = ' ';
        x += chunk;
        width -= chunk;
    }
}

static void put_label_fit(int x, int y, const char* label, int start)
{
    int maxw = start - x;
    if (maxw <= 0)
        return;

    char buf[64];
    strnfmt(buf, sizeof(buf), "%-*.*s", maxw, maxw, label);
    put_str(buf, y, x);
}

/* Pair: numbers block ends at x + LINEW20. cur_w + 1 + rhs_w == block width. */
static void put_pair20_right(int x, int y,
                             const char *label,
                             const char *cur,  int cur_w, byte col_cur,
                             char sep,
                             const char *rhs,  int rhs_w, byte col_rhs)
{
    int end   = x + LINEW20;
    int blk_w = cur_w + 1 + rhs_w;
    int start = end - blk_w;

    if (story_character_enabled())
        sdl_story_font_enable();

    put_label_fit(x, y, label, start);

    if (story_character_enabled())
        sdl_story_font_disable();

    /* Clear the numeric block so shorter values don't leave artifacts */
    display_player_clear_cells(start, y, blk_w);

    /* Trim both strings to their allotted widths */
    const char *cur_text = cur ? cur : "";
    int cur_len = (int)strlen(cur_text);
    if (cur_len > cur_w)
    {
        cur_text += cur_len - cur_w;
        cur_len = cur_w;
    }

    const char *rhs_text = rhs ? rhs : "";
    int rhs_len = (int)strlen(rhs_text);
    if (rhs_len > rhs_w)
    {
        rhs_text += rhs_len - rhs_w;
        rhs_len = rhs_w;
    }

    /* Right-align the combined "cur<sep>rhs" block as a whole so the slash
     * always hugs the digits while the entire string stays anchored to the
     * column edge. */
    int total_len = cur_len + 1 + rhs_len;
    if (total_len > blk_w)
        total_len = blk_w;
    int text_start = end - total_len;
    if (text_start < start)
        text_start = start;

    if (cur_len > 0)
        c_put_str(col_cur, cur_text, y, text_start);

    char s[2] = { sep, '\0' };
    put_str(s, y, text_start + cur_len);

    if (rhs_len > 0)
        c_put_str(col_rhs, rhs_text, y, text_start + cur_len + 1);
}

/* Single value: value block ends at x + LINEW20. */
static void put_single20_right(int x, int y,
                               const char *label,
                               const char *val, int val_w, byte col_val)
{
    int end   = x + LINEW20;
    int start = end - val_w;

    if (story_character_enabled())
        sdl_story_font_enable();

    put_label_fit(x, y, label, start);

    if (story_character_enabled())
        sdl_story_font_disable();

    display_player_clear_cells(start, y, val_w);
    const char *val_text = val ? val : "";
    int val_len = (int)strlen(val_text);
    if (val_len > val_w)
    {
        val_text += val_len - val_w;
        val_len = val_w;
    }

    if (val_len > 0)
    {
        int text_start = end - val_len;
        if (text_start < start)
            text_start = start;
        c_put_str(col_val, val_text, y, text_start);
    }
}

static void put_single_right(int x, int y, int line_w,
                             const char* label,
                             const char* val, int val_w, byte col_val)
{
    int end;
    int start;

    if (line_w < 1)
        return;

    if (val_w > line_w - 1)
        val_w = line_w - 1;
    if (val_w < 1)
        return;

    end = x + line_w - 1;
    start = end - val_w + 1;

    if (story_character_enabled())
        sdl_story_font_enable();

    put_label_fit(x, y, label ? label : "", start);

    if (story_character_enabled())
        sdl_story_font_disable();

    display_player_clear_cells(start, y, val_w);
    const char* val_text = val ? val : "";
    int val_len = (int)strlen(val_text);
    if (val_len > val_w)
    {
        val_text += val_len - val_w;
        val_len = val_w;
    }

    if (val_len > 0)
    {
        int text_start = end - val_len + 1;
        if (text_start < start)
            text_start = start;
        c_put_str(col_val, val_text, y, text_start);
    }
}

static byte format_deep_call_value(char* buf, size_t buflen, int max_width)
{
    int base_increment = 0;
    int total_increment = 0;
    int effective_total;
    char pct_buf[16];
    byte attr = TERM_L_GREEN;

    if (!buf || buflen == 0)
        return attr;

    buf[0] = '\0';
    if (max_width < 1)
        return attr;
    (void)max_width;

    min_depth_timer_status(&base_increment, NULL, &total_increment, NULL, NULL);

    effective_total = total_increment;
    if (effective_total < 0)
        effective_total = 0;

    if (base_increment > 0)
    {
        long pct = ((long)effective_total * 100L + (base_increment / 2))
            / base_increment;
        if (pct > 999L)
            pct = 999L;
        strnfmt(pct_buf, sizeof(pct_buf), "%ld%%", pct);
    }
    else if (effective_total > 0)
    {
        SDL_strlcpy(pct_buf, "INF%", sizeof(pct_buf));
    }
    else
    {
        SDL_strlcpy(pct_buf, "0%", sizeof(pct_buf));
    }

    if (base_increment <= 0)
        attr = (effective_total > 0) ? TERM_L_GREEN : TERM_YELLOW;
    else if (effective_total > base_increment)
        attr = TERM_L_GREEN;
    else if (effective_total == base_increment)
        attr = TERM_L_BLUE;
    else if (effective_total > 0)
        attr = TERM_YELLOW;
    else
        attr = TERM_L_RED;

    SDL_strlcpy(buf, pct_buf, buflen);

    return attr;
}

static bool format_min_depth_progress_bar(char* buf, size_t buflen, int line_w)
{
    int progress = 0;
    int threshold = 1;
    int bar_width;
    int filled;

    if (!buf || buflen == 0)
        return false;

    buf[0] = '\0';
    if (line_w < 12)
        return false;

    min_depth_timer_status(NULL, NULL, NULL, &progress, &threshold);
    if (threshold < 1)
        threshold = 1;
    if (progress < 0)
        progress = 0;
    if (progress > threshold)
        progress = threshold;

    bar_width = line_w - 2;
    if (bar_width > 32)
        bar_width = 32;
    if (bar_width < 8)
        return false;

    filled = (progress * bar_width) / threshold;
    if (filled < 0)
        filled = 0;
    if (filled > bar_width)
        filled = bar_width;

    if ((size_t)(bar_width + 3) > buflen)
        return false;

    buf[0] = '[';
    for (int i = 0; i < bar_width; i++)
        buf[i + 1] = (i < filled) ? '#' : '.';
    buf[bar_width + 1] = ']';
    buf[bar_width + 2] = '\0';
    return true;
}

static bool display_player_min_depth_progress_bar_line(int x, int y, int line_w)
{
    char bar_buf[96];
    int bar_len;
    int out_col;

    if (!format_min_depth_progress_bar(bar_buf, sizeof(bar_buf), line_w))
        return false;

    display_player_clear_cells(x, y, line_w);
    bar_len = (int)strlen(bar_buf);
    out_col = x + (line_w - bar_len) / 2;
    if (out_col < x)
        out_col = x;
    c_put_str(TERM_L_BLUE, bar_buf, y, out_col);
    return true;
}

static void display_player_deep_call_line(int x, int y, int line_w)
{
    const char* label = (line_w >= 16) ? "Deep Call" : "Call";
    int val_w = line_w - (int)strlen(label);
    char value_buf[96];
    byte value_attr;

    if (line_w < 6)
        return;

    if (val_w < 4)
    {
        label = "";
        val_w = line_w;
    }

    value_attr = format_deep_call_value(value_buf, sizeof(value_buf), val_w);
    put_single_right(x, y, line_w, label, value_buf, val_w, value_attr);
}

static void display_player_xtra_info(int mode)
{
    int term_wid = 80;
    int term_hgt = 24;
    int wide_offset = 0;
    int col_stats;
    int col_flags;
    int col_skills;
    bool compact_overview = (mode == DISPLAY_PLAYER_MODE_COMPACT_DESC_FLAGS);
    bool show_skills = !compact_overview;

    int row_stats = 2;
    int row_flags = 2;

    int skill;
    char cur[32], rhs[32], val[64], buf[160];

    byte history_attr = (mode == 2) ? TERM_YELLOW : TERM_WHITE;

    display_player_get_layout_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;
    (void)term_hgt;

    if (term_wid > 80)
        wide_offset = (term_wid - 80) / 2;

    col_stats = wide_offset + 1;
    col_flags = wide_offset + 23;
    col_skills = wide_offset + 41;

    if (compact_overview)
        col_flags = col_stats + 22;

    /* -------------------- STATS (col 1..20) ----------------------------- */

    strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);
    strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);
    put_pair20_right(col_stats, row_stats++,
                     "Exp",
                     cur, 5, TERM_L_GREEN,
                     '/', rhs, 6, TERM_L_GREEN);

    {
        long cur_b = (long)(p_ptr->total_weight / 10L);
        long max_b = (long)(weight_limit() / 10L);
        strnfmt(cur, sizeof(cur), "%ld", cur_b);
        strnfmt(rhs, sizeof(rhs), "%ld", max_b);
        put_pair20_right(col_stats, row_stats++,
                         "Burden",
                         cur, 4, (cur_b <= max_b) ? TERM_L_GREEN : TERM_YELLOW,
                         '/', rhs, 4, TERM_L_GREEN);
    }

    if (turn > 0)
    {
        long cur_d = (long)(p_ptr->depth * 50);
        long min_d = (long)(min_depth() * 50);

        if (cur_d > 1000)
            cur_d = 1000;
        if (min_d > 1000)
            min_d = 1000;

        strnfmt(cur, sizeof(cur), "%ld", cur_d);
        strnfmt(rhs, sizeof(rhs), "%ld", min_d);

        put_pair20_right(col_stats, row_stats++,
                         "Depth c/m",
                         cur, 4, (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW,
                         '/', rhs, 4, TERM_L_GREEN);

        if (display_player_min_depth_progress_bar_line(col_stats, row_stats,
                LINEW20))
            row_stats++;
    }

    display_player_deep_call_line(col_stats, row_stats++, LINEW20);

    comma_number(buf, playerturn);
    put_single20_right(col_stats, row_stats++,
                       "Turn", buf, 12, TERM_L_GREEN);

    strnfmt(val, sizeof(val), "%d", p_ptr->cur_light);
    put_single20_right(col_stats, row_stats++,
                       "Light", val, 2, TERM_L_GREEN);

    strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
    put_single20_right(col_stats, row_stats++,
                       "Melee", val, 12, TERM_L_BLUE);

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        put_single20_right(col_stats, row_stats++,
                           "Melee x2", val, 12, TERM_L_BLUE);
    }

    if (p_ptr->mds2 > 0)
    {
        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
                p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
                p_ptr->mdd2, p_ptr->mds2);
        put_single20_right(col_stats, row_stats++,
                           "Offhand", val, 12, TERM_L_BLUE);
    }

    strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
    put_single20_right(col_stats, row_stats++,
                       "Bows", val, 12, TERM_L_BLUE);

    strnfmt(val, sizeof(val), "[%+d,%d-%d]",
            p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
    put_single20_right(col_stats, row_stats++,
                       "Armor", val, 12, TERM_L_BLUE);

    {
        int chp = p_ptr->chp;
        if (chp > 999)
            chp = 999;
        int mhp = p_ptr->mhp;
        if (mhp > 999)
            mhp = 999;
        strnfmt(cur, sizeof(cur), "%d", chp);
        strnfmt(rhs, sizeof(rhs), "%d", mhp);
        put_pair20_right(col_stats, row_stats++,
                         "Health",
                         cur, 3, TERM_L_BLUE,
                         '/', rhs, 3, TERM_L_BLUE);
    }

    {
        int csp = p_ptr->csp;
        if (csp > 999)
            csp = 999;
        int msp = p_ptr->msp;
        if (msp > 999)
            msp = 999;
        strnfmt(cur, sizeof(cur), "%d", csp);
        strnfmt(rhs, sizeof(rhs), "%d", msp);
        put_pair20_right(col_stats, row_stats++,
                         "Voice",
                         cur, 3, TERM_L_BLUE,
                         '/', rhs, 3, TERM_L_BLUE);
    }

    if (p_ptr->song1 != SNG_NOTHING) {
        strnfmt(val, sizeof(val), "%s",
            b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name);
        put_single20_right(col_stats, row_stats++,
            "Song", val, 14, TERM_L_BLUE);
    }
    if (p_ptr->song2 != SNG_NOTHING) {
        strnfmt(val, sizeof(val), "%s",
            b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name);
        put_single20_right(col_stats, row_stats++,
            "Song", val, 14, TERM_L_BLUE);
    }

    {
        int race  = p_ptr->prace;
        int character = p_ptr->pcharacter;

        byte attr_affinity   = TERM_GREEN;
        byte attr_mastery    = TERM_L_GREEN;
        byte attr_penalty    = TERM_RED;
        byte attr_gr_penalty = TERM_L_RED;

        typedef struct {
            const char *txt;
            byte col;
        } line_t;

        line_t uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
        int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH(arr, n, text, color) do { (arr)[(n)].txt = (text); (arr)[(n)++].col = (color); } while (0)
#define HANDLE_SKILL_EX(LABEL, AFF_FLAG, PEN_FLAG)                                      \
        do {                                                                            \
            int score = 0;                                                              \
            if (p_info[race].flags      & (AFF_FLAG)) score++;                          \
            if (c_info[character].flags & (AFF_FLAG)) score++;                          \
            if (p_info[race].flags      & (PEN_FLAG)) score--;                          \
            if (c_info[character].flags & (PEN_FLAG)) score--;                          \
            score += curse_flag_count_rhf(AFF_FLAG);                                    \
            score -= curse_flag_count_rhf(PEN_FLAG);                                    \
            if (score >  2) score =  2;                                                 \
            if (score < -2) score = -2;                                                 \
            if (score ==  2)      PUSH(ma_buf,  ma_n,  LABEL "++", attr_mastery);      \
            else if (score == 1)  PUSH(af_buf,  af_n,  LABEL "+ ", attr_affinity);     \
            else if (score == -1) PUSH(pen_buf, pen_n, LABEL "- ", attr_penalty);      \
            else if (score == -2) PUSH(pen_buf, pen_n, LABEL "--", attr_gr_penalty);   \
        } while (0)
#define HANDLE_UNIQUE(LABEL, FLAG, COLOR)                                               \
        do {                                                                            \
            if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG)))    \
                PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                               \
        } while (0)
#define HANDLE_UNIQUE_U(LABEL, FLAG, COLOR)                                             \
        do {                                                                            \
            if (c_info[character].flags_u & (FLAG))                                     \
                PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                               \
        } while (0)

        HANDLE_SKILL_EX("melee",      RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
        HANDLE_SKILL_EX("evasion",    RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
        HANDLE_SKILL_EX("stealth",    RHF_STL_AFFINITY, RHF_STL_PENALTY);
        HANDLE_SKILL_EX("archery",    RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
        HANDLE_SKILL_EX("will",       RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
        HANDLE_SKILL_EX("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
        HANDLE_SKILL_EX("smithing",   RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
        HANDLE_SKILL_EX("song",       RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
        HANDLE_SKILL_EX("bow",        RHF_BOW_PROFICIENCY, 0);
        HANDLE_SKILL_EX("axe",        RHF_AXE_PROFICIENCY, 0);

        HANDLE_UNIQUE_U("Master Artisan",     UNQ_SMT_FEANOR,   TERM_VIOLET);
        HANDLE_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL,      TERM_VIOLET);
        HANDLE_UNIQUE_U("Chosen of Ulmo",     UNQ_WIL_TUOR,     TERM_VIOLET);
        HANDLE_UNIQUE_U("Indomitable Will",   UNQ_EARENDIL,     TERM_VIOLET);
        HANDLE_UNIQUE_U("Orome Himself",      UNQ_WIL_FIN,      TERM_VIOLET);
        HANDLE_UNIQUE_U("Songs of Power",     UNQ_SNG_FIN,      TERM_VIOLET);
        HANDLE_UNIQUE_U("Elven Dance",        UNQ_SNG_LUT,      TERM_VIOLET);
        HANDLE_UNIQUE_U("Girdle of Melian",   UNQ_SNG_MEL,      TERM_VIOLET);
        HANDLE_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR,  TERM_VIOLET);
        HANDLE_UNIQUE_U("Old Master",         UNQ_SMT_GAMIL,    TERM_VIOLET);
        HANDLE_UNIQUE_U("Ring Master",        UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
        HANDLE_UNIQUE_U("Aure entuluva",      UNQ_SNG_HURIN,    TERM_VIOLET);
        HANDLE_UNIQUE_U("Voice of the Girdle",UNQ_SNG_THINGOL,  TERM_VIOLET);
        HANDLE_UNIQUE_U("Forgotten",          UNQ_MIM,          TERM_VIOLET);
        HANDLE_UNIQUE_U("One Handed",         UNQ_MEL_MAEDHROS, TERM_VIOLET);
        HANDLE_UNIQUE_U("Agarwaen",           UNQ_WIL_TURIN,    TERM_VIOLET);
        HANDLE_UNIQUE_U("Shadow Walker",      UNQ_SNG_TURGON,   TERM_VIOLET);
        HANDLE_UNIQUE("Gift of Eru",          RHF_GIFTERU,      TERM_VIOLET);
        HANDLE_UNIQUE("Seafarer",             RHF_FREE,         TERM_VIOLET);

        HANDLE_UNIQUE("Kinslayer",            RHF_KINSLAYER,    TERM_UMBER);
        HANDLE_UNIQUE("Treacherous",          RHF_TREACHERY,    TERM_UMBER);
        HANDLE_UNIQUE("Doom of Mandos",       RHF_CURSE,        TERM_UMBER);
        HANDLE_UNIQUE("Morgoth Curse",        RHF_MOR_CURSE,    TERM_UMBER);

        if (story_character_enabled()) {
            sdl_story_font_enable();
        }

        for (int i = 0; i < uniq_n; ++i)
            c_put_str(uniq_buf[i].col, uniq_buf[i].txt, row_flags++,
                col_flags);
        for (int i = 0; i < ma_n; ++i)
            c_put_str(ma_buf[i].col, ma_buf[i].txt, row_flags++, col_flags);
        for (int i = 0; i < af_n; ++i)
            c_put_str(af_buf[i].col, af_buf[i].txt, row_flags++, col_flags);
        for (int i = 0; i < pen_n; ++i)
            c_put_str(pen_buf[i].col, pen_buf[i].txt, row_flags++,
                col_flags);

        if (story_character_enabled()) {
            sdl_story_font_disable();
        }

        if (show_skills)
        {
            for (skill = 0; skill < S_MAX; skill++) {
                if (skill == S_SPC)
                    continue;
                display_skill(skill, 6 + skill, col_skills);
            }
        }

        if (story_character_enabled()) {
            sdl_story_font_enable();
        }

        log_debug("Character history: terminal width=%d, using wrap=%d", term_wid,
            term_wid - 1);
        text_out_wrap   = term_wid - 1;
        text_out_indent = 1;
        move_cursor(15, text_out_indent);
        text_out_to_screen(history_attr, p_ptr->history);
        text_out_wrap   = 0;
        text_out_indent = 0;

        if (story_character_enabled()) {
            sdl_story_font_disable();
        }

#undef HANDLE_SKILL_EX
#undef HANDLE_UNIQUE
#undef HANDLE_UNIQUE_U
#undef PUSH
    }
}

/*
 * Equippy chars
 */
static void display_player_equippy(int y, int x)
{
    int i;

    byte a;
    char c;

    object_type* o_ptr;

    /* Dump equippy chars */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; ++i)
    {
        /* Object */
        o_ptr = &inventory[i];

        /* Skip empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Get attr/char for display */
        a = object_attr(o_ptr);
        c = object_char(o_ptr);

        /* Dump */
        display_player_putch(x + i - INVEN_WIELD, y, a, c);
    }
}

/*
 * Hack -- see below
 */
static const byte display_player_flag_set[4] = { 1, 2, 2, 1 };

/*
 * Hack -- see below
 */
static const u32b display_player_flag_head[4]
    = { TR1_MEL, TR2_RES_COLD, TR2_SLOW_DIGEST, TR1_SLAY_ORC };

/*
 * Hack -- see below
 */
static cptr display_player_flag_names[4][9]
    = { { "  Mel:", "  Arc:", "  Stl:", "  Per:", "  Wil:", "  Smt:", "  Sng:",
            "#####:", "#####:" },

          {
              " Cold:",
              " Fire:",
              " Elec:",
              " Pois:",
              " Dark:",
              " Fear:",
              "Blind:",
              " Conf:",
              " Stun:",
          },

          { "Sustn:",
              "Light:", "Regen:", "Invis:", " Free:", "#####:", "Speed:",
              "#####:", "#####:" },

          { "  Orc:", "Troll:", " Wolf:", "Spidr:", " Undd:", "Rauko:",
              "Dragn:", "#####:", "#####:" } };

/*
 * Special display, part 1
 */
static void display_player_flag_info(void)
{
    int x, y, i, n;

    int row, col;

    int set;
    u32b head;
    u32b flag;
    cptr name;

    u32b f[4];

    sdl_story_font_enable();

    /* Four columns */
    for (x = 0; x < 4; x++)
    {
        /* Reset */
        row = 9;
        col = 20 * x - 2;

        /* Header */
        c_put_str(TERM_WHITE, "abcdefghijkl@", row++, col + 8);

        /* Nine rows */
        for (y = 0; y < 9; y++)
        {
            byte name_attr = TERM_WHITE;

            /* Extract set */
            set = display_player_flag_set[x];

            /* Extract head */
            head = display_player_flag_head[x];

            /* Extract flag */
            flag = (head << y);

            /* Extract name */
            name = display_player_flag_names[x][y];

            /* Check equipment */
            for (n = 8, i = INVEN_WIELD; i < INVEN_TOTAL; ++i, ++n)
            {
                byte attr = TERM_SLATE;

                object_type* o_ptr;

                /* Object */
                o_ptr = &inventory[i];

                /* Known flags */
                object_flags_known(o_ptr, &f[1], &f[2], &f[3]);

                /* Color columns by parity */
                if (i % 2)
                    attr = TERM_L_WHITE;

                /* Non-existant objects */
                if (!o_ptr->k_idx)
                    attr = TERM_L_DARK;

                /* Check flags */
                if (f[set] & flag)
                {
                    c_put_str(TERM_L_BLUE, "+", row, col + n);
                    if (name_attr != TERM_L_GREEN)
                        name_attr = TERM_L_BLUE;
                }

                /* Default */
                else
                {
                    c_put_str(attr, ".", row, col + n);
                }
            }

            /* Default */
            c_put_str(TERM_SLATE, ".", row, col + n);

            /* Check flags */
            if (f[set] & flag)
            {
                c_put_str(TERM_L_BLUE, "+", row, col + n);
                if (name_attr != TERM_L_GREEN)
                    name_attr = TERM_L_BLUE;
            }

            /* Header */
            c_put_str(name_attr, name, row, col + 2);

            /* Advance */
            row++;
        }

        /* Footer */
        c_put_str(TERM_WHITE, "abcdefghijkl@", row++, col + 8);

        /* Equippy */
        display_player_equippy(row++, col + 8);
    }

    sdl_story_font_disable();
}

/*
 * Display interactive character screen tutorial
 * Shows 4 stages explaining different parts of the character screen
 * with actual character data displayed
 */
static void tutorial_prompt_label(int binding, const char* fallback, char* out,
    size_t out_size)
{
    if (!out || !out_size)
        return;

    sdl_gamepad_action_binding_short_label(binding, out, out_size);
    if (streq(out, "(unbound)") || streq(out, "Multiple"))
        SDL_strlcpy(out, fallback, out_size);
}

typedef struct tutorial_render_target {
    app_ui_scene* ui_scene;
    app_ui_panel* ui_panel;
    int width;
    int height;
} tutorial_render_target;

static bool tutorial_begin_document_ui_scene(app_ui_scene* scene,
    app_ui_panel** out_panel)
{
    app_ui_panel* panel;

    if (!scene || !out_panel)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_DOCUMENT;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;
    *out_panel = panel;
    return true;
}

static void tutorial_layout_size(int* wid, int* hgt)
{
    if (wid)
        *wid = 80;
    if (hgt)
        *hgt = 24;
}

static void tutorial_render_text(tutorial_render_target* target, int col,
    int row, byte attr, const char* text)
{
    if (!target || !target->ui_scene || !target->ui_panel || !text || !text[0])
        return;

    (void)app_ui_panel_add_document_text(target->ui_scene,
        target->ui_panel, (s16b)row, (s16b)col, attr, text);
}

static void tutorial_put_centered(tutorial_render_target* target, int row,
    byte attr, const char* text)
{
    int wid = 80;
    int len;
    int col = 0;

    if (!target || !text)
        return;

    if (target->width > 0)
        wid = target->width;
    len = (int)strlen(text);
    if (len < wid)
        col = (wid - len) / 2;
    if (col < 0)
        col = 0;

    tutorial_render_text(target, col, row, attr, text);
}

static int tutorial_put_trunc(tutorial_render_target* target, int col, int row,
    int max_wid, byte attr, const char* text)
{
    char buf[256];
    size_t len;
    int keep;

    if (!text || !text[0])
        return 0;

    if (max_wid < 4)
        max_wid = 4;

    len = strlen(text);
    if ((int)len <= max_wid)
    {
        tutorial_render_text(target, col, row, attr, text);
        return 1;
    }

    if (max_wid >= (int)sizeof(buf))
        max_wid = (int)sizeof(buf) - 1;

    keep = max_wid - 3;
    if (keep < 0)
        keep = 0;
    SDL_strlcpy(buf, text, (size_t)keep + 1);
    SDL_strlcat(buf, "...", sizeof(buf));
    tutorial_render_text(target, col, row, attr, buf);
    return 1;
}

static int tutorial_put_wrapped_limited(tutorial_render_target* target,
    const char* text, int start_col, int start_row, int max_width, int max_row,
    byte color)
{
    int term_width = 80;
    int term_height = 24;
    char line_buf[512];
    int row = start_row;
    int line_pos = 0;
    const char* p = text;

    if (!text || !text[0])
        return 0;

    if (target)
    {
        if (target->width > 0)
            term_width = target->width;
        if (target->height > 0)
            term_height = target->height;
    }

    if (max_width <= 0)
        max_width = term_width - start_col - 1;
    if (max_width < 10)
        max_width = 10;

    if (max_row <= 0 || max_row > term_height)
        max_row = term_height;

    while (*p && row < max_row)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            line_buf[line_pos] = '\0';
            if (line_pos > 0)
            {
                tutorial_render_text(target, start_col, row, color, line_buf);
                row++;
            }
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                line_buf[wrap_pos] = '\0';
                tutorial_render_text(target, start_col, row, color, line_buf);

                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_buf[line_pos] = '\0';
                tutorial_render_text(target, start_col, row, color, line_buf);
                line_pos = 0;
            }
            row++;
            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0 && row < max_row)
    {
        line_buf[line_pos] = '\0';
        tutorial_render_text(target, start_col, row, color, line_buf);
        row++;
    }

    return row - start_row;
}

static int tutorial_count_wrapped_lines_ex(const char* text, int max_width,
    bool preserve_empty)
{
    if (!text || !text[0])
        return 0;

    if (max_width < 10)
        max_width = 10;

    char line_buf[512];
    int line_pos = 0;
    int count = 0;
    const char* p = text;

    while (*p)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            if (line_pos > 0 || preserve_empty)
                count++;
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_pos = 0;
            }

            count++;
            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0)
        count++;

    return count;
}

static int tutorial_put_wrapped_slice_ex(tutorial_render_target* target,
    const char* text, int start_col, int start_row, int max_width, int max_row,
    byte color, int skip_lines, int max_lines, bool preserve_empty)
{
    if (!text || !text[0])
        return 0;

    if (skip_lines < 0)
        skip_lines = 0;
    if (max_lines < 1)
        max_lines = 1;
    if (max_width < 10)
        max_width = 10;

    char line_buf[512];
    int row = start_row;
    int line_pos = 0;
    int line_index = 0;
    int drawn = 0;
    const char* p = text;

    while (*p && row < max_row)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            bool have_line = (line_pos > 0) || preserve_empty;
            if (have_line)
            {
                if (line_index >= skip_lines && drawn < max_lines)
                {
                    if (line_pos > 0)
                    {
                        line_buf[line_pos] = '\0';
                        tutorial_render_text(target, start_col, row, color,
                            line_buf);
                    }
                    row++;
                    drawn++;
                    if (drawn >= max_lines || row >= max_row)
                        break;
                }
                line_index++;
            }
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (line_index >= skip_lines && drawn < max_lines)
            {
                if (wrap_pos > 0)
                    line_buf[wrap_pos] = '\0';
                else
                    line_buf[line_pos] = '\0';
                if (line_buf[0])
                    tutorial_render_text(target, start_col, row, color,
                        line_buf);
                row++;
                drawn++;
                if (drawn >= max_lines || row >= max_row)
                    break;
            }

            if (wrap_pos > 0)
            {
                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_pos = 0;
            }

            line_index++;
            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0 && row < max_row && drawn < max_lines)
    {
        if (line_index >= skip_lines)
        {
            line_buf[line_pos] = '\0';
            tutorial_render_text(target, start_col, row, color, line_buf);
            row++;
            drawn++;
        }
    }

    return drawn;
}

typedef struct {
    const char* txt;
    byte col;
} tutorial_trait_line;

static int tutorial_collect_traits(tutorial_trait_line* out, int max_out)
{
    if (!out || max_out <= 0)
        return 0;

    int n = 0;
    int race = p_ptr->prace;
    int character = p_ptr->pcharacter;

    byte col_mastery = TERM_L_GREEN;
    byte col_affinity = TERM_GREEN;
    byte col_penalty = TERM_L_RED;
    byte col_gr_penalty = TERM_RED;

#define PUSH_TRAIT(text_value, color_value) \
    do { \
        if ((text_value) && n < max_out) { out[n].txt = (text_value); out[n].col = (color_value); n++; } \
    } while (0)

#define CHECK_SKILL(LABEL, AFF_FLAG, PEN_FLAG) \
    do { \
        int sc = 0; \
        if (p_info[race].flags & (AFF_FLAG)) sc++; \
        if (c_info[character].flags & (AFF_FLAG)) sc++; \
        if ((PEN_FLAG) && (p_info[race].flags & (PEN_FLAG))) sc--; \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) sc--; \
        sc += curse_flag_count_rhf(AFF_FLAG); \
        if (PEN_FLAG) sc -= curse_flag_count_rhf(PEN_FLAG); \
        if (sc > 2) sc = 2; \
        if (sc < -2) sc = -2; \
        if (sc == 2) PUSH_TRAIT(LABEL "++", col_mastery); \
        else if (sc == 1) PUSH_TRAIT(LABEL "+", col_affinity); \
        else if (sc == -1) PUSH_TRAIT(LABEL "-", col_penalty); \
        else if (sc == -2) PUSH_TRAIT(LABEL "--", col_gr_penalty); \
    } while (0)

#define CHECK_UNIQUE(LABEL, FLAG, COLOR) \
    do { \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG))) \
            PUSH_TRAIT((LABEL), (COLOR)); \
    } while (0)

#define CHECK_UNIQUE_U(LABEL, FLAG, COLOR) \
    do { \
        if (c_info[character].flags_u & (FLAG)) \
            PUSH_TRAIT((LABEL), (COLOR)); \
    } while (0)

    CHECK_SKILL("melee", RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    CHECK_SKILL("evasion", RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    CHECK_SKILL("stealth", RHF_STL_AFFINITY, RHF_STL_PENALTY);
    CHECK_SKILL("archery", RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    CHECK_SKILL("will", RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    CHECK_SKILL("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    CHECK_SKILL("smithing", RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    CHECK_SKILL("song", RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    CHECK_SKILL("bow", RHF_BOW_PROFICIENCY, 0);
    CHECK_SKILL("axe", RHF_AXE_PROFICIENCY, 0);

    CHECK_UNIQUE_U("Master Artisan", UNQ_SMT_FEANOR, TERM_VIOLET);
    CHECK_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL, TERM_VIOLET);
    CHECK_UNIQUE_U("Chosen of Ulmo", UNQ_WIL_TUOR, TERM_VIOLET);
    CHECK_UNIQUE_U("Indomitable Will", UNQ_EARENDIL, TERM_VIOLET);
    CHECK_UNIQUE_U("Orome Himself", UNQ_WIL_FIN, TERM_VIOLET);
    CHECK_UNIQUE_U("Songs of Power", UNQ_SNG_FIN, TERM_VIOLET);
    CHECK_UNIQUE_U("Elven Dance", UNQ_SNG_LUT, TERM_VIOLET);
    CHECK_UNIQUE_U("Girdle of Melian", UNQ_SNG_MEL, TERM_VIOLET);
    CHECK_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR, TERM_VIOLET);
    CHECK_UNIQUE_U("Old Master", UNQ_SMT_GAMIL, TERM_VIOLET);
    CHECK_UNIQUE_U("Ring Master", UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    CHECK_UNIQUE_U("Aure entuluva", UNQ_SNG_HURIN, TERM_VIOLET);
    CHECK_UNIQUE_U("Voice of the Girdle", UNQ_SNG_THINGOL, TERM_VIOLET);
    CHECK_UNIQUE_U("Forgotten", UNQ_MIM, TERM_VIOLET);
    CHECK_UNIQUE_U("One Handed", UNQ_MEL_MAEDHROS, TERM_VIOLET);
    CHECK_UNIQUE_U("Agarwaen", UNQ_WIL_TURIN, TERM_VIOLET);
    CHECK_UNIQUE_U("Shadow Walker", UNQ_SNG_TURGON, TERM_VIOLET);
    CHECK_UNIQUE_U("Minstrel", UNQ_MINSTREL, TERM_VIOLET);
    CHECK_UNIQUE_U("Woven Master", UNQ_WOVEN_MASTER, TERM_VIOLET);
    CHECK_UNIQUE("Gift of Eru", RHF_GIFTERU, TERM_VIOLET);
    CHECK_UNIQUE("Seafarer", RHF_FREE, TERM_VIOLET);

    CHECK_UNIQUE("Kinslayer", RHF_KINSLAYER, TERM_UMBER);
    CHECK_UNIQUE("Treacherous", RHF_TREACHERY, TERM_UMBER);
    CHECK_UNIQUE("Doom of Mandos", RHF_CURSE, TERM_UMBER);
    CHECK_UNIQUE("Morgoth Curse", RHF_MOR_CURSE, TERM_UMBER);

#undef PUSH_TRAIT
#undef CHECK_SKILL
#undef CHECK_UNIQUE
#undef CHECK_UNIQUE_U

    return n;
}

void display_character_tutorial(void)
{
    int page = 0;
    char ch;
    ui_information_scene_scope info_scope;

    if (!ui_information_scene_supported() || !ui_information_scene_enter(&info_scope))
    {
        log_warn("character tutorial: semantic scene unavailable on the snapshot renderer path");
        msg_print("Character tutorial unavailable.");
        return;
    }

    while (1)
    {
        int wid = 80;
        int hgt = 24;
        tutorial_layout_size(&wid, &hgt);

        bool steamdeck = steamdeck_controls_active();
        bool birth_context = (playerturn == 0);

        const int header_row = 0;
        const int content_top = 2;
        const int nav_row = hgt - 1;
        const int hint_row = hgt - 2;
        const int content_max_row = hint_row;

        int text_col = 2;
        int text_w = wid - text_col - 2;
        if (text_w < 20)
            text_w = 20;

        int content_rows = content_max_row - content_top;
        if (content_rows < 4)
            content_rows = 4;

        tutorial_trait_line traits[160];
        int trait_n = tutorial_collect_traits(traits, (int)N_ELEMENTS(traits));

        typedef struct { const char* key; const char* desc; } ctl_line;
        ctl_line controls[24];
        int ctl_n = 0;
        char ctl_bufs[24][24];
        char ctl_note_bufs[4][128];
        int ctl_note_n = 0;

        if (steamdeck)
        {
            char confirm_label[16];
            char use_label[16];
            char examine_label[16];
            char inven_label[16];
            char equip_label[16];
            char look_label[16];
            char char_label[16];
            char fire_label[16];
            char sing_label[16];
            char activate_label[16];
            char map_label[16];
            char bash_label[16];
            char abilities_label[16];
            char help_label[16];
            char menu_label[16];
            char shift_label[16];
            char ctrl_label[16];

            tutorial_prompt_label(' ', "A", confirm_label, sizeof(confirm_label));
            tutorial_prompt_label('u', "X", use_label, sizeof(use_label));
            tutorial_prompt_label('x', "RS Right", examine_label, sizeof(examine_label));
            tutorial_prompt_label('i', "R1", inven_label, sizeof(inven_label));
            tutorial_prompt_label('e', "L1", equip_label, sizeof(equip_label));
            tutorial_prompt_label('l', "L1+R1", look_label, sizeof(look_label));
            tutorial_prompt_label('h', "Back", char_label, sizeof(char_label));
            tutorial_prompt_label('f', "RS Down", fire_label, sizeof(fire_label));
            tutorial_prompt_label('s', "Y", sing_label, sizeof(sing_label));
            tutorial_prompt_label('a', "RS Left", activate_label, sizeof(activate_label));
            tutorial_prompt_label('M', "RS Up", map_label, sizeof(map_label));
            tutorial_prompt_label('b', "B", bash_label, sizeof(bash_label));
            tutorial_prompt_label('\t', "L5", abilities_label, sizeof(abilities_label));
            tutorial_prompt_label('?', "?", help_label, sizeof(help_label));
            tutorial_prompt_label('m', "Start", menu_label, sizeof(menu_label));
            tutorial_prompt_label(GAMEPAD_BIND_SHIFT, "L2", shift_label, sizeof(shift_label));
            tutorial_prompt_label(GAMEPAD_BIND_CTRL, "R2", ctrl_label, sizeof(ctrl_label));

            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], "D-pad/Stick", sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Move/attack" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], confirm_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Pick up" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], use_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Use item" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], examine_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Examine" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], inven_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Inventory" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], equip_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Equipment" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], look_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Look" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], char_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Character" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], fire_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Fire" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], sing_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Sing" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], activate_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Activate" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], map_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Map" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], bash_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Bash" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], abilities_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Abilities" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], help_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Help" };
                ctl_n++;
            }
            if (ctl_n < (int)N_ELEMENTS(controls))
            {
                SDL_strlcpy(ctl_bufs[ctl_n], menu_label, sizeof(ctl_bufs[ctl_n]));
                controls[ctl_n] = (ctl_line){ ctl_bufs[ctl_n], "Main menu" };
                ctl_n++;
            }
        }
        else
        {
            controls[ctl_n++] = (ctl_line){ "Numpad", "Move/attack" };
            controls[ctl_n++] = (ctl_line){ "Space", "Pick up" };
            controls[ctl_n++] = (ctl_line){ "u", "Use item" };
            controls[ctl_n++] = (ctl_line){ "x", "Examine" };
            controls[ctl_n++] = (ctl_line){ "i", "Inventory" };
            controls[ctl_n++] = (ctl_line){ "e", "Equipment" };
            controls[ctl_n++] = (ctl_line){ "l", "Look" };
            controls[ctl_n++] = (ctl_line){ "Ctrl+dir", "Bash/disarm/tunnel" };
            controls[ctl_n++] = (ctl_line){ "f/F", "Fire" };
            controls[ctl_n++] = (ctl_line){ "s/S", "Sing/Stealth" };
            controls[ctl_n++] = (ctl_line){ "a", "Activate" };
            controls[ctl_n++] = (ctl_line){ "c", "Close door" };
            controls[ctl_n++] = (ctl_line){ "h", "Character" };
            controls[ctl_n++] = (ctl_line){ "m", "Main menu" };
            controls[ctl_n++] = (ctl_line){ "Tab", "Abilities" };
            controls[ctl_n++] = (ctl_line){ "?", "Help" };
            controls[ctl_n++] = (ctl_line){ NULL, "Shortcuts can be changed in user prefs." };
        }

        if (steamdeck)
        {
            char shift_label[16];
            char ctrl_label[16];
            char sing_label[16];
            char fire_label[16];

            tutorial_prompt_label(GAMEPAD_BIND_SHIFT, "L2", shift_label, sizeof(shift_label));
            tutorial_prompt_label(GAMEPAD_BIND_CTRL, "R2", ctrl_label, sizeof(ctrl_label));
            tutorial_prompt_label('s', "Y", sing_label, sizeof(sing_label));
            tutorial_prompt_label('f', "RS Down", fire_label, sizeof(fire_label));

            if (ctl_n < (int)N_ELEMENTS(controls) && ctl_note_n < (int)N_ELEMENTS(ctl_note_bufs))
            {
                strnfmt(ctl_note_bufs[ctl_note_n], sizeof(ctl_note_bufs[ctl_note_n]),
                    "Shift: %s+%s=Stealth, %s+%s=2nd quiver",
                    shift_label, sing_label, shift_label, fire_label);
                controls[ctl_n++] = (ctl_line){ NULL, ctl_note_bufs[ctl_note_n++] };
            }
            if (ctl_n < (int)N_ELEMENTS(controls) && ctl_note_n < (int)N_ELEMENTS(ctl_note_bufs))
            {
                strnfmt(ctl_note_bufs[ctl_note_n], sizeof(ctl_note_bufs[ctl_note_n]),
                    "Ctrl: %s+dir = Bash/Disarm/Tunnel", ctrl_label);
                controls[ctl_n++] = (ctl_line){ NULL, ctl_note_bufs[ctl_note_n++] };
            }
        }

        int list_rows = content_max_row - (content_top + 2);
        if (list_rows < 1)
            list_rows = 1;

        bool two_col = (wid >= 74);
        int trait_items_per_page = list_rows * (two_col ? 2 : 1);
        if (trait_items_per_page < 1)
            trait_items_per_page = 1;
        int trait_pages = (trait_n <= 0)
            ? 1
            : (trait_n + trait_items_per_page - 1) / trait_items_per_page;

        int ctl_items_per_page = list_rows * (two_col ? 2 : 1);
        if (ctl_items_per_page < 1)
            ctl_items_per_page = 1;
        int ctl_pages = (ctl_n + ctl_items_per_page - 1) / ctl_items_per_page;
        if (ctl_pages < 1)
            ctl_pages = 1;

        const char* skills_intro =
            "Total = Base + stat + equip + misc. Base affects ability purchase cost.";
        const char* skill_desc[S_MAX] = {
            "Melee chance to hit",
            "Ranged chance to hit",
            "Evade attacks",
            "Avoid detection",
            "Notice hidden",
            "Mental resistance",
            "Craft items",
            "Song power",
            ""
        };

        int skill_order[S_MAX];
        int skill_count = 0;
        for (int s = 0; s < S_MAX; s++)
        {
            if (s == S_SPC)
                continue;
            skill_order[skill_count++] = s;
        }

        int intro_lines = tutorial_count_wrapped_lines_ex(skills_intro, text_w, false);
        int skills_cap_first = content_rows - (2 + intro_lines + 1);
        int skills_cap_next = content_rows - 2;
        if (skills_cap_first < 1)
            skills_cap_first = 1;
        if (skills_cap_next < 1)
            skills_cap_next = 1;

        int skill_page_starts[32];
        int skill_page_ends[32];
        int skill_pages = 0;
        {
            int cur = 0;
            while (cur < skill_count && skill_pages < (int)N_ELEMENTS(skill_page_starts))
            {
                int cap = (skill_pages == 0) ? skills_cap_first : skills_cap_next;
                int used = 0;
                int start = cur;

                while (cur < skill_count)
                {
                    int sid = skill_order[cur];
                    int need = 1 + tutorial_count_wrapped_lines_ex(skill_desc[sid], text_w, false);
                    if (need < 1)
                        need = 1;
                    if (used + need > cap && used > 0)
                        break;
                    used += need;
                    cur++;
                }

                if (cur == start)
                    cur++;

                skill_page_starts[skill_pages] = start;
                skill_page_ends[skill_pages] = cur;
                skill_pages++;
            }
        }
        if (skill_pages < 1)
        {
            skill_pages = 1;
            skill_page_starts[0] = 0;
            skill_page_ends[0] = 0;
        }

        int history_cap = content_rows - 2;
        if (history_cap < 1)
            history_cap = 1;
        int history_lines = tutorial_count_wrapped_lines_ex(p_ptr->history, text_w, true);
        if (history_lines < 1)
            history_lines = 1;
        int history_pages = (history_lines + history_cap - 1) / history_cap;
        if (history_pages < 1)
            history_pages = 1;

        char birth_text[1200];
        int birth_pages = 0;
        if (birth_context)
        {
            size_t off = 0;
            off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                "On smaller displays the character creation screens use a compact layout. All information is still available; it is just rearranged.\n\n");
            off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                "Character selection: use Up/Down to pick, and read the description/traits for the highlighted choice. On short screens, traits may be shown in a tighter, compact list.\n\n");
            off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                "Stats allocation: select a stat, then adjust it. The display may reuse the compact Stats+Skills sheet with the current stat highlighted.\n\n");
            off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                "Skills allocation works the same way (select a skill, then adjust).\n\n");

            if (steamdeck)
            {
                char next_label[16];
                char back_label[16];
                tutorial_prompt_label(steamdeck_confirm_key(), "A", next_label, sizeof(next_label));
                tutorial_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
                off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                    "Navigation: D-pad Up/Down select, Left/Right adjust  [%s] confirm  [%s] back", next_label, back_label);
            }
            else
            {
                off += (size_t)strnfmt(birth_text + off, sizeof(birth_text) - off,
                    "Navigation: Up/Down selects; Left/Right adjusts; Enter/Space confirms; ESC goes back.");
            }

            int birth_cap = content_rows - 2;
            if (birth_cap < 1)
                birth_cap = 1;
            int birth_lines = tutorial_count_wrapped_lines_ex(birth_text, text_w, true);
            if (birth_lines < 1)
                birth_lines = 1;
            birth_pages = (birth_lines + birth_cap - 1) / birth_cap;
            if (birth_pages < 1)
                birth_pages = 1;
        }

        const int page_core_1 = 0;
        const int page_core_2 = 1;
        const int page_attrs = 2;
        const int page_skills_start = 3;
        const int page_traits_legend = page_skills_start + skill_pages;
        const int page_traits_list_start = page_traits_legend + 1;
        const int page_history_start = page_traits_list_start + trait_pages;
        const int page_controls_start = page_history_start + history_pages;
        const int page_birth_start = page_controls_start + ctl_pages;
        const int total_pages = page_birth_start + (birth_context ? birth_pages : 0);

        if (page < 0)
            page = 0;
        if (page >= total_pages)
            page = total_pages - 1;

        app_ui_scene ui_scene;
        tutorial_render_target target;

        target.width = wid;
        target.height = hgt;
        {
            app_ui_panel* panel = NULL;

            if (!tutorial_begin_document_ui_scene(&ui_scene, &panel))
            {
                ui_information_scene_leave(&info_scope);
                log_warn("character tutorial: failed to build semantic tutorial scene");
                msg_print("Character tutorial unavailable.");
                return;
            }

            target.ui_scene = &ui_scene;
            target.ui_panel = panel;
        }

        {
            char title[96];
            strnfmt(title, sizeof(title), "TUTORIAL  %d/%d", page + 1, total_pages);
            tutorial_put_centered(&target, header_row, TERM_L_BLUE, title);
        }

        int row = content_top;

        if (page == page_core_1)
        {
            tutorial_render_text(&target, 2, row++, TERM_WHITE,
                "CORE STATISTICS (1/2)");
            row++;
            {
                char buf[128];
                strnfmt(buf, sizeof(buf), "Exp: %ld/%ld", (long)p_ptr->new_exp, (long)p_ptr->exp);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "Awarded for depth progress, identifying items, spotting and killing monsters.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                long cur_wgt = p_ptr->total_weight / 10;
                long max_wgt = weight_limit() / 10;
                strnfmt(buf, sizeof(buf), "Burden: %ld/%ld lbs", cur_wgt, max_wgt);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "Weight carried / maximum capacity.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                if (turn > 0)
                {
                    long cur_d = p_ptr->depth * 50;
                    long min_d = min_depth() * 50;
                    strnfmt(buf, sizeof(buf), "Depth: %ld/%ld", cur_d, min_d);
                    tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                    row += tutorial_put_wrapped_limited(&target,
                        "Current depth / minimum return depth (rises over time).",
                        text_col, row, text_w, content_max_row, TERM_SLATE);
                }

                comma_number(buf, playerturn);
                {
                    char line[128];
                    strnfmt(line, sizeof(line), "Turn: %s", buf);
                    tutorial_render_text(&target, 2, row++, TERM_L_GREEN, line);
                }
                row += tutorial_put_wrapped_limited(&target,
                    "Total game turns elapsed.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                {
                    char line[64];
                    strnfmt(line, sizeof(line), "Light: %d", p_ptr->cur_light);
                    tutorial_render_text(&target, 2, row++, TERM_L_GREEN, line);
                }
                row += tutorial_put_wrapped_limited(&target,
                    "Current light radius.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);
            }
        }
        else if (page == page_core_2)
        {
            tutorial_render_text(&target, 2, row++, TERM_WHITE,
                "CORE STATISTICS (2/2)");
            row++;
            {
                char buf[128];
                strnfmt(buf, sizeof(buf), "Melee: (%+d,%dd%d)", p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "Main hand: (chance to hit, damage dice).",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                strnfmt(buf, sizeof(buf), "Bows:  (%+d,%dd%d)", p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "Ranged: (chance to hit, damage dice).",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                strnfmt(buf, sizeof(buf), "Armor: [%+d,%d-%d]", p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, buf);
                row += tutorial_put_wrapped_limited(&target,
                    "[evasion, protection] = hit-avoid chance and damage absorption.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                {
                    int cur_hp = MIN(p_ptr->chp, 999);
                    int max_hp = MIN(p_ptr->mhp, 999);
                    char line[64];
                    byte col = (p_ptr->chp >= p_ptr->mhp) ? TERM_L_GREEN : (p_ptr->chp > p_ptr->mhp / 4) ? TERM_YELLOW : TERM_RED;
                    strnfmt(line, sizeof(line), "Health: %d/%d", cur_hp, max_hp);
                    tutorial_render_text(&target, 2, row++, col, line);
                }
                row += tutorial_put_wrapped_limited(&target,
                    "Hit points: current / maximum.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);

                {
                    int cur_sp = MIN(p_ptr->csp, 999);
                    int max_sp = MIN(p_ptr->msp, 999);
                    char line[64];
                    byte col = (p_ptr->csp >= p_ptr->msp) ? TERM_L_GREEN : (p_ptr->csp > p_ptr->msp / 4) ? TERM_YELLOW : TERM_RED;
                    strnfmt(line, sizeof(line), "Voice:  %d/%d", cur_sp, max_sp);
                    tutorial_render_text(&target, 2, row++, col, line);
                }
                row += tutorial_put_wrapped_limited(&target,
                    "Song points: current / maximum.",
                    text_col, row, text_w, content_max_row, TERM_SLATE);
            }
        }
        else if (page == page_attrs)
        {
            tutorial_render_text(&target, 2, row++, TERM_WHITE, "ATTRIBUTES");
            row++;
            row += tutorial_put_wrapped_limited(&target,
                "Current = Base + equip + misc - drain. Green = boosted, orange = reduced.",
                text_col, row, text_w, content_max_row, TERM_SLATE);
            row++;

            for (int stat = 0; stat < A_MAX && row < content_max_row; stat++)
            {
                char line[96];
                byte a = TERM_WHITE;
                int use = p_ptr->stat_use[stat];
                int base = p_ptr->stat_base[stat];
                if (use > base) a = TERM_L_GREEN;
                else if (use < base) a = TERM_ORANGE;
                strnfmt(line, sizeof(line), "%s: %d", stat_names[stat], use);
                tutorial_render_text(&target, 2, row++, a, line);

                const char* desc = NULL;
                if (stat == A_STR) desc = "Strength: melee dice and weight capacity.";
                else if (stat == A_DEX) desc = "Dexterity: melee/evasion/archery/stealth.";
                else if (stat == A_CON) desc = "Constitution: hit points.";
                else if (stat == A_GRA) desc = "Grace: will/perception/song/smithing and voice.";
                row += tutorial_put_wrapped_limited(&target, desc, text_col,
                    row, text_w, content_max_row, TERM_SLATE);
            }
        }
        else if (page >= page_skills_start && page < page_skills_start + skill_pages)
        {
            int sub = page - page_skills_start;
            char heading[64];
            if (skill_pages > 1)
                strnfmt(heading, sizeof(heading), "SKILLS (%d/%d)", sub + 1, skill_pages);
            else
                SDL_strlcpy(heading, "SKILLS", sizeof(heading));

            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            if (sub == 0)
            {
                row += tutorial_put_wrapped_limited(&target,
                    skills_intro,
                    text_col, row, text_w, content_max_row, TERM_SLATE);
                row++;
            }

            int start_idx = skill_page_starts[sub];
            int end_idx = skill_page_ends[sub];
            for (int i = start_idx; i < end_idx && row < content_max_row; i++)
            {
                int sid = skill_order[i];
                char line[128];
                strnfmt(line, sizeof(line), "%s: %d (base %d)",
                    skill_names_full[sid], p_ptr->skill_use[sid], p_ptr->skill_base[sid]);
                tutorial_render_text(&target, 2, row++, TERM_L_GREEN, line);
                row += tutorial_put_wrapped_limited(&target, skill_desc[sid],
                    text_col, row, text_w, content_max_row, TERM_SLATE);
            }
        }
        else if (page == page_traits_legend)
        {
            tutorial_render_text(&target, 2, row++, TERM_WHITE,
                "TRAITS LEGEND");
            row++;

            tutorial_render_text(&target, 2, row, TERM_L_GREEN, "++");
            tutorial_render_text(&target, 6, row++, TERM_SLATE, "Mastery");
            tutorial_render_text(&target, 2, row, TERM_GREEN, "+");
            tutorial_render_text(&target, 6, row++, TERM_SLATE, "Affinity");
            tutorial_render_text(&target, 2, row, TERM_RED, "--");
            tutorial_render_text(&target, 6, row++, TERM_SLATE,
                "Major penalty");
            tutorial_render_text(&target, 2, row, TERM_L_RED, "-");
            tutorial_render_text(&target, 6, row++, TERM_SLATE,
                "Minor penalty");
            tutorial_render_text(&target, 2, row, TERM_VIOLET, "UNIQUE");
            tutorial_render_text(&target, 10, row++, TERM_SLATE,
                "Special ability");
            tutorial_render_text(&target, 2, row, TERM_UMBER, "CURSE");
            tutorial_render_text(&target, 10, row++, TERM_SLATE,
                "Character curse");
            row++;

            row += tutorial_put_wrapped_limited(&target,
                "Next page shows your current traits.",
                text_col, row, text_w, content_max_row, TERM_SLATE);
        }
        else if (page >= page_traits_list_start && page < page_traits_list_start + trait_pages)
        {
            int sub = page - page_traits_list_start;
            char heading[64];
            strnfmt(heading, sizeof(heading), "YOUR TRAITS (%d/%d)", sub + 1, trait_pages);
            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            if (trait_n <= 0)
            {
                tutorial_render_text(&target, 2, row++, TERM_SLATE,
                    "(No special traits)");
            }
            else
            {
                int start = sub * trait_items_per_page;
                int end = start + trait_items_per_page;
                if (start < 0) start = 0;
                if (end > trait_n) end = trait_n;

                int col1 = 2;
                int gap = 2;
                int colw = wid - 4;
                int col2 = 2;
                if (two_col)
                {
                    colw = (wid - 4 - gap) / 2;
                    if (colw < 18)
                        colw = 18;
                    col2 = col1 + colw + gap;
                }

                int idx = start;
                for (int r = 0; r < list_rows && row < content_max_row; r++)
                {
                    if (idx >= end)
                        break;
                    tutorial_put_trunc(&target, col1, row, colw, traits[idx].col,
                        traits[idx].txt);
                    idx++;
                    if (two_col && idx < end)
                    {
                        tutorial_put_trunc(&target, col2, row, colw,
                            traits[idx].col, traits[idx].txt);
                        idx++;
                    }
                    row++;
                }
            }
        }
        else if (page >= page_history_start && page < page_history_start + history_pages)
        {
            int sub = page - page_history_start;
            char heading[64];
            if (history_pages > 1)
                strnfmt(heading, sizeof(heading), "HISTORY (%d/%d)", sub + 1, history_pages);
            else
                SDL_strlcpy(heading, "HISTORY", sizeof(heading));

            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            if (p_ptr->history[0])
            {
                int skip = sub * history_cap;
                tutorial_put_wrapped_slice_ex(&target,
                    p_ptr->history,
                    text_col,
                    row,
                    text_w,
                    content_max_row,
                    TERM_WHITE,
                    skip,
                    history_cap,
                    true);
            }
            else
            {
                tutorial_render_text(&target, 2, row++, TERM_SLATE,
                    "(No history)");
            }
        }
        else if (page >= page_controls_start && page < page_controls_start + ctl_pages)
        {
            int sub = page - page_controls_start;
            char heading[64];
            strnfmt(heading, sizeof(heading), "ESSENTIAL CONTROLS (%d/%d)", sub + 1, ctl_pages);
            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            int start = sub * ctl_items_per_page;
            int end = start + ctl_items_per_page;
            if (start < 0) start = 0;
            if (end > ctl_n) end = ctl_n;

            int col1 = 2;
            int gap = 2;
            int colw = wid - 4;
            int col2 = 2;
            if (two_col)
            {
                colw = (wid - 4 - gap) / 2;
                if (colw < 20)
                    colw = 20;
                col2 = col1 + colw + gap;
            }

            int idx = start;
            for (int r = 0; r < list_rows && row < content_max_row; r++)
            {
                if (idx >= end)
                    break;
                {
                    char line[128];
                    if (controls[idx].key)
                        strnfmt(line, sizeof(line), "%s - %s", controls[idx].key, controls[idx].desc);
                    else
                        SDL_strlcpy(line, controls[idx].desc, sizeof(line));
                    tutorial_put_trunc(&target, col1, row, colw, TERM_SLATE,
                        line);
                }
                idx++;

                if (two_col && idx < end)
                {
                    char line[128];
                    if (controls[idx].key)
                        strnfmt(line, sizeof(line), "%s - %s", controls[idx].key, controls[idx].desc);
                    else
                        SDL_strlcpy(line, controls[idx].desc, sizeof(line));
                    tutorial_put_trunc(&target, col2, row, colw, TERM_SLATE,
                        line);
                    idx++;
                }
                row++;
            }
        }
        else if (birth_context && page >= page_birth_start && page < page_birth_start + birth_pages)
        {
            int sub = page - page_birth_start;
            char heading[96];
            if (birth_pages > 1)
                strnfmt(heading, sizeof(heading), "CHARACTER CREATION (COMPACT) (%d/%d)", sub + 1, birth_pages);
            else
                SDL_strlcpy(heading, "CHARACTER CREATION (COMPACT SCREENS)", sizeof(heading));

            tutorial_render_text(&target, 2, row++, TERM_WHITE, heading);
            row++;

            {
                int birth_cap = content_rows - 2;
                int skip = sub * birth_cap;
                tutorial_put_wrapped_slice_ex(&target,
                    birth_text,
                    text_col,
                    row,
                    text_w,
                    content_max_row,
                    TERM_SLATE,
                    skip,
                    birth_cap,
                    true);
            }
        }

        {
            if (page == total_pages - 1)
                tutorial_put_centered(&target, hint_row, TERM_L_GREEN,
                    "Tutorial complete!");
            else
                tutorial_put_centered(&target, hint_row, TERM_YELLOW,
                    steamdeck ? "D-pad left/right to navigate" : "Use left/right (or any key) to navigate");

            if (steamdeck)
            {
                char next_label[16];
                char back_label[16];
                tutorial_prompt_label(steamdeck_confirm_key(), "A", next_label, sizeof(next_label));
                tutorial_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));

                char nav[160];
                if (page > 0)
                    strnfmt(nav, sizeof(nav), "(D-Left Prev)   (%s Next)   (%s Exit)", next_label, back_label);
                else
                    strnfmt(nav, sizeof(nav), "(%s Next)   (%s Exit)", next_label, back_label);
                tutorial_put_centered(&target, nav_row, TERM_SLATE, nav);
            }
            else
            {
                char nav[160];
                if (page > 0)
                    strnfmt(nav, sizeof(nav), "(4/<- Prev)   (6/-> Next)   (ESC Exit)");
                else
                    strnfmt(nav, sizeof(nav), "(6/-> Next)   (ESC Exit)");
                tutorial_put_centered(&target, nav_row, TERM_SLATE, nav);
            }
        }

        if (!ui_information_scene_present_ui(&ui_scene))
        {
            ui_information_scene_leave(&info_scope);
            log_warn("character tutorial: failed to present semantic tutorial scene");
            msg_print("Character tutorial unavailable.");
            return;
        }

        ch = (char)ui_information_scene_wait_key_nonrepeat();
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;

        if (ch == ESCAPE)
            break;
        else if (ch == '4')
        {
            if (page > 0)
                page--;
        }
        else if (ch == '6' || ch == ' ' || ch == '\r' || ch == '\n'
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            if (page < total_pages - 1)
                page++;
            else
                break;
        }
        else
        {
            if (page < total_pages - 1)
                page++;
            else
                break;
        }
    }

    ui_information_scene_leave(&info_scope);
}

/*
 * Special display, part 2a
 */
static void display_player_misc_info(void)
{
    char name[40];
    int wid = 80;
    int hgt = 24;
    int col = 20;

    if (story_character_enabled()) {
        sdl_story_font_enable();
    }

    if (p_ptr->oaths_broken) {
        strnfmt(name, sizeof(name), "%s the Oathbreaker", op_ptr->full_name);
    } else {
        strnfmt(name, sizeof(name), "%s%s", op_ptr->full_name,
            c_name + current_character_profile->alt_name);
    }

    display_player_get_layout_size(&wid, &hgt);
    if (wid > 0)
    {
        int name_len = (int)strlen(name);
        if (name_len < wid)
            col = (wid - name_len) / 2;
        if (col < 0)
            col = 0;
    }

    if (p_ptr->oaths_broken)
        c_put_str(TERM_RED, name, 0, col);
    else
        c_put_str(TERM_L_BLUE, name, 0, col);

    if (story_character_enabled()) {
        sdl_story_font_disable();
    }
}

static int display_player_compact_summary_block(int row_start)
{
    int wid = 80;
    int hgt = 24;
    bool tight_spacing = display_player_compact_tight_spacing();
    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    int row_l = row_start;
    int row_r = row_start;
    int col_l = 1;
    int col_r = 1;
    char cur[32], rhs[32], val[64], buf[160];

    const bool two_col = (wid >= 50);

    if (row_l < 0)
        row_l = 0;
    if (row_r < 0)
        row_r = 0;

    if (!two_col)
    {
        int row = row_start;
        const int col = 1;

        {
            int chp = p_ptr->chp; if (chp > 999) chp = 999;
            int mhp = p_ptr->mhp; if (mhp > 999) mhp = 999;
            strnfmt(cur, sizeof(cur), "%d", chp);
            strnfmt(rhs, sizeof(rhs), "%d", mhp);
            put_pair20_right(col, row++,
                     "Health",
                     cur, 3, TERM_L_BLUE,
                     '/', rhs, 3, TERM_L_BLUE);
        }

        {
            int csp = p_ptr->csp; if (csp > 999) csp = 999;
            int msp = p_ptr->msp; if (msp > 999) msp = 999;
            strnfmt(cur, sizeof(cur), "%d", csp);
            strnfmt(rhs, sizeof(rhs), "%d", msp);
            put_pair20_right(col, row++,
                     "Voice",
                     cur, 3, TERM_L_BLUE,
                     '/', rhs, 3, TERM_L_BLUE);
        }

        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
        put_single20_right(col, row++,
                   "Melee", val, 12, TERM_L_BLUE);

        if (p_ptr->mds2 > 0)
        {
            strnfmt(val, sizeof(val), "(%+d,%dd%d)",
                p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
                p_ptr->mdd2, p_ptr->mds2);
            put_single20_right(col, row++,
                       "Offhand", val, 12, TERM_L_BLUE);
        }

        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
        put_single20_right(col, row++,
                   "Bows", val, 12, TERM_L_BLUE);

        strnfmt(val, sizeof(val), "[%+d,%d-%d]",
            p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
        put_single20_right(col, row++,
                   "Armor", val, 12, TERM_L_BLUE);

        strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);
        strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);
        put_pair20_right(col, row++,
                 "Exp",
                 cur, 5, TERM_L_GREEN,
                 '/', rhs, 6, TERM_L_GREEN);

        {
            long cur_b = (long)(p_ptr->total_weight / 10L);
            long max_b = (long)(weight_limit() / 10L);
            strnfmt(cur, sizeof(cur), "%ld", cur_b);
            strnfmt(rhs, sizeof(rhs), "%ld", max_b);
            put_pair20_right(col, row++,
                     "Burden",
                     cur, 4, (cur_b <= max_b) ? TERM_L_GREEN : TERM_YELLOW,
                     '/', rhs, 4, TERM_L_GREEN);
        }

        if (turn > 0)
        {
            long cur_d = (long)(p_ptr->depth * 50);
            long min_d = (long)(min_depth() * 50);

            if (cur_d > 1000) cur_d = 1000;
            if (min_d > 1000) min_d = 1000;

            strnfmt(cur, sizeof(cur), "%ld", cur_d);
            strnfmt(rhs, sizeof(rhs), "%ld", min_d);
            put_pair20_right(col, row++,
                     "Depth c/m",
                     cur, 4, (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW,
                     '/', rhs, 4, TERM_L_GREEN);

            if (!tight_spacing
                && display_player_min_depth_progress_bar_line(col, row,
                MAX(1, wid - COMPACT_RIGHT_PAD - col)))
            {
                row++;
            }
        }

        display_player_deep_call_line(col, row++,
            MAX(1, wid - COMPACT_RIGHT_PAD - col));

        comma_number(buf, playerturn);
        put_single20_right(col, row++,
                   "Turn", buf, 12, TERM_L_GREEN);

        strnfmt(val, sizeof(val), "%d", p_ptr->cur_light);
        put_single20_right(col, row++,
                   "Light", val, 2, TERM_L_GREEN);

        if (p_ptr->song1 != SNG_NOTHING) {
            strnfmt(val, sizeof(val), "%s",
                b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name);
            put_single20_right(col, row++,
                       "Song", val, 14, TERM_L_BLUE);
        }
        if (p_ptr->song2 != SNG_NOTHING) {
            strnfmt(val, sizeof(val), "%s",
                b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name);
            put_single20_right(col, row++,
                       "Song", val, 14, TERM_L_BLUE);
        }

        return row + 1;
    }

    col_r = compact_right_column_start(wid);

    {
        int chp = p_ptr->chp; if (chp > 999) chp = 999;
        int mhp = p_ptr->mhp; if (mhp > 999) mhp = 999;
        strnfmt(cur, sizeof(cur), "%d", chp);
        strnfmt(rhs, sizeof(rhs), "%d", mhp);
        put_pair20_right(col_l, row_l++,
                         "Health",
                         cur, 3, TERM_L_BLUE,
                         '/', rhs, 3, TERM_L_BLUE);
    }

    {
        int csp = p_ptr->csp; if (csp > 999) csp = 999;
        int msp = p_ptr->msp; if (msp > 999) msp = 999;
        strnfmt(cur, sizeof(cur), "%d", csp);
        strnfmt(rhs, sizeof(rhs), "%d", msp);
        put_pair20_right(col_l, row_l++,
                         "Voice",
                         cur, 3, TERM_L_BLUE,
                         '/', rhs, 3, TERM_L_BLUE);
    }

    strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL], p_ptr->mdd, p_ptr->mds);
    put_single20_right(col_l, row_l++,
                       "Melee", val, 12, TERM_L_BLUE);

    if (p_ptr->mds2 > 0)
    {
        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
                p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
                p_ptr->mdd2, p_ptr->mds2);
        put_single20_right(col_l, row_l++,
                           "Offhand", val, 12, TERM_L_BLUE);
    }

    strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_ARC], p_ptr->add, p_ptr->ads);
    put_single20_right(col_l, row_l++,
                       "Bows", val, 12, TERM_L_BLUE);

    strnfmt(val, sizeof(val), "[%+d,%d-%d]",
            p_ptr->skill_use[S_EVN], p_min(GF_HURT, true), p_max(GF_HURT, true));
    put_single20_right(col_l, row_l++,
                       "Armor", val, 12, TERM_L_BLUE);

    strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);
    strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);
    put_pair20_right(col_r, row_r++,
                     "Exp",
                     cur, 5, TERM_L_GREEN,
                     '/', rhs, 6, TERM_L_GREEN);

    {
        long cur_b = (long)(p_ptr->total_weight / 10L);
        long max_b = (long)(weight_limit() / 10L);
        strnfmt(cur, sizeof(cur), "%ld", cur_b);
        strnfmt(rhs, sizeof(rhs), "%ld", max_b);
        put_pair20_right(col_r, row_r++,
                         "Burden",
                         cur, 4, (cur_b <= max_b) ? TERM_L_GREEN : TERM_YELLOW,
                         '/', rhs, 4, TERM_L_GREEN);
    }

    if (turn > 0)
    {
        long cur_d = (long)(p_ptr->depth * 50);
        long min_d = (long)(min_depth() * 50);

        if (cur_d > 1000) cur_d = 1000;
        if (min_d > 1000) min_d = 1000;

        strnfmt(cur, sizeof(cur), "%ld", cur_d);
        strnfmt(rhs, sizeof(rhs), "%ld", min_d);
        put_pair20_right(col_r, row_r++,
                         "Depth c/m",
                         cur, 4, (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW,
                         '/', rhs, 4, TERM_L_GREEN);

        if (!tight_spacing
            && display_player_min_depth_progress_bar_line(col_r, row_r, LINEW20))
            row_r++;
    }

    display_player_deep_call_line(col_r, row_r++, LINEW20);

    comma_number(buf, playerturn);
    put_single20_right(col_r, row_r++,
                       "Turn", buf, 12, TERM_L_GREEN);

    strnfmt(val, sizeof(val), "%d", p_ptr->cur_light);
    put_single20_right(col_r, row_r++,
                       "Light", val, 2, TERM_L_GREEN);

    {
        int row_song = (row_l > row_r) ? row_l : row_r;

        if (p_ptr->song1 != SNG_NOTHING) {
            strnfmt(val, sizeof(val), "%s",
                    b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name);
            put_single20_right(col_l, row_song++,
                               "Song", val, 14, TERM_L_BLUE);
        }
        if (p_ptr->song2 != SNG_NOTHING) {
            strnfmt(val, sizeof(val), "%s",
                    b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name);
            put_single20_right(col_l, row_song++,
                               "Song", val, 14, TERM_L_BLUE);
        }

        row_l = row_song;
        row_r = row_song;
    }

    return ((row_l > row_r) ? row_l : row_r)
        + (display_player_compact_tight_spacing() ? 0 : 1);
}

typedef struct {
    const char *txt;
    byte col;
} compact_trait_line;

static int collect_compact_trait_lines(compact_trait_line* out, int out_max)
{
    int race = p_ptr->prace;
    int character = p_ptr->pcharacter;
    int total = 0;

    byte attr_affinity   = TERM_GREEN;
    byte attr_mastery    = TERM_L_GREEN;
    byte attr_penalty    = TERM_RED;
    byte attr_gr_penalty = TERM_L_RED;

    compact_trait_line uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
    int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH(arr, n, text, color) do { (arr)[(n)].txt = (text); (arr)[(n)++].col = (color); } while (0)
#define HANDLE_SKILL_EX(LABEL, AFF_FLAG, PEN_FLAG)                                      \
    do {                                                                                \
        int score = 0;                                                                  \
        if (p_info[race].flags      & (AFF_FLAG)) score++;                              \
        if (c_info[character].flags & (AFF_FLAG)) score++;                              \
        if ((PEN_FLAG) && (p_info[race].flags      & (PEN_FLAG))) score--;              \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) score--;              \
        score += curse_flag_count_rhf(AFF_FLAG);                                        \
        if ((PEN_FLAG)) score -= curse_flag_count_rhf(PEN_FLAG);                        \
        if (score >  2) score =  2;                                                     \
        if (score < -2) score = -2;                                                     \
        if (score ==  2)      PUSH(ma_buf,  ma_n,  LABEL "++", attr_mastery);          \
        else if (score == 1)  PUSH(af_buf,  af_n,  LABEL "+ ", attr_affinity);         \
        else if (score == -1) PUSH(pen_buf, pen_n, LABEL "- ", attr_penalty);          \
        else if (score == -2) PUSH(pen_buf, pen_n, LABEL "--", attr_gr_penalty);       \
    } while (0)
#define HANDLE_UNIQUE(LABEL, FLAG, COLOR)                                               \
    do {                                                                                \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG)))        \
            PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                                   \
    } while (0)
#define HANDLE_UNIQUE_U(LABEL, FLAG, COLOR)                                             \
    do {                                                                                \
        if (c_info[character].flags_u & (FLAG))                                         \
            PUSH(uniq_buf, uniq_n, (LABEL), (COLOR));                                   \
    } while (0)
#define EMIT(arr, n)                                                                    \
    do {                                                                                \
        for (int _i = 0; _i < (n); ++_i) {                                              \
            if (out && total < out_max) out[total] = (arr)[_i];                        \
            total++;                                                                    \
        }                                                                               \
    } while (0)

    HANDLE_SKILL_EX("melee",      RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion",    RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth",    RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery",    RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will",       RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", RHF_PER_AFFINITY, RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing",   RHF_SMT_AFFINITY, RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song",       RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow",        RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe",        RHF_AXE_PROFICIENCY, 0);

    HANDLE_UNIQUE_U("Master Artisan",     UNQ_SMT_FEANOR,   TERM_VIOLET);
    HANDLE_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Chosen of Ulmo",     UNQ_WIL_TUOR,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Indomitable Will",   UNQ_EARENDIL,     TERM_VIOLET);
    HANDLE_UNIQUE_U("Orome Himself",      UNQ_WIL_FIN,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Songs of Power",     UNQ_SNG_FIN,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Elven Dance",        UNQ_SNG_LUT,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Girdle of Melian",   UNQ_SNG_MEL,      TERM_VIOLET);
    HANDLE_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR,  TERM_VIOLET);
    HANDLE_UNIQUE_U("Old Master",         UNQ_SMT_GAMIL,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Ring Master",        UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    HANDLE_UNIQUE_U("Aure entuluva",      UNQ_SNG_HURIN,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Voice of the Girdle",UNQ_SNG_THINGOL,  TERM_VIOLET);
    HANDLE_UNIQUE_U("Forgotten",          UNQ_MIM,          TERM_VIOLET);
    HANDLE_UNIQUE_U("One Handed",         UNQ_MEL_MAEDHROS, TERM_VIOLET);
    HANDLE_UNIQUE_U("Agarwaen",           UNQ_WIL_TURIN,    TERM_VIOLET);
    HANDLE_UNIQUE_U("Shadow Walker",      UNQ_SNG_TURGON,   TERM_VIOLET);
    HANDLE_UNIQUE("Gift of Eru",          RHF_GIFTERU,      TERM_VIOLET);
    HANDLE_UNIQUE("Seafarer",             RHF_FREE,         TERM_VIOLET);

    HANDLE_UNIQUE("Kinslayer",            RHF_KINSLAYER,    TERM_UMBER);
    HANDLE_UNIQUE("Treacherous",          RHF_TREACHERY,    TERM_UMBER);
    HANDLE_UNIQUE("Doom of Mandos",       RHF_CURSE,        TERM_UMBER);
    HANDLE_UNIQUE("Morgoth Curse",        RHF_MOR_CURSE,    TERM_UMBER);

    EMIT(uniq_buf, uniq_n);
    EMIT(ma_buf, ma_n);
    EMIT(af_buf, af_n);
    EMIT(pen_buf, pen_n);

#undef EMIT
#undef HANDLE_UNIQUE_U
#undef HANDLE_UNIQUE
#undef HANDLE_SKILL_EX
#undef PUSH

    return total;
}

static int display_player_compact_traits_block(int row_start, int col, int row_limit,
    int skip_lines)
{
    int row = row_start;
    compact_trait_line lines[96];
    int line_count = collect_compact_trait_lines(lines, 96);

    if (skip_lines < 0)
        skip_lines = 0;

    if (story_character_enabled())
        sdl_story_font_enable();

    for (int i = skip_lines; i < line_count && row < row_limit; ++i)
        c_put_str(lines[i].col, lines[i].txt, row++, col);

    if (story_character_enabled())
        sdl_story_font_disable();

    return row;
}

static int display_player_compact_trait_max_label_chars(void)
{
    compact_trait_line lines[96];
    int line_count = collect_compact_trait_lines(lines, 96);
    int max_chars = 0;

    for (int i = 0; i < line_count; ++i)
    {
        int len = (int)strlen(lines[i].txt ? lines[i].txt : "");
        if (len > max_chars)
            max_chars = len;
    }

    return max_chars;
}

static bool display_player_compact_can_embed_traits(int row_start)
{
    int wid = 80;
    int hgt = 24;
    int skills_count = 0;
    int attr_block_h = 1 + A_MAX;
    int skill_block_h;
    int trait_lines;
    int trait_block_h;
    int available_rows;
    int col_attr = 1;
    int attr_width = LINEW20;
    int attr_right_edge = col_attr + attr_width - 1;
    int col_skill;
    int col_traits;
    int trait_width;
    int trait_max_chars;

    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    for (int s = 0; s < S_MAX; ++s)
        if (s != S_SPC)
            skills_count++;

    skill_block_h = 1 + skills_count;
    trait_lines = collect_compact_trait_lines(NULL, 0);
    if (trait_lines <= 0)
        return false;

    trait_block_h = 1 + trait_lines;
    available_rows = hgt - 1 - row_start;
    if (available_rows < 1)
        return false;

    col_skill = compact_right_column_start(wid);
    col_traits = attr_right_edge + 2;
    trait_width = col_skill - col_traits - 2;
    trait_max_chars = display_player_compact_trait_max_label_chars();
    if (trait_max_chars < 6)
        trait_max_chars = 6;

    if (wid < 64)
        return false;
    if (trait_width < trait_max_chars)
        return false;
    if (available_rows < attr_block_h)
        return false;
    if (available_rows < skill_block_h)
        return false;
    if (available_rows < trait_block_h)
        return false;

    return true;
}

static int display_player_compact_history_line_count(int wrap_col, int indent)
{
    if (story_character_enabled())
        return count_wrapped_lines_story(p_ptr->history, wrap_col, indent);

    return count_wrapped_lines(p_ptr->history, wrap_col, indent);
}

static int display_player_compact_wrapped_offset(const char* text, int start_row,
    int col, int wrap_col, int row_limit, int skip_lines, byte attr)
{
    int wid = 80;
    int hgt = 24;
    int row = start_row;
    int max_width;
    int line_pos = 0;
    int line_idx = 0;
    const char* p = text;
    char line_buf[512];

    if (!text || !text[0])
        return start_row;

    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    if (wrap_col <= col)
        wrap_col = wid - COMPACT_RIGHT_PAD;

    max_width = wrap_col - col;
    if (max_width < 10)
        max_width = 10;

    if (skip_lines < 0)
        skip_lines = 0;

    while (*p)
    {
        while (*p == ' ' && line_pos == 0)
            p++;

        if (*p == '\n')
        {
            line_buf[line_pos] = '\0';
            if (line_pos > 0)
            {
                if (line_idx >= skip_lines && row < row_limit)
                    c_put_str(attr, line_buf, row++, col);
                line_idx++;
            }
            line_pos = 0;
            p++;
            continue;
        }

        if (line_pos >= max_width)
        {
            int wrap_pos = line_pos - 1;
            while (wrap_pos > 0 && line_buf[wrap_pos] != ' ')
                wrap_pos--;

            if (wrap_pos > 0)
            {
                line_buf[wrap_pos] = '\0';
                if (line_idx >= skip_lines && row < row_limit)
                    c_put_str(attr, line_buf, row++, col);

                int remaining = line_pos - wrap_pos - 1;
                for (int i = 0; i < remaining; i++)
                    line_buf[i] = line_buf[wrap_pos + 1 + i];
                line_pos = remaining;
            }
            else
            {
                line_buf[line_pos] = '\0';
                if (line_idx >= skip_lines && row < row_limit)
                    c_put_str(attr, line_buf, row++, col);
                line_pos = 0;
            }

            line_idx++;
            continue;
        }

        if (line_pos < (int)sizeof(line_buf) - 2)
            line_buf[line_pos++] = *p;
        p++;
    }

    if (line_pos > 0)
    {
        line_buf[line_pos] = '\0';
        if (line_idx >= skip_lines && row < row_limit)
            c_put_str(attr, line_buf, row++, col);
    }

    return row;
}

static void display_player_compact_history_column(int row_start, int col, int wrap_col,
    int skip_lines, int row_limit)
{
    int wid = 80;
    int hgt = 24;

    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    if (col < 0)
        col = 0;
    if (wrap_col <= col)
        wrap_col = wid - COMPACT_RIGHT_PAD;

    if (story_character_enabled())
        sdl_story_font_enable();

    (void)display_player_compact_wrapped_offset(p_ptr->history, row_start, col,
        wrap_col, row_limit, skip_lines, TERM_WHITE);

    if (story_character_enabled())
        sdl_story_font_disable();
}

static void display_player_compact_heading(cptr text, int row, int col);

static void display_player_compact_traits_middle_column(int row_start, int col,
    int max_cols, int row_limit)
{
    compact_trait_line lines[96];
    int line_count = collect_compact_trait_lines(lines, 96);
    int max_chars = 0;
    int draw_w;
    int start_col;
    int row = row_start;

    if (line_count <= 0 || max_cols < 6)
        return;

    for (int i = 0; i < line_count; ++i)
    {
        int len = (int)strlen(lines[i].txt ? lines[i].txt : "");
        if (len > max_chars)
            max_chars = len;
    }

    draw_w = max_chars;
    if (draw_w > max_cols)
        draw_w = max_cols;
    if (draw_w < 1)
        draw_w = 1;

    start_col = col + (max_cols - draw_w) / 2;
    display_player_compact_heading("Traits", row++, start_col);

    for (int i = 0; i < line_count && row < row_limit; ++i)
    {
        char line_buf[64];
        const char* src = lines[i].txt ? lines[i].txt : "";

        strnfmt(line_buf, sizeof(line_buf), "%.*s", draw_w, src);
        display_player_clear_cells(col, row, max_cols);
        c_put_str(lines[i].col, line_buf, row, start_col);
        row++;
    }
}

static void display_player_compact_heading(cptr text, int row, int col)
{
    bool use_story = story_character_enabled();

    if (use_story)
        sdl_story_font_enable();

    c_put_str(TERM_L_BLUE, text ? text : "", row, col);

    if (use_story)
        sdl_story_font_disable();
}

static void display_player_compact_description_and_flags(int row_start,
    int visible_row_start)
{
    int wid = 80;
    int hgt = 24;
    int scroll = display_player_compact_scroll;
    bool tight_spacing = display_player_compact_tight_spacing();
    bool traits_moved_to_stats_page = display_player_compact_can_embed_traits(
        visible_row_start);
    int content_row = row_start;

    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    int row_limit = hgt - 1;
    int available_rows = row_limit - visible_row_start;
    if (available_rows <= 0)
    {
        display_player_compact_max_scroll = 0;
        return;
    }

    if (traits_moved_to_stats_page)
    {
        int history_lines = display_player_compact_history_line_count(
            wid - COMPACT_RIGHT_PAD, 1);
        int content_height = history_lines;
        int max_scroll = history_lines - available_rows;
        if (max_scroll < 0)
            max_scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;

        if (!tight_spacing && (max_scroll == 0)
            && (content_height < available_rows))
            content_row += (available_rows - content_height) / 2;

        display_player_compact_max_scroll = max_scroll;
        display_player_compact_history_column(content_row, 1,
            wid - COMPACT_RIGHT_PAD, scroll, row_limit);
        return;
    }

    int trait_lines = collect_compact_trait_lines(NULL, 0);
    int history_lines_stacked = display_player_compact_history_line_count(wid - 1, 1);

    bool can_side_by_side = false;
    int trait_max_chars = display_player_compact_trait_max_label_chars();
    int side_history_col = 1 + trait_max_chars + 2;
    int history_lines_side = history_lines_stacked;

    if (side_history_col < 4)
        side_history_col = 4;

    if (wid >= 46)
    {
        int side_width = wid - side_history_col - COMPACT_RIGHT_PAD;
        if (side_width >= 18)
        {
            can_side_by_side = true;
            history_lines_side = display_player_compact_history_line_count(wid - COMPACT_RIGHT_PAD, side_history_col);
        }
    }

    int side_overflow = 1000000;
    if (can_side_by_side)
    {
        int traits_over = (trait_lines > available_rows) ? (trait_lines - available_rows) : 0;
        int history_over = (history_lines_side > available_rows) ? (history_lines_side - available_rows) : 0;
        side_overflow = traits_over + history_over;
    }

    int stacked_total = trait_lines
        + ((trait_lines > 0 && history_lines_stacked > 0) ? 1 : 0)
        + history_lines_stacked;
    int stacked_overflow = (stacked_total > available_rows)
        ? (stacked_total - available_rows)
        : 0;

    bool use_side_by_side = can_side_by_side && (side_overflow <= stacked_overflow);

    log_trace("Compact description+flags fit: wid=%d rows=%d traits=%d max_trait_chars=%d history_col=%d history_stack=%d history_side=%d side_overflow=%d stacked_overflow=%d use_side=%s",
              wid, available_rows, trait_lines, trait_max_chars, side_history_col,
              history_lines_stacked, history_lines_side,
              side_overflow, stacked_overflow, use_side_by_side ? "true" : "false");

    if (use_side_by_side)
    {
        int total_height = (trait_lines > history_lines_side) ? trait_lines : history_lines_side;
        int max_scroll = total_height - available_rows;
        if (max_scroll < 0)
            max_scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;

        if (!tight_spacing && (max_scroll == 0)
            && (total_height < available_rows))
            content_row += (available_rows - total_height) / 2;

        display_player_compact_max_scroll = max_scroll;

        display_player_compact_traits_block(content_row, 1, row_limit, scroll);
        display_player_compact_history_column(content_row, side_history_col,
            wid - COMPACT_RIGHT_PAD, scroll, row_limit);
        return;
    }

    {
        int max_scroll = stacked_total - available_rows;
        if (max_scroll < 0)
            max_scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;

        if (!tight_spacing && (max_scroll == 0)
            && (stacked_total < available_rows))
            content_row += (available_rows - stacked_total) / 2;

        display_player_compact_max_scroll = max_scroll;
    }

    int trait_skip = scroll;
    if (trait_skip > trait_lines)
        trait_skip = trait_lines;

    int row_after_flags = display_player_compact_traits_block(content_row, 1,
        row_limit, trait_skip);

    int history_skip = scroll - trait_lines;
    if (trait_lines > 0 && history_lines_stacked > 0)
        history_skip--;
    if (history_skip < 0)
        history_skip = 0;

    if (trait_lines > trait_skip && history_lines_stacked > 0
        && row_after_flags < row_limit && scroll < trait_lines + 1)
        row_after_flags += (display_player_compact_tight_spacing() ? 0 : 1);

    if (history_lines_stacked > 0 && row_after_flags < row_limit)
        display_player_compact_history_column(row_after_flags, 1,
            wid - COMPACT_RIGHT_PAD, history_skip, row_limit);
}

static void display_player_compact_attribute_line(int row, int col, int max_cols, int stat)
{
    if (max_cols < 10)
        return;

    if (stat < 0 || stat >= A_MAX)
        return;

    int use = p_ptr->stat_use[stat];
    int base = p_ptr->stat_base[stat];
    int mod = use - base;

    int val_w = 10;
    if (val_w > max_cols - 4)
        val_w = max_cols - 4;
    if (val_w < 5)
        val_w = 5;

    char val_buf[16];
    if (mod != 0 && val_w >= 10)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d%+d", use, base, mod);
    else if (mod != 0 && val_w >= 8)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d", use, base);
    else if (mod != 0)
        strnfmt(val_buf, sizeof(val_buf), "%2d", use);
    else if (val_w >= 8)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d", use, base);
    else
        strnfmt(val_buf, sizeof(val_buf), "%2d", use);

    int end_col = col + max_cols - 1;
    int val_start = end_col - val_w + 1;
    int label_w = val_start - col;
    if (label_w < 1)
        label_w = 1;

    display_player_clear_cells(col, row, label_w);
    display_player_clear_cells(val_start, row, val_w);

    const char* stat_label = (p_ptr->stat_drain[stat] < 0) ? stat_names_reduced[stat] : stat_names[stat];
    char label_buf[32];
    SDL_strlcpy(label_buf, stat_label ? stat_label : "", sizeof(label_buf));
    int len = (int)strlen(label_buf);
    while (len > 0 && label_buf[len - 1] == ' ')
        label_buf[--len] = '\0';
    if (len > label_w)
        label_buf[label_w] = '\0';

    if (story_character_enabled() && label_w > 0)
    {
        int cell_w = sdl_get_cell_width();
        int max_pixels = label_w * cell_w;
        size_t label_len = strlen(label_buf);

        while (label_len > 0 && sdl_story_font_text_width(label_buf, (int)label_len) > max_pixels)
        {
            label_buf[--label_len] = '\0';
            while (label_len > 0 && isspace((unsigned char)label_buf[label_len - 1]))
                label_buf[--label_len] = '\0';
        }
    }

    if (story_character_enabled())
        sdl_story_font_enable();

    c_put_str(TERM_WHITE, label_buf, row, col);

    if (story_character_enabled())
        sdl_story_font_disable();

    const char* val_text = val_buf;
    int val_len = (int)strlen(val_text);
    if (val_len > val_w)
    {
        val_buf[val_w] = '\0';
        val_text = val_buf;
        val_len = val_w;
    }

    int out_col = end_col - val_len + 1;
    if (out_col < val_start)
        out_col = val_start;

    byte stat_color = (p_ptr->stat_drain[stat] < 0) ? TERM_YELLOW : TERM_L_GREEN;
    c_put_str(stat_color, val_text, row, out_col);
}

static void display_player_compact_attributes(int row_start, int max_cols)
{
    int wid = 80;
    int hgt = 24;
    int row = row_start;
    int col = 1;

    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    display_player_compact_heading("Attributes", row++, col);

    if (max_cols <= 0)
        max_cols = wid - col - COMPACT_RIGHT_PAD;
    if (max_cols < 10)
        max_cols = 10;

    for (int stat = 0; stat < A_MAX && row < hgt - 1; ++stat)
    {
        display_player_compact_attribute_line(row++, col, max_cols, stat);
    }
}

static void display_player_compact_skills_list(int row_start);

static void display_player_compact_skill_line(int row, int col, int max_cols, int skill)
{
    if (max_cols < 10)
        return;

    if (skill < 0 || skill >= S_MAX || skill == S_SPC)
        return;

    int use = p_ptr->skill_use[skill];
    int base = p_ptr->skill_base[skill];
    int mod = use - base;

    int val_w = 10;
    if (val_w > max_cols - 4)
        val_w = max_cols - 4;
    if (val_w < 5)
        val_w = 5;

    char val_buf[16];
    if (mod != 0 && val_w >= 10)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d%+d", use, base, mod);
    else if (mod != 0 && val_w >= 8)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d", use, base);
    else if (mod != 0)
        strnfmt(val_buf, sizeof(val_buf), "%2d", use);
    else if (val_w >= 8)
        strnfmt(val_buf, sizeof(val_buf), "%2d=%2d", use, base);
    else
        strnfmt(val_buf, sizeof(val_buf), "%2d", use);

    int end_col = col + max_cols - 1;
    int val_start = end_col - val_w + 1;
    int label_w = val_start - col;
    if (label_w < 1)
        label_w = 1;

    display_player_clear_cells(col, row, label_w);
    display_player_clear_cells(val_start, row, val_w);

    const char* name = skill_names_full[skill];
    if (!name)
        name = "";

    char label_buf[64];
    strnfmt(label_buf, sizeof(label_buf), "%.*s", label_w, name);

    if (story_character_enabled() && label_w > 0)
    {
        int cell_w = sdl_get_cell_width();
        int max_pixels = label_w * cell_w;
        size_t len = strlen(label_buf);

        while (len > 0 && sdl_story_font_text_width(label_buf, (int)len) > max_pixels)
        {
            label_buf[--len] = '\0';
            while (len > 0 && isspace((unsigned char)label_buf[len - 1]))
                label_buf[--len] = '\0';
        }
    }

    if (story_character_enabled())
        sdl_story_font_enable();

    c_put_str(TERM_WHITE, label_buf, row, col);

    if (story_character_enabled())
        sdl_story_font_disable();

    const char* val_text = val_buf;
    int val_len = (int)strlen(val_text);
    if (val_len > val_w)
    {
        val_buf[val_w] = '\0';
        val_text = val_buf;
        val_len = val_w;
    }

    int out_col = end_col - val_len + 1;
    if (out_col < val_start)
        out_col = val_start;

    c_put_str(TERM_L_GREEN, val_text, row, out_col);
}

static void display_player_compact_attributes_and_skills(int row_start)
{
    int wid = 80;
    int hgt = 24;
    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    int skills_count = 0;
    for (int s = 0; s < S_MAX; ++s)
        if (s != S_SPC)
            skills_count++;

    int attr_block_h = 1 + A_MAX;
    int skill_block_h = 1 + skills_count;
    int block_h = (attr_block_h > skill_block_h) ? attr_block_h : skill_block_h;

    int col_attr = 1;
    int attr_width = LINEW20;
    int attr_right_edge = col_attr + attr_width - 1;
    int col_skill = compact_right_column_start(wid);
    bool embed_traits = display_player_compact_can_embed_traits(row_start);

    bool side_by_side = (wid >= 50)
        && (col_skill >= attr_right_edge + 2)
        && (row_start + block_h <= hgt - 1);

    if (embed_traits)
    {
        int col_traits = attr_right_edge + 2;
        int traits_width = col_skill - col_traits - 2;
        int row = row_start;

        display_player_compact_attributes(row_start, attr_width);
        display_player_compact_heading("Skills", row++, col_skill);

        for (int skill = 0; skill < S_MAX && row < hgt - 1; ++skill)
        {
            if (skill == S_SPC)
                continue;
            display_player_compact_skill_line(row++, col_skill, LINEW20, skill);
        }

        display_player_compact_traits_middle_column(row_start, col_traits,
            traits_width, hgt - 1);
        return;
    }

    display_player_compact_attributes(row_start, side_by_side ? attr_width : 0);

    if (side_by_side)
    {
        int row = row_start;
        display_player_compact_heading("Skills", row++, col_skill);

        for (int skill = 0; skill < S_MAX && row < hgt - 1; ++skill)
        {
            if (skill == S_SPC)
                continue;
            display_player_compact_skill_line(row++, col_skill, LINEW20, skill);
        }

        return;
    }

    int row_skills = row_start + 1 + A_MAX
        + (display_player_compact_tight_spacing() ? 0 : 1);
    if (row_skills < hgt - 1)
        display_player_compact_skills_list(row_skills);
}

static void display_player_compact_skills_list(int row_start)
{
    int wid = 80;
    int hgt = 24;
    int row = row_start;
    int col = 1;

    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    display_player_compact_heading("Skills", row++, col);

    int max_cols = wid - col - COMPACT_RIGHT_PAD;
    if (max_cols < 10)
        max_cols = 10;

    for (int skill = 0; skill < S_MAX && row < hgt - 1; skill++)
    {
        if (skill == S_SPC)
            continue;

        display_player_compact_skill_line(row++, col, max_cols, skill);
    }
}

static void display_player_compact_history(int row_start)
{
    int wid = 80;
    int hgt = 24;

    display_player_get_layout_size(&wid, &hgt);
    if (wid < 1) wid = 80;
    if (hgt < 1) hgt = 24;

    display_player_compact_history_column(row_start, 1, wid - COMPACT_RIGHT_PAD,
        0, hgt - 1);
}

static void display_player_compact_desc_flags_page(bool show_misc_info,
    bool show_summary)
{
    int summary_row = show_misc_info ? display_player_compact_start_row() : 0;
    int body_row = summary_row;

    if (show_misc_info)
        display_player_misc_info();

    if (show_summary)
        body_row = display_player_compact_summary_block(summary_row);

    display_player_compact_description_and_flags(body_row, body_row);
}

static void display_player_stat_info(int row, int col)
{
    int i;
    char buf[80];

    for (i = 0; i < A_MAX; i++)
    {
        const char* stat_label;
        char trimmed_label[32];

        if (p_ptr->stat_drain[i] < 0)
            stat_label = stat_names_reduced[i];
        else
            stat_label = stat_names[i];

        SDL_strlcpy(trimmed_label, stat_label, sizeof(trimmed_label));
        int len = (int)strlen(trimmed_label);
        while (len > 0 && trimmed_label[len - 1] == ' ')
            trimmed_label[--len] = '\0';

        if (story_character_enabled()) {
            sdl_story_font_enable();
        }

        put_str(trimmed_label, row + i, col);

        if (story_character_enabled()) {
            sdl_story_font_disable();
        }
    }

    for (i = 0; i < A_MAX; i++)
    {
        cnv_stat(p_ptr->stat_use[i], buf);

        if (p_ptr->stat_drain[i] < 0)
            c_put_str(TERM_YELLOW, buf, row + i, col + 5);
        else
            c_put_str(TERM_L_GREEN, buf, row + i, col + 5);

        if (p_ptr->stat_equip_mod[i] != 0)
        {
            c_put_str(TERM_SLATE, "=", row + i, col + 8);
            cnv_stat(p_ptr->stat_base[i], buf);
            c_put_str(TERM_GREEN, buf, row + i, col + 10);
            strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_equip_mod[i]);
            c_put_str(TERM_SLATE, buf, row + i, col + 13);
        }

        if (p_ptr->stat_drain[i] != 0)
        {
            c_put_str(TERM_SLATE, "=", row + i, col + 8);
            cnv_stat(p_ptr->stat_base[i], buf);
            c_put_str(TERM_GREEN, buf, row + i, col + 10);
            strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_drain[i]);
            c_put_str(TERM_SLATE, buf, row + i, col + 17);
        }

        if (p_ptr->stat_misc_mod[i] != 0)
        {
            c_put_str(TERM_SLATE, "=", row + i, col + 8);
            cnv_stat(p_ptr->stat_base[i], buf);
            c_put_str(TERM_GREEN, buf, row + i, col + 10);
            strnfmt(buf, sizeof(buf), "%+3d", p_ptr->stat_misc_mod[i]);
            c_put_str(TERM_SLATE, buf, row + i, col + 21);
        }
    }

    sdl_story_font_disable();
}

static void display_player_sust_info(void)
{
    int i, row, col, stats;

    object_type* o_ptr;
    u32b f1, f2, f3;
    u32b ignore_f2, ignore_f3;

    byte a;
    char c;

    sdl_story_font_enable();

    row = 2;
    col = 23;
    c_put_str(TERM_WHITE, "abcdefghijkl@", row - 1, col);

    for (i = INVEN_WIELD; i < INVEN_TOTAL; ++i)
    {
        o_ptr = &inventory[i];
        object_flags_known(o_ptr, &f1, &f2, &f3);
        object_flags(o_ptr, &f1, &ignore_f2, &ignore_f3);

        for (stats = 0; stats < A_MAX; stats++)
        {
            a = TERM_SLATE;
            c = '.';

            if (f1 & (1 << stats))
            {
                c = '*';
                if (o_ptr->pval == 0)
                {
                    c = '.';
                }
                if (o_ptr->pval > 0)
                {
                    a = TERM_L_GREEN;
                    if (o_ptr->pval < 10)
                        c = I2D(o_ptr->pval);
                }
                if (o_ptr->pval < 0)
                {
                    a = TERM_RED;
                    if (o_ptr->pval > -10)
                        c = I2D(-(o_ptr->pval));
                }
            }

            if (f1 & (1 << (stats + A_MAX)))
            {
                c = '*';
                if (o_ptr->pval == 0)
                {
                    c = '.';
                }
                if (o_ptr->pval < 0)
                {
                    a = TERM_L_GREEN;
                    if (o_ptr->pval > -10)
                        c = I2D(-(o_ptr->pval));
                }
                if (o_ptr->pval > 0)
                {
                    a = TERM_RED;
                    if (o_ptr->pval < 10)
                        c = I2D(o_ptr->pval);
                }
            }

            if (f2 & (1 << stats))
            {
                if (a == TERM_RED)
                    a = TERM_ORANGE;
                else
                    a = TERM_GREEN;

                if (c == '.')
                    c = 's';
            }

            display_player_putch(col, row + stats, a, c);
        }

        col++;
    }

    for (stats = 0; stats < A_MAX; ++stats)
    {
        a = TERM_SLATE;
        c = '.';

        if (f2 & (1 << stats))
        {
            a = TERM_GREEN;
            c = 's';
        }

        display_player_putch(col, row + stats, a, c);
    }

    col = 23;
    c_put_str(TERM_WHITE, "abcdefghijkl@", row + 4, col);
    display_player_equippy(row + 5, col);

    sdl_story_font_disable();
}

static void character_sheet_build_name(char* buf, size_t buf_size)
{
    if (!buf || buf_size == 0)
        return;

    if (p_ptr->oaths_broken)
    {
        strnfmt(buf, buf_size, "%s the Oathbreaker", op_ptr->full_name);
        return;
    }

    strnfmt(buf, buf_size, "%s%s", op_ptr->full_name,
        c_name + current_character_profile->alt_name);
}

static void character_sheet_copy_trimmed(char* dst, size_t dst_size, cptr text,
    int max_chars)
{
    size_t len;

    if (!dst || dst_size == 0)
        return;

    dst[0] = '\0';
    if (!text || !text[0] || max_chars <= 0)
        return;

    len = strlen(text);
    while (len > 0 && text[len - 1] == ' ')
        len--;
    if ((int)len > max_chars)
        len = (size_t)max_chars;
    if (len >= dst_size)
        len = dst_size - 1u;

    memcpy(dst, text, len);
    dst[len] = '\0';
}

static bool character_sheet_add_metric_pair(app_ui_panel* panel, cptr label,
    cptr current, byte current_attr, char separator, cptr rhs, byte rhs_attr)
{
    return app_ui_panel_add_character_metric(panel, TERM_WHITE, label,
        current_attr, current, separator, rhs_attr, rhs);
}

static bool character_sheet_add_metric_value(app_ui_panel* panel, cptr label,
    byte value_attr, cptr value)
{
    return app_ui_panel_add_character_metric(panel, TERM_WHITE, label,
        value_attr, value, '\0', TERM_WHITE, "");
}

static bool character_sheet_build_summary(app_ui_panel* panel)
{
    char cur[32];
    char rhs[32];
    char val[64];
    char buf[160];
    byte value_attr;

    if (!panel)
        return false;

    strnfmt(cur, sizeof(cur), "%ld", (long)p_ptr->new_exp);
    strnfmt(rhs, sizeof(rhs), "%ld", (long)p_ptr->exp);
    if (!character_sheet_add_metric_pair(panel, "Exp", cur, TERM_L_GREEN, '/',
            rhs, TERM_L_GREEN))
    {
        return false;
    }

    {
        long cur_b = (long)(p_ptr->total_weight / 10L);
        long max_b = (long)(weight_limit() / 10L);

        strnfmt(cur, sizeof(cur), "%ld", cur_b);
        strnfmt(rhs, sizeof(rhs), "%ld", max_b);
        if (!character_sheet_add_metric_pair(panel, "Burden", cur,
                (cur_b <= max_b) ? TERM_L_GREEN : TERM_YELLOW, '/',
                rhs, TERM_L_GREEN))
        {
            return false;
        }
    }

    if (turn > 0)
    {
        long cur_d = (long)(p_ptr->depth * 50);
        long min_d = (long)(min_depth() * 50);

        if (cur_d > 1000)
            cur_d = 1000;
        if (min_d > 1000)
            min_d = 1000;

        strnfmt(cur, sizeof(cur), "%ld", cur_d);
        strnfmt(rhs, sizeof(rhs), "%ld", min_d);
        if (!character_sheet_add_metric_pair(panel, "Depth c/m", cur,
                (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW, '/',
                rhs, TERM_L_GREEN))
        {
            return false;
        }
        if (format_min_depth_progress_bar(buf, sizeof(buf), LINEW20)
            && !app_ui_panel_add_character_metric(panel, TERM_WHITE, "",
                TERM_L_BLUE, buf, '\0', TERM_WHITE, ""))
        {
            return false;
        }
    }

    value_attr = format_deep_call_value(val, sizeof(val), 12);
    if (!character_sheet_add_metric_value(panel, "Deep Call", value_attr, val))
        return false;

    comma_number(buf, playerturn);
    if (!character_sheet_add_metric_value(panel, "Turn", TERM_L_GREEN, buf))
        return false;

    strnfmt(val, sizeof(val), "%d", p_ptr->cur_light);
    if (!character_sheet_add_metric_value(panel, "Light", TERM_L_GREEN, val))
        return false;

    if (!app_ui_panel_add_character_metric(panel, TERM_WHITE, "", TERM_WHITE,
            "", '\0', TERM_WHITE, ""))
    {
        return false;
    }

    strnfmt(val, sizeof(val), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    if (!character_sheet_add_metric_value(panel, "Melee", TERM_L_BLUE, val))
        return false;

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK]
        && !character_sheet_add_metric_value(panel, "Melee x2", TERM_L_BLUE,
            val))
    {
        return false;
    }

    if (p_ptr->mds2 > 0)
    {
        strnfmt(val, sizeof(val), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod,
            p_ptr->mdd2, p_ptr->mds2);
        if (!character_sheet_add_metric_value(panel, "Offhand", TERM_L_BLUE,
                val))
        {
            return false;
        }
    }

    strnfmt(val, sizeof(val), "(%+d,%dd%d)", p_ptr->skill_use[S_ARC],
        p_ptr->add, p_ptr->ads);
    if (!character_sheet_add_metric_value(panel, "Bows", TERM_L_BLUE, val))
        return false;

    strnfmt(val, sizeof(val), "[%+d,%d-%d]", p_ptr->skill_use[S_EVN],
        p_min(GF_HURT, true), p_max(GF_HURT, true));
    if (!character_sheet_add_metric_value(panel, "Armor", TERM_L_BLUE, val))
        return false;

    {
        int chp = p_ptr->chp;
        int mhp = p_ptr->mhp;

        if (chp > 999)
            chp = 999;
        if (mhp > 999)
            mhp = 999;
        strnfmt(cur, sizeof(cur), "%d", chp);
        strnfmt(rhs, sizeof(rhs), "%d", mhp);
        if (!character_sheet_add_metric_pair(panel, "Health", cur, TERM_L_BLUE,
                '/', rhs, TERM_L_BLUE))
        {
            return false;
        }
    }

    {
        int csp = p_ptr->csp;
        int msp = p_ptr->msp;

        if (csp > 999)
            csp = 999;
        if (msp > 999)
            msp = 999;
        strnfmt(cur, sizeof(cur), "%d", csp);
        strnfmt(rhs, sizeof(rhs), "%d", msp);
        if (!character_sheet_add_metric_pair(panel, "Voice", cur, TERM_L_BLUE,
                '/', rhs, TERM_L_BLUE))
        {
            return false;
        }
    }

    if (p_ptr->song1 != SNG_NOTHING)
    {
        strnfmt(val, sizeof(val), "%s",
            b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name);
        if (!character_sheet_add_metric_value(panel, "Song", TERM_L_BLUE, val))
            return false;
    }
    if (p_ptr->song2 != SNG_NOTHING)
    {
        strnfmt(val, sizeof(val), "%s",
            b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name);
        if (!character_sheet_add_metric_value(panel, "Song", TERM_L_BLUE, val))
            return false;
    }

    return true;
}

static bool character_sheet_build_traits(app_ui_panel* panel)
{
    compact_trait_line traits[96];
    int i;
    byte story = story_character_enabled() ? STORY_FLAG_USE : 0;

    if (!panel)
        return false;

    for (i = 0; i < collect_compact_trait_lines(traits, N_ELEMENTS(traits)); i++)
    {
        if (!app_ui_panel_add_detail_line_ex(panel, traits[i].col, story,
                traits[i].txt))
        {
            return false;
        }
    }

    return true;
}

static bool character_sheet_build_stats(app_ui_panel* panel)
{
    int i;

    if (!panel)
        return false;

    for (i = 0; i < A_MAX; i++)
    {
        const char* stat_label = (p_ptr->stat_drain[i] < 0)
            ? stat_names_reduced[i]
            : stat_names[i];
        char label[32];
        char value[APP_UI_KEY_MAX];
        char base[APP_UI_KEY_MAX];
        char mod1[APP_UI_KEY_MAX];
        char mod2[APP_UI_KEY_MAX];
        char mod3[APP_UI_KEY_MAX];

        character_sheet_copy_trimmed(label, sizeof(label), stat_label, 12);
        mod1[0] = '\0';
        mod2[0] = '\0';
        mod3[0] = '\0';
        cnv_stat(p_ptr->stat_use[i], value);
        base[0] = '\0';

        if (p_ptr->stat_equip_mod[i] != 0 || p_ptr->stat_drain[i] != 0
            || p_ptr->stat_misc_mod[i] != 0)
        {
            cnv_stat(p_ptr->stat_base[i], base);
            if (p_ptr->stat_equip_mod[i] != 0)
                strnfmt(mod1, sizeof(mod1), "%+3d", p_ptr->stat_equip_mod[i]);
            if (p_ptr->stat_drain[i] != 0)
                strnfmt(mod2, sizeof(mod2), "%+3d", p_ptr->stat_drain[i]);
            if (p_ptr->stat_misc_mod[i] != 0)
                strnfmt(mod3, sizeof(mod3), "%+3d", p_ptr->stat_misc_mod[i]);
        }

        if (!app_ui_panel_add_character_stat(panel, TERM_WHITE, label,
                (p_ptr->stat_drain[i] < 0) ? TERM_YELLOW : TERM_L_GREEN, value,
                TERM_SLATE, base[0] ? '=' : '\0', TERM_GREEN, base, TERM_SLATE,
                mod1, TERM_SLATE, mod2, TERM_SLATE, mod3))
        {
            return false;
        }
    }

    if (!app_ui_panel_add_character_stat(panel, TERM_WHITE, "", TERM_WHITE, "",
            TERM_WHITE, '\0', TERM_WHITE, "", TERM_WHITE, "", TERM_WHITE, "",
            TERM_WHITE, ""))
    {
        return false;
    }

    for (i = 0; i < S_MAX; i++)
    {
        char value[APP_UI_KEY_MAX];
        char base[APP_UI_KEY_MAX];
        char mod1[APP_UI_KEY_MAX];
        char mod2[APP_UI_KEY_MAX];
        char mod3[APP_UI_KEY_MAX];

        if (i == S_SPC)
            continue;

        strnfmt(value, sizeof(value), "%d", p_ptr->skill_use[i]);
        strnfmt(base, sizeof(base), "%d", p_ptr->skill_base[i]);
        mod1[0] = '\0';
        mod2[0] = '\0';
        mod3[0] = '\0';
        if (p_ptr->skill_stat_mod[i] != 0)
            strnfmt(mod1, sizeof(mod1), "%+d", p_ptr->skill_stat_mod[i]);
        if (p_ptr->skill_equip_mod[i] != 0)
            strnfmt(mod2, sizeof(mod2), "%+d", p_ptr->skill_equip_mod[i]);
        if (p_ptr->skill_misc_mod[i] != 0)
            strnfmt(mod3, sizeof(mod3), "%+d", p_ptr->skill_misc_mod[i]);

        if (!app_ui_panel_add_character_stat(panel, TERM_WHITE,
                skill_names_full[i], TERM_L_GREEN, value, TERM_SLATE, '=',
                TERM_GREEN, base, TERM_SLATE, mod1, TERM_SLATE, mod2,
                TERM_SLATE, mod3))
        {
            return false;
        }
    }

    return true;
}

static bool character_sheet_build_history(app_ui_scene* scene,
    app_ui_panel* panel)
{
    byte story = story_character_enabled() ? STORY_FLAG_USE : 0;

    if (!scene || !panel || !p_ptr->history[0])
        return true;

    if (!app_ui_panel_begin_rich_paragraph(scene, panel))
        return false;

    return app_ui_panel_add_rich_text_ex(scene, panel, TERM_WHITE, story,
        p_ptr->history);
}

bool build_character_sheet_ui_scene(app_ui_scene* scene, cptr prompt_text)
{
    app_ui_panel* panel;
    char name[64];

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_CHARACTER_SHEET;
    panel->min_width_px = 0;
    panel->width_cap_px = 0;
    character_sheet_build_name(name, sizeof(name));
    app_ui_panel_set_title(panel, p_ptr->oaths_broken ? TERM_RED : TERM_L_BLUE,
        name);

    if (!character_sheet_build_summary(panel)
        || !character_sheet_build_traits(panel)
        || !character_sheet_build_stats(panel)
        || !character_sheet_build_history(scene, panel))
    {
        return false;
    }

    if (prompt_text && prompt_text[0])
    {
        app_ui_panel* strip = app_ui_scene_append_panel(scene,
            APP_UI_LAYER_CHROME);

        if (!strip)
            return false;
        strip->style = APP_UI_PANEL_STYLE_STRIP;
        strip->flags |= APP_UI_PANEL_FLAG_BOTTOM_ANCHORED;
        if (!app_ui_panel_add_body_line(strip, TERM_L_WHITE, prompt_text))
            return false;
    }

    return true;
}

void display_player(int mode)
{
    int wid = 80;
    int hgt = 24;
    int wide_offset = 0;
    bool narrow = false;

    display_player_get_layout_size(&wid, &hgt);
    (void)hgt;
    narrow = (wid > 0 && wid < 80);
    if (wid > 80)
        wide_offset = (wid - 80) / 2;

    clear_from(0);
    display_player_compact_max_scroll = 0;

    if (narrow && (mode == DISPLAY_PLAYER_MODE_STANDARD))
        mode = DISPLAY_PLAYER_MODE_COMPACT_STATS_SKILLS;

    if (mode == DISPLAY_PLAYER_MODE_COMPACT_DESC_FLAGS)
    {
        display_player_compact_desc_flags_page(true, true);
        if (display_player_compact_max_scroll > 0)
        {
            clear_from(0);
            display_player_compact_desc_flags_page(true, false);
        }
        if (display_player_compact_max_scroll > 0)
        {
            clear_from(0);
            display_player_compact_desc_flags_page(false, false);
        }
        sdl_story_font_reset();
        return;
    }

    if (mode == DISPLAY_PLAYER_MODE_COMPACT_STATS_SKILLS)
    {
        display_player_misc_info();
        int body_row = display_player_compact_summary_block(
            display_player_compact_start_row());
        display_player_compact_attributes_and_skills(body_row);
        sdl_story_font_reset();
        return;
    }

    if (mode == DISPLAY_PLAYER_MODE_COMPACT_SKILLS)
    {
        display_player_misc_info();
        int body_row = display_player_compact_summary_block(
            display_player_compact_start_row());
        display_player_compact_skills_list(body_row);
        sdl_story_font_reset();
        return;
    }

    if (mode == DISPLAY_PLAYER_MODE_COMPACT_HISTORY)
    {
        display_player_misc_info();
        int body_row = display_player_compact_summary_block(
            display_player_compact_start_row());
        display_player_compact_history(body_row);
        sdl_story_font_reset();
        return;
    }

    display_player_stat_info(1, 41 + wide_offset);

    if (mode <= DISPLAY_PLAYER_MODE_FLAGS)
    {
        display_player_misc_info();

        if (mode == DISPLAY_PLAYER_MODE_FLAGS)
        {
            display_player_sust_info();
            display_player_flag_info();
        }
        else
        {
            display_player_xtra_info(DISPLAY_PLAYER_MODE_STANDARD);
        }
    }

    sdl_story_font_reset();
}
