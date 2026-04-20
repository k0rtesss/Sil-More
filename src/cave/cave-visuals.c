/* File: cave-visuals.c */
/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

#include "angband.h"

#include "app/app-session.h"
#include "cave/cave-internal.h"
#include "log/log.h"
#include "ui/colors.h"

/* Encoded color range that indicates an absolute style index per cell.
 * We now store the chosen style for each cell directly in cave_color as
 * COLOR_STYLE_BASE + style_index. This guarantees deterministic visuals
 * and removes the need for group indirection.
 *
 * Additionally, we reserve an offset of 64 to indicate a "first-variant"
 * override. That is, values in the range COLOR_STYLE_BASE+64..+127 encode
 * the same style indices 0..63, but signal that when a style offers multiple
 * floor/door variants, variant index 0 should be used regardless of the
 * per-level/per-vault random choice. This is used by the vault halo so that
 * adjacent tiles reliably use the vault's first variant.
 */
#ifndef COLOR_STYLE_BASE
#define COLOR_STYLE_BASE 128 /* 128..(128+style_max-1) map to style_info indices */
#endif
/* Max supported styles in encoded color and the special first-variant flag */
#ifndef COLOR_STYLE_SLOT_MAX
#define COLOR_STYLE_SLOT_MAX 64
#endif
#ifndef COLOR_STYLE_FLAG_FIRSTVAR
#define COLOR_STYLE_FLAG_FIRSTVAR COLOR_STYLE_SLOT_MAX
#endif
/*
 * Weighted style selection
 * ------------------------
 * For each level (and while building a vault), we maintain a weighted list
 * of style indices. When a grid feature is set without an explicit style,
 * we pick one style randomly according to weights and encode it in
 * cave_color[y][x] as COLOR_STYLE_BASE + style_index.
 */
typedef struct {
    int count;            /* number of entries */
    int total_weight;     /* cached sum of weights */
    int sidx[64];         /* style indices */
    int weight[64];       /* weights for each index */
} style_weight_list;

/* For each style, pick one floor/door variant per level/vault */
static byte g_level_floor_choice[64];  /* index into floor_rowv/colv, 0..count-1 */
static byte g_level_door_choice[64];
static byte g_vault_floor_choice[64];
static byte g_vault_door_choice[64];

/* Active weighted style lists and selections */
static style_weight_list g_level_styles;
static style_weight_list g_vault_styles;
static style_weight_list* g_active_styles = &g_level_styles;
static int g_level_primary_style = -1;
static int g_vault_primary_style = -1;
static int g_vault_avoid_style = -1;

/* Level rules table (indexed by exact depth 0..31) */
static style_weight_list g_level_rule[32];
/* Per-depth partition style rules (by kind, indexed by exact depth 0..31) */
static style_weight_list g_partition_rule[PART_STYLE_MAX][32];

/* Helpers to mutate weight lists */
static void styles_clear(style_weight_list* L)
{
    if (!L) return;
    L->count = 0; L->total_weight = 0;
}

static void styles_add(style_weight_list* L, int sidx, int weight)
{
    if (!L || !z_info || !style_info) return;
    if (sidx < 0 || sidx >= z_info->style_max) return;
    if (!style_info[sidx].name) return;
    if (L->count >= 64) return;
    if (weight <= 0) return;
    L->sidx[L->count] = sidx;
    L->weight[L->count] = weight;
    L->count++;
    L->total_weight += weight;
}

/* Debug helper */
static void styles_log_list(const char* tag, const style_weight_list* L)
{
    if (!L) return;
    log_debug("%s: count=%d total=%d", tag ? tag : "styles", L->count, L->total_weight);
}

/* Ensure depth-based wall styling is enabled unless explicitly disabled elsewhere */
#ifndef DEPTH_BASED_WALLS
#define DEPTH_BASED_WALLS 1
#endif

/* Forward declarations for rule APIs defined later in this file */
void styles_rules_clear(void);
void styles_default_vault_clear(void);
void styles_vault_rules_clear(void);
void styles_add_level_rule(int depth, int unused, const int* sidx, const int* weight, int count);
void styles_set_vault_rule(int depth, const int* sidx, const int* weight, int count);
void styles_default_vault_add(int sidx_or_star, int weight);
void styles_partition_rules_clear(void);
void styles_add_partition_rule(int depth, int kind, const int* sidx, const int* weight, int count);
int styles_pick_partition_style(int depth, int kind);

/* Backward-compatibility: reset any cached depth/style state between levels */
void reset_depth_color_cache(void)
{
    styles_clear(&g_level_styles);
    styles_clear(&g_vault_styles);
    g_active_styles = &g_level_styles;
    g_level_primary_style = -1;
    g_vault_primary_style = -1;
    g_vault_avoid_style = -1;
    log_debug("reset_depth_color_cache: cleared style lists and selections");
}

/* Initialize the level style weights: use matching rule, else default to all */
void styles_init_for_level(void)
{
    /* Depth 0 is the Gates of Angband: force style 13 for the whole level */
    if (p_ptr && p_ptr->depth == 0) {
        styles_clear(&g_level_styles);
        styles_add(&g_level_styles, 13, 1);
        g_active_styles = &g_level_styles;
        g_level_primary_style = 13;
        /* Reset per-style variant picks */
        for (int i = 0; i < 64; ++i) { g_level_floor_choice[i] = 0; g_level_door_choice[i] = 0; }
        /* No need to randomize variants; keep first variant for cohesion */
        log_debug("styles_init_for_level: depth=0 forced style 13 as primary");
        styles_log_list("styles_init_for_level list", &g_level_styles);
        return;
    }
    /* Normal depths: initialize per rules or fallback */
    styles_clear(&g_level_styles);
    /* Reset per-style variant picks */
    for (int i = 0; i < 64; ++i) { g_level_floor_choice[i] = 0; g_level_door_choice[i] = 0; }
    bool applied = false;
    /* Apply rule matching exact depth (0..31); depth 0 is special (Gates) */
    if (p_ptr->depth >= 0 && p_ptr->depth < 32) {
        style_weight_list* L = &g_level_rule[p_ptr->depth];
        for (int i = 0; i < L->count; ++i) styles_add(&g_level_styles, L->sidx[i], L->weight[i]);
        applied = (g_level_styles.count > 0);
    }
    /* Fallback: all styles */
    if (!applied && z_info && style_info) {
        for (int i = 0; i < z_info->style_max; i++) {
            if (style_info[i].name) styles_add(&g_level_styles, i, 1);
        }
    }
    g_active_styles = &g_level_styles;
    /* Choose one exact primary style for this level (used by vault '*') */
    if (g_level_styles.count > 0) {
        int total = g_level_styles.total_weight;
        int r = rand_int(total);
        int pick = g_level_styles.sidx[0];
        for (int i = 0; i < g_level_styles.count; ++i) {
            if (r < g_level_styles.weight[i]) { pick = g_level_styles.sidx[i]; break; }
            r -= g_level_styles.weight[i];
        }
        g_level_primary_style = pick;
    } else {
        g_level_primary_style = -1;
    }
    /* For all styles, pick a variant index (if multiple) once per level */
    if (z_info && style_info) {
        for (int i = 0; i < z_info->style_max && i < 64; ++i) {
            if (!style_info[i].name) continue;
            byte fc = style_info[i].floor_count;
            byte dc = style_info[i].door_count;
            if (fc > 1) g_level_floor_choice[i] = (byte)rand_int(fc);
            if (dc > 1) g_level_door_choice[i] = (byte)rand_int(dc);
        }
    }

    log_debug("styles_init_for_level: depth=%d initialized %d styles (total_weight=%d) primary=%d",
        p_ptr->depth, g_level_styles.count, g_level_styles.total_weight, g_level_primary_style);
    styles_log_list("styles_init_for_level list", &g_level_styles);
}

/* Begin vault: by default prefer the level's chosen style with weight 5 and
 * optionally add/boost one extra style with a given weight (e.g., 2).
 */
void styles_begin_vault(int extra_sidx, int extra_weight)
{
    styles_clear(&g_vault_styles);
    g_vault_primary_style = -1;
    /* Reset and start with level picks, then override randomly per vault */
    for (int i = 0; i < 64; ++i) {
        /* Floors inside vaults use the first variant for visual cohesion */
        g_vault_floor_choice[i] = 0;
        /* Keep door variant selection from level unless overridden */
        g_vault_door_choice[i] = g_level_door_choice[i];
    }
    /* Default: start empty; callers may clone level list via API */
    /* Optionally add one more style */
    if (extra_sidx >= 0 && extra_weight > 0) styles_add(&g_vault_styles, extra_sidx, extra_weight);
    g_active_styles = &g_vault_styles;
    log_debug("styles_begin_vault: active styles=%d (extra=%d, w=%d)",
        g_vault_styles.count, extra_sidx, extra_weight);
    styles_log_list("styles_begin_vault list", &g_vault_styles);
}

/* End vault: restore level styles */
void styles_end_vault(void)
{
    g_active_styles = &g_level_styles;
    g_vault_primary_style = -1;
    g_vault_avoid_style = -1;
}

/* Helpers to get current variant choice for a style index (vault if active) */
static inline byte style_floor_choice(int sidx) {
    return (g_vault_primary_style >= 0) ? g_vault_floor_choice[sidx & 63] : g_level_floor_choice[sidx & 63];
}
static inline byte style_door_choice(int sidx) {
    return (g_vault_primary_style >= 0) ? g_vault_door_choice[sidx & 63] : g_level_door_choice[sidx & 63];
}

/* Pick a style index using the active weighted list */
/* styles_pick_random was removed; we always pick primary styles at scope start */

/* External APIs to explicitly control level/vault weight lists */
void styles_reset_level_weights(void) { styles_clear(&g_level_styles); g_active_styles = &g_level_styles; }
void styles_add_level_weight(int sidx, int weight) { styles_add(&g_level_styles, sidx, weight); }
void styles_reset_vault_weights(void) { styles_clear(&g_vault_styles); }
void styles_add_vault_weight(int sidx, int weight) { styles_add(&g_vault_styles, sidx, weight); }
void styles_add_vault_from_level(int factor)
{
    if (factor <= 0) factor = 1;
    for (int i = 0; i < g_level_styles.count; ++i) {
        styles_add(&g_vault_styles, g_level_styles.sidx[i], g_level_styles.weight[i] * factor);
    }
}

/* Default vault style list: can include '*' which means "clone level list".
 * We represent '*' as sidx == -1 and apply it when starting a vault. */
static int g_vault_default_count = 0;
static int g_vault_default_sidx[64];
static int g_vault_default_weight[64];

void styles_default_vault_clear(void) { g_vault_default_count = 0; }
void styles_default_vault_add(int sidx_or_star, int weight)
{
    if (g_vault_default_count >= 64) return;
    g_vault_default_sidx[g_vault_default_count] = sidx_or_star; /* -1 means '*' */
    g_vault_default_weight[g_vault_default_count] = weight > 0 ? weight : 0;
    g_vault_default_count++;
}

/* Apply a vault style array into the active vault list; tokens may include '*' */
void styles_apply_vault_list(const int* sidx, const int* weight, int count)
{
    styles_reset_vault_weights();
    for (int i = 0; i < count; ++i) {
        if (sidx[i] == -1) {
            if (g_level_primary_style >= 0) styles_add_vault_weight(g_level_primary_style, weight[i]);
        }
        else styles_add_vault_weight(sidx[i], weight[i]);
    }
    styles_log_list("styles_apply_vault_list", &g_vault_styles);
}

/* Per-depth default vault lists (1..20), tokens may include '*' (-1) */
typedef struct { int count; int sidx[64]; int weight[64]; } vault_rule_list;
static vault_rule_list g_vault_rule[32];

void styles_vault_rules_clear(void) { for (int i = 0; i < 32; ++i) g_vault_rule[i].count = 0; }
void styles_set_vault_rule(int depth, const int* sidx, const int* weight, int count)
{
    if (depth < 1 || depth >= 32) return;
    vault_rule_list* R = &g_vault_rule[depth];
    R->count = 0;
    for (int i = 0; i < count && i < 64; ++i) { R->sidx[i] = sidx[i]; R->weight[i] = weight[i]; R->count++; }
}
void styles_apply_vault_default_for_depth(int depth)
{
    if (depth >= 1 && depth < 32 && g_vault_rule[depth].count > 0) {
        log_debug("styles_apply_vault_default_for_depth: using per-depth defaults for depth=%d", depth);
        styles_apply_vault_list(g_vault_rule[depth].sidx, g_vault_rule[depth].weight, g_vault_rule[depth].count);
    } else if (g_vault_default_count > 0) {
        log_debug("styles_apply_vault_default_for_depth: using global defaults for depth=%d", depth);
        styles_apply_vault_list(g_vault_default_sidx, g_vault_default_weight, g_vault_default_count);
    } else {
    /* Fallback: 100% same as the exact chosen level style */
    log_debug("styles_apply_vault_default_for_depth: using fallback to level primary for depth=%d", depth);
    if (g_level_primary_style >= 0) styles_add_vault_weight(g_level_primary_style, 10);
    }
}

int styles_get_level_primary_style(void) { return g_level_primary_style; }
void styles_set_loaded_level_primary(int sidx) { g_level_primary_style = sidx; }

/* Clear in-memory level rules table */
void styles_rules_clear(void)
{
    for (int i = 0; i < 32; ++i) {
        g_level_rule[i].count = 0;
        g_level_rule[i].total_weight = 0;
    }
}

/* Add a rule for exact depth (0..31) with arrays of indices/weights */
void styles_add_level_rule(int depth, int unused, const int* sidx, const int* weight, int count)
{
    (void)unused;
    if (depth < 0 || depth >= 32) return;
    style_weight_list* L = &g_level_rule[depth];
    L->count = 0;
    L->total_weight = 0;
    if (!sidx || !weight || count <= 0) return;
    for (int i = 0; i < count && i < 64; ++i) {
        int si = sidx[i];
        int wt = weight[i];
        /* Defer validity checks that need style_info until use; store raw */
        L->sidx[L->count] = si;
        L->weight[L->count] = wt;
        L->count++;
        if (wt > 0) L->total_weight += wt;
    }
}

void styles_partition_rules_clear(void)
{
    for (int k = 0; k < PART_STYLE_MAX; ++k) {
        for (int d = 0; d < 32; ++d) {
            g_partition_rule[k][d].count = 0;
            g_partition_rule[k][d].total_weight = 0;
        }
    }
}

void styles_add_partition_rule(int depth, int kind, const int* sidx, const int* weight, int count)
{
    if (depth < 0 || depth >= 32) return;
    if (kind < 0 || kind >= PART_STYLE_MAX) return;
    style_weight_list* L = &g_partition_rule[kind][depth];
    L->count = 0;
    L->total_weight = 0;
    if (!sidx || !weight || count <= 0) return;
    for (int i = 0; i < count && i < 64; ++i) {
        int si = sidx[i];
        int wt = weight[i];
        L->sidx[L->count] = si;
        L->weight[L->count] = wt;
        L->count++;
        if (wt > 0) L->total_weight += wt;
    }
}

int styles_pick_partition_style(int depth, int kind)
{
    if (depth < 0 || depth >= 32) return styles_pick_random_from_level();
    if (kind < 0 || kind >= PART_STYLE_MAX) return styles_pick_random_from_level();

    style_weight_list* R = &g_partition_rule[kind][depth];
    if (R->count <= 0) return styles_pick_random_from_level();

    /* Filter invalid styles into a temporary list. */
    style_weight_list filtered;
    styles_clear(&filtered);
    for (int i = 0; i < R->count; ++i)
        styles_add(&filtered, R->sidx[i], R->weight[i]);

    if (filtered.count <= 0) return styles_pick_random_from_level();

    int total = filtered.total_weight;
    int r = rand_int(total);
    int pick = filtered.sidx[0];
    for (int i = 0; i < filtered.count; ++i) {
        if (r < filtered.weight[i]) { pick = filtered.sidx[i]; break; }
        r -= filtered.weight[i];
    }
    return pick;
}

/* Expose capacity for style choice arrays (for save/load) */
int styles_get_choice_capacity(void) { return 64; }

/* Copy out the current per-level door variant choices. Caller provides buffer. */
void styles_copy_level_door_choices(byte* out_buf, int max_n)
{
    if (!out_buf || max_n <= 0) return;
    int n = styles_get_choice_capacity();
    if (max_n < n) n = max_n;
    for (int i = 0; i < n; ++i) out_buf[i] = g_level_door_choice[i];
}

/* Load per-level door variant choices from a buffer of length n. */
void styles_load_level_door_choices(const byte* in_buf, int n)
{
    if (!in_buf || n <= 0) return;
    int cap = styles_get_choice_capacity();
    if (n > cap) n = cap;
    for (int i = 0; i < n; ++i) g_level_door_choice[i] = in_buf[i];
}

/* Decode a style index from a cave_color cell (or -1 if not encoded) */
static int style_index_for_color(byte color_value)
{
    if (!z_info || !style_info) return -1;
    /* Support both new (base=128) and legacy (base=200) encodings */
    int bases[2] = { 128, 200 };
    for (int bi = 0; bi < 2; ++bi) {
        int base = bases[bi];
        if (color_value >= base) {
            int slot = color_value - base;
            if (slot >= COLOR_STYLE_SLOT_MAX) slot -= COLOR_STYLE_SLOT_MAX; /* strip first-variant flag */
            if (slot >= 0 && slot < z_info->style_max && style_info[slot].name) return slot;
        }
    }
    return -1;
}

/* Does the encoded color request the first variant explicitly? */
static bool style_color_force_first_variant(byte color_value)
{
    /* Handle both encodings: if value minus either base is >= flag offset, it's first-variant */
    int bases[2] = { 128, 200 };
    for (int bi = 0; bi < 2; ++bi) {
        int base = bases[bi];
        if (color_value >= base) {
            int slot = color_value - base;
            if (slot >= COLOR_STYLE_FLAG_FIRSTVAR) return true;
        }
    }
    return false;
}

/* Return COLOR_STYLE_BASE + active chosen style (vault if active, else level) */
static byte get_active_style_color(void)
{
    int sidx = (g_vault_primary_style >= 0) ? g_vault_primary_style : g_level_primary_style;
    /* Depth-0 safety: if style rules weren't loaded yet, force style 13 */
    if (sidx < 0) {
        if (p_ptr && p_ptr->depth == 0) {
            sidx = 13;
        } else {
            return 0;
        }
    }
    return (byte)(COLOR_STYLE_BASE + sidx);
}
/* Variant of get_active_style_color() that forces the first variant */
/* Note: first-variant override is encoded directly into cave_color by callers */
void styles_select_vault_primary(void)
{
    if (g_vault_styles.count <= 0) {
        g_vault_primary_style = g_level_primary_style;
        log_debug("styles_select_vault_primary: no vault list, defaulting to level primary=%d", g_level_primary_style);
        return;
    }
    int total = 0;
    for (int i = 0; i < g_vault_styles.count; ++i) {
        if (g_vault_styles.sidx[i] == g_vault_avoid_style) continue;
        total += g_vault_styles.weight[i];
    }
    if (total <= 0) total = g_vault_styles.total_weight; /* fallback: nothing to avoid */

    int r = rand_int(total);
    int pick = g_vault_styles.sidx[0];
    for (int i = 0; i < g_vault_styles.count; ++i) {
        if (g_vault_styles.sidx[i] == g_vault_avoid_style && total != g_vault_styles.total_weight) continue;
        if (r < g_vault_styles.weight[i]) { pick = g_vault_styles.sidx[i]; break; }
        r -= g_vault_styles.weight[i];
    }
    g_vault_primary_style = pick;
    log_debug("styles_select_vault_primary: selected vault primary=%d from %d entries (total=%d, avoid=%d)",
        g_vault_primary_style, g_vault_styles.count, g_vault_styles.total_weight, g_vault_avoid_style);
    styles_log_list("styles_select_vault_primary list", &g_vault_styles);
}

int styles_get_vault_primary_style(void) { return g_vault_primary_style; }

/* Pick one weighted-random style from the current level's available list. */
int styles_pick_random_from_level(void)
{
    if (g_level_styles.count <= 0) return -1;
    int total = g_level_styles.total_weight;
    int r = rand_int(total);
    int pick = g_level_styles.sidx[0];
    for (int i = 0; i < g_level_styles.count; ++i) {
        if (r < g_level_styles.weight[i]) { pick = g_level_styles.sidx[i]; break; }
        r -= g_level_styles.weight[i];
    }
    return pick;
}

void styles_set_vault_avoid_style(int sidx)
{
    g_vault_avoid_style = sidx;
}

int styles_decode_color_style(byte color_value)
{
    int bases[2] = { COLOR_STYLE_BASE, COLOR_STYLE_BASE + COLOR_STYLE_FLAG_FIRSTVAR };
    for (int bi = 0; bi < 2; ++bi) {
        int base = bases[bi];
        if (color_value >= base) {
            int slot = color_value - base;
            if (slot >= COLOR_STYLE_FLAG_FIRSTVAR) slot -= COLOR_STYLE_FLAG_FIRSTVAR;
            slot &= (COLOR_STYLE_SLOT_MAX - 1);
            return slot;
        }
    }
    return -1;
}

/*
 * Support for tilesets, lighting and transparency effects
 * by Robert Ruehlmann (rr9@thangorodrim.net)
 */

/*
 * Determine if a given location may be "destroyed"
 *
 * Used by destruction spells, and for placing stairs, etc.
 */
bool cave_valid_bold(int y, int x)
{
    object_type* o_ptr;

    /* Forbid perma-grids */
    if (cave_perma_bold(y, x))
        return (false);

    /* Check objects */
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        // Don't destroy the crown
        if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_3))
            return false;
    }

    /* Accept */
    return (true);
}

/*
 * Multi-hued monsters shimmer according to their base colour.
 */
static byte multi_hued_attr(monster_race* r_ptr)
{
    /* Monsters with an attr other than 'w' choose colors according to attr */
    if (r_ptr->d_attr != TERM_WHITE)
    {
        if ((r_ptr->d_attr == TERM_RED) || (r_ptr->d_attr == TERM_L_RED))
            return ((one_in_(2)) ? TERM_RED : TERM_L_RED);
        if ((r_ptr->d_attr == TERM_BLUE) || (r_ptr->d_attr == TERM_L_BLUE))
            return ((one_in_(2)) ? TERM_BLUE : TERM_L_BLUE);
        if ((r_ptr->d_attr == TERM_WHITE) || (r_ptr->d_attr == TERM_L_WHITE))
            return ((one_in_(2)) ? TERM_WHITE : TERM_L_WHITE);
        if ((r_ptr->d_attr == TERM_GREEN) || (r_ptr->d_attr == TERM_L_GREEN))
            return ((one_in_(2)) ? TERM_GREEN : TERM_L_GREEN);
        if ((r_ptr->d_attr == TERM_UMBER) || (r_ptr->d_attr == TERM_L_UMBER))
            return ((one_in_(2)) ? TERM_UMBER : TERM_L_UMBER);
        if ((r_ptr->d_attr == TERM_ORANGE) || (r_ptr->d_attr == TERM_YELLOW))
            return ((one_in_(2)) ? TERM_ORANGE : TERM_YELLOW);
        if ((r_ptr->d_attr == TERM_L_DARK) || (r_ptr->d_attr == TERM_SLATE))
            return ((one_in_(2)) ? TERM_L_DARK : TERM_SLATE);
        if ((r_ptr->d_attr == TERM_VIOLET)
            || (r_ptr->d_attr == TERM_VIOLET + TERM_SHADE))
            return ((one_in_(2)) ? TERM_VIOLET : TERM_VIOLET + TERM_SHADE);
    }

    /* Otherwise can be any color */
    return (dieroll(15));
}

/*
 * Hack -- Legal monster codes
 */
static const char image_monster_hack[]
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

/*
 * Hack -- Hallucinatory monster
 */
static u16b image_monster(void)
{
    byte a;
    char c;

    /* Random symbol from set above (not including final nul) */
    c = image_monster_hack[rand_int(sizeof(image_monster_hack) - 1)];

    /* Random color */
    a = dieroll(15);

    /* Encode */
    return (PICT(a, c));
}

/*
 * Hack -- Legal object codes
 */
static const char image_object_hack[] = "?/|\\\"!$()_-=[]{},~"; /* " */

/*
 * Hack -- Hallucinatory object
 */
static u16b image_object(void)
{
    byte a;
    char c;

    /* Random symbol from set above (not including final nul) */
    c = image_object_hack[rand_int(sizeof(image_object_hack) - 1)];

    /* Random color */
    a = dieroll(15);

    /* Encode */
    return (PICT(a, c));
}

/*
 * Hack -- Random hallucination
 */
static u16b image_random(void)
{
    /* Normally, assume monsters */
    if (percent_chance(75))
    {
        return (image_monster());
    }

    /* Otherwise, assume objects */
    else
    {
        return (image_object());
    }
}

/*
 * The 16x16 tile of the terrain supports lighting
 */
bool feat_supports_lighting(int feat)
{
    /* Pseudo graphics don't support lighting */
    if (use_graphics == GRAPHICS_PSEUDO)
        return false;

    if ((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
    {
        return true;
    }

    switch (feat)
    {
    case FEAT_FLOOR:
    case FEAT_SECRET:
    case FEAT_QUARTZ:
    case FEAT_WALL_EXTRA:
    case FEAT_WALL_INNER:
    case FEAT_WALL_OUTER:
    case FEAT_WALL_SOLID:
    case FEAT_WALL_PERM:
        return true;
    default:
        return false;
    }
}

static byte darken(byte a, byte c)
{
    // don't darken the symbols for traps
    if (c == '^')
        return (a);

    // or chasms or shafts
    if (((c == '%') || (c == '>') || (c == '<')) && a == TERM_L_DARK)
        return (a);

    if (a == TERM_WHITE)
        return (TERM_SLATE);
    if (a == TERM_L_WHITE)
        return (TERM_L_DARK);
    if (a == TERM_SLATE)
        return (TERM_L_DARK);
    if (a == TERM_L_DARK)
        return (TERM_DARK + TERM_SHADE);
    if (a == TERM_L_UMBER)
        return (TERM_UMBER);
    if (a == TERM_L_BLUE)
        return (TERM_BLUE);
    if (a == TERM_L_GREEN)
        return (TERM_GREEN);
    if (a == TERM_L_WHITE + TERM_SHADE)
        return (TERM_L_DARK + TERM_SHADE);

    return (a);
}

static void special_lighting_floor(byte* a, char* c, int info, int light)
{
    /* Determine if this grid should appear dark because of blindness or lack of light. */
    bool is_dark = false;

    if (p_ptr->blind)
    {
        is_dark = true;
    }
    else
    {
        /* Treat as dark when there's no positive light and the grid isn't sunlit. */
        if (light <= 0 && !(info & (CAVE_GLOW)))
            is_dark = true;
    }

    switch (use_graphics)
    {
    case GRAPHICS_NONE:
    case GRAPHICS_PSEUDO:
        /* In ASCII modes, darken for truly dark grids; also dim unseen grids. */
        if (is_dark)
        {
            *a = TERM_DARK + TERM_SHADE;
        }
        else if (!(info & (CAVE_SEEN)))
        {
            *a = darken(*a, *c);
        }
        break;

    case GRAPHICS_MICROCHASM:
        /* In tiles, use the darker variant for dark or unseen grids. */
        if (is_dark || !(info & (CAVE_SEEN)))
        {
            *c += 1;
        }
        break;
    }
}

static void special_lighting_wall(byte* a, char* c, int feat, int info, int light)
{
    /* Determine brightness based on blindness and dynamic light (torch/lantern) */
    bool is_dark = false;

    if (p_ptr->blind)
    {
        is_dark = true;
    }
    else
    {
        /* If there's no positive light on this grid and it's not sunlit, treat as dark */
        if (light <= 0 && !(info & (CAVE_GLOW)))
            is_dark = true;
    }

    switch (use_graphics)
    {
    case GRAPHICS_NONE:
    case GRAPHICS_PSEUDO:
        if (is_dark)
        {
            /* darken the colour */
            *a = darken(*a, *c);
        }
        break;
    case GRAPHICS_MICROCHASM:
        if (feat_supports_lighting(feat)
            && (is_dark
                || (((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
                    && !(info & (CAVE_SEEN)))))
        {
            /* use darker tile variant */
            *c += 1;
        }
        break;
    }

    if (ui_colors_use_backgrounds())
    {
        switch (use_graphics)
        {
        case GRAPHICS_NONE:
        case GRAPHICS_PSEUDO:
        {
            if (hybrid_walls && ((*c == '#') || (*c == '%')))
            {
                *a = *a + (MAX_COLORS * BG_DARK);
            }
            else if (solid_walls && ((*c == '#') || (*c == '%')))
            {
                *a = *a + (MAX_COLORS * BG_SAME);
            }
        }
        break;
        }
    }
}

/*
 * Group-aware floor and door graphics (extensible)
 * These helpers allow selecting alternative tiles for floors and doors
 * based on the current level's group color (cave_color). If no specific
 * override exists for a group, they return false and the caller keeps
 * the default feature tiles. This provides a safe default while enabling
 * future per-group customization for floors and doors.
 */
static bool is_door_feat(int feat)
{
    /* Open / broken doors */
    if (feat == FEAT_OPEN || feat == FEAT_BROKEN) return true;
    /* Closed/locked/jammed doors */
    if ((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL)) return true;
    return false;
}

static bool apply_style_floor_graphics(int y, int x, int feat, int info, byte* a, char* c)
{
    /* Only consider non-ASCII graphics; ASCII uses chars/colors directly */
    if (graphics_are_ascii()) return false;

    /* Safety: only floors here */
    if (feat != FEAT_FLOOR && feat != FEAT_RAGE_FLOOR && feat != FEAT_SUNLIGHT)
        return false;

    /* Respect per-cell color selection; 0/1/2 are defaults/legacy/vault */
    byte color_value = cave_color[y][x];

    /* Use style from absolute override or group */
    int sidx = style_index_for_color(color_value);
    if (sidx >= 0)
    {
        style_type* s = &style_info[sidx];
        /* Halo can force variant 0 via color flag */
        byte choice = 0;
        if (!style_color_force_first_variant(color_value) && s->floor_count > 1) {
            choice = style_floor_choice(sidx);
        }
        byte fr = (s->floor_count > 0) ? s->floor_rowv[choice] : s->floor_row;
        byte fc = (s->floor_count > 0) ? s->floor_colv[choice] : s->floor_col;
    *a = (byte)(fr | 0x80);
    *c = (char)(fc | 0x80);
        /* Let special_lighting_floor() adjust brightness afterwards */
        return true;
    }

    (void)info; (void)a; (void)c; (void)y; (void)x; (void)feat;
    return false;
}

static bool apply_style_door_graphics(int y, int x, int feat, int info, byte* a, char* c)
{
    /* Only consider non-ASCII graphics; ASCII uses chars/colors directly */
    if (graphics_are_ascii()) return false;

    if (!is_door_feat(feat)) return false;

    /* Respect per-cell color selection; 0/1/2 are defaults/legacy/vault */
    byte color_value = cave_color[y][x];

    /* Resolve style from absolute override or group selection */
    int sidx = style_index_for_color(color_value);
    if (sidx >= 0)
    {
        style_type* s = &style_info[sidx];
        /* Respect first-variant override (if ever used for doors) */
        byte choice = 0;
        if (!style_color_force_first_variant(color_value) && s->door_count > 1) {
            choice = style_door_choice(sidx);
        }
        int row = (s->door_count > 0) ? s->door_rowv[choice] : s->door_row;
        int col = (s->door_count > 0) ? s->door_colv[choice] : s->door_col;
        if (feat == FEAT_OPEN) col += 1; else if (feat == FEAT_BROKEN) col += 2;
    *a = (byte)(row | 0x80);
    *c = (char)(col | 0x80);
        return true;
    }

    (void)info; (void)feat; (void)a; (void)c; (void)y; (void)x;
    return false;
}

static byte feature_visual_attr(const feature_type* f_ptr)
{
    return graphics_are_ascii() ? f_ptr->d_attr : f_ptr->x_attr;
}

static char feature_visual_char(const feature_type* f_ptr)
{
    return graphics_are_ascii() ? f_ptr->d_char : f_ptr->x_char;
}

static byte race_visual_attr(const monster_race* r_ptr)
{
    return graphics_are_ascii() ? r_ptr->d_attr : r_ptr->x_attr;
}

static char race_visual_char(const monster_race* r_ptr)
{
    return graphics_are_ascii() ? r_ptr->d_char : r_ptr->x_char;
}

int player_tile_offset()
{
    object_type * main_wield_ptr = &inventory[INVEN_WIELD];
    object_type * secondary_wield_ptr = &inventory[INVEN_ARM];

    byte main_type = main_wield_ptr->tval;
    byte main_subtype = main_wield_ptr->sval;

    byte secondary_type = secondary_wield_ptr->tval;

    if (secondary_type && !main_type)
    {
        main_type = secondary_type;
        main_subtype = secondary_wield_ptr->sval;
        secondary_type = 0;
    }

    bool smallSwordMain =
        (main_type == TV_SWORD && main_subtype == SV_DAGGER) ||
        (main_type == TV_SWORD && main_subtype == SV_SHORT_SWORD);
    bool curvedSwordMain =
        (main_type == TV_SWORD && main_subtype == SV_CURVED_SWORD);
    bool bigSwordMain =
        (main_type == TV_SWORD && main_subtype > SV_CURVED_SWORD) ||
        (main_type == TV_DIGGING && main_subtype == SV_SHOVEL);
    bool spearMain =
        (main_type == TV_POLEARM && main_subtype < SV_HAND_AXE);
    bool smallAxeMain =
        (main_type == TV_POLEARM && main_subtype == SV_HAND_AXE) ||
        (main_type == TV_HAFTED && main_subtype == SV_WAR_HAMMER) ||
        (main_type == TV_DIGGING && main_subtype == SV_MATTOCK);
    bool bigAxeMain =
        (main_type == TV_POLEARM && main_subtype > SV_HAND_AXE);
    bool quarterstaffMain =
        (main_type == TV_HAFTED && main_subtype == SV_QUARTERSTAFF);
    bool shieldOffhand =
        (secondary_type == TV_SHIELD);
    bool axeOffhand =
        (secondary_type == TV_POLEARM);
    bool swordOffhand =
        (secondary_type == TV_SWORD);

    if (inventory[INVEN_BOW].tval == TV_BOW &&
        p_ptr->skill_use[S_ARC] > p_ptr->skill_use[S_MEL])
    {
        return 15;
    }
    if (!secondary_type)
    {
        if (smallSwordMain)
        {
            return 1;
        }
        if (curvedSwordMain)
        {
            return 2;
        }
        if (bigSwordMain)
        {
            return 3;
        }
        if (spearMain)
        {
            return 4;
        }
        if (smallAxeMain)
        {
            return 5;
        }
        if (bigAxeMain)
        {
            return 6;
        }
        if (quarterstaffMain)
        {
            return 7;
        }
    }
    else if (shieldOffhand)
    {
        if (bigAxeMain)
        {
            return 9;
        }
        if (smallAxeMain)
        {
            return 10;
        }
        return 8;
    }
    else if (swordOffhand)
    {
        if (smallAxeMain || bigAxeMain)
        {
            return 13;
        }
        return 11;
    }
    else if (axeOffhand)
    {
        if (smallAxeMain || bigAxeMain)
        {
            return 14;
        }
        return 12;
    }

    return 0;
}


/*
 * Extract the attr/char to display at the given (legal) map location
 *
 * Note that this function feeds the live dungeon snapshot and is called
 * repeatedly from view updates, so it remains a major efficiency concern.
 *
 * Basically, we examine each "layer" of the world (terrain, objects,
 * monsters/players), from the bottom up, extracting a new attr/char
 * if necessary at each layer, and defaulting to "darkness".  This is
 * not the fastest method, but it is very simple, and it is about as
 * fast as it could be for grids which contain no "marked" objects or
 * "visible" monsters.
 *
 * We apply the effects of hallucination during each layer.  Objects will
 * always appear as random "objects", monsters will always appear as random
 * "monsters", and normal grids occasionally appear as random "monsters" or
 * "objects", but note that these random "monsters" and "objects" are really
 * just "colored ascii symbols" (which may look silly on some machines).
 *
 * The hallucination functions avoid taking any pointers to local variables
 * because some compilers refuse to use registers for any local variables
 * whose address is taken anywhere in the function.
 *
 * As an optimization, we can handle the "player" grid as a special case.
 *
 * Note that the memorization of "objects" and "monsters" is not related
 * to the memorization of "terrain".  This allows the player to memorize
 * the terrain of a grid without memorizing any objects in that grid, and
 * to detect monsters without detecting anything about the terrain of the
 * grid containing the monster.
 *
 * The fact that all interesting "objects" and "terrain features" are
 * memorized as soon as they become visible for the first time means
 * that we only have to check the "CAVE_SEEN" flag for "boring" grids.
 *
 * Note that bizarre things must be done when the "attr" and/or "char"
 * codes have the "high-bit" set, since these values are used to encode
 * various "special" pictures in some versions, and certain situations,
 * such as "multi-hued" or "clear" monsters, cause the attr/char codes
 * to be "scrambled" in various ways.
 *
 * Note that the "zero" entry in the feature/object/monster arrays are
 * used to provide "special" attr/char codes, with "monster zero" being
 * used for the player attr/char, "object zero" being used for the "pile"
 * attr/char, and "feature zero" being used for the "darkness" attr/char.
 *
 * Note the assumption that doing "x_ptr = &x_info[x]" plus a few of
 * "x_ptr->xxx", is quicker than "x_info[x].xxx", even if "x" is a fixed
 * constant.  If this is incorrect then a lot of code should be changed.
 *
 *
 * Some comments on the "terrain" layer...
 *
 * Note that "boring" grids (floors, invisible traps, and any illegal grids)
 * are very different from "interesting" grids (all other terrain features),
 * and the two types of grids are handled completely separately.  The most
 * important distinction is that "boring" grids may or may not be memorized
 * when they are first encountered, and so we must use the "CAVE_SEEN" flag
 * to see if they are "see-able".
 *
 *
 * Some comments on the "terrain" layer (boring grids)...
 *
 * Note that "boring" grids are always drawn using the picture for "empty
 * floors", which is stored in "f_info[FEAT_FLOOR]".  Sometimes, special
 * lighting effects may cause this picture to be modified.
 *
 * Note that "invisible traps" are always displayes exactly like "empty
 * floors", which prevents various forms of "cheating", with no loss of
 * efficiency.  There are still a few ways to "guess" where traps may be
 * located, for example, objects will never fall into a grid containing
 * an invisible trap.  XXX XXX
 *
 * To determine if a "boring" grid should be displayed, we simply check to
 * see if it is either memorized ("CAVE_MARK"), or currently "see-able" by
 * the player ("CAVE_SEEN").  Note that "CAVE_SEEN" is now maintained by the
 * "update_view()" function.
 *
 *
 * Some comments on the "terrain" layer (non-boring grids)...
 *
 * Note the use of the "mimic" field in the "terrain feature" processing,
 * which allows any feature to "pretend" to be another feature.  This is
 * used to "hide" secret doors, and to make all "doors" appear the same,
 * and all "walls" appear the same, and "hidden" treasure stay hidden.
 *
 * Since "interesting" grids are always memorized as soon as they become
 * "see-able" by the player ("CAVE_SEEN"), such a grid only needs to be
 * displayed if it is memorized ("CAVE_MARK").  Most "interesting" grids
 * are in fact non-memorized, non-see-able, wall grids, so the fact that
 * we do not have to check the "CAVE_SEEN" flag adds some efficiency, at
 * the cost of *forcing* the memorization of all "interesting" grids when
 * they are first seen.  Since the "CAVE_SEEN" flag is now maintained by
 * the "update_view()" function, this efficiency is not as significant as
 * it was in previous versions, and could perhaps be removed.
 *
 * Note that "wall" grids are more complicated than "boring" grids, due to
 * the fact that "CAVE_GLOW" for a "wall" grid means that the grid *might*
 * be glowing, depending on where the player is standing in relation to the
 * wall.  In particular, the wall of an illuminated room should look just
 * like any other (dark) wall unless the player is actually inside the room.
 *
 * Thus, we do not support as many visual special effects for "wall" grids
 * as we do for "boring" grids, since many of them would give the player
 * information about the "CAVE_GLOW" flag of the wall grid, in particular,
 * it would allow the player to notice the walls of illuminated rooms from
 * a dark hallway that happened to run beside the room.
 *
 *
 * Some comments on the "object" layer...
 *
 * Currently, we do nothing with multi-hued objects, because there are
 * not any.  If there were, they would have to set "shimmer_objects"
 * when they were created, and then new "shimmer" code in "dungeon.c"
 * would have to be created handle the "shimmer" effect, and the code
 * in "cave.c" would have to be updated to create the shimmer effect.
 * This did not seem worth the effort.  XXX XXX
 *
 *
 * Some comments on the "monster"/"player" layer...
 *
 * Note that monsters can have some "special" flags, including "ATTR_MULTI",
 * which means their color changes, and "ATTR_CLEAR", which means they take
 * the color of whatever is under them, and "CHAR_CLEAR", which means that
 * they take the symbol of whatever is under them.  Technically, the flag
 * "CHAR_MULTI" is supposed to indicate that a monster looks strange when
 * examined, but this flag is currently ignored.
 *
 * Normally, players could be handled just like monsters, except that the
 * concept of the "torch lite" of others player would add complications.
 * For efficiency, however, we handle the (only) player first, since the
 * "player" symbol always "pre-empts" any other facts about the grid.
 *
 * ToDo: The transformations for tile colors, or brightness for the 16x16
 * tiles should be handled differently.  One possibility would be to
 * extend feature_type with attr/char definitions for the different states.
 */

#define GRAF_BROKEN_BONE 440

static bool monster_can_see_player_for_stealth_vision(monster_type* m_ptr)
{
    if (!m_ptr || !m_ptr->r_idx)
        return false;

    /* Sleeping creatures cannot see */
    if (m_ptr->alertness < ALERTNESS_UNWARY)
        return false;

    const monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Peaceful creatures don't matter for stealth vision */
    if (r_ptr->flags1 & (RF1_PEACEFUL))
        return false;

    /* Shortsighted creatures can't see beyond 2 squares */
    if ((r_ptr->flags2 & (RF2_SHORT_SIGHTED)) && (m_ptr->cdis > 2))
        return false;

    if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
        return false;

    /* Visual recognition for intelligent monsters */
    if (visual_recognition && (r_ptr->flags2 & (RF2_SMART)))
    {
        /* Disguise reduces monster's effective perception */
        int per_divisor = p_ptr->active_ability[S_STL][STL_DISGUISE] ? 4 : 2;

        int vision_score = monster_skill(m_ptr, S_PER) / per_divisor + p_ptr->cur_light
            + ((cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)) ? 2 : 0);

        if (vision_score < m_ptr->cdis)
            return false;
    }

    return true;
}

void map_info(int y, int x, byte* ap, char* cp, byte* tap, char* tcp)
{
    byte a = TERM_DARK; // these are defaults to soothe compilation warnings
    char c = ' '; //

    byte feat;
    u16b info;

    feature_type* f_ptr;
    object_type* o_ptr;

    s16b m_idx;

    s16b image = p_ptr->image;

    /* Monster/Player */
    m_idx = cave_m_idx[y][x];

    /* Feature */
    feat = cave_feat[y][x];

    /* Cave flags */
    info = cave_info[y][x];

    bool hide_square = false;
    bool rage_active = false;

    // Hide memorized squares out of line of sight during rage, and while in labyrinth partitions.
    if ((!p_ptr->is_dead) && (p_ptr->rage || g_labyrinth_view_active) && !(info & (CAVE_SEEN)))
        hide_square = true;

    // 'rage' visuals (red filter, rage tiles, etc.) - labyrinth uses the hide-only behavior above.
    if ((!p_ptr->is_dead) && p_ptr->rage)
        rage_active = true;

    /* make sure not to display things off screen */
    if ((y < 0) || (x < 0) || (y >= p_ptr->cur_map_hgt)
        || (x >= p_ptr->cur_map_wid))
    {
        /* Get the darkness feature */
        f_ptr = &f_info[FEAT_NONE];

        /* Normal attr */
        a = feature_visual_attr(f_ptr);

        /* Normal char */
        c = feature_visual_char(f_ptr);
    }

    // hiding squares out of line of sight during rage
    else if (hide_square)
    {
        /* Get the darkness feature */
        f_ptr = &f_info[FEAT_NONE];

        /* Normal attr */
        a = feature_visual_attr(f_ptr);

        /* Normal char */
        c = feature_visual_char(f_ptr);
    }

    /* Boring grids (floors, etc) */
    else if (cave_floorlike_bold(y, x))
    {
        /* Memorized (or seen) floor */
        if ((info & (CAVE_MARK)) || (info & (CAVE_SEEN)))
        {
            int floor_feat = FEAT_FLOOR;

            if (rage_active && !graphics_are_ascii())
            {
                floor_feat = FEAT_RAGE_FLOOR;
            }

            /* Get the floor feature */
            f_ptr = &f_info[floor_feat];

            /* Normal attr */
            a = feature_visual_attr(f_ptr);

            /* Normal char */
            c = feature_visual_char(f_ptr);

            /* Optional: apply group-based override for floor tiles */
            (void)apply_style_floor_graphics(y, x, feat, info, &a, &c);

            /* Skip special light for the player tile. */
            special_lighting_floor(&a, &c, info, cave_light[y][x]);
        }

        /* Unknown */
        else
        {
            /* Get the darkness feature */
            f_ptr = &f_info[FEAT_NONE];

            /* Normal attr */
            a = feature_visual_attr(f_ptr);

            /* Normal char */
            c = feature_visual_char(f_ptr);
        }
    }

    /* Interesting grids (non-floors) */
    else
    {
        /* Memorized grids */
        if (info & (CAVE_MARK))
        {
            /* Apply "mimic" field */
            feat = f_info[feat].mimic;

            if (rage_active && !graphics_are_ascii()
                && (feat >= FEAT_WALL_HEAD && feat <= FEAT_WALL_TAIL))
            {
                feat = FEAT_RAGE_WALL;
            }

            /* Get the feature */
            f_ptr = &f_info[feat];

            /* Normal attr */
            a = feature_visual_attr(f_ptr);

            /* Normal char */
            c = feature_visual_char(f_ptr);

            /* Optional: apply group-based override for doors */
            (void)apply_style_door_graphics(y, x, feat, info, &a, &c);

#if DEPTH_BASED_WALLS
            /* Apply style-based wall/vein graphics for non-ASCII graphics */
            if (!graphics_are_ascii() && (feat >= FEAT_WALL_HEAD && feat <= FEAT_WALL_TAIL) && feat != FEAT_RUBBLE)
            {
                /* Get the cave color for this location */
                byte color_value = cave_color[y][x];

                /* Decode style index from cave_color (first-variant flag is ignored here) */
                int sidx2 = style_index_for_color(color_value);
                if (feat == FEAT_QUARTZ) {
                    /* Veins */
                    if (sidx2 >= 0) {
                        style_type* s = &style_info[sidx2];
                        if (s->vein_defined) {
                            /* Full replacement vein tile */
                            a = (byte)(s->vein_row | 0x80);
                            c = (char)(s->vein_col | 0x80);
                        } else {
                            /* Overlay default vein tile on this style's wall tile */
                            extern byte get_default_vein_row(void);
                            extern byte get_default_vein_col(void);
                            byte dv_r = get_default_vein_row();
                            byte dv_c = get_default_vein_col();
                            byte wall_a = (byte)(s->wall_row | 0x80);
                            byte wall_c = (byte)(s->wall_col | 0x80);
                            if (use_graphics == GRAPHICS_MICROCHASM && feat_supports_lighting(feat)) {
                                if (p_ptr->blind || (!(info & (CAVE_GLOW)) && cave_light[y][x] <= 0)) {
                                    wall_c += 1;
                                }
                            }
                            *tap = wall_a; *tcp = wall_c;
                            a = (byte)(dv_r | 0x80); c = (char)(dv_c | 0x80);
                            if (use_graphics == GRAPHICS_MICROCHASM && feat_supports_lighting(feat)) {
                                if (p_ptr->blind || (!(info & (CAVE_GLOW)) && cave_light[y][x] <= 0)) {
                                    c += 1;
                                }
                            }
                            
                            /* Check for visible monster on this vein before returning */
                            if ((m_idx > 0) && !hide_square) {
                                monster_type* m_ptr = &mon_list[m_idx];
                                if (m_ptr->ml) {
                                    monster_race* r_ptr = &r_info[m_ptr->r_idx];
                                    if (image) r_ptr = &r_info[m_ptr->image_r_idx];
                                    byte da = r_ptr->x_attr;
                                    char dc = r_ptr->x_char;
                                    if ((da & 0x80) && (dc & 0x80)) {
                                        a = da; c = dc;
                                    } else if (r_ptr->flags1 & (RF1_ATTR_MULTI)) {
                                        a = multi_hued_attr(r_ptr); c = dc;
                                    } else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR | RF1_CHAR_CLEAR))) {
                                        a = da; c = dc;
                                    }
                                    if (rage_active && graphics_are_ascii()) a = TERM_RED;
                                    if (!graphics_are_ascii() && m_ptr->alertness >= ALERTNESS_ALERT) c += GRAPHICS_ALERT_MASK;
                                }
                            }
                            
                            *ap = a; *cp = c;
                            return;
                        }
                    } else {
                        /* No encoded style in cave_color: fall back to primary style (level or vault). */
                        int fb = (g_vault_primary_style >= 0 && (cave_info[y][x] & CAVE_ICKY)) ? g_vault_primary_style : g_level_primary_style;
                        if (fb >= 0 && style_info[fb].name) {
                            style_type* sfb = &style_info[fb];
                            byte wall_a = (byte)(sfb->wall_row | 0x80);
                            byte wall_c = (byte)(sfb->wall_col | 0x80);
                            extern byte get_default_vein_row(void);
                            extern byte get_default_vein_col(void);
                            byte dv_r = get_default_vein_row();
                            byte dv_c = get_default_vein_col();
                            a = (byte)(dv_r | 0x80); c = (char)(dv_c | 0x80);
                            if (use_graphics == GRAPHICS_MICROCHASM && feat_supports_lighting(feat)) {
                                if (p_ptr->blind || (!(info & (CAVE_GLOW)) && cave_light[y][x] <= 0)) {
                                    c += 1; wall_c += 1;
                                }
                            }
                            *tap = wall_a; *tcp = wall_c; /* base wall */
                            log_warn("VEIN fallback: unencoded cave_color=%d at (%d,%d); using primary style %d wall(row=%d,col=%d)", color_value, y, x, fb, sfb->wall_row, sfb->wall_col);
                            
                            /* Check for visible monster on this vein before returning */
                            if ((m_idx > 0) && !hide_square) {
                                monster_type* m_ptr = &mon_list[m_idx];
                                if (m_ptr->ml) {
                                    monster_race* r_ptr = &r_info[m_ptr->r_idx];
                                    if (image) r_ptr = &r_info[m_ptr->image_r_idx];
                                    byte da = r_ptr->x_attr;
                                    char dc = r_ptr->x_char;
                                    if ((da & 0x80) && (dc & 0x80)) {
                                        a = da; c = dc;
                                    } else if (r_ptr->flags1 & (RF1_ATTR_MULTI)) {
                                        a = multi_hued_attr(r_ptr); c = dc;
                                    } else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR | RF1_CHAR_CLEAR))) {
                                        a = da; c = dc;
                                    }
                                    if (rage_active && graphics_are_ascii()) a = TERM_RED;
                                    if (!graphics_are_ascii() && m_ptr->alertness >= ALERTNESS_ALERT) c += GRAPHICS_ALERT_MASK;
                                }
                            }
                            
                            *ap = a; *cp = c;
                            return;
                        } else {
                            /* Give up: leave existing tiles */
                            log_warn("VEIN fallback: no primary style available at (%d,%d)", y, x);
                            return;
                        }
                    }
                } else {
                    /* Walls */
                    if (sidx2 >= 0) {
                        style_type* s2 = &style_info[sidx2];
                        a = (byte)(s2->wall_row | 0x80);
                        c = (char)(s2->wall_col | 0x80);
                    } else {
                        int fb = (g_vault_primary_style >= 0 && (cave_info[y][x] & CAVE_ICKY)) ? g_vault_primary_style : g_level_primary_style;
                        if (fb >= 0 && style_info[fb].name) {
                            style_type* sfb = &style_info[fb];
                            a = (byte)(sfb->wall_row | 0x80);
                            c = (char)(sfb->wall_col | 0x80);
                            log_warn("WALL fallback: unencoded cave_color=%d at (%d,%d); using primary style %d (row=%d,col=%d)", color_value, y, x, fb, sfb->wall_row, sfb->wall_col);
                        } else {
                            a = (byte)(15 | 0x80);
                            c = (char)(14 | 0x80);
                            log_warn("WALL fallback: no primary style; using hard fallback at (%d,%d)", y, x);
                        }
                    }
                }

                /* Apply standard lighting effects (+1 for dark version)
                 * Dark variant only if not sunlit (no CAVE_GLOW) or if blind. */
                if (use_graphics == GRAPHICS_MICROCHASM && feat_supports_lighting(feat)) {
                    if (p_ptr->blind || (!(info & (CAVE_GLOW)) && cave_light[y][x] <= 0)) {
                        c += 1;
                    }
                }

                /* Save both terrain and display attributes */
                *tap = a;
                *tcp = c;

                /* Don't return early - let monster display code run */
            }
            else {
                /* Standard lighting effects for non-wall features */
                special_lighting_wall(&a, &c, feat, info, cave_light[y][x]);
            }
#else
            /* Depth-based walls disabled, use standard lighting only */
            special_lighting_wall(&a, &c, feat, info, cave_light[y][x]);
#endif /* DEPTH_BASED_WALLS */
        }

        /* Unknown */
        else
        {
            /* Get the darkness feature */
            f_ptr = &f_info[FEAT_NONE];

            /* Normal attr */
            a = feature_visual_attr(f_ptr);

            /* Normal char */
            c = feature_visual_char(f_ptr);
        }
    }

    /* Save the terrain info for the transparency effects */
    byte terrain_a = a;
    char terrain_c = c;

    /* Traps, stairs, shafts, forges, sunlight, and rubble are drawn as a middle layer in
     * the SDL renderer (floor -> feature -> monster). For transparency to work, use a
     * floor tile as the terrain underlay when one of these features is visible. */
    if ((info & (CAVE_MARK)) &&
        (((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL)) ||
         ((feat >= FEAT_STAIR_HEAD) && (feat <= FEAT_STAIR_TAIL)) ||
         ((feat >= FEAT_FORGE_HEAD) && (feat <= FEAT_FORGE_TAIL)) ||
         (feat == FEAT_SUNLIGHT) ||
         (feat == FEAT_RUBBLE)))
    {
        int floor_feat = FEAT_FLOOR;
        if (rage_active && !graphics_are_ascii())
            floor_feat = FEAT_RAGE_FLOOR;

        feature_type* floor_ptr = &f_info[floor_feat];
        terrain_a = feature_visual_attr(floor_ptr);
        terrain_c = feature_visual_char(floor_ptr);

        (void)apply_style_floor_graphics(y, x, floor_feat, info, &terrain_a, &terrain_c);
        special_lighting_floor(&terrain_a, &terrain_c, info, cave_light[y][x]);
    }

    (*tap) = terrain_a;
    (*tcp) = terrain_c;

    /* Objects (only shown when on floors, not when in rubble) */
    if (feat == FEAT_FLOOR || feat == FEAT_SUNLIGHT)
    {
        for (o_ptr = get_first_object(y, x); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            /* Memorized objects */
            if (o_ptr->marked && !hide_square)
            {
                /* Normal attr */
                a = object_attr(o_ptr);

                /* Normal char */
                c = object_char(o_ptr);

                /* display this */
                break;
            }
        }
    }

    /* Monsters */
    if ((m_idx > 0) && !hide_square)
    {
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        if (!m_ptr->ml)
        {
            byte ha;
            char hc;

            if (song_revealing_overlay(m_idx, &ha, &hc)
                || listen_hint_overlay(m_idx, &ha, &hc))
            {
                a = ha;
                c = hc;
            }
        }

        /* Visible monster*/
        if (m_ptr->ml)
        {
            byte da;
            char dc;

            /* Hack -- monster hallucination */
            if (image)
            {
                r_ptr = &r_info[m_ptr->image_r_idx];
            }

            /* Desired attr */
            da = race_visual_attr(r_ptr);

            /* Desired char */
            dc = race_visual_char(r_ptr);

            /* Special attr/char codes */
            if ((da & 0x80) && (dc & 0x80))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Multi-hued monster */
            else if (r_ptr->flags1 & (RF1_ATTR_MULTI))
            {
                /* Multi-hued attr */
                a = multi_hued_attr(r_ptr);

                /* Normal char */
                c = dc;
            }

            /* Normal monster (not "clear" in any way) */
            else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR | RF1_CHAR_CLEAR)))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Hack -- Bizarre grid under monster */
            else if ((a & 0x80) || (c & 0x80))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Normal char, Clear attr, monster */
            else if (!(r_ptr->flags1 & (RF1_CHAR_CLEAR)))
            {
                /* Normal char */
                c = dc;
            }

            /* Normal attr, Clear char, monster */
            else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR)))
            {
                /* Normal attr */
                a = da;
            }

            if (rage_active && graphics_are_ascii())
            {
                a = TERM_RED;
            }

            if (hilite_unwary && (m_ptr->alertness < ALERTNESS_ALERT)
                && ui_colors_use_backgrounds() && graphics_are_ascii())
            {
                a += (MAX_COLORS * BG_DARK);
            }
            else if (!graphics_are_ascii()
                && m_ptr->alertness >= ALERTNESS_ALERT)
            {
                c += GRAPHICS_ALERT_MASK;
            }

            /* Sleeping overlay: indicate when this monster is asleep. */
            if (!graphics_are_ascii() && sleep_icon && tap != ap
                && m_ptr->alertness < ALERTNESS_UNWARY)
            {
                *tap = (byte)(((byte)(*tap)) | GRAPHICS_SLEEP_MASK);
            }

            /* Stealth vision overlay: indicate when this monster can see you. */
            if (!graphics_are_ascii() && stealth_vision && tcp != cp
                && monster_can_see_player_for_stealth_vision(m_ptr))
            {
                *tcp = (char)(((byte)(*tcp)) | GRAPHICS_SEEN_MASK);
            }
        }
    }

    /* Handle "player" */
    else if (m_idx < 0)
    {
        monster_race* r_ptr = &r_info[0];

        if (graphics_are_ascii())
        {
            a = health_attr(p_ptr->chp, p_ptr->mhp);
            c = r_ptr->d_char;
        }
        else
        {
            r_ptr = &r_info[p_ptr->prace]; // XXX grafic for player

            a = r_ptr->x_attr;
            c = r_ptr->x_char;
            c += player_tile_offset();
        }
    }

    /* Result */
    (*ap) = a;
    (*cp) = c;
}

/*
 * Same as map_info, but always return the char/attr specified by the
 * info files.
 * This IS an hack, I dont like to duplicate code like that, but the only
 * other way it to hack map_info itself and put lots of if statements in it,
 * which could reduce speed.
 */
void map_info_default(int y, int x, byte* ap, char* cp)
{
    byte a;
    char c;

    byte feat;
    u16b info;

    feature_type* f_ptr;
    object_type* o_ptr;

    s16b m_idx;

    s16b image = p_ptr->image;

    int floor_num = 0;

    /* Monster/Player */
    m_idx = cave_m_idx[y][x];

    /* Feature */
    feat = cave_feat[y][x];

    /* Cave flags */
    info = cave_info[y][x];

    /* Hack -- rare random hallucination on non-outer walls */
    if ((image) && (feat < FEAT_WALL_PERM) && (image_count-- <= 0))
    {
        int i = image_random();

        a = PICT_A(i);
        c = PICT_C(i);
    }

    /* Boring grids (floors, etc) */
    else if (cave_floorlike_bold(y, x))
    {
        /* Memorized (or seen) floor */
        if ((info & (CAVE_MARK)) || (info & (CAVE_SEEN)))
        {
            /* Get the floor feature */
            f_ptr = &f_info[FEAT_FLOOR];

            /* Normal attr */
            a = f_ptr->d_attr;

            /* Normal char */
            c = f_ptr->d_char;

            /* Handle "seen" grids */
            if (info & (CAVE_SEEN))
            {
                /* Do Nothing */
            }

            /* Handle "blind" */
            else if ((p_ptr->blind) || (!(info & (CAVE_GLOW))))
            {
                /* Darken the colour */
                a = darken(a, c);
            }

            /* Handle "unseen" grids */
            else
            {
                /* Darken the colour */
                a = darken(a, c);
            }
        }

        /* Unknown */
        else
        {
            /* Get the darkness feature */
            f_ptr = &f_info[FEAT_NONE];

            /* Normal attr */
            a = f_ptr->d_attr;

            /* Normal char */
            c = f_ptr->d_char;
        }
    }

    /* Interesting grids (non-floors) */
    else
    {
        /* Memorized grids */
        if (info & (CAVE_MARK))
        {
            /* Apply "mimic" field */
            feat = f_info[feat].mimic;

            /* Get the feature */
            f_ptr = &f_info[feat];

            /* Normal attr */
            a = f_ptr->d_attr;

            /* Normal char */
            c = f_ptr->d_char;

            /* Special lighting effects (walls only) */
            if (cave_wall_bold(y, x) || feat_supports_lighting(feat))
            {
                /* Handle "seen" grids */
                if (info & (CAVE_SEEN))
                {
                    /* Do nothing */
                }

                /* Handle "blind" */
                else if (p_ptr->blind)
                {
                    /* Darken the colour */
                    a = darken(a, c);
                }

                /* Handle "unseen" grids */
                else
                {
                    /* Darken the colour */
                    a = darken(a, c);
                }
            }
        }

        /* Unknown */
        else
        {
            /* Get the darkness feature */
            f_ptr = &f_info[FEAT_NONE];

            /* Normal attr */
            a = f_ptr->d_attr;

            /* Normal char */
            c = f_ptr->d_char;
        }
    }

    /* Objects */
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        /* Memorized objects */
        if (o_ptr->marked)
        {
            /* Hack -- object hallucination */
            if (image)
            {
                int i = image_object();

                a = PICT_A(i);
                c = PICT_C(i);

                break;
            }

            /* Normal attr */
            a = object_attr_default(o_ptr);

            /* Normal char */
            c = object_char_default(o_ptr);

            /* Special stack symbol for piles */
            if ((++floor_num > 1))
            {
                object_kind* k_ptr;

                /* Get the "pile" feature */
                k_ptr = &k_info[0];

                /* Normal attr */
                a = k_ptr->d_attr;

                /* Normal char */
                c = k_ptr->d_char;

                break;
            }

            break;
        }
    }

    /* Monsters */
    if (m_idx > 0)
    {
        monster_type* m_ptr = &mon_list[m_idx];

        /* Visible monster */
        if (m_ptr->ml)
        {
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            byte da;
            char dc;

            /* Desired attr */
            da = r_ptr->d_attr;

            /* Desired char */
            dc = r_ptr->d_char;

            /* Hack -- monster hallucination */
            if (image)
            {
                int i = image_monster();

                a = PICT_A(i);
                c = PICT_C(i);
            }

            /* Special attr/char codes */
            else if ((da & 0x80) && (dc & 0x80))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Multi-hued monster */
            else if (r_ptr->flags1 & (RF1_ATTR_MULTI))
            {
                /* Multi-hued attr */
                a = dieroll(15);

                /* Normal char */
                c = dc;
            }

            /* Normal monster (not "clear" in any way) */
            else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR | RF1_CHAR_CLEAR)))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Hack -- Bizarre grid under monster */
            else if ((a & 0x80) || (c & 0x80))
            {
                /* Use attr */
                a = da;

                /* Use char */
                c = dc;
            }

            /* Normal char, Clear attr, monster */
            else if (!(r_ptr->flags1 & (RF1_CHAR_CLEAR)))
            {
                /* Normal char */
                c = dc;
            }

            /* Normal attr, Clear char, monster */
            else if (!(r_ptr->flags1 & (RF1_ATTR_CLEAR)))
            {
                /* Normal attr */
                a = da;
            }
        }
    }

    /* Handle "player" */
    else if (m_idx < 0)
    {
        monster_race* r_ptr = &r_info[0];

        /*change player color with HP*/
        a = health_attr(p_ptr->chp, p_ptr->mhp);

        /* Get the "player" char */
        c = r_ptr->d_char;
    }

#ifdef MAP_INFO_MULTIPLE_PLAYERS
    /* Players */
    else if (m_idx < 0)
#else /* MAP_INFO_MULTIPLE_PLAYERS */
    /* Handle "player" */
    else if (m_idx < 0)
#endif /* MAP_INFO_MULTIPLE_PLAYERS */
    {
        monster_race* r_ptr = &r_info[0];

        /* Get the "player" attr */
        a = r_ptr->d_attr;

        /* Get the "player" char */
        c = r_ptr->d_char;
    }

    /* Result */
    (*ap) = a;
    (*cp) = c;
}

void dungeon_mark_map_for_redraw(void)
{
    p_ptr->redraw |= PR_MAP;
    p_ptr->window |= PW_OVERHEAD;
}

void dungeon_note_cursor_relative(int y, int x)
{
    app_session_note_cursor_relative(app_session_current(), y, x);
}

void dungeon_sync_cursor_state(void)
{
    app_session* session = app_session_current();

    if (hilite_target && target_sighted())
    {
        dungeon_note_cursor_relative(p_ptr->target_row, p_ptr->target_col);
        return;
    }

    if (hilite_player)
    {
        dungeon_note_cursor_relative(p_ptr->py, p_ptr->px);
        return;
    }

    if (session)
        app_session_set_cursor_visible(session, false);
}

/*
 * Memorize interesting viewable object/features in the given grid
 *
 * This function should only be called on "legal" grids.
 *
 * This function will memorize the object and/or feature in the given grid,
 * if they are (1) see-able and (2) interesting.  Note that all objects are
 * interesting, all terrain features except floors (and invisible traps) are
 * interesting, and floors (and invisible traps) are interesting sometimes
 * (depending on various options involving the illumination of floor grids).
 *
 * The automatic memorization of all objects and non-floor terrain features
 * as soon as they are displayed allows incredible amounts of optimization
 * in various places, especially "map_info()" and this function itself.
 *
 * Note that the memorization of objects is completely separate from the
 * memorization of terrain features, preventing annoying floor memorization
 * when a detected object is picked up from a dark floor, and object
 * memorization when an object is dropped into a floor grid which is
 * memorized but out-of-sight.
 *
 * This function should be called every time the "memorization" of a grid
 * (or the object in a grid) is called into question, such as when an object
 * is created in a grid, when a terrain feature "changes" from "floor" to
 * "non-floor", and when any grid becomes "see-able" for any reason.
 *
 * This function is called primarily from the "update_view()" function, for
 * each grid which becomes newly "see-able".
 */
void note_spot(int y, int x)
{
    u16b info;

    object_type* o_ptr;

    /* Get cave info */
    info = cave_info[y][x];

    /* Require "seen" flag */
    if (!(info & (CAVE_SEEN)))
        return;

    /* Hack -- memorize objects */
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        /* Memorize objects */
        o_ptr->marked = true;
    }

    /* Hack -- memorize grids */
    if (!(info & (CAVE_MARK)))
    {
        /* Memorize some "boring" grids */
        if (cave_floorlike_bold(y, x))
        {
            /* Option -- memorize certain floors */
            if (info & (CAVE_GLOW))
            {
                /* Memorize */
                cave_info[y][x] |= (CAVE_MARK);
            }
        }

        /* Memorize all "interesting" grids */
        else
        {
            /* Memorize */
            cave_info[y][x] |= (CAVE_MARK);
        }
    }
}

/* Internal helpers backing the public cave wrappers in cave.c. */
byte cave_visuals_get_depth_color(int depth)
{
    /* Ensure depth 0 gets a valid encoded style even if rules haven't loaded yet */
    if (depth == 0)
    {
        int sidx = g_level_primary_style;
        if (sidx < 0)
            sidx = 13; /* default Gates style */
        return (byte)(COLOR_STYLE_BASE + (sidx & (COLOR_STYLE_SLOT_MAX - 1)));
    }

    return get_active_style_color();
}

void cave_visuals_set_feat_with_color(int y, int x, int feat, int color)
{
    /* Change the feature */
    cave_feat[y][x] = feat;

    /* Set the color (0 means use depth default) */
    if (color == 0)
    {
        /* Preserve existing per-cell style if already encoded. */
        if (cave_color[y][x] < COLOR_STYLE_BASE)
        {
            /* No style encoded yet: use active style (level or vault). */
            cave_color[y][x] = get_active_style_color();
        }
    }
    else
    {
        /* If a raw style index is passed, encode it; if already encoded, keep */
        if (color < COLOR_STYLE_BASE)
            cave_color[y][x] = (byte)(COLOR_STYLE_BASE + color);
        else
            cave_color[y][x] = color;
    }

    /* Handle wall, door, and warded grids. */
    if (((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_WALL_TAIL))
        || feat == FEAT_WARDED || feat == FEAT_WARDED2 || feat == FEAT_WARDED3)
    {
        cave_info[y][x] |= CAVE_WALL;
    }
    else
    {
        cave_info[y][x] &= ~(CAVE_WALL);
    }

    /* Notice/Redraw */
    if (character_dungeon)
    {
        note_spot(y, x);
        dungeon_mark_map_for_redraw();
    }
}
