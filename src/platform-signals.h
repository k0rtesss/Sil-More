/* File: platform-signals.h */
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

#ifndef INCLUDED_SIGNALS_H
#define INCLUDED_SIGNALS_H

#include "h-basic.h"

#ifdef HANDLE_SIGNALS
typedef void (*signal_handler_t)(int);
extern signal_handler_t (*signal_aux)(int, signal_handler_t);
#endif

extern void signals_ignore_tstp(void);
extern void signals_handle_tstp(void);
extern void signals_init(void);

#endif /* INCLUDED_SIGNALS_H */
