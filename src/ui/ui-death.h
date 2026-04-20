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

#ifndef INCLUDED_UI_DEATH_H
#define INCLUDED_UI_DEATH_H

#include "h-basic.h"

struct high_score;

void do_cmd_morgoth_victory(void);
void ui_death_show_character_info(void);
int ui_death_final_menu(const struct high_score* score, int* highlight);

#endif /* INCLUDED_UI_DEATH_H */
