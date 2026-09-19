#ifndef CAVE_FLOOD_H
#define CAVE_FLOOD_H

/* A completed flood remains bankless, so its source stage stays serialized. */
enum {
    CAVE_FLOOD_STAGE_COMPLETE = 3,
    CAVE_FLOOD_KIND_WATER = 1,
    CAVE_FLOOD_KIND_ACID = 2
};

/* The flooding trap keeps one terrain feature; these atlas cells are its
 * blue-water and green-acid plate variants. */
#define CAVE_FLOOD_TRAP_TILE_ROW 37
#define CAVE_FLOOD_TRAP_WATER_TILE_COL 0
#define CAVE_FLOOD_TRAP_ACID_TILE_COL 1
#define CAVE_FLOOD_TRAP_KIND_SAVE_MAGIC 0xF101
#define CAVE_FLOOD_SURFACE_SAVE_MAGIC 0xF102

/* Pending floods belong to the current level, with fixed trigger origins. */
void cave_flood_clear(void);
void cave_flood_begin_action(void);
void cave_flood_end_action(void);
void cave_flood_trigger(int y, int x);
void cave_flood_set_trap_kind(int y, int x, byte kind);
byte cave_flood_trap_kind_at(int y, int x);
bool cave_flood_trap_is_acid_at(int y, int x);
bool cave_flood_restore_trap_kind(int y, int x, byte kind);
byte cave_flood_stage_at(int y, int x);
bool cave_flood_restore(int y, int x, byte stage);

/* Flood-created liquids do not expose the ordinary one-cell floor bank. */
bool cave_flood_surface_at(int y, int x);
byte cave_flood_surface_kind_at(int y, int x);
void cave_flood_clear_surface_markers(void);
bool cave_flood_restore_surface(int y, int x, byte kind);
void cave_flood_surface_changed(int y, int x, int new_feat);

#endif
