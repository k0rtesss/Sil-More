/* File: spell/spell-detection.h */
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
 * Detection spells and stair creation.
 */

#ifndef INCLUDED_SPELL_DETECTION_H
#define INCLUDED_SPELL_DETECTION_H

#include "../h-basic.h"

void detect_all_doors_traps(void);
bool detect_traps(void);
bool detect_doors(void);
bool detect_stairs(void);
bool detect_objects_normal(int radius);
bool detect_objects_magic(void);
bool detect_monsters(int radius);
bool detect_monsters_invis(void);
bool detect_all(void);
void stair_creation(void);

#endif /* INCLUDED_SPELL_DETECTION_H */
