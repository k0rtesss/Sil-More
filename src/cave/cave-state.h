#ifndef INCLUDED_CAVE_CAVE_STATE_H
#define INCLUDED_CAVE_CAVE_STATE_H

#include "h-basic.h"

extern int view_n;
extern u16b* view_g;
extern int temp_n;
extern u16b* temp_g;
extern byte* temp_y;
extern byte* temp_x;

extern u16b (*cave_info)[256];
extern byte (*cave_feat)[MAX_DUNGEON_WID];
extern byte (*cave_color)[MAX_DUNGEON_WID];
extern s16b (*cave_light)[MAX_DUNGEON_WID];
extern s16b (*cave_o_idx)[MAX_DUNGEON_WID];
extern s16b (*cave_m_idx)[MAX_DUNGEON_WID];

extern byte cave_cost[MAX_FLOWS][MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
extern byte (*cave_when)[MAX_DUNGEON_WID];
extern int scent_when;
extern byte flow_center_y[MAX_FLOWS];
extern byte flow_center_x[MAX_FLOWS];
extern byte update_center_y[MAX_FLOWS];
extern byte update_center_x[MAX_FLOWS];
extern s16b wandering_pause[MAX_FLOWS];
extern s16b image_count;
extern bool shimmer_objects;

#endif /* INCLUDED_CAVE_CAVE_STATE_H */
