#include "angband.h"
#include "ui/ui-language.h"
#include "object/object-difficulty.h"
#include "object/object-info.h"
#include "cave/cave-events.h"
#include "achievements.h"
#include "achievements-character.h"
#include "tutorial/tutorial-game.h"
#include "sdl-config.h"
#include "sound-config.h"
#include "sdl-sound.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "pane.h"
#include "cmd/ui/cmd-ui-internal.h"

bool enchant_then_numbers;

typedef struct smithing_tval_desc
{
    int category;
    int tval;
} smithing_tval_desc;

// object being created
object_type smith_o_body;
object_type* smith_o_ptr = &smith_o_body;

// backup object
object_type smith2_o_body;
object_type* smith2_o_ptr = &smith2_o_body;

// backup while previewing numbers
object_type smith3_o_body;
object_type* smith3_o_ptr = &smith3_o_body;

typedef enum
{
    SMITH_ALLOY_NONE = 0,
    SMITH_ALLOY_MITHRIL,
    SMITH_ALLOY_STAR_IRON,
} smith_alloy_type;

typedef struct
{
    smith_alloy_type type;
    byte bonus_att;
    byte bonus_ds;
    byte bonus_evn;
    byte bonus_ps;
} smith_alloy_state;

static smith_alloy_state smith_alloy;
static smith_alloy_state smith2_alloy;
static smith_alloy_state smith3_alloy;

/* This work owns a withdrawn item and has already paid its delta costs.
 * Both the object marker and remaining work are stored in existing saves. */
static bool smith_pending_reforge(void)
{
    return p_ptr->smithing_leftover > 0 && smith_o_ptr->k_idx
        && smith_o_ptr->unused1 == 2;
}

// artefact being created
#define smith_a_name (z_info->art_self_made_max - 1)
#define smith_a_ptr (&a_info[smith_a_name])

// backup artefact
#define smith2_a_name (z_info->art_self_made_max - 2)
#define smith2_a_ptr (&a_info[smith2_a_name])

/*
 * A structure to hold the costs of smithing something
 */
typedef struct smithing_cost_type
{
    int str;
    int dex;
    int con;
    int gra;
    int exp;
    int smt;
    int mithril;
    int star_iron;
    int alloy_weight;
    int alloy_metal;
    int alloy_mastery;
    int uses;
    int drain;
    int weaponsmith;
    int armoursmith;
    int jeweller;
    int enchantment;
    int artifice;
} smithing_cost_type;

smithing_cost_type smithing_cost;

/* The Details view reads the same bill as the current preview. */
static smithing_cost_type smith_ui_detail_cost;
static int smith_ui_detail_turns;
static bool smith_ui_detail_valid;
static bool smith_ui_detail_reforge;
static bool smith_ui_detail_paid;
static char smith_ui_detail_name[160];
static void smith_show_cost_details(void);

#define CAT_WEAPON 0
#define CAT_ARMOUR 1
#define CAT_JEWELRY 2

#define MAX_SMITHING_TVALS 17

#define SMT_MENU_CREATE 1
#define SMT_MENU_ENCHANT 2
#define SMT_MENU_ARTEFACT 3
#define SMT_MENU_NUMBERS 4
#define SMT_MENU_MELT 5
#define SMT_MENU_REPAIR 6
#define SMT_MENU_ACCEPT 7

#define SMT_MENU_MAX 7

typedef enum smith_ui_scroll_id
{
    SMITH_SCROLL_SVAL = 0,
    SMITH_SCROLL_VALUES,
    SMITH_SCROLL_REFORGE,
    SMITH_SCROLL_ENCHANT,
    SMITH_SCROLL_FLAG,
    SMITH_SCROLL_ABILITY,
    SMITH_SCROLL_MELT,
    SMITH_SCROLL_EDITOR,
    SMITH_SCROLL_MAX
} smith_ui_scroll_id;

static int smith_ui_scroll_top[SMITH_SCROLL_MAX];

#define SMITH_CLICK_BACK 33000
#define SMITH_CLICK_CALC 33001
#define SMITH_CLICK_DESCRIPTION 33002

static struct {
    char text[2048];
    int top, rows, max_top;
    bool active, effects;
} smith_description;

static void smith_show_description(void);

static int smith_ui_term_wid(void)
{
    int wid = 80;

    if (Term_get_size(&wid, NULL) == 0 && wid > 0)
        return wid;

    return (Term && (Term->wid > 0)) ? Term->wid : 80;
}

static int smith_ui_term_hgt(void)
{
    int hgt = 24;

    if (Term_get_size(NULL, &hgt) == 0 && hgt > 0)
        return hgt;

    return (Term && (Term->hgt > 0)) ? Term->hgt : 24;
}

static int smith_ui_content_bottom_row(void)
{
    int bottom = smith_ui_term_hgt() - 1
        - sdl_touch_menu_button_reserved_rows();

    /* Keyboard/controller Smithing uses the same dedicated bottom prompt row
     * as Inventory and Abilities. Touch-only layouts use the native buttons
     * below the sheet. */
    if (!sdl_touch_only_device_active())
        bottom--;

    return MAX(0, bottom);
}

static int smith_ui_prompt_row(void)
{
    return MAX(0, smith_ui_term_hgt() - 1
        - sdl_touch_menu_button_reserved_rows());
}

static int smith_ui_safe_width(int col, int width)
{
    int term_wid = smith_ui_term_wid();
    int max_width;

    if (col < 0)
    {
        width += col;
        col = 0;
    }

    if (col >= term_wid || width <= 0)
        return 0;

    /* Leave the final column alone so Term_addstr() never marks the cursor
     * unusable after a fitted UI write. */
    max_width = term_wid - col - 1;
    if (max_width < 1 && term_wid > col)
        max_width = 1;

    if (width > max_width)
        width = max_width;

    return (width > 0) ? width : 0;
}

static int smith_ui_utf8_prefix_len(cptr text, int max_cols)
{
    int bytes = 0;
    int cols = 0;

    if (!text || max_cols <= 0)
        return 0;

    while (text[bytes])
    {
        int char_len = utf8_sequence_len(text + bytes);
        int char_width;

        if (char_len <= 0)
            break;

        char_width = utf8_display_width_n(text + bytes, char_len);
        if (char_width > 0 && cols + char_width > max_cols)
            break;

        cols += char_width;
        bytes += char_len;
    }

    return bytes;
}

static void smith_ui_fit_text(char* buf, size_t buflen, cptr text, int width)
{
    if (!buf || buflen == 0)
        return;

    if (!text)
        text = "";

    if (width <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if (utf8_display_width_n(text, (int)strlen(text)) <= width)
    {
        SDL_strlcpy(buf, text, buflen);
        return;
    }

    if (width <= 3)
    {
        int copy_len = smith_ui_utf8_prefix_len(text, width);
        if (copy_len >= (int)buflen)
            copy_len = utf8_safe_prefix_len(text, (int)buflen - 1);
        if (copy_len < 0)
            copy_len = 0;
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        return;
    }

    {
        int copy_len = smith_ui_utf8_prefix_len(text, width - 3);
        if (copy_len >= (int)buflen)
            copy_len = utf8_safe_prefix_len(text, (int)buflen - 1);
        if (copy_len < 0)
            copy_len = 0;
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        SDL_strlcat(buf, "...", buflen);
    }
}

static void smith_ui_draw_fitted(int col, int row, int width, byte attr,
    cptr text, bool erase)
{
    char fitted[180];

    if (row < 0 || row > smith_ui_content_bottom_row())
        return;

    width = smith_ui_safe_width(col, width);
    if (width <= 0)
        return;

    smith_ui_fit_text(fitted, sizeof(fitted), text, width);
    if (erase)
        Term_erase(col, row, width);
    Term_putstr(col, row, width, attr, fitted);
}

static void smith_ui_put_fitted(int col, int row, int width, byte attr, cptr text)
{
    smith_ui_draw_fitted(col, row, width, attr, text, true);
}

static void smith_ui_build_object_stats(char* stats, size_t stats_size,
    cptr suffix, cptr weight)
{
    if (!stats || stats_size == 0)
        return;

    stats[0] = '\0';
    if (suffix && suffix[0])
        SDL_strlcpy(stats, suffix, stats_size);
    if (weight && weight[0])
    {
        if (stats[0])
            SDL_strlcat(stats, "   ", stats_size);
        SDL_strlcat(stats, weight, stats_size);
    }
}

static void smith_ui_fill_row(int col, int row, int width, byte attr)
{
    char fill[180];

    if (row < 0 || row > smith_ui_content_bottom_row())
        return;

    width = smith_ui_safe_width(col, width);
    if (width <= 0)
        return;
    if (width >= (int)sizeof(fill))
        width = (int)sizeof(fill) - 1;

    SDL_memset(fill, ' ', (size_t)width);
    fill[width] = '\0';
    Term_putstr(col, row, width, attr, fill);
}

static void smith_prepare_display_object(object_type* object, int kind)
{
    u64b saved_state = Rand_state_export();
    object_prep(object, kind);
    Rand_state_pop(saved_state);
}

static bool smith_ui_wrap_next(cptr* cursor, int width, char* line, size_t size)
{
    cptr text = *cursor;
    while (isspace((unsigned char)*text)) text++;
    if (!*text || width <= 0) return false;

    int take = smith_ui_utf8_prefix_len(text, width);
    take = MIN(take, utf8_safe_prefix_len(text, (int)size - 1));
    take = MIN(take, (int)strcspn(text, "\r\n"));
    if (!take) return false;
    if (text[take] && !isspace((unsigned char)text[take]))
    {
        int space = take;
        while (space > 0 && !isspace((unsigned char)text[space])) space--;
        if (space > 0) take = space;
    }
    memcpy(line, text, take);
    line[take] = '\0';
    *cursor = text + take;
    return true;
}

static int smith_ui_put_wrapped(int col, int row, int width, int max_lines,
    byte attr, cptr text)
{
    char line[180];
    int used = 0;
    width = smith_ui_safe_width(col, width);
    while (text && used < max_lines)
    {
        cptr start = text;
        if (!smith_ui_wrap_next(&text, width, line, sizeof(line))) break;
        if (used + 1 == max_lines && *text)
        {
            while (isspace((unsigned char)*start)) start++;
            smith_ui_fit_text(line, sizeof(line), start, width);
        }
        smith_ui_put_fitted(col, row + used++, width, attr, line);
    }
    return used;
}

typedef struct smith_bill_line {
    cptr label;
    char value[32];
    bool requirement;
} smith_bill_line;

#define SMITH_BILL_EXTRAS_MAX 15

/* The full cost report and the work order itemize the very same bill. */
static int smith_bill_extras(const smithing_cost_type* cost, bool reforge,
    smith_bill_line lines[SMITH_BILL_EXTRAS_MAX])
{
    int n = 0;
    const struct { cptr name; bool missing; } abilities[] = {
        { ui_text("Reforging"), reforge }, { ui_text("Weaponsmith"), cost->weaponsmith },
        { ui_text("Armoursmith"), cost->armoursmith }, { ui_text("Jeweller"), cost->jeweller },
        { ui_text("Enchantment"), cost->enchantment }, { ui_text("Artifice"), cost->artifice },
        { ui_text("Alloy Mastery"), cost->alloy_mastery }
    };
    for (size_t i = 0; i < N_ELEMENTS(abilities); i++)
    {
        if (!abilities[i].missing) continue;
        lines[n].label = ui_text("Requires");
        lines[n].requirement = true;
        SDL_strlcpy(lines[n++].value, abilities[i].name, sizeof(lines[0].value));
    }
    const struct { cptr name; int amount; bool weight; } charges[] = {
        { ui_text("Smithing ranks"), cost->drain, false },
        { "Mithril", cost->mithril, true }, { ui_text("Star iron"), cost->star_iron, true },
        { ui_text("Strength"), cost->str, false }, { ui_text("Dexterity"), cost->dex, false },
        { ui_text("Constitution"), cost->con, false }, { ui_text("Grace"), cost->gra, false },
        { ui_text("Experience"), cost->exp, false }
    };
    for (size_t i = 0; i < N_ELEMENTS(charges); i++)
    {
        int amount = charges[i].amount;
        if (!amount) continue;
        lines[n].label = charges[i].name;
        lines[n].requirement = false;
        if (charges[i].weight)
            strnfmt(lines[n].value, sizeof(lines[n].value), ui_text("%d.%d lb"),
                amount / 10, ABS(amount % 10));
        else
            strnfmt(lines[n].value, sizeof(lines[n].value), "%d", amount);
        n++;
    }
    return n;
}

static bool smith_ui_take_click_action(int* choice, int* action)
{
    if (!ui_menu_click_take_action(choice, action))
        return false;
    if (*choice == SMITH_CLICK_CALC)
    {
        if (*action != UI_MENU_CLICK_HOVER)
            smith_show_cost_details();
        *choice = 0;
        *action = UI_MENU_CLICK_HOVER;
    }
    else if (*choice == SMITH_CLICK_DESCRIPTION && *action != UI_MENU_CLICK_SECONDARY)
    {
        if (*action == UI_MENU_CLICK_PRIMARY) smith_show_description();
        *choice = 0;
        *action = UI_MENU_CLICK_HOVER;
    }
    return true;
}

static int smith_sheet_read_key(int previous_tab, int next_tab)
{
    hide_cursor = true;
    int key = steamdeck_menu_key(inkey(), previous_tab, next_tab);
    hide_cursor = false;
    if (smith_description.active && (key == 'D' || (key == '3' && !smith_description.rows)))
    {
        smith_show_description();
        return 0;
    }
    if (smith_description.active && (key == '9' || key == '3'))
    {
        int step = MAX(1, smith_description.rows - 1);
        smith_description.top = MAX(0, MIN(smith_description.max_top,
            smith_description.top + (key == '9' ? -step : step)));
        return 0;
    }
    return key;
}

/*
 * Match the abilities menu's pointer contract: pointing/first tapping moves
 * the highlight, while activating an already-highlighted row confirms it.
 * Keyboard and controller confirmations do not pass through this helper.
 */
static bool smith_ui_pointer_choice_confirms(
    int choice, int action, int* highlight)
{
    bool same_choice;

    if (!highlight)
        return false;

    same_choice = (*highlight == choice);
    *highlight = choice;

    return action != UI_MENU_CLICK_HOVER && same_choice;
}

static int smith_ui_configure_list_view(smith_ui_scroll_id id, int count,
    int highlight, int first_row, int last_row)
{
    int visible;
    int max_top;
    int* top;
    bool touch_only = sdl_touch_only_device_active();

    if (id < 0 || id >= SMITH_SCROLL_MAX)
        return 0;

    visible = MAX(1, last_row - first_row + 1);
    max_top = MAX(0, count - visible);
    top = &smith_ui_scroll_top[id];

    if (touch_only)
        (void)ui_scroll_area_take_touch_scrolled();

    if (*top < 0)
        *top = 0;
    if (*top > max_top)
        *top = max_top;

    /* Inventory-style touch lists are viewport-driven: an off-screen selection
     * must not pull the list back after the finger drags it.  Non-touch control
     * schemes continue to keep keyboard/controller highlights visible. */
    if (!touch_only && count > 0)
    {
        int selected = MAX(0, MIN(count - 1, highlight - 1));

        if (selected < *top)
            *top = selected;
        else if (selected >= *top + visible)
            *top = selected - visible + 1;
    }

    if (touch_only)
        ui_scroll_area_set_offset_target(top, max_top);
    ui_scroll_area_set_indicator(*top, max_top);

    return *top;
}

/*
 * Item types in display order within each smithing craft.
 */
static const smithing_tval_desc smithing_tvals[MAX_SMITHING_TVALS] = {
    { CAT_WEAPON, TV_SWORD },
    { CAT_WEAPON, TV_POLEARM },
    { CAT_WEAPON, TV_HAFTED },
    { CAT_WEAPON, TV_DIGGING },
    { CAT_WEAPON, TV_BOW },
    { CAT_WEAPON, TV_ARROW },
    { CAT_JEWELRY, TV_RING },
    { CAT_JEWELRY, TV_AMULET },
    { CAT_JEWELRY, TV_LIGHT },
    { CAT_JEWELRY, TV_HORN },
    { CAT_ARMOUR, TV_SOFT_ARMOR },
    { CAT_ARMOUR, TV_MAIL },
    { CAT_ARMOUR, TV_CLOAK },
    { CAT_ARMOUR, TV_SHIELD },
    { CAT_ARMOUR, TV_HELM },
    { CAT_ARMOUR, TV_GLOVES },
    { CAT_ARMOUR, TV_BOOTS },
};

static bool object_has_evil_alignment(const object_type* o_ptr);

static void smith_clear_alloy_state(smith_alloy_state* state)
{
    state->type = SMITH_ALLOY_NONE;
    state->bonus_att = 0;
    state->bonus_ds = 0;
    state->bonus_evn = 0;
    state->bonus_ps = 0;
}

static void smith_remove_alloy_bonus(object_type* o_ptr, smith_alloy_state* state)
{
    if (!state)
        return;

    if (state->type != SMITH_ALLOY_NONE && o_ptr && o_ptr->k_idx)
    {
        o_ptr->att -= state->bonus_att;
        if (o_ptr->ds >= state->bonus_ds)
            o_ptr->ds -= state->bonus_ds;
        else
            o_ptr->ds = 0;
        o_ptr->evn -= state->bonus_evn;
        if (o_ptr->ps >= state->bonus_ps)
            o_ptr->ps -= state->bonus_ps;
        else
            o_ptr->ps = 0;
    }

    smith_clear_alloy_state(state);
}

static int smith_item_category(const object_type* o_ptr)
{
    if (!o_ptr)
        return -1;

    for (int i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        if (smithing_tvals[i].tval == o_ptr->tval)
            return smithing_tvals[i].category;
    }

    return -1;
}

static bool smith_alloy_applicable(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (object_has_evil_alignment(o_ptr))
        return false;

    int cat = smith_item_category(o_ptr);
    if ((cat != CAT_WEAPON) && (cat != CAT_ARMOUR))
        return false;

    /* Cannot alloy items that are already made of special metal */
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    if (k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
        return false;

    /* Armour: only major metal pieces */
    if (cat == CAT_ARMOUR)
    {
        switch (o_ptr->tval)
        {
        case TV_MAIL:
        case TV_SHIELD:
        case TV_HELM:
            return true;
        default:
            return false;
        }
    }

    /* Weapons: exclude quarterstaves (wooden) */
    if ((o_ptr->tval == TV_HAFTED) && (o_ptr->sval == SV_QUARTERSTAFF))
        return false;

    return true;
}

static bool smith_apply_alloy(object_type* o_ptr, smith_alloy_state* state, smith_alloy_type new_type)
{
    if (!o_ptr || !state)
        return false;

    smith_remove_alloy_bonus(o_ptr, state);

    if (new_type == SMITH_ALLOY_NONE)
        return true;

    if (!smith_alloy_applicable(o_ptr))
        return false;

    int cat = smith_item_category(o_ptr);
    if (cat == CAT_WEAPON)
    {
        if (new_type == SMITH_ALLOY_MITHRIL)
            state->bonus_att = 1;
        else if (new_type == SMITH_ALLOY_STAR_IRON)
            state->bonus_ds = 1;
    }
    else if (cat == CAT_ARMOUR)
    {
        if (new_type == SMITH_ALLOY_MITHRIL)
            state->bonus_evn = 1;
        else if (new_type == SMITH_ALLOY_STAR_IRON)
            state->bonus_ps = 1;
    }
    else
    {
        return false;
    }

    o_ptr->att += state->bonus_att;
    o_ptr->ds += state->bonus_ds;
    o_ptr->evn += state->bonus_evn;
    o_ptr->ps += state->bonus_ps;
    state->type = new_type;
    return true;
}

static int smith_alloy_weight_required(const object_type* o_ptr)
{
    int total_weight = o_ptr->weight * ((o_ptr->number > 0) ? o_ptr->number : 1);
    return (total_weight + 3) / 4;
}

void smithing_alloy_save_state(byte state[SMITHING_ALLOY_STATE_BYTES])
{
    if (!state)
        return;

    memset(state, 0, SMITHING_ALLOY_STATE_BYTES);
    if (!smith_o_ptr || !smith_o_ptr->k_idx)
        return;

    state[0] = (byte)smith_alloy.type;
    state[1] = smith_alloy.bonus_att;
    state[2] = smith_alloy.bonus_ds;
    state[3] = smith_alloy.bonus_evn;
    state[4] = smith_alloy.bonus_ps;
}

void smithing_alloy_load_state(const byte state[SMITHING_ALLOY_STATE_BYTES])
{
    byte expected[SMITHING_ALLOY_STATE_BYTES] = { 0 };
    int cat;

    /* Metarun loading can reuse this process for another character. */
    smith_clear_alloy_state(&smith_alloy);
    smith_clear_alloy_state(&smith2_alloy);
    smith_clear_alloy_state(&smith3_alloy);
    if (!state)
        return;

    if (state[0] == SMITH_ALLOY_NONE)
    {
        if (memcmp(state, expected, sizeof(expected)) != 0)
            log_warn("Ignoring invalid saved smithing alloy bonuses.");
        return;
    }

    if (state[0] > SMITH_ALLOY_STAR_IRON || smith_pending_reforge()
        || !smith_alloy_applicable(smith_o_ptr))
    {
        log_warn("Ignoring invalid saved smithing alloy type %u.",
            (unsigned)state[0]);
        return;
    }

    cat = smith_item_category(smith_o_ptr);
    expected[0] = state[0];
    if (cat == CAT_WEAPON)
        expected[state[0] == SMITH_ALLOY_MITHRIL ? 1 : 2] = 1;
    else
        expected[state[0] == SMITH_ALLOY_MITHRIL ? 3 : 4] = 1;

    if (memcmp(state, expected, sizeof(expected)) != 0)
    {
        log_warn("Ignoring invalid saved smithing alloy bonuses.");
        return;
    }

    /* The object reader already restored these bonuses on the blueprint.
     * Restore their accounting metadata without applying them again. */
    smith_alloy.type = (smith_alloy_type)state[0];
    smith_alloy.bonus_att = state[1];
    smith_alloy.bonus_ds = state[2];
    smith_alloy.bonus_evn = state[3];
    smith_alloy.bonus_ps = state[4];
}

void smithing_reset_work(void)
{
    object_wipe(smith_o_ptr);
    object_wipe(smith2_o_ptr);
    object_wipe(smith3_o_ptr);
    smithing_alloy_load_state(NULL);
    if (a_info && z_info && z_info->art_self_made_max >= 2
        && z_info->art_self_made_max <= z_info->art_max)
    {
        /* These are draft definitions, rather than completed artefacts. */
        for (int i = z_info->art_self_made_max - 2;
             i < z_info->art_self_made_max; i++)
            artefact_wipe(i);
    }
    memset(&smithing_cost, 0, sizeof(smithing_cost));
    enchant_then_numbers = false;
}

/*
 * A structure to hold a flag and its smithing category
 */
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
#define SMITH_ARTEFACT_ABILITY_MAX 4

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

/*
 * Artifice (custom artefact) bonus limits.
 *
 * When smithing a custom artefact, the item's max values from the R: line
 * are extended by these per-category bonuses.  All artefact-specific limits
 * live in this single table so they are easy to find and tune.
 *
 * 'bonus' fields are ADDED to the normal max (e.g. weapon att = max_att + 4).
 * 'floor' fields set a MINIMUM artefact max (e.g. rings always reach att 4).
 * The result is: artefact_max = max(normal_max + ego + bonus, floor).
 */

/* Forward declarations for data-driven smithing limit functions */
static void smithing_ego_bonus_sums(const object_type* o_ptr,
    int* max_att_sum, int* max_att_min_inc,
    int* to_ds_sum, int* to_ds_min_inc,
    int* max_evn_sum, int* max_evn_min_inc,
    int* to_ps_sum, int* to_ps_min_inc,
    int* max_pval_sum, int* max_pval_min_inc);
int att_max(void);
int att_min(void);
int ds_max(void);
int ds_min(void);
int evn_max(void);
int evn_min(void);
int ps_max(void);
int ps_min(void);

typedef struct
{
    int att_bonus;
    int att_floor;   /* 0 = unused */
    int ds_bonus;
    int evn_bonus;
    int evn_floor;   /* 0 = unused */
    int ps_bonus;
    int ps_floor;    /* 0 = unused */
    int pval_bonus;
} artifice_limits_t;

/* Indexed by a small enum - looked up via artifice_bonus_for(). */
enum {
    ARTIFICE_ARROW,
    ARTIFICE_MELEE,     /* sword, polearm, hafted */
    ARTIFICE_BOW,
    ARTIFICE_DIGGING,
    ARTIFICE_ARMOR,
    ARTIFICE_GLOVES,
    ARTIFICE_RING,
    ARTIFICE_AMULET,
    ARTIFICE_DEFAULT,
    ARTIFICE_MAX
};

static const artifice_limits_t artifice_table[ARTIFICE_MAX] = {
    /*               att_b att_f ds_b evn_b evn_f ps_b ps_f pval_b */
    /* ARROW   */  {  8,    0,    0,   0,    0,    0,   0,   0  },
    /* MELEE   */  {  4,    0,    2,   1,    0,    0,   0,   4  },
    /* BOW     */  {  4,    0,    2,   0,    0,    0,   0,   4  },
    /* DIGGING */  {  4,    0,    2,   0,    0,    0,   0,   4  },
    /* ARMOR   */  {  1,    0,    0,   1,    0,    2,   0,   4  },
    /* GLOVES  */  {  2,    0,    0,   1,    0,    2,   0,   4  },
    /* RING    */  {  0,    4,    0,   0,    4,    0,   0,   4  },
    /* AMULET  */  {  0,    0,    0,   0,    0,    0,   3,   4  },
    /* DEFAULT */  {  0,    0,    0,   0,    0,    0,   0,   4  },
};

static int artifice_category(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:      return ARTIFICE_ARROW;
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:     return ARTIFICE_MELEE;
    case TV_BOW:        return ARTIFICE_BOW;
    case TV_DIGGING:    return ARTIFICE_DIGGING;
    case TV_GLOVES:     return ARTIFICE_GLOVES;
    case TV_BOOTS:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:       return ARTIFICE_ARMOR;
    case TV_RING:       return ARTIFICE_RING;
    case TV_AMULET:     return ARTIFICE_AMULET;
    default:            return ARTIFICE_DEFAULT;
    }
}

static const artifice_limits_t* artifice_bonus_for(const object_type* o_ptr)
{
    return &artifice_table[artifice_category(o_ptr)];
}

/*
 * Determines the maximum legal attack bonus for an item.
 * Uses data-driven max_att from object.txt R: lines, ego sums,
 * and the artifice table for custom artefacts.
 */
int att_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, &max_att_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int att = k_ptr->max_att;
    att += max_att_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        att += al->att_bonus;
        if (al->att_floor > att)
            att = al->att_floor;
    }

    return (att);
}

/*
 * Determines the minimum legal attack bonus for an item.
 */
int att_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, &max_att_min_inc, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int att = k_ptr->att;
    att += max_att_min_inc;
    return (att);
}

/*
 * Determines the maximum legal damage sides for an item.
 */
int ds_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, &to_ds_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int ds = k_ptr->max_ds;
    ds += to_ds_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        ds += al->ds_bonus;
    }

    return (ds);
}

/*
 * Determines the minimum legal damage sides for an item.
 */
int ds_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, &to_ds_min_inc, NULL, NULL, NULL, NULL, NULL, NULL);

    int ds = k_ptr->ds;
    ds += to_ds_min_inc;

    /* Never allow weapons to reach 0-sided damage. */
    if (k_ptr->dd > 0 && ds < 1)
        ds = 1;

    return (ds);
}

/*
 * Determines the maximum legal evasion bonus for an item.
 */
int evn_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, &max_evn_sum, NULL, NULL, NULL, NULL, NULL);

    int evn = k_ptr->max_evn;
    evn += max_evn_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        evn += al->evn_bonus;
        if (al->evn_floor > evn)
            evn = al->evn_floor;
    }

    return (evn);
}

/*
 * Determines the minimum legal evasion bonus for an item.
 */
int evn_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, &max_evn_min_inc, NULL, NULL, NULL, NULL);

    int evn = k_ptr->evn;
    evn += max_evn_min_inc;
    return (evn);
}

/*
 * Determines the maximum legal protection sides for an item.
 */
int ps_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_sum, NULL, NULL, NULL);

    int ps = k_ptr->max_ps;
    ps += to_ps_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        ps += al->ps_bonus;
        if (al->ps_floor > ps)
            ps = al->ps_floor;
    }

    return (ps);
}

/*
 * Determines the minimum legal protection sides for an item.
 */
int ps_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_min_inc, NULL, NULL);

    int ps = k_ptr->ps;
    ps += to_ps_min_inc;
    return (ps);
}

static bool smithing_variable_protection_dice(const object_type* o_ptr)
{
    return o_ptr && o_ptr->tval == TV_AMULET
        && ((o_ptr->sval == SV_AMULET_PROTECTION)
            || (o_ptr->name1 && (o_ptr->pd > 0)));
}

typedef struct
{
    byte pd;
    byte ps;
} smithing_protection_combo;

static const smithing_protection_combo smithing_amulet_protection_combos[] = {
    { 1, 1 },
    { 1, 2 },
    { 1, 3 },
    { 2, 1 },
    { 2, 2 },
    { 2, 3 },
};

static int smithing_protection_combo_index(const object_type* o_ptr)
{
    size_t i;

    if (!smithing_variable_protection_dice(o_ptr))
        return -1;

    for (i = 0; i < N_ELEMENTS(smithing_amulet_protection_combos); i++)
    {
        if ((o_ptr->pd == smithing_amulet_protection_combos[i].pd)
            && (o_ptr->ps == smithing_amulet_protection_combos[i].ps))
        {
            return (int)i;
        }
    }

    return -1;
}

static void smithing_set_protection_combo(object_type* o_ptr, int combo_idx)
{
    if (!o_ptr)
        return;

    if (combo_idx < 0 || combo_idx >= (int)N_ELEMENTS(smithing_amulet_protection_combos))
        return;

    o_ptr->pd = smithing_amulet_protection_combos[combo_idx].pd;
    o_ptr->ps = smithing_amulet_protection_combos[combo_idx].ps;
}

/*
 * Determines the maximum legal pval for an item.
 * Uses data-driven max_pval from object.txt R: lines, ego sums,
 * and the artifice table for custom artefacts.
 */
int pval_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    u32b f1, f2, f3;
    int max_pval_sum = 0;
    int max_pval_min_inc = 0;

    object_flags(smith_o_ptr, &f1, &f2, &f3);
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &max_pval_sum, &max_pval_min_inc);

    /* Start with the data-driven max from R: line */
    int pval = k_ptr->max_pval;

    /* Artefact bonus from the centralized artifice table */
    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        pval += al->pval_bonus;
    }

    /* Ego items have pvals limited by their 'special.txt' C: entries. */
    if (cursed_p(smith_o_ptr))
    {
        pval -= max_pval_min_inc;
    }
    else
    {
        pval += max_pval_sum;
    }

    return (pval);
}

/*
 * Determines the minimum legal pval for an item.
 * Accounts for ego min_pval requirements from special.txt C: line.
 */
int pval_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int base_min = k_ptr->pval;

    /* Check both prefix and suffix egos for min_pval requirements */
    byte egos[2] = { object_ego_prefix(smith_o_ptr), object_ego_suffix(smith_o_ptr) };
    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        if (e_ptr->min_pval > 0)
        {
            /* Ego requires a minimum pval contribution */
            base_min += e_ptr->min_pval;
        }
        else if (e_ptr->max_pval > 0)
        {
            /* Default: at least +1 pval when ego grants pval */
            base_min += 1;
        }
    }

    return base_min;
}

static void smithing_ego_bonus_sums(const object_type* o_ptr,
    int* max_att_sum, int* max_att_min_inc,
    int* to_ds_sum, int* to_ds_min_inc,
    int* max_evn_sum, int* max_evn_min_inc,
    int* to_ps_sum, int* to_ps_min_inc,
    int* max_pval_sum, int* max_pval_min_inc)
{
    if (max_att_sum) *max_att_sum = 0;
    if (max_att_min_inc) *max_att_min_inc = 0;
    if (to_ds_sum) *to_ds_sum = 0;
    if (to_ds_min_inc) *to_ds_min_inc = 0;
    if (max_evn_sum) *max_evn_sum = 0;
    if (max_evn_min_inc) *max_evn_min_inc = 0;
    if (to_ps_sum) *to_ps_sum = 0;
    if (to_ps_min_inc) *to_ps_min_inc = 0;
    if (max_pval_sum) *max_pval_sum = 0;
    if (max_pval_min_inc) *max_pval_min_inc = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return;

    byte egos[2] = { object_ego_prefix(o_ptr), object_ego_suffix(o_ptr) };
    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        int max_att = (int)(int8_t)e_ptr->max_att;
        int to_ds = (int)(int8_t)e_ptr->to_ds;
        int max_evn = (int)(int8_t)e_ptr->max_evn;
        int to_ps = (int)(int8_t)e_ptr->to_ps;

        if (max_att)
        {
            if (max_att_sum) *max_att_sum += max_att;
            if (max_att_min_inc)
                (*max_att_min_inc) += (max_att > 0) ? 1 : -1;
        }
        if (to_ds)
        {
            if (to_ds_sum) *to_ds_sum += to_ds;
            if (to_ds_min_inc)
                (*to_ds_min_inc) += (to_ds > 0) ? 1 : -1;
        }
        if (max_evn)
        {
            if (max_evn_sum) *max_evn_sum += max_evn;
            if (max_evn_min_inc)
                (*max_evn_min_inc) += (max_evn > 0) ? 1 : -1;
        }
        if (to_ps)
        {
            if (to_ps_sum) *to_ps_sum += to_ps;
            if (to_ps_min_inc)
                (*to_ps_min_inc) += (to_ps > 0) ? 1 : -1;
        }

        if (e_ptr->max_pval > 0)
        {
            if (max_pval_sum) *max_pval_sum += e_ptr->max_pval;
            if (max_pval_min_inc)
                (*max_pval_min_inc) += (e_ptr->min_pval > 0) ? e_ptr->min_pval : 1;
        }
    }
}

/*
 * Determines whether the weight of an item is eligible for modification.
 */
int wgt_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_ARROW:
    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
    case TV_HORN:
    {
        return (false);
    }
    }

    return (true);
}

/*
 * Determines the maximum legal weight for an item.
 */
int wgt_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 2) * 3;
    return (weight);
}

/*
 * Determines the minimum legal weight for an item.
 */
int wgt_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 3) * 2;
    return (weight);
}

bool melt_metal_item(int item_num)
{
    if (!tutorial_game_action_allowed("smith", NULL)) return false;
    int number = 0;
    u32b f1, f2, f3;
    int total = player_pack_entry_count() + (INVEN_TOTAL - INVEN_WIELD);

    for (int ordinal = 0; ordinal < total; ordinal++)
    {
        int item = ordinal < player_pack_entry_count()
            ? player_pack_entry_handle_at(ordinal)
            : INVEN_WIELD + ordinal - player_pack_entry_count();
        object_type* o_ptr = player_inventory_object(item);

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Skip metal items that can't be melted (Gamil-forged) */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON)) && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            number += 1;
        }

        if (number == item_num)
        {
            {
                char o_name[80];
                char prompt[160];

                object_desc(o_name, sizeof(o_name), o_ptr, false, 4);
                strnfmt(prompt, sizeof(prompt), ui_text("Melt %s down? "), o_name);
                if (!get_check(prompt))
                    return (false);
            }

            /* Browsing or declining a melt must retain interrupted work.
             * Only the confirmed conversion abandons that work. */
            p_ptr->smithing_leftover = 0;

            {
                int slot;
                object_type* i_ptr;
                object_type object_type_body;
                int metal_sval;

                // Determine which metal type to create
                if (f3 & TR3_STAR_IRON)
                    metal_sval = SV_METAL_STAR_IRON;
                else
                    metal_sval = SV_METAL_MITHRIL;

                // Get local object
                i_ptr = &object_type_body;

                // Prepare the base object for the metal
                object_prep(i_ptr, lookup_kind(TV_METAL, metal_sval));

                // set the appropriate quantity
                i_ptr->number = o_ptr->weight;

                // remove the item
                inven_item_increase(item, -1);
                inven_item_describe(item);
                inven_item_optimize(item);
                window_stuff();

                // give the mithril to the player...

                // if there is too much, then break it up
                while (i_ptr->number > 99)
                {
                    object_type* i_ptr2;
                    object_type object_type_body2;

                    // Get local object
                    i_ptr2 = &object_type_body2;

                    // decrease the main stack
                    i_ptr->number -= 99;

                    // Prepare the base object for the metal
                    object_prep(
                        i_ptr2, lookup_kind(TV_METAL, metal_sval));

                    // increase the new stack
                    i_ptr2->number = 99;

                    // give it to the player
                    slot = inven_carry(i_ptr2, true);
                    if (player_inventory_handle_is_carried(slot))
                    {
                        inven_item_optimize(slot);
                        inven_item_describe(slot);
                    }
                    else
                    {
                        drop_near(i_ptr2, 0, p_ptr->py, p_ptr->px);
                        msg_print("Some metal falls to the floor.");
                    }
                    window_stuff();
                }

                // now give the last stack of mithril to the player
                slot = inven_carry(i_ptr, true);
                if (player_inventory_handle_is_carried(slot))
                {
                    inven_item_optimize(slot);
                    inven_item_describe(slot);
                }
                else
                {
                    drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
                    msg_print("Some metal falls to the floor.");
                }
                window_stuff();

                return (true);
            }
        }
    }

    return (false);
}

static int meltable_metal_items_carried(void)
{
    int number = 0;
    u32b f1, f2, f3;
    int total = player_pack_entry_count() + (INVEN_TOTAL - INVEN_WIELD);

    for (int ordinal = 0; ordinal < total; ordinal++)
    {
        int item = ordinal < player_pack_entry_count()
            ? player_pack_entry_handle_at(ordinal)
            : INVEN_WIELD + ordinal - player_pack_entry_count();
        object_type* o_ptr = player_inventory_object(item);

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Only count metal items that can be melted (exclude Gamil-forged) */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            number += 1;
        }
    }

    return (number);
}

static int metal_carried(byte sval)
{
    int w = 0;
    int item;

    for (item = 0; item < player_pack_entry_count(); item++)
    {
        object_type* o_ptr = player_pack_entry_at(item);

        if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == sval))
        {
            w += o_ptr->number;
        }
    }

    return (w);
}

static bool smith_melt_item_is_eligible(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags(o_ptr, &f1, &f2, &f3);
    (void)f1;
    (void)f2;
    return (f3 & (TR3_MITHRIL | TR3_STAR_IRON))
        && !(o_ptr->ident & IDENT_CANT_MELT);
}

static int smith_melt_item_handle_for_choice(int choice)
{
    int number = 0;
    int total = player_pack_entry_count()
        + (INVEN_TOTAL - INVEN_WIELD);

    if (choice < 1)
        return -1;

    for (int ordinal = 0; ordinal < total; ordinal++)
    {
        int item = ordinal < player_pack_entry_count()
            ? player_pack_entry_handle_at(ordinal)
            : INVEN_WIELD + ordinal - player_pack_entry_count();
        object_type* o_ptr = player_inventory_object(item);

        if (!smith_melt_item_is_eligible(o_ptr))
            continue;

        number++;
        if (number == choice)
            return item;
    }

    return -1;
}

int mithril_carried(void)
{
    return metal_carried(SV_METAL_MITHRIL);
}

int star_iron_carried(void)
{
    return metal_carried(SV_METAL_STAR_IRON);
}

static void use_metal(byte sval, int cost)
{
    while (cost > 0)
    {
        int item = -1;
        object_type* o_ptr = NULL;

        for (int ordinal = player_pack_entry_count() - 1; ordinal >= 0;
             ordinal--)
        {
            object_type* candidate = player_pack_entry_at(ordinal);

            if (candidate->tval == TV_METAL && candidate->sval == sval)
            {
                item = player_pack_entry_handle_at(ordinal);
                o_ptr = candidate;
                break;
            }
        }

        if (!o_ptr)
            break;

        int use = MIN(o_ptr->number, cost);
        inven_item_increase(item, -use);
        inven_item_describe(item);
        inven_item_optimize(item);
        cost -= use;
    }
}

void use_mithril(int cost)
{
    use_metal(SV_METAL_MITHRIL, cost);
}

void use_star_iron(int cost)
{
    use_metal(SV_METAL_STAR_IRON, cost);
}

/*
 * Determines how many uses are left for a given forge.
 */
int forge_uses(int y, int x)
{
    byte feat = cave_feat[y][x];

    if (!cave_forge_bold(y, x))
        return (0);

    if (feat <= FEAT_FORGE_NORMAL_TAIL)
        return (feat - FEAT_FORGE_NORMAL_HEAD);
    if (feat <= FEAT_FORGE_GOOD_TAIL)
        return (feat - FEAT_FORGE_GOOD_HEAD);
    else
        return (feat - FEAT_FORGE_UNIQUE_HEAD);
}

static void spend_forge_uses(int uses)
{
    int y = p_ptr->py, x = p_ptr->px;
    cave_feat[y][x] -= uses;
    if (!forge_uses(y, x))
        cave_events_terrain_changed();
}

/*
 * Determines how high a bonus is provided by a given forge.
 */
int forge_bonus(int y, int x)
{
    byte feat = cave_feat[y][x];

    if (!cave_forge_bold(y, x))
        return (0);

    if (feat <= FEAT_FORGE_NORMAL_TAIL)
        return (0);
    if (feat <= FEAT_FORGE_GOOD_TAIL)
        return (3);
    else
        return (7);
}

static int smithing_capacity(void)
{
    return p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);
}

/* Difficulty comes exclusively from the shared item evaluator. This layer
 * collects the existing resource fees and explicit smith-specific benefits. */
int object_difficulty(object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx) return 0;
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    object_type intrinsic = *o_ptr;
    u32b f1, f2, f3, f4;
    int adjustment = 0;
    u32b hero = c_info[p_ptr->pcharacter].flags_u;

    memset(&smithing_cost, 0, sizeof(smithing_cost));
    smithing_cost.uses = 1;
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* The optional alloy's combat benefit is supplied by the paid material.
     * Value the same unalloyed blueprint as before; the finished item retains
     * its full intrinsic value in descriptions and drop generation. */
    if (o_ptr == smith_o_ptr)
    {
        intrinsic.att -= smith_alloy.bonus_att;
        intrinsic.evn -= smith_alloy.bonus_evn;
        intrinsic.ds -= smith_alloy.bonus_ds;
        intrinsic.ps -= smith_alloy.bonus_ps;
    }

    if ((hero & UNQ_SMT_TELCHAR)
        && ((f1 & (TR1_SHARPNESS | TR1_SHARPNESS2)) || (f3 & TR3_ACCURATE)))
        adjustment -= 25;
    if (hero & UNQ_SMT_FEANOR)
    {
        if (o_ptr->tval == TV_LIGHT) adjustment -= 40;
        else if ((f1 & TR1_BRAND_FIRE) || (f2 & (TR2_LIGHT | TR2_RADIANCE)))
            adjustment -= 25;
    }
    if ((hero & UNQ_SMT_CELEBRIMBOR) && o_ptr->tval == TV_RING)
    {
        adjustment -= 20; /* Rings cease to be minor slots. */
        if (!(f3 & TR3_ENCHANTABLE)) adjustment -= 30;
    }
    int dif = object_difficulty_adjusted(&intrinsic, adjustment);

    /* Keep the established resource-fee policy for intrinsic base powers.
     * This mask affects payments only, never the shared difficulty value. */
    if (o_ptr->tval != TV_HORN && o_ptr->tval != TV_RING && o_ptr->tval != TV_AMULET)
    {
        f1 = (f1 & ~k_ptr->flags1) | (k_ptr->flags1
            & (TR1_TUNNEL | TR1_STL | TR1_SHARPNESS | TR1_SHARPNESS2 | TR1_DAMAGE_SIDES));
        f2 = (f2 & ~k_ptr->flags2) | (k_ptr->flags2
            & (TR2_REGEN | TR2_RES_COLD | TR2_RES_FIRE));
        f3 = (f3 & ~k_ptr->flags3) | (k_ptr->flags3
            & (TR3_ACCURATE | TR3_CHEAT_DEATH | TR3_STAND_FAST | TR3_ENCHANTABLE));
    }
    if (o_ptr->tval == TV_HORN)
    {
        switch (o_ptr->sval)
        {
            case SV_HORN_TERROR: ++smithing_cost.gra; break;
            case SV_HORN_THUNDER: ++smithing_cost.dex; break;
            case SV_HORN_FORCE: ++smithing_cost.str; break;
            case SV_HORN_BLASTING: ++smithing_cost.con; break;
        }
    }
    if (f1 & TR1_BRAND_COLD) smithing_cost.str += 2;
    if (f1 & TR1_BRAND_FIRE) smithing_cost.str += 2;
    if (f1 & TR1_BRAND_POIS) smithing_cost.str += o_ptr->tval == TV_ARROW ? 1 : 2;
    if (f1 & TR1_BRAND_ELEC) smithing_cost.str += 2;
    if (f1 & TR1_SHARPNESS) smithing_cost.str += o_ptr->tval == TV_ARROW ? 1 : 2;
    if (f1 & TR1_SHARPNESS2) smithing_cost.str += 4;
    if (f1 & TR1_VAMPIRIC) ++smithing_cost.str;
    if (f3 & TR3_ACCURATE) ++smithing_cost.dex;
    if (f1 & TR1_TUNNEL) smithing_cost.str += MAX(0, o_ptr->pval - k_ptr->pval);
    if ((f1 & TR1_DAMAGE_SIDES) && o_ptr->pval > 0) smithing_cost.str += o_ptr->pval;
    smithing_cost.str += MAX(0, o_ptr->stat_bonus[A_STR]);
    smithing_cost.dex += MAX(0, o_ptr->stat_bonus[A_DEX]);
    smithing_cost.con += MAX(0, o_ptr->stat_bonus[A_CON]);
    smithing_cost.gra += MAX(0, o_ptr->stat_bonus[A_GRA]);
    if (f2 & TR2_RADIANCE) ++smithing_cost.gra;
    if (f2 & TR2_LIGHT) ++smithing_cost.gra;
    if (f2 & TR2_SPEED) smithing_cost.con += 5;
    if (f3 & TR3_CHEAT_DEATH) ++smithing_cost.con;
    for (int i = 0; i < o_ptr->abilities; ++i)
        smithing_cost.exp += 50 * b_info[ability_index(
            o_ptr->skilltype[i], o_ptr->abilitynum[i])].level;
    if (o_ptr->name1)
        smithing_cost.uses += (hero & UNQ_SMT_FEANOR) ? 1 : 2;

    if (k_ptr->flags3 & TR3_MITHRIL) smithing_cost.mithril += o_ptr->weight;
    if (k_ptr->flags3 & TR3_STAR_IRON) smithing_cost.star_iron += o_ptr->weight;
    if (smith_alloy.type != SMITH_ALLOY_NONE)
    {
        int alloy_weight = smith_alloy_weight_required(o_ptr);
        smithing_cost.alloy_weight = alloy_weight;
        smithing_cost.alloy_metal = smith_alloy.type;
        if (smith_alloy.type == SMITH_ALLOY_MITHRIL) smithing_cost.mithril += alloy_weight;
        else if (smith_alloy.type == SMITH_ALLOY_STAR_IRON) smithing_cost.star_iron += alloy_weight;
    }
    if ((hero & UNQ_SMT_GAMIL) && (k_ptr->flags3 & TR3_MITHRIL)
        && mithril_carried() < smithing_cost.mithril)
    {
        smithing_cost.uses = MAX(smithing_cost.uses, 3);
        smithing_cost.mithril = 0;
        o_ptr->ident |= IDENT_CANT_MELT;
    }

    int effective_skill = smithing_capacity();
    if (p_ptr->have_ability[S_SPC][SPC_AULE] && dif > effective_skill)
    {
        int maximum = effective_skill + p_ptr->skill_base[S_SMT] * 2;
        smithing_cost.drain = dif <= maximum ? (dif - effective_skill + 1) / 2
            : p_ptr->skill_base[S_SMT] + dif - maximum;
    }
    else if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE] && dif > effective_skill)
    {
        int maximum = effective_skill + p_ptr->skill_base[S_SMT];
        smithing_cost.drain = dif <= maximum ? dif - effective_skill
            : p_ptr->skill_base[S_SMT] + dif - maximum;
    }

    bool needs_alloy_mastery = (k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
        || smith_alloy.type != SMITH_ALLOY_NONE;
    int cat = smith_item_category(o_ptr);
    smithing_cost.weaponsmith = cat == CAT_WEAPON && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH];
    smithing_cost.armoursmith = cat == CAT_ARMOUR && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH];
    smithing_cost.jeweller = cat == CAT_JEWELRY && !p_ptr->active_ability[S_SMT][SMT_JEWELLER];
    smithing_cost.artifice = o_ptr->name1 && !p_ptr->active_ability[S_SMT][SMT_ARTEFACT];
    smithing_cost.enchantment = object_has_ego(o_ptr) && !p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT];
    smithing_cost.alloy_mastery = needs_alloy_mastery && !p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY];
    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        smithing_cost.str = smithing_cost.dex = smithing_cost.con = smithing_cost.gra = 0;
        smithing_cost.exp = 0;
    }
    return dif;
}

/*
 * Determines whether an item is too difficult to make.
 */
int too_difficult(object_type* o_ptr)
{
    int ability = smithing_capacity();
    int dif = object_difficulty(o_ptr);

    if (p_ptr->have_ability[S_SPC][SPC_AULE]) {
        // Aule's Forge adds twice the invested ranks to item capacity.
        int max_aule_difficulty = ability + (p_ptr->skill_base[S_SMT] * 2);
        log_trace("ABILITY DEBUG: AulÃ«'s Forge too_difficult check - max possible: %d, difficulty: %d", max_aule_difficulty, dif);
        if (max_aule_difficulty >= dif)
            return (false);
        else
            return (true);
    } else if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]) {
        // Masterpiece adds the invested ranks to item capacity.
        ability += p_ptr->skill_base[S_SMT];
    }

    if (ability < dif)
        return (true);
    else
        return (false);
}

/* Snapshot the bill before returning a previewed object to its baseline. */
static int smith_measure_design(void)
{
    if (!smith_o_ptr->k_idx)
    {
        smith_ui_detail_valid = false;
        return 0;
    }
    int dif = object_difficulty(smith_o_ptr);
    smith_ui_detail_valid = true;
    smith_ui_detail_reforge = smith_pending_reforge();
    smith_ui_detail_paid = smith_ui_detail_reforge;
    smith_ui_detail_cost = smithing_cost;
    smith_ui_detail_turns = MAX(10, dif
        * (p_ptr->active_ability[S_SMT][SMT_EXPERTISE] ? 5 : 10));
    if (smith_ui_detail_paid)
    {
        memset(&smith_ui_detail_cost, 0, sizeof(smith_ui_detail_cost));
        smith_ui_detail_turns = p_ptr->smithing_leftover;
    }

    return dif;
}

/*
 * Checks whether you can pay the costs in terms of ability points and
 * experience needed to make the object.
 */
bool affordable(object_type* o_ptr)
{
    bool can_afford = true;

    // can't afford non-existant items
    if (o_ptr->tval == 0)
        return (false);
    if (object_has_evil_alignment(o_ptr))
        return (false);

    if (too_difficult(o_ptr))
        can_afford = false;
    if ((smithing_cost.str > 0)
        && (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
            < -5))
        can_afford = false;
    if ((smithing_cost.dex > 0)
        && (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
            < -5))
        can_afford = false;
    if ((smithing_cost.con > 0)
        && (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
            < -5))
        can_afford = false;
    if ((smithing_cost.gra > 0)
        && (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
            < -5))
        can_afford = false;
    if (smithing_cost.exp > p_ptr->new_exp)
        can_afford = false;
    if ((smithing_cost.mithril > 0)
        && (smithing_cost.mithril > mithril_carried()))
        can_afford = false;
    if ((smithing_cost.star_iron > 0)
        && (smithing_cost.star_iron > star_iron_carried()))
        can_afford = false;
    if (forge_uses(p_ptr->py, p_ptr->px) < smithing_cost.uses)
        can_afford = false;

    if (smithing_cost.weaponsmith || smithing_cost.armoursmith
        || smithing_cost.jeweller || smithing_cost.enchantment
        || smithing_cost.artifice || smithing_cost.alloy_mastery)
        can_afford = false;

    return (can_afford);
}

/*
 * Pay the costs in terms of ability points and experience needed to make the
 * object.
 */
void pay_costs()
{
    if (smithing_cost.str > 0)
        p_ptr->stat_drain[A_STR] -= smithing_cost.str;
    if (smithing_cost.dex > 0)
        p_ptr->stat_drain[A_DEX] -= smithing_cost.dex;
    if (smithing_cost.con > 0)
        p_ptr->stat_drain[A_CON] -= smithing_cost.con;
    if (smithing_cost.gra > 0)
        p_ptr->stat_drain[A_GRA] -= smithing_cost.gra;

    if (smithing_cost.exp > 0)
        p_ptr->new_exp -= smithing_cost.exp;
    if (smithing_cost.mithril > 0)
        use_mithril(smithing_cost.mithril);
    if (smithing_cost.star_iron > 0)
        use_star_iron(smithing_cost.star_iron);
    if (smithing_cost.uses > 0)
        spend_forge_uses(smithing_cost.uses);
    if (smithing_cost.drain > 0)
        p_ptr->skill_base[S_SMT] -= smithing_cost.drain;

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Set the redraw flag for everything */
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

typedef struct reforge_preview_type
{
    int scaled_difficulty;
    int raw_delta_difficulty;
    int turns;
    smithing_cost_type cost;
    bool affordable;
    bool needs_forge;
    bool needs_forge_resources;
    bool needs_reforging;
} reforge_preview_type;

static void smithing_cost_reset_local(smithing_cost_type* cost)
{
    if (!cost)
        return;

    memset(cost, 0, sizeof(*cost));
}

static bool smith_has_category_ability(const object_type* o_ptr)
{
    int cat;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    cat = smith_item_category(o_ptr);
    if ((cat == CAT_WEAPON) && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
        return false;
    if ((cat == CAT_ARMOUR) && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
        return false;
    if ((cat == CAT_JEWELRY) && !p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        return false;

    return true;
}

static bool object_has_evil_alignment(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f3;
    return (f4 & TR4_EVIL_ITEM) != 0;
}

static bool smith_has_alignment_conflict(const object_type* o_ptr,
    int prefix_idx, int suffix_idx)
{
    u32b f1, f2, f3, f4;
    bool has_noble;
    bool has_evil;

    if (!o_ptr)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f3;

    has_noble = ((f4 & TR4_NOBLE_ITEM) != 0);
    has_evil = ((f4 & TR4_EVIL_ITEM) != 0);

    if (prefix_idx > 0)
    {
        if (e_info[prefix_idx].flags4 & TR4_NOBLE_ITEM)
            has_noble = true;
        if (e_info[prefix_idx].flags4 & TR4_EVIL_ITEM)
            has_evil = true;
    }

    if (suffix_idx > 0)
    {
        if (e_info[suffix_idx].flags4 & TR4_NOBLE_ITEM)
            has_noble = true;
        if (e_info[suffix_idx].flags4 & TR4_EVIL_ITEM)
            has_evil = true;
    }

    return has_noble && has_evil;
}

static bool ego_forbids_prefix_combo(int e_idx)
{
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    return (e_info[e_idx].flags4 & TR4_NO_PREFIX) != 0;
}

static bool smith_ego_is_forbidden_affix(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return true;
    if (e_ptr->flags3 & (TR3_DAMAGED | TR3_NO_SMITHING))
        return true;
    if (e_ptr->flags4 & (TR4_JINX | TR4_EVIL_ITEM))
        return true;
    return false;
}

static bool smith_ego_matches_item_type(const object_type* o_ptr,
    const ego_item_type* e_ptr)
{
    int j;

    if (!o_ptr || !o_ptr->k_idx || !e_ptr)
        return false;

    for (j = 0; j < EGO_TVALS_MAX; j++)
    {
        if (o_ptr->tval != e_ptr->tval[j])
            continue;
        if (o_ptr->sval < e_ptr->min_sval[j])
            continue;
        if (o_ptr->sval > e_ptr->max_sval[j])
            continue;

        return true;
    }

    return false;
}

static bool smith_ego_can_apply_to_object(const object_type* o_ptr, int e_idx,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    ego_item_type* e_ptr;
    const char* raw_name;
    bool is_prefix;

    if (!o_ptr || !o_ptr->k_idx || e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    e_ptr = &e_info[e_idx];
    raw_name = e_name + e_ptr->name;
    is_prefix = ego_name_is_prefix(raw_name);

    if (selecting_prefix != is_prefix)
        return false;
    if (smith_ego_is_forbidden_affix(e_ptr))
        return false;
    if (!smith_ego_matches_item_type(o_ptr, e_ptr))
        return false;

    if (selecting_prefix)
    {
        if (ego_forbids_prefix_combo(fixed_suffix))
            return false;
        if (smith_has_alignment_conflict(o_ptr, e_idx, fixed_suffix))
            return false;
    }
    else
    {
        if ((fixed_prefix != 0) && ego_forbids_prefix_combo(e_idx))
            return false;
        if (smith_has_alignment_conflict(o_ptr, fixed_prefix, e_idx))
            return false;
    }

    return true;
}

static bool ego_prefix_can_apply_to_object(const object_type* o_ptr, int e_idx)
{
    return smith_ego_can_apply_to_object(o_ptr, e_idx, 0, 0, true);
}

static bool object_can_reforge_prefix_aux(
    const object_type* o_ptr, bool require_category_ability)
{
    int i;

    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (o_ptr->name1)
        return false;
    if (object_is_damaged_item(o_ptr))
        return false;
    if (object_has_evil_alignment(o_ptr))
        return false;
    if (is_smithed_by_player(o_ptr))
        return false;
    if (object_ego_prefix(o_ptr))
        return false;
    if (ego_forbids_prefix_combo((int)object_ego_suffix(o_ptr)))
        return false;
    if (require_category_ability && !smith_has_category_ability(o_ptr))
        return false;

    for (i = 1; i < z_info->e_max; i++)
    {
        if (ego_prefix_can_apply_to_object(o_ptr, i))
            return true;
    }

    return false;
}

static bool object_can_preview_reforge_prefix(const object_type* o_ptr)
{
    return object_can_reforge_prefix_aux(o_ptr, false);
}

static int find_reforge_target_item(void)
{
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;
        if (object_can_repair_damage(o_ptr)
            || object_can_preview_reforge_prefix(o_ptr))
            return i;
    }

    for (i = 0; i < player_carried_extra_entry_count(); i++)
    {
        object_type* o_ptr = player_carried_extra_entry_at(i);
        if (o_ptr && o_ptr->k_idx && (object_can_repair_damage(o_ptr)
                || object_can_preview_reforge_prefix(o_ptr)))
            return CARRIED_EXTRA_INDEX + i;
    }

    return -1;
}

static void smith_eval_object(const object_type* src, int* difficulty,
    smithing_cost_type* cost_out)
{
    object_type smith_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smithing_cost_type smithing_cost_backup = smithing_cost;

    if (!src || !src->k_idx)
        return;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(smith_o_ptr, src);
    smith_clear_alloy_state(&smith_alloy);

    if (difficulty)
        *difficulty = object_difficulty(smith_o_ptr);
    else
        (void)object_difficulty(smith_o_ptr);

    if (cost_out)
        *cost_out = smithing_cost;

    object_copy(smith_o_ptr, &smith_backup);
    smith_alloy = alloy_backup;
    smithing_cost = smithing_cost_backup;
}

/* Read the accepted blueprint's rating without changing its craft bill. */
int smithing_work_difficulty(void)
{
    object_type saved_object;
    smith_alloy_state saved_alloy;
    smithing_cost_type saved_cost;
    int difficulty;

    if (!smith_o_ptr || !smith_o_ptr->k_idx)
        return 0;
    saved_object = *smith_o_ptr;
    saved_alloy = smith_alloy;
    saved_cost = smithing_cost;
    difficulty = object_difficulty(smith_o_ptr);
    *smith_o_ptr = saved_object;
    smith_alloy = saved_alloy;
    smithing_cost = saved_cost;
    return difficulty;
}

static bool smith_reforge_difficulty_affordable(int difficulty, int* drain_out)
{
    int effective_skill = smithing_capacity();

    if (drain_out)
        *drain_out = 0;

    if (p_ptr->have_ability[S_SPC][SPC_AULE])
    {
        int max_aule_difficulty = effective_skill + (p_ptr->skill_base[S_SMT] * 2);

        if (difficulty <= effective_skill)
            return true;
        if (difficulty <= max_aule_difficulty)
        {
            if (drain_out)
                *drain_out = (difficulty - effective_skill + 1) / 2;
            return true;
        }
        if (drain_out)
            *drain_out = p_ptr->skill_base[S_SMT] + (difficulty - max_aule_difficulty);
        return false;
    }

    if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE])
    {
        int max_masterpiece_difficulty = effective_skill + p_ptr->skill_base[S_SMT];

        if (difficulty <= effective_skill)
            return true;
        if (difficulty <= max_masterpiece_difficulty)
        {
            if (drain_out)
                *drain_out = difficulty - effective_skill;
            return true;
        }
        if (drain_out)
            *drain_out = p_ptr->skill_base[S_SMT] + (difficulty - max_masterpiece_difficulty);
        return false;
    }

    return (difficulty <= effective_skill);
}

static void smithing_cost_delta_positive(const smithing_cost_type* before,
    const smithing_cost_type* after, smithing_cost_type* delta)
{
    smithing_cost_reset_local(delta);

    if (!before || !after || !delta)
        return;

    delta->str = MAX(0, after->str - before->str);
    delta->dex = MAX(0, after->dex - before->dex);
    delta->con = MAX(0, after->con - before->con);
    delta->gra = MAX(0, after->gra - before->gra);
    delta->exp = MAX(0, after->exp - before->exp);
    delta->mithril = MAX(0, after->mithril - before->mithril);
    delta->star_iron = MAX(0, after->star_iron - before->star_iron);
}

static bool reforge_preview_build(const object_type* source, int prefix_idx,
    reforge_preview_type* preview)
{
    object_type single_source;
    int before_diff = 0;
    int after_diff = 0;
    int turn_multiplier = 10;
    smithing_cost_type before_cost;
    smithing_cost_type after_cost;

    if (!source || !source->k_idx || !preview || prefix_idx <= 0)
        return false;

    object_copy(&single_source, source);
    single_source.number = 1;
    source = &single_source;

    memset(preview, 0, sizeof(*preview));
    smithing_cost_reset_local(&before_cost);
    smithing_cost_reset_local(&after_cost);

    smith_eval_object(source, &before_diff, &before_cost);

    object_copy(smith_o_ptr, source);
    object_set_ego_prefix(smith_o_ptr, prefix_idx);
    if (!object_apply_ego_affix(smith_o_ptr, prefix_idx, true))
        return false;

    smith_eval_object(smith_o_ptr, &after_diff, &after_cost);

    preview->raw_delta_difficulty = MAX(0, after_diff - before_diff);
    preview->scaled_difficulty = (preview->raw_delta_difficulty * 3 + 1) / 2;
    smithing_cost_delta_positive(&before_cost, &after_cost, &preview->cost);
    preview->cost.uses = 1;

    preview->affordable = smith_reforge_difficulty_affordable(
        preview->scaled_difficulty, &preview->cost.drain);

    if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
    {
        preview->needs_reforging = true;
        preview->affordable = false;
    }

    switch (smith_item_category(source))
    {
    case CAT_WEAPON:
        if (!p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
        {
            preview->cost.weaponsmith = 1;
            preview->affordable = false;
        }
        break;
    case CAT_ARMOUR:
        if (!p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
        {
            preview->cost.armoursmith = 1;
            preview->affordable = false;
        }
        break;
    case CAT_JEWELRY:
        if (!p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        {
            preview->cost.jeweller = 1;
            preview->affordable = false;
        }
        break;
    }

    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        preview->cost.str = 0;
        preview->cost.dex = 0;
        preview->cost.con = 0;
        preview->cost.gra = 0;
        preview->cost.exp = 0;
        turn_multiplier /= 2;
    }

    if ((preview->cost.str > 0)
        && (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - preview->cost.str
            < -5))
        preview->affordable = false;
    if ((preview->cost.dex > 0)
        && (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - preview->cost.dex
            < -5))
        preview->affordable = false;
    if ((preview->cost.con > 0)
        && (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - preview->cost.con
            < -5))
        preview->affordable = false;
    if ((preview->cost.gra > 0)
        && (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - preview->cost.gra
            < -5))
        preview->affordable = false;
    if (preview->cost.exp > p_ptr->new_exp)
        preview->affordable = false;
    if ((preview->cost.mithril > 0)
        && (preview->cost.mithril > mithril_carried()))
        preview->affordable = false;
    if ((preview->cost.star_iron > 0)
        && (preview->cost.star_iron > star_iron_carried()))
        preview->affordable = false;
    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        preview->needs_forge = true;
        preview->affordable = false;
    }
    else if (forge_uses(p_ptr->py, p_ptr->px) < preview->cost.uses)
    {
        preview->needs_forge_resources = true;
        preview->affordable = false;
    }
    if ((preview->cost.drain > 0)
        && (preview->cost.drain > p_ptr->skill_base[S_SMT]))
        preview->affordable = false;

    preview->turns = MAX(10, preview->scaled_difficulty * turn_multiplier);
    return true;
}

static void pay_smithing_cost_struct(const smithing_cost_type* cost)
{
    if (!cost)
        return;

    if (cost->str > 0)
        p_ptr->stat_drain[A_STR] -= cost->str;
    if (cost->dex > 0)
        p_ptr->stat_drain[A_DEX] -= cost->dex;
    if (cost->con > 0)
        p_ptr->stat_drain[A_CON] -= cost->con;
    if (cost->gra > 0)
        p_ptr->stat_drain[A_GRA] -= cost->gra;
    if (cost->exp > 0)
        p_ptr->new_exp -= cost->exp;
    if (cost->mithril > 0)
        use_mithril(cost->mithril);
    if (cost->star_iron > 0)
        use_star_iron(cost->star_iron);
    if (cost->uses > 0)
    {
        spend_forge_uses(cost->uses);
        lite_spot(p_ptr->py, p_ptr->px);
    }
    if (cost->drain > 0)
        p_ptr->skill_base[S_SMT] -= cost->drain;

    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

// Determine default stack sizes for smithing-created items.
// Normal: arrows 24/18/12, hand axes and spears 3/2/1, daggers 5/3/1
// (normal/enchanted/artefact).  This keeps arrows and throwable weapons in
// sensible stack counts.
static byte smith_default_stack_size(const object_type* o_ptr)
{
    bool is_arrow = (o_ptr->tval == TV_ARROW);
    bool is_spear = (o_ptr->tval == TV_POLEARM) && (o_ptr->sval == SV_SPEAR);
    bool is_hand_axe = (o_ptr->tval == TV_POLEARM) && (o_ptr->sval == SV_HAND_AXE);
    bool is_dagger = (o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_DAGGER);

    if (!(is_arrow || is_spear || is_hand_axe || is_dagger))
    {
        return (o_ptr->number ? o_ptr->number : 1);
    }

    bool is_artifact = (o_ptr->name1 != 0);
    bool is_enchanted = (!is_artifact) && object_has_ego(o_ptr);

    if (is_arrow)
    {
        if (is_artifact) return 12;
        if (is_enchanted) return 18;
        return 24;
    }

    if (is_artifact) return 1;
    if (is_enchanted) return is_dagger ? 3 : 2;
    return is_dagger ? 5 : 3;
}

/*
 * Creates the base object (not in the dungeon, but just as a work in progress).
 */
static void smith_prepare_base_object(object_type* object, int kind)
{
    object_wipe(object);
    smith_prepare_display_object(object, kind);
    apply_magic_fake(object);
    object->weight = k_info[kind].weight;
    object->ident |= (IDENT_KNOWN | IDENT_SPOIL);
    object->number = smith_default_stack_size(object);
}

void create_base_object(int tval, int sval)
{
    smith_clear_alloy_state(&smith_alloy);
    smith_prepare_base_object(smith_o_ptr, lookup_kind(tval, sval));
}

enum {
    SMITH_BROWSER_GROUP_COUNT = 3,
    SMITH_BROWSER_GROUP_CLICK = SMITH_CLICK_DESCRIPTION + 1,
    SMITH_BROWSER_MELT_CLICK = SMITH_BROWSER_GROUP_CLICK + SMITH_BROWSER_GROUP_COUNT,
    SMITH_BROWSER_REFORGE_CLICK,
    SMITH_BROWSER_INSPECT_CLICK,
    SMITH_BROWSER_DESIGN_CLICK,
};

enum {
    SMITH_EDITOR_TAB = 33100,
    SMITH_EDITOR_INSPECT = 33110,
    SMITH_EDITOR_COSTS,
    SMITH_EDITOR_CRAFT,
    SMITH_EDITOR_APPLY,
    SMITH_EDITOR_REMOVE,
    SMITH_EDITOR_RENAME,
};

typedef struct smith_picker_entry {
    int kind;
    char name[80];
} smith_picker_entry;

/* All views use the same piece of the terminal for the editable list,
 * the work order, and the action strip. Bounds are half-open. */
typedef struct {
    int width, margin, tabs, body, footer;
    int divider, summary;
    int list_width, list_end;
} smith_sheet_layout;

typedef struct {
    int columns, step, first, stride, visible, top;
} smith_picker_grid;

typedef struct {
    int id;
    cptr label;
    byte attr;
} smith_sheet_action;

static smith_sheet_layout smith_sheet_geometry(void)
{
    int width = smith_ui_term_wid();
    int footer = smith_ui_content_bottom_row();
    int margin = width >= 72 ? 3 : width >= 40 ? 2 : 1;
    int tabs = smith_ui_term_hgt() >= 19 ? 3 : 1;
    int body = tabs + (footer >= 19 ? 2 : 1);
    int space = MAX(1, footer - body);
    bool landscape = 2 * width >= 3 * smith_ui_term_hgt();
    int divider = landscape && width >= 68 && space >= 7 ? width * 56 / 100 : -1;
    /* Keep at least a heading and two choices above the compact work order. */
    int summary = divider >= 0 ? body
        : footer - MIN(MAX(1, space - 3), MAX(3, (space * 2) / 5));
    return (smith_sheet_layout){
        .width = width, .margin = margin, .tabs = tabs, .body = body,
        .footer = footer, .divider = divider, .summary = summary,
        .list_width = MAX(1, (divider >= 0 ? divider : width) - 2 * margin),
        .list_end = divider >= 0 ? footer : summary,
    };
}

/* Keep the work order adjacent to short lists on narrow screens. */
static void smith_sheet_fit_list(smith_sheet_layout* sheet, int count, int columns,
    int details)
{
    if (sheet->divider >= 0) return;
    int rows = MAX(1, (count + columns - 1) / columns);
    int space = sheet->summary - sheet->body - 1 - details;
    int stride = sdl_touch_only_device_active() && space >= 2 * rows ? 2 : 1;
    sheet->summary = sheet->list_end = MIN(sheet->summary,
        sheet->body + 1 + rows * stride + details);
}

static void smith_sheet_begin(const smith_sheet_layout* sheet, bool root)
{
    int x = sheet->width >= 72 ? 3 : 2;
    int y = sheet->tabs == 1 ? 0 : 1;
    bool forge = cave_forge_bold(p_ptr->py, p_ptr->px);
    cptr location = forge ? ui_text("At the forge") : ui_text("Away from forge");
    int status_x = sheet->divider >= 0 ? sheet->divider + sheet->margin : x + 19;

    smith_description.active = false;
    sdl_smithing_artwork_clear();
    Term_clear();
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_scroll_area_clear();
    if (root)
        ui_menu_click_set_touch_exit_button(true);
    sdl_smithing_sheet_set(sheet->tabs, sheet->body, sheet->divider,
        sheet->divider >= 0 ? -1 : sheet->summary, sheet->footer);
    smith_ui_put_fitted(x, y, status_x - x - 1, TERM_L_WHITE,
        ui_text("Smithing"));
    if (status_x + (int)strlen(location) + 17 <= sheet->width)
        smith_ui_put_fitted(status_x, y, (int)strlen(location),
            forge ? TERM_L_GREEN : TERM_L_WHITE, location);
}

static void smith_sheet_tabs(const smith_sheet_layout* sheet, const cptr* names,
    int count, int selected, int first_id)
{
    int letters = 0;
    for (int i = 0; i < count; i++) letters += (int)strlen(ui_text(names[i]));
    int available = sheet->width - 2 * sheet->margin;
    bool brackets = available >= letters + count + 1;
    int pad = MAX(0, MIN(2, (available - letters - count - 1) / (2 * count)));
    int x = sheet->margin;
    for (int i = 0; i < count; i++)
    {
        char label[32];
        if (i == selected && brackets)
            strnfmt(label, sizeof(label), "[%s]", ui_text(names[i]));
        else
            SDL_strlcpy(label, ui_text(names[i]), sizeof(label));
        int w = (int)strlen(label) + 2 * pad;
        smith_ui_draw_fitted(x + pad, sheet->tabs, w - pad,
            i == selected ? TERM_L_BLUE : TERM_L_WHITE, label, false);
        if (first_id > 0) ui_menu_click_add(first_id + i, x, sheet->tabs, w);
        x += w + 1;
    }
}

static void smith_sheet_actions(const smith_sheet_layout* sheet,
    const smith_sheet_action* actions, int count, int focus)
{
    if (sdl_touch_only_device_active())
    {
        for (int i = 0; i < count; i++)
        {
            if (actions[i].id != SMITH_CLICK_BACK)
                ui_menu_click_add_touch_button(actions[i].id, actions[i].label,
                    actions[i].attr == TERM_SLATE ? TERM_L_DARK : TERM_DARK);
        }
        if (!ui_menu_click_touch_exit_button_active())
            ui_menu_click_add_touch_button(SMITH_CLICK_BACK, ui_text("Back"), TERM_DARK);
        return;
    }

    int x = sheet->margin;
    int end = sheet->width - sheet->margin;
    int letters = 0;
    for (int i = 0; i < count; i++) letters += (int)strlen(actions[i].label);
    int pad = MAX(0, MIN(2, (end - x - letters - count + 1) / (2 * count)));
    for (int i = 0; i < count; i++)
    {
        int w = (int)strlen(actions[i].label) + 2 * pad;
        if (i == count - 1) x = MAX(x, end - w);
        byte attr = actions[i].id == focus ? TERM_UI_SELECTED + TERM_L_BLUE
            : actions[i].attr;
        if (attr >= TERM_UI_SELECTED) smith_ui_fill_row(x, sheet->footer, w, attr);
        smith_ui_draw_fitted(x + pad, sheet->footer, w - pad, attr,
            actions[i].label, false);
        ui_menu_click_add(actions[i].id, x, sheet->footer, w);
        x += w + 1;
    }
}

static void smith_sheet_navigation(const smith_sheet_layout* sheet, bool root,
    cptr confirm, bool pages)
{
    if (sdl_touch_only_device_active()) return;
    char text[128], back[16] = "Esc", accept[16] = "Enter", page_keys[40] = "[ ]";
    if (steamdeck_controls_active())
    {
        char prev[16], next[16];
        controller_prompt_label(steamdeck_back_key(), "B", back, sizeof(back));
        controller_prompt_label(steamdeck_confirm_key(), "A", accept, sizeof(accept));
        controller_prompt_label(steamdeck_prev_page_key(), "LB", prev, sizeof(prev));
        controller_prompt_label(steamdeck_next_page_key(), "RB", next, sizeof(next));
        strnfmt(page_keys, sizeof(page_keys), "%s/%s", prev, next);
    }
    strnfmt(text, sizeof(text), "%s %s", back, root ? ui_text("Exit") : ui_text("Back"));
    if (confirm)
    {
        size_t len = strlen(text);
        strnfcat(text, sizeof(text), &len, "   %s %s", accept, confirm);
    }
    if (pages && sheet->width >= 40)
    {
        size_t len = strlen(text);
        strnfcat(text, sizeof(text), &len, ui_text("   %s Tabs"), page_keys);
    }
    int row = smith_ui_prompt_row();
    Term_putstr(sheet->margin, row, sheet->width - sheet->margin, TERM_SLATE, text);
    ui_menu_click_add(SMITH_CLICK_BACK, sheet->margin, row,
        (int)strlen(back) + 5);
}

static void smith_sheet_list_scroll(const smith_sheet_layout* sheet,
    int first, int end)
{
    ui_scroll_area_begin_cols(sheet->margin, sheet->margin + sheet->list_width - 1,
        first, MAX(first, end - 1), SDL_TOUCH_MENU_CATEGORY_OTHER);
    ui_scroll_area_set_keys('8', '2', '6', '4');
}

/* One ruled entry, with the value aligned to the end of the same row. */
static void smith_sheet_field(int x, int row, int width, cptr label, cptr value,
    byte label_attr, byte value_attr)
{
    int label_min = MIN(menu_text_display_width(label), MAX(3, width / 2));
    int value_width = MIN(MAX(0, width - label_min - 1), menu_text_display_width(value));
    int label_width = MAX(0, width - value_width - (value_width != 0));
    smith_ui_draw_fitted(x, row, label_width, label_attr, label, false);
    smith_ui_draw_fitted(x + width - value_width, row, value_width, value_attr,
        value, false);
}

static bool smith_kind_is_browsable(const object_kind* kind)
{
    if ((kind->flags3 & TR3_INSTA_ART) || (kind->flags4 & TR4_EVIL_ITEM))
        return false;
    if (!(kind->flags3 & TR3_NO_SMITHING))
        return true;
    return (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_EOL)
        && kind->tval == TV_SOFT_ARMOR && kind->sval == SV_ARMOUR_OF_GALVORN;
}

static int smith_browser_entries(int group, smith_picker_entry* entries)
{
    int count = 0;
    /* Keep the established item type order, then the template order within it. */
    for (int i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        if (smithing_tvals[i].category != group)
            continue;

        for (int kind = 1; kind < z_info->k_max; kind++)
        {
            const object_kind* candidate = &k_info[kind];
            if (candidate->tval != smithing_tvals[i].tval
                || !smith_kind_is_browsable(candidate))
            {
                continue;
            }
            entries[count].kind = kind;
            if (candidate->tval == TV_RING || candidate->tval == TV_AMULET)
            {
                char name[80];
                strip_name(name, kind);
                strnfmt(entries[count].name, sizeof(entries[count].name),
                    "%s of %s", candidate->tval == TV_RING ? "Ring" : "Amulet",
                    name);
            }
            else
                strip_name(entries[count].name, kind);
            count++;
        }
    }
    return count;
}

static smith_picker_grid smith_sheet_grid(const smith_sheet_layout* sheet,
    int count, int columns, int first, int end)
{
    int rows = MAX(1, (count + columns - 1) / columns);
    int space = MAX(1, end - first);
    /* Preserve larger hit targets when the whole list fits with them. */
    int stride = sdl_touch_only_device_active() && space >= 2 * rows ? 2 : 1;
    return (smith_picker_grid){
        .columns = columns, .step = sheet->list_width / columns,
        .first = first, .stride = stride, .visible = MAX(1, space / stride)
    };
}

static void smith_sheet_open_list(const smith_sheet_layout* sheet,
    smith_picker_grid* grid, smith_ui_scroll_id id, int count, int focus)
{
    smith_sheet_list_scroll(sheet, grid->first,
        grid->first + grid->stride * grid->visible);
    grid->top = smith_ui_configure_list_view(id,
        (count + grid->columns - 1) / grid->columns,
        focus / grid->columns + 1, 0, grid->visible - 1);
}

static int smith_sheet_list_first(const smith_sheet_layout* sheet)
{
    return sheet->body + 1;
}

static void smith_sheet_heading(const smith_sheet_layout* sheet, cptr title,
    const smith_picker_grid* grid, int count)
{
    char range[32] = "";
    if (grid && grid->visible * grid->columns < count)
        strnfmt(range, sizeof(range), "%d-%d/%d", grid->top * grid->columns + 1,
            MIN(count, (grid->top + grid->visible) * grid->columns), count);
    smith_sheet_field(sheet->margin, sheet->body, sheet->list_width, title,
        range, TERM_WHITE + TERM_SHADE, TERM_SLATE);
}

static void smith_sheet_choice(const smith_sheet_layout* sheet,
    const smith_picker_grid* grid, int index, int id, int shortcut, cptr label,
    cptr value, const object_type* icon, bool selected, byte normal)
{
    int x = sheet->margin + (index % grid->columns) * grid->step;
    int row = grid->first + (index / grid->columns - grid->top) * grid->stride;
    int w = grid->step - (grid->columns > 1);
    byte attr = selected ? TERM_UI_SELECTED + TERM_L_BLUE
        : normal == TERM_L_DARK ? TERM_SLATE : normal;
    int inset = w >= 16 ? 1 : 0;
    int col = x + inset;

    /* A single uninterrupted hit area includes the sprite, label and value. */
    for (int dy = 0; dy < grid->stride; dy++)
        ui_menu_click_add(id, x, row + dy, w);
    if (selected) smith_ui_fill_row(x, row, w, attr);
    if (sdl_menu_letters_enabled() && shortcut >= 0 && shortcut < 26)
    {
        char key[4] = { (char)('a' + shortcut), ')', 0 };
        smith_ui_draw_fitted(col, row, 2, selected ? attr : TERM_SLATE, key, false);
        col += 3;
    }
    if (icon)
    {
        draw_supply_icon(col, row, icon);
        col += use_bigtile ? 3 : 2;
    }
    smith_sheet_field(col, row, MAX(1, x + w - inset - col), label, value,
        attr, selected ? attr : (normal == TERM_L_DARK || normal == TERM_SLATE
            ? TERM_L_WHITE : TERM_L_BLUE));
}

/* Wrap the full paragraph for both the work order and the reading view. */
static cptr smith_description_heading(void)
{
    return smith_description.effects ? ui_text("Effects") : ui_text("Description");
}

static int smith_description_viewport(int x, int row, int width, int end,
    int* top, bool inline_order)
{
    int rows = MAX(0, end - row - 1);
    char line[180], range[40] = "";
    cptr text = smith_description.text;
    int wrap = smith_ui_safe_width(x, MIN(width, 72));
    int count = 0;
    while (smith_ui_wrap_next(&text, wrap, line, sizeof(line))) count++;
    int max_top = rows > 0 ? MAX(0, count - rows) : 0;
    *top = MAX(0, MIN(*top, max_top));

    if (max_top)
        strnfmt(range, sizeof(range), "%d-%d/%d%s", *top + 1, *top + rows, count,
            inline_order && !sdl_touch_only_device_active() && !steamdeck_controls_active() ? " [D]" : "");
    else if (inline_order && !rows)
        SDL_strlcpy(range, sdl_touch_only_device_active() ? ui_text("Read") : ui_text("Read [D]"), sizeof(range));

    smith_sheet_field(x, row, width, smith_description_heading(), range,
        TERM_WHITE + TERM_SHADE, TERM_L_BLUE);
    if (inline_order)
        ui_menu_click_add(SMITH_CLICK_DESCRIPTION, x, row, width);
    if (max_top && ui_scroll_area_add_cols(x, x + width - 1, row, end - 1,
            SDL_TOUCH_MENU_CATEGORY_OTHER))
    {
        ui_scroll_area_set_keys('8', '2', 0, 0);
        ui_scroll_area_set_offset_target(top, max_top);
    }

    text = smith_description.text;
    for (int i = 0; i < *top + rows && smith_ui_wrap_next(&text, wrap, line, sizeof(line)); i++)
        if (i >= *top)
            smith_ui_put_fitted(x, row + 1 + i - *top, wrap, TERM_WHITE, line);
    return max_top;
}

static void smith_show_description(void)
{
    enum { DESC_PREVIOUS = 33410, DESC_NEXT };
    int top = 0;
    screen_save();
    for (;;)
    {
        smith_sheet_layout sheet = smith_sheet_geometry();
        sheet.divider = -1;
        sheet.summary = sheet.footer;
        smith_sheet_begin(&sheet, false);
        int width = sheet.width - 2 * sheet.margin;
        smith_ui_put_fitted(sheet.margin, sheet.tabs, width, TERM_L_BLUE, smith_ui_detail_name);
        int max_top = smith_description_viewport(sheet.margin, sheet.body, width,
            sheet.footer, &top, false);
        smith_sheet_action actions[3];
        int count = 0;
        if (max_top)
        {
            actions[count++] = (smith_sheet_action){ DESC_PREVIOUS, ui_text("Previous"), top ? TERM_WHITE : TERM_SLATE };
            actions[count++] = (smith_sheet_action){ DESC_NEXT, ui_text("Next"), top < max_top ? TERM_WHITE : TERM_SLATE };
        }
        actions[count++] = (smith_sheet_action){ SMITH_CLICK_BACK, ui_text("Back"), TERM_L_BLUE };
        smith_sheet_actions(&sheet, actions, count, 0);
        smith_sheet_navigation(&sheet, false, NULL, false);
        Term_fresh();
        int key = smith_sheet_read_key('9', '3');
        int id, action;
        if (ui_menu_click_take_action(&id, &action))
        {
            if (action == UI_MENU_CLICK_HOVER) continue;
            if (id == SMITH_CLICK_BACK || action == UI_MENU_CLICK_SECONDARY) break;
            if (id == DESC_PREVIOUS) key = '9';
            if (id == DESC_NEXT) key = '3';
        }
        if (key == ESCAPE || key == '\r' || key == '\n' || key == ' ' || key == 'D') break;
        int page = MAX(1, sheet.footer - sheet.body - 2);
        if (key == '8') top--;
        if (key == '2') top++;
        if (key == '9' || key == '4') top -= page;
        if (key == '3' || key == '6') top += page;
    }
    sdl_smithing_sheet_clear();
    ui_menu_click_clear();
    ui_scroll_area_clear();
    screen_load();
}

typedef enum {
    SMITH_NAME_READ_ONLY, SMITH_NAME_EDITABLE, SMITH_NAME_FOCUSED
} smith_name_mode;

static void smith_append_volume_effect(char* text, size_t size, int from, int to)
{
    if (from == to) return;
    size_t used = strlen(text);
    strnfcat(text, size, &used, "%s%s %d.%d → %d.%d qt.", used ? " " : "",
        ui_text("Volume per item"), from / 10, from % 10, to / 10, to % 10);
}

static void smith_sheet_order_with_effects(const smith_sheet_layout* sheet,
    const object_type* item, int difficulty, bool continuing, bool draft,
    smith_name_mode name_mode, cptr effects)
{
    const int x = (sheet->divider >= 0 ? sheet->divider : 0) + sheet->margin;
    int w = sheet->width - sheet->margin - x;
    int y = sheet->summary;
    int space = sheet->footer - y;
    int weight = item->weight * item->number;
    char desc[160], name[160], weight_text[32], stats[150];
    int skill = smithing_capacity();
    int uses = forge_uses(p_ptr->py, p_ptr->px);
    if (continuing) smith_ui_detail_turns = p_ptr->smithing_leftover;
    int turns = smith_ui_detail_turns;
    const smithing_cost_type* cost = &smith_ui_detail_cost;

    object_desc(name, sizeof(name), item, item->number > 1, 0);
    object_desc(desc, sizeof(desc), item, item->number > 1, 2);
    cptr suffix = desc + strlen(name);
    while (*suffix == ' ') suffix++;
    SDL_strlcpy(smith_ui_detail_name, name, sizeof(smith_ui_detail_name));
    char text_buf[sizeof(smith_description.text)];
    if (!effects && (item->name1 || object_has_ego(item)))
    {
        object_info_effect_text(item, NULL, text_buf, sizeof(text_buf));
        if (object_has_ego(item) && item->storage != OBJECT_STORAGE_NONE)
            smith_append_volume_effect(text_buf, sizeof(text_buf), item->volume,
                object_effective_volume(item));
        if (*text_buf) effects = text_buf;
    }
    cptr description = effects ? effects
        : object_lore_select_base_text(item, text_buf, sizeof(text_buf));
    if (!description) description = "";
    if (strcmp(smith_description.text, description) != 0)
    {
        SDL_strlcpy(smith_description.text, description, sizeof(smith_description.text));
        smith_description.top = 0;
    }
    smith_description.active = *description != '\0';
    smith_description.effects = effects != NULL;
    strnfmt(weight_text, sizeof(weight_text), ui_text("%d.%d lb"), weight / 10, weight % 10);
    smith_ui_build_object_stats(stats, sizeof(stats), suffix, weight_text);
    if (space >= 6)
        smith_sheet_field(x, y++, w, ui_text("Work order"), draft ? ui_text("Draft") : "",
            TERM_WHITE + TERM_SHADE, TERM_L_BLUE);
    int art_rows = space >= 8 ? 2 : 1;
    int art_cols = art_rows * 2;
    if (!sdl_smithing_artwork_set(item, x, y, art_cols, art_rows))
        draw_supply_icon(x, y, item);
    int text_x = x + art_cols + 1;
    int text_w = w - art_cols - 1;
    int name_row = y;
    byte name_attr = item->name1 ? TERM_YELLOW : object_has_ego(item) ? TERM_L_BLUE : TERM_WHITE;
    if (name_mode == SMITH_NAME_EDITABLE) name_attr = TERM_L_BLUE;
    if (name_mode == SMITH_NAME_FOCUSED) name_attr = TERM_UI_SELECTED + TERM_L_BLUE;
    if (space < 6 && draft)
        smith_sheet_field(text_x, y++, text_w, name, ui_text("Draft"), name_attr, TERM_L_BLUE);
    else
    {
        int name_rows = smith_ui_put_wrapped(text_x, y, text_w,
            space >= 10 ? 2 : 1, name_attr, name);
        y += MAX(1, name_rows);
    }
    if (name_mode != SMITH_NAME_READ_ONLY)
        for (int row = name_row; row < y; row++)
            ui_menu_click_add(SMITH_EDITOR_RENAME, text_x, row, text_w);
    if (space >= 6)
        smith_ui_put_fitted(text_x, y++, text_w, TERM_L_BLUE, stats);

    /* Keep the core bill even in the smallest work order. */
    int measure = MIN(w, 43);
    byte difficulty_attr = difficulty > skill ? TERM_L_RED : TERM_L_GREEN;
    byte uses_attr = !smith_ui_detail_paid && cost->uses > uses ? TERM_L_RED : TERM_YELLOW;
    if (y < sheet->footer)
        smith_sheet_field(x, y++, measure, ui_text("Difficulty"), format("%d / %d", difficulty, skill),
            TERM_WHITE, difficulty_attr);
    if (space <= 6 && y < sheet->footer)
    {
        int left = measure / 2;
        smith_sheet_field(x, y, left - 1, ui_text("Uses"), smith_ui_detail_paid ? ui_text("Paid") : format("%d/%d", cost->uses, uses),
            TERM_WHITE, uses_attr);
        smith_sheet_field(x + left, y++, measure - left, ui_text("Turns"), format("%d", turns),
            TERM_WHITE, TERM_VIOLET);
    }
    else if (y + 1 < sheet->footer)
    {
        smith_sheet_field(x, y++, measure, ui_text("Forge uses"), smith_ui_detail_paid ? ui_text("Paid")
            : format("%d / %d", cost->uses, uses), TERM_WHITE, uses_attr);
        smith_sheet_field(x, y++, measure, ui_text("Turns"), format("%d", turns), TERM_WHITE, TERM_VIOLET);
    }

    int room = sheet->footer - y;
    int detail_rows = smith_description.effects ? 5 : room >= 4 ? 3 : 2;
    int extra_end = sheet->footer - (smith_description.active
        ? MIN(room, detail_rows) : 0);
    smith_bill_line extra[SMITH_BILL_EXTRAS_MAX];
    int n = smith_bill_extras(cost, smith_ui_detail_reforge && !smith_ui_detail_paid
        && !p_ptr->active_ability[S_SMT][SMT_REPAIR], extra);
    for (int i = 0; i < n && y < extra_end; i++, y++)
    {
        if (y == extra_end - 1 && i + 1 < n)
        {
            smith_ui_put_fitted(x, y, measure, TERM_L_BLUE, ui_text("See full bill"));
            ui_menu_click_add(SMITH_CLICK_CALC, x, y++, measure);
            break;
        }
        smith_sheet_field(x, y, measure, extra[i].label, extra[i].value,
            TERM_WHITE, extra[i].requirement ? TERM_L_RED : TERM_YELLOW);
    }
    if (smith_description.active && y < sheet->footer)
    {
        smith_description.rows = sheet->footer - y - 1;
        smith_description.max_top = smith_description_viewport(x, y, w,
            sheet->footer, &smith_description.top, true);
    }
    else if (smith_description.active)
    {
        cptr heading = smith_description_heading();
        cptr label = sdl_touch_only_device_active() ? heading : format("[D] %s", heading);
        int length = (int)strlen(label);
        int col = sheet->width - sheet->margin - length;
        int row = sheet->tabs == 1 ? 0 : 1;
        smith_description.rows = smith_description.max_top = 0;
        smith_ui_put_fitted(col, row, length, TERM_L_BLUE, label);
        ui_menu_click_add(SMITH_CLICK_DESCRIPTION, col, row, length);
    }
}

static void smith_sheet_order(const smith_sheet_layout* sheet,
    const object_type* item, int difficulty, bool continuing, bool draft,
    smith_name_mode name_mode)
{
    smith_sheet_order_with_effects(sheet, item, difficulty, continuing, draft,
        name_mode, NULL);
}

static void smith_sheet_stock(int x, int row, int width, int end)
{
    if (row + 3 >= end) return;
    smith_ui_put_fitted(x, row++, width, TERM_WHITE + TERM_SHADE, ui_text("On hand"));
    if (row + 4 < end) row++;
    int mithril = mithril_carried(), iron = star_iron_carried();
    smith_sheet_field(x, row++, width, "Mithril", format(ui_text("%d.%d lb"), mithril / 10,
        mithril % 10), TERM_WHITE, TERM_L_BLUE);
    smith_sheet_field(x, row++, width, ui_text("Star iron"), format(ui_text("%d.%d lb"), iron / 10,
        iron % 10), TERM_WHITE, TERM_VIOLET);
    smith_sheet_field(x, row, width, ui_text("Experience"), format("%ld", (long)p_ptr->new_exp),
        TERM_WHITE, TERM_YELLOW);
}

static int smith_complete_bill(const smithing_cost_type* cost, bool reforge,
    int turns, smith_bill_line* bill)
{
    bill[0] = (smith_bill_line){ .label = ui_text("Forge uses") };
    strnfmt(bill[0].value, sizeof(bill[0].value), "%d", cost->uses);
    int n = 1 + smith_bill_extras(cost, reforge, bill + 1);
    bill[n] = (smith_bill_line){ .label = ui_text("Turns") };
    strnfmt(bill[n].value, sizeof(bill[n].value), "%d", turns);
    return n + 1;
}

/* Browsing a base or a provisional value may already have restored the live
 * object. This read-only sheet uses the item name and bill of that preview. */
static void smith_show_cost_details(void)
{
    enum { BILL_PREVIOUS = 33400, BILL_NEXT };
    smith_bill_line bill[SMITH_BILL_EXTRAS_MAX + 2];
    int count = smith_ui_detail_valid ? smith_complete_bill(&smith_ui_detail_cost,
        smith_ui_detail_reforge && !smith_ui_detail_paid, smith_ui_detail_turns, bill) : 0;
    int top = 0;
    screen_save();
    for (;;)
    {
        smith_sheet_layout sheet = smith_sheet_geometry();
        if (sheet.divider < 0) sheet.summary = sheet.list_end = sheet.footer;
        smith_sheet_begin(&sheet, false);
        cptr name = count && smith_ui_detail_name[0] ? smith_ui_detail_name : ui_text("Work order");
        smith_ui_put_fitted(sheet.margin, sheet.tabs, sheet.width - 2 * sheet.margin,
            TERM_L_BLUE, name);
        int first = smith_sheet_list_first(&sheet);
        if (count && smith_ui_detail_paid && first + 1 < sheet.list_end)
            smith_ui_put_fitted(sheet.margin, first++, sheet.list_width, TERM_L_BLUE,
                ui_text("Costs already paid."));
        smith_picker_grid grid = smith_sheet_grid(&sheet, count, 1, first, sheet.list_end);
        int max_top = MAX(0, count - grid.visible);
        top = MAX(0, MIN(top, max_top));
        grid.top = top;
        smith_sheet_list_scroll(&sheet, first, sheet.list_end);
        ui_scroll_area_set_offset_target(&top, max_top);
        ui_scroll_area_set_indicator(top, max_top);
        smith_sheet_heading(&sheet, ui_text("COMPLETE COSTS"), &grid, count);
        if (!count)
            smith_ui_put_wrapped(sheet.margin, first, sheet.list_width,
                sheet.list_end - first, TERM_WHITE, ui_text("Choose an item to see its complete costs."));
        for (int i = top; i < count && i < top + grid.visible; i++)
        {
            int row = first + (i - top) * grid.stride;
            smith_sheet_field(sheet.margin, row, MIN(sheet.list_width, 48),
                bill[i].label, bill[i].value, TERM_WHITE,
                bill[i].requirement ? TERM_L_BLUE : TERM_WHITE);
        }
        if (sheet.divider >= 0)
        {
            int x = sheet.divider + sheet.margin;
            smith_sheet_stock(x, sheet.body, MIN(sheet.width - x - sheet.margin, 43),
                sheet.footer);
        }
        smith_sheet_action actions[3];
        int n = 0;
        if (max_top)
        {
            actions[n++] = (smith_sheet_action){ BILL_PREVIOUS, ui_text("Previous"), top ? TERM_WHITE : TERM_SLATE };
            actions[n++] = (smith_sheet_action){ BILL_NEXT, ui_text("Next"), top < max_top ? TERM_WHITE : TERM_SLATE };
        }
        actions[n++] = (smith_sheet_action){SMITH_CLICK_BACK, ui_text("Back"), TERM_L_BLUE};
        smith_sheet_actions(&sheet, actions, n, 0);
        smith_sheet_navigation(&sheet, false, NULL, false);
        Term_fresh();
        hide_cursor = true;
        int key = steamdeck_menu_key(inkey(), '[', ']');
        hide_cursor = false;
        int id, action;
        if (ui_menu_click_take_action(&id, &action))
        {
            if (action == UI_MENU_CLICK_HOVER) continue;
            if (id == SMITH_CLICK_BACK || action == UI_MENU_CLICK_SECONDARY) break;
            if (id == BILL_PREVIOUS) key = '[';
            if (id == BILL_NEXT) key = ']';
        }
        if (key == ESCAPE || key == 'q' || key == '\r' || key == '\n' || key == ' ') break;
        if (key == '8') top--;
        if (key == '2') top++;
        if (key == '[' || key == '4') top -= grid.visible;
        if (key == ']' || key == '6') top += grid.visible;
    }
    sdl_smithing_sheet_clear();
    ui_menu_click_clear();
    ui_scroll_area_clear();
    screen_load();
}

static void smith_picker_draw(const smith_sheet_layout* sheet,
    const smith_picker_grid* grid, const smith_picker_entry* entries,
    int count, int highlight, int draft_kind)
{
    smithing_cost_type saved_cost = smithing_cost;
    smith_alloy_state saved_alloy = smith_alloy;
    smith_clear_alloy_state(&smith_alloy);
    int first = grid->top * grid->columns;
    int end = MIN(count, first + grid->visible * grid->columns);
    for (int i = first; i < end; i++)
    {
        object_type icon;
        smith_prepare_base_object(&icon, entries[i].kind);
        byte attr = affordable(&icon) ? object_display_color(&icon, TERM_WHITE) : TERM_L_WHITE;
        smith_sheet_choice(sheet, grid, i, i + 1, i - first, entries[i].name,
            entries[i].kind == draft_kind && grid->step >= 25 ? ui_text("Draft") : "",
            &icon, i + 1 == highlight, attr);
    }
    smith_alloy = saved_alloy;
    smithing_cost = saved_cost;
}

static void smith_picker_move(int key, int count, int columns, int* highlight)
{
    int index = *highlight - 1;

    if (key == '4' || key == '6')
        index = (index + (key == '4' ? -1 : 1) + count) % count;
    else if (key == '8' || key == '2')
    {
        int rows = (count + columns - 1) / columns;
        int row = (index / columns + (key == '8' ? -1 : 1) + rows) % rows;
        index = MIN(count - 1, row * columns + index % columns);
    }
    *highlight = index + 1;
}

static void smith_browser_preview(const smith_sheet_layout* sheet, int kind,
    int draft_kind)
{
    object_type* saved_object = smith_o_ptr;
    smith_alloy_state saved_alloy = smith_alloy;
    smithing_cost_type saved_cost = smithing_cost;
    bool draft = kind == draft_kind && saved_object->k_idx;
    object_type candidate;

    if (!draft)
    {
        smith_prepare_base_object(&candidate, kind);
        smith_clear_alloy_state(&smith_alloy);
        smith_o_ptr = &candidate;
    }
    int difficulty = smith_measure_design();
    smith_sheet_order(sheet, smith_o_ptr, difficulty,
        draft && p_ptr->smithing_leftover > 0, draft, SMITH_NAME_READ_ONLY);
    smith_o_ptr = saved_object;
    smith_alloy = saved_alloy;
    smithing_cost = saved_cost;
}

static void smith_browser_inspect(int kind, int draft_kind)
{
    object_type preview;
    if (kind == draft_kind && smith_o_ptr->k_idx)
        preview = *smith_o_ptr;
    else
        smith_prepare_base_object(&preview, kind);
    describe_object_with_comparisons(&preview, true);
}

static int smith_item_browser(int* group, int* highlight, bool replace, int* kind)
{
    smith_picker_entry* entries = mem_alloc_array(z_info->k_max, smith_picker_entry);
    int draft_kind = *kind;
    smith_sheet_layout sheet = smith_sheet_geometry();
    int action = 0;
    int clicked = 0;
    int click_action = UI_MENU_CLICK_PRIMARY;
    bool controller = steamdeck_controls_active();
    /* A negative highlight retains the item whose Costs button has focus. */
    bool cost_focus = controller && *highlight < 0;
    bool allow_melt = !replace && cave_forge_bold(p_ptr->py, p_ptr->px)
        && meltable_metal_items_carried() > 0;
    bool allow_reforge = !replace && find_reforge_target_item() >= 0;

    *group = MAX(0, MIN(*group, SMITH_BROWSER_GROUP_COUNT - 1));
    int next_group = *group;
    int count = smith_browser_entries(*group, entries);
    int columns = sheet.list_width >= (*group == CAT_JEWELRY ? 56 : 40) ? 2 : 1;
    smith_sheet_fit_list(&sheet, count, columns, 0);
    smith_picker_grid grid = smith_sheet_grid(&sheet, count, columns,
        smith_sheet_list_first(&sheet), sheet.list_end);
    if (*highlight == 0)
    {
        *highlight = 1;
        for (int i = 0; i < count; i++)
            if (entries[i].kind == draft_kind)
            {
                *highlight = i + 1;
                break;
            }
        smith_ui_scroll_top[SMITH_SCROLL_SVAL] = (*highlight - 1) / grid.columns;
    }
    int item = MAX(1, MIN(ABS(*highlight), count));
    *highlight = cost_focus ? -item : item;

    smith_sheet_begin(&sheet, !replace);
    static const cptr groups[] = { "Weapons", "Armour", "Jewellery" };
    smith_sheet_tabs(&sheet, groups, SMITH_BROWSER_GROUP_COUNT, *group,
        SMITH_BROWSER_GROUP_CLICK);
    smith_sheet_open_list(&sheet, &grid, SMITH_SCROLL_SVAL, count, item - 1);
    smith_sheet_heading(&sheet, replace ? ui_text("Change base item") : ui_text("Choose base item"),
        &grid, count);
    if (count)
    {
        smith_picker_draw(&sheet, &grid, entries, count, item, draft_kind);
        smith_browser_preview(&sheet, entries[item - 1].kind, draft_kind);
    }
    else
    {
        smith_ui_detail_valid = false;
        sdl_smithing_artwork_clear();
        smith_ui_put_fitted(sheet.margin, grid.first, sheet.list_width,
            TERM_SLATE, ui_text("No items in this category."));
    }
    smith_sheet_action actions[5];
    int num_actions = 0;
    bool keys = sheet.width >= 76 && !sdl_touch_only_device_active() && !controller;
    char melt[32];
    char reforge[32];
    SDL_strlcpy(melt, ui_text("Melt"), sizeof(melt));
    SDL_strlcpy(reforge, ui_text("Reforge"), sizeof(reforge));
    if (!replace)
    {
        if (controller && sheet.width >= 40)
        {
            char button[16];
            controller_prompt_label(steamdeck_alt_action_key(), "X", button, sizeof(button));
            strnfmt(melt, sizeof(melt), ui_text("[%s] Melt"), button);
            controller_prompt_label(steamdeck_secondary_key(), "Y", button, sizeof(button));
            strnfmt(reforge, sizeof(reforge), ui_text("[%s] Reforge"), button);
        }
        actions[num_actions++] = (smith_sheet_action){SMITH_BROWSER_MELT_CLICK,
            keys ? ui_text("[M] Melt") : melt, allow_melt ? TERM_WHITE : TERM_SLATE};
        actions[num_actions++] = (smith_sheet_action){SMITH_BROWSER_REFORGE_CLICK,
            keys ? ui_text("[R] Reforge") : reforge, allow_reforge ? TERM_WHITE : TERM_SLATE};
    }
    if (sheet.width >= 48 || replace)
        actions[num_actions++] = (smith_sheet_action){SMITH_BROWSER_INSPECT_CLICK,
            keys ? ui_text("[I] Inspect") : ui_text("Inspect"), TERM_WHITE};
    actions[num_actions++] = (smith_sheet_action){SMITH_CLICK_CALC,
        keys ? ui_text("[?] Costs") : ui_text("Costs"), TERM_WHITE};
    actions[num_actions++] = (smith_sheet_action){SMITH_BROWSER_DESIGN_CLICK, ui_text("Design"),
        count ? TERM_L_GREEN : TERM_SLATE};
    smith_sheet_actions(&sheet, actions, num_actions, cost_focus ? SMITH_CLICK_CALC : 0);
    smith_sheet_navigation(&sheet, !replace, ui_text("Design"), true);
    Term_fresh();

    int ch = smith_sheet_read_key('[', ']');

    if (ch == '?')
    {
        smith_show_cost_details();
        goto done;
    }
    if (!replace && controller)
    {
        if (ch == steamdeck_alt_action_key())
            ch = 'M';
        if (ch == steamdeck_secondary_key())
            ch = 'R';
    }

    if (smith_ui_take_click_action(&clicked, &click_action))
    {
        ch = 0;
        if (clicked >= 1 && clicked <= count)
        {
            *highlight = clicked;
            cost_focus = false;
            if (click_action == UI_MENU_CLICK_SECONDARY)
                smith_browser_inspect(entries[*highlight - 1].kind, draft_kind);
            if (click_action != UI_MENU_CLICK_PRIMARY)
                goto done;
            ch = '\r';
        }
        else if (click_action == UI_MENU_CLICK_PRIMARY)
        {
            if (clicked == SMITH_CLICK_BACK)
                ch = ESCAPE;
            else if (clicked >= SMITH_BROWSER_GROUP_CLICK
                && clicked < SMITH_BROWSER_MELT_CLICK)
            {
                next_group = clicked - SMITH_BROWSER_GROUP_CLICK;
            }
            else if (clicked == SMITH_BROWSER_MELT_CLICK)
                ch = 'M';
            else if (clicked == SMITH_BROWSER_REFORGE_CLICK)
                ch = 'R';
            else if (clicked == SMITH_BROWSER_INSPECT_CLICK)
                ch = 'I';
            else if (clicked == SMITH_BROWSER_DESIGN_CLICK)
            {
                cost_focus = false;
                *highlight = item;
                ch = '\r';
            }
        }
    }

    if (ch == '[')
        next_group = (*group + SMITH_BROWSER_GROUP_COUNT - 1) % SMITH_BROWSER_GROUP_COUNT;
    if (ch == ']' || ch == '\t')
        next_group = (*group + 1) % SMITH_BROWSER_GROUP_COUNT;
    if (next_group != *group)
    {
        *group = next_group;
        *highlight = 1;
        smith_ui_scroll_top[SMITH_SCROLL_SVAL] = 0;
        goto done;
    }

    if (ch == ESCAPE)
        action = -1;
    else if (!replace && (ch == 'M' || ch == 'R'))
    {
        if (ch == 'M' ? allow_melt : allow_reforge)
            action = (ch == 'M') ? SMT_MENU_MELT : SMT_MENU_REPAIR;
        else
            bell(ch == 'M' ? ui_text("You cannot melt anything here.")
                           : ui_text("You have nothing to repair or reforge."));
    }
    else if (count)
    {
        int first = grid.top * grid.columns;
        int visible = MIN(count - first, grid.visible * grid.columns);
        if (ch == '\r' || ch == '\n' || ch == ' ')
        {
            if (cost_focus)
                smith_show_cost_details();
            else
                action = SMT_MENU_CREATE;
        }
        else if (sdl_menu_letters_enabled() && ch >= 'a' && ch < 'a' + visible)
        {
            *highlight = first + ch - 'a' + 1;
            action = SMT_MENU_CREATE;
        }
        else if (ch == 'I' || (controller && ch == steamdeck_info_key()))
            smith_browser_inspect(entries[item - 1].kind, draft_kind);
        else if (cost_focus && (ch == '8' || ch == '2'))
        {
            *highlight = item;
            if (ch == '2')
                smith_picker_move(ch, count, grid.columns, highlight);
        }
        else if (controller && ch == '2'
            && (item - 1) / grid.columns == (count - 1) / grid.columns)
            *highlight = -item;
        else if (!cost_focus)
            smith_picker_move(ch, count, grid.columns, highlight);
    }

    if (action == SMT_MENU_CREATE)
        *kind = entries[*highlight - 1].kind;
done:
    if (action) sdl_smithing_sheet_clear();
    mem_free(entries);
    return action;
}

static void smith_apply_stat_skill_flag_delta(object_type* o_ptr, u32b f1_before, u32b f1_after)
{
    if (!o_ptr)
        return;

    int pval = o_ptr->pval;
    int pval_abs = ABS(pval);

    bool before_str = (f1_before & (TR1_STR | TR1_NEG_STR)) != 0;
    bool after_str = (f1_after & (TR1_STR | TR1_NEG_STR)) != 0;
    if (!after_str)
    {
        o_ptr->stat_bonus[A_STR] = 0;
    }
    else if (!before_str)
    {
        o_ptr->stat_bonus[A_STR] = (f1_after & TR1_NEG_STR) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_STR) && !(f1_after & TR1_NEG_STR) && o_ptr->stat_bonus[A_STR] < 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];
    if ((f1_after & TR1_NEG_STR) && !(f1_after & TR1_STR) && o_ptr->stat_bonus[A_STR] > 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];

    bool before_dex = (f1_before & (TR1_DEX | TR1_NEG_DEX)) != 0;
    bool after_dex = (f1_after & (TR1_DEX | TR1_NEG_DEX)) != 0;
    if (!after_dex)
    {
        o_ptr->stat_bonus[A_DEX] = 0;
    }
    else if (!before_dex)
    {
        o_ptr->stat_bonus[A_DEX] = (f1_after & TR1_NEG_DEX) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_DEX) && !(f1_after & TR1_NEG_DEX) && o_ptr->stat_bonus[A_DEX] < 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];
    if ((f1_after & TR1_NEG_DEX) && !(f1_after & TR1_DEX) && o_ptr->stat_bonus[A_DEX] > 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];

    bool before_con = (f1_before & (TR1_CON | TR1_NEG_CON)) != 0;
    bool after_con = (f1_after & (TR1_CON | TR1_NEG_CON)) != 0;
    if (!after_con)
    {
        o_ptr->stat_bonus[A_CON] = 0;
    }
    else if (!before_con)
    {
        o_ptr->stat_bonus[A_CON] = (f1_after & TR1_NEG_CON) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_CON) && !(f1_after & TR1_NEG_CON) && o_ptr->stat_bonus[A_CON] < 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];
    if ((f1_after & TR1_NEG_CON) && !(f1_after & TR1_CON) && o_ptr->stat_bonus[A_CON] > 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];

    bool before_gra = (f1_before & (TR1_GRA | TR1_NEG_GRA)) != 0;
    bool after_gra = (f1_after & (TR1_GRA | TR1_NEG_GRA)) != 0;
    if (!after_gra)
    {
        o_ptr->stat_bonus[A_GRA] = 0;
    }
    else if (!before_gra)
    {
        o_ptr->stat_bonus[A_GRA] = (f1_after & TR1_NEG_GRA) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_GRA) && !(f1_after & TR1_NEG_GRA) && o_ptr->stat_bonus[A_GRA] < 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];
    if ((f1_after & TR1_NEG_GRA) && !(f1_after & TR1_GRA) && o_ptr->stat_bonus[A_GRA] > 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];

    bool before_mel = (f1_before & TR1_MEL) != 0;
    bool after_mel = (f1_after & TR1_MEL) != 0;
    if (!after_mel)
        o_ptr->skill_bonus[S_MEL] = 0;
    else if (!before_mel)
        o_ptr->skill_bonus[S_MEL] = pval;

    bool before_arc = (f1_before & TR1_ARC) != 0;
    bool after_arc = (f1_after & TR1_ARC) != 0;
    if (!after_arc)
        o_ptr->skill_bonus[S_ARC] = 0;
    else if (!before_arc)
        o_ptr->skill_bonus[S_ARC] = pval;

    bool before_stl = (f1_before & TR1_STL) != 0;
    bool after_stl = (f1_after & TR1_STL) != 0;
    if (!after_stl)
        o_ptr->skill_bonus[S_STL] = 0;
    else if (!before_stl)
        o_ptr->skill_bonus[S_STL] = pval;

    bool before_per = (f1_before & TR1_PER) != 0;
    bool after_per = (f1_after & TR1_PER) != 0;
    if (!after_per)
        o_ptr->skill_bonus[S_PER] = 0;
    else if (!before_per)
        o_ptr->skill_bonus[S_PER] = pval;

    bool before_wil = (f1_before & TR1_WIL) != 0;
    bool after_wil = (f1_after & TR1_WIL) != 0;
    if (!after_wil)
        o_ptr->skill_bonus[S_WIL] = 0;
    else if (!before_wil)
        o_ptr->skill_bonus[S_WIL] = pval;

    bool before_smt = (f1_before & TR1_SMT) != 0;
    bool after_smt = (f1_after & TR1_SMT) != 0;
    if (!after_smt)
        o_ptr->skill_bonus[S_SMT] = 0;
    else if (!before_smt)
        o_ptr->skill_bonus[S_SMT] = pval;

    bool before_sng = (f1_before & TR1_SNG) != 0;
    bool after_sng = (f1_after & TR1_SNG) != 0;
    if (!after_sng)
        o_ptr->skill_bonus[S_SNG] = 0;
    else if (!before_sng)
        o_ptr->skill_bonus[S_SNG] = pval;
}

typedef enum
{
    SMITH_NUMBER_ATTACK,
    SMITH_NUMBER_DAMAGE,
    SMITH_NUMBER_EVASION,
    SMITH_NUMBER_PROTECTION,
    SMITH_NUMBER_PROTECTION_DICE,
    SMITH_NUMBER_WEIGHT,
    SMITH_NUMBER_ALLOY,
    SMITH_NUMBER_STAT,
    SMITH_NUMBER_SKILL,
    SMITH_NUMBER_PVAL,
} smith_number_kind;

#define SMITH_SIMPLE_NUMBER_MAX 7
#define SMITH_BONUS_NUMBER_MAX (A_MAX + 7 + 2)

typedef struct
{
    cptr label;
    char value[32];
    bool editable;
    smith_number_kind kind;
    u32b flags;
    int index;
    int current;
    int first;
    int step;
    int count;
} smith_number_card;

static void smith_format_number(const smith_number_card* card, int value,
    char* text, size_t size)
{
    switch (card->kind)
    {
    case SMITH_NUMBER_DAMAGE:
        strnfmt(text, size, "%dd%d", smith_o_ptr->dd, value);
        break;
    case SMITH_NUMBER_PROTECTION:
        strnfmt(text, size, "%dd%d", smith_o_ptr->pd, value);
        break;
    case SMITH_NUMBER_PROTECTION_DICE:
        if (value == -1)
            SDL_strlcpy(text, "1d0", size);
        else if (value >= 0)
            strnfmt(text, size, "%dd%d", smithing_amulet_protection_combos[value].pd,
                smithing_amulet_protection_combos[value].ps);
        else
            strnfmt(text, size, "%dd%d", smith_o_ptr->pd, smith_o_ptr->ps);
        break;
    case SMITH_NUMBER_WEIGHT:
        strnfmt(text, size, ui_text("%d.%d lb"), value / 10, ABS(value % 10));
        break;
    case SMITH_NUMBER_ALLOY:
    {
        static const cptr names[] = { "None", "Mithril", "Star iron" };
        SDL_strlcpy(text, ui_text(names[value]), size);
        break;
    }
    default:
        strnfmt(text, size, "%+d", value);
        break;
    }
}

static void smith_set_number_card(smith_number_card* card, cptr label,
    smith_number_kind kind, int index, int current, int min, int max,
    int step)
{
    /* Preserve the blueprint's step alignment. A carried-over value
     * outside its current limits can still be displayed and left alone. */
    int remainder = (current - min) % step;
    int first = min + (remainder + step) % step;

    *card = (smith_number_card){
        .label = label, .kind = kind, .index = index, .current = current,
        .first = first, .step = step,
        .count = (max >= first) ? (max - first) / step + 1 : 0,
    };
    card->editable = (card->count > 1
        || (card->count == 1 && card->first != current))
        && !smith_pending_reforge();
    smith_format_number(card, current, card->value, sizeof(card->value));
}

static bool smith_has_builtin_tunneling(const object_type* item)
{
    return item->tval == TV_DIGGING && (k_info[item->k_idx].flags1 & TR1_TUNNEL);
}

static void smith_set_tunneling_card(smith_number_card* card)
{
    smith_set_number_card(card, ui_text("Tunneling"), SMITH_NUMBER_PVAL, 0,
        smith_o_ptr->pval, MAX(1, pval_min()), pval_max(), 1);
    card->flags = TR1_TUNNEL;
}

static int smith_collect_simple_numbers(smith_number_card* cards)
{
    object_type* item = smith_o_ptr;
    int count = 0;

    if (!item->k_idx)
        return 0;

    int att_lo = att_min(), att_hi = att_max();
    int ds_lo = ds_min(), ds_hi = ds_max();
    int evn_lo = evn_min(), evn_hi = evn_max();
    int ps_lo = ps_min(), ps_hi = ps_max();

    if (item->att || att_hi > att_lo)
        smith_set_number_card(&cards[count++], ui_text("Attack"), SMITH_NUMBER_ATTACK,
            0, item->att, att_lo, (att_hi > att_lo) ? att_hi : att_lo - 1,
            (item->tval == TV_ARROW && !item->name1) ? 3 : 1);
    if (item->dd || ds_hi > ds_lo)
        smith_set_number_card(&cards[count++], ui_text("Damage"), SMITH_NUMBER_DAMAGE,
            0, item->ds, ds_lo, (ds_hi > ds_lo) ? ds_hi : ds_lo - 1, 1);
    if (item->evn || evn_hi > evn_lo)
        smith_set_number_card(&cards[count++], ui_text("Evasion"), SMITH_NUMBER_EVASION,
            0, item->evn, evn_lo, (evn_hi > evn_lo) ? evn_hi : evn_lo - 1, 1);

    if (smithing_variable_protection_dice(item))
    {
        int current = smithing_protection_combo_index(item);
        bool empty = item->pd <= 1 && item->ps == 0;
        if (current < 0)
            current = (item->pd == 1 && empty) ? -1 : -2;
        bool can_edit = ps_hi > ps_lo && (current >= 0 || empty);
        smith_set_number_card(&cards[count++], ui_text("Protection"),
            SMITH_NUMBER_PROTECTION_DICE, 0, current, (ps_lo < 1) ? -1 : 0,
            can_edit ? (int)N_ELEMENTS(smithing_amulet_protection_combos) - 1 : -2,
            1);
    }
    else if (item->pd || ps_hi > ps_lo)
        smith_set_number_card(&cards[count++], ui_text("Protection"),
            SMITH_NUMBER_PROTECTION, 0, item->ps, ps_lo,
            (ps_hi > ps_lo) ? ps_hi : ps_lo - 1, 1);

    if (smith_has_builtin_tunneling(item))
        smith_set_tunneling_card(&cards[count++]);

    bool can_weigh = wgt_valid();
    smith_set_number_card(&cards[count++], "Weight", SMITH_NUMBER_WEIGHT, 0,
        item->weight, can_weigh ? wgt_min() : item->weight,
        can_weigh ? wgt_max() : item->weight, 5);

    bool can_alloy = smith_alloy_applicable(item);
    if (can_alloy || smith_alloy.type != SMITH_ALLOY_NONE)
    {
        const bool mastery = p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY];
        smith_set_number_card(&cards[count++], "Alloy", SMITH_NUMBER_ALLOY, 0,
            smith_alloy.type, SMITH_ALLOY_NONE,
            can_alloy && mastery ? SMITH_ALLOY_STAR_IRON : SMITH_ALLOY_NONE, 1);
    }
    return count;
}

static int smith_collect_bonus_numbers(smith_number_card* cards)
{
    const object_type* item = smith_o_ptr;
    int count = 0;
    u32b f1, f2, f3;
    if (!item->k_idx) return 0;

    int floor_bonus = pval_min();
    int max_bonus = pval_max();
    object_flags(item, &f1, &f2, &f3);
    static const struct
    {
        cptr label;
        u32b positive;
        u32b negative;
    } stats[A_MAX] = {
        [A_STR] = { "Strength", TR1_STR, TR1_NEG_STR },
        [A_DEX] = { "Dexterity", TR1_DEX, TR1_NEG_DEX },
        [A_CON] = { "Constitution", TR1_CON, TR1_NEG_CON },
        [A_GRA] = { "Grace", TR1_GRA, TR1_NEG_GRA },
    };
    for (int i = 0; i < A_MAX; i++)
    {
        bool positive = (f1 & stats[i].positive) != 0;
        bool negative = (f1 & stats[i].negative) != 0;
        if (positive || negative)
        {
            smith_set_number_card(&cards[count], ui_text(stats[i].label),
                SMITH_NUMBER_STAT, i, item->stat_bonus[i],
                negative ? -max_bonus : floor_bonus, positive ? max_bonus : 0, 1);
            cards[count++].flags = f1 & (stats[i].positive | stats[i].negative);
        }
    }

    static const struct
    {
        int skill;
        u32b flag;
    } skills[] = {
        { S_MEL, TR1_MEL }, { S_ARC, TR1_ARC }, { S_STL, TR1_STL },
        { S_PER, TR1_PER }, { S_WIL, TR1_WIL }, { S_SMT, TR1_SMT },
        { S_SNG, TR1_SNG },
    };
    for (size_t i = 0; i < N_ELEMENTS(skills); i++)
    {
        int skill = skills[i].skill;
        if (f1 & skills[i].flag)
        {
            smith_set_number_card(&cards[count], skill_names_full[skill],
                SMITH_NUMBER_SKILL, skill, item->skill_bonus[skill],
                floor_bonus, max_bonus, 1);
            cards[count++].flags = skills[i].flag;
        }
    }

    if (f1 & TR1_DAMAGE_SIDES)
    {
        smith_set_number_card(&cards[count], ui_text("Damage bonus"), SMITH_NUMBER_PVAL,
            0, item->pval, floor_bonus, max_bonus, 1);
        cards[count++].flags = TR1_DAMAGE_SIDES;
    }
    if (f1 & TR1_TUNNEL)
        smith_set_tunneling_card(&cards[count++]);

    return count;
}

static void smith_apply_number(const smith_number_card* card, int value)
{
    if (value == card->current)
        return;

    switch (card->kind)
    {
    case SMITH_NUMBER_ATTACK:
        smith_o_ptr->att = value;
        break;
    case SMITH_NUMBER_DAMAGE:
        smith_o_ptr->ds = value;
        break;
    case SMITH_NUMBER_EVASION:
        smith_o_ptr->evn = value;
        break;
    case SMITH_NUMBER_PROTECTION:
        smith_o_ptr->ps = value;
        break;
    case SMITH_NUMBER_PROTECTION_DICE:
        if (value == -1)
        {
            smith_o_ptr->pd = 1;
            smith_o_ptr->ps = 0;
        }
        else
            smithing_set_protection_combo(smith_o_ptr, value);
        break;
    case SMITH_NUMBER_WEIGHT:
        smith_o_ptr->weight = value;
        break;
    case SMITH_NUMBER_ALLOY:
        (void)smith_apply_alloy(smith_o_ptr, &smith_alloy, (smith_alloy_type)value);
        break;
    case SMITH_NUMBER_STAT:
        smith_o_ptr->stat_bonus[card->index] = value;
        break;
    case SMITH_NUMBER_SKILL:
        smith_o_ptr->skill_bonus[card->index] = value;
        break;
    case SMITH_NUMBER_PVAL:
        smith_o_ptr->pval = value;
        break;
    }
}

typedef enum {
    SMITH_SIMPLE, SMITH_ENHANCEMENTS, SMITH_ARTIFICE, SMITH_PAGE_COUNT
} smith_page;

static void smith_editor_tabs(const smith_sheet_layout* sheet, smith_page page,
    bool interactive)
{
    static const cptr names[] = { "Simple", "Enhancements", "Artifice" };
    smith_sheet_tabs(sheet, names, SMITH_PAGE_COUNT, page,
        interactive ? SMITH_EDITOR_TAB : 0);
}

static void smith_flag_effect_text(const object_type* item, int flagset,
    u32b flag, char* text, size_t size)
{
    object_type property = *item;
    property.abilities = 0;
    u32b mask[4] = { 0 };
    mask[flagset - 1] = flag;
    object_info_effect_text(&property, mask, text, size);
}

static bool smith_number_card_menu(const smith_number_card* card, smith_page page,
    int* page_out)
{
    if (page_out) *page_out = -1;
    if (!card->editable || smith_pending_reforge()) return false;

    object_copy(smith3_o_ptr, smith_o_ptr);
    smith3_alloy = smith_alloy;
    const smithing_cost_type original_cost = smithing_cost;
    const int offset = (card->current - card->first) / card->step;
    const bool current_in_range = offset >= 0 && offset < card->count
        && card->first + offset * card->step == card->current;
    const int count = card->count + !current_in_range;
    const int current = current_in_range ? offset : count - 1;
    int focus = current, choice = -1;

    bool initial = true;
    for (;;)
    {
        smith_sheet_layout sheet = smith_sheet_geometry();
        int cols = MAX(1, MIN(count, MIN(4, sheet.list_width / 15)));
        smith_sheet_fit_list(&sheet, count, cols, 0);
        smith_sheet_begin(&sheet, false);
        if (page_out)
            smith_editor_tabs(&sheet, page, true);
        else
            smith_ui_put_fitted(sheet.margin, sheet.tabs, sheet.list_width, TERM_L_BLUE, ui_text("Artifice"));
        smith_picker_grid grid = smith_sheet_grid(&sheet, count, cols,
            smith_sheet_list_first(&sheet), sheet.list_end);
        if (initial)
        {
            smith_ui_scroll_top[SMITH_SCROLL_VALUES] = MAX(0, focus / cols - grid.visible / 2);
            initial = false;
        }
        smith_sheet_open_list(&sheet, &grid, SMITH_SCROLL_VALUES, count, focus);
        smith_sheet_heading(&sheet, format(ui_text("Choose %s"), card->label), &grid, count);
        int start = grid.top * cols;
        int limit = MIN(count, start + grid.visible * cols);
        for (int i = start; i < limit; i++)
        {
            int value = i == current ? card->current : card->first + i * card->step;
            char label[32];
            smith_format_number(card, value, label, sizeof(label));
            smith_apply_number(card, value);
            byte attr = affordable(smith_o_ptr) ? TERM_WHITE : TERM_SLATE;
            *smith_o_ptr = *smith3_o_ptr;
            smith_alloy = smith3_alloy;
            smith_sheet_choice(&sheet, &grid, i, i - start + 1, i - start,
                label, i == current ? (grid.step >= 22 ? ui_text("Current") : "*") : "",
                NULL, i == focus, attr);
        }
        int value = focus == current ? card->current : card->first + focus * card->step;
        smith_apply_number(card, value);
        int difficulty = smith_measure_design();
        char effects[sizeof(smith_description.text)];
        if (card->flags)
        {
            smith_flag_effect_text(smith_o_ptr, 1, card->flags, effects, sizeof(effects));
            if (!*effects)
            {
                char preview[32];
                smith_format_number(card, value, preview, sizeof(preview));
                strnfmt(effects, sizeof(effects), "%s %s.", card->label, preview);
            }
        }
        smith_sheet_order_with_effects(&sheet, smith_o_ptr, difficulty,
            focus == current && p_ptr->smithing_leftover > 0, false,
            SMITH_NAME_READ_ONLY, card->flags ? effects : NULL);
        *smith_o_ptr = *smith3_o_ptr;
        smith_alloy = smith3_alloy;
        smithing_cost = original_cost;

        const smith_sheet_action actions[] = {
            { SMITH_CLICK_BACK, ui_text("Back"), TERM_WHITE }, { SMITH_EDITOR_COSTS, ui_text("Costs"), TERM_WHITE },
            { SMITH_EDITOR_APPLY, ui_text("Apply"), TERM_L_GREEN }
        };
        smith_sheet_actions(&sheet, actions, N_ELEMENTS(actions), 0);
        smith_sheet_navigation(&sheet, false, ui_text("Apply"), page_out != NULL);
        Term_fresh();
        int key = smith_sheet_read_key(page_out ? '[' : 0, page_out ? ']' : 0);

        int clicked, action;
        if (smith_ui_take_click_action(&clicked, &action))
        {
            key = 0;
            if (clicked == SMITH_CLICK_BACK || action == UI_MENU_CLICK_SECONDARY)
            {
                if (action != UI_MENU_CLICK_HOVER) break;
            }
            else if (clicked >= 1 && clicked <= limit - start)
            {
                focus = start + clicked - 1;
                if (action == UI_MENU_CLICK_PRIMARY) key = '\r';
            }
            else if (action == UI_MENU_CLICK_PRIMARY)
            {
                if (page_out && clicked >= SMITH_EDITOR_TAB && clicked < SMITH_EDITOR_TAB + SMITH_PAGE_COUNT)
                    *page_out = clicked - SMITH_EDITOR_TAB;
                else if (clicked == SMITH_EDITOR_COSTS) key = '?';
                else if (clicked == SMITH_EDITOR_APPLY) key = '\r';
            }
        }
        if (page_out)
        {
            if (key == '[') *page_out = (page + SMITH_PAGE_COUNT - 1) % SMITH_PAGE_COUNT;
            if (key == ']' || key == '\t') *page_out = (page + 1) % SMITH_PAGE_COUNT;
        }
        if (key == ESCAPE || (page_out && *page_out >= 0)) break;
        if (key == '?')
        {
            smith_show_cost_details();
        }
        else if (key == '\r' || key == '\n' || key == ' ')
        {
            choice = focus;
            break;
        }
        else if (sdl_menu_letters_enabled() && key >= 'a' && key <= 'z'
            && start + key - 'a' < limit)
        {
            choice = start + key - 'a';
            break;
        }
        else if (key == '4' || key == '6')
            focus = (focus + count + (key == '4' ? -1 : 1)) % count;
        else if (key == '8' || key == '2')
        {
            int step = count > cols ? cols : 1;
            if (key == '8') focus = focus >= step ? focus - step : count - 1;
            else focus = focus + step < count ? focus + step : 0;
        }
    }

    ui_menu_click_clear();
    ui_scroll_area_clear();
    if (choice < 0 || choice == current) return false;
    smith_apply_number(card, card->first + choice * card->step);
    if (object_has_ego(smith_o_ptr)) enchant_then_numbers = true;
    return true;
}

static void ego_name_for_enchant_menu(int e_idx, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;
    buf[0] = '\0';
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return;

    ego_item_type* e_ptr = &e_info[e_idx];
    const char* raw = e_name + e_ptr->name;
    if (!raw || !raw[0])
        return;

    if (ego_name_is_prefix(raw))
    {
        size_t len = strlen(raw);
        size_t copy_len = (len >= 2) ? (len - 2) : 0;
        if (copy_len >= buflen)
            copy_len = buflen - 1;
        if (copy_len > 0)
        {
            memcpy(buf, raw + 1, copy_len);
            buf[copy_len] = '\0';
        }
        return;
    }

    SDL_strlcpy(buf, raw, buflen);
}

static smith_picker_grid smith_sheet_sublist(smith_sheet_layout* sheet,
    smith_ui_scroll_id id, int count, int focus, cptr section, cptr heading,
    cptr verb, cptr remove)
{
    if (id != SMITH_SCROLL_MELT)
        smith_sheet_fit_list(sheet, count, 1, 0);
    smith_sheet_begin(sheet, false);
    smith_ui_put_fitted(sheet->margin, sheet->tabs, sheet->width - 2 * sheet->margin,
        TERM_L_BLUE, section);
    smith_picker_grid grid = smith_sheet_grid(sheet, count, 1,
        smith_sheet_list_first(sheet), sheet->list_end);
    smith_sheet_open_list(sheet, &grid, id, count, focus);
    smith_sheet_heading(sheet, heading, &grid, count);
    smith_sheet_action actions[4];
    int n = 0;
    actions[n++] = (smith_sheet_action){ SMITH_CLICK_BACK, ui_text("Back"), TERM_WHITE };
    if (id != SMITH_SCROLL_MELT)
        actions[n++] = (smith_sheet_action){ SMITH_CLICK_CALC, ui_text("Costs"), TERM_WHITE };
    if (remove)
        actions[n++] = (smith_sheet_action){ SMITH_EDITOR_REMOVE, remove, TERM_L_RED };
    if (verb)
        actions[n++] = (smith_sheet_action){ SMITH_EDITOR_APPLY, verb,
            count ? TERM_L_GREEN : TERM_SLATE };
    smith_sheet_actions(sheet, actions, n, 0);
    smith_sheet_navigation(sheet, false, verb, false);
    return grid;
}

static int smith_sheet_read_choice(int* highlight, int count)
{
    Term_fresh();
    int key = smith_sheet_read_key(0, 0);
    int id, action;
    if (smith_ui_take_click_action(&id, &action))
    {
        if (id == SMITH_CLICK_BACK || action == UI_MENU_CLICK_SECONDARY)
            return action == UI_MENU_CLICK_HOVER ? 0 : ESCAPE;
        if (id == SMITH_EDITOR_APPLY) return action == UI_MENU_CLICK_PRIMARY ? '\r' : 0;
        if (id == SMITH_EDITOR_REMOVE) return action == UI_MENU_CLICK_PRIMARY ? '-' : 0;
        if (id > 0 && id <= count)
            return smith_ui_pointer_choice_confirms(id, action, highlight) ? '\r' : 0;
        return 0;
    }
    if (key == '?' && smith_ui_detail_valid)
    {
        smith_show_cost_details();
        return 0;
    }
    return key;
}

static void smith_affix_effects(const object_type* item, const object_type* base,
    int affix, char* text, size_t size)
{
    const ego_item_type* ego = &e_info[affix];
    u32b mask[4] = { ego->flags1, ego->flags2, ego->flags3, ego->flags4 };
    if (item->pval != base->pval)
        mask[0] |= object_kind_pval_flags1(&k_info[item->k_idx])
            | TR1_TUNNEL | TR1_DAMAGE_SIDES;

    object_type changed = *item;
    changed.abilities = 0;
    for (int i = 0; i < item->abilities; i++)
    {
        if (object_grants_ability(base, item->skilltype[i], item->abilitynum[i]))
            continue;
        int j = changed.abilities++;
        changed.skilltype[j] = item->skilltype[i];
        changed.abilitynum[j] = item->abilitynum[i];
        changed.bane_type[j] = item->bane_type[i];
    }
    object_info_effect_text(&changed, mask, text, size);

    size_t used = strlen(text);
    if (item->att != base->att)
        strnfcat(text, size, &used, "%s%s %+d.", used ? " " : "",
            ui_text("Attack"), item->att - base->att);
    if (item->evn != base->evn)
        strnfcat(text, size, &used, "%s%s %+d.", used ? " " : "",
            ui_text("Evasion"), item->evn - base->evn);
    if (item->dd != base->dd || item->ds != base->ds)
        strnfcat(text, size, &used, "%s%s %dd%d → %dd%d.", used ? " " : "",
            ui_text("Damage"), base->dd, base->ds, item->dd, item->ds);
    if (item->pd != base->pd || item->ps != base->ps)
        strnfcat(text, size, &used, "%s%s %dd%d → %dd%d.", used ? " " : "",
            ui_text("Protection"), base->pd, base->ps, item->pd, item->ps);
    smith_append_volume_effect(text, size, object_effective_volume(base),
        object_effective_volume(item));
    if (!*text)
        SDL_strlcpy(text, ui_text("No additional effects."), size);
}

static void prt_reforge_preview(const smith_sheet_layout* sheet,
    const reforge_preview_type* preview, const object_type* source)
{
    smith_ui_detail_valid = preview != NULL;
    if (!preview) return;
    smith_ui_detail_reforge = true;
    smith_ui_detail_paid = false;
    smith_ui_detail_cost = preview->cost;
    smith_ui_detail_turns = preview->turns;
    char effects[sizeof(smith_description.text)];
    smith_affix_effects(smith_o_ptr, source, object_ego_prefix(smith_o_ptr),
        effects, sizeof(effects));
    smith_sheet_order_with_effects(sheet, smith_o_ptr, preview->scaled_difficulty,
        false, false, SMITH_NAME_READ_ONLY, effects);
}

static bool reforge_preview_missing_ability(const reforge_preview_type* preview)
{
    if (!preview)
        return false;

    return preview->needs_reforging || preview->cost.weaponsmith
        || preview->cost.armoursmith || preview->cost.jeweller;
}

static bool reforge_preview_missing_forge(const reforge_preview_type* preview)
{
    if (!preview)
        return false;

    return preview->needs_forge || preview->needs_forge_resources;
}

static bool reforge_preview_primary_blocker(const reforge_preview_type* preview)
{
    return reforge_preview_missing_ability(preview)
        || reforge_preview_missing_forge(preview);
}

static int reforge_prefix_menu(const object_type* source)
{
    char ch;

    int i;
    int highlight = 1;
    int entry_count = 0;
    int choice[26];
    bool valid[26];
    reforge_preview_type previews[26];
    if (!source || !source->k_idx)
        return 0;

    screen_save();

    while (true)
    {
        smith_sheet_layout sheet = smith_sheet_geometry();
        entry_count = 0;
        memset(choice, 0, sizeof(choice));
        memset(valid, 0, sizeof(valid));
        memset(previews, 0, sizeof(previews));

        for (i = 1; i < z_info->e_max && entry_count < (int)N_ELEMENTS(choice); i++)
        {
            if (!ego_prefix_can_apply_to_object(source, i))
                continue;
            if (!reforge_preview_build(source, i, &previews[entry_count]))
                continue;

            valid[entry_count] = previews[entry_count].affordable;
            choice[entry_count] = i;

            entry_count++;
        }

        highlight = MAX(1, MIN(highlight, entry_count));
        smith_picker_grid grid = smith_sheet_sublist(&sheet, SMITH_SCROLL_REFORGE,
            entry_count, highlight - 1, ui_text("Reforging"), ui_text("Prefix"),
            entry_count ? ui_text("Reforge") : NULL, NULL);
        if (!entry_count)
        {
            smith_ui_put_fitted(sheet.margin, grid.first, sheet.list_width,
                TERM_SLATE, ui_text("No legal prefixes available."));
            (void)smith_sheet_read_choice(&highlight, 0);
            sdl_smithing_sheet_clear();
            ui_menu_click_clear();
            screen_load();
            return 0;
        }

        for (i = grid.top; i < entry_count && i < grid.top + grid.visible; i++)
        {
            char label[64];
            ego_name_for_enchant_menu(choice[i], label, sizeof(label));
            byte attr = valid[i] ? TERM_WHITE :
                (reforge_preview_primary_blocker(&previews[i]) ? TERM_L_RED : TERM_SLATE);
            smith_sheet_choice(&sheet, &grid, i, i + 1, i, label, "",
                NULL, highlight == i + 1, attr);
        }
        (void)reforge_preview_build(source, choice[highlight - 1],
            &previews[highlight - 1]);
        prt_reforge_preview(&sheet, &previews[highlight - 1], source);
        int detail = grid.first + entry_count * grid.stride + 1;
        if (detail < sheet.list_end)
            smith_sheet_field(sheet.margin, detail, sheet.list_width, ui_text("Raw difficulty increase"),
                format("+%d", previews[highlight - 1].raw_delta_difficulty), TERM_WHITE, TERM_L_WHITE);
        ch = smith_sheet_read_choice(&highlight, entry_count);

        if (sdl_menu_letters_enabled()
            && (ch >= 'a') && (ch <= (char)'a' + entry_count - 1))
        {
            highlight = (int)ch - 'a' + 1;
            ch = '\r';
        }
        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (reforge_preview_missing_ability(&previews[highlight - 1]))
                bell(ui_text("You lack the ability for that reforge."));
            else if (reforge_preview_missing_forge(&previews[highlight - 1]))
                bell(ui_text("You need a forge with resources for that reforge."));
            else if (!valid[highlight - 1])
                bell(ui_text("You cannot afford that reforge."));
            else
            {
                sdl_smithing_sheet_clear();
                ui_menu_click_clear();
                screen_load();
                return choice[highlight - 1];
            }
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            sdl_smithing_sheet_clear();
            ui_menu_click_clear();
            screen_load();
            return 0;
        }
        else if (ch == '8'
#ifdef ARROW_UP
            || ch == ARROW_UP
#endif
            )
        {
            if (highlight > 1)
                highlight--;
            else
                highlight = entry_count;
        }
        else if (ch == '2'
#ifdef ARROW_DOWN
            || ch == ARROW_DOWN
#endif
            )
        {
            if (highlight < entry_count)
                highlight++;
            else
                highlight = 1;
        }
    }
}

static void create_special(int ego_prefix, int ego_suffix)
{
    /* Retrieve a backup of the object */
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    /* Suffix egos marked NO_PREFIX cannot be combined with any prefix. */
    if (ego_forbids_prefix_combo(ego_suffix))
        ego_prefix = 0;

    /* Apply requested ego affixes */
    object_set_ego_prefix(smith_o_ptr, ego_prefix);
    object_set_ego_suffix(smith_o_ptr, ego_suffix);

    /* Apply ego bonuses */
    if (object_has_ego(smith_o_ptr))
        object_into_special(smith_o_ptr, p_ptr->skill_use[S_SMT], true);

    /* Re-evaluate stack size now that an enchantment is applied */
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

static bool enchant_menu_has_applicable_affix(const object_type* base_o_ptr,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    int i;

    if (!base_o_ptr || !smith_o_ptr || base_o_ptr->tval == 0)
        return false;
    if (object_has_evil_alignment(smith_o_ptr))
        return false;

    for (i = 1; i < z_info->e_max; i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
            return true;
    }

    return false;
}

/*
 * Performs the interface and selection work for the enchantment menu.
 */
static int enchant_menu_aux(int* highlight, int fixed_prefix, int fixed_suffix,
    bool selecting_prefix, const object_type* base_o_ptr)
{
    char ch;
    int i;
    int entry_count = 0;

    bool valid[26];
    int choice[26];
    smith_sheet_layout sheet = smith_sheet_geometry();
    const object_type original = *smith_o_ptr;
    const smith_alloy_state original_alloy = smith_alloy;
    const smithing_cost_type original_cost = smithing_cost;
    bool no_prefix_allowed = selecting_prefix
        && ego_forbids_prefix_combo(fixed_suffix);

    int selected_affix = selecting_prefix ? object_ego_prefix(&original)
        : object_ego_suffix(&original);

    create_special(fixed_prefix, fixed_suffix);
    const object_type base = *smith_o_ptr;
    valid[entry_count] = affordable(smith_o_ptr);
    choice[entry_count] = 0;
    entry_count++;

    /* We have to search the whole special item list. */
    for (i = 1; i < z_info->e_max && entry_count < (int)N_ELEMENTS(choice); i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
        {
            /* Make a preview 'special' version of the object */
            if (selecting_prefix)
                create_special(i, fixed_suffix);
            else
                create_special(fixed_prefix, i);

            // Check whether it is a valid choice for creating
            if (affordable(smith_o_ptr))
            {
                valid[entry_count] = true;
            }
            else
            {
                valid[entry_count] = false;
            }

            /* Remember the object index */
            choice[entry_count] = i;

            // count the applicable items
            entry_count++;
        }
    }

    if (*highlight == 0)
    {
        for (int row = 0; row < entry_count; ++row)
            if (choice[row] == selected_affix)
                *highlight = row + 1;
    }
    if (*highlight < 1) *highlight = 1;
    if (*highlight > entry_count) *highlight = entry_count;

    int affix = choice[*highlight - 1];
    cptr verb = !affix && selected_affix ? ui_text("Remove") : ui_text("Apply");
    smith_picker_grid grid = smith_sheet_sublist(&sheet, SMITH_SCROLL_ENCHANT,
        entry_count, *highlight - 1, ui_text("Enhancements"),
        selecting_prefix ? ui_text("Prefix") : ui_text("Suffix"), verb, NULL);
    for (i = grid.top; i < entry_count && i < grid.top + grid.visible; i++)
    {
        char label[64];
        if (choice[i] == 0) SDL_strlcpy(label, ui_text("(none)"), sizeof(label));
        else ego_name_for_enchant_menu(choice[i], label, sizeof(label));
        smith_sheet_choice(&sheet, &grid, i, i + 1, i, label,
            choice[i] == selected_affix ? ui_text("Current") : "",
            NULL, i + 1 == *highlight, valid[i] ? TERM_WHITE : TERM_SLATE);
    }

    create_special(selecting_prefix ? affix : fixed_prefix,
        selecting_prefix ? fixed_suffix : affix);
    char effects[sizeof(smith_description.text)];
    if (affix)
    {
        smith_affix_effects(smith_o_ptr, &base, affix, effects, sizeof(effects));
        if (!selecting_prefix && fixed_prefix && ego_forbids_prefix_combo(affix))
        {
            SDL_strlcat(effects, " ", sizeof(effects));
            SDL_strlcat(effects, ui_text("This suffix excludes prefixes."), sizeof(effects));
        }
    }
    else
    {
        cptr description = no_prefix_allowed ? ui_text("This suffix excludes prefixes.")
            : selecting_prefix ? ui_text("Use this item without a prefix.")
            : ui_text("Use this item without a suffix.");
        SDL_strlcpy(effects, description, sizeof(effects));
    }
    smith_sheet_order_with_effects(&sheet, smith_o_ptr, smith_measure_design(),
        false, false, SMITH_NAME_READ_ONLY, effects);
    *smith_o_ptr = original;
    smith_alloy = original_alloy;
    smithing_cost = original_cost;
    ch = smith_sheet_read_choice(highlight, entry_count);

    /* Choose by letter */
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + entry_count - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        ch = '\r';
    }

    /* Rebuild from the selected logical row for every input source.  Pointer
     * activation can update the highlight after the old preview was made. */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
        || (ch == ARROW_RIGHT)
#endif
        )
    {
        if (selecting_prefix)
            create_special(choice[*highlight - 1], fixed_suffix);
        else
            create_special(fixed_prefix, choice[*highlight - 1]);

        return (*highlight);
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE)
#ifdef ARROW_LEFT
        || (ch == ARROW_LEFT)
#endif
        )
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8'
#ifdef ARROW_UP
        || (ch == ARROW_UP)
#endif
        )
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = entry_count;
    }

    /* Next item */
    if (ch == '2'
#ifdef ARROW_DOWN
        || (ch == ARROW_DOWN)
#endif
        )
    {
        if (*highlight < entry_count)
            (*highlight)++;
        else if (*highlight == entry_count)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu for making an item into a {special} item.
 */
static bool smith_choose_affix(bool prefix)
{
    object_type original = *smith_o_ptr;
    smith_alloy_state original_alloy = smith_alloy;
    int old_prefix = object_ego_prefix(&original);
    int old_suffix = object_ego_suffix(&original);

    int highlight = 0, choice;

    screen_save();
    Term_clear();
    do {
        choice = enchant_menu_aux(&highlight, prefix ? 0 : old_prefix,
            prefix ? old_suffix : 0, prefix, smith2_o_ptr);
    } while (!choice);
    ui_menu_click_clear();
    screen_load();
    sdl_smithing_sheet_clear();

    if (choice < 0 || (object_ego_prefix(smith_o_ptr) == old_prefix
        && object_ego_suffix(smith_o_ptr) == old_suffix))
    {
        *smith_o_ptr = original;
        smith_alloy = original_alloy;
        return false;
    }
    return true;
}

/*
 * Copies an artefact structure over the top of another one.
 */
void artefact_copy(artefact_type* a1_ptr, artefact_type* a2_ptr)
{
    /* Copy the structure */
    memcpy(a1_ptr, a2_ptr, sizeof(artefact_type));
}

/*
 * Fills in the details on the artefact type being created.
 */
void add_artefact_details(void)
{
    smith_a_ptr->tval = smith_o_ptr->tval;
    smith_a_ptr->sval = smith_o_ptr->sval;
    smith_a_ptr->pval = smith_o_ptr->pval;
    smith_a_ptr->att = smith_o_ptr->att;
    smith_a_ptr->evn = smith_o_ptr->evn;
    smith_a_ptr->dd = smith_o_ptr->dd;
    smith_a_ptr->ds = smith_o_ptr->ds;
    smith_a_ptr->pd = smith_o_ptr->pd;
    smith_a_ptr->ps = smith_o_ptr->ps;
    smith_a_ptr->weight = smith_o_ptr->weight;
    smith_a_ptr->flags1 |= (&k_info[smith_o_ptr->k_idx])->flags1;
    smith_a_ptr->flags2 |= (&k_info[smith_o_ptr->k_idx])->flags2;
    smith_a_ptr->flags3 |= (&k_info[smith_o_ptr->k_idx])->flags3;

    memcpy(smith_a_ptr->stat_bonus, smith_o_ptr->stat_bonus, sizeof(smith_a_ptr->stat_bonus));
    memcpy(smith_a_ptr->skill_bonus, smith_o_ptr->skill_bonus, sizeof(smith_a_ptr->skill_bonus));
    memset(smith_a_ptr->stat_bonus_set, 0, sizeof(smith_a_ptr->stat_bonus_set));
    memset(smith_a_ptr->skill_bonus_set, 0, sizeof(smith_a_ptr->skill_bonus_set));

    smith_a_ptr->cur_num = 1;
    smith_a_ptr->found_num = 1;
    smith_a_ptr->spawn_num = 1;
    smith_a_ptr->level = object_difficulty(smith_o_ptr);
    smith_a_ptr->rarity = 10;
}

/*
 * Prepares an artefact for modification.
 */
void prepare_artefact(void)
{
    int i;

    log_debug("Preparing artifact for modification");

    // retrieve a backup of the artefact
    artefact_copy(smith_a_ptr, smith2_a_ptr);

    // retrieve a backup of the object
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    // set its 'artefact' name to reflect the chosen type
    smith_o_ptr->name1 = smith_a_name;

    // Restore default stack sizes for arrows and other throwable gear
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;

    log_trace("Artifact preparation complete - %d abilities synchronized", smith_a_ptr->abilities);
}

static bool smith_property_is_listed(const smithing_flag_desc* property)
{
    return property->flagset != 1 || property->flag != TR1_SHARPNESS2
        || (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR);
}

static bool smith_artefact_has_property(const artefact_type* art,
    const smithing_flag_desc* property)
{
    if (!art) return false;
    const u32b flags[] = { 0, art->flags1, art->flags2, art->flags3, art->flags4 };
    return (flags[property->flagset] & property->flag) != 0;
}

static bool smith_kind_has_property(const object_type* item,
    const smithing_flag_desc* property)
{
    const object_kind* kind = &k_info[item->k_idx];
    const u32b flags[] = { 0, kind->flags1, kind->flags2, kind->flags3, kind->flags4 };
    return (flags[property->flagset] & property->flag) != 0;
}

static bool smith_property_applicable(const smithing_flag_desc* property,
    const object_type* item)
{
    /* Telchar may always put Sharpness II on a melee weapon. */
    if (property->flagset == 1 && property->flag == TR1_SHARPNESS2)
    {
        switch (item->tval)
        {
            case TV_SWORD: case TV_HAFTED:
            case TV_POLEARM: case TV_DIGGING:
                return true;
        }
    }

    /* Warhammers-only: Smithing bonus requires Brand Fire on the same item. */
    if (property->flagset == 1 && property->flag == TR1_SMT)
    {
        if (item->tval != TV_HAFTED || item->sval != SV_WAR_HAMMER)
            return false;
        u32b f1, f2, f3;
        object_flags(item, &f1, &f2, &f3);
        return (f1 & TR1_BRAND_FIRE) != 0;
    }

    /* Match properties carried by existing artefacts of this item type. */
    for (int i = ART_ULTIMATE; i < z_info->art_norm_max; i++)
        if (a_info[i].tval == item->tval && smith_artefact_has_property(&a_info[i], property))
            return true;
    return false;
}

/*
 * Adds a given flag to the dummy artefact.
 */
void add_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Adding artifact flag %u in flagset %d", f, flagset);

    // prepare the artefact and object for modification
    prepare_artefact();

    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    // set new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 |= f;
    if (flagset == 2)
        smith_a_ptr->flags2 |= f;
    if (flagset == 3)
        smith_a_ptr->flags3 |= f;
    if (flagset == 4)
        smith_a_ptr->flags4 |= f;

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
    if (!(f1_before & TR1_TUNNEL) && (f1_after & TR1_TUNNEL))
        smith_o_ptr->pval = MAX(1, smith_o_ptr->pval);
}

/*
 * Removes a given flag from the dummy artefact.
 */
void remove_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Removing artifact flag %u from flagset %d", f, flagset);

    // prepare the artefact and object for modification
    prepare_artefact();

    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    // unset new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 &= ~(f);
    if (flagset == 2)
        smith_a_ptr->flags2 &= ~(f);
    if (flagset == 3)
        smith_a_ptr->flags3 &= ~(f);
    if (flagset == 4)
        smith_a_ptr->flags4 &= ~(f);

    /* Keep Smithing dependent on Brand Fire. */
    if ((flagset == 1) && (f == TR1_BRAND_FIRE))
        smith_a_ptr->flags1 &= ~(TR1_SMT);

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
    if ((f1_before & TR1_TUNNEL) && !(f1_after & (TR1_TUNNEL | TR1_DAMAGE_SIDES)))
        smith_o_ptr->pval = k_info[smith_o_ptr->k_idx].pval;
}

/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_flag_menu_aux(int category, int* highlight)
{
    struct property_row {
        const smithing_flag_desc* property;
        const smith_number_card* number;
        bool inherent, added;
        byte attr;
    } rows[MAX_SMITHING_FLAGS];
    int num = 0;
    smith_number_card bonuses[SMITH_BONUS_NUMBER_MAX];
    smith_sheet_layout sheet = smith_sheet_geometry();
    const smithing_cost_type original_cost = smithing_cost;
    prepare_artefact();
    int bonus_count = smith_collect_bonus_numbers(bonuses);

    for (const smithing_flag_desc* property = smithing_flag_types; property->flag; property++)
    {
        if (property->category != category || !smith_property_is_listed(property))
            continue;

        bool inherent = smith_kind_has_property(smith_o_ptr, property);
        bool added = !inherent && smith_artefact_has_property(smith2_a_ptr, property);
        if (!inherent && !added && !smith_property_applicable(property, smith_o_ptr))
            continue;

        struct property_row* row = &rows[num++];
        *row = (struct property_row){ .property = property,
            .inherent = inherent, .added = added, .attr = TERM_L_BLUE };
        if ((inherent || added) && property->flagset == 1)
        {
            for (int j = 0; j < bonus_count; j++)
                if (bonuses[j].flags & property->flag) row->number = &bonuses[j];
        }
        if (!inherent && !added)
        {
            add_artefact_flag(property->flag, property->flagset);
            row->attr = affordable(smith_o_ptr) ? TERM_WHITE : TERM_SLATE;
            prepare_artefact();
        }
    }

    *highlight = MAX(1, MIN(*highlight, num));
    const struct property_row* selected = num ? &rows[*highlight - 1] : NULL;
    cptr verb = NULL, remove = NULL;
    char remove_label[40];
    if (selected && selected->number && selected->number->editable)
    {
        verb = ui_text("Edit");
        if (selected->added)
        {
            char key[16] = "-";
            if (steamdeck_controls_active())
                controller_prompt_label(steamdeck_alt_action_key(), "X", key, sizeof(key));
            strnfmt(remove_label, sizeof(remove_label), ui_text("[%s] Remove"), key);
            remove = sdl_touch_only_device_active() || sheet.width < 40
                ? ui_text("Remove") : remove_label;
        }
    }
    else if (selected && !selected->inherent)
        verb = selected->added ? ui_text("Remove") : ui_text("Add");

    char section[80];
    strnfmt(section, sizeof(section), ui_text("Artifice  /  %s"), ui_text(smithing_flag_cats[category - 1].desc));
    smith_picker_grid grid = smith_sheet_sublist(&sheet, SMITH_SCROLL_FLAG,
        num, *highlight - 1, section, ui_text("Property"), verb, remove);
    if (!selected)
    {
        smith_ui_put_fitted(sheet.margin, grid.first, sheet.list_width, TERM_SLATE,
            ui_text("No properties can be added."));
        int key = smith_sheet_read_choice(highlight, 0);
        return key == ESCAPE || key == '4' ? -1 : 0;
    }
    for (int i = grid.top; i < num && i < grid.top + grid.visible; i++)
    {
        const struct property_row* row = &rows[i];
        cptr value = row->number ? row->number->value : row->inherent ? ui_text("Built in")
            : row->added ? ui_text("Added") : "";
        smith_sheet_choice(&sheet, &grid, i, i + 1, i, ui_text(row->property->desc),
            value, NULL, *highlight == i + 1, row->attr);
    }

    const smithing_flag_desc* property = selected->property;
    if (!selected->inherent && !selected->added)
        add_artefact_flag(property->flag, property->flagset);
    char effects[sizeof(smith_description.text)];
    smith_flag_effect_text(smith_o_ptr, property->flagset, property->flag,
        effects, sizeof(effects));
    if (!*effects && selected->number)
        strnfmt(effects, sizeof(effects), "%s %s.", selected->number->label,
            selected->number->value);
    smith_sheet_order_with_effects(&sheet, smith_o_ptr, smith_measure_design(),
        false, false, SMITH_NAME_READ_ONLY,
        *effects ? effects : ui_text("No additional effects."));
    prepare_artefact();
    smithing_cost = original_cost;
    int ch = smith_sheet_read_choice(highlight, num);
    if (steamdeck_controls_active() && ch == steamdeck_alt_action_key()) ch = '-';

    /* Choose by letter */
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        ch = '\r';
    }

    if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '6' || ch == '-')
    {
        selected = &rows[*highlight - 1];
        property = selected->property;
        if (ch != '-' && selected->number && selected->number->editable)
        {
            if (smith_number_card_menu(selected->number, SMITH_ARTIFICE, NULL))
            {
                *smith2_o_ptr = *smith_o_ptr;
                smith2_alloy = smith_alloy;
            }
            return 0;
        }

        if (selected->inherent || (ch == '-' && !selected->added))
            return 0;
        if (selected->added)
            remove_artefact_flag(property->flag, property->flagset);
        else
            add_artefact_flag(property->flag, property->flagset);
        artefact_copy(smith2_a_ptr, smith_a_ptr);
        object_copy(smith2_o_ptr, smith_o_ptr);
        smith2_alloy = smith_alloy;
        return *highlight;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu to select individual flags of a given type to
 * add to (or subtract from) an artefact.
 */
void artefact_flag_menu(int category)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    smith_ui_scroll_top[SMITH_SCROLL_FLAG] = 0;
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_flag_menu_aux(category, &highlight);

        if (choice >= 1)
        {
            // don't leave the menu
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();
}

/*
 * Can this ability be applied to any item at all?
 * Returns false for stat-only abilities like Grace/Strength/etc that have no valid item types.
 */
static bool ability_can_be_smithed(const ability_type* b_ptr)
{
    int j;

    /* Check if this ability has any valid item types defined */
    for (j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        if (b_ptr->tval[j] != 0)
            return true;
    }

    return false;
}

static bool smith_ability_can_add(const ability_type* ability, const object_type* item,
    int ability_count)
{
    if (ability_count >= SMITH_ARTEFACT_ABILITY_MAX) return false;

    for (int i = 0; i < ABILITY_TVALS_MAX; i++)
    {
        if (item->tval == ability->tval[i] && item->sval >= ability->min_sval[i]
            && item->sval <= ability->max_sval[i])
            return true;
    }

    /* Polearm Mastery also applies to items with the polearm flag. */
    if (ability->skilltype == S_MEL && ability->abilitynum == MEL_POLEARMS)
    {
        u32b f1, f2, f3;
        object_flags(item, &f1, &f2, &f3);
        return (f3 & TR3_POLEARM) != 0;
    }
    return false;
}

/*
 * Adds a given ability to the dummy artefact.
 */
void add_artefact_ability(int skilltype, int abilitynum)
{
    int i;

    log_trace("Adding artifact ability - skill:%d ability:%d", skilltype, abilitynum);

    // prepare the artefact and object for modification
    prepare_artefact();

    // set new ability on the artefact
    if (smith_a_ptr->abilities < SMITH_ARTEFACT_ABILITY_MAX)
    {
        bool already_present = false;

        for (i = 0; i < smith_a_ptr->abilities; i++)
        {
            if ((smith_a_ptr->skilltype[i] == skilltype)
                && (smith_a_ptr->abilitynum[i] == abilitynum))
            {
                already_present = true;
            }
        }

        if (!already_present)
        {
            smith_a_ptr->skilltype[smith_a_ptr->abilities] = skilltype;
            smith_a_ptr->abilitynum[smith_a_ptr->abilities] = abilitynum;
            smith_a_ptr->bane_type[smith_a_ptr->abilities] = 0; // Player-smithed banes use player choice
            smith_a_ptr->abilities++;
        }
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

/*
 * Removes a given ability from the dummy artefact.
 */
void remove_artefact_ability(int skilltype, int abilitynum)
{
    int i;
    int location = -1;

    log_trace("Removing artifact ability - skill:%d ability:%d", skilltype, abilitynum);

    // prepare the artefact and object for modification
    prepare_artefact();

    // remove new ability on the artefact
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        if ((smith_a_ptr->skilltype[i] == skilltype)
            && (smith_a_ptr->abilitynum[i] == abilitynum))
        {
            location = i;
        }
    }

    if (location >= 0)
    {
        for (i = location; i < smith_a_ptr->abilities - 1; i++)
        {
            smith_a_ptr->skilltype[i] = smith_a_ptr->skilltype[i + 1];
            smith_a_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i + 1];
            smith_a_ptr->bane_type[i] = smith_a_ptr->bane_type[i + 1];
        }

        smith_a_ptr->skilltype[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->abilitynum[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->bane_type[smith_a_ptr->abilities - 1] = 0;

        smith_a_ptr->abilities--;
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

/*
 * Determines if an artefact type has a given ability.
 */
static bool smith_artefact_has_ability(const artefact_type* art,
    const ability_type* ability)
{
    if (!art) return false;
    for (int i = 0; i < art->abilities; i++)
    {
        if (art->skilltype[i] == ability->skilltype
            && art->abilitynum[i] == ability->abilitynum)
            return true;
    }
    return false;
}

/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_ability_menu_aux(int skill, int* highlight)
{
    char ch;
    int i, num = 0;

    ability_type* b_ptr;
    smith_sheet_layout sheet = smith_sheet_geometry();

    /* Allocate arrays dynamically based on actual max abilities */
    bool* ability_present = mem_alloc_array(z_info->b_max, bool);
    bool* ability_valid = mem_alloc_array(z_info->b_max, bool);
    bool* ability_affordable = mem_alloc_array(z_info->b_max, bool);
    int* ability_nums = mem_alloc_array(z_info->b_max, int);

    /* Initialize arrays to zero/false */
    memset(ability_present, 0, z_info->b_max * sizeof(bool));
    memset(ability_valid, 0, z_info->b_max * sizeof(bool));
    memset(ability_affordable, 0, z_info->b_max * sizeof(bool));

    // list the abilities
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skill)
            continue;

        /* Skip abilities that can't be smithed onto any item (like Grace, stat improvements) */
        if (!ability_can_be_smithed(b_ptr))
            continue;

        // Store the mapping from display index to actual ability number
        ability_nums[num] = b_ptr->abilitynum;

        // Determine the appropriate colour
        if (smith_artefact_has_ability(smith2_a_ptr, b_ptr))
        {
            ability_present[num] = true;
            ability_valid[num] = true;
        }
        else if (smith_ability_can_add(b_ptr, smith_o_ptr, smith2_a_ptr->abilities))
        {
            add_artefact_ability(skill, b_ptr->abilitynum);
            ability_valid[num] = true;
            ability_affordable[num] = affordable(smith_o_ptr);
        }

        num++;
    }

    *highlight = MAX(1, MIN(*highlight, num));
    char section[80];
    strnfmt(section, sizeof(section), ui_text("Artifice  /  %s"), ui_text(skill_names_full[skill]));
    int selected = *highlight - 1;
    cptr verb = num && ability_valid[selected]
        ? (ability_present[selected] ? ui_text("Remove") : ui_text("Add")) : NULL;
    smith_picker_grid grid = smith_sheet_sublist(&sheet, SMITH_SCROLL_ABILITY,
        num, selected, section, ui_text("Ability"), verb, NULL);
    if (!num)
    {
        smith_ui_put_fitted(sheet.margin, grid.first, sheet.list_width, TERM_SLATE,
            ui_text("No abilities can be added."));
        (void)smith_sheet_read_choice(highlight, 0);
        mem_free(ability_present);
        mem_free(ability_valid);
        mem_free(ability_affordable);
        mem_free(ability_nums);
        return (-1);
    }

    for (i = grid.top; i < num && i < grid.top + grid.visible; i++)
    {
        byte attr = ability_present[i]
            ? TERM_BLUE
            : (ability_valid[i]
                    ? (ability_affordable[i] ? TERM_WHITE : TERM_SLATE)
                    : TERM_L_DARK);
        cptr name = ui_text("Ability");


        for (int j = 0; j < z_info->b_max; j++)
        {
            b_ptr = &b_info[j];
            if (b_ptr->name && b_ptr->skilltype == skill
                && b_ptr->abilitynum == ability_nums[i])
            {
                name = b_name + b_ptr->name;
                break;
            }
        }
        smith_sheet_choice(&sheet, &grid, i, i + 1, i, name,
            ability_present[i] ? ui_text("Added") : "", NULL, *highlight == i + 1,
            ability_present[i] ? TERM_L_BLUE : attr);
    }

    if (ability_valid[selected] && !ability_present[selected])
        add_artefact_ability(skill, ability_nums[selected]);
    else
        prepare_artefact();

    b_ptr = &b_info[ability_index(skill, ability_nums[selected])];
    smith_sheet_order_with_effects(&sheet, smith_o_ptr, smith_measure_design(),
        false, false, SMITH_NAME_READ_ONLY, b_ptr->effect ? ui_text(b_text + b_ptr->effect) : NULL);
    ch = smith_sheet_read_choice(highlight, num);

    /* Choose by letter */
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        ch = '\r';
    }

    /* Toggle the selected logical row from the saved baseline.  This avoids
     * committing the old preview after a one-step pointer activation. */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (ability_valid[*highlight - 1])
        {
            if (ability_present[*highlight - 1])
                remove_artefact_ability(skill, ability_nums[*highlight - 1]);
            else
                add_artefact_ability(skill, ability_nums[*highlight - 1]);

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);

            mem_free(ability_present);
            mem_free(ability_valid);
            mem_free(ability_affordable);
            mem_free(ability_nums);
            return (*highlight);
        }
        else
        {
            bell(ui_text("Invalid choice."));
        }
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        // remove any tentatively-added ability from the object
        if (!ability_present[*highlight - 1])
            remove_artefact_ability(skill, ability_nums[*highlight - 1]);

        // restore the backup artefact
        artefact_copy(smith_a_ptr, smith2_a_ptr);

        *highlight = -1;

        mem_free(ability_present);
        mem_free(ability_valid);
        mem_free(ability_affordable);
        mem_free(ability_nums);
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    mem_free(ability_present);
    mem_free(ability_valid);
    mem_free(ability_affordable);
    mem_free(ability_nums);
    return (0);
}

/*
 * Brings up a menu to select individual abilities of a given skill to
 * add to (or subtract from) an artefact.
 */
void artefact_ability_menu(int skill)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_ability_menu_aux(skill, &highlight);

        if (choice >= 1)
        {
            // don't leave the menu
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();
}

/*
 * Allows the player to choose a new name for an artefact.
 */
static bool rename_artefact(void)
{
    char name[MAX_LEN_ART_NAME];
    SDL_strlcpy(name, smith2_a_ptr->name, sizeof(name));
    do
    {
        if (!askfor_name(name, sizeof(name))) return false;
    } while (!name[0]);

    SDL_strlcpy(smith2_a_ptr->name, name, sizeof(smith2_a_ptr->name));
    artefact_copy(smith_a_ptr, smith2_a_ptr);
    p_ptr->redraw |= PR_MISC;
    return true;
}

static int smith_artifice_jewellery_sval(int tval)
{
    if (tval == TV_RING) return SV_RING_SELF_MADE;
    if (tval == TV_AMULET) return SV_AMULET_SELF_MADE;
    return 0;
}

static void smith_prepare_artifice_design(void)
{
    if (!smith_o_ptr->name1)
    {
        log_debug("Initializing new artifact creation");
        // wipe the existing artefact (and its backup)
        artefact_wipe(smith_a_name);
        artefact_wipe(smith2_a_name);

        // add 'ignore all'
        smith2_a_ptr->flags3 |= (TR3_IGNORE_MASK);

        /* Rings and amulets use their custom base when Artifice begins. */
        int sval = smith_artifice_jewellery_sval(smith_o_ptr->tval);
        if (sval)
        {
            create_base_object(smith_o_ptr->tval, sval);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
            if (smith_o_ptr->tval == TV_AMULET) smith2_o_ptr->pd = 1;
        }
    }

    // set the backup artefact name to the player character's name
    if (strlen(smith2_a_ptr->name) == 0)
    {
        strnfmt(smith2_a_ptr->name, MAX_LEN_ART_NAME, "of %s", op_ptr->full_name);
    }

    // prepare the artefact and object for modification
    prepare_artefact();

}

/*
 * Performs the interface and selection work for the melting menu.
 */
int melt_menu_aux(int* highlight)
{
    char ch;
    int i;
    int num = 0;
    object_type* o_ptr;
    char desc[80];
    smith_sheet_layout sheet = smith_sheet_geometry();
    smith_ui_detail_valid = false;
    while (smith_melt_item_handle_for_choice(num + 1) >= 0)
        num++;

    *highlight = MAX(1, MIN(*highlight, num));
    smith_picker_grid grid = smith_sheet_sublist(&sheet, SMITH_SCROLL_MELT, num,
        *highlight - 1, ui_text("Melting"), ui_text("Carried metalwork"),
        num ? ui_text("Melt") : NULL, NULL);
    if (!num)
    {
        smith_ui_put_fitted(sheet.margin, grid.first, sheet.list_width, TERM_SLATE,
            ui_text("No carried metal items can be melted."));
        (void)smith_sheet_read_choice(highlight, 0);
        return -1;
    }
    for (i = grid.top; i < num && i < grid.top + grid.visible; i++)
    {
        int handle = smith_melt_item_handle_for_choice(i + 1);
        o_ptr = player_inventory_object(handle);
        object_desc(desc, sizeof(desc), o_ptr, false, 0);
        smith_sheet_choice(&sheet, &grid, i, i + 1, i, desc,
            format(ui_text("%d.%d lb"), o_ptr->weight / 10, o_ptr->weight % 10), o_ptr,
            i + 1 == *highlight, TERM_WHITE);
    }
    o_ptr = player_inventory_object(smith_melt_item_handle_for_choice(*highlight));
    int x = (sheet.divider >= 0 ? sheet.divider : 0) + sheet.margin;
    int w = sheet.width - sheet.margin - x, y = sheet.summary;
    u32b f1, f2, f3;
    object_flags(o_ptr, &f1, &f2, &f3);
    bool iron = (f3 & TR3_STAR_IRON) != 0;
    smith_ui_put_fitted(x, y++, w, TERM_WHITE + TERM_SHADE, ui_text("Metal recovery"));
    if (sheet.footer - y >= 4)
    {
        object_desc(desc, sizeof(desc), o_ptr, false, 0);
        if (!sdl_smithing_artwork_set(o_ptr, x, y, 2, 1)) draw_supply_icon(x, y, o_ptr);
        smith_ui_put_fitted(x + 3, y++, w - 3, TERM_L_WHITE, desc);
        y++;
    }
    smith_sheet_field(x, y++, w, iron ? ui_text("Star iron") : "Mithril",
        format(ui_text("%d.%d lb"), o_ptr->weight / 10, o_ptr->weight % 10), TERM_WHITE, TERM_L_WHITE);
    if (y < sheet.footer)
    {
        int carried = iron ? star_iron_carried() : mithril_carried();
        smith_sheet_field(x, y, w, ui_text("Already carried"), format(ui_text("%d.%d lb"), carried / 10,
            carried % 10), TERM_WHITE, TERM_L_WHITE);
    }
    Term_fresh();
    hide_cursor = true;
    ch = steamdeck_menu_key(inkey(), 0, 0);
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (smith_ui_take_click_action(&clicked_choice, &click_action))
        {
            if (clicked_choice == SMITH_CLICK_BACK)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice == SMITH_EDITOR_APPLY)
                ch = click_action == UI_MENU_CLICK_PRIMARY ? '\r' : 0;
            else if (clicked_choice >= 1 && clicked_choice <= num)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;

                if (click_action == UI_MENU_CLICK_SECONDARY)
                {
                    int item = smith_melt_item_handle_for_choice(
                        clicked_choice);

                    if (item >= 0)
                    {
                        *highlight = clicked_choice;
                        describe_item_with_comparisons(item, true);
                    }
                    return 0;
                }

                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    // choose an option by letter
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        return (-1);
    }

    return (0);
}

/*
 * Produces the menu for melting down mithril and star-iron items into their metal pieces.
 */
void melt_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = melt_menu_aux(&highlight);

        if (choice >= 1)
        {
            if (melt_metal_item(choice))
            {
                leave_menu = true;
            }
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();

    sdl_smithing_sheet_clear();
}

static bool smith_item_tester_hook_reforge_target(const object_type* o_ptr)
{
    return object_can_repair_damage(o_ptr)
        || object_can_preview_reforge_prefix(o_ptr);
}

static bool smith_design_allows(int action)
{
    if (!smith_o_ptr->tval) return false;
    switch (action)
    {
    case SMT_MENU_ENCHANT:
        return !smith_o_ptr->name1 && !enchant_then_numbers
            && smith_o_ptr->tval != TV_HORN
            && !(smith_o_ptr->tval == TV_DIGGING && smith_o_ptr->sval == SV_SHOVEL);
    case SMT_MENU_ARTEFACT:
        return !object_has_ego(smith_o_ptr) && smith_o_ptr->tval != TV_HORN
            && p_ptr->self_made_arts < z_info->art_self_made_max - z_info->art_rand_max - 2;
    case SMT_MENU_ACCEPT:
        return cave_forge_bold(p_ptr->py, p_ptr->px)
            && forge_uses(p_ptr->py, p_ptr->px) > 0 && affordable(smith_o_ptr);
    case SMT_MENU_NUMBERS:
        return true;
    default:
        return false;
    }
}

static void smith_root_build_entries(bool valid[SMT_MENU_MAX],
    byte menu_attr[SMT_MENU_MAX], char labels[SMT_MENU_MAX][32])
{
    bool at_forge = cave_forge_bold(p_ptr->py, p_ptr->px);
    int uses = at_forge ? forge_uses(p_ptr->py, p_ptr->px) : 0;
    bool has_item = (smith_o_ptr->tval != 0);
    int reforge_target = find_reforge_target_item();
    byte valid_attr;

    SDL_strlcpy(labels[SMT_MENU_CREATE - 1], ui_text("Change base item"), 32);
    SDL_strlcpy(labels[SMT_MENU_ENCHANT - 1], ui_text("Enhancements"), 32);
    SDL_strlcpy(labels[SMT_MENU_ARTEFACT - 1], ui_text("Artifice"), 32);
    SDL_strlcpy(labels[SMT_MENU_NUMBERS - 1], ui_text("Edit item"), 32);
    SDL_strlcpy(labels[SMT_MENU_MELT - 1], ui_text("Melt"), 32);
    SDL_strlcpy(labels[SMT_MENU_REPAIR - 1], ui_text("Reforge"), 32);
    SDL_strlcpy(labels[SMT_MENU_ACCEPT - 1],
        (p_ptr->smithing_leftover == 0) ? ui_text("Craft") : ui_text("Resume"), 32);

    valid[SMT_MENU_CREATE - 1] = true;
    valid[SMT_MENU_ENCHANT - 1] = smith_design_allows(SMT_MENU_ENCHANT);
    valid[SMT_MENU_ARTEFACT - 1] = smith_design_allows(SMT_MENU_ARTEFACT);
    valid[SMT_MENU_NUMBERS - 1] = has_item;
    valid[SMT_MENU_MELT - 1] = meltable_metal_items_carried() && at_forge;
    valid[SMT_MENU_REPAIR - 1] = (reforge_target >= 0);
    valid[SMT_MENU_ACCEPT - 1] = smith_design_allows(SMT_MENU_ACCEPT);

    if (smith_pending_reforge())
    {
        for (int i = 0; i < SMT_MENU_MAX; ++i)
            valid[i] = false;
        /* Acceptance may have consumed the final forge use. */
        valid[SMT_MENU_ACCEPT - 1] = at_forge;
    }

    valid_attr = (p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        ? TERM_WHITE
        : TERM_RED;
    menu_attr[SMT_MENU_CREATE - 1]
        = valid[SMT_MENU_CREATE - 1] ? valid_attr : TERM_L_DARK;

    valid_attr = p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] ? TERM_WHITE
                                                               : TERM_RED;
    menu_attr[SMT_MENU_ENCHANT - 1]
        = valid[SMT_MENU_ENCHANT - 1] ? valid_attr : TERM_L_DARK;

    valid_attr = p_ptr->active_ability[S_SMT][SMT_ARTEFACT] ? TERM_WHITE
                                                            : TERM_RED;
    menu_attr[SMT_MENU_ARTEFACT - 1]
        = valid[SMT_MENU_ARTEFACT - 1] ? valid_attr : TERM_L_DARK;

    menu_attr[SMT_MENU_NUMBERS - 1]
        = valid[SMT_MENU_NUMBERS - 1] ? TERM_WHITE : TERM_L_DARK;
    menu_attr[SMT_MENU_MELT - 1]
        = valid[SMT_MENU_MELT - 1] ? TERM_WHITE : TERM_L_DARK;

    valid_attr = (p_ptr->active_ability[S_SMT][SMT_REPAIR] && at_forge
                     && (uses > 0))
        ? TERM_WHITE
        : TERM_RED;
    menu_attr[SMT_MENU_REPAIR - 1]
        = valid[SMT_MENU_REPAIR - 1] ? valid_attr : TERM_L_DARK;

    menu_attr[SMT_MENU_ACCEPT - 1]
        = valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK;
}

static void smith_root_note_for_choice(int choice, bool valid, char* buf,
    size_t buflen)
{
    bool at_forge = cave_forge_bold(p_ptr->py, p_ptr->px);
    int uses = at_forge ? forge_uses(p_ptr->py, p_ptr->px) : 0;

    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    if (smith_pending_reforge())
    {
        if (choice != SMT_MENU_ACCEPT)
            SDL_strlcpy(buf, ui_text("Finish the current reforging work first."), buflen);
        else if (!at_forge)
            SDL_strlcpy(buf, ui_text("Resume at a forge; costs are already paid."), buflen);
        return;
    }

    switch (choice)
    {
    case SMT_MENU_CREATE:
        if (!(p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                || p_ptr->active_ability[S_SMT][SMT_JEWELLER]))
        {
            SDL_strlcpy(buf, ui_text("No smithing craft is learned yet."), buflen);
        }
        break;
    case SMT_MENU_ENCHANT:
        if (!smith_o_ptr->tval)
            SDL_strlcpy(buf, ui_text("Select a base item first."), buflen);
        else if (smith_o_ptr->name1)
            SDL_strlcpy(buf, ui_text("Artifice already claims this work."), buflen);
        else if (enchant_then_numbers)
            SDL_strlcpy(buf, ui_text("This enchanted item has custom stats."), buflen);
        else if (smith_o_ptr->tval == TV_HORN)
            SDL_strlcpy(buf, ui_text("Horns cannot take this enchantment path."), buflen);
        else if ((smith_o_ptr->tval == TV_DIGGING)
            && (smith_o_ptr->sval == SV_SHOVEL))
        {
            SDL_strlcpy(buf, ui_text("Shovels cannot be enchanted here."), buflen);
        }
        else if (!p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT])
            SDL_strlcpy(buf, ui_text("Requires Enchantment."), buflen);
        break;
    case SMT_MENU_ARTEFACT:
        if (!smith_o_ptr->tval)
            SDL_strlcpy(buf, ui_text("Select a base item first."), buflen);
        else if (object_has_ego(smith_o_ptr))
            SDL_strlcpy(buf, ui_text("Remove the enchantment choice first."), buflen);
        else if (smith_o_ptr->tval == TV_HORN)
            SDL_strlcpy(buf, ui_text("Horns cannot be made into custom artefacts."), buflen);
        else if (!p_ptr->active_ability[S_SMT][SMT_ARTEFACT])
            SDL_strlcpy(buf, ui_text("Requires Artifice."), buflen);
        else if (!valid)
            SDL_strlcpy(buf, ui_text("Self-made artefact limit reached."), buflen);
        break;
    case SMT_MENU_NUMBERS:
        if (!smith_o_ptr->tval)
            SDL_strlcpy(buf, ui_text("Select a base item first."), buflen);
        break;
    case SMT_MENU_MELT:
        if (!at_forge)
            SDL_strlcpy(buf, ui_text("Requires a forge."), buflen);
        else if (!meltable_metal_items_carried())
            SDL_strlcpy(buf, ui_text("Carry mithril or star-iron gear to melt."), buflen);
        break;
    case SMT_MENU_REPAIR:
        if (find_reforge_target_item() < 0)
            SDL_strlcpy(buf, ui_text("Carry damaged or prefixable gear."), buflen);
        else if (!at_forge)
            SDL_strlcpy(buf, ui_text("Preview only away from a forge."), buflen);
        else if (uses <= 0)
            SDL_strlcpy(buf, ui_text("This forge is exhausted."), buflen);
        else if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
            SDL_strlcpy(buf, ui_text("Requires Reforging."), buflen);
        break;
    case SMT_MENU_ACCEPT:
        if (!smith_o_ptr->tval)
            SDL_strlcpy(buf, ui_text("Select a base item first."), buflen);
        else if (!at_forge)
            SDL_strlcpy(buf, ui_text("Requires a forge."), buflen);
        else if (uses <= 0)
            SDL_strlcpy(buf, ui_text("This forge is exhausted."), buflen);
        else if (object_has_evil_alignment(smith_o_ptr))
            SDL_strlcpy(buf, ui_text("This work is too fell to make."), buflen);
        else if (!valid)
            SDL_strlcpy(buf, ui_text("The design is beyond your current means."), buflen);
        break;
    }
}

static int smith_work_actions(int actions[5])
{
    if (smith_pending_reforge())
    {
        actions[0] = SMT_MENU_ACCEPT;
        return 1;
    }
    int count = 0;
    if (smith_o_ptr->k_idx)
    {
        actions[count++] = SMT_MENU_NUMBERS;
        actions[count++] = SMT_MENU_ACCEPT;
    }
    actions[count++] = SMT_MENU_CREATE;
    actions[count++] = SMT_MENU_MELT;
    actions[count++] = SMT_MENU_REPAIR;
    return count;
}

static bool smith_begin_reforge(int slot, int prefix_idx)
{
    reforge_preview_type preview;
    object_type source;
    object_type* target = player_inventory_object(slot);

    if (smith_pending_reforge() || !target || !target->k_idx
        || target->number == 0 || !object_can_preview_reforge_prefix(target))
        return false;
    if (player_equipment_slot_counts_as_equipped(slot) && cursed_p(target))
    {
        msg_print("You cannot remove that cursed item to reforge it.");
        return false;
    }

    object_copy(&source, target);
    /* The operation works on one item, even when its source is a stack. */
    source.number = 1;
    if (!reforge_preview_build(&source, prefix_idx, &preview)
        || !preview.affordable)
        return false;

    smith_o_ptr->unused1 = 2;
    smith_o_ptr->pickup = false;
    smith_o_ptr->pickup_slot = -1;
    smith_o_ptr->iy = smith_o_ptr->ix = 0;
    smith_o_ptr->next_o_idx = smith_o_ptr->held_m_idx = 0;
    smith_clear_alloy_state(&smith_alloy);

    /* Withdraw before payment: consuming metal may compact the source slot. */
    inven_item_increase(slot, -1);
    inven_item_optimize(slot);
    pay_smithing_cost_struct(&preview.cost);
    p_ptr->smithing_leftover = preview.turns;
    p_ptr->window |= PW_INVEN | PW_EQUIP;
    p_ptr->redraw |= PR_BASIC | PR_EQUIPPY;
    return true;
}

static bool smith_reforge_item(void)
{
    if (!tutorial_game_action_allowed("smith", NULL)) return false;
    if (smith_pending_reforge())
    {
        msg_print("Finish your current reforging work first.");
        return false;
    }
    int slot = -1;
    int prefix_idx = 0;
    char old_name[80];
    char new_name[80];
    object_type smith_backup;
    object_type smith2_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smith_alloy_state alloy2_backup = smith2_alloy;

    screen_save();
    item_tester_hook = smith_item_tester_hook_reforge_target;
    bool selected = open_inventory_item_select_menu(USE_EQUIP | USE_INVEN,
        ui_text("Reforge which item? "), ui_text("You have nothing to repair or reforge."), &slot);
    item_tester_hook = NULL;
    screen_load();
    ui_menu_click_clear();
    ui_scroll_area_clear();
    if (!selected) return false;

    object_type* target = player_inventory_object(slot);
    if (!target || !target->k_idx)
        return false;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(&smith2_backup, smith2_o_ptr);

    if (object_can_repair_damage(target))
    {
        if (!cave_forge_bold(p_ptr->py, p_ptr->px))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell(ui_text("You can only reforge items at a forge."));
            return false;
        }

        if (forge_uses(p_ptr->py, p_ptr->px) <= 0)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell(ui_text("This forge has no resources left."));
            return false;
        }

        if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell(ui_text("You do not know how to reforge gear."));
            return false;
        }

        if (!repair_damaged_item(slot))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell(ui_text("You cannot repair that item."));
            return false;
        }

        spend_forge_uses(1);
        lite_spot(p_ptr->py, p_ptr->px);

        object_desc(new_name, sizeof(new_name), target, true, 0);
        msg_format("You repair %s.", new_name);
    }
    else
    {
        reforge_preview_type preview;

        if (!object_can_preview_reforge_prefix(target))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell(ui_text("You cannot reforge that item."));
            return false;
        }

        prefix_idx = reforge_prefix_menu(target);
        if (!prefix_idx)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            return false;
        }

        if (!reforge_preview_build(target, prefix_idx, &preview)
            || !preview.affordable)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell(ui_text("You cannot afford that reforge."));
            return false;
        }

        object_desc(old_name, sizeof(old_name), target, true, 0);
        if (!smith_begin_reforge(slot, prefix_idx))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell(ui_text("You cannot reforge that item."));
            return false;
        }

        /* Keep the accepted item in saved work, rather than the preview backup. */
        object_copy(smith2_o_ptr, smith_o_ptr);
        smith2_alloy = smith_alloy;
        msg_format("You set %s aside for reforging.", old_name);
        return true;
    }

    object_copy(smith_o_ptr, &smith_backup);
    object_copy(smith2_o_ptr, &smith2_backup);
    smith_alloy = alloy_backup;
    smith2_alloy = alloy2_backup;

    p_ptr->redraw |= PR_BASIC;
    return true;
}

typedef struct {
    cptr label;
    char value[64];
    bool enabled;
    bool prefix;
    bool indented;
    const smith_number_card* number;
} smith_editor_card;

#define SMITH_EDITOR_MAX MAX(2 + 2 * SMITH_BONUS_NUMBER_MAX, MAX_CATS + S_SPC)

static bool smith_edit_artifice(int index)
{
    object_type original = *smith_o_ptr, original_base = *smith2_o_ptr;
    smith_alloy_state original_alloy = smith_alloy, base_alloy = smith2_alloy;
    artefact_type original_art = *smith_a_ptr, original_art_base = *smith2_a_ptr;

    smith_prepare_artifice_design();
    artefact_type before = *smith2_a_ptr;
    object_type before_object = *smith2_o_ptr;
    bool named_new_design = false;
    if (index == SMITH_EDITOR_RENAME)
        named_new_design = rename_artefact() && !original.name1;
    else if (index < MAX_CATS)
        artefact_flag_menu(smithing_flag_cats[index].category);
    else
        artefact_ability_menu(index - MAX_CATS);

    /* Previewing a property can temporarily change the object's kind (jewellery),
     * flags and abilities. Back without an edit preserves the exact draft. */
    if (!named_new_design && !memcmp(&before, smith2_a_ptr, sizeof(before))
        && !memcmp(&before_object, smith2_o_ptr, sizeof(before_object)))
    {
        *smith_o_ptr = original;
        *smith2_o_ptr = original_base;
        smith_alloy = original_alloy;
        smith2_alloy = base_alloy;
        *smith_a_ptr = original_art;
        *smith2_a_ptr = original_art_base;
        return false;
    }
    prepare_artefact();
    return true;
}

static void smith_editor_number(smith_editor_card* card,
    const smith_number_card* number, bool indented)
{
    *card = (smith_editor_card){ .label = number->label, .enabled = number->editable,
        .indented = indented, .number = number };
    if (number->flags == TR1_TUNNEL && !number->editable)
        strnfmt(card->value, sizeof(card->value), "%s (%s)", number->value, ui_text("Fixed"));
    else
        SDL_strlcpy(card->value, number->value, sizeof(card->value));
}

typedef struct {
    int added, available;
} smith_artifice_count;

static int smith_artifice_cards(bool allowed, bool compact, smith_editor_card* cards)
{
    smith_artifice_count properties[MAX_CATS] = { 0 }, abilities[S_SPC] = { 0 };
    object_type item = *smith_o_ptr;
    const artefact_type* art = item.name1 ? smith_a_ptr : NULL;
    int ability_count = art ? art->abilities : 0;
    int jewellery_sval = smith_artifice_jewellery_sval(item.tval);
    if (!art && allowed && jewellery_sval)
    {
        item.sval = jewellery_sval;
        item.k_idx = lookup_kind(item.tval, item.sval);
    }

    /* Count options that can be toggled, including ones already added. */
    for (const smithing_flag_desc* property = smithing_flag_types; property->flag; property++)
    {
        if (!smith_property_is_listed(property)) continue;
        smith_artifice_count* count = &properties[property->category - 1];
        bool inherent = smith_kind_has_property(&item, property);
        bool added = !inherent && smith_artefact_has_property(art, property);
        count->added += added;
        count->available += added || (allowed && (inherent
                ? property->flagset == 1 && (property->flag & TR1_PVAL_MASK)
                : smith_property_applicable(property, &item)));
    }
    for (int i = 0; i < z_info->b_max; i++)
    {
        const ability_type* ability = &b_info[i];
        if (!ability->name || ability->skilltype >= S_SPC || !ability_can_be_smithed(ability))
            continue;
        smith_artifice_count* count = &abilities[ability->skilltype];
        bool added = smith_artefact_has_ability(art, ability);
        count->added += added;
        count->available += added || (allowed && smith_ability_can_add(ability, &item, ability_count));
    }

    for (int i = 0; i < MAX_CATS; i++)
    {
        smith_editor_card* card = &cards[i];
        smith_artifice_count count = properties[i];
        card->label = ui_text(smithing_flag_cats[i].desc);
        card->enabled = allowed;
        strnfmt(card->value, sizeof(card->value), "%d / %d%s", count.added,
            count.available, compact ? "" : count.available == 1 ? ui_text(" property") : ui_text(" properties"));
    }
    for (int i = 0; i < S_SPC; i++)
    {
        smith_editor_card* card = &cards[MAX_CATS + i];
        smith_artifice_count count = abilities[i];
        card->label = ui_text(skill_names_full[i]);
        card->enabled = allowed;
        strnfmt(card->value, sizeof(card->value), "%d / %d%s", count.added,
            count.available, compact ? "" : count.available == 1 ? ui_text(" ability") : ui_text(" abilities"));
    }
    return MAX_CATS + S_SPC;
}

static int smith_editor_cards(smith_page page, bool allowed, bool compact,
    smith_number_card *numbers, smith_editor_card *cards)
{
    if (page == SMITH_SIMPLE)
    {
        int count = smith_collect_simple_numbers(numbers);
        for (int i = 0; i < count; i++)
            smith_editor_number(&cards[i], &numbers[i], false);
        return count;
    }
    if (page == SMITH_ENHANCEMENTS)
    {
        int prefix = object_ego_prefix(smith_o_ptr);
        int suffix = object_ego_suffix(smith_o_ptr);
        int bonuses = smith_collect_bonus_numbers(numbers);
        u32b simple = smith_has_builtin_tunneling(smith_o_ptr) ? TR1_TUNNEL : 0;
        u32b placed = simple;
        if (smith_o_ptr->name1)
        {
            /* Added artefact powers have their own property rows. */
            for (int i = 0; smithing_flag_types[i].flag; i++)
                if (smithing_flag_types[i].flagset == 1)
                    placed |= smith_a_ptr->flags1 & smithing_flag_types[i].flag;
        }
        int count = 0;
        for (int i = 0; i < 2; i++)
        {
            int affix = i == 0 ? prefix : suffix;
            u32b flags = affix ? e_info[affix].flags1 : 0;
            smith_editor_card* slot = &cards[count++];
            slot->label = i == 0 ? ui_text("Prefix") : ui_text("Suffix");
            slot->prefix = i == 0;
            SDL_strlcpy(slot->value, ui_text("None"), sizeof(slot->value));
            if (affix)
                ego_name_for_enchant_menu(affix, slot->value, sizeof(slot->value));
            slot->enabled = allowed && (affix ||
                enchant_menu_has_applicable_affix(smith2_o_ptr,
                    i == 0 ? 0 : prefix, i == 0 ? suffix : 0, i == 0));
            for (int j = 0; j < bonuses; j++)
                if (numbers[j].flags & flags & ~simple)
                    smith_editor_number(&cards[count++], &numbers[j], true);
            placed |= flags;
        }
        /* Intrinsic and carried-over bonuses also have an editor here. */
        for (int i = 0; i < bonuses; i++)
            if (!(numbers[i].flags & placed))
                smith_editor_number(&cards[count++], &numbers[i], false);
        return count;
    }

    return smith_artifice_cards(allowed, compact, cards);
}

static cptr smith_fixed_number_note(const smith_number_card* number)
{
    if (!number || number->editable || number->flags != TR1_TUNNEL) return NULL;
    if (!smith_o_ptr->name1 && !object_has_ego(smith_o_ptr)
        && smith_design_allows(SMT_MENU_ARTEFACT))
    {
        if (smith_o_ptr->sval == SV_MATTOCK)
            return ui_text("Raise with Artifice or the Stone-hewers.");
        return ui_text("Use Artifice to raise tunneling.");
    }
    return ui_text("Tunneling is fixed for this design.");
}

static void smith_editor_context(const smith_sheet_layout* sheet, int row,
    const smith_editor_card* card, cptr note)
{
    int x = sheet->margin, w = sheet->list_width, end = sheet->list_end;
    if (row >= end) return;
    if (note[0])
        row += smith_ui_put_wrapped(x, row, w, MIN(2, end - row), TERM_L_WHITE, note);
    if (card && card->number && card->enabled && row < end)
    {
        const smith_number_card* field = card->number;
        char lo[32], hi[32];
        smith_format_number(field, field->first, lo, sizeof(lo));
        smith_format_number(field, field->first + field->step * (field->count - 1),
            hi, sizeof(hi));
        if (field->kind == SMITH_NUMBER_ALLOY)
            smith_ui_put_fitted(x, row++, w, TERM_L_WHITE, ui_text("None, Mithril, Star iron"));
        else
            smith_sheet_field(x, row++, w, ui_text("Available range"), format(ui_text("%s to %s"), lo, hi),
                TERM_L_WHITE, TERM_L_BLUE);
    }
    else if (card && row < end)
    {
        cptr hint = smith_fixed_number_note(card->number);
        if (hint) row += smith_ui_put_wrapped(x, row, w, MIN(2, end - row), TERM_WHITE, hint);
    }
    smith_sheet_stock(x, row, w, end);
}

static bool smith_editor(smith_page page, bool death_view)
{
    int focus = 0;
    int next_page = -1;
    bool craft = false;
    smith_ui_scroll_top[SMITH_SCROLL_EDITOR] = 0;
    screen_save();
    for (;;)
    {
        if (next_page >= 0)
        {
            page = (smith_page)next_page;
            focus = 0;
            smith_ui_scroll_top[SMITH_SCROLL_EDITOR] = 0;
            next_page = -1;
        }
        bool craftable = !death_view && smith_design_allows(SMT_MENU_ACCEPT);
        int action = page == SMITH_ARTIFICE ? SMT_MENU_ARTEFACT
            : page == SMITH_ENHANCEMENTS ? SMT_MENU_ENCHANT : SMT_MENU_NUMBERS;
        char note[120];
        bool allowed = smith_design_allows(action);
        smith_root_note_for_choice(action, allowed, note, sizeof(note));
        smith_number_card numbers[MAX(SMITH_SIMPLE_NUMBER_MAX, SMITH_BONUS_NUMBER_MAX)];
        smith_editor_card cards[SMITH_EDITOR_MAX] = { 0 };
        smith_sheet_layout sheet = smith_sheet_geometry();
        int cols = page == SMITH_ARTIFICE && sheet.list_width >= 62 ? 2 : 1;
        bool compact = sheet.list_width / cols < (sdl_menu_letters_enabled() ? 38 : 35);
        int count = smith_editor_cards(page, allowed, compact, numbers, cards);
        bool can_rename = page == SMITH_ARTIFICE ? allowed
            : smith_o_ptr->name1 == smith_a_name && smith_design_allows(SMT_MENU_ARTEFACT);
        int name_focus = can_rename ? count : -1;
        int first_action = count + can_rename;
        int last = first_action + 2;
        focus = MIN(focus, last);
        const smith_editor_card* card = focus < count ? &cards[focus] : NULL;
        int notes = note[0] ? 1 : 0;
        /* Keep the split stable while moving among this page's fields. */
        smith_sheet_fit_list(&sheet, count, cols, notes + (page != SMITH_ARTIFICE));

        smith_sheet_begin(&sheet, false);
        smith_editor_tabs(&sheet, page, true);
        int first = smith_sheet_list_first(&sheet);
        int list_end = sheet.list_end;
        if (notes && list_end - first >= 4) list_end--;
        if (page != SMITH_ARTIFICE && list_end - first > (count + cols - 1) / cols)
            list_end--;
        smith_picker_grid grid = smith_sheet_grid(&sheet, count, cols, first, list_end);
        int current = focus < count ? focus : smith_ui_scroll_top[SMITH_SCROLL_EDITOR] * cols;
        smith_sheet_open_list(&sheet, &grid, SMITH_SCROLL_EDITOR, count, current);
        static const cptr headings[] = { "Item attributes", "Affix slots", "Artefact design" };
        cptr heading = page == SMITH_ENHANCEMENTS && count > 2 ? ui_text("Affixes and bonuses") : ui_text(headings[page]);
        smith_sheet_heading(&sheet, heading, &grid, count);
        for (int i = grid.top * cols; i < count && i < (grid.top + grid.visible) * cols; i++)
        {
            cptr label = cards[i].indented ? format("  %s", cards[i].label) : cards[i].label;
            smith_sheet_choice(&sheet, &grid, i, i + 1, i, label,
                cards[i].value, NULL, i == focus, cards[i].enabled ? TERM_WHITE : TERM_L_DARK);
        }

        int used = MIN(grid.visible, (count + cols - 1) / cols) * grid.stride;
        smith_editor_context(&sheet, first + used, card, note);

        int difficulty = smith_measure_design();
        smith_name_mode name_mode = SMITH_NAME_READ_ONLY;
        if (can_rename)
            name_mode = focus == name_focus ? SMITH_NAME_FOCUSED : SMITH_NAME_EDITABLE;
        smith_sheet_order(&sheet, smith_o_ptr, difficulty, p_ptr->smithing_leftover > 0,
            false, name_mode);
        bool keys = sheet.width >= 68 && !sdl_touch_only_device_active() && !steamdeck_controls_active();
        cptr accept = p_ptr->smithing_leftover ? ui_text("Resume") : ui_text("Craft");
        const smith_sheet_action actions[] = {
            { SMITH_EDITOR_INSPECT, keys ? ui_text("[I] Inspect") : ui_text("Inspect"), TERM_WHITE },
            { SMITH_EDITOR_COSTS, keys ? ui_text("[?] Costs") : ui_text("Costs"), TERM_WHITE },
            { SMITH_EDITOR_CRAFT, keys ? format("[F] %s", accept) : accept,
                craftable ? TERM_L_GREEN : TERM_SLATE },
        };
        smith_sheet_actions(&sheet, actions, N_ELEMENTS(actions),
            focus >= first_action ? actions[focus - first_action].id : 0);
        cptr confirm = focus == name_focus ? ui_text("Rename")
            : focus == first_action ? ui_text("Inspect")
            : focus == first_action + 1 ? ui_text("Costs")
            : focus == last ? accept
            : card && card->enabled ? (card->number ? ui_text("Edit") : ui_text("Choose")) : NULL;
        smith_sheet_navigation(&sheet, false, confirm, true);
        Term_fresh();
        int key = smith_sheet_read_key('[', ']');

        int choice, click;
        bool activate = false;
        if (smith_ui_take_click_action(&choice, &click))
        {
            key = 0;
            if (choice == SMITH_CLICK_BACK || click == UI_MENU_CLICK_SECONDARY)
                key = click == UI_MENU_CLICK_HOVER ? 0 : ESCAPE;
            else if (choice >= SMITH_EDITOR_TAB && choice < SMITH_EDITOR_TAB + SMITH_PAGE_COUNT)
            {
                if (click != UI_MENU_CLICK_HOVER) next_page = choice - SMITH_EDITOR_TAB;
            }
            else if (choice >= SMITH_EDITOR_INSPECT && choice <= SMITH_EDITOR_CRAFT)
            {
                focus = first_action + choice - SMITH_EDITOR_INSPECT;
                activate = click != UI_MENU_CLICK_HOVER;
            }
            else if (choice == SMITH_EDITOR_RENAME && can_rename)
            {
                focus = name_focus;
                activate = click != UI_MENU_CLICK_HOVER;
            }
            else if (choice >= 1 && choice <= count)
            {
                focus = choice - 1;
                activate = click != UI_MENU_CLICK_HOVER;
            }
        }
        if (key == ESCAPE) break;
        if (key == '[') next_page = (page + SMITH_PAGE_COUNT - 1) % SMITH_PAGE_COUNT;
        if (key == ']' || key == '\t') next_page = (page + 1) % SMITH_PAGE_COUNT;
        if (next_page >= 0) continue;
        if (key == '4' || key == '6')
            focus = (focus + (key == '4' ? last : 1)) % (last + 1);
        if (key == '2')
        {
            if (focus >= first_action)
                focus = MIN(count - 1, (focus - first_action) % cols);
            else if (focus == name_focus)
                focus = first_action;
            else if (focus + cols < count)
                focus += cols;
            else
                focus = can_rename ? name_focus : first_action + MIN(2, focus % cols);
        }
        if (key == '8')
        {
            if (focus >= first_action)
                focus = can_rename ? name_focus : MIN(count - 1,
                    ((count - 1) / cols) * cols + MIN(cols - 1, focus - first_action));
            else if (focus == name_focus)
                focus = count - 1;
            else
                focus = focus >= cols ? focus - cols : first_action + MIN(2, focus);
        }
        if (sdl_menu_letters_enabled() && key >= 'a' && key <= 'z' && key - 'a' < count)
        {
            focus = key - 'a';
            activate = true;
        }
        if (key == 'I' || key == 'F' || key == '?'
            || (steamdeck_controls_active() && key == steamdeck_info_key()))
        {
            focus = key == 'F' ? last : key == '?' ? first_action + 1 : first_action;
            activate = true;
        }
        if (!activate && key != '\r' && key != '\n' && key != ' ') continue;
        bool edit_number = focus < count && cards[focus].number && cards[focus].enabled;
        if (!edit_number) sdl_smithing_sheet_clear();
        if (focus == first_action)
            describe_object_with_comparisons(smith_o_ptr, true);
        else if (focus == first_action + 1)
            smith_show_cost_details();
        else if (focus == last)
        {
            if (craftable)
            {
                craft = true;
                break;
            }
            smith_root_note_for_choice(SMT_MENU_ACCEPT, false, note, sizeof(note));
            bell(death_view ? ui_text("You cannot craft during this final look.") : note);
        }
        else if (focus < count && !cards[focus].enabled)
        {
            cptr hint = smith_fixed_number_note(cards[focus].number);
            bell(hint ? hint : note[0] ? note : ui_text("This choice is unavailable for this item."));
        }
        else
        {
            bool changed;
            if (focus == name_focus)
                changed = smith_edit_artifice(SMITH_EDITOR_RENAME);
            else if (edit_number)
            {
                changed = smith_number_card_menu(cards[focus].number, page, &next_page);
                if (changed)
                {
                    *smith2_o_ptr = *smith_o_ptr;
                    smith2_alloy = smith_alloy;
                }
            }
            else if (page == SMITH_ENHANCEMENTS)
                changed = smith_choose_affix(cards[focus].prefix);
            else
                changed = smith_edit_artifice(focus);
            if (changed && !death_view) p_ptr->smithing_leftover = 0;
        }
    }
    sdl_smithing_sheet_clear();
    ui_menu_click_clear();
    ui_scroll_area_clear();
    screen_load();
    return craft;
}

/*
 * Performs the interface and selection work for the smithing screen.
 */
int smithing_menu_aux(int* highlight)
{
    bool valid[SMT_MENU_MAX];
    byte menu_attr[SMT_MENU_MAX];
    char labels[SMT_MENU_MAX][32];

    smith_sheet_layout sheet = smith_sheet_geometry();
    smith_root_build_entries(valid, menu_attr, labels);
    int actions[5];
    int count = smith_work_actions(actions);
    int index = 0;
    for (int i = 0; i < count; i++)
        if (actions[i] == *highlight) index = i;
    *highlight = actions[index];
    smith_sheet_fit_list(&sheet, count, 1, 1);
    smith_sheet_begin(&sheet, true);
    smith_ui_put_fitted(sheet.margin, sheet.tabs, sheet.list_width, TERM_L_BLUE,
        smith_pending_reforge() ? ui_text("Reforging") : ui_text("Smithing in progress"));
    smith_picker_grid grid = smith_sheet_grid(&sheet, count, 1,
        smith_sheet_list_first(&sheet), sheet.list_end);
    smith_sheet_open_list(&sheet, &grid, SMITH_SCROLL_EDITOR, count, index);
    smith_sheet_heading(&sheet, ui_text("Work in progress"), &grid, count);
    for (int i = grid.top; i < count && i < grid.top + grid.visible; i++)
    {
        int id = actions[i];
        smith_sheet_choice(&sheet, &grid, i, id, id - 1, labels[id - 1],
            "", NULL, id == *highlight, menu_attr[id - 1]);
    }
    int detail = MAX(grid.first + count * grid.stride + 1,
        sheet.body + (sheet.list_end - sheet.body) / 2);
    char note[120];
    smith_root_note_for_choice(*highlight, valid[*highlight - 1], note, sizeof(note));
    if (detail < sheet.list_end && *note)
        detail += smith_ui_put_wrapped(sheet.margin, detail, sheet.list_width,
            sheet.list_end - detail, TERM_L_WHITE, note) + 1;
    smith_sheet_stock(sheet.margin, detail, sheet.list_width, sheet.list_end);
    if (smith_o_ptr->k_idx)
    {
        int difficulty = smith_measure_design();
        smith_sheet_order(&sheet, smith_o_ptr, difficulty, true, false,
            SMITH_NAME_READ_ONLY);
    }
    const smith_sheet_action footer[] = {
        { SMITH_CLICK_BACK, ui_text("Exit"), TERM_WHITE }, { SMITH_CLICK_CALC, ui_text("Costs"), TERM_WHITE }
    };
    smith_sheet_actions(&sheet, footer, N_ELEMENTS(footer), 0);
    smith_sheet_navigation(&sheet, true, ui_text("Select"), false);
    Term_fresh();

    int ch = smith_sheet_read_key(0, 0);

    int clicked, how;
    if (smith_ui_take_click_action(&clicked, &how))
    {
        ch = 0;
        if (clicked == SMITH_CLICK_BACK || how == UI_MENU_CLICK_SECONDARY)
            return how == UI_MENU_CLICK_HOVER ? 0 : -1;
        for (int i = 0; i < count; i++)
        {
            if (actions[i] != clicked) continue;
            if (!smith_ui_pointer_choice_confirms(clicked, how, highlight)) return 0;
            ch = '\r';
        }
    }
    if (sdl_menu_letters_enabled() && ch >= 'a' && ch <= 'g')
    {
        int id = ch - 'a' + 1;
        for (int i = 0; i < count; i++)
            if (actions[i] == id)
            {
                *highlight = id;
                ch = '\r';
                break;
            }
    }
    if (ch == '?' && smith_o_ptr->k_idx) smith_show_cost_details();
    if (ch == ESCAPE || ch == '4') return -1;
    if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '6')
    {
        if (valid[*highlight - 1]) return *highlight;
        bell(ui_text("Invalid choice."));
        return 0;
    }
    if (ch == '8') *highlight = actions[(index + count - 1) % count];
    if (ch == '2') *highlight = actions[(index + 1) % count];
    return 0;
}

/*
 * Brings up a screen for making new items (only works at a forge).
 * Leads to many submenus which help to determine the item's attributes.
 */
void do_cmd_smithing_screen(void)
{
    tutorial_game_menu("smithing", ui_text("Preview an item and read its difficulty and full costs before accepting. Creating, resuming and reforging keep their ordinary forge, resource and time costs."));
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool create = false;
    bool death_view = death_spectator_active();
    int old_smithing = p_ptr->smithing;
    int old_smithing_leftover = p_ptr->smithing_leftover;

    // if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    //{
    //	msg_print("You can only create items at a forge.");
    //	return;
    //}

    if (cave_forge_bold(p_ptr->py, p_ptr->px)
        && forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        msg_print("The resources of this forge are exhausted.");
        msg_print(
            "You will be able to browse the options but not make new things.");
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale_for(SDL_MENU_FONT_SMITHING);
    smith_description.text[0] = '\0';

    /* Clear screen */
    Term_clear();

    // Hack: flag that we are in the middle of smithing
    p_ptr->smithing = 1;

    // deal with previous interruptions
    if (p_ptr->smithing_leftover > 0)
    {
        // default to 'resume' if an item is already in progress
        highlight = SMT_MENU_ACCEPT;

        // and backup the smithing item
        object_copy(smith2_o_ptr, smith_o_ptr);
        smith2_alloy = smith_alloy;
        /* A resumed enchanted object already includes its affix bonuses. */
        enchant_then_numbers = object_has_ego(smith_o_ptr);
        if (smith_o_ptr->name1 == smith_a_name)
            *smith2_a_ptr = *smith_a_ptr;
    }

    // otherwise wipe the smithing item
    else
    {
        object_wipe(smith_o_ptr);
        smith_clear_alloy_state(&smith_alloy);
    }

    /* New projects start at the item grid. Interrupted work retains a direct
     * Resume action, including prepaid reforging at an exhausted forge. */
    enum forge_view { FORGE_ITEMS, FORGE_REPLACE, FORGE_WORK, FORGE_EDITOR };
    enum forge_view view = p_ptr->smithing_leftover ? FORGE_WORK : FORGE_ITEMS;
    enum forge_view editor_back = view;
    int group = smith_o_ptr->k_idx ? smith_item_category(smith_o_ptr) : CAT_WEAPON;
    int item_highlight = 0;
    int design_kind = smith_o_ptr->k_idx;
    while (!leave_menu)
    {
        if (view == FORGE_EDITOR)
        {
            if (!smith_editor(SMITH_SIMPLE, death_view))
            {
                view = p_ptr->smithing_leftover ? editor_back : FORGE_ITEMS;
                continue;
            }
            actiontype = SMT_MENU_ACCEPT;
        }
        else if (view == FORGE_WORK)
        {
            actiontype = smithing_menu_aux(&highlight);
            if (actiontype == SMT_MENU_NUMBERS)
            {
                editor_back = FORGE_WORK;
                view = FORGE_EDITOR;
                continue;
            }
            if (actiontype == SMT_MENU_CREATE)
            {
                group = smith_item_category(smith_o_ptr);
                item_highlight = 0;
                view = FORGE_REPLACE;
                continue;
            }
        }
        else
        {
            int kind = design_kind;
            actiontype = smith_item_browser(&group, &item_highlight,
                view == FORGE_REPLACE, &kind);
            if (actiontype == SMT_MENU_CREATE)
            {
                /* Keep edits when returning to the same base, even after
                 * Artifice has converted jewellery to its custom kind. */
                if (kind != design_kind || !smith_o_ptr->k_idx)
                {
                    create_base_object(k_info[kind].tval, k_info[kind].sval);
                    if (!death_view) p_ptr->smithing_leftover = 0;
                    enchant_then_numbers = false;
                    *smith2_o_ptr = *smith_o_ptr;
                    smith2_alloy = smith_alloy;
                    design_kind = kind;
                }
                editor_back = view;
                view = FORGE_EDITOR;
                continue;
            }
            if (actiontype == -1 && view == FORGE_REPLACE)
            {
                view = p_ptr->smithing_leftover ? FORGE_WORK : FORGE_ITEMS;
                continue;
            }
        }

        if (actiontype) sdl_smithing_sheet_clear();
        if (smith_pending_reforge() && actiontype > 0 && actiontype != SMT_MENU_ACCEPT)
        {
            msg_print("Finish your current reforging work first.");
            continue;
        }

        switch (actiontype)
        {
        case SMT_MENU_MELT:
        {
            if (death_view)
            {
                msg_print("You cannot do that during this final look.");
                break;
            }

            if (meltable_metal_items_carried())
            {
                melt_menu();
                if (!smith_o_ptr->k_idx)
                {
                    design_kind = 0;
                    view = FORGE_ITEMS;
                }
            }
            else
            {
                bell(ui_text("You don't have any mithril or star-iron items."));
            }

            break;
        }
        case SMT_MENU_REPAIR:
        {
            if (death_view)
            {
                msg_print("You cannot do that during this final look.");
                break;
            }

            if (smith_reforge_item() && smith_pending_reforge())
            {
                create = true;
                leave_menu = true;
            }
            break;
        }
        case SMT_MENU_ACCEPT:
        {
            if (!tutorial_game_action_allowed("smith", NULL)) break;
            if (death_view)
            {
                msg_print("You cannot do that during this final look.");
                break;
            }

            if (!smith_pending_reforge() && smithing_cost.drain > 0)
            {
                char buf[80];

                sprintf(buf,
                    ui_text("This will drain your smithing skill by %d points. "
                    "Proceed? "),
                    smithing_cost.drain);
                if (!get_check(buf))
                    break;
            }

            create = true;
            leave_menu = true;
            break;
        }
        case -1:
        {
            leave_menu = true;
            break;
        }
        }
    }

    if (create)
    {
        int turn_multiplier = 10;

        if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
        {
            turn_multiplier /= 2;
        }

        // Display a message
        msg_print("You begin your work.");

        // add the details to the artefact type if applicable
        if (smith_o_ptr->name1)
            add_artefact_details();

        /* Cancel stealth mode */
        if (p_ptr->stealth_mode && pixel_monster_status_icons)
            p_ptr->redraw |= (PR_MAP);
        p_ptr->stealth_mode = false;

        // Allow the resumption of interrupted smithing
        if (p_ptr->smithing_leftover > 0)
        {
            p_ptr->smithing = p_ptr->smithing_leftover;
        }
        else
        {
            // Set smithing counter
            p_ptr->smithing
                = MAX(10, object_difficulty(smith_o_ptr) * turn_multiplier);

            // Also set the smithing leftover counter (to allow you to resume if
            // interrupted)
            p_ptr->smithing_leftover = p_ptr->smithing;
        }

        /* Restoring gameplay zoom after this saved-screen menu can recenter the
         * panel.  Applying the forge's light also refreshes the view.  Neither
         * UI transition should cancel the action that was just accepted. */
        p_ptr->smithing_starting = true;

        /* The work has been accepted and the player is now actually forging;
         * previews, browsing, and cancelled menus stay silent. */
        if (forge_uses(p_ptr->py, p_ptr->px) > 0)
            sound(MSG_FORGE);

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS | PU_TORCH);

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Handle stuff */
        handle_stuff();

        /* Refresh */
        Term_fresh();
    }

    else if (death_view)
    {
        p_ptr->smithing = old_smithing;
        p_ptr->smithing_leftover = old_smithing_leftover;
    }
    else
    {
        if (p_ptr->smithing_leftover == 0)
        {
            /* Wipe the smithing object */
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);
        }

        // Hack: flag that we are done with smithing
        p_ptr->smithing = 0;
    }

    /* Load screen */
    sdl_smithing_sheet_clear();
    ui_menu_click_clear();
    ui_scroll_area_clear();
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    screen_load();

    /* Process the restored panel and initial forge-light view while the startup
     * guard is still active, even when no supporting pane requested a refresh. */
    if (p_ptr->smithing_starting)
        handle_stuff();

    /* From this point onward, input, monsters, damage, and other disturbances
     * interrupt smithing normally. */
    p_ptr->smithing_starting = false;
}

static void finish_smith_reforge(void)
{
    char name[160];
    object_aware(smith_o_ptr);
    object_known(smith_o_ptr);
    object_desc(name, sizeof(name), smith_o_ptr, true, 3);

    /* Delta costs were paid when this exact item entered the work. */
    int slot = inven_carry(smith_o_ptr, true);
    bool delivered = true;
    if (player_inventory_handle_valid(slot))
        msg_format("You have reforged %s (%c).", name, player_inventory_label(slot));
    else
    {
        delivered = drop_near(smith_o_ptr, 0, p_ptr->py, p_ptr->px) > 0;
        msg_format("You have reforged %s, but it falls to the floor.", name);
    }
    if (delivered)
        sil_achievements_on_smithing_completed(false);
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);
    p_ptr->update |= PU_BONUS;
    p_ptr->window |= PW_INVEN | PW_EQUIP;
}

/*
 * Actually creates the item.
 */
void create_smithing_item(void)
{
    if (smith_o_ptr->k_idx && smith_o_ptr->unused1 == 2)
    {
        finish_smith_reforge();
        return;
    }
    int slot;
    object_type* o_ptr;
    char o_name[80];
    bool custom_artefact = smith_o_ptr->name1 != 0;
    bool delivered = true;
    int achievement_forge_before = forge_uses(p_ptr->py, p_ptr->px);
    int achievement_forge_paid = smithing_cost.uses;
    int achievement_mithril_paid = smithing_cost.mithril;
    /* Difficulty calculation refreshes the preview bill. Preserve the project's
     * agreed costs while measuring the completed custom artefact. */
    smithing_cost_type achievement_cost = smithing_cost;
    int achievement_difficulty = custom_artefact ? object_difficulty(smith_o_ptr) : 0;
    smithing_cost = achievement_cost;
    object_type achievement_created;

    log_debug("Creating smithing item");

    // pay the ability/experience costs of smithing
    pay_costs();

    // if making an artefact, copy its attributes into the proper place in the
    // a_info array
    if (smith_o_ptr->name1)
    {
        log_info("Creating new artifact");
        smith_o_ptr->name1 = z_info->art_rand_max + p_ptr->self_made_arts;

        artefact_copy(&a_info[smith_o_ptr->name1], smith_a_ptr);
        artefact_type* created = &a_info[smith_o_ptr->name1];
        if (score_guid_is_zero(&created->guid)) {
            created->guid = score_guid_random();
        }
        (void)score_artefact_register(created);
        p_ptr->self_made_arts++;

        // make sure to display it as cursed if it is so
        if (smith_a_ptr->flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        {
            smith_o_ptr->ident |= (IDENT_CURSED);
            log_debug("Artifact marked as cursed");
        }

        // Store the depth at which it was created
        smith_o_ptr->xtra1 = p_ptr->depth;

        log_debug("Artifact #%d created at depth %d", p_ptr->self_made_arts, p_ptr->depth);
    }

        /* ------------------------------------------------------ */
        /* New escape-curse: smithing can back-fire               */
        /* ------------------------------------------------------ */
        {
            int stacks = curse_flag_count_cur(CUR_SMITHCURSE);          /* 0-3 */
            if (stacks &&            /* must have the curse          */
                !(smith_o_ptr->ident & IDENT_CURSED) &&             /* not already */
                (smith_o_ptr->tval != TV_LIGHT))                    /* skip torches */
            {
                if (rand_int(100) < 10 * stacks)                    /* 10 % / stack */
                {
                    log_debug("Smithing curse triggered - adding random curse");
                    add_random_curse(smith_o_ptr);
                }
            }
        }


    // remove the spoiler ident flag
    smith_o_ptr->ident &= ~(IDENT_SPOIL);

    // identify the object
    ident(smith_o_ptr);

    // create description
    object_desc(o_name, sizeof(o_name), smith_o_ptr, true, 3);

    // Record the depth where the object was created
    do_cmd_note(format(ui_text("Made %s  %d.%d lb"), o_name,
                    (smith_o_ptr->weight * smith_o_ptr->number) / 10,
                    (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        p_ptr->depth);

    /* Stamp the new object before inventory/drop copies preserve it. */
    sil_achievement_character_on_forged(smith_o_ptr);
    object_copy(&achievement_created, smith_o_ptr);

    // Get the slot of the forged item
    slot = inven_carry(smith_o_ptr, true);

    /* Supply items are absorbed into the separate supplies store.  inven_carry
     * wipes smith_o_ptr after that transfer, so the returned value is not a
     * real inventory slot and must not be used to index inventory[]. */
    if (slot == SUPPLIES_INDEX)
    {
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("You add %s to your supplies (%c).", o_name, label);
    }

    // Check if the item couldn't fit in inventory (e.g., group limit)
    else if (slot < 0)
    {
        // Drop it on the floor instead
        log_debug("Smithed item couldn't fit in inventory, dropping to floor");
        delivered = drop_near(smith_o_ptr, 0, p_ptr->py, p_ptr->px) > 0;

        // Describe the object
        object_desc(o_name, sizeof(o_name), smith_o_ptr, true, 3);

        // Message
        msg_format("You have forged %s, but it falls to the floor.", o_name);
        log_info("Created smithing item (dropped): %s", o_name);
    }
    else
    {
        // Get the item itself
        o_ptr = player_inventory_object(slot);
        if (!o_ptr || !o_ptr->k_idx)
        {
            log_warn("Smithing returned an invalid inventory handle: %d", slot);
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);
            return;
        }

        // Mark the item as smithed by the player (using unused1 field)
        o_ptr->unused1 = 1;  /* 1 = smithed by player, 0 = found item */

        // Describe the object
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        // Message
        msg_format("You have %s (%c).", o_name, player_inventory_label(slot));
        log_info("Created smithing item: %s", o_name);
    }

    /* Costs, artefact registration and delivery have all completed. */
    if (delivered)
    {
        sil_achievements_on_smithing_completed(custom_artefact);
        if (custom_artefact)
            sil_achievements_on_artefact_created(achievement_difficulty);
        sil_achievement_character_on_smithing_completed(&achievement_created,
            custom_artefact, achievement_forge_before,
            achievement_forge_paid, achievement_mithril_paid);
    }

    // Wipe the smithing object
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);
}
