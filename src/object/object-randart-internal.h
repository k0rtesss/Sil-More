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

#ifndef INCLUDED_OBJECT_RANDART_INTERNAL_H
#define INCLUDED_OBJECT_RANDART_INTERNAL_H

#include "h-basic.h"

typedef struct artefact_type artefact_type;

void store_base_power(void);
void remove_contradictory(artefact_type* a_ptr);
void artefact_apply_pval_stat_skill_bonuses(artefact_type* a_ptr);
void artefact_prep(s16b k_idx, int a_idx);
void build_freq_table(artefact_type* a_ptr);
void adjust_art_freq_table(void);
void build_art_freq_table(void);
byte get_theme(void);
void choose_item(int a_idx);
void add_feature(artefact_type* a_ptr);
void try_supercharge(artefact_type* a_ptr, int final_power);
void do_curse(artefact_type* a_ptr);

#endif /* INCLUDED_OBJECT_RANDART_INTERNAL_H */
