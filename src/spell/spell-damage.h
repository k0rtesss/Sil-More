/* File: spell/spell-damage.h */
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
 * Damage calculations, elemental effects, resistances, and stat manipulation.
 */

#ifndef INCLUDED_SPELL_DAMAGE_H
#define INCLUDED_SPELL_DAMAGE_H

#include "../h-basic.h"

typedef struct object_type object_type;

u16b bolt_pict(int y, int x, int ny, int nx, int typ);

void attempt_to_cheat_death(void);
void take_hit(int dam, cptr kb_str);

bool hates_acid(const object_type* o_ptr);
bool hates_elec(const object_type* o_ptr);
bool hates_fire(const object_type* o_ptr);
bool hates_cold(const object_type* o_ptr);
bool elemental_attack_destroys_object(int attack_type,
    const object_type* o_ptr);

void acid_dam(int raw_dam, int min_raw, int max_raw, int hp_dam, cptr kb_str);
void elec_dam(int raw_dam, int min_raw, int max_raw, int hp_dam, cptr kb_str);
int resist_fire(void);
int resist_cold(void);
int resist_pois(void);
int resist_dark(void);
void fire_dam_mixed(int raw_dam, int min_raw, int max_raw, int hp_dam,
    cptr kb_str);
void fire_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str);
void cold_dam_mixed(int raw_dam, int min_raw, int max_raw, int hp_dam,
    cptr kb_str);
void cold_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str);
void sound_dam(int raw_dam, int min_raw, int max_raw, int hp_dam);
void dark_dam_mixed(int dam, cptr kb_str);
void dark_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str);
void pois_dam_mixed(int dam);
void pois_dam_pure(int dd, int ds, bool update_rolls);

bool inc_stat(int stat);
bool dec_stat(int stat, int amount, bool permanent);
bool res_stat(int stat, int points);
void disease(int* damage);
bool apply_disenchant(int mode);

#endif /* INCLUDED_SPELL_DAMAGE_H */
