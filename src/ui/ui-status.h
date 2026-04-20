/* File: ui/ui-status.h */
/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

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
