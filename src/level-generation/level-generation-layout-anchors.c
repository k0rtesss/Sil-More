/* File: level-generation-layout-anchors.c */

#include "angband.h"
#include "log/log.h"
#include "level-generation/level-generation-internal.h"

extern bool build_type6(int y0, int x0, bool force_forge);
extern bool build_type7(int y0, int x0);
extern bool build_type8(int y0, int x0);

int style_at_color(int y, int x)
{
    if (y < 0 || x < 0 || y >= MAX_DUNGEON_HGT || x >= MAX_DUNGEON_WID)
        return -1;
    return styles_decode_color_style(cave_color[y][x]);
}

void layout_anchor_reset(void)
{
    layout_anchor_count = 0;
    for (int i = 0; i < LAYOUT_ANCHOR_MAX; ++i)
    {
        layout_anchors[i].kind = LAYOUT_ANCHOR_NONE;
        layout_anchors[i].style_primary = -1;
        layout_anchors[i].room_slot = -1;
        layout_anchors[i].requires_neighbor = false;
        layout_anchors[i].neighbor_linked = false;
    }
    for (int i = 0; i < CENT_MAX; ++i)
    {
        room_anchor_kind[i] = LAYOUT_ANCHOR_NONE;
        room_anchor_requires_neighbor[i] = false;
    }
}

void mark_room_anchor_meta(int room_idx, layout_anchor_kind_t kind,
    bool requires_neighbor)
{
    if (room_idx < 0 || room_idx >= CENT_MAX)
        return;
    room_anchor_kind[room_idx] = kind;
    room_anchor_requires_neighbor[room_idx] = requires_neighbor;
}

static void layout_anchor_capture_room(int room_idx)
{
    if (layout_anchor_count >= LAYOUT_ANCHOR_MAX)
        return;

    layout_anchor_t* a = &layout_anchors[layout_anchor_count++];
    layout_anchor_kind_t kind = room_anchor_kind[room_idx];
    if (kind == LAYOUT_ANCHOR_NONE)
        kind = LAYOUT_ANCHOR_ROOM;
    a->kind = kind;
    a->bounds = dun->corner[room_idx];
    a->center = dun->cent[room_idx];
    a->room_kind = dun->kind[room_idx];
    a->is_quest = dun->is_quest[room_idx];
    a->style_primary = style_at_color(a->center.y, a->center.x);
    a->requires_neighbor = room_anchor_requires_neighbor[room_idx];
    a->neighbor_linked = false;
    a->room_slot = room_idx;
}

void layout_anchor_capture_existing_rooms(void)
{
    for (int i = 0; i < dun->cent_n; ++i)
        layout_anchor_capture_room(i);
}

static bool place_prefab_anchor_of_type(int typ, bool require_neighbor)
{
    if (dun->cent_n >= room_capacity_limit())
        return false;

    int y = rand_range(5, p_ptr->cur_map_hgt - 5);
    int x = rand_range(5, p_ptr->cur_map_wid - 5);
    int before = dun->cent_n;
    bool ok = false;

    switch (typ)
    {
    case 8:
        ok = build_type8(y, x);
        break;
    case 7:
        ok = build_type7(y, x);
        break;
    case 6:
    default:
        ok = build_type6(y, x, false);
        break;
    }

    if (!ok || dun->cent_n <= before)
        return false;

    mark_room_anchor_meta(dun->cent_n - 1, LAYOUT_ANCHOR_PREFAB,
        require_neighbor);
    return true;
}

void seed_prefab_anchors(void)
{
    int target = 1;
    if (p_ptr->depth >= 15)
        target++;
    if (p_ptr->depth >= 30 && one_in_(2))
        target++;

    int placed = 0;
    int attempts = 0;
    int max_attempts = target * 6;

    while (placed < target && attempts < max_attempts)
    {
        attempts++;

        int roll = rand_int(100);
        int typ = 6;
        if (roll > 85 && p_ptr->depth > 25)
            typ = 8;
        else if (roll > 60 && p_ptr->depth > 10)
            typ = 7;

        bool require_neighbor = one_in_(3);

        if (place_prefab_anchor_of_type(typ, require_neighbor))
        {
            placed++;
            log_trace("Prefab anchor: placed type %d (require_neighbor=%s) [placed=%d target=%d attempts=%d]",
                typ, require_neighbor ? "true" : "false", placed, target,
                attempts);
        }
    }

    log_trace("Prefab anchor seeding complete: placed=%d target=%d attempts=%d depth=%d",
        placed, target, attempts, p_ptr->depth);
}
