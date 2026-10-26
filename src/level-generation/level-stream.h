#ifndef INCLUDED_LEVEL_STREAM_H
#define INCLUDED_LEVEL_STREAM_H

#include "h-basic.h"

/* Gameplay still uses compact local grids.  Only the world address is large.
 * The outer row/column is a transient guard, never a world boundary. */
#define LEVEL_STREAM_PART_SIZE 44
#define LEVEL_STREAM_GRID 5
#define LEVEL_STREAM_SIZE (LEVEL_STREAM_PART_SIZE * LEVEL_STREAM_GRID + 2)

typedef struct level_stream_frame {
    u64b seed, serial;
    s64b y, x; /* World tile corresponding to local (1,1). */
    bool enabled;
} level_stream_frame;

#define LEVEL_STREAM_SITE_DEFS 32
typedef struct level_stream_site_def {
    s16b y1, x1, y2, x2;
    byte kind; /* 1 great vault, 2 quest, 3 authored endgame/surface site. */
    char name[80];
} level_stream_site_def;

struct level_image;
struct monster_type;
level_stream_frame level_stream_get_frame(void);
void level_stream_set_frame(level_stream_frame frame);
bool level_stream_enabled(void);
void level_stream_clear(void);
void level_stream_prepare_generation(void);
void level_stream_record_generation_sites(void);
void level_stream_update(void);
void level_stream_capture_sites(struct level_image* image);
void level_stream_restore_sites(struct level_image* image);
bool level_stream_has_sites(int depth);
void level_stream_site_mask(int depth, level_stream_frame frame,
    void* mask);
bool level_stream_site_monster(int depth, u64b token);
bool level_stream_site_artefact(int artefact);
void level_stream_project_quests(void);
void level_stream_normalize_carried(void);
bool level_stream_site_discovery_awarded(int y, int x);
void level_stream_note_site_discovery(int y, int x);
int level_stream_bookmark_count(void);
bool level_stream_bookmark(int index, char* text, size_t length);
void level_stream_write(void);
errr level_stream_read(void);
void level_stream_capture_frame(struct level_image* image);
void level_stream_restore_frame(const struct level_image* image);
void level_stream_write_frame(const struct level_image* image);
bool level_stream_read_frame(struct level_image* image);
bool level_cache_rebase_window(int dy, int dx);
bool level_cache_stream_has_sites(void);
void level_cache_stream_capture_sites(void);
bool level_cache_stream_actor_present(u64b token);
void level_stream_rebase_narrative(int dy, int dx);

/* Coordinate-based geography and a bounded ordinary-partition builder. */
u64b level_stream_hash(u64b seed, int depth, s64b y, s64b x, unsigned domain);
s64b level_stream_floor_div(s64b value, int divisor);
bool level_stream_major_feature(u64b seed, int depth, s64b y, s64b x,
    int* feature, int* direction);
void level_stream_foundation(void);
void level_stream_generate_partition(int pi);
void level_stream_connect_partition(int pi);
bool level_stream_partition_bounds(int pi, int* y1, int* y2, int* x1, int* x2);

/* Transactional translation of every state lane in an existing local image. */
bool level_window_translate(struct level_image* image, int dy, int dx,
    int height, int width);
void level_window_translate_monster(struct monster_type* monster,
    int dy, int dx, int height, int width);

#endif
