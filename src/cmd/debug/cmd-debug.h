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

#ifndef INCLUDED_CMD_DEBUG_H
#define INCLUDED_CMD_DEBUG_H

#include "h-basic.h"

#ifdef ALLOW_DEBUG
void display_light_map(void);
void display_scent_map(void);
void display_noise_map(void);
void do_cmd_debug(void);
void do_cmd_wiz_unhide(int d);
#endif /* ALLOW_DEBUG */

#ifdef ALLOW_SPOILERS
void do_cmd_spoilers(void);
#endif /* ALLOW_SPOILERS */

#endif /* INCLUDED_CMD_DEBUG_H */
