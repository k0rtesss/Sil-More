#ifndef INCLUDED_LEVEL_GENERATION_TERRAIN_HISTORY_H
#define INCLUDED_LEVEL_GENERATION_TERRAIN_HISTORY_H
#include "h-basic.h"

enum { TERRAIN_HISTORY_ANCIENT, TERRAIN_HISTORY_DISASTER, TERRAIN_HISTORY_OVERFLOW };
#define TERRAIN_HISTORY_SYSTEMS 2
void terrain_history_reset(void);
void terrain_history_begin(void);
void terrain_history_start_tunnels(void);
bool terrain_history_finish(void);
bool terrain_history_active(void);
bool terrain_history_started(void);
int terrain_history_count(void);
int terrain_history_epoch(int system);
int terrain_history_construction_feature(int y, int x, int feature);
bool terrain_history_reserved(int y, int x);
bool terrain_history_vault_fits(int y1, int x1, int y2, int x2);
int terrain_history_material_in_bounds(int y1, int x1, int y2, int x2);
void terrain_history_pick_bank_site(int* y, int* x, int y1, int x1, int y2, int x2);
#endif
