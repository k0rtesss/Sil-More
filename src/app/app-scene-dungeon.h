#ifndef INCLUDED_APP_SCENE_DUNGEON_H
#define INCLUDED_APP_SCENE_DUNGEON_H

#include "app-interaction.h"
#include "app-snapshot.h"
#include "app-ui.h"
#include "h-basic.h"

#ifdef __cplusplus
extern "C" {
#endif

struct app_session;
struct app_wait_state;

#define APP_DUNGEON_MAP_FORMAT_VERSION 1u
#define APP_DUNGEON_PANES_FORMAT_VERSION 5u
#define APP_DUNGEON_OVERLAY_FORMAT_VERSION 6u

#define APP_DUNGEON_PLAYER_SUBJECT (-1)

#define APP_DUNGEON_NAME_TEXT_MAX 32u
#define APP_DUNGEON_STATUS_TEXT_MAX 32u
#define APP_DUNGEON_MESSAGE_TEXT_MAX 160u
#define APP_DUNGEON_PANE_TEXT_MAX 32u
#define APP_DUNGEON_HIDDEN_OVERLAY_MAX 16u
#define APP_DUNGEON_COMBAT_ENTRY_MAX 100u
#define APP_DUNGEON_COMPACT_SEGMENT_MAX 16u
#define APP_DUNGEON_COMPACT_SHORT_TEXT_MAX 12u
#define APP_PACK_COORD(y, x) \
    ((((u32b)((u16b)(y))) << 16) | ((u32b)((u16b)(x))))
#define APP_UNPACK_COORD_Y(packed) ((s16b)(((packed) >> 16) & 0xFFFFu))
#define APP_UNPACK_COORD_X(packed) ((s16b)((packed) & 0xFFFFu))

typedef enum app_snapshot_invalidation_mask {
    APP_SNAPSHOT_INVALIDATE_NONE = 0x00000000u,
    APP_SNAPSHOT_INVALIDATE_MAP = 0x00000001u,
    APP_SNAPSHOT_INVALIDATE_STATUS = 0x00000002u,
    APP_SNAPSHOT_INVALIDATE_MESSAGES = 0x00000004u,
    APP_SNAPSHOT_INVALIDATE_PANES = 0x00000008u,
    APP_SNAPSHOT_INVALIDATE_CURSOR = 0x00000010u,
    APP_SNAPSHOT_INVALIDATE_TARGET = 0x00000020u,
    APP_SNAPSHOT_INVALIDATE_OVERLAY = 0x00000040u,
    APP_SNAPSHOT_INVALIDATE_ALL = 0x0000007Fu
} app_snapshot_invalidation_mask;

typedef enum app_animation_hint_kind {
    APP_ANIMATION_HINT_NONE = 0,
    APP_ANIMATION_HINT_ACTOR_MOVED = 1,
    APP_ANIMATION_HINT_DAMAGE = 2,
    APP_ANIMATION_HINT_PROJECTILE = 3,
    APP_ANIMATION_HINT_OBJECT_TRANSFER = 4
} app_animation_hint_kind;

typedef enum app_map_cell_flag {
    APP_MAP_CELL_FLAG_SEEN = 0x0001u,
    APP_MAP_CELL_FLAG_MARK = 0x0002u,
    APP_MAP_CELL_FLAG_VIEW = 0x0004u,
    APP_MAP_CELL_FLAG_WALL = 0x0008u,
    APP_MAP_CELL_FLAG_PLAYER = 0x0010u,
    APP_MAP_CELL_FLAG_MONSTER = 0x0020u,
    APP_MAP_CELL_FLAG_OBJECT = 0x0040u,
    APP_MAP_CELL_FLAG_TARGET = 0x0080u,
    APP_MAP_CELL_FLAG_CURSOR = 0x0100u
} app_map_cell_flag;

typedef enum app_dungeon_snapshot_flag {
    APP_DUNGEON_SNAPSHOT_FLAG_WAITING = 0x0004u
} app_dungeon_snapshot_flag;

typedef enum app_dungeon_overlay_snapshot_flag {
    APP_DUNGEON_OVERLAY_SNAPSHOT_FLAG_NONE = 0x0000u,
    APP_DUNGEON_OVERLAY_SNAPSHOT_FLAG_TRANSIENT_MENU = 0x0001u
} app_dungeon_overlay_snapshot_flag;

typedef enum app_status_text_kind {
    APP_STATUS_TEXT_HUNGER = 0,
    APP_STATUS_TEXT_BLIND = 1,
    APP_STATUS_TEXT_CONFUSED = 2,
    APP_STATUS_TEXT_AFRAID = 3,
    APP_STATUS_TEXT_CUT = 4,
    APP_STATUS_TEXT_POISONED = 5,
    APP_STATUS_TEXT_STUN = 6,
    APP_STATUS_TEXT_SPEED = 7,
    APP_STATUS_TEXT_TERRAIN = 8,
    APP_STATUS_TEXT_DEPTH = 9,
    APP_STATUS_TEXT_PARTITION = 10,
    APP_STATUS_TEXT_STATE = 11,
    APP_STATUS_TEXT_SONG = 12,
    APP_STATUS_TEXT_LIGHT = 13,
    APP_STATUS_TEXT_EVASION = 14
} app_status_text_kind;

typedef struct app_status_compact_segment {
    byte attr;
    byte required;
    char long_text[APP_DUNGEON_STATUS_TEXT_MAX];
    char short_text[APP_DUNGEON_COMPACT_SHORT_TEXT_MAX];
} app_status_compact_segment;

typedef struct app_status_compact_line {
    u16b segment_count;
    u16b reserved;
    app_status_compact_segment segments[APP_DUNGEON_COMPACT_SEGMENT_MAX];
} app_status_compact_line;

typedef struct app_status_tracked_monster_live {
    byte visible;
    byte hp_attr;
    byte alertness_attr;
    byte confused;
    byte stunned;
    s16b hp_cur;
    s16b hp_max;
    char name[APP_DUNGEON_STATUS_TEXT_MAX];
    char health[APP_DUNGEON_STATUS_TEXT_MAX];
    char alertness[APP_DUNGEON_STATUS_TEXT_MAX];
} app_status_tracked_monster_live;

typedef struct app_status_quiver_live {
    byte q1_active;
    byte q2_active;
    byte same_type;
    byte q1_attr;
    byte q2_attr;
    char q1_char;
    char q2_char;
    s16b q1_current;
    s16b q1_max;
    s16b q2_current;
    s16b q2_max;
} app_status_quiver_live;

typedef struct app_cursor_snapshot {
    byte visible;
    byte relative;
    s16b row;
    s16b col;
    s16b map_y;
    s16b map_x;
} app_cursor_snapshot;

typedef struct app_target_snapshot {
    byte active;
    byte reserved;
    s16b who;
    s16b map_y;
    s16b map_x;
} app_target_snapshot;

typedef struct app_map_cell_snapshot {
    s16b map_y;
    s16b map_x;
    s16b feat;
    s16b light;
    s16b m_idx;
    s16b o_idx;
    u16b cave_info;
    u16b flags;
    byte terrain_attr;
    byte attr;
    char terrain_char;
    char ch;
} app_map_cell_snapshot;

typedef struct app_map_snapshot {
    u16b format_version;
    u16b flags;
    u16b width;
    u16b height;
    s16b panel_y;
    s16b panel_x;
    s16b player_y;
    s16b player_x;
    app_cursor_snapshot cursor;
    app_target_snapshot target;
    u32b cell_count;
    app_map_cell_snapshot cells[];
} app_map_snapshot;

typedef app_raw_cell_snapshot app_panel_cell_snapshot;

typedef struct app_combat_roll_snapshot {
    s16b round;
    s16b index;
    s16b att_type;
    s16b dam_type;
    char attacker_char;
    byte attacker_attr;
    char defender_char;
    byte defender_attr;
    byte is_attacker_player;
    byte is_defender_player;
    s16b att;
    s16b att_roll;
    s16b evn;
    s16b evn_roll;
    s16b dd;
    s16b ds;
    s16b dam;
    s16b pd;
    s16b ps;
    s16b prot;
    s16b prt_percent;
    byte melee;
    byte reserved;
} app_combat_roll_snapshot;

typedef struct app_panes_snapshot {
    u16b format_version;
    u16b flags;
    u16b combat_entry_count;
    u16b main_combat_roll_lines;
    app_combat_roll_snapshot combat_entries[APP_DUNGEON_COMBAT_ENTRY_MAX];
} app_panes_snapshot;

typedef struct app_dungeon_overlay_snapshot {
    u16b format_version;
    u16b flags;
    app_interaction_state interaction;
    app_ui_scene transient_scene;
    app_ui_scene chrome_scene;
} app_dungeon_overlay_snapshot;

typedef struct app_dungeon_snapshot {
    app_snapshot snapshot;
    app_snapshot_blob blobs[3];
    byte* map_data;
    size_t map_size;
    size_t map_capacity;
    byte* panes_data;
    size_t panes_size;
    size_t panes_capacity;
    byte* overlay_data;
    size_t overlay_size;
    size_t overlay_capacity;
    app_cursor_snapshot cursor_state;
} app_dungeon_snapshot;

void app_dungeon_snapshot_init(app_dungeon_snapshot* snapshot);
void app_dungeon_snapshot_destroy(app_dungeon_snapshot* snapshot);
bool app_status_text_live(app_status_text_kind kind, char* out_text,
    size_t out_text_sz, byte* out_attr);
bool app_status_song_lines_live(char* out_primary, size_t out_primary_sz,
    byte* out_primary_attr, char* out_secondary, size_t out_secondary_sz,
    byte* out_secondary_attr);
bool app_status_tracked_monster_live_build(
    app_status_tracked_monster_live* tracked);
bool app_status_quiver_live_build(app_status_quiver_live* quiver);
bool app_status_compact_line_build_live(app_status_compact_line* compact,
    bool include_song, bool include_wounds);
byte app_status_depth_attr_live(void);
bool app_status_state_text_live(char* out_long, size_t out_long_sz,
    char* out_short, size_t out_short_sz, byte* out_attr);
bool app_build_dungeon_snapshot(app_dungeon_snapshot* snapshot,
    u64b revision, const struct app_wait_state* wait_state,
    const app_interaction_state* interaction,
    const app_ui_scene* transient_scene, u32b update_mask, u32b redraw_mask,
    u32b window_mask);
const app_dungeon_snapshot* app_session_dungeon_snapshot(
    const struct app_session* session);
bool app_dump_dungeon_snapshot_text(const app_dungeon_snapshot* snapshot,
    char* buf, size_t buf_size);
u32b app_snapshot_invalidation_from_masks(u32b update_mask, u32b redraw_mask,
    u32b window_mask);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_APP_SCENE_DUNGEON_H */
