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

#ifndef INCLUDED_PLAYER_ABILITY_LOG_H
#define INCLUDED_PLAYER_ABILITY_LOG_H

#include "h-basic.h"

void ability_log_reset(void);
void ability_log_record_gain(int skilltype, int abilitynum);
void ability_log_sync_missing(void);

#endif /* INCLUDED_PLAYER_ABILITY_LOG_H */
