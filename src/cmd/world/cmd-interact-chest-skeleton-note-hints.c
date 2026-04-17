/* File: cmd-interact-chest-skeleton-note-hints.c */

#include "angband.h"
#include "cmd-interact-chest-skeleton-note-internal.h"

#define HINT_MESSAGE_MAX 32
#define HINT_MESSAGE_LINES_MAX 16
#define SKELETON_NOTE_HOARD_GUARD_RADIUS 10

typedef struct hint_message_state {
    s16b level_depth;
    s16b map_wid;
    s16b map_hgt;
    byte message_count;
    byte line_counts[HINT_MESSAGE_MAX];
    char lines[HINT_MESSAGE_MAX][HINT_MESSAGE_LINES_MAX][100];
    hint_message_meta meta[HINT_MESSAGE_MAX];
} hint_message_state;

static hint_message_state g_hint_message_state = {
    .level_depth = -1,
    .map_wid = 0,
    .map_hgt = 0,
    .message_count = 0,
    .line_counts = {0},
    .lines = {{{0}}},
    .meta = {{0}},
};

static void hint_messages_clear_for_level(s16b level_depth, s16b map_wid,
    s16b map_hgt)
{
    g_hint_message_state.level_depth = level_depth;
    g_hint_message_state.map_wid = map_wid;
    g_hint_message_state.map_hgt = map_hgt;
    g_hint_message_state.message_count = 0;
    for (int i = 0; i < HINT_MESSAGE_MAX; ++i)
    {
        g_hint_message_state.line_counts[i] = 0;
        g_hint_message_state.meta[i].source_y = -1;
        g_hint_message_state.meta[i].source_x = -1;
        g_hint_message_state.meta[i].cue_count = 0;
        for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
        {
            g_hint_message_state.meta[i].cue_dirs[cue][0] = '\0';
            g_hint_message_state.meta[i].cue_dists[cue][0] = '\0';
        }
    }
}

static void hint_message_meta_copy(hint_message_meta* dst,
    const hint_message_meta* src)
{
    if (!dst)
        return;

    dst->source_y = -1;
    dst->source_x = -1;
    dst->cue_count = 0;
    for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
    {
        dst->cue_dirs[cue][0] = '\0';
        dst->cue_dists[cue][0] = '\0';
    }

    if (!src)
        return;

    dst->source_y = src->source_y;
    dst->source_x = src->source_x;
    dst->cue_count = MIN(src->cue_count, HINT_MESSAGE_CUE_MAX);
    for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
    {
        strnfmt(dst->cue_dirs[cue], HINT_MESSAGE_CUE_TEXT_MAX, "%s",
            (cue < dst->cue_count) ? src->cue_dirs[cue] : "");
        strnfmt(dst->cue_dists[cue], HINT_MESSAGE_CUE_TEXT_MAX, "%s",
            (cue < dst->cue_count) ? src->cue_dists[cue] : "");
    }
}

static int hint_messages_push_internal(const char lines[][100], int line_count,
    const hint_message_meta* meta)
{
    if (line_count <= 0)
        return -1;
    if (line_count > HINT_MESSAGE_LINES_MAX)
        line_count = HINT_MESSAGE_LINES_MAX;

    int slot = g_hint_message_state.message_count;
    if (slot >= HINT_MESSAGE_MAX)
    {
        for (int i = 1; i < HINT_MESSAGE_MAX; ++i)
        {
            g_hint_message_state.line_counts[i - 1]
                = g_hint_message_state.line_counts[i];
            hint_message_meta_copy(&g_hint_message_state.meta[i - 1],
                &g_hint_message_state.meta[i]);
            for (int j = 0; j < HINT_MESSAGE_LINES_MAX; ++j)
            {
                strnfmt(g_hint_message_state.lines[i - 1][j], 100, "%s",
                    g_hint_message_state.lines[i][j]);
            }
        }
        slot = HINT_MESSAGE_MAX - 1;
    }
    else
    {
        g_hint_message_state.message_count++;
    }

    g_hint_message_state.line_counts[slot] = (byte)line_count;
    hint_message_meta_copy(&g_hint_message_state.meta[slot], meta);
    for (int i = 0; i < line_count; ++i)
        strnfmt(g_hint_message_state.lines[slot][i], 100, "%s", lines[i]);
    for (int i = line_count; i < HINT_MESSAGE_LINES_MAX; ++i)
        g_hint_message_state.lines[slot][i][0] = '\0';

    return slot;
}

void hint_messages_level_reset(void)
{
    hint_messages_clear_for_level(p_ptr->depth, p_ptr->cur_map_wid,
        p_ptr->cur_map_hgt);
}

void hint_messages_ensure_level_state(void)
{
    if (g_hint_message_state.level_depth != p_ptr->depth
        || g_hint_message_state.map_wid != p_ptr->cur_map_wid
        || g_hint_message_state.map_hgt != p_ptr->cur_map_hgt)
    {
        hint_messages_level_reset();
    }
}

byte hint_messages_count_for_save(void)
{
    hint_messages_ensure_level_state();
    return g_hint_message_state.message_count;
}

s16b hint_messages_level_depth_for_save(void)
{
    hint_messages_ensure_level_state();
    return g_hint_message_state.level_depth;
}

s16b hint_messages_map_wid_for_save(void)
{
    hint_messages_ensure_level_state();
    return g_hint_message_state.map_wid;
}

s16b hint_messages_map_hgt_for_save(void)
{
    hint_messages_ensure_level_state();
    return g_hint_message_state.map_hgt;
}

byte hint_messages_message_line_count(int index)
{
    if (index < 0 || index >= g_hint_message_state.message_count)
        return 0;
    return g_hint_message_state.line_counts[index];
}

const char* hint_messages_message_line(int index, int line)
{
    if (index < 0 || index >= g_hint_message_state.message_count)
        return "";
    if (line < 0 || line >= g_hint_message_state.line_counts[index])
        return "";
    return g_hint_message_state.lines[index][line];
}

void hint_messages_message_meta(int index, hint_message_meta* out)
{
    if (!out)
        return;

    if (index < 0 || index >= g_hint_message_state.message_count)
    {
        hint_message_meta_copy(out, NULL);
        return;
    }

    hint_message_meta_copy(out, &g_hint_message_state.meta[index]);
}

void hint_messages_clear_for_load(s16b level_depth, s16b map_wid,
    s16b map_hgt)
{
    hint_messages_clear_for_level(level_depth, map_wid, map_hgt);
}

int hint_messages_add_for_load(const char lines[][100], int line_count,
    const hint_message_meta* meta)
{
    return hint_messages_push_internal(lines, line_count, meta);
}

int hint_messages_add_note_lines(const char note_lines[][100],
    const hint_message_meta* meta)
{
    int line_count = 0;

    hint_messages_ensure_level_state();
    while (line_count < HINT_MESSAGE_LINES_MAX && note_lines[line_count][0])
        line_count++;

    return hint_messages_push_internal(note_lines, line_count, meta);
}

const char* skeleton_get_unique_type_name(const monster_race* r_ptr)
{
    if (!r_ptr) return "creature";

    if (r_ptr->flags3 & RF3_DRAGON) return "dragon";
    if (r_ptr->flags3 & RF3_RAUKO) return "demon";
    if (r_ptr->flags3 & RF3_UNDEAD) return "spirit";
    if (r_ptr->flags3 & RF3_ORC) return "orc";
    if (r_ptr->flags3 & RF3_TROLL) return "troll";
    if (r_ptr->flags3 & RF3_SPIDER) return "spider";
    if (r_ptr->flags3 & RF3_WOLF) return "wolf";
    if (r_ptr->d_char == 'C') return "hound";
    if (r_ptr->flags3 & RF3_MAN) return "human";
    if (r_ptr->flags3 & RF3_ELF) return "elf";

    return "horror";
}

static int skeleton_note_map_distance(int y1, int x1, int y2, int x2)
{
    return distance(y1, x1, y2, x2);
}

const char* skeleton_note_direction_phrase(int from_y, int from_x, int to_y,
    int to_x)
{
    int dy = to_y - from_y;
    int dx = to_x - from_x;
    int sy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    int sx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);

    if (sy == 0 && sx == 0)
        return "here";
    if (sy < 0 && sx == 0)
        return "to the north";
    if (sy < 0 && sx > 0)
        return "to the north-east";
    if (sy == 0 && sx > 0)
        return "to the east";
    if (sy > 0 && sx > 0)
        return "to the south-east";
    if (sy > 0 && sx == 0)
        return "to the south";
    if (sy > 0 && sx < 0)
        return "to the south-west";
    if (sy == 0 && sx < 0)
        return "to the west";
    return "to the north-west";
}

const char* skeleton_note_distance_phrase(int dist,
    const level_layout_info* layout)
{
    int side = layout ? MAX(layout->map_wid, layout->map_hgt) : 0;
    int near_limit = 10;
    int mid_limit = 24;

    if (side > 0)
    {
        near_limit = MAX(8, side / 8);
        mid_limit = MAX(near_limit + 8, side / 4);
    }

    if (dist <= near_limit)
        return "a short way";
    if (dist <= mid_limit)
        return "some distance";
    return "a long way";
}

static bool skeleton_note_find_nearest_stairs_kind(bool want_down, int from_y,
    int from_x, int* out_y, int* out_x, int* out_feat, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_feat = 0;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int feat = cave_feat[y][x];
            bool ok = want_down ? (feat == FEAT_MORE || feat == FEAT_MORE_SHAFT)
                                : (feat == FEAT_LESS || feat == FEAT_LESS_SHAFT);
            if (!ok)
                continue;

            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_feat = feat;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_feat = feat;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_feat) *out_feat = best_feat;
    if (out_dist) *out_dist = best_dist;
    return true;
}

bool skeleton_note_find_nearest_stairs(byte sval, int from_y, int from_x,
    int* out_y, int* out_x, int* out_feat, int* out_dist)
{
    int prefer_down = 0;
    bool want_down;

    switch (sval)
    {
    case SV_SKELETON_ORC:
        prefer_down = 80;
        break;
    case SV_SKELETON_ELF:
        prefer_down = 65;
        break;
    default:
        prefer_down = 55;
        break;
    }

    want_down = percent_chance(prefer_down);
    if (want_down)
    {
        if (skeleton_note_find_nearest_stairs_kind(true, from_y, from_x, out_y,
                out_x, out_feat, out_dist))
        {
            return true;
        }
        return skeleton_note_find_nearest_stairs_kind(false, from_y, from_x,
            out_y, out_x, out_feat, out_dist);
    }

    if (skeleton_note_find_nearest_stairs_kind(false, from_y, from_x, out_y,
            out_x, out_feat, out_dist))
    {
        return true;
    }
    return skeleton_note_find_nearest_stairs_kind(true, from_y, from_x, out_y,
        out_x, out_feat, out_dist);
}

bool skeleton_note_find_nearest_forge(int from_y, int from_x, int* out_y,
    int* out_x, int* out_feat, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_feat = 0;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (!cave_forge_bold(y, x))
                continue;

            int feat = cave_feat[y][x];
            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_feat = feat;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_feat = feat;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_feat) *out_feat = best_feat;
    if (out_dist) *out_dist = best_dist;
    return true;
}

bool skeleton_note_find_nearest_quest_site(int from_y, int from_x, int* out_y,
    int* out_x, int* out_dist, const char** out_site)
{
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    const char* best_site = NULL;
    int seen = 0;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        int r_idx;
        int dist;

        if (!m_ptr->r_idx)
            continue;

        r_idx = m_ptr->r_idx;
        if (!skeleton_note_is_quest_giver_r_idx(r_idx) && r_idx != R_IDX_DURUIN)
            continue;

        dist = skeleton_note_map_distance(from_y, from_x, m_ptr->fy, m_ptr->fx);
        if (best_y < 0 || dist < best_dist)
        {
            best_y = m_ptr->fy;
            best_x = m_ptr->fx;
            best_dist = dist;
            best_site = skeleton_note_quest_site_name(r_idx);
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
            {
                best_y = m_ptr->fy;
                best_x = m_ptr->fx;
                best_site = skeleton_note_quest_site_name(r_idx);
            }
        }
    }

    if (p_ptr->aule_level == p_ptr->depth && p_ptr->aule_quest != AULE_QUEST_NOT_STARTED)
    {
        int y = p_ptr->aule_forge_y;
        int x = p_ptr->aule_forge_x;
        if (in_bounds(y, x) && cave_forge_bold(y, x))
        {
            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                best_site = "a forge of strange craft";
                seen = 1;
            }
            else if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_site = "a forge of strange craft";
                }
            }
        }
    }

    if (p_ptr->mandos_level == p_ptr->depth
        && p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED)
    {
        int y = p_ptr->mandos_vault_y;
        int x = p_ptr->mandos_vault_x;
        if (in_bounds(y, x))
        {
            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                best_site = "a hall of doom";
                seen = 1;
            }
            else if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_site = "a hall of doom";
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    if (out_site) *out_site = best_site ? best_site : "a Power";
    return true;
}

bool skeleton_note_find_nearest_great_vault(int from_y, int from_x, int* out_y,
    int* out_x, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int dist;

            if (!(cave_info[y][x] & CAVE_G_VAULT))
                continue;

            dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    return true;
}

static const char* skeleton_note_artefact_kind_name(const object_type* o_ptr)
{
    if (!o_ptr)
        return "artefact";

    switch (o_ptr->tval)
    {
    case TV_SWORD:
        return "sword";
    case TV_POLEARM:
        return "spear";
    case TV_HAFTED:
        return "hammer";
    case TV_BOW:
        return "bow";
    case TV_ARROW:
        return "arrow";
    case TV_SOFT_ARMOR:
        return "suit of armour";
    case TV_MAIL:
        return "mail shirt";
    case TV_CLOAK:
        return "cloak";
    case TV_SHIELD:
        return "shield";
    case TV_HELM:
        return "helm";
    case TV_CROWN:
        return "crown";
    case TV_GLOVES:
        return "pair of gloves";
    case TV_BOOTS:
        return "pair of boots";
    case TV_RING:
        return "ring";
    case TV_AMULET:
        return "amulet";
    case TV_LIGHT:
        return "lamp";
    case TV_HORN:
        return "horn";
    case TV_STAFF:
        return "staff";
    case TV_DIGGING:
        return "mattock";
    case TV_GEM:
        return "jewel";
    default:
        return "artefact";
    }
}

static const char* skeleton_note_indefinite_article(const char* noun)
{
    char c = (noun && noun[0]) ? noun[0] : 'a';

    if (c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');

    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
        return "an";
    return "a";
}

static const char* skeleton_note_format_artefact_kind(
    const object_type* o_ptr, char* buf, size_t buf_sz)
{
    const char* kind = skeleton_note_artefact_kind_name(o_ptr);

    if (!buf || buf_sz == 0)
        return "an artefact";

    if (!kind || !kind[0])
        kind = "artefact";

    strnfmt(buf, buf_sz, "%s %s",
        skeleton_note_indefinite_article(kind), kind);
    return buf;
}

static u32b skeleton_note_nearest_guardian_source_ident(int y, int x)
{
    int best_dist = 0;
    int seen = 0;
    u32b best_ident = 0;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr;
        u32b source_ident = 0;
        int dist;

        if (!m_ptr->r_idx)
            continue;

        dist = skeleton_note_map_distance(y, x, m_ptr->fy, m_ptr->fx);
        if (dist > SKELETON_NOTE_HOARD_GUARD_RADIUS)
            continue;

        r_ptr = &r_info[m_ptr->r_idx];
        if (r_ptr->flags3 & RF3_DRAGON)
            source_ident |= IDENT_DRAGON_DROP;
        if (r_ptr->flags1 & RF1_UNIQUE)
            source_ident |= IDENT_UNIQUE_DROP;
        if (!source_ident)
            continue;

        if (!best_ident || dist < best_dist)
        {
            best_ident = source_ident;
            best_dist = dist;
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
                best_ident = source_ident;
        }
    }

    return best_ident;
}

static const char* skeleton_note_hoard_site_for_source_ident(u32b source_ident)
{
    if ((source_ident & IDENT_DRAGON_DROP)
        && (source_ident & IDENT_UNIQUE_DROP))
    {
        return "a dragon-lord's hoard";
    }
    if (source_ident & IDENT_DRAGON_DROP)
        return "a dragon's hoard";
    if (source_ident & IDENT_UNIQUE_DROP)
        return "a unique foe's hoard";
    return NULL;
}

static const char* skeleton_note_hoard_site_for_object(const object_type* o_ptr)
{
    u32b source_ident = 0;
    const char* source_site = NULL;

    if (!o_ptr)
        return "a hidden cache";

    if (o_ptr->ident & (IDENT_CHASM_SANCTUM_ITEM | IDENT_CHASM_SANCTUM_DROP))
        return "a chasm sanctum";

    source_ident = o_ptr->ident & (IDENT_DRAGON_DROP | IDENT_UNIQUE_DROP);
    source_site = skeleton_note_hoard_site_for_source_ident(source_ident);
    if (source_site)
        return source_site;

    if (in_bounds_fully(o_ptr->iy, o_ptr->ix)
        && (cave_info[o_ptr->iy][o_ptr->ix] & CAVE_G_VAULT))
    {
        if (g_vault_name[0] != '\0')
            return g_vault_name;
        return "a great vault";
    }

    if (o_ptr->ident & IDENT_HOARD_DROP)
    {
        source_ident =
            skeleton_note_nearest_guardian_source_ident(o_ptr->iy, o_ptr->ix);
        source_site = skeleton_note_hoard_site_for_source_ident(source_ident);
        if (source_site)
            return source_site;
        return "a treasure hoard";
    }
    return "a hidden cache";
}

bool skeleton_note_find_nearest_artefact(int from_y, int from_x, int* out_y,
    int* out_x, int* out_dist, const char** out_site,
    char* out_artefact_kind, size_t out_artefact_kind_sz)
{
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    const char* best_site = NULL;
    char best_artefact_kind[64];
    int seen = 0;

    best_artefact_kind[0] = '\0';

    for (int i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];
        int dist;

        if (!o_ptr->k_idx)
            continue;
        if (o_ptr->held_m_idx)
            continue;
        if (!artefact_p(o_ptr))
            continue;
        if (o_ptr->iy >= p_ptr->cur_map_hgt || o_ptr->ix >= p_ptr->cur_map_wid)
            continue;

        dist = skeleton_note_map_distance(from_y, from_x, o_ptr->iy, o_ptr->ix);
        if (best_y < 0 || dist < best_dist)
        {
            best_y = o_ptr->iy;
            best_x = o_ptr->ix;
            best_dist = dist;
            best_site = skeleton_note_hoard_site_for_object(o_ptr);
            (void)skeleton_note_format_artefact_kind(
                o_ptr, best_artefact_kind, sizeof(best_artefact_kind));
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
            {
                best_y = o_ptr->iy;
                best_x = o_ptr->ix;
                best_site = skeleton_note_hoard_site_for_object(o_ptr);
                (void)skeleton_note_format_artefact_kind(
                    o_ptr, best_artefact_kind, sizeof(best_artefact_kind));
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    if (out_site) *out_site = best_site ? best_site : "a hidden cache";
    if (out_artefact_kind && out_artefact_kind_sz > 0)
    {
        strnfmt(out_artefact_kind, out_artefact_kind_sz, "%s",
            best_artefact_kind[0] ? best_artefact_kind : "an artefact");
    }
    return true;
}

bool skeleton_note_find_nearest_unique(int from_y, int from_x, int* out_r_idx,
    int* out_y, int* out_x, int* out_dist)
{
    int best_r_idx = 0;
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    int seen = 0;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr;
        int dist;

        if (!m_ptr->r_idx)
            continue;
        r_ptr = &r_info[m_ptr->r_idx];
        if (!(r_ptr->flags1 & RF1_UNIQUE))
            continue;

        dist = skeleton_note_map_distance(from_y, from_x, m_ptr->fy, m_ptr->fx);
        if (best_y < 0 || dist < best_dist)
        {
            best_r_idx = m_ptr->r_idx;
            best_y = m_ptr->fy;
            best_x = m_ptr->fx;
            best_dist = dist;
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
            {
                best_r_idx = m_ptr->r_idx;
                best_y = m_ptr->fy;
                best_x = m_ptr->fx;
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_r_idx) *out_r_idx = best_r_idx;
    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    return true;
}

bool skeleton_note_find_nearest_partition_site(level_partition_kind kind,
    big_cave_type_t cave_type, int from_y, int from_x, int* out_y,
    int* out_x, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int dist;

            if (level_partition_kind_for_point(y, x) != kind)
                continue;
            if (kind == LEVEL_PART_BIG_CAVE
                && level_partition_big_cave_type_for_point(y, x) != cave_type)
            {
                continue;
            }

            dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    return true;
}

const char* skeleton_note_stair_site(int feat)
{
    switch (feat)
    {
    case FEAT_MORE:
        return "stair down";
    case FEAT_MORE_SHAFT:
        return "shaft down";
    case FEAT_LESS:
        return "stair up";
    case FEAT_LESS_SHAFT:
        return "shaft up";
    default:
        return "stairs";
    }
}

const char* skeleton_note_stair_title(int feat)
{
    switch (feat)
    {
    case FEAT_MORE:
        return "Hint: Down Stairs";
    case FEAT_MORE_SHAFT:
        return "Hint: Down Shaft";
    case FEAT_LESS:
        return "Hint: Up Stairs";
    case FEAT_LESS_SHAFT:
        return "Hint: Up Shaft";
    default:
        return "Hint: Stairs";
    }
}

void skeleton_note_partition_meta_for_hint(skeleton_hint_kind hint,
    level_partition_kind* out_kind, big_cave_type_t* out_type)
{
    if (out_kind)
        *out_kind = LEVEL_PART_NONE;
    if (out_type)
        *out_type = BIG_CAVE_NONE;

    switch (hint)
    {
    case SKEL_HINT_PART_LABYRINTH:
        if (out_kind) *out_kind = LEVEL_PART_LABYRINTH;
        break;
    case SKEL_HINT_PART_CHASM:
        if (out_kind) *out_kind = LEVEL_PART_CHASM;
        break;
    case SKEL_HINT_PART_CAVE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        break;
    case SKEL_HINT_PART_CAVE_ICE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_ICE;
        break;
    case SKEL_HINT_PART_CAVE_FIRE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_FIRE;
        break;
    case SKEL_HINT_PART_CAVE_POIS:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_POIS;
        break;
    case SKEL_HINT_PART_ROOMY:
        if (out_kind) *out_kind = LEVEL_PART_ROOMY;
        break;
    case SKEL_HINT_PART_RUINED:
        if (out_kind) *out_kind = LEVEL_PART_RUINED;
        break;
    case SKEL_HINT_PART_CAVEY:
        if (out_kind) *out_kind = LEVEL_PART_CAVEY;
        break;
    default:
        break;
    }
}

const char* skeleton_note_forge_site(int feat, char* buf, size_t buf_sz)
{
    (void)buf;
    (void)buf_sz;

    if (feat >= FEAT_FORGE_UNIQUE_HEAD && feat <= FEAT_FORGE_UNIQUE_TAIL)
        return "unique forge";
    if (feat >= FEAT_FORGE_GOOD_HEAD && feat <= FEAT_FORGE_GOOD_TAIL)
        return "enchanted forge";

    return "forge";
}

void hint_message_meta_init(hint_message_meta* meta, int source_y,
    int source_x)
{
    if (!meta)
        return;

    memset(meta, 0, sizeof(*meta));
    meta->source_y = (s16b)source_y;
    meta->source_x = (s16b)source_x;
}

static bool hint_message_cue_is_specific(const char* dist, const char* dir)
{
    if ((dist && streq(dist, "somewhere"))
        || (dir && streq(dir, "on this level")))
    {
        return false;
    }

    return ((dist && dist[0]) || (dir && dir[0]));
}

void hint_message_meta_add_cue(hint_message_meta* meta, const char* dist,
    const char* dir)
{
    if (!meta || !hint_message_cue_is_specific(dist, dir))
        return;

    for (int i = 0; i < meta->cue_count; ++i)
    {
        if (streq(meta->cue_dists[i], dist ? dist : "")
            && streq(meta->cue_dirs[i], dir ? dir : ""))
        {
            return;
        }
    }

    if (meta->cue_count >= HINT_MESSAGE_CUE_MAX)
        return;

    {
        int slot = meta->cue_count++;
        strnfmt(meta->cue_dists[slot], HINT_MESSAGE_CUE_TEXT_MAX, "%s",
            dist ? dist : "");
        strnfmt(meta->cue_dirs[slot], HINT_MESSAGE_CUE_TEXT_MAX, "%s",
            dir ? dir : "");
    }
}

const char* skeleton_hint_title(skeleton_hint_kind hint, int stairs_feat)
{
    switch (hint)
    {
    case SKEL_HINT_GREAT_VAULT:
        return "Hint: Great Vault";
    case SKEL_HINT_VAULT_ARTIFACT:
        return "Hint: Dragon's Hoard";
    case SKEL_HINT_STAIRS:
        return skeleton_note_stair_title(stairs_feat);
    case SKEL_HINT_PARTITION_PRESENCE:
        return "Hint: Layout";
    case SKEL_HINT_FORGE:
        return "Hint: Forge";
    case SKEL_HINT_UNIQUE_MONSTER:
        return "Hint: Unique Monster";
    case SKEL_HINT_TIP:
        return "Hint: Survival Tip";
    case SKEL_HINT_LEVEL_SIZE:
        return "Hint: Level Size";
    case SKEL_HINT_QUEST:
        return "Hint: Quest";
    case SKEL_HINT_PART_LABYRINTH:
        return "Hint: Labyrinth";
    case SKEL_HINT_PART_CHASM:
        return "Hint: Chasm";
    case SKEL_HINT_PART_CAVE:
    case SKEL_HINT_PART_CAVEY:
        return "Hint: Caves";
    case SKEL_HINT_PART_CAVE_ICE:
        return "Hint: Ice Cave";
    case SKEL_HINT_PART_CAVE_FIRE:
        return "Hint: Fire Cave";
    case SKEL_HINT_PART_CAVE_POIS:
        return "Hint: Poison Cave";
    case SKEL_HINT_PART_ROOMY:
        return "Hint: Rooms";
    case SKEL_HINT_PART_RUINED:
        return "Hint: Ruins";
    default:
        return "Hint: Note";
    }
}
