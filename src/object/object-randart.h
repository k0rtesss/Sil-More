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

#ifndef INCLUDED_OBJECT_RANDART_H
#define INCLUDED_OBJECT_RANDART_H

#include "h-basic.h"

typedef struct object_type object_type;

void make_random_name(char* random_name, size_t max);
s32b artefact_power(int a_idx);
void build_randart_tables(void);
void free_randart_tables(void);
errr do_randart(u32b randart_seed, bool full);
bool make_one_randart(object_type* o_ptr, int art_power, bool namechoice);
bool make_fake_artefact(object_type* o_ptr, byte name1);
void artefact_wipe(int a_idx);
bool can_be_randart(const object_type* o_ptr);

#endif /* INCLUDED_OBJECT_RANDART_H */
