#include "angband.h"
#include "level-generation/level-generation-internal.h"
#include "cave/cave-fixtures.h"
#include "rng.h"

/* Ordinary cave decoration changes only the already serialized cave_color.
 * A private stream keeps palette edits from perturbing layout, loot or actors. */
static u32b cave_floor_random(u32b* state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

/* Compare the floor art actually selected for this level. Old styles sharing
 * the same current variant must not produce invisible "different" patches. */
static int cave_floor_art(int style, unsigned tiles[8])
{
    const style_type* s = &style_info[style];
    int count = 0;
    int variants = s->floor_tiled ? MAX(1, MIN(8, s->floor_count)) : 1;
    for (int i = 0; i < variants; i++)
    {
        int choice = s->floor_tiled ? i : cave_style_floor_choice(style);
        if (choice >= s->floor_count) choice = 0;
        unsigned row = s->floor_count ? s->floor_rowv[choice] : s->floor_row;
        unsigned col = s->floor_count ? s->floor_colv[choice] : s->floor_col;
        unsigned tile = (row << 8) | col;
        int at = 0;
        while (at < count && tiles[at] < tile) at++;
        if (at < count && tiles[at] == tile) continue;
        for (int j = count; j > at; j--) tiles[j] = tiles[j - 1];
        tiles[at] = tile;
        count++;
    }
    return count;
}

static bool cave_floor_same_art(int a, int b)
{
    unsigned first[8], second[8];
    int n = cave_floor_art(a, first), m = cave_floor_art(b, second);
    return n == m && !memcmp(first, second, n * sizeof(first[0]));
}

static bool cave_floor_patch_eligible(int y, int x, int base_style)
{
    return cave_feat[y][x] == FEAT_FLOOR && cave_natural[y][x]
        && (cave_info[y][x] & CAVE_ROOM)
        && !(cave_info[y][x] & (CAVE_ICKY | CAVE_G_VAULT
            | CAVE_MORGOTH_TUNNEL | CAVE_CHASM_AREA))
        && cave_color[y][x] == COLOR_STYLE_BASE + base_style
        && !cave_o_idx[y][x] && !cave_m_idx[y][x]
        && cave_fixture_at(y, x) == CAVE_FIXTURE_NONE;
}

static int cave_floor_adjacent(int cell, int width, int height,
    const byte* owner, int label)
{
    int y = cell / width, x = cell % width, count = 0;
    if (y > 0 && owner[cell - width] == label) count++;
    if (y + 1 < height && owner[cell + width] == label) count++;
    if (x > 0 && owner[cell - 1] == label) count++;
    if (x + 1 < width && owner[cell + 1] == label) count++;
    return count;
}

/* Grow from a random frontier, favouring contact with the existing patch.
 * Sampling a bounded number of frontier entries avoids a full-map rescan for
 * every new tile. Every claimed cell is cardinally adjacent to its seed. */
static int cave_floor_grow(int seed, int target, int label, int width,
    int height, byte* owner, byte* queued, int* frontier, u32b* random)
{
    int pending = 1, painted = 0;
    frontier[0] = seed;
    queued[seed] = 1;
    while (pending && painted < target)
    {
        int pick = 0, best = -1;
        for (int sample = 0; sample < MIN(pending, 8); sample++)
        {
            int at = cave_floor_random(random) % pending;
            int cell = frontier[at];
            int distance = ABS(cell / width - seed / width)
                + ABS(cell % width - seed % width);
            int score = 64 * cave_floor_adjacent(cell, width, height, owner, label)
                - distance * 3 + (int)(cave_floor_random(random) % 32);
            if (sample == 0 || score > best) { best = score; pick = at; }
        }
        int cell = frontier[pick];
        frontier[pick] = frontier[--pending];
        owner[cell] = (byte)label;
        painted++;
        int y = cell / width, x = cell % width;
        const int next[4] = { y ? cell - width : -1,
            y + 1 < height ? cell + width : -1,
            x ? cell - 1 : -1, x + 1 < width ? cell + 1 : -1 };
        for (int d = 0; d < 4; d++)
        {
            int n = next[d];
            if (n < 0 || owner[n] || queued[n]) continue;
            queued[n] = 1;
            frontier[pending++] = n;
        }
    }
    return painted;
}

void cave_apply_floor_palette(int y1, int y2, int x1, int x2, int base_style)
{
    cave_floor_palette palette;
    if (!p_ptr || !cave_feat || !cave_color || !cave_natural
        || !styles_cave_floor_palette(base_style, &palette)) return;
    y1 = MAX(1, y1); x1 = MAX(1, x1);
    y2 = MIN(y2, MIN(MAX_DUNGEON_HGT, p_ptr->cur_map_hgt) - 2);
    x2 = MIN(x2, MIN(MAX_DUNGEON_WID, p_ptr->cur_map_wid) - 2);
    if (y2 < y1 || x2 < x1) return;

    u64b rng = Rand_state_export();
    u32b random = (u32b)rng ^ (u32b)(rng >> 32) ^ (u32b)y1 * 0x9e3779b9u
        ^ (u32b)x1 * 0x85ebca6bu ^ (u32b)p_ptr->depth * 0xc2b2ae35u;
    random |= 1u;
    int accents[2], accent_count = 0;
    for (int pick = 0; pick < 2; pick++)
    {
        int total = 0;
        for (int i = 0; i < palette.count; i++)
            if (!cave_floor_same_art(base_style, palette.styles[i])
                && (!pick || !cave_floor_same_art(accents[0], palette.styles[i])))
                total += palette.weights[i];
        if (!total) break;
        int roll = cave_floor_random(&random) % total;
        for (int i = 0; i < palette.count; i++)
        {
            int style = palette.styles[i];
            if (cave_floor_same_art(base_style, style)
                || (pick && cave_floor_same_art(accents[0], style))) continue;
            if (roll < palette.weights[i]) { accents[accent_count++] = style; break; }
            roll -= palette.weights[i];
        }
    }
    if (!accent_count) return;

    int width = x2 - x1 + 1, height = y2 - y1 + 1, size = width * height;
    byte* owner = mem_alloc_array(size, byte);
    byte* queued = mem_alloc_array(size, byte);
    int* frontier = mem_alloc_array(size, int);
    if (!owner || !queued || !frontier) goto cleanup;
    int eligible = 0;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            bool allowed = cave_floor_patch_eligible(y1 + y, x1 + x, base_style);
            owner[y * width + x] = allowed ? 0 : 255;
            if (allowed) eligible++;
        }
    int target = eligible * palette.coverage / 100;
    int patches = MIN(palette.patches, target / 4), painted = 0;
    for (int patch = 0; patch < patches; patch++)
    {
        int seed = -1;
        u32b best = 0;
        for (int cell = 0; cell < size; cell++)
        {
            if (owner[cell] || cave_floor_adjacent(cell, width, height, owner, 0) < 2)
                continue;
            u32b rank = cave_floor_random(&random);
            if (seed < 0 || rank > best) { best = rank; seed = cell; }
        }
        if (seed < 0) break;
        memset(queued, 0, size);
        int label = patch + 1;
        int count = cave_floor_grow(seed, (target - painted) / (patches - patch),
            label, width, height, owner, queued, frontier, &random);
        /* Do not leave speckles in tiny disconnected cave pockets. */
        if (count < 4)
        {
            for (int i = 0; i < size; i++)
                if (owner[i] == label) owner[i] = 255;
            continue;
        }
        painted += count;
        int style = accents[patch % accent_count];
        for (int i = 0; i < size; i++)
            if (owner[i] == label)
                cave_color[y1 + i / width][x1 + i % width]
                    = (byte)(COLOR_STYLE_BASE + style);
    }
    log_trace("Cave floor palette: base=%d bounds=(%d,%d)-(%d,%d) eligible=%d accents=%d",
        base_style, y1, x1, y2, x2, eligible, painted);
cleanup:
    mem_free(frontier); mem_free(queued); mem_free(owner);
}
