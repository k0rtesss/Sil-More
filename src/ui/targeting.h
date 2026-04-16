/* File: ui/targeting.h */

/*
 * Transitional public header for panel and targeting helpers.
 */

#ifndef INCLUDED_UI_TARGETING_H
#define INCLUDED_UI_TARGETING_H

#include "../h-basic.h"

bool modify_panel(int wy, int wx);
bool adjust_panel(int y, int x);
bool change_panel(int dir);
void verify_panel(void);
void ang_sort_aux(void* u, void* v, int p, int q);
void ang_sort(void* u, void* v, int n);
int motion_dir(int y1, int x1, int y2, int x2);
int target_dir(char ch);
bool target_able(int m_idx);
bool target_okay(int range);
bool target_sighted(void);
void target_set_monster(int m_idx);
void target_set_location(int y, int x);
void get_sorted_target_list(int mode, int range);
bool target_set_interactive(int mode, int range);
int dir_from_delta(int deltay, int deltax);
int rough_direction(int y1, int x1, int y2, int x2);
bool get_aim_dir(int* dp, int range);
bool get_rep_dir(int* dp);
bool confuse_dir(int* dp);

#endif /* INCLUDED_UI_TARGETING_H */
