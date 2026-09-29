#ifndef INCLUDED_CAVE_ATMOSPHERE_H
#define INCLUDED_CAVE_ATMOSPHERE_H

#include "h-basic.h"

/* Saved values: append new kinds; never reorder these. */
typedef enum cave_atmosphere_kind {
    CAVE_ATMOSPHERE_NONE = 0,
    CAVE_ATMOSPHERE_HUSHED,
    CAVE_ATMOSPHERE_ECHOING,
    CAVE_ATMOSPHERE_DRAUGHTY,
    CAVE_ATMOSPHERE_MAX
} cave_atmosphere_kind;

/* Authored generation context, independent of floor artwork and live effects. */
typedef enum cave_atmosphere_ground {
    ATMOSPHERE_GROUND_STONE = 0,
    ATMOSPHERE_GROUND_SOFT,
    ATMOSPHERE_GROUND_SNOW,
    ATMOSPHERE_GROUND_MAX
} cave_atmosphere_ground;

void cave_atmosphere_ground_rules_clear(void);
bool cave_atmosphere_set_ground_rule(int style, int ground);
/* Fill PARTITION_META_MAX percentage chances from the finished level. */
void cave_atmosphere_generation_chances(byte* out);

void cave_atmosphere_reset(void);
void cave_atmosphere_generate(void);
void cave_atmosphere_get_partitions(byte* out);
void cave_atmosphere_set_partitions(const byte* in, int count);
cave_atmosphere_kind cave_atmosphere_at(int y, int x);
bool cave_atmosphere_changed(int old_y, int old_x, int new_y, int new_x);
int cave_atmosphere_skill_modifier(int y, int x, int skill);
int cave_atmosphere_flame_penalty(int y, int x, bool lantern);
cptr cave_atmosphere_name(cave_atmosphere_kind kind);
cptr cave_atmosphere_description(cave_atmosphere_kind kind);
void cave_atmosphere_note_player_position(void);

#endif
