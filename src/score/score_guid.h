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

#ifndef INCLUDED_SCORE_GUID_H
#define INCLUDED_SCORE_GUID_H

#include "h-basic.h"

guid64 score_guid_from_u64(u64b value);
guid64 score_guid_from_string(const char* text, u32b salt);
guid64 score_guid_random(void);
bool parse_u64b_hex(const char* text, u64b* out);
bool score_guid_is_zero(const guid64* guid);
void score_guid_clear(guid64* guid);

#endif /* INCLUDED_SCORE_GUID_H */
