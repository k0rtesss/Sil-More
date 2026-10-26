#include "angband.h"
#include "level-generation/level-state.h"
#include "level-generation/level-stream.h"
#include "level-generation/level-cache.h"
#include "fs/save-internal.h"
#include "fs/load-internal.h"

static _Thread_local level_stream_frame frame;
static _Thread_local u64b materializations;
static _Thread_local byte definition_count;
static _Thread_local level_stream_site_def definitions[LEVEL_STREAM_SITE_DEFS];

level_stream_frame level_stream_get_frame(void) { return frame; }
void level_stream_set_frame(level_stream_frame value)
{
    if (!value.enabled && !value.seed) materializations = 0;
    materializations = MAX(materializations, value.serial);
    value.serial = materializations;
    frame = value;
}
bool level_stream_enabled(void) { return frame.enabled; }

void level_stream_capture_frame(level_image* image)
{
    image->stream = frame;
    image->stream_site_count = definition_count;
    SDL_memcpy(image->stream_sites, definitions, sizeof(definitions));
}

void level_stream_restore_frame(const level_image* image)
{
    level_stream_set_frame(image->stream);
    definition_count = image->stream_site_count;
    SDL_memcpy(definitions, image->stream_sites, sizeof(definitions));
}

void level_stream_prepare_generation(void)
{
    if (!frame.seed) {
        frame.seed = ((u64b)Rand_div(UINT32_MAX) << 32) | Rand_div(UINT32_MAX);
        if (!frame.seed) frame.seed = 1;
    }
    frame.enabled = true;
    definition_count = 0;
    SDL_memset(definitions, 0, sizeof(definitions));
}

static void add_definition(int y1, int x1, int y2, int x2,
    int kind, const char* name)
{
    y1 = MAX(1, y1 - 2); x1 = MAX(1, x1 - 2);
    y2 = MIN(p_ptr->cur_map_hgt - 2, y2 + 2);
    x2 = MIN(p_ptr->cur_map_wid - 2, x2 + 2);
    if (y1 > y2 || x1 > x2) return;
    /* Overlapping authored footprints have one owner.  This also joins a
     * giver's room with its quest vault rather than snapshotting it twice. */
    for (int i = 0; i < definition_count; i++) {
        level_stream_site_def* d = &definitions[i];
        if (y1 > d->y2 || y2 < d->y1 || x1 > d->x2 || x2 < d->x1) continue;
        y1 = MIN(y1, d->y1); x1 = MIN(x1, d->x1);
        y2 = MAX(y2, d->y2); x2 = MAX(x2, d->x2);
        kind = MIN(kind, d->kind);
        if (!name || !name[0]) name = d->name;
        char copy[80]; SDL_strlcpy(copy, name ? name : "", sizeof(copy));
        definitions[i] = definitions[--definition_count];
        add_definition(y1 + 2, x1 + 2, y2 - 2, x2 - 2, kind, copy);
        return;
    }
    if (definition_count >= LEVEL_STREAM_SITE_DEFS)
        quit("Too many permanent sites on one dungeon window");
    level_stream_site_def* d = &definitions[definition_count++];
    d->y1 = y1; d->x1 = x1; d->y2 = y2; d->x2 = x2; d->kind = kind;
    SDL_strlcpy(d->name, name && name[0] ? name
        : kind == 2 ? "Quest site" : "Great vault", sizeof(d->name));
}

void level_stream_record_generation_sites(void)
{
    if (!frame.enabled) return;
    definition_count = 0;
    if (p_ptr->depth == 0 || p_ptr->depth == UTUMNO_FORGE_DEPTH || p_ptr->depth == UTUMNO_DEPTH) {
        add_definition(1, 1, p_ptr->cur_map_hgt - 2,
            p_ptr->cur_map_wid - 2, 3,
            p_ptr->depth == UTUMNO_DEPTH ? "Utumno entrance"
            : p_ptr->depth ? "Utumno forge" : "Gates of Angband");
        return;
    }
    if (dun) for (int i = 0; i < dun->cent_n; i++) {
        if (!dun->is_quest[i] && dun->kind[i] != ROOM_KIND_GREATER_VAULT
            && dun->kind[i] != 9) continue;
        const rectangle* r = &dun->corner[i];
        add_definition(r->y1, r->x1, r->y2, r->x2,
            dun->is_quest[i] ? 2 : 1, dun->is_quest[i] ? "Quest site" : g_vault_name);
    }
    int y1 = MAX_DUNGEON_HGT, x1 = MAX_DUNGEON_WID, y2 = 0, x2 = 0;
    for (int y = 1; y < p_ptr->cur_map_hgt - 1; y++)
        for (int x = 1; x < p_ptr->cur_map_wid - 1; x++)
            if (cave_info[y][x] & CAVE_G_VAULT) {
                y1 = MIN(y1, y); x1 = MIN(x1, x);
                y2 = MAX(y2, y); x2 = MAX(x2, x);
            }
    if (y2) add_definition(y1, x1, y2, x2, 1, g_vault_name);
    for (int i = 1; i < mon_max; i++) {
        const monster_type* m = &mon_list[i];
        if (m->r_idx != R_IDX_TULKAS && m->r_idx != R_IDX_AULE && m->r_idx != R_IDX_MANDOS && m->r_idx != R_IDX_NIENA
            && m->r_idx != R_IDX_OROME && m->r_idx != R_IDX_VARDA) continue;
        bool found = false;
        if (dun) for (int r = 0; r < dun->cent_n; r++) {
            const rectangle* b = &dun->corner[r];
            if (m->fy >= b->y1 && m->fy <= b->y2 && m->fx >= b->x1 && m->fx <= b->x2) {
                add_definition(b->y1, b->x1, b->y2, b->x2, 2, "Quest site");
                found = true; break;
            }
        }
        if (!found) add_definition(m->fy - 8, m->fx - 8,
            m->fy + 8, m->fx + 8, 2, "Quest site");
    }
}

void level_stream_update(void)
{
    if (!character_dungeon || !p_ptr || p_ptr->leaving || p_ptr->is_dead
        || character_icky || level_cache_in_worker() || level_cache_transition_pending()) return;
    if (!frame.enabled) {
        /* Import the already loaded finite map intact.  The first shift
         * expands/crops only the portion leaving its moving window. */
        level_stream_prepare_generation();
        level_cache_stream_capture_sites();
        level_image* imported = level_image_new();
        if (!imported) return;
        level_image_capture(imported);
        level_stream_capture_sites(imported);
        level_image_free(imported);
    }
    int margin = LEVEL_STREAM_PART_SIZE + 8;
    int dy = p_ptr->py < margin ? -LEVEL_STREAM_PART_SIZE
        : p_ptr->py >= p_ptr->cur_map_hgt - margin ? LEVEL_STREAM_PART_SIZE : 0;
    int dx = p_ptr->px < margin ? -LEVEL_STREAM_PART_SIZE
        : p_ptr->px >= p_ptr->cur_map_wid - margin ? LEVEL_STREAM_PART_SIZE : 0;
    if (!dy && !dx && p_ptr->cur_map_hgt == LEVEL_STREAM_SIZE
        && p_ptr->cur_map_wid == LEVEL_STREAM_SIZE) return;
    if (!dy && !dx && (p_ptr->cur_map_hgt < LEVEL_STREAM_SIZE
        || p_ptr->cur_map_wid < LEVEL_STREAM_SIZE)) { dy = -LEVEL_STREAM_PART_SIZE; dx = -LEVEL_STREAM_PART_SIZE; }
    (void)level_cache_rebase_window(dy, dx);
}

static void write64(u64b value)
{ save_wr_u32b((u32b)value); save_wr_u32b((u32b)(value >> 32)); }
static u64b read64(void)
{ u32b lo = 0, hi = 0; load_rd_u32b(&lo); load_rd_u32b(&hi); return ((u64b)hi << 32) | lo; }

void level_stream_write_frame(const level_image* image)
{
    save_wr_byte(image->stream.enabled);
    write64(image->stream.seed); write64(image->stream.serial);
    write64((u64b)image->stream.y); write64((u64b)image->stream.x);
    save_wr_byte(image->stream_site_count);
    for (int i = 0; i < image->stream_site_count; i++) {
        const level_stream_site_def* d = &image->stream_sites[i];
        save_wr_s16b(d->y1); save_wr_s16b(d->x1);
        save_wr_s16b(d->y2); save_wr_s16b(d->x2);
        save_wr_byte(d->kind); save_wr_string(d->name);
    }
}

bool level_stream_read_frame(level_image* image)
{
    byte enabled = 0, count = 0;
    u32b start = load_byte_offset;
    load_rd_byte(&enabled);
    image->stream.seed = read64(); image->stream.serial = read64();
    image->stream.y = (s64b)read64(); image->stream.x = (s64b)read64();
    load_rd_byte(&count);
    /* Leave arithmetic headroom for local projection even on corrupt input. */
    if (enabled > 1 || count > LEVEL_STREAM_SITE_DEFS
        || (enabled && !image->stream.seed)
        || image->stream.y < INT64_MIN / 2 || image->stream.y > INT64_MAX / 2
        || image->stream.x < INT64_MIN / 2 || image->stream.x > INT64_MAX / 2
        || load_byte_offset - start < 34) return false;
    image->stream.enabled = enabled;
    image->stream_site_count = count;
    for (int i = 0; i < count; i++) {
        level_stream_site_def* d = &image->stream_sites[i];
        load_rd_s16b(&d->y1); load_rd_s16b(&d->x1);
        load_rd_s16b(&d->y2); load_rd_s16b(&d->x2);
        load_rd_byte(&d->kind); load_rd_string(d->name, sizeof(d->name));
        if (d->y1 < 1 || d->x1 < 1 || d->y2 >= image->hgt - 1
            || d->x2 >= image->wid - 1 || d->y1 > d->y2 || d->x1 > d->x2
            || d->kind < 1 || d->kind > 3) return false;
    }
    return true;
}
