/* File: ui/layout.h */
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

/*
 * Public layout-state queries used by shared UI and gameplay presentation
 * code without exposing platform-specific configuration APIs.
 */

#ifndef INCLUDED_UI_LAYOUT_H
#define INCLUDED_UI_LAYOUT_H

#include "../h-basic.h"

bool ui_left_panel_hidden(void);

#endif /* INCLUDED_UI_LAYOUT_H */
