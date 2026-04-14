/* File: ui/ui-status.h */

#ifndef INCLUDED_UI_STATUS_H
#define INCLUDED_UI_STATUS_H

#include "../h-basic.h"

typedef struct monster_type monster_type;

void cnv_stat(int val, char* out_val);
int health_level(int current, int max);
bool get_alertness_text(monster_type* m_ptr, int text_size, char* text, int* color);
byte health_attr(int current, int max);

void notice_stuff(void);
void update_stuff(void);
void redraw_stuff(void);
void ui_status_refresh_window_mask(u32b window_mask);
void window_stuff(void);
void handle_stuff(void);

#endif /* INCLUDED_UI_STATUS_H */
