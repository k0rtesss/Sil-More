/* File: ui-help.h */
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

#ifndef INCLUDED_UI_HELP_H
#define INCLUDED_UI_HELP_H

#include "h-basic.h"

extern void binding_action_label(int binding, char* buf, size_t buflen);
extern void binding_action_short(int binding, char* buf, size_t buflen);
extern void do_cmd_help(void);

#endif /* INCLUDED_UI_HELP_H */
