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

#ifndef INCLUDED_METARUN_STATE_H
#define INCLUDED_METARUN_STATE_H

#include "../metarun.h"

extern metarun metar;
extern metarun *metaruns;
extern s16b metarun_max;
extern s16b current_run;
extern bool metarun_created;

void metarun_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);

#endif /* INCLUDED_METARUN_STATE_H */
