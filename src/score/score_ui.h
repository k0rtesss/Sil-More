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

#ifndef INCLUDED_SCORE_UI_H
#define INCLUDED_SCORE_UI_H

#include "h-basic.h"

struct high_score;

void display_scores(int from, int to);
void display_scores_short(int from, int to);
void show_scores(bool longscore);
void show_scores_interactive(bool longscore);
void show_scores_interactive_highlight(bool longscore,
                                       const struct high_score* entry);
void show_scores_interactive_highlight_from_file(bool longscore,
                                       const char* filepath,
                                       const struct high_score* entry);
void do_cmd_run_history(void);

#endif /* INCLUDED_SCORE_UI_H */
