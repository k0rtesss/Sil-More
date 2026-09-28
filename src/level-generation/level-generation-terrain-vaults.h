#ifndef INCLUDED_LEVEL_GENERATION_TERRAIN_VAULTS_H
#define INCLUDED_LEVEL_GENERATION_TERRAIN_VAULTS_H

#include "angband.h"

enum terrain_vault_policy
{
    TERRAIN_VAULT_CROSSING = 1,
    TERRAIN_VAULT_FLOOD = 2,
    TERRAIN_VAULT_RUINED = 4,
    TERRAIN_VAULT_REPAIRED = 8
};

/* Generation-only metadata, reset before each map construction attempt. */
void terrain_vault_reset(void);
/* Start one placement; repeated copies of a template get distinct instances. */
void terrain_vault_begin(void);
void terrain_vault_record(int y, int x, const vault_type* vault, char symbol);
/* Template serial, or -1 outside the exact authored footprint. */
int terrain_vault_id_at(int y, int x);
/* Placement instance, or -1 outside the exact authored footprint. */
int terrain_vault_instance_at(int y, int x);
unsigned terrain_vault_policy_at(int y, int x);
char terrain_vault_symbol_at(int y, int x);

#endif
